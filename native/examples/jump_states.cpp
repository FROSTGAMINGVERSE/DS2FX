#include "../sdk/ds2fx_api.h"
#ifdef DS2FX_STATE_FIXTURE
#include <thread>
#endif

namespace {
const DS2FX_Api* host=nullptr;
bool reported=false;
void event(uint32_t,const DS2FX_PlayerState*) {
    if(reported)return;
    DS2FX_StateServiceStatus status{sizeof(status)};
    if(host->get_player_state_service(&status)) {
        host->log("ds2-jump-states",status.executor_available?
            "State executor available":"States registered; native jump executor unavailable; original jumps remain active");
        reported=true;
    }
}
}
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* api,DS2FX_Plugin* plugin) {
    if(!api || !plugin || api->version!=DS2FX_API_VERSION ||
       api->size<offsetof(DS2FX_Api,register_player_jump_routes)+sizeof(api->register_player_jump_routes) ||
       !api->register_player_state || !api->register_player_jump_routes || !api->get_player_state_service)return 0;
    const DS2FX_StateDefinition idle{sizeof(idle),1,DS2FX_STATE_VERTICAL_JUMP,948,2500,0};
    // The local content builder reserves995 for the converted walking clip.
    // Original947 remains the sprint takeoff. This example stays uninstalled
    // until its graph/physics executor is bound and validated.
    const DS2FX_StateDefinition moving{sizeof(moving),2,DS2FX_STATE_FORWARD_JUMP,995,2500,0};
    const DS2FX_StateDefinition sprint{sizeof(sprint),3,DS2FX_STATE_NATIVE_PASS,0,2500,0};
#ifdef DS2FX_STATE_FIXTURE
    uint32_t foreign_thread_handle=0;
    std::thread foreign([&]{foreign_thread_handle=api->register_player_state(&idle);});
    foreign.join();
    if(foreign_thread_handle)return 0;
#endif
    const auto idle_handle=api->register_player_state(&idle);
    const auto moving_handle=api->register_player_state(&moving);
    const auto sprint_handle=api->register_player_state(&sprint);
    if(!idle_handle || !moving_handle || !sprint_handle)return 0;
    const DS2FX_JumpRoutes routes{sizeof(routes),idle_handle,moving_handle,sprint_handle};
    if(api->register_player_jump_routes(&routes)!=DS2FX_STATE_OK)return 0;
    host=api;
    *plugin={sizeof(*plugin),DS2FX_API_VERSION,"ds2-jump-states","0.1.0",event};return 1;
}
#ifdef DS2FX_STATE_FIXTURE
/* Owned DLL integration fixture only. Not present in the example DLL. */
DS2FX_EXPORT const DS2FX_Api* DS2FX_TestStateApi() { return host; }
DS2FX_EXPORT uint32_t DS2FX_TestStateOwner() {
    DS2FX_StateServiceStatus status{sizeof(status)};
    const DS2FX_StateDefinition late{sizeof(late),4,DS2FX_STATE_ANIMATION,947,1000,0};
    return host && host->get_player_state_service(&status) && status.registered_states==3 &&
        status.jump_routes_registered==1 && status.executor_available==0 &&
        host->register_player_state(&late)==0;
}
#endif
