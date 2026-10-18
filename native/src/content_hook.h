#pragma once
#include "content_pack.h"
#include "stamina_hook.h"
#include <mutex>
#include <array>
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
inline fs::path installed_root;
inline fs::path mount_root;
inline std::atomic<uint64_t> mounted_requests{0},mounted_resolved{0};
// Bounded, opt-in diagnostic capture. Never log or wait on the resolver thread.
struct RequestTrace {
    wchar_t input[256]{},output[256]{};
    uintptr_t result=0;std::atomic<unsigned> phase{0};unsigned reported_phase=0;
};
inline std::array<RequestTrace,256> request_trace;
inline std::atomic<unsigned> request_count{0};
inline std::atomic<bool> capture_requests{false};
inline bool capture_candidate(const GameString* path) noexcept {
    try {
        if(!path || path->capacity<8 || !path->data || !path->length || path->length>=2048)return false;
        auto key=asset_key(std::wstring(path->data,static_cast<size_t>(path->length)));
        if(key.find(L"enc_regulation")!=key.npos || key.ends_with(L".param"))return true;
        if(key.starts_with(L"gamedata:/"))key.erase(0,10);else if(key.starts_with(L"title:/"))key.erase(0,7);
        return routes.contains(key);
    }catch(...){return false;}
}
inline void copy_trace(wchar_t (&to)[256],const GameString* path) {
    if(!path || !path->length || path->length>=2048)return;
    const auto from=path->capacity>=8?path->data:reinterpret_cast<const wchar_t*>(path);
    if(!from)return;
    const auto count=std::min<size_t>(path->length,255);
    std::copy_n(from,count,to);to[count]=0;
}
inline void report_requests(void (*log)(const char*,const char*)) {
    if(!capture_requests.load(std::memory_order_relaxed))return;
    unsigned emitted=0;
    for(auto& event:request_trace) {
        const auto phase=event.phase.load(std::memory_order_acquire);
        if(!phase || phase==event.reported_phase)continue;
        std::ostringstream message;
        auto ascii=[&](const wchar_t* value){for(size_t n=0;value[n]&&n<255;++n)message<<char(value[n]>=32&&value[n]<127?value[n]:'?');};
        message<<"REQUEST input=";ascii(event.input);message<<" output=";ascii(event.output);
        if(phase==2)message<<" mount="<<(event.result?"found":"missing");else message<<" native-call=pending";
        log("content-diagnostic",message.str().c_str());event.reported_phase=phase;
        if(++emitted==16)break;
    }
}
inline std::wstring mounted_target(const fs::path& target) {
    const auto absolute=target.lexically_normal().generic_wstring();
    const auto prefix=mount_root.generic_wstring()+L'/';
    require(target.is_absolute() && asset_key(absolute).starts_with(asset_key(prefix)),"Content target is outside the game mount");
    const fs::path relative=absolute.substr(prefix.size());
    require(!relative.empty() && !relative.is_absolute(),"Invalid game-relative content target");
    for(const auto& part:relative) require(part!=L".." && part!=L".","Content target escapes the game mount");
    auto result=L"./"+relative.generic_wstring();
    require(result.size()<2048,"Mounted content path too long");
    return result;
}
inline std::set<std::wstring> reported; inline std::mutex report_mutex;
inline void (*report)(const char*,const char*)=nullptr;
inline std::atomic<bool> enabled{false};
// Opt-in diagnostics: never wait for disk logging during ordinary asset loading.
inline std::atomic<bool> trace_redirects{false};
inline std::vector<forge::Mod> active_mods;
inline uintptr_t route(uintptr_t context,GameString* path) {
    RequestTrace* trace=nullptr;
    if(capture_requests.load(std::memory_order_relaxed) && capture_candidate(path)) {
        const auto index=request_count.fetch_add(1,std::memory_order_relaxed);
        if(index<request_trace.size()){trace=&request_trace[index];copy_trace(trace->input,path);}
    }
    bool redirected=false;
    if(enabled && path && path->capacity>=8 && path->data && path->length>0 && path->length<2048) {
        try {
            auto key=asset_key(std::wstring(path->data,static_cast<size_t>(path->length)));
            if(key.starts_with(L"gamedata:/")) key.erase(0,10);
            else if(key.starts_with(L"title:/")) key.erase(0,7);
            else key.clear();
            const auto found=routes.find(key);
            if(found!=routes.end()) {
                // The native function looks up a mounted path prefix. A Windows
                // drive such as F: is not the game's relative filesystem mount.
                const auto target=mounted_target(found->second.target);
                if(path->capacity<target.size()+1) grow(path,target.size()+1,static_cast<size_t>(path->length));
                if(path->data && path->capacity>=target.size()+1) {
                    std::copy(target.begin(),target.end(),path->data);
                    path->data[target.size()]=0; path->length=target.size();
                    redirected=true;
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
    if(trace){copy_trace(trace->output,path);trace->phase.store(1,std::memory_order_release);}
    const auto result=original(context,path);
    if(trace){trace->result=result;trace->phase.store(2,std::memory_order_release);}
    if(redirected) {
        mounted_requests.fetch_add(1,std::memory_order_relaxed);
        if(result) mounted_resolved.fetch_add(1,std::memory_order_relaxed);
    }
    return result;
}
inline std::map<std::wstring,Asset> resolved_routes(const fs::path& root) {
    std::map<std::wstring,Asset> result;
    // Same audited integration selection as preflight, never first-pack wins.
    for(const auto& asset:load(root)) require(result.emplace(asset.key,asset).second,"Duplicate effective runtime route");
    return result;
}
inline void install(const fs::path& root,void (*log)(const char*,const char*)) {
    const auto selected=packs(root); const auto effective=resolved_routes(root);
    if(enabled.load()) {
        require(fs::equivalent(installed_root,root) && effective.size()==routes.size(),"Pre-entry content selection changed before runtime initialization");
        for(const auto& [key,asset]:effective) {
            const auto found=routes.find(key);
            require(found!=routes.end() && found->second.target==asset.target && found->second.hash==asset.hash,"Pre-entry content asset changed before runtime initialization");
        }
        log("content","Verified pre-entry content routing retained for runtime initialization");return;
    }
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
    installed_root=root;
#ifdef DS2FX_FIXTURE
    mount_root=fs::absolute(root).lexically_normal();
#else
    mount_root=module_path().parent_path().lexically_normal();
#endif
    for(const auto& [key,asset]:routes) mounted_target(asset.target);
    auto result=MH_Initialize(); require(result==MH_OK || result==MH_ERROR_ALREADY_INITIALIZED,"Content hook backend failed");
    require(MH_CreateHook(address,reinterpret_cast<void*>(&route),reinterpret_cast<void**>(&original))==MH_OK,"Content hook preparation failed");
    enabled=true;
    if(MH_EnableHook(address)!=MH_OK) { enabled=false; MH_RemoveHook(address); throw std::runtime_error("Content hook activation failed"); }
    log("content","Native content targets use the game-relative filesystem mount; no Windows file API detour");
    log("content",("Content routing active; assets="+std::to_string(routes.size())+"; identical peer pack and settings required; no DInput/save/network patches").c_str());
}
}
