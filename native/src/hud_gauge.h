#pragma once
#include "../sdk/ds2fx_api.h"
#include <algorithm>
#include <cmath>
namespace fx::hud_gauge {
// Read-only translation of native FeSceneHp::update RVA50A730. Native
// widths interpolate between the installed level-1/99 stats and MenuGauge
// row1 lengths. No assumed conversion from engine SP to menu SP.
template<class Read> uintptr_t row(uintptr_t wrapper,uint32_t id,Read read) {
    uintptr_t data=0;uint16_t count=0;
    if(!wrapper || !read(wrapper+0x10,data)||!data||!read(data+0xa,count)||!count||count>10000)return 0;
    int lo=0,hi=count-1;
    while(lo<=hi){int mid=lo+(hi-lo)/2;uint32_t key=0;uint64_t offset=0;
        if(!read(data+0x40+mid*24,key))return 0;
        if(key<id)lo=mid+1;else if(key>id)hi=mid-1;
        else {uintptr_t after=0;uint16_t after_count=0;uint32_t after_key=0;
            if(!read(data+0x48+mid*24,offset)||offset<0x40+count*24||offset>0x2000000||
               !read(wrapper+0x10,after)||after!=data||!read(data+0xa,after_count)||after_count!=count||
               !read(data+0x40+mid*24,after_key)||after_key!=key)return 0;return data+offset;}}
    return 0;
}
inline float length(float current,float low,float high,float minimum,float maximum){
    const float upper=std::max(current,high);
    if(!std::isfinite(current)||!std::isfinite(low)||!std::isfinite(high)||!std::isfinite(minimum)||!std::isfinite(maximum)||low<0||high<=low||current<=0||minimum<=0||maximum<minimum||maximum>1)return 0;
    return minimum+(maximum-minimum)*std::clamp((current-low)/(upper-low),0.f,1.f);
}
template<class Read> void sample(uintptr_t base,uintptr_t allocator,const DS2FX_PlayerState& player,DS2FX_HudPresentation& out,Read read){
    uintptr_t params=0,physical=0,gauge=0,after=0;
    if(!read(base+0x18,params)||!params||!read(params+0x5a0,physical)||!physical)return;
    const auto first=row(physical+0xc8,1,read),last=row(physical+0xc8,99,read);if(!first||!last)return;
    int16_t hp_low=0,hp_high=0,sp_low=0,sp_high=0;uint8_t bonus_low=0,bonus_high=0;
    if(!read(first+4,hp_low)||!read(last+4,hp_high)||!read(first+6,sp_low)||!read(last+6,sp_high)||!read(first+0x8a,bonus_low)||!read(last+0x8a,bonus_high))return;
    float hp_min=0,hp_max=0,sp_min=0,sp_max=0;
    if(!read(allocator+0x280,gauge)||!gauge)return;
    const auto settings=row(gauge+0xc8,1,read);if(!settings || !read(settings+0x24,hp_min)||!read(settings+0x2c,hp_max)||!read(settings+0x28,sp_min)||!read(settings+0x30,sp_max))return;
    const auto hp=length(static_cast<float>(player.max_health),hp_low+8*bonus_low,hp_high+8*bonus_high,hp_min,hp_max);
    const auto sp=length(player.max_stamina,sp_low,sp_high,sp_min,sp_max);
    if(!hp||!sp||!read(base+0x18,after)||after!=params||!read(params+0x5a0,after)||after!=physical||
       !read(allocator+0x280,after)||after!=gauge)return;
    out.gauge_available=1;out.health_length=hp;out.stamina_length=sp;
}
}
