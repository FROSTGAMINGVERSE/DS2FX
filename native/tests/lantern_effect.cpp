#include "../src/lantern_effect.h"
#include "../src/common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#pragma comment(linker,"/export:DS2FX_EffectFixtureTarget")
extern "C" {
__declspec(dllexport) void DS2FX_EffectFixtureTarget();
uint64_t DS2FX_EffectInvoke(void* target, uintptr_t action, const float* unused);
alignas(16) unsigned char DS2FX_EffectObserved[352]{};
}
namespace {
alignas(16) std::array<unsigned char,0x200> manager{},player{},owner{},action{},params{};
alignas(16) std::array<unsigned char,0x400> flags{};
uintptr_t slot=0;
template<class T,class A> void put(A& area,size_t off,T v) {std::memcpy(area.data()+off,&v,sizeof(v));}
template<class T,class A> T get(const A& area,size_t off) {T v{};std::memcpy(&v,area.data()+off,sizeof(v));return v;}
void initialize() {
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    slot=reinterpret_cast<uintptr_t>(manager.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(player,0xb8,reinterpret_cast<uintptr_t>(flags.data()));
    put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(owner,0,image+0x10af2b8);put(owner,8,reinterpret_cast<uintptr_t>(player.data()));put(owner,0x18,uintptr_t{0});
    put(action,0,image+0x10ccbd8);put(action,0x48,reinterpret_cast<uintptr_t>(owner.data()));
    // A real EMEVD argument blob may be 4-byte aligned without being 8-byte aligned.
    put(action,0x40,reinterpret_cast<uintptr_t>(params.data()+4));
    put(params,4,int32_t{19});put(params,8,float{300});put(params,12,int32_t{51});
    put(action,0x10,int32_t{60425000});put(action,0x14,int32_t{1000});put(action,0x18,int32_t{9});
    put(action,0x20,float{250});put(action,0x24,float{50});put(action,0x30,int8_t{0});put(flags,0x368,int32_t{51});
}
}
int main() {
    try {
        namespace p=fx::lantern_effect;
        initialize();const auto base_slot=reinterpret_cast<uintptr_t>(&slot);
        const auto address=reinterpret_cast<uintptr_t>(action.data());
        const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        p::Sample sample{};
        fx::require(!p::install(base_slot,false) && !p::status().installed,"Default must not install");
        const auto pristine=action;
        fx::require(p::read_native_sample(base_slot,address,image,sample) && sample.available && sample.active &&
                    sample.remaining_seconds==250 && sample.capacity_seconds==300 && sample.owner_token==reinterpret_cast<uintptr_t>(player.data()),"Canonical owned countdown copied");
        fx::require(action==pristine,"Reader never writes action data");
        put(action,0,image+0x10ccb08);
        fx::require(!p::read_native_sample(base_slot,address,image,sample),"Byte-status action is not the integer light instruction");initialize();
        put(flags,0x318,int8_t{101});
        fx::require(p::read_native_sample(base_slot,address,image,sample) && sample.light_value==51,"Value32 light reads flags368 independently of byte status318");initialize();
        put(owner,8,reinterpret_cast<uintptr_t>(manager.data()));
        fx::require(!p::read_native_sample(base_slot,address,image,sample),"Nonlocal owner rejected");initialize();
        put(owner,0x18,reinterpret_cast<uintptr_t>(manager.data()));
        fx::require(!p::read_native_sample(base_slot,address,image,sample),"Foreign status override rejected");initialize();
        put(params,12,int32_t{50});fx::require(!p::read_native_sample(base_slot,address,image,sample),"Original Cast Light row excluded");initialize();
        put(params,4,int32_t{18});fx::require(!p::read_native_sample(base_slot,address,image,sample),"Wrong status excluded");initialize();
        put(action,0x30,int8_t{2});fx::require(!p::read_native_sample(base_slot,address,image,sample),"Infinite mode excluded");initialize();
        put(action,0x20,std::numeric_limits<float>::quiet_NaN());fx::require(!p::read_native_sample(base_slot,address,image,sample),"NaN excluded");initialize();
        put(action,0x40,uintptr_t{1});fx::require(!p::read_native_sample(base_slot,address,image,sample),"Malformed parameters excluded");initialize();
        put(player,0x168,int32_t{0});fx::require(!p::read_native_sample(base_slot,address,image,sample),"Dead character excluded");initialize();
        put(action,0x20,float{-0.01f});fx::require(p::read_native_sample(base_slot,address,image,sample) && !sample.active && sample.remaining_seconds==0,"Native expiry clamps to inactive zero");initialize();
        auto target=reinterpret_cast<void*>(&DS2FX_EffectFixtureTarget);
        const float unused=0;
        const auto baseline_return=DS2FX_EffectInvoke(target,address,&unused);
        std::array<unsigned char,352> observed{};std::memcpy(observed.data(),DS2FX_EffectObserved,observed.size());
        const auto baseline_action=action;initialize();
        fx::require(p::install(base_slot,true),"Explicit fixture hook installation");
        const auto hooked_return=DS2FX_EffectInvoke(target,address,&unused);
        fx::require(baseline_return==hooked_return && hooked_return==0xbadc0ffee0ddf00dULL,"Original return preserved");
        fx::require(std::memcmp(observed.data(),DS2FX_EffectObserved,observed.size())==0,"Actual short-branch prologue preserves original GPR/flags/stack/16XMM/x87/MXCSR behavior");
        fx::require(action==baseline_action,"Detour adds no action writes beyond the original owned function");
        fx::require(p::snapshot(sample) && sample.remaining_seconds==250 && p::status().samples==1,"Native pre-update lifetime published");
        put(flags,0x368,int32_t{101});
        fx::require(p::snapshot(sample) && sample.remaining_seconds==250 && !sample.active && sample.light_value==101,"Overlapping light status does not claim a lit owned row");
        put(flags,0x368,int32_t{51});
        auto native=reinterpret_cast<void(*)(void*,float)>(target);initialize();native(action.data(),0.125f);
        fx::require(get<float>(action,0x20)==249.875f && get<float>(action,0x24)==50.125f,"Original dt countdown executes exactly once");
        fx::require(p::snapshot(sample) && sample.remaining_seconds==250,"HUD copies native value without elapsed simulation");
        put(action,0x30,int8_t{2});const auto count=p::status().samples;native(action.data(),1.0f);
        fx::require(p::status().samples==count && get<float>(action,0x20)==249.875f,"Original nondecrement branch preserved");initialize();
        put(action,0x20,float{-0.01f});native(action.data(),0.125f);
        fx::require(p::snapshot(sample) && !sample.active && sample.remaining_seconds==0,"Observed expiry clears active");initialize();
        native(action.data(),0.125f);put(manager,0xd0,reinterpret_cast<uintptr_t>(owner.data()));
        fx::require(!p::snapshot(sample),"Character change invalidates cached sample");initialize();
        std::thread second([&]{native(action.data(),0.125f);});second.join();
        fx::require(p::status().thread_fault && !p::status().observing && !p::snapshot(sample),"Second owner thread disables observation");
        const float before=get<float>(action,0x20);native(action.data(),0.125f);
        fx::require(get<float>(action,0x20)==before-0.125f,"Fault leaves native update functional");p::disable();
        fx::require(MH_DisableHook(target)==MH_OK && MH_RemoveHook(target)==MH_OK,"Fixture cleanup");
        std::cout<<"PASS owned native effect lifetime: exact short-branch MinHook fixture, canonical owner, instruction identity, malformed state, native expiry, all register/FP/flags preservation, no added action writes, thread fault pass-through\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
