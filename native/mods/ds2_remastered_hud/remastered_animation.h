#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace remastered {
struct Heartbeat {
    float phase=0,signal=0;uint64_t last=0;
    static float rate(float hp){const float stress=1-std::clamp(hp,0.f,1.f);return 48+102*stress*stress;}
    static float strength(float hp){const float stress=1-std::clamp(hp,0.f,1.f);return .14f+.86f*stress*stress;}
    static float wave(float phase){
        const float first=(phase-.08f)/.035f,second=(phase-.23f)/.045f;
        return std::exp(-first*first)+.55f*std::exp(-second*second);
    }
    void reset(){phase=signal=0;last=0;}
    void update(float hp,uint64_t now){
        if(!last || now<last || now-last>1500)phase=0;
        else phase=std::fmod(phase+static_cast<float>(now-last)/1000.f*rate(hp)/60.f,1.f);
        last=now;signal=hp>0?wave(phase)*strength(hp):0;
    }
};
// Presentation only: the actual value changes immediately; the lost segment
// holds briefly, then eases down. Never writes character state.
struct DrainTrail {
    float value=0,previous=0;
    uint64_t last=0,hold_until=0;
    bool ready=false;
    void reset(){ready=false;}
    float update(float actual,uint64_t now,uint64_t hold,float seconds) {
        actual=std::clamp(actual,0.0f,1.0f);
        if(!ready || now<last || now-last>1500) {
            ready=true;value=previous=actual;last=now;hold_until=now;return value;
        }
        if(actual<previous-0.0001f)hold_until=now+hold;
        if(actual>=value)value=actual;
        else if(now>hold_until) {
            const auto from=std::max(last,hold_until);
            const float dt=static_cast<float>(now-from)/1000.0f;
            value=actual+(value-actual)*std::exp(-dt/seconds);
            if(value-actual<0.001f)value=actual;
        }
        previous=actual;last=now;return value;
    }
};
struct BarAnimation {
    DrainTrail health,stamina;
    Heartbeat heartbeat;
    uint64_t stamina_spent_at=0;
    float stamina_flash=0;
    static float consumption_flash(float spent,float elapsed_ms){return spent>0 ? std::min(1.f,spent/0.12f)*std::exp(-std::max(0.f,elapsed_ms)/220.f) : 0.f;}
    float health_fill=0,stamina_fill=0;uint64_t filled_at=0;
    uint64_t epoch=0;int maximum=0;float stamina_maximum=0;
    void reset(){health.reset();stamina.reset();heartbeat.reset();filled_at=0;stamina_spent_at=0;stamina_flash=0;}
    void update(uint64_t character,int hp_max,float sp_max,float hp,float sp,uint64_t now,uint64_t hp_hold=260,int hp_drain=250,uint64_t sp_hold=160,int sp_drain=180) {
        if(epoch!=character || maximum!=hp_max || stamina_maximum!=sp_max)reset();
        epoch=character;maximum=hp_max;stamina_maximum=sp_max;
        if(!filled_at || now<filled_at || now-filled_at>1500){health_fill=hp;stamina_fill=sp;}
        else {
            const float blend=1-std::exp(-static_cast<float>(now-filled_at)/60.f);
            // Consumption is immediate. Recovery glides between 100ms samples.
            health_fill=hp<health_fill?hp:health_fill+(hp-health_fill)*blend;
            stamina_fill=sp<stamina_fill?sp:stamina_fill+(sp-stamina_fill)*blend;
        }
        filled_at=now;
        // A short neutral highlight follows newly consumed energy, never the live fill.
        if(stamina.ready && now>=stamina.last && now-stamina.last<=1500 && sp<stamina.previous-0.0001f){
            stamina_flash=std::max(stamina_flash,consumption_flash(stamina.previous-sp,0));stamina_spent_at=now;
        } else if(stamina_spent_at && now>=stamina_spent_at){
            stamina_flash*=std::exp(-static_cast<float>(now-stamina_spent_at)/220.f);stamina_spent_at=now;
        }
        health.update(hp,now,hp_hold,hp_drain/1000.0f);stamina.update(sp,now,sp_hold,sp_drain/1000.0f);
        if(stamina.value<=sp+.001f)stamina_flash=0;
        heartbeat.update(hp,now);
    }
};
}
