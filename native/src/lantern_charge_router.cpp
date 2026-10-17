#include "lantern_charge_router.h"

namespace fx::lantern_charge_router {

bool Router::prepare(uint32_t capacity) {
    if (prepared_ || !capacity) return false;
    if (!ledger_.define({wayfarer_lantern_item, capacity, true})) return false;
    prepared_ = true;
    return true;
}

custom_item_resource::UseResult Router::on_authenticated_toggle(uint64_t owner_token) {
    if (!prepared_ || !owner_token) {
        ++status_.rejected_receipts;
        return custom_item_resource::UseResult::unavailable;
    }
    if (!ledger_.bind_owned(owner_token, wayfarer_lantern_item)) {
        // Existing binding is expected after the first authenticated use.
        custom_item_resource::Snapshot existing{};
        if (!ledger_.snapshot(owner_token, wayfarer_lantern_item, existing)) {
            ++status_.rejected_receipts;
            return custom_item_resource::UseResult::unavailable;
        }
    }
    ++status_.toggle_receipts;
    return ledger_.use(owner_token, wayfarer_lantern_item);
}

bool Router::on_authenticated_bonfire(uint64_t owner_token) {
    if (!prepared_ || !owner_token) {
        ++status_.rejected_receipts;
        return false;
    }
    custom_item_resource::Snapshot existing{};
    if (!ledger_.snapshot(owner_token, wayfarer_lantern_item, existing)) {
        // Bonfire attendance must not grant a Lantern to a character that has
        // never produced an authenticated Lantern-use receipt.
        ++status_.rejected_receipts;
        return false;
    }
    ++status_.bonfire_receipts;
    status_.refills += ledger_.refill_at_bonfire(owner_token);
    return true;
}

bool Router::snapshot(uint64_t owner_token, custom_item_resource::Snapshot& output) const {
    return prepared_ && ledger_.snapshot(owner_token, wayfarer_lantern_item, output);
}

Status Router::status() const noexcept { return status_; }

} // namespace fx::lantern_charge_router
