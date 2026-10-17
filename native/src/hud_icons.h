#pragma once
#include "hud_replacement_policy.h"
#include "../sdk/ds2fx_api.h"
#include <cstring>

namespace fx::hud_icons {
struct SampleDiagnostics { unsigned stage=0,row=20;uint32_t hidden=0;int32_t disabled=0; };
struct EffectsIdentity {
    uintptr_t base=0,player=0,header=0,sprite=0;
    std::array<uintptr_t,8> chain{};
    std::array<uintptr_t,20> rows{},leaves{};
    bool operator==(const EffectsIdentity&) const = default;
};
template<class Read> bool effects_chain(uintptr_t image,uintptr_t slot,uintptr_t player,EffectsIdentity& v,Read read) {
    if(!player || !read(slot,v.base) || !v.base || !read(v.base+0xd0,v.player) || v.player!=player)return false;
    constexpr uintptr_t offsets[]{0x22e0,0x10,0x38,8,0x28,0x30,0x38,0x38};
    constexpr uintptr_t tables[]{0x10c0768,0x10fa628,0x10b1278,0x11d6420,0x11d6408,0x11de158,0x11de318,0x11ddfa8};
    for(unsigned i=0;i<8;++i) {
        uintptr_t table=0;
        if(!read((i?v.chain[i-1]:v.base)+offsets[i],v.chain[i]) || !v.chain[i] ||
           !read(v.chain[i],table) || table!=image+tables[i])return false;
    }
    uintptr_t self=0,table=0;uint64_t count=0;uint32_t slots=0;
    return read(v.chain[2]+0xb8,slots) && slots==20 &&
        read(v.chain[2]+0x220,v.header) && v.header && read(v.header,self) && self==v.header &&
        read(v.header+8,count) && count==20 && read(v.chain[7]+0x38,v.sprite) && v.sprite &&
        read(v.sprite,table) && table==image+0x11de318;
}
template<class Read> bool effect_row(uintptr_t image,EffectsIdentity& v,unsigned i,Read read) {
    uintptr_t table=0,parent=0,resource=0;uint32_t id=0;
    return i<20 && read(v.header+0x10+i*16,v.rows[i]) && v.rows[i] &&
        read(v.header+0x1c+i*16,id) && id==0x5f5c3e0+i &&
        read(v.rows[i],table) && table==image+0x11ddfa8 &&
        read(v.rows[i]+0x18,parent) && parent==v.sprite &&
        read(v.rows[i]+0x48,resource) && resource && read(resource+0x1c,id) && id==0x5f5c3e0+i &&
        read(v.rows[i]+0x38,v.leaves[i]) && v.leaves[i] &&
        read(v.leaves[i],table) && table==image+0x11dea18 &&
        read(v.leaves[i]+0x18,parent) && parent==v.rows[i];
}
template<class Read> bool identify_effects(uintptr_t image,uintptr_t slot,uintptr_t player,EffectsIdentity& out,Read read) {
    EffectsIdentity v{};if(!effects_chain(image,slot,player,v,read))return false;
    for(unsigned i=0;i<20;++i)if(!effect_row(image,v,i,read))return false;
    out=v;return true;
}
template<class Read> bool icon(uintptr_t image,uintptr_t leaf,uint32_t type,DS2FX_HudIcon& out,Read read) {
    uintptr_t table=0,provider=0,definition=0,colors=0;uint16_t count=0;uint8_t selected=0;
    if(!read(leaf,table) || table!=image+0x11dea18 ||
       !read(leaf+0x68,provider) || !provider || !read(provider,table) || table!=image+0x10af1a0 ||
       !read(provider+0x18,out.type) || out.type!=type || !read(provider+0x1c,out.id) || !out.id || out.id>9999 ||
       !read(leaf+0x40,definition) || !definition || !read(definition+2,count) || count!=1 ||
       !read(leaf+0x7a,selected) || selected!=0 || !read(leaf+0x60,colors) || !colors ||
       !read(colors,out.tint))return false;
    // The native GPU texture is transiently absent even while this owned
    // provider retains its original icon key. Our independently hash-checked
    // 64x64 asset cache supplies pixels; authenticate the stable provider key.
    uintptr_t provider_after=0;uint32_t type_after=0,id_after=0;
    return read(leaf+0x68,provider_after) && provider_after==provider &&
        read(provider+0x18,type_after) && type_after==out.type &&
        read(provider+0x1c,id_after) && id_after==out.id;
}
// Find the sole icon in the covenant's authenticated, bounded component branch.
template<class Read> bool covenant_leaf(uintptr_t image,uintptr_t node,uintptr_t& leaf,unsigned depth,Read read) {
    if(depth>8 || !node)return false;
    uintptr_t table=0,child=0,parent=0,sibling=0;
    if(!read(node,table))return false;
    if(table==image+0x11dea18){if(leaf)return false;leaf=node;return true;}
    if(table!=image+0x11ddfa8 && table!=image+0x11de318)return false;
    if(!read(node+0x38,child) || !child)return false;
    // This reviewed covenant branch contains a single child at each level.
    return read(child+0x18,parent) && parent==node && read(child+0x28,sibling) && !sibling &&
        covenant_leaf(image,child,leaf,depth+1,read);
}
template<class Read> DS2FX_HudIcons sample(uintptr_t image,uintptr_t slot,uintptr_t expected_player,
    const DS2FX_PlayerState& player,Read read,SampleDiagnostics* diagnostic=nullptr) {
    SampleDiagnostics detail{};
    DS2FX_HudIcons result{sizeof(DS2FX_HudIcons)};
    result.sample=player.sample;result.character_epoch=player.character_epoch;
    if(!player.available)return result;
    hud_replacement::Identity hp{},hp_after{};
    if(hud_replacement::identify(image,slot,expected_player,hp,read)) {
        uintptr_t leaf=0,param=0,param_after=0;uint8_t alpha=0,current=255,desired=255,covenant=255,after=255;
        const bool settled=read(expected_player+0x490,param) && param && read(param+0x1ad,covenant) && covenant<=9 &&
            read(hp.chain[2]+0xe0,current) && read(hp.chain[2]+0xe1,desired) && current==covenant && desired==covenant;
        bool mapped=false;
        if(settled && !covenant) {
            result.covenant={16,0,0xffffffff,255};mapped=true;
        } else if(settled && covenant_leaf(image,hp.targets[11],leaf,0,read) &&
                  icon(image,leaf,16,result.covenant,read) && result.covenant.id==covenant)mapped=true;
        if(mapped && read(expected_player+0x490,param_after) && param_after==param &&
           read(param+0x1ad,after) && after==covenant &&
           read(hp.chain[7]+0x98,alpha) && hud_replacement::identify(image,slot,expected_player,hp_after,read) && hp==hp_after) {
            result.covenant.opacity=alpha;result.available_regions|=DS2FX_HUD_REGION_COVENANT;
        }
    }
    EffectsIdentity effects{},after{};uint8_t alpha=0,hidden=0;int32_t disabled=0;
    detail.stage=1;
    if(identify_effects(image,slot,expected_player,effects,read) &&
       (detail.stage=2,read(effects.chain[7]+0x98,alpha)) && read(effects.chain[7]+0x44,hidden) && hidden<=1 &&
       (detail.stage=3,read(effects.chain[3]+0x18,disabled)) && disabled>=-32 && disabled<=32) {
        result.effect_opacity=disabled>0 || hidden ? 0u : alpha;
        bool valid=true;
        for(unsigned i=0;i<20 && valid;++i) {
            detail.stage=4;detail.row=i;
            uint8_t row_hidden=0,row_alpha=0;
            valid=read(effects.rows[i]+0x44,row_hidden) && row_hidden<=1 && read(effects.rows[i]+0x98,row_alpha);
            if(valid && !row_hidden) {
                detail.stage=5;
                auto& item=result.effects[result.effect_count];
                valid=icon(image,effects.leaves[i],15,item,read);
                if(valid){item.opacity=row_alpha;++result.effect_count;}
            }
        }
        if(valid && (detail.stage=6,identify_effects(image,slot,expected_player,after,read)) && effects==after) {
            result.available_regions|=DS2FX_HUD_REGION_EFFECTS;
            detail.stage=0;
        }
        else {result.effect_count=0;result.effect_opacity=0;}
    }
    detail.hidden=hidden;detail.disabled=disabled;
    if(diagnostic)*diagnostic=detail;
    return result;
}
}
