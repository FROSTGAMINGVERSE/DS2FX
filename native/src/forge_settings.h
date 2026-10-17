#pragma once
#include "common.h"
namespace fx::forge {
inline fs::path settings_file() {
    wchar_t local[32768]{}; auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    require(n && n<32768,"Forge settings location unavailable");
    auto file=fs::path(local)/"DS2FX"/"Forge"/"settings.ini"; no_links(file); return file;
}
inline int setting(const fs::path& file,const wchar_t* section,const wchar_t* key,int fallback,int low,int high) {
    wchar_t value[64]{}; GetPrivateProfileStringW(section,key,L"",value,64,file.c_str());
    if(!value[0]) return fallback;
    wchar_t* end=nullptr; auto v=wcstol(value,&end,10);
    return end && !*end && v>=low && v<=high ? static_cast<int>(v) : fallback;
}
inline bool save_setting(const wchar_t* section,const wchar_t* key,int value) {
    try {
        const auto file=settings_file(); no_links(file); fs::create_directories(file.parent_path());
        const auto str=std::to_wstring(value);
        return WritePrivateProfileStringW(section,key,str.c_str(),file.c_str())!=FALSE;
    } catch(...) { return false; }
}
inline bool jump_enabled() { return setting(settings_file(),L"modern-souls-2",L"DedicatedJump",1,0,1)!=0; }
inline bool estus_enabled() { return setting(settings_file(),L"modern-souls-2",L"WalkingEstus",1,0,1)!=0; }
}
