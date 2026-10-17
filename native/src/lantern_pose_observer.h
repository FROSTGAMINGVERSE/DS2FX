#pragma once
#include <windows.h>
#include <cstdint>
namespace fx::lantern_pose_observer {
struct Sample {
    uint64_t sampled_at_ms=0;
    DWORD callback_thread=0;
    uint32_t bone_count=0;
    int32_t pivot_index=-1,parent_index=-1;
    bool on_authorized_thread=false,parent_matches=false,pose_valid=false;
    float translation[4]{},rotation[4]{},scale[4]{};
};
struct Status {
    uint64_t calls=0,canonical=0,samples=0,owned_bone=0,foreign_thread=0,invalid=0,dropped=0;
    DWORD engine_thread=0;
    bool installed=false,enabled=false,fault=false;
};
// Observation is read-only by default. A separately gated, explicitly registered
// owned handler can use the verified execution seam; there is no setter here.
using OwnedPoseHandler=void(*)(uintptr_t player,uintptr_t pose,const Sample&) noexcept;
void set_owned_pose_handler(OwnedPoseHandler handler) noexcept;
bool install(uintptr_t base_slot,bool explicit_offline_opt_in);
void authorize_engine_tick(uintptr_t manager) noexcept;
bool snapshot(Sample& output) noexcept;
Status status() noexcept;
void disable() noexcept;
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t image,void* target,bool opt_in);
#endif
}
extern "C" void DS2FX_LanternPoseObserverDetour();
extern "C" void* DS2FX_LanternPoseObserverOriginal;
