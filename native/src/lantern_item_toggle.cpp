#include "lantern_item_toggle.h"
#include "common.h"
#include <atomic>
#include <cmath>

namespace fx::lantern_item_toggle {
namespace {
constexpr int32_t signal_status=19,signal_amount=51;
std::atomic<bool> enabled{false},active{false};
std::atomic<uintptr_t> slot{0},owner{0};
std::atomic<uint64_t> signals{0},toggles{0},rejected{0},receipt_skipped{0},sequence{0};
std::atomic_flag publishing = ATOMIC_FLAG_INIT;
Receipt current{};
std::atomic<DWORD> engine_thread{0};
std::atomic<uint64_t> authorized_at{0};
uintptr_t last_manager=0,last_player=0,last_flags=0;
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out,uintptr_t alignment=8) noexcept {
    out=0;return base&&offset<=UINTPTR_MAX-base&&fx::read(base+offset,out)&&out>=0x10000&&out<=0x00007fffffffffffULL&&!(out&(alignment-1));
}
struct Parameters { int32_t status; float duration; int32_t amount; };
}
void prepare(uintptr_t base_slot,bool explicit_opt_in,bool independent_charges) {
    fx::require(explicit_opt_in&&base_slot&&!enabled.load(),"Invalid Lantern quick-slot toggle setup");
    fx::require(!independent_charges,"Lantern charges are retired; use the unlimited toggle");
    slot.store(base_slot);owner.store(0);active.store(false);enabled.store(true);
}
void on_engine_tick(uintptr_t manager) noexcept {
    if(!enabled.load())return;
    uintptr_t actual=0,player=0,flags=0;int32_t hp=0,max_hp=0;
    if(!fx::read(slot.load(),actual)||actual!=manager||!manager){active.store(false);owner.store(0);authorized_at.store(0);last_manager=last_player=last_flags=0;return;}
    DWORD expected=0;engine_thread.compare_exchange_strong(expected,GetCurrentThreadId());
    if(engine_thread.load()!=GetCurrentThreadId()){disable();return;}
    const auto now=GetTickCount64();
    const bool living=pointer(manager,0xd0,player)&&pointer(player,0xb8,flags)&&
        fx::read(player+0x168,hp)&&fx::read(player+0x170,max_hp)&&hp>0&&hp<=max_hp&&max_hp<=1000000;
    // Preserve intent across stalls/world-data replacement only for the same
    // live owner/controller. Freshness still gates every consumer. This is
    // not persistence across character recreation, missing manager or saves.
    if(!living||manager!=last_manager||player!=last_player||flags!=last_flags) {active.store(false);owner.store(0);}
    last_manager=manager;last_player=living?player:0;last_flags=living?flags:0;
    authorized_at.store(living?now:0);
}
bool charge_snapshot(custom_item_resource::Snapshot& output) noexcept {
    output={};return false; // Retained extension ABI; simple toggle has no resource pool.
}
void disable() noexcept {enabled.store(false);active.store(false);owner.store(0);}
bool active_for(uintptr_t canonical_player) noexcept {const auto now=GetTickCount64(),when=authorized_at.load();return enabled.load()&&active.load()&&canonical_player&&owner.load()==canonical_player&&when&&now>=when&&now-when<=500;}
bool is_enabled() noexcept {return enabled.load();}
bool snapshot(Receipt& output) noexcept {
    output={};
    if(!enabled.load()||publishing.test_and_set(std::memory_order_acquire)) return false;
    output=current;
    publishing.clear(std::memory_order_release);
    if(!enabled.load()||!output.sampled_at_ms||GetTickCount64()-output.sampled_at_ms>1500||owner.load()!=output.owner_token) {output={};return false;}
    return true;
}
Status status() noexcept {return {signals.load(),toggles.load(),rejected.load(),receipt_skipped.load(),owner.load(),enabled.load(),active.load()};}
void observe_active_item(uintptr_t action,uintptr_t image_base) noexcept {
    if(!enabled.load())return;
    uintptr_t vtable=0,params=0,manager=0,player=0,flags=0,action_owner=0,owner_vtable=0,owner_player=0,override_flags=0;
    Parameters signal{};int32_t hp=0,max_hp=0,metadata_10=0,metadata_14=0,metadata_18=0,metadata_1c=0;
    if(!action||!fx::read(action,vtable)||vtable!=image_base+0x10ccbd8||!pointer(action,0x40,params,4)||!fx::read(params,signal)||
       signal.status!=signal_status||signal.amount!=signal_amount||signal.duration!=0.0f||!std::isfinite(signal.duration)||
       !fx::read(slot.load(),manager)||!pointer(manager,0xd0,player)||!fx::read(player+0x168,hp)||!fx::read(player+0x170,max_hp)||hp<=0||max_hp<hp||max_hp>1000000||
       !pointer(player,0xb8,flags)||!pointer(action,0x48,action_owner)||!fx::read(action_owner,owner_vtable)||owner_vtable!=image_base+0x10af2b8||
       !pointer(action_owner,8,owner_player)||owner_player!=player||!fx::read(action_owner+0x18,override_flags)||(override_flags&&override_flags!=flags)) {++rejected;return;}
    if(!fx::read(action+0x10,metadata_10)||!fx::read(action+0x14,metadata_14)||!fx::read(action+0x18,metadata_18)||!fx::read(action+0x1c,metadata_1c)) {++rejected;return;}
    uintptr_t check=0;if(!fx::read(slot.load(),check)||check!=manager||!pointer(manager,0xd0,check)||check!=player||!pointer(action,0x48,check)||check!=action_owner) {++rejected;return;}
    const auto now=GetTickCount64(),when=authorized_at.load();
        if(!when||now<when||now-when>250||engine_thread.load()!=GetCurrentThreadId()||
           metadata_10!=60425001||metadata_14!=1000||metadata_18!=9) {++rejected;return;}
    const auto previous=owner.exchange(player);
    if(previous!=player)active.store(true);else active.store(!active.load());
    ++signals;++toggles;
    if(publishing.test_and_set(std::memory_order_acquire)) {++receipt_skipped;return;}
    current={GetTickCount64(),sequence.fetch_add(1)+1,player,metadata_10,metadata_14,metadata_18,metadata_1c};
    publishing.clear(std::memory_order_release);
}
}
