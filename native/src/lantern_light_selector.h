#pragma once
#include <windows.h>
#include <cstdint>

// Replaces only the row result at ChrLightCtrl's slot3 comparison. The game's
// own row lookup, selection, interpolation and resource creation remain intact.
namespace fx::lantern_light_selector {
struct Status { uint64_t calls=0, selected=0, rejected=0; DWORD thread=0; bool installed=false, enabled=false; };
bool install(uintptr_t base_slot, bool offline_opt_in);
void authorize_engine_tick(uintptr_t manager) noexcept;
void disable() noexcept;
Status status() noexcept;
int32_t select(uintptr_t control, uintptr_t flags, int32_t native_row) noexcept;
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot, uintptr_t image, void* target);
#endif
}
extern "C" void DS2FX_LanternLightSelectorDetour();
extern "C" void* DS2FX_LanternLightSelectorOriginal;
