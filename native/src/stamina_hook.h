#pragma once
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cmath>

extern "C" void DS2FX_StaminaDetour();
extern "C" void* DS2FX_StaminaOriginal;
namespace fx::stamina_hook {
inline std::atomic<unsigned> percent{100};
inline std::atomic<bool> accepting{false},claimed{false},fault{false};
inline std::atomic<unsigned long long> calls{0},local_calls{0},eligible{0},modified{0},rejected{0};
inline std::atomic<DWORD> first_thread{0};
inline std::atomic<bool> multiple_threads{false};
inline uintptr_t base_slot=0;
inline uint32_t register_policy(uint32_t value) {
    if (!accepting || value < 10 || value > 100) return 0;
    bool expected=false;
    if (!claimed.compare_exchange_strong(expected,true)) return 0;
    percent=value; return 1;
}
inline float scaled(float cost,unsigned value) { return cost*(static_cast<float>(value)/100.0f); }
inline std::string code_hash(uintptr_t address,size_t length) {
    std::vector<unsigned char> bytes(length); SIZE_T read=0;
    require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),length,&read) && read==length,"Cannot verify stamina routine");
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Hash provider failed");
    struct Cleanup { BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE& h; ~Cleanup(){ if(h) BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a,0); } } cleanup{algorithm,hash};
    require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Hash init failed");
    unsigned char result[32];
    require(BCryptHashData(hash,bytes.data(),static_cast<ULONG>(length),0)==0 && BCryptFinishHash(hash,result,32,0)==0,"Hash failed");
    std::ostringstream out; for(auto byte:result) out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(byte); return out.str();
}
inline void install(uintptr_t slot) {
    base_slot=slot;
    uintptr_t address=0;
#ifdef DS2FX_FIXTURE
    address=reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_TestStaminaCost"));
    require(address!=0,"Owned fixture cost function missing");
#else
    address=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+0x3335d0;
    require(code_hash(address,0x1a5)=="63db97571ce916add0b67f5e2d22da60a048d414102b242c1cc124811ed9c94a","Stamina function bytes changed; refused hook");
#endif
    // Called before the launcher resumes the owned game's entrypoint.
    const auto initialized=MH_Initialize();
    require(initialized==MH_OK || initialized==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize owned hook backend");
    require(MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&DS2FX_StaminaDetour),&DS2FX_StaminaOriginal)==MH_OK,"Cannot prepare stamina hook");
    require(MH_EnableHook(reinterpret_cast<void*>(address))==MH_OK,"Cannot enable stamina hook");
}
}
