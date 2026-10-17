#include "lantern_runtime.h"
#include "lantern_oil_award.h"
#include "lantern_item_toggle.h"
#include "lantern_flame.h"
#include "common.h"
#include <atomic>
#include <cmath>
namespace fx::lantern_runtime {
namespace {
std::atomic<bool> enabled{false};
std::atomic<DWORD> thread{0};
uintptr_t slot=0,last_player=0,last_manager=0;
uint64_t epoch=0;
lantern_oil_award::Delivery delivery;
bonfire_item_delivery::Delivery bonfire_delivery;
lantern_effect_test::Driver visual_test;
std::atomic<bool> observe_interact{false};
std::atomic<uint32_t> interact_flags{0};
std::atomic<uint64_t> interact_held_mask{0};
}
void prepare(uintptr_t base_slot,const Settings& settings) {
    fx::require(!enabled.load(),"Lantern engine coordinator already prepared");
    slot=base_slot;
    if(settings.companion_oil) delivery.prepare(base_slot,true);
    if(settings.bonfire_delivery) bonfire_delivery.prepare(base_slot,true);
    if(settings.visual_effect_test) visual_test.prepare(base_slot,true,settings.test_save_slot,settings.test_delay_seconds);
    if(settings.observe_interact) fx::require(lantern_interact::prepare(base_slot,true),"Interact observer preparation failed");
    fx::require(!settings.quick_slot_toggle||(!settings.companion_oil&&!settings.visual_effect_test),"Reusable Lantern cannot enable retired Oil actions");
    if(settings.quick_slot_toggle) fx::lantern_item_toggle::prepare(base_slot,true,false);
    observe_interact.store(settings.observe_interact);
    enabled.store(settings.companion_oil||settings.visual_effect_test||settings.observe_interact||settings.quick_slot_toggle||settings.bonfire_delivery);
}
void disable() noexcept {enabled.store(false);observe_interact.store(false);interact_flags.store(0);interact_held_mask.store(0);lantern_interact::disable();lantern_item_toggle::disable();delivery.disable();bonfire_delivery.disable();visual_test.disable();}
bonfire_item_delivery::Status bonfire_delivery_status() noexcept {return bonfire_delivery.status();}
void bonfire_rest(const lantern_bonfire::Receipt& receipt) noexcept {bonfire_delivery.on_bonfire(receipt);}
lantern_oil_award::Status delivery_status() noexcept {return delivery.status();}
lantern_effect_test::Status visual_test_status() noexcept {return visual_test.status();}
lantern_interact::Sample interact_status() noexcept {
    const auto flags=interact_flags.load(); return {(flags&1)!=0,(flags&2)!=0,(flags&4)!=0,0,interact_held_mask.load()};
}
void native_tick(uintptr_t manager,uintptr_t update_value) noexcept {
    if(!enabled.load())return;
    DWORD expected=0;const auto current_thread=GetCurrentThreadId();thread.compare_exchange_strong(expected,current_thread);
    if(thread.load()!=current_thread) {disable();return;}
    lantern_item_toggle::on_engine_tick(manager);
    float delivery_dt=0;
    bonfire_delivery.on_engine_tick(manager,fx::read(update_value,delivery_dt)&&std::isfinite(delivery_dt)&&delivery_dt>0&&delivery_dt<=0.25f);
    lantern_flame::native_tick(manager);
    visual_test.on_engine_tick(manager,update_value,current_thread);
    if(observe_interact.load()) {
        lantern_interact::Sample input{};
        if(lantern_interact::read(input)) { interact_held_mask.store(input.held_mask); interact_flags.store((input.available?1u:0u)|(input.held?2u:0u)|(input.conversion_blocked?4u:0u)); }
        else { interact_held_mask.store(0); interact_flags.store(0); }
    }
    uintptr_t current=0,player=0;int32_t health=0,maximum=0;float dt=0;
    const bool valid=fx::read(slot,current)&&current==manager&&manager&&fx::read(manager+0xd0,player)&&player&&
        fx::read(player+0x168,health)&&fx::read(player+0x170,maximum)&&health>0&&maximum>=health&&maximum<=1000000;
    if(!valid) {
        last_player=0;last_manager=0;
        delivery.on_engine_tick(0);return;
    }
    if(last_player!=player||last_manager!=manager) {++epoch;last_player=player;last_manager=manager;}
    // A suspended, invalid or discontinuous frame cannot dispatch an action.
    if(!fx::read(update_value,dt)||!std::isfinite(dt)||dt<=0||dt>0.25f)return;
    delivery.on_engine_tick(epoch);
}
void successful_torch_award(const lantern_award::Receipt& receipt) noexcept {
    if(!enabled.load()||thread.load()!=GetCurrentThreadId()||!last_player)return;
    delivery.on_successful_torch_award(receipt,epoch);
}
}
