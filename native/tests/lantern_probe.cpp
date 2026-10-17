#include "../src/lantern_probe.h"
#include "../src/common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#pragma comment(linker,"/export:DS2FX_ProbeFixtureTarget")

extern "C" {
__declspec(dllexport) void DS2FX_ProbeFixtureTarget();
uint64_t DS2FX_ProbeInvoke(void* target,uintptr_t manager,const float* update);
alignas(16) unsigned char DS2FX_ProbeObserved[352]{};
}
namespace {
alignas(16) std::array<unsigned char,0x200> manager{},player{},data{},inventory{},bag{},holder{},value{};
alignas(16) std::array<unsigned char,0x1900> profile{};
uintptr_t slot=0;
template<class T,class A> void put(A& area,size_t offset,T v) {std::memcpy(area.data()+offset,&v,sizeof(v));}
uint64_t qword(size_t off) {uint64_t v;std::memcpy(&v,DS2FX_ProbeObserved+off,8);return v;}
void initialize() {
    slot=reinterpret_cast<uintptr_t>(manager.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(manager,0xa8,reinterpret_cast<uintptr_t>(data.data()));
    put(data,0x10,reinterpret_cast<uintptr_t>(inventory.data()));
    put(data,0xc0,reinterpret_cast<uintptr_t>(profile.data()));
    put(inventory,0x10,reinterpret_cast<uintptr_t>(bag.data()));
    put(bag,0x28,reinterpret_cast<uintptr_t>(holder.data()));
    put(holder,0x10,reinterpret_cast<uintptr_t>(value.data()));
    put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(profile,0x18a8,int32_t{3});put(value,0,float{312.5f});
}
}
int main() {
    try {
        namespace p=fx::lantern_probe;
        initialize(); const auto base_slot=reinterpret_cast<uintptr_t>(&slot); const float dt=1.0f/60;
        p::Sample sample{};
        fx::require(!p::install(base_slot,false) && !p::status().installed,"Default must not install");
        fx::require(p::read_native_sample(base_slot,slot,reinterpret_cast<uintptr_t>(&dt),sample),"Owned local player sample");
        fx::require(sample.character_available && sample.native_torch_available && sample.native_torch_time_raw==312.5f && sample.save_slot_available && sample.save_slot_index==3 && sample.update_value_raw==dt,"Native fields copied without reinterpreting torch as oil");
        auto pristine=value;
        put(value,0,std::numeric_limits<float>::quiet_NaN());
        p::read_native_sample(base_slot,slot,1,sample);
        fx::require(!sample.native_torch_available && !sample.update_value_available,"NaN and unreadable update rejected");
        value=pristine;
        put(profile,0x18a8,int32_t{10});p::read_native_sample(base_slot,slot,0,sample);
        fx::require(!sample.save_slot_available && sample.native_torch_available,"Optional slot failure isolated");
        put(profile,0x18a8,int32_t{3});
        put(holder,0x10,uintptr_t{1});p::read_native_sample(base_slot,slot,0,sample);
        fx::require(sample.character_available && !sample.native_torch_available,"Malformed optional pointer rejected");
        put(holder,0x10,reinterpret_cast<uintptr_t>(value.data()));
        put(player,0x168,int32_t{0});fx::require(!p::read_native_sample(base_slot,slot,0,sample) && !sample.character_available,"Dead player unavailable");
        put(player,0x168,int32_t{100});
        fx::require(!p::read_native_sample(base_slot,slot+16,0,sample),"Wrong manager rejected");
        fx::require(p::install(base_slot,true),"Explicit fixture installation");
        auto target=reinterpret_cast<void*>(&DS2FX_ProbeFixtureTarget);
        const auto returned=DS2FX_ProbeInvoke(target,slot,&dt);
        fx::require(returned==0xbadc0ffee0ddf00dULL,"Original return preserved");
        fx::require(qword(0)==0xcafeface12345678ULL && qword(8)==slot && qword(16)==reinterpret_cast<uintptr_t>(&dt),"RAX/RCX/RDX preserved");
        fx::require(qword(24)==0x8877665544332211ULL && qword(32)==0x9988776655443322ULL && qword(40)==0x1020304050607080ULL && qword(48)==0xa0b0c0d0e0f01020ULL,"R8/R9/R10/R11 preserved");
        fx::require((qword(56)&0xcd5)==(0x247&0xcd5),"Arithmetic/direction flags preserved");
        fx::require(qword(64)==0x55aa55aa55aa55aaULL && qword(72)==0xaa55aa55aa55aa55ULL,"Stack arguments preserved");
        for(size_t n=0;n<16;n++) fx::require(qword(80+n*16)==0x123456789abcdef0ULL && qword(88+n*16)==0xfedcba9876543210ULL,"All XMM registers preserved");
        uint32_t mxcsr=0;uint16_t x87=0;std::memcpy(&mxcsr,DS2FX_ProbeObserved+336,4);std::memcpy(&x87,DS2FX_ProbeObserved+340,2);
        fx::require(mxcsr==0x5f80 && x87==0x77f,"Nondefault MXCSR/x87 control preserved");
        fx::require(p::snapshot(sample) && sample.native_torch_time_raw==312.5f && p::status().samples==1,"Real detour published read-only data");
        fx::require(value==pristine,"Native timer bytes never written");
        const auto count=p::status().manager_calls;
        DS2FX_ProbeInvoke(target,slot+16,&dt);
        fx::require(p::status().manager_calls==count && !p::status().thread_fault,"Non-manager call remains pass-through");
        std::thread second([&]{DS2FX_ProbeInvoke(target,slot,&dt);});second.join();
        fx::require(p::status().thread_fault && !p::status().observing && !p::snapshot(sample),"Second manager thread permanently disables observation");
        fx::require(DS2FX_ProbeInvoke(target,slot,&dt)==0xbadc0ffee0ddf00dULL,"Fault remains pass-through");
        p::disable();
        fx::require(MH_DisableHook(target)==MH_OK && MH_RemoveHook(target)==MH_OK,"Fixture cleanup");
        std::cout<<"PASS owned real MinHook+MASM probe: opt-in, read-only native fields, invalid pointers/NaN/death, GPR/flags/stack/16 XMM/MXCSR/x87/return preservation, thread-fault pass-through\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
