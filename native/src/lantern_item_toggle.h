#pragma once
#include <windows.h>
#include <cstdint>
#include "custom_item_resource.h"

// Process-local state driven only by the owned active-item event.
namespace fx::lantern_item_toggle {
struct Receipt {
    uint64_t sampled_at_ms = 0;
    uint64_t sequence = 0;
    uintptr_t owner_token = 0; // Session pointer only, never a save identity.
    int32_t metadata_10 = 0, metadata_14 = 0, metadata_18 = 0, metadata_1c = 0;
};
struct Status { uint64_t signals=0,toggles=0,rejected=0,receipt_skipped=0; uintptr_t owner=0; bool enabled=false,active=false; };
void prepare(uintptr_t base_slot,bool explicit_opt_in,bool independent_charges=false);
// Same-owner loading gaps suspend presentation, then resume intent. Death,
// unavailable manager or changed controller stows on the guarded engine tick.
void on_engine_tick(uintptr_t manager) noexcept;
bool charge_snapshot(custom_item_resource::Snapshot& output) noexcept;
void disable() noexcept;
void observe_active_item(uintptr_t action,uintptr_t image_base) noexcept;
bool active_for(uintptr_t canonical_player) noexcept;
bool is_enabled() noexcept;
// A short-lived record of an already authenticated active-item signature. Its
// metadata proves item60425001 and its exact event1000[9].
bool snapshot(Receipt& output) noexcept;
Status status() noexcept;
}
