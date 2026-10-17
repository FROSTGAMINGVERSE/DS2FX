#pragma once
#include <windows.h>
#include <cstdint>

// Internal, read-only instruction-lifetime observer. No effect writes, engine
// calls, input interception, elapsed-time simulation or save persistence.
namespace fx::lantern_effect {
struct Sample {
    uint64_t sampled_at_ms = 0;
    uintptr_t owner_token = 0; // process-local character address; NOT save identity
    float remaining_seconds = 0;
    float capacity_seconds = 0;
    uint32_t available = 0;
    uint32_t active = 0;
    int32_t metadata_10 = 0, metadata_14 = 0, metadata_18 = 0, metadata_1c = 0;
    uint32_t light_value = 0;
};
struct Status {
    uint64_t calls = 0, matched_calls = 0, samples = 0, skipped_publish = 0;
    DWORD first_thread = 0;
    bool installed = false, observing = false, thread_fault = false;
    uint64_t status_actions = 0;
    uint32_t last_rejection = 0; // bounded diagnostic: first failed validation stage
};
bool install(uintptr_t base_slot, bool explicit_opt_in);
void disable() noexcept;
bool snapshot(Sample& output) noexcept;
Status status() noexcept;
// Uses the Value32 RTTI vtable and owned content instruction: status19, 300s,
// amount51. Copies native remaining time directly; owner must be canonical.
bool read_native_sample(uintptr_t base_slot, uintptr_t action,
                        uintptr_t image_base, Sample& output, uint32_t* rejection = nullptr) noexcept;
}
extern "C" void DS2FX_LanternEffectDetour();
extern "C" void* DS2FX_LanternEffectOriginal;
