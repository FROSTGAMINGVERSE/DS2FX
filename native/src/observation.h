#pragma once
#include "../sdk/ds2fx_api.h"
#include <cmath>
#include <vector>

namespace fx {
// Lethal hits can retain negative raw HP during the death animation.
// Validate the bounded value before projecting it to the SDK's zero-HP death state.
inline bool valid_health(int32_t value,int32_t maximum) {
    return maximum > 0 && maximum <= 1000000 && value >= -1000000 && value <= maximum;
}
inline int32_t observed_health(int32_t value) { return value < 0 ? 0 : value; }
inline bool valid_stamina(float value,float maximum) {
    return std::isfinite(value) && std::isfinite(maximum) && maximum > 0 && maximum <= 1000000 &&
        value >= -maximum*10 && value <= maximum;
}
// Polling observations, not engine notifications. A quick transition can be missed.
// Pointer replacement emits unavailable(old), then available(new), with a new epoch.
template<class Emit> void transitions(const DS2FX_PlayerState& previous,const DS2FX_PlayerState& next,Emit emit) {
    const bool replaced = previous.available && next.available && previous.character_epoch != next.character_epoch;
    if (previous.available && (!next.available || replaced)) {
        auto absent = previous; absent.available = 0; absent.stamina_available = 0; absent.sample = next.sample;
        emit(DS2FX_PLAYER_UNAVAILABLE,absent);
    }
    if (next.available && (!previous.available || replaced)) emit(DS2FX_PLAYER_AVAILABLE,next);
    if (previous.available && next.available && !replaced) {
        if (next.health != previous.health || next.max_health != previous.max_health) emit(DS2FX_HEALTH_CHANGED,next);
        if (previous.health > 0 && next.health == 0) emit(DS2FX_PLAYER_DIED,next);
        if (previous.health == 0 && next.health > 0) emit(DS2FX_PLAYER_REVIVED,next);
        if (next.stamina_available != previous.stamina_available ||
            (next.stamina_available && (next.stamina != previous.stamina || next.max_stamina != previous.max_stamina)))
            emit(DS2FX_STAMINA_CHANGED,next);
    }
    emit(DS2FX_TICK,next);
}
}
