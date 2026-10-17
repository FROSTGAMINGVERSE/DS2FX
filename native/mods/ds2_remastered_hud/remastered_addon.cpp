#include <reshade.hpp>
#include <d3d11.h>
#include <filesystem>
#include <atomic>

// ReShade loads this small adapter during its own add-on initialization. This
// avoids registering/unregistering event vectors from DS2FX's observer worker
// while ReShade may be iterating them on the rendering thread.
extern "C" __declspec(dllexport) const char* NAME="DS2FX Remastered HUD";
extern "C" __declspec(dllexport) const char* DESCRIPTION="Game-frame HUD before effects and overlay (0.1.27)";
namespace {
HMODULE self=nullptr;
std::atomic<uint64_t> presented{0},resets{0};
using Draw=void(*)(HWND,ID3D11DeviceContext*,ID3D11Texture2D*,BOOL);
using Reset=void(*)();
HMODULE plugin() {
    wchar_t path[32768]{};const auto n=GetModuleFileNameW(nullptr,path,32768);
    if(!n || n>=32768)return nullptr;
    const auto expected=std::filesystem::path(path).parent_path()/L"DS2FX"/L"Plugins"/L"ds2-remastered-hud"/L"plugin.dll";
    return GetModuleHandleW(expected.c_str());
}
void present(reshade::api::command_queue* queue,reshade::api::swapchain* chain,
    const reshade::api::rect*,const reshade::api::rect*,uint32_t,const reshade::api::rect*) {
    if(!queue || !chain || queue->get_device()->get_api()!=reshade::api::device_api::d3d11)return;
    const auto module=plugin();if(!module)return;
    auto draw=reinterpret_cast<Draw>(GetProcAddress(module,"DS2FX_RenderRemasteredHud"));if(!draw)return;
    draw(static_cast<HWND>(chain->get_hwnd()),
        reinterpret_cast<ID3D11DeviceContext*>(queue->get_immediate_command_list()->get_native()),
        reinterpret_cast<ID3D11Texture2D*>(chain->get_current_back_buffer().handle),
        chain->get_color_space()==reshade::api::color_space::srgb);
    ++presented; // Retain an adapter return address for HUD caller authentication.
}
void destroy_device(reshade::api::device*) {
    const auto module=plugin();if(!module)return;
    auto reset=reinterpret_cast<Reset>(GetProcAddress(module,"DS2FX_ResetRemasteredHud"));if(reset)reset();
    ++resets;
}
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID reserved) {
    if(reason==DLL_PROCESS_ATTACH){
        self=module;DisableThreadLibraryCalls(module);
        if(!reshade::register_addon(module,GetModuleHandleW(L"ReShade68.dll")))return FALSE;
        reshade::register_event<reshade::addon_event::present>(present);
        reshade::register_event<reshade::addon_event::destroy_device>(destroy_device);
    }else if(reason==DLL_PROCESS_DETACH && !reserved){
        reshade::unregister_event<reshade::addon_event::present>(present);
        reshade::unregister_event<reshade::addon_event::destroy_device>(destroy_device);
        reshade::unregister_addon(self);
    }
    return TRUE;
}
