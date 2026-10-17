#include "hud_view.h"
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <cstdio>

namespace {
HMODULE self = nullptr;
const DS2FX_Api* api = nullptr;
hud::Settings settings;
std::mutex state_mutex;
DS2FX_PlayerState latest{sizeof(DS2FX_PlayerState),0,0,0,0};
ULONGLONG last_update = 0;
DS2FX_PlayerState painted{sizeof(DS2FX_PlayerState),0,0,0,0};
std::atomic<bool> initialized{false}, window_ready{false};
HANDLE ready_event = nullptr;
HWND overlay = nullptr;
bool last_visible = false;
DWORD last_position_error = 0;

int setting(const std::filesystem::path& file,const wchar_t* key,int fallback,int minimum,int maximum) {
    wchar_t value[64];
    GetPrivateProfileStringW(L"HUD",key,L"",value,64,file.c_str());
    if (!value[0]) return fallback;
    wchar_t* end = nullptr;
    long parsed = wcstol(value,&end,10);
    return end && !*end && parsed >= minimum && parsed <= maximum ? static_cast<int>(parsed) : fallback;
}
void load_settings() {
    wchar_t path[32768];
    auto length = GetModuleFileNameW(self,path,32768);
    if (!length || length >= 32768) return;
    auto config = std::filesystem::path(path).parent_path()/L"hud.ini";
    settings.enabled = setting(config,L"Enabled",1,0,1);
    settings.threshold = setting(config,L"LowHealthPercent",30,1,100);
    settings.scale = setting(config,L"ScalePercent",100,50,200);
    settings.x = setting(config,L"MarginX",24,0,4096);
    settings.y = setting(config,L"MarginY",24,0,4096);
    settings.anchor_right = setting(config,L"AnchorRight",1,0,1);
    settings.show_stamina = setting(config,L"ShowStamina",1,0,1);
    settings.opacity = setting(config,L"Opacity",240,32,255);
}
BOOL CALLBACK find_game(HWND window,LPARAM result) {
    DWORD pid = 0; GetWindowThreadProcessId(window,&pid);
    if (pid != GetCurrentProcessId() || window == overlay || !IsWindowVisible(window) || GetWindow(window,GW_OWNER)) return TRUE;
    if (GetWindowLongPtrW(window,GWL_EXSTYLE) & WS_EX_TOOLWINDOW) return TRUE;
    RECT rect{};
    if (!GetClientRect(window,&rect) || rect.right < 320 || rect.bottom < 200) return TRUE;
    *reinterpret_cast<HWND*>(result) = window;
    return FALSE;
}
void refresh(HWND window) {
    DS2FX_PlayerState next{};
    ULONGLONG updated = 0;
    { std::lock_guard lock(state_mutex); next = latest; updated = last_update; }
    HWND game = nullptr;
    EnumWindows(find_game,reinterpret_cast<LPARAM>(&game));
    const auto foreground = GetForegroundWindow();
    if (!game || !hud::visible(next,settings.enabled != 0,foreground == game,IsIconic(game) != 0,GetTickCount64()-updated)) {
        if (last_visible) { api->log("frost-player-hud","HUD hidden: game focus, character availability or observation freshness changed"); last_visible = false; }
        ShowWindow(window,SW_HIDE); return;
    }
    RECT client{};
    if (!GetClientRect(game,&client)) { ShowWindow(window,SW_HIDE); return; }
    POINT origin{0,0};
    if (!ClientToScreen(game,&origin)) { ShowWindow(window,SW_HIDE); return; }
    OffsetRect(&client,origin.x,origin.y);
    const auto dpi = static_cast<int>(GetDpiForWindow(game));
    const auto width = std::min(static_cast<int>(client.right-client.left),MulDiv(280,settings.scale*dpi,9600));
    const auto height = MulDiv(settings.show_stamina ? 140 : 88,width,280);
    auto position = hud::placement(client,width,height,settings,dpi);
    const auto error = hud::position_window(window,game,position,true);
    if (error) {
        if (error != last_position_error) {
            char diagnostic[120]; std::snprintf(diagnostic,sizeof(diagnostic),"HUD positioning failed: Win32=%lu",error);
            api->log("frost-player-hud",diagnostic);
        }
        last_position_error = error;
        return;
    }
    last_position_error = 0;
    if (!last_visible) {
        char diagnostic[240];
        std::snprintf(diagnostic,sizeof(diagnostic),"HUD shown: HP=%d/%d position=%ld,%ld size=%dx%d owned=%d topmost=%d",next.health,next.max_health,position.left,position.top,width,height,GetWindow(window,GW_OWNER)==game,(GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0);
        api->log("frost-player-hud",diagnostic);
        last_visible = true;
        InvalidateRect(window,nullptr,FALSE);
    }
    const bool changed = next.health != painted.health || next.max_health != painted.max_health || next.available != painted.available || next.stamina_available != painted.stamina_available || next.stamina != painted.stamina || next.max_stamina != painted.max_stamina;
    painted = next;
    if (changed) InvalidateRect(window,nullptr,FALSE);
}
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch (message) {
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_ERASEBKGND: return 1;
        case WM_TIMER: refresh(window); return 0;
        case WM_SIZE: InvalidateRect(window,nullptr,FALSE); return 0;
        case WM_PAINT: {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(window,&paint);
            RECT rect{}; GetClientRect(window,&rect);
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc,std::max(1L,rect.right),std::max(1L,rect.bottom));
            auto old = SelectObject(memory,bitmap);
            hud::draw(memory,rect.right,rect.bottom,painted,settings);
            BitBlt(dc,0,0,rect.right,rect.bottom,memory,0,0,SRCCOPY);
            SelectObject(memory,old); DeleteObject(bitmap); DeleteDC(memory);
            EndPaint(window,&paint); return 0;
        }
        case WM_CLOSE: DestroyWindow(window); return 0;
        case WM_DESTROY: KillTimer(window,1); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
DWORD WINAPI run_ui(void*) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    WNDCLASSW window_class{};
    window_class.hInstance = self; window_class.lpfnWndProc = procedure; window_class.lpszClassName = hud::window_class;
    if (!RegisterClassW(&window_class)) { SetEvent(ready_event); return 1; }
    overlay = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        hud::window_class,L"DS2FX Player HUD",WS_POPUP,0,0,280,88,nullptr,nullptr,self,nullptr);
    if (overlay && SetLayeredWindowAttributes(overlay,0,static_cast<BYTE>(settings.opacity),LWA_ALPHA) && SetTimer(overlay,1,100,nullptr)) window_ready = true;
    SetEvent(ready_event);
    if (!window_ready) { if (overlay) DestroyWindow(overlay); return 1; }
    MSG message{};
    while (GetMessageW(&message,nullptr,0,0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    UnregisterClassW(hud::window_class,self);
    return 0;
}
void on_event(uint32_t,const DS2FX_PlayerState* state) {
    if (!state || state->size != sizeof(DS2FX_PlayerState)) return;
    std::lock_guard lock(state_mutex);
    latest = *state; last_update = GetTickCount64();
}
}
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* host,DS2FX_Plugin* output) {
    if (!host || host->size < sizeof(DS2FX_Api) || host->version != DS2FX_API_VERSION || !output || output->size != sizeof(DS2FX_Plugin)) return 0;
    if (initialized.exchange(true)) return 0;
    api = host;
    try { load_settings(); } catch (...) { api->log("frost-player-hud","Configuration read failed; initialization refused"); return 0; }
    if (settings.enabled) {
        ready_event = CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if (!ready_event) return 0;
        HANDLE thread = CreateThread(nullptr,0,run_ui,nullptr,0,nullptr);
        if (!thread) { CloseHandle(ready_event); ready_event = nullptr; return 0; }
        CloseHandle(thread);
        // On timeout the event remains valid for the UI thread; startup will fail.
        if (WaitForSingleObject(ready_event,5000) != WAIT_OBJECT_0) return 0;
        CloseHandle(ready_event); ready_event = nullptr;
        if (!window_ready) return 0;
    }
    *output = {sizeof(DS2FX_Plugin),DS2FX_API_VERSION,"frost-player-hud","0.2.0",on_event};
    api->log("frost-player-hud",settings.enabled ? "HUD initialized: numeric HP, optional stamina and low-health warning; windowed/borderless; follows foreground game; no renderer hooks" : "HUD disabled in hud.ini");
    return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { self = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
