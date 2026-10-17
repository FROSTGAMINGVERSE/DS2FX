#pragma once
#include "hud_replacement_policy.h"
#include "hud_icons.h"
#include <windows.h>
namespace fx::hud_replacement {
bool install(uintptr_t base_slot);
void observe(uintptr_t player,uint64_t epoch,bool playable) noexcept;
uint32_t present(uint64_t epoch,uint32_t visible) noexcept;
void observe_icons(const DS2FX_HudIcons& icons) noexcept;
uint32_t present_regions(uint64_t epoch,uint64_t sample,uint32_t regions) noexcept;
uint32_t present_region_mask(uint64_t epoch,uint64_t sample,uint32_t regions) noexcept;
bool should_hide(uintptr_t object) noexcept;
struct Status {
    uint64_t hidden, lease_rejected, alternate_thread_draws, identity_rejected,
        serial_rejected, grants, revokes, owner_invalidations, max_grant_gap_ms,
        lease_age_ms;
    unsigned identity_failure;
    DWORD thread;
    bool installed;
};
Status status() noexcept;
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t slot,uintptr_t image,void* const targets[3]);
void set_fixture_after_serial(void(*callback)()) noexcept;
#endif
}
extern "C" void DS2FX_HudObjectDrawDetour();
extern "C" void DS2FX_HudObjectMaskDetour();
extern "C" void DS2FX_HudTextureDetour();
