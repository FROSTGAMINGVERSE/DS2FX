#include "../src/hud_observation.h"
#include "../src/hud_presentation.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include <limits>

namespace {
void check(bool valid,const char* reason) { if(!valid) throw std::runtime_error(reason); }
struct Memory {
    std::map<uintptr_t,std::vector<uint8_t>> cells;
    template<class T> void put(uintptr_t address,T value) {
        auto& bytes=cells[address];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));
    }
    template<class T> bool read(uintptr_t address,T& value) const {
        const auto it=cells.find(address);
        if(it==cells.end() || it->second.size()!=sizeof(T)) return false;
        std::memcpy(&value,it->second.data(),sizeof(T));return true;
    }
};
}
int main() { try {
    constexpr uintptr_t slot=0x1000,base=0x2000,player=0x3000,param=0x4000,equip=0x5000,scene_root=0x6000,scene_leaf=0x7000;
    Memory memory;
    memory.put(slot,base);memory.put(base+0xd0,player);
    memory.put(player+0x490,param);memory.put(param+0xec,int32_t(3200));memory.put(param+0x1ad,uint8_t(2));
    memory.put(player+0x378,equip);memory.put(equip+0x5a4,int32_t(1));memory.put(equip+0x5a8,int32_t(2));
    memory.put(base+0x24ac,int32_t(30));memory.put(base+0x80,scene_root);
    memory.put(scene_root+0x8,scene_leaf);memory.put(scene_leaf+0xbb4,uint8_t(0));
    memory.put(equip+0x9c+0x28,int32_t(123400));memory.put(equip+0xb0+0x50,int32_t(567800));
    DS2FX_PlayerState state{sizeof(DS2FX_PlayerState),1,42,800,1000};state.character_epoch=9;
    auto reader=[&](uintptr_t address,auto& value){return memory.read(address,value);};
    memory.put(param+0x1ac,uint8_t(0));
    auto form=fx::hud_observation::sample_form(slot,player,state,reader);
    check(form.available && form.human && form.hollow_level==0 && form.character_epoch==9,"Human observation failed");
    for(uint8_t level:{uint8_t(1),uint8_t(15),uint8_t(32)}) {
        memory.put(param+0x1ac,level);form=fx::hud_observation::sample_form(slot,player,state,reader);
        check(form.available && !form.human && form.hollow_level==level,"Hollow stages failed");
    }
    memory.put(param+0x1ac,uint8_t(33));
    check(!fx::hud_observation::sample_form(slot,player,state,reader).available,"Invalid hollow byte accepted");
    memory.put(param+0x1ac,uint8_t(0));int form_reads=0;
    auto changing_form=[&](uintptr_t address,auto& value){
        if(address==param+0x1ac && ++form_reads==2) {value=static_cast<std::remove_reference_t<decltype(value)>>(1);return true;}
        return memory.read(address,value);
    };
    check(!fx::hud_observation::sample_form(slot,player,state,changing_form).available,"Changing form published");
    check(!fx::hud_observation::sample_form(slot,player+8,state,reader).available,"Foreign form owner accepted");
    auto result=fx::hud_observation::sample(slot,player,state,reader);
    check(result.size==sizeof(result) && result.player_available && result.sample==42 && result.character_epoch==9,"Snapshot identity invalid");
    check(result.available_fields==(DS2FX_HUD_SOULS|DS2FX_HUD_COVENANT|DS2FX_HUD_LEFT_HAND|DS2FX_HUD_RIGHT_HAND|DS2FX_HUD_SCENE),"Only proved fields must be available");
    check(result.game_state==30 && result.loading==0,"Playable scene observation wrong");
    check(result.souls==3200 && result.covenant_id==2 && result.left_hand_item_id==123400 && result.right_hand_item_id==567800,"Field values wrong");
    memory.put(param+0xec,int32_t(-10));memory.put(equip+0x5a4,int32_t(4));
    result=fx::hud_observation::sample(slot,player,state,reader);
    check(!(result.available_fields&DS2FX_HUD_SOULS) && !(result.available_fields&DS2FX_HUD_LEFT_HAND),"Invalid values leaked");
    check((result.available_fields&DS2FX_HUD_COVENANT) && (result.available_fields&DS2FX_HUD_RIGHT_HAND),"Optional failures hid independent fields");
    memory.put(base+0x24ac,int32_t(31));memory.put(scene_leaf+0xbb4,uint8_t(1));
    result=fx::hud_observation::sample(slot,player,state,reader);
    check((result.available_fields&DS2FX_HUD_SCENE) && result.game_state==31 && result.loading==1,"Loading transition must be exposed");
    memory.put(scene_leaf+0xbb4,uint8_t(2));
    result=fx::hud_observation::sample(slot,player,state,reader);
    check(!(result.available_fields&DS2FX_HUD_SCENE) && (result.available_fields&DS2FX_HUD_COVENANT),"Invalid loading flag must not hide independent data");
    memory.put(base+0x24ac,int32_t(30));memory.put(scene_leaf+0xbb4,uint8_t(0));
    memory.put(param+0xec,int32_t(3200));memory.put(equip+0x5a4,int32_t(1));
    int slot_reads=0;
    auto changed=[&](uintptr_t address,auto& value) {
        if(address==slot && ++slot_reads==2) { value=static_cast<std::remove_reference_t<decltype(value)>>(base+0x100);return true; }
        return memory.read(address,value);
    };
    result=fx::hud_observation::sample(slot,player,state,changed);
    check(!result.player_available && result.available_fields==0,"Changed owner chain must invalidate all HUD fields");
    slot_reads=0;check(!fx::hud_observation::sample_form(slot,player,state,changed).available,"Form survived reallocation");
    state.available=0;
    result=fx::hud_observation::sample(slot,player,state,reader);
    check(!result.player_available && result.available_fields==0,"Unavailable player must clear HUD");
    // Owned memory fixture for the separately sized native presentation query.
    state.available=1;constexpr uintptr_t image=0x140000000;
    constexpr uintptr_t offsets[]{0x22e0,0x10,0x10,8,0x28,0x30,0x38,0x38};
    constexpr uintptr_t tables[]{0,0x10fa628,0x10fab70,0x11d6420,0x11d6408,0x11de158,0x11de318,0x11ddfa8};
    uintptr_t targets[8]{};
    for(unsigned i=0;i<8;++i) {
        targets[i]=0x10000+uintptr_t(i)*0x1000;
        memory.put((i ? targets[i-1] : base)+offsets[i],targets[i]);
        if(tables[i]) memory.put(targets[i],image+tables[i]);
    }
    memory.put(targets[3]+0x18,int32_t(0));
    for(uint8_t alpha:{uint8_t(0),uint8_t(37),uint8_t(127),uint8_t(255)}) {
        memory.put(targets[7]+0x98,alpha);
        auto presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
        check(presentation.available && presentation.opacity==alpha && presentation.sample==42 &&
              presentation.character_epoch==9,"Native alpha and owner identity must be preserved");
    }
    memory.put(targets[3]+0x18,int32_t(1));
    auto presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
    check(presentation.available && !presentation.opacity,"Native disabled layout must hide");
    memory.put(targets[3]+0x18,int32_t(0));
    for(unsigned i=1;i<8;++i) {
        memory.put(targets[i],image+tables[i]+8);
        presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
        check(!presentation.available,"Wrong UI class must reject presentation");
        memory.put(targets[i],image+tables[i]);
    }
    slot_reads=0;
    presentation=fx::hud_presentation::sample(image,slot,player,state,changed);
    check(!presentation.available,"Reallocated UI owner must reject presentation");
    // Installed param bounds, with different raw units for SP: the native
    // gauge mapping, rather than the source art aspect, determines the spans.
    constexpr uintptr_t manager=0x30000,physical=0x31000,phys_data=0x32000,gauge=0x33000,gauge_data=0x34000;
    memory.put(base+0x18,manager);memory.put(manager+0x5a0,physical);
    memory.put(physical+0xd8,phys_data);memory.put(phys_data+0xa,uint16_t(2));
    memory.put(phys_data+0x40,uint32_t(1));memory.put(phys_data+0x48,uint64_t(0x100));
    memory.put(phys_data+0x58,uint32_t(99));memory.put(phys_data+0x60,uint64_t(0x200));
    memory.put(phys_data+0x104,int16_t(500));memory.put(phys_data+0x204,int16_t(2500));
    memory.put(phys_data+0x106,int16_t(750));memory.put(phys_data+0x206,int16_t(1500));
    memory.put(phys_data+0x18a,uint8_t(0));memory.put(phys_data+0x28a,uint8_t(0));
    memory.put(targets[0]+0x280,gauge);memory.put(gauge+0xd8,gauge_data);
    memory.put(gauge_data+0xa,uint16_t(1));memory.put(gauge_data+0x40,uint32_t(1));memory.put(gauge_data+0x48,uint64_t(0x100));
    memory.put(gauge_data+0x124,.05f);memory.put(gauge_data+0x12c,.1f);
    memory.put(gauge_data+0x128,.04f);memory.put(gauge_data+0x130,.08f);
    state.max_health=1500;state.max_stamina=1125;
    presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
    check(presentation.available && presentation.gauge_available && std::abs(presentation.health_length-.075f)<.00001f &&
          std::abs(presentation.stamina_length-.06f)<.00001f,"Installed native param lengths not mapped");
    state.max_health=3000;state.max_stamina=1800;
    presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
    check(std::abs(presentation.health_length-.1f)<.00001f && std::abs(presentation.stamina_length-.08f)<.00001f,"Above-bound buffs must remain within native full span");
    memory.put(gauge_data+0x12c,std::numeric_limits<float>::quiet_NaN());
    presentation=fx::hud_presentation::sample(image,slot,player,state,reader);
    check(presentation.available && !presentation.gauge_available,"Invalid optional gauge must not revoke healthy presentation");
    memory.put(gauge_data+0x12c,.1f);int gauge_reads=0;
    auto changing_gauge=[&](uintptr_t address,auto& value){
        if(address==targets[0]+0x280 && ++gauge_reads==2){value=static_cast<std::remove_reference_t<decltype(value)>>(gauge+8);return true;}
        return memory.read(address,value);
    };
    presentation=fx::hud_presentation::sample(image,slot,player,state,changing_gauge);
    check(presentation.available && !presentation.gauge_available,"Changed gauge owner leaked stat lengths");
    memory.cells.erase(targets[7]+0x98);
    check(!fx::hud_presentation::sample(image,slot,player,state,reader).available,"Unreadable alpha must hide");
    check(!fx::hud_presentation::sample(image,slot,player+8,state,reader).available,"Foreign player must reject presentation");
    std::cout<<"PASS: read-only HUD fields, independent validity and owner-chain invalidation\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
