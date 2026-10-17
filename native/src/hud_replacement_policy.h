#pragma once
#include <array>
#include <cstdint>

namespace fx::hud_replacement {
struct Identity {
    uintptr_t base=0,player=0;
    std::array<uintptr_t,8> chain{};
    std::array<uintptr_t,13> targets{};
    bool operator==(const Identity&) const = default;
};
// Only the reviewed HP layout's direct bar/frame rows. Status effects have
// their own FeSceneSpecialEffectIcon (+38 in FeOperatorFrontend).
inline constexpr uint32_t ids[13]{0xf6950,0xf6951,0xf6958,0xf6959,
    0xf6952,0xf6953,0xf6954,0xf6955,0xf6957,0xf6956,0xffffffff,0xf6963,0xf6964};
template<class Read> bool identify(uintptr_t image,uintptr_t slot,uintptr_t player,Identity& out,Read read,unsigned* failure=nullptr) {
    auto fail=[&](unsigned reason){if(failure)*failure=reason;return false;};if(failure)*failure=0;
    Identity value{};
    if(!player || !read(slot,value.base) || !value.base || !read(value.base+0xd0,value.player) || value.player!=player)return fail(1);
    constexpr uintptr_t offsets[]{0x22e0,0x10,0x10,8,0x28,0x30,0x38,0x38};
    constexpr uintptr_t tables[]{0x10c0768,0x10fa628,0x10fab70,0x11d6420,0x11d6408,0x11de158,0x11de318,0x11ddfa8};
    for(unsigned i=0;i<8;++i) {
        uintptr_t table=0;
        if(!read((i?value.chain[i-1]:value.base)+offsets[i],value.chain[i]) || !value.chain[i] ||
           !read(value.chain[i],table) || table!=image+tables[i])return fail(2);
    }
    const auto scene=value.chain[2];uintptr_t header=0,self=0;uint64_t count=0;
    if(!read(scene+0x2e0,header) || !header || !read(header,self) || self!=header ||
       !read(header+8,count) || count!=13)return fail(3);
    for(unsigned i=0;i<13;++i) {
        uint32_t id=0;uintptr_t table=0,parent=0,resource=0;uint32_t resource_id=0;
        if(!read(header+0x10+i*16,value.targets[i]) || !value.targets[i] ||
           !read(header+0x1c+i*16,id) || id!=ids[i] ||
           !read(value.targets[i],table) || table!=image+(i==10?0x11dea18:0x11ddfa8))return fail(4);
        uintptr_t sprite=0;
        if(!read(value.chain[7]+0x38,sprite) || !sprite || !read(value.targets[i]+0x18,parent) || parent!=sprite)return fail(5);
        if(i!=10 && (!read(value.targets[i]+0x48,resource) || !resource ||
           !read(resource+0x1c,resource_id) || resource_id!=ids[i]))return fail(6);
    }
    uintptr_t hp=0,stamina=0;
    if(!read(scene+0xd0,hp) || hp!=value.targets[11] || !read(scene+0xd8,stamina) || stamina!=value.targets[12])return fail(7);
    out=value;return true;
}
inline bool lease_valid(uint64_t now,uint64_t last_frame,uint64_t epoch,uint64_t current_epoch) {
    return last_frame && now>=last_frame && now-last_frame<=350 && epoch && epoch==current_epoch;
}
}
