#pragma once
#include "common.h"

// This opt-in contract exists only in the dedicated Nucleus launcher build.
// Ordinary DS2FX retains its single-process guard.
namespace fx::nucleus {
inline bool instance_directory(const fs::path& directory,const fs::path& root) {
    const auto d=fs::absolute(directory).lexically_normal();
    const auto r=fs::absolute(root).lexically_normal();
    if(d.parent_path()!=r) return false;
    const auto name=d.filename().wstring();
    return name.size()==9 && name.substr(0,8)==L"Instance" && name[8]>=L'0' && name[8]<=L'3';
}
inline void validate(const fs::path& game,const fs::path& root) {
    require(!root.empty() && instance_directory(game.parent_path(),root),"Nucleus game must belong to Instance0 through Instance3 of this session");
    no_links(root); no_links(game);
    const auto marker=root/".ds2fx-session"; no_links(marker);
    DWORD owner=0; std::ifstream(marker)>>owner;
    require(owner!=0,"Nucleus session marker missing; start through MO2's Nucleus option");
    Handle process(OpenProcess(SYNCHRONIZE,FALSE,owner));
    require(process.value && WaitForSingleObject(process.value,0)==WAIT_TIMEOUT,"Nucleus session owner is no longer running");
    wchar_t profile[32768]; auto n=GetEnvironmentVariableW(L"USERPROFILE",profile,32768);
    require(n>0 && n<32768,"Nucleus player environment missing");
    auto p=fs::path(profile);
    require(p.parent_path().filename()==L"NucleusCoop","Nucleus requires a separate player environment");
    wchar_t local[32768]; n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    require(n>0 && n<32768 && fs::path(local).lexically_normal()==(p/"AppData"/"Local").lexically_normal(),"Nucleus local settings are not isolated");
}
inline void verify_other(const fs::path& other,const fs::path& game,const fs::path& root) {
    require(instance_directory(other.parent_path(),root),"A DS2 process outside this Nucleus session is running");
    require(other.parent_path()!=game.parent_path(),"This Nucleus player instance is already running");
}
}
