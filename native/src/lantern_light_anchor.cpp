#include "lantern_light_anchor.h"
#include "common.h"
#include "lantern_item_toggle.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>

extern "C" { void* DS2FX_LanternLightAnchorOriginal=nullptr; }
namespace fx::lantern_light_anchor {
namespace {
using Resolve=bool(__fastcall*)(uintptr_t,int32_t,Matrix*,bool);
uintptr_t slot=0,image=0;
Resolve resolve=nullptr;
std::atomic<bool> installed{false},enabled{false},fault{false};
std::atomic<DWORD> thread{0};
std::atomic<uint64_t> authorized_at{0},calls{0},eligible{0},relocated{0},missing{0},invalid{0},rejected_thread{0};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
std::array<unsigned char,375> resolver_body{};
bool verify_resolver=false;
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out) noexcept {
    return base && offset<=UINTPTR_MAX-base && fx::read(base+offset,out) &&
        out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
std::string hash_bytes(uintptr_t address,size_t length) {
    std::vector<unsigned char> bytes(length);SIZE_T copied=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),bytes.size(),&copied) && copied==bytes.size(),"Cannot read light-anchor native code");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Light-anchor SHA256 provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Light-anchor SHA256 init failed");
    unsigned char digest[32];
    fx::require(BCryptHashData(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)==0 && BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Light-anchor SHA256 failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
bool valid_matrix(const Matrix& m) noexcept {
    for(auto& row:m.rows)for(float f:row)if(!std::isfinite(f)||std::abs(f)>10000000.0f)return false;
    for(unsigned r=0;r<3;++r) {
        float length=0;for(unsigned c=0;c<3;++c)length+=m.rows[r][c]*m.rows[r][c];
        if(length<0.01f||length>100.0f||std::abs(m.rows[r][3])>0.01f)return false;
    }
    return std::abs(m.rows[3][3]-1.0f)<0.01f;
}
bool owned_controller(uintptr_t control,uintptr_t& player) noexcept {
    uintptr_t manager=0,owner=0,vtable=0,flags=0,selected=0;int32_t hp=0,max=0,value=0,index=-1,id=-1;
    return control && !(control&15) && pointer(slot,0,manager) && pointer(manager,0xd0,player) &&
        pointer(control,8,owner) && owner==player && fx::read(control,vtable) && vtable==image+0x10bfb78 &&
        fx::read(player+0x168,hp) && fx::read(player+0x170,max) && hp>0 && hp<=max && max<=1000000 &&
        pointer(player,0xb8,flags) && (fx::lantern_item_toggle::is_enabled()?fx::lantern_item_toggle::active_for(player):(fx::read(flags+0x368,value) && value==51)) &&
        fx::read(control+0x1c,id) && id==51 && fx::read(control+0x1f0,index) && index==3 &&
        fx::read(control+0x2b0,selected) && selected==control+0xf0;
}
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* target,Resolve resolver,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!installed.load() && base_slot && executable && target && resolver,"Invalid/repeated lantern light-anchor setup");
    slot=base_slot;image=executable;resolve=resolver;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize light-anchor hook backend");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_LanternLightAnchorDetour),&DS2FX_LanternLightAnchorOriginal)==MH_OK,"Cannot prepare lantern light-anchor hook");
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);DS2FX_LanternLightAnchorOriginal=nullptr;fx::require(false,"Cannot enable lantern light-anchor hook");}
    installed.store(true);enabled.store(true);return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;fx::require(false,"Lantern light anchoring is offline-only");return false;
#else
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==executable+0x16148f0,"Unsupported executable/BaseA for lantern light anchor");
    fx::require(hash_bytes(executable+0x1432d0,849)=="caf5def302d93e2f20e4c3fe363ff5a27eef68f2753baf5346cf26acdaebaebf","ChrLightCtrl transform body mismatch");
    fx::require(hash_bytes(executable+0x312750,375)=="57235a92e66c8c1a7687b0f2f7aae12f45ff598f664dec8d8807fe3696b03ee9" && fx::read(executable+0x312750,resolver_body),"Native dummy resolver body mismatch");
    verify_resolver=true;
    return install_impl(base_slot,executable,reinterpret_cast<void*>(executable+0x1432d0),reinterpret_cast<Resolve>(executable+0x312750),true);
#endif
}
void authorize_engine_tick(uintptr_t manager) noexcept {
    if(!enabled.load())return;
    uintptr_t actual=0;if(!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();thread.compare_exchange_strong(expected,tid);
    if(thread.load()!=tid){fault.store(true);enabled.store(false);return;}
    authorized_at.store(GetTickCount64());
}
void disable() noexcept {enabled.store(false);}
Status status() noexcept {return {calls.load(),eligible.load(),relocated.load(),missing.load(),invalid.load(),rejected_thread.load(),thread.load(),installed.load(),enabled.load(),fault.load()};}
void after_original(uintptr_t control) noexcept {
    ++calls;if(!enabled.load()||fault.load())return;
    const auto when=authorized_at.load();const auto now=GetTickCount64();
    if(!when||now<when||now-when>250||thread.load()!=GetCurrentThreadId()){++rejected_thread;return;}
    if(busy.test_and_set(std::memory_order_acquire)){fault.store(true);enabled.store(false);return;}
    struct Unlock {~Unlock(){busy.clear(std::memory_order_release);}} unlock;
    uintptr_t player=0;if(!owned_controller(control,player))return;
    ++eligible;
    std::array<unsigned char,375> bytes{};
    if(verify_resolver && (!fx::read(reinterpret_cast<uintptr_t>(resolve),bytes)||bytes!=resolver_body)){fault.store(true);enabled.store(false);return;}
    Matrix matrix{};
    if(!resolve(player,247,&matrix,true)){++missing;return;}
    if(!valid_matrix(matrix)){++invalid;return;}
    uintptr_t after=0;if(!enabled.load()||!owned_controller(control,after)||after!=player)return;
    // Engine-owned slot3 only, synchronously inside its own native transform
    // update. Native light interpolation and renderer consumption run afterward.
    SIZE_T copied=0;
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(control+0xf0),&matrix,sizeof(matrix),&copied)||copied!=sizeof(matrix)) {
        fault.store(true);enabled.store(false);return;
    }
    ++relocated;
}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t executable,void* target,ResolveDummy resolver,bool opt_in){return install_impl(base_slot,executable,target,resolver,opt_in);}
void after_original_for_fixture(uintptr_t control) noexcept {after_original(control);}
#endif
}
extern "C" void DS2FX_AfterLanternLightAnchor(uintptr_t controller) noexcept {fx::lantern_light_anchor::after_original(controller);}
