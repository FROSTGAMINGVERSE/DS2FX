#pragma once
#include <windows.h>
#include <cstdint>

// Offline native render-transform adapter, not a light or fuel implementation.
namespace fx::lantern_light_anchor {
struct alignas(16) Matrix { float rows[4][4]{}; };
struct Status {
    uint64_t calls=0, eligible=0, relocated=0, missing_dummy=0, invalid_matrix=0, rejected_thread=0;
    DWORD engine_thread=0;
    bool installed=false, enabled=false, fault=false;
};
bool install(uintptr_t base_slot, bool explicit_offline_opt_in);
// Call only from the exact-hash-gated GameManUpdate observation callback.
// This authorizes the current thread for 250ms after canonical-manager validation.
void authorize_engine_tick(uintptr_t manager) noexcept;
void disable() noexcept;
Status status() noexcept;
#ifdef DS2FX_FIXTURE
using ResolveDummy=bool(__fastcall*)(uintptr_t,int32_t,Matrix*,bool);
bool install_owned_fixture(uintptr_t base_slot,uintptr_t image,void* target,ResolveDummy resolver,bool opt_in);
void after_original_for_fixture(uintptr_t controller) noexcept;
#endif
}
extern "C" void DS2FX_LanternLightAnchorDetour();
extern "C" void* DS2FX_LanternLightAnchorOriginal;
