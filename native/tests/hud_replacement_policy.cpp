#include "../src/hud_replacement_policy.h"
#include "../src/hud_icons.h"
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Memory {
    std::unordered_map<uintptr_t, uint64_t> cells;
    template<class T> void put(uintptr_t address, T value) {
        uint64_t bytes = 0;
        static_assert(sizeof(T) <= sizeof(bytes));
        std::memcpy(&bytes, &value, sizeof(T));
        cells[address] = bytes;
    }
    template<class T> bool read(uintptr_t address, T& value) const {
        auto found = cells.find(address);
        if (found == cells.end()) return false;
        std::memcpy(&value, &found->second, sizeof(T));
        return true;
    }
};

constexpr uintptr_t image = 0x140000000;
constexpr uintptr_t slot = 0x20000;
constexpr uintptr_t base = 0x30000;
constexpr uintptr_t player = 0x40000;
constexpr uintptr_t scene_address = 0x80000;
constexpr uintptr_t header_address = 0x90000;
constexpr uintptr_t sprite_address = 0xa0000;
constexpr uintptr_t chain_offsets[]{0x22e0,0x10,0x10,8,0x28,0x30,0x38,0x38};
constexpr uintptr_t chain_tables[]{0x10c0768,0x10fa628,0x10fab70,0x11d6420,
    0x11d6408,0x11de158,0x11de318,0x11ddfa8};

Memory canonical() {
    Memory m;
    m.put(slot, base);
    m.put(base+0xd0, player);
    uintptr_t previous = base;
    for (unsigned i=0; i<8; ++i) {
        const uintptr_t current = i==2 ? scene_address : 0x300000+i*0x10000;
        m.put(previous+chain_offsets[i], current);
        m.put(current, image+chain_tables[i]);
        previous = current;
    }
    m.put(previous+0x38, sprite_address);
    m.put(scene_address+0x2e0, header_address);
    m.put(header_address, header_address);
    m.put(header_address+8, uint64_t{13});
    for (unsigned i=0; i<13; ++i) {
        const uintptr_t target=0x100000+i*0x10000;
        const uintptr_t resource=0x200000+i*0x10000;
        m.put(header_address+0x10+i*16,target);
        m.put(header_address+0x1c+i*16,fx::hud_replacement::ids[i]);
        m.put(target,image+(i==10 ? 0x11dea18 : 0x11ddfa8));
        m.put(target+0x18,sprite_address);
        if (i!=10) {
            m.put(target+0x48,resource);
            m.put(resource+0x1c,fx::hud_replacement::ids[i]);
        }
        if (i==11) m.put(scene_address+0xd0,target);
        if (i==12) m.put(scene_address+0xd8,target);
    }
    return m;
}

bool identify(const Memory& m, uintptr_t expected_player=player) {
    fx::hud_replacement::Identity identity{};
    return fx::hud_replacement::identify(image,slot,expected_player,identity,
        [&m](uintptr_t address, auto& value) { return m.read(address,value); });
}
void icon_data(Memory& m,uintptr_t leaf,uintptr_t p,uint32_t type,uint32_t id) {
    m.put(leaf,image+0x11dea18);m.put(leaf+0x68,p);m.put(p,image+0x10af1a0);
    m.put(p+0x18,type);m.put(p+0x1c,id);m.put(p+0x10,p+0x1000);
    m.put(p+0x1000,image+0x1161c28);m.put(p+0x1040,uint32_t{64});m.put(p+0x1044,uint32_t{64});
    m.put(leaf+0x40,p+0x2000);m.put(p+0x2002,uint16_t{1});m.put(leaf+0x7a,uint8_t{0});
    m.put(leaf+0x60,p+0x3000);m.put(p+0x3000,uint32_t{0xffffffff});
}
Memory icons_fixture() {
    auto m=canonical();
    m.put(player+0x490,uintptr_t{0xf00000});m.put(0xf001ad,uint8_t{8});
    m.put(scene_address+0xe0,uint8_t{8});m.put(scene_address+0xe1,uint8_t{8});
    m.put(0x370000+0x98,uint8_t{255});
    const uintptr_t covenant_row=0x100000+11*0x10000,leaf=0xe00000;
    m.put(covenant_row+0x38,leaf);m.put(leaf+0x18,covenant_row);m.put(leaf+0x28,uintptr_t{0});
    icon_data(m,leaf,0xe10000,16,8);
    uintptr_t previous=0x310000;
    constexpr uintptr_t offsets[]{0x38,8,0x28,0x30,0x38,0x38};
    constexpr uintptr_t tables[]{0x10b1278,0x11d6420,0x11d6408,0x11de158,0x11de318,0x11ddfa8};
    for(unsigned i=0;i<6;++i) {
        const uintptr_t target=0x520000+i*0x10000;
        m.put(previous+offsets[i],target);m.put(target,image+tables[i]);previous=target;
    }
    m.put(0x520000+0xb8,uint32_t{20});m.put(0x520000+0x220,uintptr_t{0x600000});
    m.put(0x600000,uintptr_t{0x600000});m.put(0x600008,uint64_t{20});
    m.put(previous+0x38,uintptr_t{0x610000});m.put(0x610000,image+0x11de318);
    m.put(previous+0x98,uint8_t{255});m.put(previous+0x44,uint8_t{0});m.put(0x530000+0x18,int32_t{0});
    for(unsigned i=0;i<20;++i) {
        const uintptr_t row=0x700000+i*0x1000,resource=0x800000+i*0x1000,shape=0x900000+i*0x1000;
        m.put(0x600010+i*16,row);m.put(0x60001c+i*16,uint32_t{0x5f5c3e0+i});
        m.put(row,image+0x11ddfa8);m.put(row+0x18,uintptr_t{0x610000});m.put(row+0x48,resource);
        m.put(resource+0x1c,uint32_t{0x5f5c3e0+i});m.put(row+0x38,shape);m.put(shape,image+0x11dea18);m.put(shape+0x18,row);
        m.put(row+0x44,uint8_t(i>=2));m.put(row+0x98,uint8_t{255});
        if(i<2)icon_data(m,shape,0xa00000+i*0x10000,15,i?10:40);
    }
    return m;
}
DS2FX_HudIcons icons(const Memory& m) {
    DS2FX_PlayerState state{sizeof(state),1,42,1,1};state.character_epoch=7;
    return fx::hud_icons::sample(image,slot,player,state,[&](uintptr_t p,auto& v){return m.read(p,v);});
}
}

int main() {
    auto m=canonical();
    check(identify(m),"canonical health scene rejected");
    check(!identify(m,player+8),"foreign player accepted");
    auto bad=m; bad.put(scene_address,image+0x10b1278);
    check(!identify(bad),"foreign status scene accepted");
    bad=m; bad.put(header_address+8,uint64_t{14});
    check(!identify(bad),"changed row count accepted");
    bad=m; bad.put(header_address+0x1c+11*16,uint32_t{0xf6950});
    check(!identify(bad),"changed covenant candidate row accepted");
    bad=m; bad.put(0x100000+0x18,sprite_address+8);
    check(!identify(bad),"foreign parent accepted");
    bad=m; bad.put(0x200000+0x1c,uint32_t{0xf6964});
    check(!identify(bad),"changed resource accepted");
    bad=m; bad.put(scene_address+0xd0,uintptr_t{0});
    check(!identify(bad),"changed final group accepted");
    using fx::hud_replacement::lease_valid;
    check(lease_valid(1350,1000,2,2),"valid lease rejected");
    check(!lease_valid(1351,1000,2,2),"expired lease accepted");
    check(!lease_valid(999,1000,2,2),"future frame accepted");
    check(!lease_valid(1100,1000,2,3),"stale epoch accepted");
    check(!lease_valid(1100,0,2,2),"revoked lease accepted");
    m=icons_fixture();auto result=icons(m);
    check(result.available_regions==6 && result.covenant.type==16 && result.covenant.id==8 &&
        result.effect_count==2 && result.effects[0].id==40 && result.effects[1].id==10,
        "Original covenant and active effects in native order rejected");
    bad=m;bad.put(0xa00000,uintptr_t{image+0x10af1a8});
    result=icons(bad);check(result.available_regions==2,"Foreign effect provider accepted or covenant lost");
    bad=m;bad.put(0xa00000+0x10,uintptr_t{0});
    result=icons(bad);check(result.available_regions==6 && result.effects[0].id==40,
        "Transient native GPU texture absence discarded authenticated original icon key");
    bad=m;bad.put(0x700000+0x18,uintptr_t{0});
    check(icons(bad).available_regions==2,"Foreign effect parent accepted");
    bad=m;bad.put(0x900000+0x18,uintptr_t{0});
    check(icons(bad).available_regions==2,"Foreign effect leaf accepted");
    bad=m;bad.put(0xe10000+0x1c,uint32_t{9});
    check(icons(bad).available_regions==4,"Different covenant artwork accepted");
    bad=m;bad.put(0xf001ad,uint8_t{0});bad.put(scene_address+0xe0,uint8_t{0});bad.put(scene_address+0xe1,uint8_t{0});
    result=icons(bad);check(result.available_regions==6 && result.covenant.id==0,"Valid empty covenant rejected");
    bad=m;bad.put(0x530000+0x18,int32_t{1});
    check(icons(bad).effect_opacity==0 && icons(bad).available_regions==6,"Menu opacity discarded valid status ownership");
    bad=m;bad.put(0x700000+0x44,uint8_t{1});
    result=icons(bad);check(result.effect_count==1 && result.effects[0].id==10,"Inactive native status copied");
}
