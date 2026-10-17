#include "ds2fx_api.h"
#include <cstdio>
namespace {
const DS2FX_Api* api = nullptr;
void event(uint32_t kind,const DS2FX_PlayerState* state) {
    if (kind == DS2FX_TICK) return;
    char message[320];
    const char* name = "unknown";
    switch (kind) {
        case DS2FX_PLAYER_AVAILABLE: name="player_available"; break;
        case DS2FX_PLAYER_UNAVAILABLE: name="player_unavailable"; break;
        case DS2FX_HEALTH_CHANGED: name="health_changed"; break;
        case DS2FX_STAMINA_CHANGED: name="stamina_changed"; break;
        case DS2FX_PLAYER_DIED: name="player_died"; break;
        case DS2FX_PLAYER_REVIVED: name="player_revived"; break;
    }
    std::snprintf(message,sizeof(message),"%s sample=%llu health=%d max_health=%d epoch=%llu stamina_valid=%u stamina=%.2f/%.2f",name,static_cast<unsigned long long>(state->sample),state->health,state->max_health,static_cast<unsigned long long>(state->character_epoch),state->stamina_available,state->stamina,state->max_stamina);
    api->log("frost-player-observer",message);
}
}
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* host,DS2FX_Plugin* output) {
    if (!host || host->size < sizeof(DS2FX_Api) || host->version != DS2FX_API_VERSION || !output || output->size != sizeof(DS2FX_Plugin)) return 0;
    api = host;
    *output = {sizeof(DS2FX_Plugin),DS2FX_API_VERSION,"frost-player-observer","0.2.0",event};
    api->log("frost-player-observer","Initialized using DS2FX API 2; observes character lifecycle, HP and stamina without changing the game");
    return 1;
}
