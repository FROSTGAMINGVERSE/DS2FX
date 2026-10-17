#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

// Pure controller contract. The backend supplies game-owned oil and light state.
// This controller never invents loot, decrements a second clock, writes inventory,
// or treats an ordinary torch-timer increase as an oil pickup.
namespace lantern {
enum class Status { unavailable, off, lit, low_oil, empty, refilling };
enum class Action { none, ignite, stow };
struct Snapshot {
    bool charge_mode = false;
    bool available = false;
    bool alive = false;
    bool equipped = false;
    bool lit = false;
    // Belt visibility is independent of flame/oil: an exhausted lantern remains
    // visible until the player deliberately stows it.
    bool visible = true;
    bool refilling = false;
    bool context_known = false;
    bool context_clear = false;
    bool menu_open = true;
    bool interact_held = false;
    uint64_t character_epoch = 0;
    uint64_t revision = 0;
    double oil_seconds = 0;
    double capacity_seconds = 1800;
};
struct Request {
    Action action = Action::none;
    uint64_t character_epoch = 0;
    uint64_t revision = 0;
};
inline bool valid(const Snapshot& s) {
    return s.available && s.character_epoch != 0 &&
        std::isfinite(s.oil_seconds) && std::isfinite(s.capacity_seconds) &&
        s.capacity_seconds > 0 && s.oil_seconds >= 0 && s.oil_seconds <= s.capacity_seconds;
}
inline Status status(const Snapshot& s) {
    if (!valid(s) || !s.alive || !s.equipped) return Status::unavailable;
    if (s.refilling) return Status::refilling;
    if (s.oil_seconds <= 0 && !(s.charge_mode && s.lit)) return Status::empty;
    if (!s.lit) return Status::off;
    return s.oil_seconds <= s.capacity_seconds * 0.25 ? Status::low_oil : Status::lit;
}
inline const wchar_t* label(Status s) {
    switch (s) {
        case Status::off: return L"OFF";
        case Status::lit: return L"LIT";
        case Status::low_oil: return L"LOW OIL";
        case Status::empty: return L"EMPTY";
        case Status::refilling: return L"REFILLING";
        default: return L"UNAVAILABLE";
    }
}
class HoldController {
    uint64_t epoch_ = 0, revision_ = 0;
    double hold_ = 0;
    // A native release is required after startup, reload, interaction or completion.
    bool latched_ = true;
public:
    double fraction() const { return std::clamp(hold_ / 2.0, 0.0, 1.0); }
    Request update(const Snapshot& s, double elapsed_seconds) {
        const bool discontinuity = s.character_epoch != epoch_ || s.revision < revision_;
        epoch_ = s.character_epoch;
        revision_ = s.revision;
        const bool eligible = valid(s) && s.alive && s.equipped && !s.refilling &&
            s.context_known && s.context_clear && !s.menu_open;
        // Large gaps are not proof that the action remained safe while unobserved.
        if (discontinuity || !eligible || !std::isfinite(elapsed_seconds) ||
            elapsed_seconds < 0 || elapsed_seconds > 0.25) {
            hold_ = 0; latched_ = true; return {};
        }
        if (!s.interact_held) { hold_ = 0; latched_ = false; return {}; }
        if (latched_) return {};
        hold_ += elapsed_seconds;
        if (hold_ + 1e-9 < 2.0) return {};
        hold_ = 0; latched_ = true;
        if (s.visible) return {Action::stow, s.character_epoch, s.revision};
        if (s.oil_seconds <= 0) return {};
        return {Action::ignite, s.character_epoch, s.revision};
    }
};
// Recheck this on the actual game thread, immediately before an action. Requests
// are one-shot; a provider must not retry stale requests against a new character.
inline bool accepts(const Request& r, const Snapshot& now) {
    if (r.action == Action::none || !valid(now) || !now.alive || !now.equipped ||
        now.refilling || !now.context_known || !now.context_clear || now.menu_open ||
        r.character_epoch != now.character_epoch || r.revision != now.revision) return false;
    return r.action == Action::ignite ? !now.visible && !now.lit && now.oil_seconds > 0 : now.visible;
}
}
