#include "remastered_view.h"
#include "remastered_geometry.h"
#include "remastered_surface.h"
#include "remastered_ingame.h"
#include "../../src/forge_options.h"
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace {
HMODULE self=nullptr;
const DS2FX_Api* api=nullptr;
remastered::Settings settings;
std::mutex state_mutex;
DS2FX_PlayerState latest{sizeof(DS2FX_PlayerState)};
DS2FX_HudState hud{sizeof(DS2FX_HudState)};
DS2FX_HudPresentation presentation{sizeof(DS2FX_HudPresentation)};
DS2FX_HudIcons icons{sizeof(DS2FX_HudIcons)};
DS2FX_PlayerForm form{sizeof(DS2FX_PlayerForm)};
remastered::Assets assets;
remastered::Skins skins;
remastered::FormSelector form_selector;
remastered::BarAnimation bar_animation;
remastered::Surface surface;
bool layout_editing=false;
uint64_t last_update=0;
std::atomic<bool> initialized{false},window_ready{false};
HANDLE ready_event=nullptr;
HWND overlay=nullptr;
uint64_t ready_since=0,ready_epoch=0;

int setting(const std::filesystem::path& file,const wchar_t* key,int fallback,int low,int high) {
    wchar_t value[64]{};
    GetPrivateProfileStringW(L"HUD",key,L"",value,64,file.c_str());
    if (!value[0]) return fallback;
    wchar_t* end=nullptr;
    const long number=wcstol(value,&end,10);
    return end && !*end && number>=low && number<=high ? static_cast<int>(number) : fallback;
}
void load_settings() {
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(self,path,32768);
    if (!length || length>=32768) return;
    const auto file=std::filesystem::path(path).parent_path()/L"hud.ini";
    settings.enabled=setting(file,L"Enabled",1,0,1);
    settings.scale=setting(file,L"ScalePercent",100,50,200);
    settings.margin=setting(file,L"Margin",24,0,128);
    settings.opacity=setting(file,L"Opacity",245,32,255);
    settings.pulse=setting(file,L"Pulse",1,0,1);
    settings.diagnostic_ids=setting(file,L"DiagnosticIds",0,0,1);
    settings.show_quickslots=setting(file,L"ShowQuickSlots",0,0,1);
    settings.show_souls=setting(file,L"ShowSouls",0,0,1);
    settings.skin_mode=setting(file,L"SkinMode",0,0,2);
    const auto cfg=fx::forge::settings_file();
    auto option=[&](const wchar_t* key,int fallback,int low,int high){return fx::forge::setting(cfg,L"ds2-remastered-hud",key,fallback,low,high);};
    settings.scale=option(L"ScalePercent",settings.scale,50,200);
    settings.margin=option(L"Margin",settings.margin,4,128);
    settings.opacity=option(L"Opacity",settings.opacity,32,255);
    settings.pulse=option(L"Pulse",settings.pulse,0,1);
    settings.skin_mode=option(L"SkinMode",settings.skin_mode,0,2);
    settings.pulse_strength=option(L"PulseStrength",100,0,100);
    settings.ember_strength=option(L"EmberStrength",60,0,100);
    settings.animation_fps=option(L"AnimationFPS",60,30,60);
    settings.icon_size=option(L"StatusIconSize",20,18,32);
    settings.health_hold=option(L"HealthHoldMs",260,0,600);
    settings.health_drain=option(L"HealthDrainMs",250,80,600);
    settings.stamina_hold=option(L"StaminaHoldMs",160,0,400);
    settings.stamina_drain=option(L"StaminaDrainMs",180,80,500);
    settings.stat_lengths=option(L"StatBarLengths",1,0,1);settings.shadow_strength=option(L"ShadowStrength",40,0,100);
    if(!layout_editing)for(int i=0;i<4;++i){
        const auto section=std::wstring(L"HUD.Layout.")+std::to_wstring(i);
        settings.layout.items[i]={fx::forge::setting(cfg,section.c_str(),L"X",-1,-1,10000),
            fx::forge::setting(cfg,section.c_str(),L"Y",-1,-1,10000),fx::forge::setting(cfg,section.c_str(),L"Size",i==3?50:100,25,400),fx::forge::setting(cfg,section.c_str(),L"Length",100,25,300)};
    }
}
BOOL CALLBACK find_game(HWND candidate,LPARAM target) {
    DWORD pid=0; GetWindowThreadProcessId(candidate,&pid);
    if (pid!=GetCurrentProcessId() || candidate==overlay || !IsWindowVisible(candidate) || GetWindow(candidate,GW_OWNER)) return TRUE;
    if (GetWindowLongPtrW(candidate,GWL_EXSTYLE)&WS_EX_TOOLWINDOW) return TRUE;
    RECT r{}; if (!GetClientRect(candidate,&r) || r.right<320 || r.bottom<200) return TRUE;
    *reinterpret_cast<HWND*>(target)=candidate;
    return FALSE;
}
HANDLE frame_timer=nullptr;int timer_interval=0;
void cadence(HWND window,bool active) {
    const int desired=active?std::max(16,1000/settings.animation_fps):100;
    if(desired!=timer_interval){
        LARGE_INTEGER due{};due.QuadPart=-10000LL*desired;
        if(SetWaitableTimerEx(frame_timer,&due,desired,nullptr,nullptr,nullptr,0))timer_interval=desired;
    }
}
void revoke() noexcept {
    remastered::ingame::clear();
    if(api && api->update_hud_regions)api->update_hud_regions(0,0,0);
    if(api && api->update_hud_replacement) api->update_hud_replacement(0,0);
}
void hide(HWND window) noexcept {
    cadence(window,false);
    bar_animation.reset();
    revoke();
    ShowWindow(window,SW_HIDE);
}
void report_fault(const char* reason,const DS2FX_PlayerState& player,const DS2FX_HudPresentation& native,
    const DS2FX_HudIcons& icon_state,uint64_t age,uint64_t elapsed=0) {
    static uint64_t last_report=0;const auto now=GetTickCount64();
    if(now-last_report<1000)return;last_report=now;
    char message[320]{};
    std::snprintf(message,sizeof(message),"HUD hidden reason=%s sample=%llu epoch=%llu native_sample=%llu native_available=%u icon_sample=%llu regions=%u age_ms=%llu render_ms=%llu",
        reason,player.sample,player.character_epoch,native.sample,native.available,icon_state.sample,icon_state.available_regions,age,elapsed);
    api->log("ds2-remastered-hud",message);
}
void refresh(HWND window) {
    remastered::ingame::attach();
    DS2FX_PlayerState player{}; DS2FX_HudState extra{}; DS2FX_HudPresentation native{};DS2FX_HudIcons icon_state{};DS2FX_PlayerForm player_form{};uint64_t update=0;
    { std::lock_guard lock(state_mutex); player=latest; extra=hud;native=presentation;icon_state=icons;player_form=form; update=last_update; }
    static HWND game=nullptr;
    if(!game || !IsWindow(game)) {game=nullptr;EnumWindows(find_game,reinterpret_cast<LPARAM>(&game));}
    const auto now=GetTickCount64();
    static uint64_t settings_at=0;
    if(now-settings_at>=1000) {settings_at=now;load_settings();}
    const auto skin=form_selector.select(player,player_form,now,settings.skin_mode);
    static int previous_skin=-1;
    if(player.available && previous_skin!=static_cast<int>(skin)) {
        previous_skin=static_cast<int>(skin);char message[160]{};
        std::snprintf(message,sizeof(message),"Grimdark skin=%s form_available=%u hollow_level=%u epoch=%llu mode=%d",
            skin==remastered::Skin::Human?"human":"hollow",player_form.available,player_form.hollow_level,player.character_epoch,settings.skin_mode);
        api->log("ds2-remastered-hud",message);
    }
    if(!game || !remastered::replacement_owned(player,extra,native,now-update,settings.enabled!=0)){
        if(game && player.available)report_fault("replacement-ownership",player,native,icon_state,now-update);
        ready_since=0;hide(window);return;
    }
    const bool scene=remastered::gameplay_scene(extra);
    if(!scene)ready_since=0;
    else if(!ready_since || ready_epoch!=player.character_epoch){ready_since=now;ready_epoch=player.character_epoch;}
    // Game-frame rendering continues while visible but unfocused. Minimize,
    // death and loading still hide presentation, not an authenticated native UI.
    // Renew only after a successful transparent surface update; a stalled or
    // failed renderer still expires the same short lease and restores native draws.
    const int blank=IsIconic(game)?1:player.health==0?2:!scene?3:!remastered::scene_settled(now,ready_since)?4:0;
    static int previous_blank=-1;
    if(blank!=previous_blank){previous_blank=blank;char message[160]{};
        std::snprintf(message,sizeof(message),"HUD presentation mode=%d health=%d game_state=%d loading=%u epoch=%llu",blank,player.health,extra.game_state,extra.loading,player.character_epoch);
        api->log("ds2-remastered-hud",message);}
    if(blank){
        cadence(window,false);bar_animation.reset();
        if(icon_state.sample!=player.sample || icon_state.character_epoch!=player.character_epoch)icon_state.available_regions=0;
        auto frame=std::make_shared<remastered::ingame::Frame>();
        frame->game=game;frame->epoch=player.character_epoch;frame->sample=player.sample;frame->created=now;
        frame->blank=true;frame->regions=DS2FX_HUD_REGION_BARS|remastered::available_icon_regions(icon_state,assets);frame->icons=icon_state;
        remastered::ingame::publish(std::move(frame));
        return;
    }
    static RECT client{};static uint64_t bounds_at=0;
    if(!bounds_at || now-bounds_at>=250) {
        if(!remastered::game_client_bounds(game,client)){hide(window);return;}
        bounds_at=now;
    }
    const int width=client.right-client.left,height=client.bottom-client.top;
    if (width<320 || height<200 || uint64_t(width)*height>16777216) { hide(window); return; }
    cadence(window,true);
    settings.native_hp=native.gauge_available?native.health_length:0;settings.native_sp=native.gauge_available?native.stamina_length:0;
    static int logged_hp=-1;static float logged_sp=-1,logged_native_hp=-1,logged_native_sp=-1;
    if(logged_hp!=player.max_health || logged_sp!=player.max_stamina || logged_native_hp!=settings.native_hp || logged_native_sp!=settings.native_sp){
        logged_hp=player.max_health;logged_sp=player.max_stamina;logged_native_hp=settings.native_hp;logged_native_sp=settings.native_sp;
        const auto hp_box=remastered::part_box(1,width,height,settings.scale,settings.margin,settings.layout,icon_state.effect_count,player.max_health,player.max_stamina,settings.stat_lengths!=0,settings.native_hp,settings.native_sp);
        const auto sp_box=remastered::part_box(2,width,height,settings.scale,settings.margin,settings.layout,icon_state.effect_count,player.max_health,player.max_stamina,settings.stat_lengths!=0,settings.native_hp,settings.native_sp);
        char message[260]{};std::snprintf(message,sizeof(message),"Native gauge available=%u HP=%d SP=%.2f hp_length=%.6f sp_length=%.6f hp_pixels=%.2f sp_pixels=%.2f stat_mode=%d",native.gauge_available,player.max_health,player.max_stamina,settings.native_hp,settings.native_sp,hp_box.w,sp_box.w,settings.stat_lengths);api->log("ds2-remastered-hud",message);
    }
    const auto area=(settings.show_souls || settings.show_quickslots)?RECT{0,0,width,height}:
        remastered::layout_bounds(width,height,settings.scale,settings.margin,settings.layout,icon_state.effect_count,player.max_health,player.max_stamina,settings.stat_lengths!=0,settings.native_hp,settings.native_sp);
    settings.offset_x=area.left;settings.offset_y=area.top;
    const SIZE render{area.right-area.left,area.bottom-area.top};
    if(!surface.resize(render.cx,render.cy)){hide(window);return;}
    auto& graphics=surface.context();
    LARGE_INTEGER begun{},frequency{};QueryPerformanceCounter(&begun);QueryPerformanceFrequency(&frequency);
    if(icon_state.sample!=player.sample || icon_state.character_epoch!=player.character_epoch)icon_state.available_regions=0;
    if(native.opacity==0)bar_animation.reset();
    bar_animation.update(player.character_epoch,player.max_health,player.max_stamina,
        remastered::fraction(static_cast<float>(player.health),static_cast<float>(player.max_health)),
        player.stamina_available?remastered::fraction(player.stamina,player.max_stamina):0.0f,now,settings.health_hold,settings.health_drain,settings.stamina_hold,settings.stamina_drain);
    auto frame=std::make_shared<remastered::ingame::Frame>();
    static uint64_t revision=0;
    frame->game=game;frame->epoch=player.character_epoch;frame->sample=player.sample;frame->created=now;frame->revision=++revision;
    frame->width=render.cx;frame->height=render.cy;frame->canvas_width=width;frame->canvas_height=height;
    frame->x=area.left;frame->y=area.top;frame->opacity=static_cast<BYTE>(settings.opacity);frame->icons=icon_state;
    frame->regions=DS2FX_HUD_REGION_BARS|remastered::available_icon_regions(icon_state,assets);
    constexpr uint32_t layer_bits[]{DS2FX_HUD_REGION_COVENANT,DS2FX_HUD_REGION_BARS,DS2FX_HUD_REGION_EFFECTS};
    for(unsigned i=0;i<3;++i)if(frame->regions&layer_bits[i]) {
        graphics.ResetTransform();
        remastered::draw(graphics,width,height,player,extra,settings,now,static_cast<BYTE>(native.opacity),&icon_state,&assets,skin,&skins,&bar_animation,render.cx,render.cy,layer_bits[i]);
        graphics.Flush(Gdiplus::FlushIntentionSync);
        if(graphics.GetLastStatus()!=Gdiplus::Ok){report_fault("graphics",player,native,icon_state,now-update,GetTickCount64()-now);hide(window);return;}
        frame->pixels[i].assign(surface.pixels(),surface.pixels()+size_t(render.cx)*render.cy*4);
    }
    remastered::ingame::publish(std::move(frame));
    LARGE_INTEGER ended{};QueryPerformanceCounter(&ended);
    static double total_ms=0,max_ms=0;static unsigned frames=0,intervals=0;static uint64_t measured_at=0,frame_at=0,frame_ms=0;
    if(frame_at && now>frame_at && now-frame_at<=100){frame_ms+=now-frame_at;++intervals;}frame_at=now;
    const double ms=1000.0*double(ended.QuadPart-begun.QuadPart)/double(frequency.QuadPart);
    static std::array<uint64_t,128> histogram{};
    static uint64_t lifetime_frames=0,slow_frames=0;
    static double lifetime_ms=0,lifetime_max=0;
    ++histogram[std::min<size_t>(127,static_cast<size_t>(std::max(0.0,ms)*4))];
    ++lifetime_frames;lifetime_ms+=ms;lifetime_max=std::max(lifetime_max,ms);if(ms>4)++slow_frames;
    total_ms+=ms;max_ms=std::max(max_ms,ms);++frames;
    if(now-measured_at>=15000) {
        measured_at=now;char message[512]{};
        auto percentile=[&](uint64_t percent){uint64_t count=0;for(size_t i=0;i<histogram.size();++i){count+=histogram[i];if(count*100>=lifetime_frames*percent)return (i+1)*.25;}return 32.0;};
        std::snprintf(message,sizeof(message),"HUD render surface=%dx%d avg_ms=%.3f max_ms=%.3f frames=%u actual_fps=%.1f target_fps=%d lifetime_frames=%llu lifetime_avg_ms=%.3f p95_ms=%.2f p99_ms=%.2f lifetime_max_ms=%.3f over4ms=%llu cache_builds=%llu cache_total_ms=%.3f cache_max_ms=%.3f",render.cx,render.cy,total_ms/frames,max_ms,frames,frame_ms?intervals*1000.0/frame_ms:0.0,settings.animation_fps,lifetime_frames,lifetime_ms/lifetime_frames,percentile(95),percentile(99),lifetime_max,slow_frames,remastered::PhysicalSprite::builds,remastered::PhysicalSprite::build_total_ms,remastered::PhysicalSprite::build_max_ms);
        api->log("ds2-remastered-hud",message);total_ms=max_ms=0;frames=intervals=0;frame_ms=0;
    }
}
LRESULT CALLBACK procedure(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    switch (message) {
        case WM_APP+42: {std::lock_guard lock(state_mutex);return latest.max_health;}
        case WM_APP+44: {std::lock_guard lock(state_mutex);return presentation.gauge_available?static_cast<LRESULT>(presentation.health_length*1000000):0;}
        case WM_APP+45: {std::lock_guard lock(state_mutex);return presentation.gauge_available?static_cast<LRESULT>(presentation.stamina_length*1000000):0;}
        case WM_APP+43: {std::lock_guard lock(state_mutex);return static_cast<LRESULT>(latest.max_stamina*100);}
        case WM_APP+41: {std::lock_guard lock(state_mutex);return icons.effect_count;}
        case WM_COPYDATA: {
            auto* data=reinterpret_cast<COPYDATASTRUCT*>(lparam);HWND sender=reinterpret_cast<HWND>(wparam);
            DWORD pid=0;GetWindowThreadProcessId(sender,&pid);wchar_t name[64]{};GetClassNameW(sender,name,64);
            if(pid!=GetCurrentProcessId() || wcscmp(name,remastered::forge_class)!=0 ||
                !data || data->dwData!=remastered::layout_message || data->cbData!=sizeof(remastered::LayoutPreview) || !data->lpData)return FALSE;
            auto value=*static_cast<const remastered::LayoutPreview*>(data->lpData);
            if(!remastered::valid_layout(value.layout) || value.editing<0 || value.editing>1)return FALSE;
            settings.layout=value.layout;layout_editing=value.editing!=0;return TRUE;
        }
        case WM_NCHITTEST: return HTTRANSPARENT;
        case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
        case WM_CLOSE: DestroyWindow(window); return 0;
        case WM_DESTROY: revoke(); CancelWaitableTimer(frame_timer); PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window,message,wparam,lparam);
}
DWORD WINAPI run_ui(void*) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    Gdiplus::GdiplusStartupInput input{}; ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) { SetEvent(ready_event); return 1; }
    try {
        wchar_t path[32768]{};GetModuleFileNameW(self,path,32768);
        assets.load(std::filesystem::path(path).parent_path()/L"Assets");
        skins.load(std::filesystem::path(path).parent_path()/L"Skins");
        char message[96]{};std::snprintf(message,sizeof(message),"Validated %zu original local HUD icon assets",assets.size());api->log("ds2-remastered-hud",message);
        api->log("ds2-remastered-hud","Validated Hollow and Human skin atlases; native form selection enabled");
    } catch(const std::exception& e) {api->log("ds2-remastered-hud",e.what());assets.clear();skins.clear();Gdiplus::GdiplusShutdown(token);SetEvent(ready_event);return 1;}
    WNDCLASSW cls{}; cls.hInstance=self; cls.lpfnWndProc=procedure; cls.lpszClassName=remastered::window_class;
    if (!RegisterClassW(&cls)) { skins.clear();assets.clear();Gdiplus::GdiplusShutdown(token); SetEvent(ready_event); return 1; }
    overlay=CreateWindowExW(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
        remastered::window_class,L"DS2 Remastered HUD",WS_POPUP,0,0,1,1,nullptr,nullptr,self,nullptr);
    // WM_TIMER messages are coalesced by the window queue even when multimedia
    // timer precision is requested. Use a waitable timer on this owned UI thread.
    frame_timer=CreateWaitableTimerExW(nullptr,nullptr,0x00000002,TIMER_ALL_ACCESS);
    if(!frame_timer)frame_timer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_ALL_ACCESS);
    if(overlay && frame_timer){cadence(overlay,false);window_ready=timer_interval!=0;}
    SetEvent(ready_event);
    if (!window_ready) { if (overlay) DestroyWindow(overlay);if(frame_timer)CloseHandle(frame_timer);skins.clear();assets.clear();Gdiplus::GdiplusShutdown(token); return 1; }
    MSG message{};
    bool running=true;
    while(running){
        const auto wake=MsgWaitForMultipleObjectsEx(1,&frame_timer,INFINITE,QS_ALLINPUT,MWMO_INPUTAVAILABLE);
        if(wake==WAIT_FAILED){hide(overlay);DestroyWindow(overlay);break;}
        while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){
            if(message.message==WM_QUIT){running=false;break;}
            TranslateMessage(&message);DispatchMessageW(&message);
        }
        if(running && wake==WAIT_OBJECT_0)refresh(overlay);
    }
    CancelWaitableTimer(frame_timer);CloseHandle(frame_timer);frame_timer=nullptr;
    remastered::ingame::detach();
    UnregisterClassW(remastered::window_class,self);
    surface.clear();
    assets.clear();
    skins.clear();
    Gdiplus::GdiplusShutdown(token);
    return 0;
}
void on_event(uint32_t,const DS2FX_PlayerState* player) {
    if (!player || player->size!=sizeof(DS2FX_PlayerState)) return;
    if (!player->available) revoke();
    DS2FX_HudState extra{sizeof(DS2FX_HudState)};
    DS2FX_HudPresentation native{sizeof(DS2FX_HudPresentation)};
    DS2FX_HudIcons icon_state{sizeof(DS2FX_HudIcons)};
    DS2FX_PlayerForm player_form{sizeof(DS2FX_PlayerForm)};
    if (api->get_hud_state) api->get_hud_state(&extra);
    if(api->get_hud_presentation) api->get_hud_presentation(&native);
    if(api->get_hud_icons)api->get_hud_icons(&icon_state);
    if(api->get_player_form)api->get_player_form(&player_form);
    std::lock_guard lock(state_mutex);
    latest=*player; hud=extra;presentation=native;icons=icon_state;form=player_form; last_update=GetTickCount64();
}
}
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* host,DS2FX_Plugin* output) {
    if (!host || host->version!=DS2FX_API_VERSION || host->size<sizeof(DS2FX_Api) ||
        !host->get_player_state || !host->get_hud_state || !host->get_hud_presentation || !host->update_hud_replacement ||
        !host->get_hud_icons || !host->update_hud_regions || !host->update_hud_region_mask || !host->get_player_form ||
        !host->log || !output || output->size!=sizeof(DS2FX_Plugin) || initialized.exchange(true)) return 0;
    api=host;
    remastered::ingame::configure(self,api);
    try { load_settings(); } catch (...) { api->log("ds2-remastered-hud","Configuration read failed"); return 0; }
    if (settings.enabled) {
        ready_event=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if (!ready_event) return 0;
        HANDLE thread=CreateThread(nullptr,0,run_ui,nullptr,0,nullptr);
        if (!thread) { CloseHandle(ready_event); ready_event=nullptr; return 0; }
        CloseHandle(thread);
        if (WaitForSingleObject(ready_event,5000)!=WAIT_OBJECT_0) return 0;
        CloseHandle(ready_event); ready_event=nullptr;
        if (!window_ready) return 0;
    }
    *output={sizeof(DS2FX_Plugin),DS2FX_API_VERSION,"ds2-remastered-hud","0.1.28",on_event};
    api->log("ds2-remastered-hud",settings.enabled ?
        "In-game renderer initialized; hidden Forge IPC window, independent bars/covenant/effects layers; awaiting standalone DXGI or pinned ReShade68" : "HUD disabled in hud.ini");
    return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason==DLL_PROCESS_ATTACH) { self=module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
