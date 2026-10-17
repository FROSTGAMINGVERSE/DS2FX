#pragma once
#include "forge_settings.h"
#include <vector>
namespace fx::forge {
struct Option {const wchar_t* key;const wchar_t* label;int fallback,low,high,step;bool live;};
inline std::vector<Option> options(const std::string& id) {
    if(id=="ds2fx-core")return {
        {L"ObservationIntervalMs",L"Observation interval (ms)",100,50,100,25,false},
        {L"VerboseLanternTelemetry",L"Detailed Lantern diagnostics",0,0,1,1,false}};
    if(id=="ds2-remastered-hud")return {
        {L"ScalePercent",L"HUD size (%)",100,50,200,5,true},
        {L"Margin",L"Screen margin",24,4,128,2,true},
        {L"Opacity",L"HUD opacity",245,32,255,8,true},
        {L"Pulse",L"Bar pulse",1,0,1,1,true},
        {L"PulseStrength",L"Pulse strength (%)",100,0,100,10,true},
        {L"EmberStrength",L"Human embers (%)",60,0,100,10,true},
        {L"AnimationFPS",L"Animation rate",60,30,60,15,true},
        {L"StatusIconSize",L"Status artwork size",20,18,32,2,true},
        {L"HealthHoldMs",L"Damage trail delay (ms)",260,0,600,40,true},
        {L"HealthDrainMs",L"Damage drain time (ms)",250,80,600,40,true},
        {L"StaminaHoldMs",L"Stamina trail delay (ms)",160,0,400,40,true},
        {L"StaminaDrainMs",L"Stamina drain time (ms)",180,80,500,40,true},
        {L"StatBarLengths",L"Bar lengths follow player stats",1,0,1,1,true},
        {L"ShadowStrength",L"Frame shadow (%)",40,0,100,5,true},
        {L"SkinMode",L"Skin: Auto / Hollow / Human",0,0,2,1,true}};
    if(id=="wayfarers-lantern")return {
        {L"LanternFlickerPercent",L"Flame flicker (%)",10,0,12,1,false},
        {L"LanternDiffusePercent",L"Light brightness (%)",65,0,100,5,false},
        {L"LanternSpecularPercent",L"Metal reflection (%)",40,0,100,5,false},
        {L"LanternRadiusPercent",L"Light radius (%)",125,100,150,5,false}};
    return {};
}
inline int option_value(const std::string& id,const Option& o) {
    return setting(settings_file(),utf8_path(id).wstring().c_str(),o.key,o.fallback,o.low,o.high);
}
}
