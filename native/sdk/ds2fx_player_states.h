#pragma once
#include <stdint.h>

/* Pointer-free, owner-scoped state API. Definitions do not modify the NMB.
   Core owns execution; plugins never supply engine pointers or tick callbacks.
   Registration is initialization-only. Check host size before using API tails.
   An animation asset ID alone does not prove a native animation/physics route. */
enum DS2FX_StateMotion : uint32_t {
    DS2FX_STATE_NATIVE_PASS = 0,
    DS2FX_STATE_ANIMATION = 1,
    DS2FX_STATE_VERTICAL_JUMP = 2,
    DS2FX_STATE_FORWARD_JUMP = 3
};
enum DS2FX_StateResult : uint32_t {
    DS2FX_STATE_INVALID = 0, DS2FX_STATE_OK = 1,
    DS2FX_STATE_EXECUTOR_UNAVAILABLE = 2, DS2FX_STATE_STALE = 3,
    DS2FX_STATE_BUSY = 4, DS2FX_STATE_INELIGIBLE = 5,
    DS2FX_STATE_NATIVE_PASSTHROUGH = 6, DS2FX_STATE_EXPIRED = 7,
    DS2FX_STATE_EXECUTION_REJECTED = 8, DS2FX_STATE_CANCELLED = 9
};
enum DS2FX_StatePhase : uint32_t {
    DS2FX_STATE_NONE = 0, DS2FX_STATE_QUEUED = 1,
    DS2FX_STATE_ACTIVE = 2, DS2FX_STATE_COMPLETED = 3,
    DS2FX_STATE_ABORTED = 4
};
struct DS2FX_StateDefinition {
    uint32_t size, local_id;
    uint32_t motion, animation_asset_id;
    /* Watchdog only, not animation speed or physics. 100..10000 ms. */
    uint32_t maximum_duration_ms, reserved;
};
struct DS2FX_StateRequest {
    uint32_t size, handle;
    uint64_t character_epoch;
    /* Queue lifetime, 1..250 ms. Epoch is checked again on the game thread. */
    uint32_t expires_after_ms, reserved;
};
struct DS2FX_JumpRoutes {
    uint32_t size, idle_handle, moving_handle, sprint_handle;
};
struct DS2FX_StateServiceStatus {
    uint32_t size, executor_available;
    uint32_t registered_states, jump_routes_registered;
    uint64_t character_epoch, sequence;
    uint32_t handle, phase, result, reserved;
};
static_assert(sizeof(DS2FX_StateDefinition)==24);
static_assert(sizeof(DS2FX_StateRequest)==24);
static_assert(sizeof(DS2FX_StateServiceStatus)==48);
