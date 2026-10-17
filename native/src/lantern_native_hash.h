#pragma once
#include "common.h"
namespace fx {
inline std::string lantern_native_hash(uintptr_t address,size_t size) {
    std::vector<unsigned char> bytes(size);SIZE_T copied=0;
    require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),size,&copied)&&copied==size,"Cannot read Lantern native entry");
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    require(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Native entry SHA256 failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{a,h};
    require(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0,"Native entry hash init failed");
    unsigned char digest[32]{};
    require(BCryptHashData(h,bytes.data(),static_cast<ULONG>(size),0)==0&&BCryptFinishHash(h,digest,32,0)==0,"Native entry hash failed");
    std::ostringstream result;for(auto c:digest)result<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(c);return result.str();
}
}
