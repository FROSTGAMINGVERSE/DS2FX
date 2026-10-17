#pragma once
#include "../mods/wayfarer_lantern/lantern_state.h"
#include <atomic>
#include <cstdint>

// DS2FX-owned lantern toggle pipeline. A future native provider is responsible
// for filling NativeSample from the game's Interact/context state on GameManUpdate.
// This component never reads input, writes memory, advances oil, or calls the game.
namespace fx::lantern_toggle {
struct NativeSample {
    lantern::Snapshot lantern{};
    uint64_t sampled_at_ms=0;
};
struct Command {
    uint64_t nonce=0;
    lantern::Request request{};
};
struct State {
    uint64_t sampled_at_ms=0, nonce=0;
    double hold_fraction=0;
    lantern::Status status=lantern::Status::unavailable;
    bool provider_ready=false, awaiting_ack=false, model_visible=true, light_enabled=false;
};
class Pipeline {
    lantern::HoldController controller_{};
    State state_{};
    Command pending_{};
public:
    // Accepts a fresh, game-thread sample. At most one command is returned per hold.
    Command sample(const NativeSample& input,double elapsed_seconds) noexcept;
    // The native executor must acknowledge the same nonce after atomically applying
    // the matching effect/model/light result. A mismatch clears the pending command.
    bool acknowledge(uint64_t nonce,const lantern::Snapshot& applied) noexcept;
    void invalidate() noexcept;
    State state() const noexcept { return state_; }
};
}