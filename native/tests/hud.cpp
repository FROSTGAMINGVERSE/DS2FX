#include "../mods/player_hud/hud_view.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void check(bool valid,const char* message) { if (!valid) throw std::runtime_error(message); }
void log(const char*,const char* message) { std::cout << message << '\n'; }
uint32_t query(DS2FX_PlayerState*) { return 0; }
void preview(const std::filesystem::path& path,DS2FX_PlayerState state,COLORREF expected) {
    const int width = 560,height = 280;
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -height; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    auto bitmap = CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    check(dc && bitmap && pixels,"Cannot create rendering fixture");
    auto old = SelectObject(dc,bitmap);
    hud::draw(dc,width,height,state,{});
    check(GetPixel(dc,1,90) == expected,"Warning color does not match state");
    GdiFlush();
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42; file.bfOffBits = sizeof(file)+sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits+width*height*4;
    std::ofstream output(path,std::ios::binary);
    output.write(reinterpret_cast<char*>(&file),sizeof(file));
    output.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    output.write(static_cast<char*>(pixels),width*height*4);
    check(output.good(),"Preview save failed");
    SelectObject(dc,old); DeleteObject(bitmap); DeleteDC(dc);
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc == 2,"Expected plugin DLL argument");
        DS2FX_PlayerState healthy{sizeof(DS2FX_PlayerState),1,1,826,826};
        DS2FX_PlayerState damaged{sizeof(DS2FX_PlayerState),1,2,247,826};
        healthy.stamina_available=damaged.stamina_available=1;
        healthy.stamina=97.5f; healthy.max_stamina=100;
        damaged.stamina=12; damaged.max_stamina=100;
        check(!hud::low(healthy,30) && hud::low(damaged,30),"Warning threshold failed");
        auto boundary = healthy; boundary.health = 30; boundary.max_health = 100;
        check(hud::low(boundary,30),"Threshold must be inclusive");
        boundary.health = 31; check(!hud::low(boundary,30),"Above threshold must be normal");
        boundary.max_health = 0; check(!hud::valid(boundary),"Zero maximum must be rejected");
        check(hud::visible(healthy,true,true,false,100),"Healthy foreground should show");
        check(!hud::visible(healthy,true,false,false,100),"Alt-tab should hide");
        check(!hud::visible(healthy,true,true,true,100),"Minimize should hide");
        check(!hud::visible(healthy,true,true,false,1501),"Stale observations should hide");
        auto absent = healthy; absent.available = 0;
        check(!hud::visible(absent,true,true,false,100),"Character unload should hide");
        check(!hud::visible(healthy,false,true,false,100),"Disabled HUD should hide");
        auto rect = hud::placement({100,100,1100,900},280,88,{},96);
        check(rect.left == 796 && rect.top == 124,"HUD must follow client-relative top-right anchor");
        hud::Settings small; small.x = 4096; small.y = 4096;
        rect = hud::placement({100,100,500,400},280,88,small,96);
        check(rect.left >= 100 && rect.right <= 500 && rect.bottom <= 400,"HUD must clamp to client bounds");
        auto output = std::filesystem::path(argv[1]).parent_path()/"hud-previews";
        std::filesystem::create_directories(output);
        preview(output/"normal.bmp",healthy,RGB(111,220,195));
        preview(output/"low-health.bmp",damaged,RGB(255,105,91));
        // Exercise the production plugin with a mock ABI host, without touching DS2.
        HMODULE dll = LoadLibraryW(argv[1]); check(dll != nullptr,"HUD DLL failed to load");
        auto init = reinterpret_cast<DS2FX_InitializePluginFn>(GetProcAddress(dll,"DS2FX_InitializePlugin"));
        check(init != nullptr,"HUD export missing");
        DS2FX_Api api{sizeof(DS2FX_Api),DS2FX_API_VERSION,"0.3.0-alpha.1",log,query};
        DS2FX_Plugin plugin{}; plugin.size = sizeof(plugin);
        check(init(&api,&plugin) == 1,"HUD initialization failed");
        check(std::string(plugin.id) == "frost-player-hud" && plugin.on_event,"HUD identity/API invalid");
        auto window = FindWindowW(hud::window_class,L"DS2FX Player HUD");
        check(window != nullptr,"HUD window missing");
        const auto style = GetWindowLongPtrW(window,GWL_EXSTYLE);
        check((style & (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) == (WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW),"HUD must be layered, click-through, non-activating and hidden from task switcher");
        check(SendMessageW(window,WM_NCHITTEST,0,0) == HTTRANSPARENT,"HUD must pass through hit tests");
        plugin.on_event(DS2FX_PLAYER_AVAILABLE,&healthy);
        plugin.on_event(DS2FX_HEALTH_CHANGED,&damaged);
        plugin.on_event(DS2FX_PLAYER_UNAVAILABLE,&absent);
        Sleep(200);
        check(!IsWindowVisible(window),"HUD should be hidden without a visible game window");
        HWND owner = CreateWindowExW(0,L"STATIC",L"DS2FX owned test window",WS_OVERLAPPEDWINDOW,0,0,800,600,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        check(owner != nullptr,"Cannot create owned stacking fixture");
        check(hud::position_window(window,owner,{50,50,330,138},false) == ERROR_SUCCESS,"HUD positioning helper failed");
        check(GetWindow(window,GW_OWNER) == owner,"HUD must be explicitly owned by game window");
        check((GetWindowLongPtrW(window,GWL_EXSTYLE)&WS_EX_TOPMOST) != 0,"HUD must retain topmost band");
        SendMessageW(window,WM_CLOSE,0,0);
        check(!IsWindow(window),"HUD window teardown failed");
        DestroyWindow(owner);
        // DLL remains resident by design until process exit, as with the real host.
        std::cout << "PASS: threshold boundaries, visibility, client positioning, rendered colors, ABI loading, click-through styles and teardown\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
