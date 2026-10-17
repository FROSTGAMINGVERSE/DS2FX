#pragma once
#include "../sdk/ds2fx_api.h"
#include <cstdint>
#include "hud_gauge.h"

namespace fx::hud_presentation {
// Exact reviewed Scholar image only (the runtime authenticates its hash).
// No engine calls, hooks or writes. The native renderer at RVA b6a08f
// consumes FeComponentObject+98 as byte alpha; live Equipment, Inventory
// and Video Settings snapshots show 255 -> 0 -> 255 on the health component.
template<class Read>
DS2FX_HudPresentation sample(uintptr_t image,uintptr_t base_slot,
    uintptr_t expected_player,const DS2FX_PlayerState& player,Read read) {
    DS2FX_HudPresentation result{sizeof(DS2FX_HudPresentation)};
    result.sample=player.sample;result.character_epoch=player.character_epoch;
    if(!player.available || !expected_player || !image) return result;
    uintptr_t owners[8]{};uintptr_t canonical=0;
    if(!read(base_slot,owners[0]) || !owners[0] ||
       !read(owners[0]+0xd0,canonical) || canonical!=expected_player) return result;
    constexpr uintptr_t offsets[]{0x22e0,0x10,0x10,0x8,0x28,0x30,0x38,0x38};
    constexpr uintptr_t tables[]{0,0x10fa628,0x10fab70,0x11d6420,
        0x11d6408,0x11de158,0x11de318,0x11ddfa8};
    uintptr_t targets[8]{};
    for(unsigned i=0;i<8;++i) {
        const auto owner=i ? targets[i-1] : owners[0];
        uintptr_t table=0;
        if(!read(owner+offsets[i],targets[i]) || !targets[i] ||
           (tables[i] && (!read(targets[i],table) || table!=image+tables[i]))) return result;
    }
    uint8_t alpha=0;int32_t disabled=0;
    if(!read(targets[7]+0x98,alpha) || !read(targets[3]+0x18,disabled) ||
       disabled < -32 || disabled>32) return result;
    // Reject a transition/reallocation anywhere in the ownership chain.
    uintptr_t after=0;
    if(!read(base_slot,after) || after!=owners[0] ||
       !read(owners[0]+0xd0,after) || after!=canonical) return result;
    for(unsigned i=0;i<8;++i) {
        const auto owner=i ? targets[i-1] : owners[0];uintptr_t table=0;
        if(!read(owner+offsets[i],after) || after!=targets[i] ||
           (tables[i] && (!read(targets[i],table) || table!=image+tables[i]))) return result;
    }
    result.available=1;result.opacity=disabled>0 ? 0u : alpha;
    hud_gauge::sample(owners[0],targets[0],player,result,read);
    return result;
}
}
