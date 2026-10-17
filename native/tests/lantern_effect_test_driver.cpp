#include "../src/lantern_effect_test_driver.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
namespace {
alignas(16) std::array<unsigned char,0x200> manager{},control{},data{};
alignas(16) std::array<unsigned char,0x1900> profile{};
alignas(16) std::array<unsigned char,0x400> player{};
uintptr_t slot=0;unsigned grants=0;bool request_ok=true;
template<class T,class A> void put(A& a,size_t o,T v){std::memcpy(a.data()+o,&v,sizeof(v));}
void initialize(){
    slot=reinterpret_cast<uintptr_t>(manager.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(manager,0xa8,reinterpret_cast<uintptr_t>(data.data()));put(data,0xc0,reinterpret_cast<uintptr_t>(profile.data()));put(profile,0x18a8,int32_t{5});
    put(player,0x3e0,reinterpret_cast<uintptr_t>(control.data()));
    put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(control,0,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+0x10bfee0);
}
void __fastcall owned_apply(uintptr_t ctrl,fx::lantern_effect_test::Request* r){
    ++grants;
    const fx::lantern_effect_test::Request expected{};
    request_ok=request_ok && ctrl==reinterpret_cast<uintptr_t>(control.data()) && r && std::memcmp(&expected,r,sizeof(expected))==0;
}
}
int main(){try{
    using fx::lantern_effect_test::Driver;
    initialize();const auto bs=reinterpret_cast<uintptr_t>(&slot);const DWORD tid=GetCurrentThreadId();float dt=0.25f;
    auto tick=[&](Driver& d,unsigned n){for(unsigned i=0;i<n;++i)d.on_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt),tid);};
    Driver disabled;
    fx::require(!disabled.prepare_owned_fixture(bs,&owned_apply,false),"Opt-in is required");tick(disabled,30);fx::require(grants==0,"Disabled cannot call");
    Driver d;fx::require(d.prepare_owned_fixture(bs,&owned_apply,true),"Owned driver prepared");
    const auto pristine=player;tick(d,19);fx::require(grants==0,"Five game seconds required");tick(d,1);
    fx::require(grants==1 && request_ok && d.status().issued==1,"Exact native request and canonical controller dispatched once");
    tick(d,100);fx::require(grants==1 && player==pristine,"No repeats and no direct player writes");
    put(player,0x168,int32_t{0});tick(d,1);initialize();tick(d,19);fx::require(grants==1,"New epoch waits again");tick(d,1);
    fx::require(grants==2 && d.status().character_epochs==2,"One call per explicitly observed living epoch");d.disable();tick(d,30);fx::require(grants==2,"Disable is immediate");
    Driver malformed;malformed.prepare_owned_fixture(bs,&owned_apply,true);tick(malformed,19);dt=std::numeric_limits<float>::quiet_NaN();tick(malformed,1);dt=0.25f;
    tick(malformed,19);fx::require(grants==2,"Invalid dt resets warmup");tick(malformed,1);fx::require(grants==3,"Valid dt can warm up again");malformed.disable();
    Driver foreign;foreign.prepare_owned_fixture(bs,&owned_apply,true);put(control,0,uintptr_t{1});tick(foreign,50);fx::require(grants==3,"Unverified effect controller rejected");initialize();foreign.disable();
    Driver wrong_slot;wrong_slot.prepare_owned_fixture(bs,&owned_apply,true,5);put(profile,0x18a8,int32_t{4});tick(wrong_slot,50);
    fx::require(grants==3,"Existing other save slot cannot receive visual test effect");initialize();tick(wrong_slot,19);fx::require(grants==3,"Correct slot needs complete warmup");wrong_slot.disable();
    Driver worker;worker.prepare_owned_fixture(bs,&owned_apply,true);tick(worker,19);
    std::thread second([&]{worker.on_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt),tid);});second.join();
    fx::require(worker.status().fault && !worker.status().enabled && grants==3,"Worker thread cannot issue native application");tick(worker,50);fx::require(grants==3,"Thread fault is permanent");
    Driver invalid;invalid.prepare_owned_fixture(bs,&owned_apply,true);tick(invalid,19);dt=10.0f;tick(invalid,1);dt=0.25f;tick(invalid,1);fx::require(grants==3,"Large load-frame delta cannot trigger call");invalid.disable();
    Driver lab_delay;lab_delay.prepare_owned_fixture(bs,&owned_apply,true,5,20.0f);tick(lab_delay,79);
    fx::require(grants==3,"Configured twenty-second lab delay is honored");tick(lab_delay,1);
    fx::require(grants==4,"Configured delay dispatches on its first complete native tick");lab_delay.disable();
    std::cout<<"PASS visual effect test driver: explicit opt-in, canonical living controller, native five-second delay, exact16-byte request, once per epoch, invalid dt/owner, no direct game data writes, thread rejection\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
