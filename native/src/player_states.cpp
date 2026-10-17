#include "player_states.h"
#include <algorithm>
#include <atomic>

namespace fx::player_states {
Service::Entry* Service::find(Owner owner, uint32_t handle) {
    for(auto& entry:entries_) if(entry.owner==owner && entry.handle==handle)return &entry;
    return nullptr;
}
uint32_t Service::add(Owner owner,const DS2FX_StateDefinition& d) {
    std::lock_guard lock(mutex_);
    if(disabled_ || !owner || d.size!=sizeof(d) || !d.local_id || d.reserved ||
       d.motion>DS2FX_STATE_FORWARD_JUMP || d.maximum_duration_ms<100 ||
       d.maximum_duration_ms>10000 || (d.motion!=DS2FX_STATE_NATIVE_PASS && !d.animation_asset_id))return 0;
    for(const auto& e:entries_) if(e.owner==owner &&
        (e.committed || e.definition.local_id==d.local_id))return 0;
    for(auto& e:entries_) if(!e.owner) {
        if(!next_handle_)return 0;
        e={owner,next_handle_++,false,d}; return e.handle;
    }
    return 0;
}
void Service::commit(Owner owner) {
    std::lock_guard lock(mutex_);
    if(disabled_)return;
    for(auto& e:entries_)if(e.owner==owner)e.committed=true;
}
uint32_t Service::routes(Owner owner,const DS2FX_JumpRoutes& r) {
    std::lock_guard lock(mutex_);
    if(disabled_ || !owner || r.size!=sizeof(r) || route_owner_)return DS2FX_STATE_INVALID;
    auto idle=find(owner,r.idle_handle), moving=find(owner,r.moving_handle), sprint=find(owner,r.sprint_handle);
    if(!idle || !moving || !sprint || idle->committed || moving->committed || sprint->committed ||
       idle->definition.motion!=DS2FX_STATE_VERTICAL_JUMP ||
       moving->definition.motion!=DS2FX_STATE_FORWARD_JUMP ||
       sprint->definition.motion!=DS2FX_STATE_NATIVE_PASS)return DS2FX_STATE_INVALID;
    routes_=r;route_owner_=owner;return DS2FX_STATE_OK;
}
void Service::finish(uint32_t result,uint32_t phase) {
    result_=result; phase_=phase; deadline_=0; ++sequence_;
}
uint32_t Service::enqueue(Owner owner,const DS2FX_StateRequest& r,uint64_t now) {
    auto e=find(owner,r.handle);
    if(disabled_ || !e || !e->committed || r.size!=sizeof(r) || r.reserved ||
       !r.expires_after_ms || r.expires_after_ms>250)return DS2FX_STATE_INVALID;
    if(!epoch_ || r.character_epoch!=epoch_)return DS2FX_STATE_STALE;
    if(!eligible_)return DS2FX_STATE_INELIGIBLE;
    if(phase_==DS2FX_STATE_QUEUED || phase_==DS2FX_STATE_ACTIVE)return DS2FX_STATE_BUSY;
    if(e->definition.motion==DS2FX_STATE_NATIVE_PASS)return DS2FX_STATE_NATIVE_PASSTHROUGH;
    if(!executor_ || !executor_->ready())return DS2FX_STATE_EXECUTOR_UNAVAILABLE;
    if(now>UINT64_MAX-r.expires_after_ms)return DS2FX_STATE_INVALID;
    action_owner_=owner; handle_=r.handle;deadline_=now+r.expires_after_ms;
    phase_=DS2FX_STATE_QUEUED;result_=DS2FX_STATE_OK;++sequence_;
    return DS2FX_STATE_OK;
}
uint32_t Service::request(Owner owner,const DS2FX_StateRequest& r,uint64_t now) {
    std::lock_guard lock(mutex_);return enqueue(owner,r,now);
}
DS2FX_StateServiceStatus Service::status(Owner owner) {
    std::lock_guard lock(mutex_);
    DS2FX_StateServiceStatus s{sizeof(s)};
    s.executor_available=!disabled_ && executor_ && executor_->ready();
    for(const auto& e:entries_)if(e.owner==owner && e.committed)++s.registered_states;
    s.jump_routes_registered=route_owner_==owner && s.registered_states!=0;
    s.character_epoch=epoch_;s.sequence=sequence_;
    if(action_owner_==owner){s.handle=handle_;s.phase=phase_;s.result=result_;}
    else if(!s.executor_available)s.result=DS2FX_STATE_EXECUTOR_UNAVAILABLE;
    return s;
}
void Service::observe(uint64_t epoch,bool eligible) {
    std::lock_guard lock(mutex_);
    if(epoch!=epoch_ || !eligible) {
        if(phase_==DS2FX_STATE_QUEUED || phase_==DS2FX_STATE_ACTIVE)
            finish(DS2FX_STATE_CANCELLED,DS2FX_STATE_ABORTED);
        jump_held_=true; // Require a known release after any identity/context gap.
    }
    epoch_=epoch;eligible_=eligible && epoch && !disabled_;
}
void Service::revoke_all() {
    std::lock_guard lock(mutex_);
    disabled_=true;eligible_=false;route_owner_=0;entries_={};jump_held_=true;
    if(phase_==DS2FX_STATE_QUEUED || phase_==DS2FX_STATE_ACTIVE)
        finish(DS2FX_STATE_CANCELLED,DS2FX_STATE_ABORTED);
}
void Service::bind(Executor* executor) {
    std::lock_guard lock(mutex_);executor_=executor;
}
void Service::tick(const Context& c) noexcept {
    std::unique_lock lock(mutex_,std::try_to_lock);
    if(!lock || disabled_)return;
    // A foreign thread or owner cannot cancel or mutate canonical state.
    if(!c.game_thread || !c.canonical)return;
    if(!eligible_ || c.epoch!=epoch_ || !c.alive || !c.playable || !c.context_known) {
        jump_held_=true;
        if(phase_==DS2FX_STATE_QUEUED || phase_==DS2FX_STATE_ACTIVE)
            finish(DS2FX_STATE_CANCELLED,DS2FX_STATE_ABORTED);
        return;
    }
    if(phase_==DS2FX_STATE_ACTIVE) {
        if(!executor_ || !executor_->ready() || c.now>=deadline_)
            finish(DS2FX_STATE_CANCELLED,DS2FX_STATE_ABORTED);
        else {
            const auto receipt=executor_->poll(handle_,c);
            if(receipt==Receipt::completed)finish(DS2FX_STATE_OK,DS2FX_STATE_COMPLETED);
            else if(receipt==Receipt::rejected)finish(DS2FX_STATE_EXECUTION_REJECTED,DS2FX_STATE_ABORTED);
        }
    }
    const bool edge=c.input_known && c.jump_held && !jump_held_;
    jump_held_=!c.input_known || c.jump_held;
    if(edge && c.grounded && route_owner_) {
        const auto selected=c.sprinting?routes_.sprint_handle:(c.moving?routes_.moving_handle:routes_.idle_handle);
        const DS2FX_StateRequest request{sizeof(request),selected,c.epoch,100,0};
        const auto result=enqueue(route_owner_,request,c.now);
        if(result!=DS2FX_STATE_OK && result!=DS2FX_STATE_BUSY) {
            action_owner_=route_owner_;handle_=selected;
            finish(result,result==DS2FX_STATE_NATIVE_PASSTHROUGH?DS2FX_STATE_COMPLETED:DS2FX_STATE_ABORTED);
        }
    }
    if(phase_!=DS2FX_STATE_QUEUED)return;
    auto e=find(action_owner_,handle_);
    if(c.now>=deadline_)finish(DS2FX_STATE_EXPIRED,DS2FX_STATE_ABORTED);
    else if(!c.grounded)finish(DS2FX_STATE_INELIGIBLE,DS2FX_STATE_ABORTED);
    else if(!e || !executor_ || !executor_->ready())finish(DS2FX_STATE_EXECUTOR_UNAVAILABLE,DS2FX_STATE_ABORTED);
    else if(c.now>UINT64_MAX-e->definition.maximum_duration_ms)finish(DS2FX_STATE_INVALID,DS2FX_STATE_ABORTED);
    else {
        const auto receipt=executor_->start(e->definition,handle_,c);
        if(receipt==Receipt::running) {
            phase_=DS2FX_STATE_ACTIVE;result_=DS2FX_STATE_OK;
            deadline_=c.now+e->definition.maximum_duration_ms;++sequence_;
        } else if(receipt==Receipt::completed)finish(DS2FX_STATE_OK,DS2FX_STATE_COMPLETED);
        else finish(DS2FX_STATE_EXECUTION_REJECTED,DS2FX_STATE_ABORTED);
    }
}
Service& service() { static Service instance;return instance; }
namespace { std::atomic<NativeAdapter*> adapter{nullptr}; }
void bind_native_adapter(NativeAdapter* value) noexcept {
    // Core binds once before hooks/worker startup; adapters remain resident.
    service().bind(value);adapter.store(value,std::memory_order_release);
}
void native_tick(uintptr_t manager) noexcept {
    auto value=adapter.load(std::memory_order_acquire);
    if(!value || !value->ready())return;
    Context context{};
    if(value->sample(manager,context))service().tick(context);
}
}
