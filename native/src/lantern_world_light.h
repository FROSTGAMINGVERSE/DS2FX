#pragma once
#include <windows.h>
#include <cstdint>

// Local-player adapter in offline/Seamless Core for the owned GXPointLight, after its normal
// update. Never edits Lighting Engine globals or creates an ambient light.
namespace fx::lantern_world_light {
// true only for a present, exactly supported LE and a fresh finite [0,10] value.
// false preserves vanilla behavior (including absent/unsupported LE).
using ReadIntensity=bool(*)(float& value) noexcept;
struct Status {
    uint64_t calls=0, eligible=0, applied=0, restored=0, rejected_thread=0;
    uint64_t unavailable=0, invalid_value=0, provider_faults=0, lost_owner=0;
    DWORD engine_thread=0;
    bool installed=false, enabled=false, fault=false, restore_pending=false;
    float last_multiplier=1, last_native_brightness=0, last_output_brightness=0;
    float last_x=0, last_y=0, last_z=0;
    bool last_render_enabled=true;
    float last_native_radius=0,last_output_radius=0,last_flicker_gain=1;
};
void configure_presentation(unsigned flicker_percent,unsigned specular_percent,unsigned diffuse_percent=100,unsigned radius_percent=100);
float flicker_gain(uint64_t milliseconds,float amount) noexcept;
bool install(uintptr_t base_slot,bool explicit_offline_opt_in,ReadIntensity read_intensity);
// Must be called directly by the verified GameManUpdate observation callback.
// Keep authorizing after disable until restore_pending becomes false.
void authorize_engine_tick(uintptr_t manager) noexcept;
// Disables new overrides. Restoration runs on the next valid native light tick;
// never dereferences a stale character/resource during shutdown or replacement.
void disable() noexcept;
Status status() noexcept;
#ifdef DS2FX_FIXTURE
void configure_presentation_for_fixture(unsigned flicker_percent,unsigned specular_percent,unsigned diffuse_percent=100,unsigned radius_percent=100);
bool install_owned_fixture(uintptr_t base_slot,uintptr_t image,void* target,ReadIntensity read_intensity,bool opt_in);
void after_original_for_fixture(uintptr_t controller) noexcept;
#endif
}
extern "C" void DS2FX_LanternWorldLightDetour();
extern "C" void* DS2FX_LanternWorldLightOriginal;
