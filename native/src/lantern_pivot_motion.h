#pragma once
#include "lantern_pose_observer.h"
namespace fx::lantern_pivot_motion {
struct Status {
    uint64_t frames=0,steps=0,applied=0,repeated=0,rejected=0,resets=0;
    DWORD engine_thread=0;
    bool prepared=false,enabled=false,fault=false;
    float world_pivot[3]{},lean[2]{},local_rotation[4]{0,0,0,1};
    uint64_t contacts=0;
    float clearance[3]{};
};
// Source-only capability until v9 live observer confirms the exact bone/thread.
// No hook or observer registration is performed by prepare. Root explicitly
// registers on_owned_pose only for an authorized offline motion configuration.
bool prepare(uintptr_t base_slot,bool explicit_offline_motion_opt_in);
void authorize_engine_tick(uintptr_t manager,uintptr_t native_seconds_address) noexcept;
void on_owned_pose(uintptr_t player,uintptr_t pose,const fx::lantern_pose_observer::Sample&) noexcept;
Status status() noexcept;
// Native mapper restores the owned unmapped bind QST before every callback;
// disabling simply stops replacing it. No saved pointer is written on shutdown.
void disable() noexcept;
#ifdef DS2FX_FIXTURE
using FixtureSetter=void(*)(uintptr_t,uint32_t,const void*);
bool prepare_owned_fixture(uintptr_t base_slot,bool opt_in,FixtureSetter setter);
#endif
}
