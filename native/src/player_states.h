#pragma once
#include "../sdk/ds2fx_player_states.h"
#include <array>
#include <mutex>

namespace fx::player_states {
using Owner = uintptr_t;
/* Core-internal context. Only a proven native adapter may assert these gates;
   the 100ms observation snapshot cannot authorize engine actions. */
struct Context {
    uint64_t epoch=0, now=0;
    bool game_thread=false, canonical=false, alive=false, playable=false;
    bool context_known=false, grounded=false, input_known=false;
    bool jump_held=false, moving=false, sprinting=false;
};
enum class Receipt { rejected, running, completed };
struct Executor {
    virtual ~Executor() = default;
    virtual bool ready() const noexcept = 0;
    virtual Receipt start(const DS2FX_StateDefinition&, uint32_t handle,
                          const Context&) noexcept = 0;
    virtual Receipt poll(uint32_t handle, const Context&) noexcept = 0;
};
struct NativeAdapter : Executor {
    virtual bool sample(uintptr_t canonical_manager, Context&) noexcept = 0;
};
/* Fixed storage, no allocation in the tick. Native tick uses try_lock and
   skips contention instead of waiting on plugin/observation work. */
class Service {
public:
    uint32_t add(Owner, const DS2FX_StateDefinition&);
    void commit(Owner);
    uint32_t routes(Owner, const DS2FX_JumpRoutes&);
    uint32_t request(Owner, const DS2FX_StateRequest&, uint64_t now);
    DS2FX_StateServiceStatus status(Owner);
    void observe(uint64_t epoch, bool eligible);
    void revoke_all();
    /* Binding is Core-only, before worker/tick startup. Never a plugin API. */
    void bind(Executor* executor);
    void tick(const Context&) noexcept;
private:
    struct Entry { Owner owner=0; uint32_t handle=0; bool committed=false;
                   DS2FX_StateDefinition definition{}; };
    std::array<Entry,32> entries_{};
    std::mutex mutex_;
    Executor* executor_=nullptr;
    uint32_t next_handle_=1;
    uint64_t epoch_=0, sequence_=0, deadline_=0;
    bool eligible_=false, disabled_=false, jump_held_=true;
    Owner route_owner_=0, action_owner_=0;
    DS2FX_JumpRoutes routes_{};
    uint32_t handle_=0, phase_=0, result_=0;
    Entry* find(Owner, uint32_t);
    void finish(uint32_t result, uint32_t phase);
    uint32_t enqueue(Owner, const DS2FX_StateRequest&, uint64_t now);
};
Service& service();
void bind_native_adapter(NativeAdapter*) noexcept;
/* Production has no bound executor until native state/physics dispatch is
   proven. This hook deliberately cannot turn observation into action. */
void native_tick(uintptr_t canonical_manager) noexcept;
}
