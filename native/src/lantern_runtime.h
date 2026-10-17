#pragma once
#include <cstdint>
#include "lantern_award.h"
#include "lantern_oil_award.h"
#include "lantern_effect_test_driver.h"
#include "lantern_interact_observer.h"
#include "bonfire_item_delivery.h"
namespace fx::lantern_runtime {
struct Settings {
    bool companion_oil=false;
    bool visual_effect_test=false;
    bool observe_interact=false;
    bool quick_slot_toggle=false;
    bool bonfire_delivery=false;
    int32_t test_save_slot=-1;
    float test_delay_seconds=20;
};
void prepare(uintptr_t base_slot,const Settings& settings);
void native_tick(uintptr_t manager,uintptr_t update_value) noexcept;
void successful_torch_award(const lantern_award::Receipt& receipt) noexcept;
void bonfire_rest(const lantern_bonfire::Receipt& receipt) noexcept;
bonfire_item_delivery::Status bonfire_delivery_status() noexcept;
void disable() noexcept;
lantern_oil_award::Status delivery_status() noexcept;
lantern_effect_test::Status visual_test_status() noexcept;
lantern_interact::Sample interact_status() noexcept;
}
