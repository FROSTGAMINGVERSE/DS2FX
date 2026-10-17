#pragma once
#include <cstdint>
namespace fx::lantern_interact {
struct Sample { bool available=false, held=false, conversion_blocked=false; uint64_t player=0, held_mask=0; };
// Read-only observer for the exact supported Scholar executable. It validates
// PlayerOperator -> ChrPadManipulator -> ChrLogicalInput before reading logical
// held bit 14 (the descriptor labelled Interact). It does not infer safe context.
bool prepare(uintptr_t base_slot,bool explicit_offline_opt_in);
bool read(Sample& out) noexcept;
void disable() noexcept;
#ifdef DS2FX_FIXTURE
bool prepare_fixture(uintptr_t base_slot,uintptr_t image,bool opt_in);
#endif
}
