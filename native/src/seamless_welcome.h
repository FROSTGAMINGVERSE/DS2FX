#pragma once
#include <cstdint>
namespace fx::seamless_welcome {
// Local presentation only. Official files, networking and save flow are untouched.
bool install(bool enabled);
uint64_t suppressed() noexcept;
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t image,uintptr_t global_slot,void* target);
void set_enabled_fixture(bool enabled) noexcept;
#endif
}
