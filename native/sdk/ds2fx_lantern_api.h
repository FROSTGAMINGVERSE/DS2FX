#pragma once
#include <stdint.h>

// Experimental read-only extension; core ABI 2 is unchanged. Values are copied,
// never engine pointers. UI may call this from its own thread. A zero return
// means no fresh canonical oil effect is available: hide the timer.
#define DS2FX_LANTERN_STATE_VERSION 1u
struct DS2FX_LanternState {
    uint32_t size;
    uint32_t version;
    uint64_t sampled_at_ms;
    uint64_t character_epoch;
    double remaining_seconds;
    double capacity_seconds;
    uint32_t active;
    uint32_t reserved;
};
typedef uint32_t (*DS2FX_GetLanternStateFn)(DS2FX_LanternState* output);

// Independent DS2FX resource, not the engine Estus counter. Session-local until
// stable character identity/persistence is separately proven. ABI2 unchanged.
#define DS2FX_LANTERN_CHARGES_VERSION 1u
struct DS2FX_LanternCharges {
    uint32_t size,version;
    uint64_t sampled_at_ms,character_epoch,revision;
    uint32_t current,capacity,active,reserved;
};
typedef uint32_t (*DS2FX_GetLanternChargesFn)(DS2FX_LanternCharges* output);

// True when native quick-slot presentation owns the charge display.
typedef uint32_t (*DS2FX_HasNativeLanternQuickslotFn)();
