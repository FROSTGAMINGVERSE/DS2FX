#include "lantern_effect_test_driver.h"
#include "common.h"
#include <array>
#include <cmath>
#include <cstring>
namespace fx::lantern_effect_test {
namespace {
constexpr std::array<unsigned char,5> entry{0xe9,0x1c,0x0d,0x9f,0x01};
constexpr std::array<unsigned char,30> thunk{0x48,0x89,0x6c,0x24,0xf8,0x48,0x8d,0x64,0x24,0xf8,0x48,0x8d,0x2d,0x33,0xa7,0x0a,0x00,0x48,0x87,0x2c,0x24,0x48,0x8d,0x64,0x24,0x08,0xff,0x64,0x24,0xf8};
bool native_entries_match(uintptr_t image) noexcept {
    std::array<unsigned char,5> e{};std::array<unsigned char,30> t{};
    return fx::read(image+0x14bec0,e) && e==entry && fx::read(image+0x1b3cbe1,t) && t==thunk;
}
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out) noexcept {
    return base && offset<=UINTPTR_MAX-base && fx::read(base+offset,out) && out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
bool selected_slot(uintptr_t manager,int32_t& value) noexcept {
    uintptr_t data=0,profile=0;
    return pointer(manager,0xa8,data) && pointer(data,0xc0,profile) &&
        fx::read(profile+0x18a8,value) && value>=0 && value<10;
}
}
bool Driver::prepare_impl(uintptr_t slot,Apply fn,bool opt_in,int32_t expected_slot,float required_delay) {
    if(!opt_in) return false;
    fx::require(!prepared_.load() && slot && fn && expected_slot>=0 && expected_slot<10 &&
        std::isfinite(required_delay) && required_delay>=5.0f && required_delay<=60.0f,"Invalid or repeated visual test driver setup");
    base_slot_=slot;apply_=fn;image_=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    expected_slot_=expected_slot;required_delay_=required_delay;
    prepared_.store(true);enabled_.store(true);return true;
}
bool Driver::prepare(uintptr_t slot,bool opt_in,int32_t expected_slot,float required_delay) {
    if(!opt_in) return false;
#ifdef DS2FX_SEAMLESS_BRIDGE
    (void)slot;(void)expected_slot;(void)required_delay;fx::require(false,"Lantern visual test driver is offline-only");return false;
#else
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash,"Unsupported executable for lantern visual test");
    fx::require(slot==image+0x16148f0 && native_entries_match(image),"ApplySpEffect entry mismatch; visual test refused");
    verify_native_=true;
    return prepare_impl(slot,reinterpret_cast<Apply>(image+0x14bec0),true,expected_slot,required_delay);
#endif
}
#ifdef DS2FX_FIXTURE
bool Driver::prepare_owned_fixture(uintptr_t slot,Apply fn,bool opt_in,int32_t expected_slot,float required_delay) {return prepare_impl(slot,fn,opt_in,expected_slot,required_delay);}
#endif
void Driver::disable() noexcept {enabled_.store(false);}
Status Driver::status() const noexcept {return {calls_.load(),issued_.load(),epochs_.load(),engine_thread_.load(),prepared_.load(),enabled_.load(),fault_.load()};}
void Driver::on_engine_tick(uintptr_t manager,uintptr_t update_address,DWORD verified_thread) noexcept {
    if(!enabled_.load() || fault_.load()) return;
    ++calls_;
    const DWORD tid=GetCurrentThreadId();
    if(!verified_thread || verified_thread!=tid) {fault_.store(true);enabled_.store(false);return;}
    DWORD expected=0;engine_thread_.compare_exchange_strong(expected,tid);
    if(engine_thread_.load()!=tid || lock_.test_and_set(std::memory_order_acquire)) {fault_.store(true);enabled_.store(false);return;}
    struct Unlock {std::atomic_flag& f;~Unlock(){f.clear(std::memory_order_release);}} unlock{lock_};
    uintptr_t canonical=0,player=0,control=0,vtable=0;
    int32_t health=0,maximum=0,save_slot=-1;float dt=0;
    const bool living=fx::read(base_slot_,canonical) && canonical==manager &&
        selected_slot(manager,save_slot) && save_slot==expected_slot_ &&
        pointer(manager,0xd0,player) && pointer(player,0x3e0,control) &&
        fx::read(control,vtable) && vtable==image_+0x10bfee0 &&
        fx::read(player+0x168,health) && fx::read(player+0x170,maximum) &&
        health>0 && health<=maximum && maximum<=1000000;
    if(!living) {character_=control_=0;delay_=0;called_for_epoch_=false;return;}
    if(player!=character_ || control!=control_) {character_=player;control_=control;delay_=0;called_for_epoch_=false;++epochs_;}
    if(called_for_epoch_) return;
    if(!fx::read(update_address,dt) || !std::isfinite(dt) || dt<0 || dt>0.25f) {delay_=0;return;}
    delay_+=dt;
    if(delay_<required_delay_) return;
    if(verify_native_ && !native_entries_match(image_)) {fault_.store(true);enabled_.store(false);return;}
    // Recheck character ownership immediately before the one engine call.
    uintptr_t check=0;
    if(!enabled_.load() || fault_.load() || !fx::read(base_slot_,check) || check!=manager || !pointer(manager,0xd0,check) || check!=player ||
       !pointer(player,0x3e0,check) || check!=control || !fx::read(player+0x168,health) || health<=0 ||
       !selected_slot(manager,save_slot) || save_slot!=expected_slot_) {
        character_=control_=0;delay_=0;return;
    }
    Request request{};
    called_for_epoch_=true; // Mark before calling: nested updates cannot repeat it.
    ++issued_;
    apply_(control,&request); // Native application only. No inventory consumption.
}
}
