#include "lantern_toggle_pipeline.h"
#include <cmath>
namespace fx::lantern_toggle {
namespace { bool fresh(const NativeSample& s) noexcept { return s.sampled_at_ms!=0 && lantern::valid(s.lantern); } }
Command Pipeline::sample(const NativeSample& input,double elapsed_seconds) noexcept {
    if(!fresh(input) || state_.awaiting_ack) {
        if(!fresh(input)) invalidate();
        return {};
    }
    state_.provider_ready=true;
    state_.sampled_at_ms=input.sampled_at_ms;
    state_.status=lantern::status(input.lantern);
    state_.model_visible=input.lantern.visible;
    state_.light_enabled=input.lantern.lit;
    const auto request=controller_.update(input.lantern,elapsed_seconds);
    state_.hold_fraction=controller_.fraction();
    if(request.action==lantern::Action::none) return {};
    pending_={++state_.nonce,request};
    state_.awaiting_ack=true;
    return pending_;
}
bool Pipeline::acknowledge(uint64_t nonce,const lantern::Snapshot& applied) noexcept {
    const bool valid=lantern::valid(applied) && applied.alive && applied.equipped && !applied.refilling &&
        applied.context_known && applied.context_clear && !applied.menu_open &&
        pending_.request.character_epoch==applied.character_epoch && pending_.request.revision<=applied.revision;
    const bool applied_action=pending_.request.action==lantern::Action::stow ?
        (!applied.visible && !applied.lit) : (applied.visible && applied.lit && applied.oil_seconds>0);
    if(!state_.awaiting_ack || nonce!=pending_.nonce || !valid || !applied_action) { invalidate(); return false; }
    // The command result is represented only after an executor has atomically made
    // the model/light/effect state authoritative in its supplied snapshot.
    state_.awaiting_ack=false;
    state_.sampled_at_ms=0;
    state_.hold_fraction=0;
    state_.status=lantern::status(applied);
    state_.model_visible=applied.visible;
    state_.light_enabled=applied.lit;
    pending_={};
    return true;
}
void Pipeline::invalidate() noexcept {
    controller_=lantern::HoldController{};
    state_={};
    pending_={};
}
}