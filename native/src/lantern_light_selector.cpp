#include "lantern_light_selector.h"
#include "lantern_item_toggle.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <array>

extern "C" { void* DS2FX_LanternLightSelectorOriginal=nullptr; }
namespace fx::lantern_light_selector {
namespace {
uintptr_t slot=0,image=0;
std::atomic<bool> installed{false},enabled{false};
std::atomic<DWORD> thread{0};
std::atomic<uint64_t> authorized{0},calls{0},selected{0},rejected{0};
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* target) {
    fx::require(!installed.load() && base_slot && target,"Invalid Lantern selector setup");
    slot=base_slot;image=executable;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Selector hook backend failed");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_LanternLightSelectorDetour),&DS2FX_LanternLightSelectorOriginal)==MH_OK,"Selector trampoline failed");
    if(MH_EnableHook(target)!=MH_OK) {MH_RemoveHook(target);fx::require(false,"Selector enable failed");}
    installed.store(true);enabled.store(true);return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==executable+0x16148f0,"Unsupported Lantern selector executable");
    // All 208 bytes of the slot update, not just a permissive byte pattern.
    std::array<unsigned char,208> body{};
    fx::require(fx::read(executable+0x142480,body),"Cannot read selector body");
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char digest[32]{};
    fx::require(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Selector SHA provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{alg,hash};
    fx::require(BCryptCreateHash(alg,&hash,nullptr,0,nullptr,0,0)==0 && BCryptHashData(hash,body.data(),static_cast<ULONG>(body.size()),0)==0 && BCryptFinishHash(hash,digest,32,0)==0,"Selector SHA failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);
    fx::require(out.str()=="6df27d3641603189843cfc3c106cbde955d3a911000bd5860bdb39de86637140","ChrLightCtrl slot3 code changed");
    return install_impl(base_slot,executable,reinterpret_cast<void*>(executable+0x14252a));
}
void authorize_engine_tick(uintptr_t manager) noexcept {
    uintptr_t actual=0;if(!installed.load()||!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();thread.compare_exchange_strong(expected,tid);
    if(thread.load()!=tid){enabled.store(false);return;}
    authorized.store(GetTickCount64());
}
void disable() noexcept {enabled.store(false);}
Status status() noexcept {return {calls.load(),selected.load(),rejected.load(),thread.load(),installed.load(),enabled.load()};}
int32_t select(uintptr_t control,uintptr_t flags,int32_t native_row) noexcept {
    ++calls;const auto now=GetTickCount64(),when=authorized.load();
    uintptr_t manager=0,player=0,owner=0,table=0,actual_flags=0;int32_t hp=0,max_hp=0;
    if(!enabled.load()||!when||now<when||now-when>250||thread.load()!=GetCurrentThreadId()||
       !control||(control&15)||!fx::read(slot,manager)||!manager||!fx::read(manager+0xd0,player)||!player||
       !fx::read(control,table)||table!=image+0x10bfb78||!fx::read(control+8,owner)||owner!=player||
       !fx::read(player+0xb8,actual_flags)||actual_flags!=flags||!fx::read(player+0x168,hp)||
       !fx::read(player+0x170,max_hp)||hp<=0||hp>max_hp||max_hp>1000000) {++rejected;return native_row;}
    // While visible, Lantern51 supplies the canonical character lighting.
    // This selects the native controller row; player/status/Estus remain untouched.
    if(!fx::lantern_item_toggle::active_for(player))return native_row;
    ++selected;return 51;
}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t executable,void* target){return install_impl(base_slot,executable,target);}
#endif
}
extern "C" int32_t DS2FX_SelectLanternLight(uintptr_t control,uintptr_t flags,int32_t native_row) noexcept {return fx::lantern_light_selector::select(control,flags,native_row);}
