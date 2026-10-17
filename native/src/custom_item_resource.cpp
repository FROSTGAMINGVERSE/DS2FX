#include "custom_item_resource.h"

namespace fx::custom_item_resource {

bool Ledger::define(const Definition& definition) {
    if (!definition.item_id || !definition.capacity || definition.capacity > 99) return false;
    std::lock_guard lock(mutex_);
    const auto [it, inserted] = definitions_.emplace(definition.item_id, definition);
    return inserted || (it->second.capacity == definition.capacity &&
                        it->second.refill_at_bonfire == definition.refill_at_bonfire);
}

bool Ledger::bind_owned(uint64_t owner_token, uint32_t item_id) {
    if (!owner_token || !item_id) return false;
    std::lock_guard lock(mutex_);
    const auto definition = definitions_.find(item_id);
    if (definition == definitions_.end()) return false;
    const auto [state, inserted] = states_.try_emplace({owner_token, item_id});
    if (inserted) {
        state->second.current = definition->second.capacity;
        state->second.revision = 1;
        state->second.active = false;
    }
    return true;
}

UseResult Ledger::use(uint64_t owner_token, uint32_t item_id) {
    if (!owner_token || !item_id) return UseResult::unavailable;
    std::lock_guard lock(mutex_);
    const auto definition = definitions_.find(item_id);
    const auto state = states_.find({owner_token, item_id});
    if (definition == definitions_.end() || state == states_.end()) return UseResult::unavailable;
    if (state->second.active) {
        state->second.active = false;
        ++state->second.revision;
        return UseResult::stowed;
    }
    if (!state->second.current) return UseResult::empty;
    --state->second.current;
    state->second.active = true;
    ++state->second.revision;
    return UseResult::activated;
}

uint32_t Ledger::refill_at_bonfire(uint64_t owner_token) {
    if (!owner_token) return 0;
    std::lock_guard lock(mutex_);
    uint32_t changed = 0;
    for (auto& [key, state] : states_) {
        if (key.first != owner_token) continue;
        const auto definition = definitions_.find(key.second);
        if (definition == definitions_.end() || !definition->second.refill_at_bonfire) continue;
        if (state.current == definition->second.capacity && !state.active) continue;
        state.current = definition->second.capacity;
        state.active = false;
        ++state.revision;
        ++changed;
    }
    return changed;
}

bool Ledger::snapshot(uint64_t owner_token, uint32_t item_id, Snapshot& output) const {
    output = Snapshot{};
    if (!owner_token || !item_id) return false;
    std::lock_guard lock(mutex_);
    const auto definition = definitions_.find(item_id);
    const auto state = states_.find({owner_token, item_id});
    if (definition == definitions_.end() || state == states_.end()) return false;
    output = {owner_token, item_id, state->second.current, definition->second.capacity,
              state->second.revision, true, state->second.active};
    return true;
}

uint32_t Ledger::forget_owner(uint64_t owner_token) {
    if (!owner_token) return 0;
    std::lock_guard lock(mutex_);
    uint32_t removed = 0;
    for (auto state = states_.begin(); state != states_.end();) {
        if (state->first.first == owner_token) {
            state = states_.erase(state);
            ++removed;
        } else {
            ++state;
        }
    }
    return removed;
}

} // namespace fx::custom_item_resource
