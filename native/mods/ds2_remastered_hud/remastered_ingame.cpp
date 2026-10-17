#include "remastered_ingame.h"
#include "../../src/common.h"
#include "remastered_addon_identity.h"
#include <atomic>
#include <mutex>
#include <cstring>
#include <intrin.h>
#include <cstdio>
#include "../../vendor/minhook/include/MinHook.h"

namespace remastered::ingame {
namespace {
HMODULE self=nullptr;
const DS2FX_Api* api=nullptr;
std::atomic<bool> attached{false};
uint64_t core_graphics_handle=0;
std::atomic<HMODULE> adapter_module{nullptr};
std::atomic<std::shared_ptr<const Frame>> pending;
std::atomic<uint64_t> mailbox_generation{0};
// All GPU objects live exclusively on ReShade's serialized render callback.
Compositor compositor;
bool logged_draw=false;
uint64_t submitted_at=0;
uint32_t submitted_regions=0;
uint64_t submitted_generation=0,submitted_epoch=0;
void report(const char* reason,HWND game,const Frame* frame=nullptr,const D3D11_TEXTURE2D_DESC* desc=nullptr) {
    static const char* previous=nullptr;static uint64_t at=0;
    const auto now=GetTickCount64();
    if(previous && std::strcmp(previous,reason)==0 && now-at<5000)return;
    previous=reason;at=now;
    char message[360]{};
    std::snprintf(message,sizeof(message),"HUD GPU path: %s; hwnd=%p frame_hwnd=%p blank=%u age_ms=%llu format=%u samples=%u size=%ux%u state=%s",
        reason,game,frame?frame->game:nullptr,frame?frame->blank:0,frame?now-frame->created:0,
        desc?desc->Format:0,desc?desc->SampleDesc.Count:0,desc?desc->Width:0,desc?desc->Height:0,
        compositor.isolated_state()?"D3D11.1":"D3D11 bindings");
    api->log("ds2-remastered-hud",message);
}
using Update=void(*)(void*);
using UpdateDepth=void(*)(void*,void*);
Update original_update=nullptr;
UpdateDepth original_depth=nullptr;
void* update_target=nullptr;void* depth_target=nullptr;
bool hooked=false;
bool bootstrap_attempted=false;
std::atomic<bool> stopping{false};
std::atomic<bool> adapter_attempted{false};
std::filesystem::path adapter_path;
void load_adapter_on_render_thread() {
    if(stopping.load() || adapter_module.load() || adapter_attempted.exchange(true))return;
    // The pinned manual runtime does not run ReShade's proxy add-on loader.
    // Bootstrap at the entry of its public update API, before event dispatch,
    // on the same rendering thread already serialized by the existing bridge.
    const auto adapter=LoadLibraryW(adapter_path.c_str());
    if(adapter)adapter_module.store(adapter);
    else api->log("ds2-remastered-hud","HUD adapter failed to initialize on render thread; native HUD retained; restart after correcting installation");
}
void update(void* runtime){load_adapter_on_render_thread();original_update(runtime);}
void update_depth(void* runtime,void* depth){load_adapter_on_render_thread();original_depth(runtime,depth);}
void revoke(){submitted_at=0;submitted_regions=0;if(api)api->update_hud_region_mask(0,0,0);}
void present(HWND game,ID3D11DeviceContext* context,ID3D11Texture2D* texture,BOOL sdr) {
    if(!api || !attached.load())return;
    try {
        const auto generation=mailbox_generation.load();
        const auto frame=pending.load();
        if(!frame || game!=frame->game){report("no matching window frame",game,frame.get());return;}
        DS2FX_PlayerState player{sizeof(player)};api->get_player_state(&player);
        if(!player.available || !fresh(*frame,GetTickCount64(),player.character_epoch)){report("stale frame or player epoch",game,frame.get());revoke();return;}
        if(!sdr){report("non-SDR color space",game,frame.get());revoke();return;}
        if(!context || !texture || !compositor.prepare(context,*frame)){report(compositor.failure(),game,frame.get());revoke();return;}
        D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
        if(desc.SampleDesc.Count!=1 || (desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format!=DXGI_FORMAT_R10G10B10A2_UNORM)) {report("unsupported backbuffer descriptor",game,frame.get(),&desc);revoke();return;}
        Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
        // The RTV is scoped to this callback. ResizeBuffers never sees an owned
        // reference to a game back buffer after the callback returns.
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> target;
        D3D11_RENDER_TARGET_VIEW_DESC view{};view.Format=desc.Format;view.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D;
        if(FAILED(device->CreateRenderTargetView(texture,&view,&target))){report("backbuffer RTV creation failed",game,frame.get(),&desc);revoke();return;}
        uint32_t regions=frame->regions;
        DS2FX_HudIcons current{sizeof(current)};api->get_hud_icons(&current);
        if(current.character_epoch!=frame->epoch ||
           std::memcmp(&current.covenant,&frame->icons.covenant,sizeof(current.covenant)))regions&=~DS2FX_HUD_REGION_COVENANT;
        if(current.character_epoch!=frame->epoch || current.effect_count!=frame->icons.effect_count ||
           current.effect_opacity!=frame->icons.effect_opacity ||
           std::memcmp(current.effects,frame->icons.effects,sizeof(current.effects)))regions&=~DS2FX_HUD_REGION_EFFECTS;
        // Acquire after resources and target have validated, on the rendering
        // thread immediately before submission. Separate layers mean rejected
        // optional regions never enter the back buffer. Any submission failure
        // revokes all leases; a stopped callback expires naturally in 350 ms.
        const auto granted=api->update_hud_region_mask(frame->epoch,frame->sample,regions);
        if(!(granted&DS2FX_HUD_REGION_BARS)){report("native bar grant rejected",game,frame.get(),&desc);revoke();return;}
        // Present runs after the native UI. A new grant applies to the NEXT
        // game frame: drawing replacement pixels now would double the bars.
        // Prime on acquisition/recovery, then draw only continuously leased
        // regions. Keep the existing 200 ms artwork / 350 ms native expiry.
        const auto now=GetTickCount64();
        const auto drawable=submitted_at && submitted_generation==generation && submitted_epoch==frame->epoch &&
            now>=submitted_at && now-submitted_at<=350 ? granted&submitted_regions : 0u;
        submitted_at=now;submitted_regions=granted;submitted_generation=generation;submitted_epoch=frame->epoch;
        if(!(drawable&DS2FX_HUD_REGION_BARS)){report("priming native suppression for next frame",game,frame.get(),&desc);return;}
        if(!compositor.draw(context,target.Get(),*frame,drawable)){report(compositor.failure(),game,frame.get(),&desc);revoke();return;}
        if(!frame->blank && !logged_draw)report("first submission",game,frame.get(),&desc);
        if(!frame->blank && !logged_draw){logged_draw=true;api->log("ds2-remastered-hud","In-game D3D11 HUD submitted before ReShade effects/overlay; no visible desktop overlay");}
    } catch(...) {revoke();}
}
void destroy_device() {compositor.reset();revoke();logged_draw=false;}
void core_present(const DS2FX_GraphicsFrame* frame,void*) {
    if(!frame || frame->size!=sizeof(*frame) || frame->version!=1)return;
    // Core owns the presentation adapter; HUD owns its compositor and leases.
    const auto artwork=pending.load();
    if(artwork && artwork->game==frame->window)
        present(frame->window,frame->context,frame->back_buffer,frame->sdr);
}
}
void configure(HMODULE module,const DS2FX_Api* host){
    self=module;api=host;
    if(!core_graphics_handle && api->size>=offsetof(DS2FX_Api,unregister_graphics_callback)+sizeof(api->unregister_graphics_callback) && api->register_graphics_callback) {
        const DS2FX_GraphicsCallback callback{sizeof(callback),1,core_present,nullptr};
        core_graphics_handle=api->register_graphics_callback(&callback);
    }
}
bool attach() {
    if(attached.load())return true;
    if(core_graphics_handle){attached.store(true);api->log("ds2-remastered-hud","Using public DS2FX Core graphics bridge; no graphics mods required");return true;}
    const auto expected=fx::module_path().parent_path()/L"reshade-addons"/L"DS2FX-Remastered-HUD.addon64";
    const auto adapter=GetModuleHandleW(expected.c_str());
    if(!adapter) {
        if(hooked || bootstrap_attempted || stopping.load())return false;
        const auto runtime=GetModuleHandleW(L"ReShade68.dll");if(!runtime)return false;
        bootstrap_attempted=true;
        try {
            if(fx::hash_file(fx::module_path(runtime))!=manual_runtime_hash){api->log("ds2-remastered-hud","HUD bootstrap refused: manual ReShade runtime hash changed");return false;}
            if(fx::hash_file(expected)!=addon_hash){api->log("ds2-remastered-hud","HUD bootstrap refused: adapter hash changed");return false;}
            adapter_path=expected;
        }catch(...){api->log("ds2-remastered-hud","HUD bootstrap waiting: adapter/runtime file unavailable");return false;}
        update_target=GetProcAddress(runtime,"ReShadeUpdateAndPresentEffectRuntime");
        depth_target=GetProcAddress(runtime,"ReShadeUpdateAndPresentEffectRuntimeWithDepth");
        if(!update_target || MH_Initialize()!=MH_OK){api->log("ds2-remastered-hud","HUD bootstrap refused: exports or hook initialization unavailable");return false;}
        if(MH_CreateHook(update_target,update,reinterpret_cast<void**>(&original_update))!=MH_OK){api->log("ds2-remastered-hud","HUD bootstrap refused: update API hook unsupported");return false;}
        if(depth_target && MH_CreateHook(depth_target,update_depth,reinterpret_cast<void**>(&original_depth))!=MH_OK){MH_RemoveHook(update_target);api->log("ds2-remastered-hud","HUD bootstrap refused: depth update API hook unsupported");return false;}
        if(MH_QueueEnableHook(update_target)!=MH_OK || (depth_target && MH_QueueEnableHook(depth_target)!=MH_OK) || MH_ApplyQueued()!=MH_OK) {
            MH_DisableHook(update_target);MH_RemoveHook(update_target);if(depth_target){MH_DisableHook(depth_target);MH_RemoveHook(depth_target);}return false;
        }
        hooked=true;api->log("ds2-remastered-hud","Pinned manual ReShade API bootstrap armed; adapter loads on its serialized render thread before events");
        return false;
    }
    try {if(fx::module_path(adapter)!=expected || fx::hash_file(expected)!=addon_hash)return false;}catch(...){return false;}
    adapter_module.store(adapter);attached.store(true);api->log("ds2-remastered-hud","Authenticated ReShade game-frame adapter; native HUD retained until first healthy frame");
    return true;
}
void detach() {
    stopping.store(true);attached.store(false);clear();
    if(core_graphics_handle){api->unregister_graphics_callback(core_graphics_handle);core_graphics_handle=0;}
    if(hooked){MH_DisableHook(update_target);if(depth_target)MH_DisableHook(depth_target);}
    // Keep trampolines and adapter alive through process exit; an in-flight
    // callback may still be returning through them after hooks are disabled.
}
// Only the separately authenticated ReShade adapter can reach the GPU entry.
// Both entry points remain inside the pinned DS2FX HUD module.
extern "C" __declspec(dllexport) void DS2FX_RenderRemasteredHud(HWND game,ID3D11DeviceContext* context,ID3D11Texture2D* texture,BOOL sdr) {
    HMODULE caller=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(_ReturnAddress()),&caller) || caller!=remastered::ingame::adapter_module.load())return;
    remastered::ingame::present(game,context,texture,sdr);
}
extern "C" __declspec(dllexport) void DS2FX_ResetRemasteredHud(){
    HMODULE caller=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(_ReturnAddress()),&caller) || caller!=remastered::ingame::adapter_module.load())return;
    remastered::ingame::destroy_device();
}
void publish(std::shared_ptr<const Frame> frame){pending.store(std::move(frame));}
void clear(){++mailbox_generation;pending.store(nullptr);if(api)api->update_hud_region_mask(0,0,0);}
}
