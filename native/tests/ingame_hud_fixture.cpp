#include "../mods/ds2_remastered_hud/remastered_ingame.h"
#include "../src/graphics_bridge.h"
#include <cstdio>
namespace {
HMODULE self=nullptr;
uint32_t grants=0,revokes=0;
uint64_t epoch=1;
uint32_t state(DS2FX_PlayerState* out){*out={sizeof(*out)};out->available=1;out->character_epoch=epoch;return 1;}
uint32_t icons(DS2FX_HudIcons* out){*out={sizeof(*out)};return 0;}
uint32_t lease(uint64_t owner,uint64_t,uint32_t regions){if(!regions){++revokes;return 0;}if(owner!=epoch)return 0;++grants;return regions&DS2FX_HUD_REGION_BARS;}
void log(const char*,const char* message){std::printf("FIXTURE %s\n",message);}
DS2FX_Api api{};
uint64_t subscribe(const DS2FX_GraphicsCallback* callback){return fx::graphics::subscribe(self,callback);}
uint32_t unsubscribe(uint64_t handle){return fx::graphics::unsubscribe(self,handle);}
}
extern "C" __declspec(dllexport) bool FixturePublish(HWND game,bool stale) {
    api.size=sizeof(api);api.version=DS2FX_API_VERSION;api.get_player_state=state;api.get_hud_icons=icons;api.update_hud_region_mask=lease;api.log=log;
    if(!GetModuleHandleW(L"ReShade68.dll")){api.register_graphics_callback=subscribe;api.unregister_graphics_callback=unsubscribe;}
    remastered::ingame::configure(self,&api);
    remastered::ingame::attach();
    auto frame=std::make_shared<remastered::ingame::Frame>();
    frame->game=game;frame->epoch=epoch;frame->sample=1;frame->created=GetTickCount64()-(stale?201:0);frame->revision=grants+1;
    frame->width=frame->height=2;frame->canvas_width=frame->canvas_height=4;frame->x=frame->y=1;frame->regions=DS2FX_HUD_REGION_BARS;
    for(int i=0;i<4;++i)frame->pixels[1].insert(frame->pixels[1].end(),{0,0,255,255});
    remastered::ingame::publish(frame);return true;
}
extern "C" __declspec(dllexport) unsigned FixtureGrants(){return grants;}
extern "C" __declspec(dllexport) unsigned FixtureRevokes(){return revokes;}
extern "C" __declspec(dllexport) void FixtureDetach(){remastered::ingame::detach();}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);}return TRUE;}
