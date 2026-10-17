#pragma once
#include <windows.h>
#include <cstdint>
namespace fx::lantern_model_visibility {
struct Status {
    uint64_t calls=0,owned=0,local=0,hidden=0,unknown=0,transitions=0;
    DWORD first_draw_thread=0;
    bool installed=false,enabled=false;
};
// Only exact owned base meshes on a positively identified nonlocal character
// can be suppressed. Unknown or unavailable identity always preserves native draw.
bool install(uintptr_t base_slot,bool explicit_offline_opt_in);
Status status() noexcept;
void disable() noexcept;
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t image,void* target,bool opt_in);
#endif
}
extern "C" void DS2FX_LanternModelVisibilityDetour();
extern "C" void* DS2FX_LanternModelVisibilityOriginal;
