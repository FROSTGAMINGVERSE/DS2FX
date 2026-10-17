#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <utility>

// Core-owned state machine for content-defined, Estus-like resources.  It has
// no engine calls, input hooks, file I/O or save identity policy: callers must
// establish all of those separately.  In particular, an OwnerToken is only a
// runtime scope token until a verified persistent character identity exists.
namespace fx::custom_item_resource {

enum class UseResult : uint32_t {
    unavailable = 0, // Unknown resource or owner; do not change presentation.
    activated = 1,   // One charge was committed and the bound item became active.
    stowed = 2,      // The bound item was active and was stowed without a charge.
    empty = 3,       // No charge exists; force the bound item inactive.
};

struct Definition {
    uint32_t item_id = 0;
    uint32_t capacity = 0;
    bool refill_at_bonfire = false;
};

// A copied, pointer-free view suitable for a future HUD/API boundary.
struct Snapshot {
    uint64_t owner_token = 0;
    uint32_t item_id = 0;
    uint32_t current = 0;
    uint32_t capacity = 0;
    uint64_t revision = 0;
    bool available = false;
    bool active = false;
};

class Ledger final {
public:
    // A definition is immutable once accepted. Re-registering the identical
    // definition is harmless; a conflicting redefinition is rejected.
    bool define(const Definition& definition);

    // Binds a resource only after a caller has independently authenticated
    // ownership (for example, an exact active-item event). New bindings begin
    // full. This deliberately does not infer ownership from an item ID.
    bool bind_owned(uint64_t owner_token, uint32_t item_id);

    // Toggle policy used by reusable items: activating spends exactly one
    // charge, while stowing never spends a charge.
    UseResult use(uint64_t owner_token, uint32_t item_id);

    // Applies a previously authenticated local bonfire-rest event. All
    // matching resources for this owner refill and stow before the next use.
    uint32_t refill_at_bonfire(uint64_t owner_token);

    bool snapshot(uint64_t owner_token, uint32_t item_id, Snapshot& output) const;
    uint32_t forget_owner(uint64_t owner_token);

private:
    struct State {
        uint32_t current = 0;
        uint64_t revision = 0;
        bool active = false;
    };

    using Key = std::pair<uint64_t, uint32_t>;
    mutable std::mutex mutex_;
    std::map<uint32_t, Definition> definitions_;
    std::map<Key, State> states_;
};

} // namespace fx::custom_item_resource
