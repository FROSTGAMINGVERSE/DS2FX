#pragma once
#include <cstdint>
#include "player_jump_routes.h"
namespace jump_observer {
struct Receipt {
    uint64_t sequence=0, timestamp=0;
    uintptr_t player=0, controller=0;
    uint32_t message=0, frame=0, thread=0;
    uint8_t status=0;
    uintptr_t caller=0;
    fx::jump_routes::Movement movement{};
    fx::jump_routes::Route proposed_route=fx::jump_routes::Route::native;
    uint32_t dispatched_message=0;
};
bool install(uintptr_t slot, bool opt_in);
void disable() noexcept;
bool snapshot(Receipt&) noexcept;
using ThreadAuthority=bool(*)() noexcept;
// Core-only. Enable only after authenticating the exact routed content. The
// authority callback reads the independently hash-gated native update hook.
// The exact pinned jump call site also authorizes native animation jobs solely
// to select this existing dispatch; it does not enable a generic API executor.
void configure_jump_routes(bool,ThreadAuthority) noexcept;
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t slot, uintptr_t image, void* target, bool opt_in);
void authorize_fixture_caller(uintptr_t) noexcept;
#endif
}
