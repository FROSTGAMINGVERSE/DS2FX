#pragma once
#include "common.h"
#include <array>
namespace fx::lantern_quickslot_ui {
struct Status {uint64_t counts,icons,depleted,rejected_thread;DWORD thread;bool installed,enabled;uint64_t inventory_counts;};
bool install(uintptr_t base_slot,bool opt_in);
void authorize_engine_tick(uintptr_t manager) noexcept;
void disable() noexcept;
Status status() noexcept;
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t image,const std::array<void*,5>& entries);
void allow_owned_fixture_callers(bool allowed) noexcept;
#endif
}
