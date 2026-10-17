#include "../src/common.h"
#include "../src/lantern_flame.h"
#include "../src/lantern_item_toggle.h"
#include <array>
#include <cstring>
#include <thread>
#include <iostream>
namespace {
alignas(16) std::array<unsigned char,0xc00> manager{};
alignas(16) std::array<unsigned char,0x500> player{},control{},flags{},effects{},panel{};
alignas(16) std::array<unsigned char,0x100> action{},params{},owner{},node{},ref{},data{},repository{};
uintptr_t slot=0;uint32_t create_calls=0,remove_calls=0;bool dummy_exists=true,trackable=true;
int32_t native_count=-1,observed_property=0;uint32_t observed_flags=0,original_calls=0;
template<class A,class T>void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
template<class T,class A>T get(A& a,size_t offset){T value{};std::memcpy(&value,a.data()+offset,sizeof(value));return value;}
int32_t __fastcall create(uintptr_t c,uint16_t kind,int32_t effect,uint16_t dummy,bool one_shot,float duration,int32_t unknown) {
    fx::require(c==reinterpret_cast<uintptr_t>(control.data())&&kind==0xc087&&effect==6042&&dummy==247&&!one_shot&&duration==0&&unknown==0,"Exact native seven-argument SFX request");
    ++create_calls;put(node,0,uint32_t{(53u<<20)|0x14087});put(node,4,int32_t{6042});put(node,0x70,int32_t{42});
    put(control,0x20,trackable?reinterpret_cast<uintptr_t>(node.data()):uintptr_t{0});return 42;
}
void __fastcall remove(uintptr_t c,int32_t id){fx::require(c==reinterpret_cast<uintptr_t>(control.data())&&id==42,"Only owned handle released");++remove_calls;put(control,0x20,uintptr_t{0});}
bool __fastcall resolve(uintptr_t p,int32_t dummy,void* output,bool animated) {
    fx::require(p==reinterpret_cast<uintptr_t>(player.data())&&dummy==247&&animated,"Animated wick dummy, no pelvis fallback");
    std::memset(output,0,64);return dummy_exists;
}
}
int main(){try{
    namespace flame=fx::lantern_flame;namespace toggle=fx::lantern_item_toggle;
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),p=reinterpret_cast<uintptr_t>(player.data());
    slot=reinterpret_cast<uintptr_t>(manager.data());const auto base=reinterpret_cast<uintptr_t>(&slot);
    put(manager,0xbc8,uintptr_t{1});
    put(manager,0xd0,p);put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});put(player,0xb8,reinterpret_cast<uintptr_t>(flags.data()));
    put(player,0x248,reinterpret_cast<uintptr_t>(control.data()));put(control,0,image+0x10e08d8);put(control,0x58,p);
    put(player,0x3e0,reinterpret_cast<uintptr_t>(effects.data()));put(effects,0,image+0x10bfee0);
    put(owner,0,image+0x10af2b8);put(owner,8,p);put(action,0,image+0x10ccbd8);put(action,0x48,reinterpret_cast<uintptr_t>(owner.data()));put(action,0x40,reinterpret_cast<uintptr_t>(params.data()));
    put(params,0,int32_t{19});put(params,4,float{0});put(params,8,int32_t{51});put(action,0x10,int32_t{60425001});put(action,0x14,int32_t{1000});put(action,0x18,int32_t{9});
    toggle::prepare(base,true,false);flame::prepare_owned_fixture(base,image,create,remove,resolve);
    auto tick=[&]{toggle::on_engine_tick(slot);flame::native_tick(slot);};
    auto use=[&]{tick();toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);tick();};
    tick();const auto pristine_player=player,pristine_flags=flags;
    dummy_exists=false;use();fx::require(create_calls==0,"Missing wick cannot fall back to player center");
    dummy_exists=true;tick();fx::require(create_calls==1&&flame::status().handle==42,"External dummy247 resolves to owned internal node53");
    flame::native_tick(0);for(int n=0;n<300;++n)tick();fx::require(create_calls==1,"Frames and unavailable manager cannot duplicate SFX");
    use();fx::require(remove_calls==1&&flame::status().handle==-1,"Stow releases exactly returned native SFX");
    for(int n=0;n<20;++n){use();use();}fx::require(create_calls==21&&remove_calls==21,"Unlimited activations never deplete");
    fx::custom_item_resource::Snapshot ledger{};fx::require(!toggle::charge_snapshot(ledger),"Simple item creates no charge pool");
    use();put(node,4,int32_t{230});tick();for(int n=0;n<300;++n)tick();fx::require(create_calls==22&&flame::status().handle==-1,"Recycled foreign effect is not removed or recreated every frame");
    use();fx::require(remove_calls==21,"Stowing cannot delete a foreign handle");
    trackable=false;use();for(int n=0;n<300;++n)tick();fx::require(create_calls==23,"Untrackable native result cannot cause allocation spam");use();trackable=true;
    use();std::thread worker([&]{flame::native_tick(slot);});worker.join();fx::require(create_calls==24,"Worker cannot execute a native flame action");
    put(player,0x168,int32_t{0});tick();fx::require(!toggle::active_for(p)&&remove_calls==22,"Death stows and releases owned flame");
    put(player,0x168,int32_t{100});tick();fx::require(create_calls==24&&!toggle::active_for(p),"Revive remains stowed");
    use();const auto before_travel=create_calls,before_remove=remove_calls;
    // Same character/controller, but native loading discarded its SFX list.
    put(control,0x20,uintptr_t{0});put(manager,0xbc8,uintptr_t{0});tick();
    fx::require(create_calls==before_travel,"Unloaded scene cannot allocate SFX");
    put(manager,0xbc8,uintptr_t{2});tick();
    fx::require(create_calls==before_travel+1&&flame::status().handle==42,"Scene change restores missing flame once without another toggle");
    for(int n=0;n<300;++n)tick();fx::require(create_calls==before_travel+1,"Scene recovery does not allocate repeatedly");
    use();fx::require(remove_calls==before_remove+1,"Recovered flame remains exactly owned and removable");
    fx::require(player==pristine_player&&flags==pristine_flags,"Player, inventory, status and Estus are never modified");
    std::cout<<"PASS unlimited authenticated toggle, resolved wick index, one flame per activation, exact handle removal, lost/foreign handle and death/thread guards\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
