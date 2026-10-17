#include "../src/lantern_light_selector.h"
#include "../src/lantern_item_toggle.h"
#include "../src/lantern_bonfire_observer.h"
#include "../src/common.h"
#include "../vendor/minhook/include/MinHook.h"
#include "../mods/wayfarer_lantern/lantern_state.h"
#include <array>
#include <cstring>
#include <thread>
#include <iostream>
extern "C" int32_t DS2FX_SelectorFixtureInvoke(uintptr_t,uintptr_t,int32_t);
extern "C" void DS2FX_SelectorFixturePatch();
extern "C" void DS2FX_BonfireChargeFixture(uintptr_t,uintptr_t);
namespace {
alignas(16) std::array<unsigned char,0xc00> manager{};
alignas(16) std::array<unsigned char,0x500> player{},flags{},control{},effect_control{};
alignas(16) std::array<unsigned char,0x100> action{},params{},owner{};
uintptr_t slot=0;
template<class A,class T>void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
template<class T,class A>T get(A& a,size_t offset){T value{};std::memcpy(&value,a.data()+offset,sizeof(value));return value;}
}
int main(){try{
    namespace toggle=fx::lantern_item_toggle;namespace selector=fx::lantern_light_selector;
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto player_ptr=reinterpret_cast<uintptr_t>(player.data()),control_ptr=reinterpret_cast<uintptr_t>(control.data()),flags_ptr=reinterpret_cast<uintptr_t>(flags.data());
    slot=reinterpret_cast<uintptr_t>(manager.data());const auto base=reinterpret_cast<uintptr_t>(&slot);
    put(manager,0xd0,player_ptr);put(player,0xb8,flags_ptr);put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(player,0x3e0,reinterpret_cast<uintptr_t>(effect_control.data()));put(effect_control,0,image+0x10bfee0);
    put(control,0,image+0x10bfb78);put(control,8,player_ptr);put(control,0x1c,int32_t{-1});
    put(owner,0,image+0x10af2b8);put(owner,8,player_ptr);
    put(action,0,image+0x10ccbd8);put(action,0x48,reinterpret_cast<uintptr_t>(owner.data()));put(action,0x40,reinterpret_cast<uintptr_t>(params.data()));
    put(params,0,int32_t{19});put(params,4,float{0});put(params,8,int32_t{51});
    put(action,0x10,int32_t{60425001});put(action,0x14,int32_t{1000});put(action,0x18,int32_t{9});
    toggle::prepare(base,true,false);
    fx::require(selector::install_owned_fixture(base,image,reinterpret_cast<void*>(&DS2FX_SelectorFixturePatch)),"Inline selector hook installs");
    fx::require(fx::lantern_bonfire::install_owned_fixture(base,image,reinterpret_cast<void*>(&DS2FX_BonfireChargeFixture),true),"Bonfire hook installs");
    auto authorize=[&]{toggle::on_engine_tick(slot);selector::authorize_engine_tick(slot);};
    auto use=[&]{authorize();toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);};
    auto row=[&](int32_t native=-1){return DS2FX_SelectorFixtureInvoke(control_ptr,flags_ptr,native);};
    fx::require(row()==-1,"Unowned Lantern cannot create illumination");
    toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
    fx::require(!toggle::active_for(player_ptr),"Unauthorised receipt rejected before game tick");
    use();fx::require(toggle::active_for(player_ptr)&&row()==51,"First exact use selects Lantern lighting without charges");
    const auto pristine_flags=flags,pristine_player=player;
    const auto transitions=get<uint32_t>(control,0x2d8);
    for(int n=0;n<120;n++){authorize();fx::require(row()==51,"Repeated frames retain row51");}
    fx::require(get<uint32_t>(control,0x2d8)==transitions,"Stable row does not restart interpolation");
    fx::require(flags==pristine_flags&&player==pristine_player,"Selector leaves player/status/Estus byte-identical");
    fx::require(row(50)==51,"Visible Lantern supplies character lighting even with native status row");
    use();fx::require(row()==-1&&!toggle::active_for(player_ptr),"Use again stows");
    put(action,0x10,int32_t{60425000});use();fx::require(!toggle::active_for(player_ptr),"Oil cannot drive Lantern");put(action,0x10,int32_t{60425001});
    for(int n=0;n<20;++n){use();fx::require(row()==51,"Unlimited light activation");use();fx::require(row()==-1,"Unlimited stow");}
    use();
    int32_t foreign_row=0;std::thread worker([&]{foreign_row=row();});worker.join();fx::require(foreign_row==-1,"Foreign thread cannot select light");
    put(player,0x168,int32_t{0});authorize();fx::require(!toggle::active_for(player_ptr)&&row()==-1,"Death stows model/light");
    put(player,0x168,int32_t{100});authorize();fx::require(!toggle::active_for(player_ptr),"Revive remains stowed");
    selector::disable();fx::require(row()==-1,"Disabled selector keeps native behavior");
    MH_DisableHook(MH_ALL_HOOKS);MH_Uninitialize();
    std::cout<<"PASS native inline selector + unlimited toggle: no Oil, stable row, stow, death, thread gating, player/Estus unchanged\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
