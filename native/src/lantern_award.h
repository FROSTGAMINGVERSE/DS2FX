#pragma once
#include <windows.h>
#include <cstdint>
namespace fx::lantern_award {
// A receipt proves only that the exact local native torch-award function returned
// with a greater timer. It is not permission to write inventory on another thread.
struct Receipt {
    uint64_t sequence=0, observed_at_ms=0;
    uint32_t quantity=0;
    float native_torch_before=0, native_torch_after=0;
    DWORD game_thread=0;
};
struct Status {
    uint64_t calls=0, local_calls=0, receipts=0, rejected=0;
    DWORD first_thread=0;
    bool installed=false, observing=false, thread_fault=false, queue_fault=false;
};
bool install(uintptr_t base_slot,bool explicit_opt_in);
void disable() noexcept;
bool pop(Receipt& output) noexcept;
Status status() noexcept;
}
extern "C" void DS2FX_LanternAwardDetour();
extern "C" void* DS2FX_LanternAwardOriginal;
