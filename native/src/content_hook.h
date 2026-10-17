#pragma once
#include "content_pack.h"
#include "stamina_hook.h"
#include <mutex>
// Original DS2FX routing implementation. ABI/RVAs researched from
// WildBunnie/ds2-modengine and checked against the pinned Scholar executable.
// No vendor code is included. No save/network patches are made.
namespace fx::content {
struct GameString { wchar_t* data; void* allocator; uint64_t length, capacity; };
static_assert(sizeof(GameString)==32);
using Resolve=uintptr_t(*)(uintptr_t,GameString*);
using Grow=void(*)(GameString*,size_t,size_t);
inline Resolve original=nullptr; inline Grow grow=nullptr; inline void* address=nullptr;
inline std::map<std::wstring,Asset> routes;
inline std::set<std::wstring> reported; inline std::mutex report_mutex;
inline void (*report)(const char*,const char*)=nullptr;
inline std::atomic<bool> enabled{false};
// Opt-in diagnostics: never wait for disk logging during ordinary asset loading.
inline std::atomic<bool> trace_redirects{false};
inline std::vector<forge::Mod> active_mods;
inline uintptr_t route(uintptr_t context,GameString* path) {
    if(enabled && path && path->data && path->length>0 && path->length<2048) {
        try {
            auto key=asset_key(std::wstring(path->data,static_cast<size_t>(path->length)));
            if(key.starts_with(L"gamedata:/")) key.erase(0,10);
            else if(key.starts_with(L"title:/")) key.erase(0,7);
            else return original(context,path);
            const auto found=routes.find(key);
            if(found!=routes.end()) {
                const auto& target=found->second.target;
                if(path->capacity<target.size()+1) grow(path,target.size()+1,static_cast<size_t>(path->length));
                if(path->data && path->capacity>=target.size()+1) {
                    std::copy(target.begin(),target.end(),path->data);
                    path->data[target.size()]=0; path->length=target.size();
                    if(trace_redirects.load(std::memory_order_relaxed)) {
                        std::lock_guard lock(report_mutex);
                        if(reported.insert(key).second && report) report("content",("REDIRECT "+fs::path(key).string()).c_str());
                    }
                }
            }
        } catch(const std::exception&) {
            enabled=false; if(report) report("content","Routing exception; disabled for this session");
        }
    }
    return original(context,path);
}
inline std::map<std::wstring,Asset> resolved_routes(const fs::path& root) {
    std::map<std::wstring,Asset> result;
    // Same audited integration selection as preflight, never first-pack wins.
    for(const auto& asset:load(root)) require(result.emplace(asset.key,asset).second,"Duplicate effective runtime route");
    return result;
}
inline void install(const fs::path& root,void (*log)(const char*,const char*)) {
    const auto selected=packs(root); const auto effective=resolved_routes(root);
    for(const auto& pack:selected) {
        active_mods.push_back(pack.ui);

        log("content",("Pack "+pack.id+" "+pack.version+"; manifest="+pack.manifest_hash).c_str());

    }
    if(effective.empty()) return; report=log;
    for(const auto& [key,asset]:effective) if(key==L"enc_regulation.bnd.dcx") log("content",("Effective regulation SHA256="+asset.hash).c_str());
#ifdef DS2FX_FIXTURE
    address=reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_TestResolve"));
    grow=reinterpret_cast<Grow>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_TestGrow"));
    require(address && grow,"Content fixture exports missing");
#else
    auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    address=reinterpret_cast<void*>(base+0x89c5a0); grow=reinterpret_cast<Grow>(base+0x1be60);
    require(stamina_hook::code_hash(base+0x89c5a0,64)=="0a4d8919b5e72a8548414503a2939f8c4387280437c8ccff32ae959001f4f17f","Content resolver already modified; refusing to overwrite another hook");
    require(stamina_hook::code_hash(base+0x1be60,64)=="9939042a0a0dc4d434f53a1900d19db07920e7918b4e473eca9c7ceb2e21aa0b","Content string allocator bytes changed");
#endif
    routes=effective;
    auto result=MH_Initialize(); require(result==MH_OK || result==MH_ERROR_ALREADY_INITIALIZED,"Content hook backend failed");
    require(MH_CreateHook(address,reinterpret_cast<void*>(&route),reinterpret_cast<void**>(&original))==MH_OK,"Content hook preparation failed");
    enabled=true;
    if(MH_EnableHook(address)!=MH_OK) { enabled=false; MH_RemoveHook(address); throw std::runtime_error("Content hook activation failed"); }
    log("content",("Content routing active; assets="+std::to_string(routes.size())+"; identical peer pack and settings required; no DInput/save/network patches").c_str());
}
}
