#pragma once

#include "custom_item_resource.h"
#include <cstdint>

// Pure coordination layer for the Wayfarer's Lantern resource.  It accepts
// only receipts that an engine-facing observer has already authenticated; it
// never reads memory, changes inventory, draws UI, or invokes game code.
namespace fx::lantern_charge_router {

inline constexpr uint32_t wayfarer_lantern_item = 60425001;
inline constexpr uint32_t default_capacity = 3;

struct Status {
    uint64_t toggle_receipts = 0;
    uint64_t bonfire_receipts = 0;
    uint64_t rejected_receipts = 0;
    uint64_t refills = 0;
};

class Router final {
public:
    bool prepare(uint32_t capacity = default_capacity);

    // Call only after the exact custom-item action authenticated owner_token.
    custom_item_resource::UseResult on_authenticated_toggle(uint64_t owner_token);

    // Call only after the BonfireRest observer authenticated the same live
    // owner.  A stale, foreign, or zero token is rejected.
    bool on_authenticated_bonfire(uint64_t owner_token);

    bool snapshot(uint64_t owner_token, custom_item_resource::Snapshot& output) const;
    Status status() const noexcept;

private:
    custom_item_resource::Ledger ledger_;
    Status status_{};
    bool prepared_ = false;
};

} // namespace fx::lantern_charge_router
