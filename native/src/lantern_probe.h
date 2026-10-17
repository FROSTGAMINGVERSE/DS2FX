#pragma once
#include <windows.h>
#include <cstdint>

// Internal, read-only research capability. It is not a lantern gameplay API.
// No engine actions, game-data writes, plugin callbacks or save persistence.
namespace fx::lantern_probe {
struct Sample {
    uint64_t sampled_at_ms = 0;
    uint32_t character_available = 0;
    uint32_t native_torch_available = 0;
    float native_torch_time_raw = 0;
    uint32_t save_slot_available = 0;
    int32_t save_slot_index = -1; // diagnostic only; not a unique save identity
    uint32_t update_value_available = 0;
    float update_value_raw = 0;  // units intentionally not assumed
};
struct Status {
    uint64_t calls = 0, manager_calls = 0, samples = 0, skipped_publish = 0;
    DWORD first_thread = 0;
    bool installed = false, observing = false, thread_fault = false;
};

// Production verifies both the executable SHA256 and the 67-byte live code hash.
// Must be invoked during the existing offline pre-entrypoint initialization.
// explicit_opt_in=false is a guaranteed no-op. Never install in Seamless.
bool install(uintptr_t base_slot, bool explicit_opt_in);
// Stops observation only. Keep the original trampoline alive until process exit.
void disable() noexcept;
bool snapshot(Sample& output) noexcept; // bounded try-lock; false means unavailable
Status status() noexcept;

// Read-only helper also exercised by owned fixtures. All offsets are source-backed
// diagnostic candidates, not yet live gameplay contracts. Requires a consistent
// local living-player chain and clears failed optional fields.
bool read_native_sample(uintptr_t base_slot, uintptr_t manager,
                        uintptr_t update_value_address, Sample& output) noexcept;
}

extern "C" void DS2FX_LanternProbeDetour();
extern "C" void* DS2FX_LanternProbeOriginal;
