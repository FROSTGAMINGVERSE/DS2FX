#pragma once

#include <windows.h>
#include <cstdint>

// Read-only detector for the established local ApplySpEffect route carrying
// Scholar's BonfireRest special effect. A receipt is not yet permission to
// mutate a resource: it must be live-validated against a normal bonfire rest
// before a future custom-resource dispatcher consumes it.
namespace fx::lantern_bonfire {

inline constexpr int32_t bonfire_rest_effect = 110000010;

struct Receipt {
    uint64_t sampled_at_ms = 0;
    uint64_t sequence = 0;
    uintptr_t owner_token = 0; // Session scope only; never a save identity.
    int32_t effect_id = 0;
};

struct Status {
    uint64_t calls = 0;
    uint64_t bonfire_effects = 0;
    uint64_t accepted = 0;
    uint64_t rejected = 0;
    DWORD first_thread = 0;
    bool installed = false;
    bool observing = false;
};

bool install(uintptr_t base_slot, bool explicit_offline_opt_in);
void disable() noexcept;
bool snapshot(Receipt& output) noexcept;
Status status() noexcept;

#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot, uintptr_t image, void* target, bool explicit_opt_in);
#endif

} // namespace fx::lantern_bonfire

extern "C" void DS2FX_LanternBonfireDetour();
extern "C" void* DS2FX_LanternBonfireOriginal;
