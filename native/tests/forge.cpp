#include "../src/forge_ui.h"
#include "../src/forge_settings.h"
#include "../src/forge_options.h"
#include "../src/forge_hud_presets.h"
#include "../src/forge_cursor.h"
#include "../mods/ds2_remastered_hud/remastered_layout.h"
#include <iostream>
namespace {
remastered::LayoutPreview received;
int cursor_calls=0,cursor_x=0,cursor_y=0;
int clip_calls=0;const RECT* clip_received=nullptr;
BOOL WINAPI clip_original(const RECT* bounds){++clip_calls;clip_received=bounds;return FALSE;}
BOOL WINAPI cursor_original(int x,int y){++cursor_calls;cursor_x=x;cursor_y=y;return FALSE;}
LRESULT CALLBACK editor_fixture(HWND w,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_COPYDATA){const auto* data=reinterpret_cast<COPYDATASTRUCT*>(lp);
        if(data && data->dwData==remastered::layout_message && data->cbData==sizeof(received)){
            received=*static_cast<const remastered::LayoutPreview*>(data->lpData);return TRUE;}return FALSE;}
    if(msg==WM_APP+41)return 2;
    return DefWindowProcW(w,msg,wp,lp);
}
}
int wmain(int argc,wchar_t** argv) {
    // The product UI thread uses physical per-monitor coordinates. Match that
    // context in this owned input fixture rather than virtualizing its points.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    if(argc==7 && std::wstring(argv[1])==L"--preview-mod")return fx::forge::preview(argv[2],argv[3],_wtoi(argv[4]),_wtoi(argv[4])*720/1200,fx::fs::path(argv[5]).string(),_wtoi(argv[6]))?0:1;
    if(argc==3 && std::wstring(argv[1])==L"--smoke") {
        const auto sandbox=fx::fs::temp_directory_path()/("DS2FX-Forge-ui-"+std::to_string(GetCurrentProcessId()));
        fx::fs::create_directories(sandbox/"DS2FX"/"Forge");
        fx::fs::copy(argv[2],sandbox/"DS2FX"/"Forge"/"assets",fx::fs::copy_options::recursive);
        SetEnvironmentVariableW(L"LOCALAPPDATA",sandbox.c_str());
        WNDCLASSW owned{};owned.hInstance=GetModuleHandleW(nullptr);owned.lpfnWndProc=editor_fixture;owned.lpszClassName=L"DS2FX.EditorFixture.Game";RegisterClassW(&owned);
        auto fixture_game=CreateWindowW(owned.lpszClassName,L"Owned editor fixture",WS_OVERLAPPEDWINDOW|WS_VISIBLE,50,50,1280,760,nullptr,nullptr,owned.hInstance,nullptr);
        owned.lpszClassName=L"DS2FX.RemasteredHUD.1";RegisterClassW(&owned);
        auto fixture_hud=CreateWindowExW(WS_EX_TOOLWINDOW,owned.lpszClassName,L"Owned HUD fixture",WS_POPUP,0,0,1,1,nullptr,nullptr,owned.hInstance,nullptr);
        fx::forge::Mod mod;mod.id="modern-souls-2";mod.name=L"Modern Souls 2";mod.jump_option=true;mod.estus_option=true;
        fx::forge::Mod hud;hud.id="ds2-remastered-hud";hud.name=L"Remastered HUD";
        fx::forge::Mod lantern;lantern.id="wayfarers-lantern";lantern.name=L"Wayfarer's Lantern";
        fx::forge::Mod core;core.id="ds2fx-core";core.name=L"DS2FX Core";
        if(!fx::forge::start(GetModuleHandleW(nullptr),sandbox/"DS2FX",{mod,hud,lantern,core},true,[](const char*,const char* s){std::cout<<s<<'\n';}))return 2;
        Sleep(150); // Let the real Forge tick discover the owned game fixture.
        auto window=FindWindowW(L"DS2FX.Forge.Menu.1",nullptr);DWORD pid=0;GetWindowThreadProcessId(window,&pid);
        if(!window || pid!=GetCurrentProcessId())return 3;
        // Synthetic messages exercise the real procedure; physical mouse
        // activity must not enter this disposable fixture between them.
        EnableWindow(window,FALSE);
        SendMessageW(window,WM_KEYDOWN,VK_TAB,0); // presentation page
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0); // scale decrement
        const auto file=fx::forge::settings_file();
        if(fx::forge::setting(file,L"Forge",L"ScalePercent",100,70,125)!=95)return 4;
        SendMessageW(window,WM_KEYDOWN,VK_TAB,0); // module page
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0); // save jump OFF
        if(fx::forge::jump_enabled())return 5;
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,1LL<<30); // key repeat must not toggle
        if(fx::forge::jump_enabled())return 6;
        SendMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::estus_enabled()||fx::forge::jump_enabled())return 8;
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,1LL<<30);
        if(fx::forge::estus_enabled())return 9;
        SendMessageW(window,WM_KEYDOWN,VK_LEFT,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(!fx::forge::jump_enabled()||fx::forge::estus_enabled())return 10;
        SendMessageW(window,WM_KEYDOWN,VK_DOWN,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"ds2-remastered-hud",L"ScalePercent",100,50,200)!=105)return 11;
        for(int i=0;i<3;++i)SendMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"ds2-remastered-hud",L"Pulse",1,0,1)!=0)return 12;
        for(int i=0;i<11;++i)SendMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"ds2-remastered-hud",L"SkinMode",0,0,2)!=1)return 13;
        SendMessageW(window,WM_KEYDOWN,VK_DOWN,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"wayfarers-lantern",L"LanternFlickerPercent",10,0,12)!=11)return 14;
        SendMessageW(window,WM_KEYDOWN,VK_DOWN,0);
        SendMessageW(window,WM_KEYDOWN,VK_RIGHT,0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"ds2fx-core",L"VerboseLanternTelemetry",0,0,1)!=1)return 15;
        SendMessageW(window,WM_KEYDOWN,VK_UP,0);SendMessageW(window,WM_KEYDOWN,VK_UP,0);
        SendMessageW(window,WM_KEYDOWN,'E',0);
        if(received.editing!=1)return 16;
        fx::forge::cursor::original=cursor_original;
        if(!fx::forge::cursor::warp(window,window,321,456) || cursor_calls)return 24;
        if(fx::forge::cursor::warp(nullptr,window,321,456) || cursor_calls!=1 || cursor_x!=321 || cursor_y!=456)return 25;
        if(fx::forge::cursor::warp(fixture_game,window,12,34) || cursor_calls!=2 || cursor_x!=12 || cursor_y!=34)return 26;
        if(!fx::forge::cursor::warp(window,fixture_game,56,78) || cursor_calls!=2)return 27;
        if(fx::forge::cursor::warp(window,fixture_hud,91,92) || cursor_calls!=3 || cursor_x!=91 || cursor_y!=92)return 30;
        fx::forge::cursor::original_clip=clip_original;RECT clip_bounds{1,2,3,4};
        if(!fx::forge::cursor::clip(window,fixture_game,&clip_bounds) || clip_calls)return 31;
        if(fx::forge::cursor::clip(nullptr,window,&clip_bounds) || clip_calls!=1 || clip_received!=&clip_bounds)return 32;
        if(!remastered::forge_focus(window,fixture_game) || remastered::forge_focus(fixture_hud,fixture_game))return 22;
        RECT area{};GetClientRect(window,&area);
        auto original=received.layout;
        auto b=remastered::part_box(1,area.right,area.bottom,105,24,received.layout,2);
        const int x=static_cast<int>(b.x+b.w/2),y=static_cast<int>(b.y+b.h/2);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x+100,y+70));
        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(x+100,y+70));
        if(received.layout.items[1].x<0 || received.layout.items[1].y<0)return 17;
        if(fx::forge::setting(file,L"HUD.Layout.1",L"X",-1,-1,10000)!=-1)return 18;
        SendMessageW(window,WM_KEYDOWN,VK_ESCAPE,0);
        if(received.editing || received.layout.items[1].x!=original.items[1].x)return 19;
        SendMessageW(window,WM_KEYDOWN,'E',0);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
        SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x+100,y+70));
        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(x+100,y+70));
        b=remastered::part_box(1,area.right,area.bottom,105,24,received.layout,2);
        const int rx=static_cast<int>(b.x+b.w-2),ry=static_cast<int>(b.y+b.h-2);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(rx,ry));
        SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(rx+70,ry+10));
        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(rx+70,ry+10));
        if(received.layout.items[1].size<=100){std::cerr<<"Resize failed area="<<area.right<<'x'<<area.bottom<<" box="<<b.x<<','<<b.y<<','<<b.w<<','<<b.h<<" selected sizes="<<received.layout.items[0].size<<','<<received.layout.items[1].size<<','<<received.layout.items[2].size<<','<<received.layout.items[3].size<<'\n';return 20;}
        b=remastered::part_box(1,area.right,area.bottom,105,24,received.layout,2);
        const auto previous_size=received.layout.items[1].size;
        const int lx=static_cast<int>(b.x+b.w-2),ly=static_cast<int>(b.y+b.h/2);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(lx,ly));
        SendMessageW(window,WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(lx+90,ly));
        SendMessageW(window,WM_LBUTTONUP,0,MAKELPARAM(lx+90,ly));
        if(received.layout.items[1].length<=100 || received.layout.items[1].size!=previous_size)return 28;
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(received.editing || fx::forge::setting(file,L"HUD.Layout.1",L"X",-1,-1,10000)!=received.layout.items[1].x)return 21;
        if(fx::forge::setting(file,L"HUD.Layout.1",L"Length",100,25,300)!=received.layout.items[1].length || fx::forge::setting(file,L"HUD.Editor",L"Grid",0,0,200)!=100)return 29;
        SendMessageW(window,WM_KEYDOWN,'P',0);
        RECT preset_area{};GetClientRect(window,&preset_area);
        SendMessageW(window,WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(static_cast<int>(740.f/1200*preset_area.right),static_cast<int>(396.f/720*preset_area.bottom)));
        if(fx::forge::setting(file,L"ds2-remastered-hud",L"SkinMode",0,0,2)!=2)return 32;
        SendMessageW(window,WM_KEYDOWN,'S',0);
        if(!fx::forge::hud_preset_exists(1))return 30;
        fx::forge::save_setting(L"ds2-remastered-hud",L"ScalePercent",150);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,0);
        if(fx::forge::setting(file,L"ds2-remastered-hud",L"ScalePercent",100,50,200)!=105)return 31;
        SendMessageW(window,WM_KEYDOWN,'P',0);
        DestroyWindow(fixture_hud);DestroyWindow(fixture_game);
        if(!fx::forge::shutdown() || IsWindow(window))return 7;
        std::cout<<"PASS Forge window lifecycle, keyboard navigation, setting persistence, key-repeat rejection, shutdown\n";return 0;
    }
    if(argc==4 && std::wstring(argv[1])==L"--title") return fx::forge::title_texture(argv[2],argv[3])?0:1;
    if(argc==5 && std::wstring(argv[1])==L"--preview") return fx::forge::preview(argv[2],argv[3],_wtoi(argv[4]),_wtoi(argv[4])*720/1200)?0:1;
    try {
        for(auto bounds:{RECT{0,0,1280,720},RECT{0,0,3840,2160},RECT{-3440,0,0,1440},RECT{1920,300,3200,1020}})
            for(auto percent:{70,100,125}) for(auto le:{.75f,1.f,1.5f}) {
                auto p=fx::forge::layout(bounds,percent,le);
                fx::require(p.x>=bounds.left && p.y>=bounds.top && p.x+p.width<=bounds.right && p.y+p.height<=bounds.bottom,"Forge outside display");
                fx::require(abs((p.x*2+p.width)-(bounds.left+bounds.right))<=1,"Forge not horizontally centered");
                fx::require(abs((p.y*2+p.height)-(bounds.top+bounds.bottom))<=1,"Forge not vertically centered");
            }
        auto p=fx::forge::layout({0,0,3840,2160},100);
        fx::require(p.width>2000 && p.height>1200,"4K layout did not scale");
        auto tmp=fx::fs::temp_directory_path()/("forge-settings-"+std::to_string(GetCurrentProcessId())+".ini");
        std::ofstream(tmp)<<"[Forge]\nScalePercent=garbage\nOpacity=999\n";
        fx::require(fx::forge::setting(tmp,L"Forge",L"ScalePercent",100,70,125)==100,"Invalid setting accepted");
        fx::require(fx::forge::setting(tmp,L"Forge",L"Opacity",238,204,255)==238,"Unbounded opacity accepted");
        const auto sandbox=fx::fs::temp_directory_path()/("forge-presets-"+std::to_string(GetCurrentProcessId()));
        SetEnvironmentVariableW(L"LOCALAPPDATA",sandbox.c_str());
        fx::forge::save_setting(L"modern-souls-2",L"DedicatedJump",0);
        fx::forge::save_setting(L"HUD.Layout.1",L"X",598);
        fx::forge::save_setting(L"HUD.Layout.2",L"Size",102);
        fx::forge::save_setting(L"HUD.Editor",L"Grid",100);
        fx::require(fx::forge::hud_preset_save(3),"Preset save failed");
        fx::forge::save_setting(L"HUD.Layout.1",L"X",999);
        fx::require(fx::forge::hud_theme(2),"Theme apply failed");
        fx::require(fx::forge::setting(fx::forge::settings_file(),L"HUD.Layout.1",L"X",-1,-1,10000)==999,"Theme moved layout");
        fx::require(fx::forge::hud_preset_load(3),"Preset load failed");
        fx::require(fx::forge::setting(fx::forge::settings_file(),L"HUD.Layout.1",L"X",-1,-1,10000)==598,"Layout not restored");
        fx::require(!fx::forge::jump_enabled(),"Preset changed unrelated gameplay settings");
        const auto before=fx::hash_file(fx::forge::settings_file());
        fx::require(!fx::forge::hud_preset_load(6),"Empty preset loaded");
        WritePrivateProfileStringW(L"HUD.Layout.1",L"X",L"broken",fx::forge::hud_preset_file(3).c_str());
        fx::require(!fx::forge::hud_preset_load(3),"Corrupt preset loaded");
        fx::require(fx::hash_file(fx::forge::settings_file())==before,"Rejected preset changed settings");
        fx::require(!fx::forge::hud_preset_save(7),"Invalid slot accepted");
        std::cout<<"PASS HUD presets: disk roundtrip, layout, theme isolation, unrelated settings, empty/corrupt rejection\n";
        fx::fs::remove(tmp);std::cout<<"PASS Forge: 36 layouts, 4K scaling, negative monitor coordinates, malformed settings\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what();return 1;}
}
