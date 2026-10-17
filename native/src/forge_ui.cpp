#include "forge_ui.h"
#include "forge_settings.h"
#include "forge_options.h"
#include "forge_hud_presets.h"
#include "forge_cursor.h"
#include "../mods/ds2_remastered_hud/remastered_layout.h"
#include "../mods/ds2_remastered_hud/remastered_geometry.h"
#include "../mods/ds2_remastered_hud/remastered_surface.h"
#include <objidl.h>
#include <propidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <atomic>
#include <memory>
#include <cmath>

namespace fx::forge {
namespace {
using namespace Gdiplus;
constexpr wchar_t class_name[]=L"DS2FX.Forge.Menu.1";
HMODULE instance=nullptr;
fs::path root_dir;
std::vector<Mod> catalog;
void(*logger)(const char*,const char*)=nullptr;
HANDLE thread_handle=nullptr,ready=nullptr;
std::atomic<bool> stopping{false},healthy{false};
std::atomic<HWND> window{nullptr};
HWND game=nullptr;
bool opened=false,end_down=false,seamless_mode=false,settings_page=false,desired_jump=true,desired_estus=true;
float editor_native_hp=0,editor_native_sp=0;
bool editor=false,editor_snap=true,dragging=false,resizing=false,length_resize=false;
int editor_selected=1,grid_cells=100,editor_scale=100,editor_margin=24,editor_hp=1400,editor_stamina=120,editor_stats=1;
unsigned editor_effects=2;
remastered::HudLayout editor_layout,editor_saved;
remastered::Box drag_start{};POINT drag_point{};int drag_size=100,drag_length=100;
HWND hud_window=nullptr;
remastered::Surface editor_surface;
void placement();void finish_editor(bool save);
int mod_control=0;
int option_page=0;
bool preset_page=false;int preset_slot=1;
int selection=0,scroll=0,focus_control=0,scale_percent=100,opacity=238;
std::wstring notice;
ULONG_PTR gdiplus_token=0;
std::unique_ptr<Image> background;
std::unique_ptr<Bitmap> background_frame;
std::unique_ptr<PrivateFontCollection> private_fonts;
std::unique_ptr<FontFamily> title_family;
std::unique_ptr<FontFamily> body_family;
Color ivory(255,228,221,203),muted(255,151,149,139),gold(255,197,163,98),rule(150,113,99,71);
void log(const std::string& s) { if(logger) logger("forge",s.c_str()); }
std::wstring wide(const std::string& s) { return utf8_path(s).wstring(); }
void resources(const fs::path& assets) {
    body_family=std::make_unique<FontFamily>(L"Segoe UI");
    background=std::make_unique<Image>((assets/"forge-background.png").c_str());
    if(background->GetLastStatus()!=Ok) background.reset();
    private_fonts=std::make_unique<PrivateFontCollection>();
    if(private_fonts->AddFontFile((assets/"Cinzel.ttf").c_str())==Ok && private_fonts->GetFamilyCount()>0) {
        FontFamily family; INT count=0; private_fonts->GetFamilies(1,&family,&count);
        if(count) title_family.reset(family.Clone());
    }
    if(!title_family) title_family=std::make_unique<FontFamily>(L"Georgia");
}
void release_resources() { background_frame.reset();background.reset();title_family.reset();private_fonts.reset();body_family.reset(); }
bool prepare_background(int width,int height) {
    if(!background || width<=0 || height<=0 || uint64_t(width)*height>16777216)return false;
    if(background_frame && background_frame->GetWidth()==static_cast<UINT>(width) &&
        background_frame->GetHeight()==static_cast<UINT>(height))return true;
    const auto began=GetTickCount64();
    auto next=std::make_unique<Bitmap>(width,height,PixelFormat32bppPARGB);
    if(next->GetLastStatus()!=Ok)return false;
    {Graphics g(next.get());g.SetCompositingMode(CompositingModeSourceCopy);
        g.DrawImage(background.get(),Rect(0,0,width,height));g.Flush(FlushIntentionSync);
        if(g.GetLastStatus()!=Ok)return false;}
    background_frame=std::move(next);
    char message[140]{};std::snprintf(message,sizeof(message),"Background cached %dx%d build_ms=%llu; reused until display size changes",width,height,GetTickCount64()-began);log(message);
    return true;
}
void text(Graphics& g,const std::wstring& s,float x,float y,float w,float h,float size,Color c,bool serif=false,StringAlignment alignment=StringAlignmentNear) {
    Font font(serif?title_family.get():body_family.get(),size,FontStyleRegular,UnitPixel);
    SolidBrush brush(c);StringFormat format;
    format.SetAlignment(alignment);format.SetLineAlignment(StringAlignmentCenter);format.SetTrimming(StringTrimmingEllipsisWord);format.SetFormatFlags(StringFormatFlagsNoWrap);
    g.DrawString(s.c_str(),-1,&font,RectF(x,y,w,h),&format,&brush);
}
void fill(Graphics& g,float x,float y,float w,float h,Color color) { SolidBrush b(color);g.FillRectangle(&b,x,y,w,h); }
void line(Graphics& g,float x,float y,float x2,float y2,Color color,float width=1) { Pen p(color,width);g.DrawLine(&p,x,y,x2,y2); }
void diamond(Graphics& g,float x,float y,float r,Color c) { Pen p(c,1.2f);PointF points[]={{x,y-r},{x+r,y},{x,y+r},{x-r,y},{x,y-r}};g.DrawLines(&p,points,5); }
void button(Graphics& g,float x,float y,float w,float h,const std::wstring& label,bool selected=false) {
    fill(g,x,y,w,h,selected?Color(220,57,48,33):Color(180,23,24,23));
    Pen p(selected?gold:rule,1);g.DrawRectangle(&p,x,y,w,h);text(g,label,x+8,y,w-16,h,17,selected?ivory:muted);
}
void draw_editor(Graphics& g,int width,int height) {
    // Alpha 1 retains editor hit testing across transparent squares. Color-key
    // transparency would pass clicks through to the moving game underneath.
    g.Clear(Color(1,0,0,0));
    const float u=std::min(width/1600.f,height/900.f);
    const int step=std::max(8,width/grid_cells);
    Pen grid(Color(65,110,128,130),1);for(int x=step;x<width;x+=step)g.DrawLine(&grid,x,0,x,height);
    for(int y=step;y<height;y+=step)g.DrawLine(&grid,0,y,width,y);
    for(int i=0;i<4;++i){const auto b=remastered::part_box(i,width,height,editor_scale,editor_margin,editor_layout,editor_effects,editor_hp,static_cast<float>(editor_stamina),editor_stats!=0,editor_native_hp,editor_native_sp);
        Pen rim(i==editor_selected?gold:muted,i==editor_selected?2.f:1.f);g.DrawRectangle(&rim,b.x,b.y,b.w,b.h);
        const float handle=std::min(8*u,b.h/4);
        fill(g,b.x+b.w-handle,b.y+b.h-handle,handle,handle,i==editor_selected?gold:muted);
        if((i==1 || i==2) && !(editor_stats && editor_native_hp>0 && editor_native_sp>0))fill(g,b.x+b.w-5*u,b.y+b.h/2-4*u,10*u,8*u,i==editor_selected?gold:muted);}
    const auto old=g.Save();g.TranslateTransform(0,height-82*u);g.ScaleTransform(u,u);
    fill(g,16,0,1180,72,Color(255,12,16,17));
    text(g,std::wstring(L"HUD EDITOR · ")+remastered::part_names[editor_selected]+(editor_stats && editor_native_hp>0 && editor_native_sp>0 ? L" · stat lengths automatic / corner: thickness" : L" · corner: size / right edge: length"),30,0,650,28,17,ivory);
    button(g,30,32,110,30,L"Save");button(g,150,32,110,30,L"Cancel");
    button(g,270,32,170,30,editor_snap?L"Snap to grid":L"Free move",editor_snap);
    button(g,450,32,44,30,L"−");text(g,L"Grid "+std::to_wstring(grid_cells),500,32,115,30,16,gold);button(g,620,32,44,30,L"+");
    button(g,680,32,145,30,L"Reset layout");
    text(g,L"TAB select · arrows move · ESC cancel",835,30,350,33,14,muted);
    g.Restore(old);
}
void draw(Graphics& g,int width,int height) {
    if(editor){draw_editor(g,width,height);return;}
    g.SetSmoothingMode(SmoothingModeAntiAlias);g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.ScaleTransform(width/1200.f,height/720.f);
    fill(g,0,0,1200,720,Color(255,9,12,13));
    if(prepare_background(width,height)){
        const auto saved=g.Save();g.ResetTransform();
        g.DrawImage(background_frame.get(),Rect(0,0,width,height),0,0,width,height,UnitPixel);
        g.Restore(saved);
    }
    fill(g,16,16,1168,688,Color(160,7,10,11));
    fill(g,32,118,277,508,Color(150,9,12,14));
    Pen frame(Color(200,126,107,72),1);g.DrawRectangle(&frame,16.f,16.f,1167.f,687.f);
    for(auto x:{28.f,1172.f}) for(auto y:{28.f,692.f}) diamond(g,x,y,6,gold);
    text(g,L"F O R G E",48,30,500,57,38,ivory,true);
    text(g,L"DARK SOULS II  /  MOD CONFIGURATION",50,83,610,22,14,muted);
    text(g,seamless_mode?L"SEAMLESS SESSION":L"OFFLINE RUNTIME",810,45,300,27,14,gold,false,StringAlignmentFar);
    button(g,1123,40,40,34,L"×");
    line(g,40,118,1160,118,rule);line(g,310,134,310,625,rule);
    text(g,L"YOUR MODS",50,137,190,28,15,gold,true);
    text(g,std::to_wstring(catalog.size()),245,137,35,28,15,muted,false,StringAlignmentFar);
    const int visible=8;
    for(int i=scroll;i<static_cast<int>(catalog.size()) && i<scroll+visible;i++) {
        const float y=182.f+(i-scroll)*45.f; const bool selected=!settings_page && i==selection;
        if(selected) {fill(g,40,y,260,41,Color(170,62,53,35));line(g,40,y,40,y+41,gold,3);}
        text(g,catalog[i].name,56,y,226,41,19,selected?ivory:muted,true);
    }
    if(catalog.empty()) text(g,L"No DS2FX mods loaded",54,192,232,60,18,muted);
    button(g,50,570,240,40,L"Forge settings",settings_page);
    if(settings_page) {
        text(g,L"Forge settings",346,141,740,55,30,ivory,true);
        text(g,L"Presentation follows the displayed game area and DS2LE UI font scale.",346,201,764,50,18,muted);
        text(g,L"Interface scale",346,277,430,38,21,ivory);
        button(g,844,277,58,38,L"−",focus_control==0);text(g,std::to_wstring(scale_percent)+L"%",911,277,107,38,20,gold,false,StringAlignmentCenter);button(g,1028,277,58,38,L"+",focus_control==1);
        line(g,346,335,1110,335,rule);
        text(g,L"Panel opacity",346,355,430,38,21,ivory);
        button(g,844,355,58,38,L"−",focus_control==2);text(g,std::to_wstring((opacity*100+127)/255)+L"%",911,355,107,38,20,gold,false,StringAlignmentCenter);button(g,1028,355,58,38,L"+",focus_control==3);
        text(g,L"Changes are saved automatically. END or Escape closes Forge.",346,419,760,50,18,muted);
        button(g,346,501,246,40,L"Restore presentation",focus_control==4);
    } else if(!catalog.empty()) {
        const auto& mod=catalog.at(selection);
        text(g,mod.name,346,140,715,55,31,ivory,true);
        text(g,wide(mod.version)+L"  /  "+mod.status,348,197,580,26,15,gold);
        if(mod.id=="ds2-remastered-hud"){button(g,959,197,120,34,L"Editor");button(g,818,197,130,34,L"Presets",preset_page);}
        text(g,mod.description,348,243,748,62,19,ivory);
        line(g,346,328,1110,328,rule);
        if(mod.id=="ds2-remastered-hud" && preset_page){
            text(g,L"Frame themes",348,340,740,30,20,ivory,true);
            const wchar_t* names[]{L"Automatic",L"Ashen",L"Ember",L"Quiet ember"};
            for(int i=0;i<4;++i)button(g,348+i*185.f,379,174,34,names[i]);
            text(g,L"Automatic follows Hollow/Human state. Other themes lock the frame style.",348,419,748,28,15,muted);
            text(g,L"UI presets · layout, theme and animation settings",348,461,740,30,19,ivory);
            for(int i=1;i<=6;++i){const float x=348+(i-1)*123.f;
                button(g,x,498,112,34,L"Slot "+std::to_wstring(i),i==preset_slot);
                text(g,hud_preset_exists(i)?L"Saved":L"Empty",x+8,534,96,18,12,muted,false,StringAlignmentCenter); }
            button(g,348,563,232,34,L"Save current to slot");button(g,600,563,232,34,L"Load selected slot");
            text(g,L"← → select slot · S save · ENTER load · P options",348,601,748,24,15,muted);
        } else if(mod.jump_option) {
            text(g,L"Dedicated jump",348,349,460,38,22,ivory,true);
            button(g,929,349,158,38,desired_jump?L"ON   ◇":L"OFF   ◇",mod_control==0);
            text(g,L"Tap L3 without sprinting. Set Jump to L3 in game controls.",348,389,747,26,16,muted);
            if(mod.estus_option) {
                text(g,L"Walking Estus",348,425,460,38,22,ivory,true);
                button(g,929,425,158,38,desired_estus?L"ON   ◇":L"OFF   ◇",mod_control==1);
                text(g,L"Move while drinking. Healing and drink timing stay unchanged.",348,465,747,25,16,muted);
            }
            const bool pending=desired_jump!=mod.jump_active||(mod.estus_option&&desired_estus!=mod.estus_active);
            text(g,pending?L"Saved for next launch. Current session unchanged.":L"Active this session. Restart the game after changing settings.",348,492,747,25,16,pending?gold:muted);
        } else if(!options(mod.id).empty()) {
            const auto controls=options(mod.id);
            const int pages=(static_cast<int>(controls.size())+4)/5;
            option_page=std::clamp(option_page,0,pages-1);
            for(int row=0;row<5;++row) {
                const int i=option_page*5+row;if(i>=static_cast<int>(controls.size()))break;
                const auto& o=controls[i];const float y=340.f+row*40.f;
                text(g,o.label,348,y,450,34,18,ivory);
                button(g,844,y,48,32,L"−",mod_control==i);
                const int value=option_value(mod.id,o);
                std::wstring label=std::to_wstring(value);
                if(o.low==0 && o.high==1)label=value?L"ON":L"OFF";
                if(std::wstring(o.key)==L"SkinMode")label=value==0?L"Auto":value==1?L"Hollow":L"Human";
                text(g,label,899,y,118,32,17,gold,false,StringAlignmentCenter);
                button(g,1028,y,48,32,L"+",mod_control==i);
            }
            text(g,mod.id=="ds2-remastered-hud"?L"Applies live within one second. ENTER + / Shift ENTER −.":L"Saved for next launch. Restart to apply light settings.",348,545,740,24,15,muted);
            if(pages>1) {
                button(g,348,582,120,32,L"Previous");
                text(g,std::to_wstring(option_page+1)+L" / "+std::to_wstring(pages),500,582,380,32,16,gold,false,StringAlignmentCenter);
                button(g,958,582,120,32,L"Next");
            }
        } else text(g,L"This content pack has no adjustable gameplay settings.",348,355,750,80,20,muted);
        if(options(mod.id).empty()) {line(g,346,522,1110,522,rule);
        text(g,mod.credits,348,534,750,45,15,muted);
        text(g,(mod.asset_count?std::to_wstring(mod.asset_count)+L" verified files  ·  ":L"")+L"Build "+wide(mod.hash.substr(0,16)),348,584,755,24,14,gold);}
    }
    line(g,40,636,1160,636,rule);
    text(g,notice.empty()?L"END / ESC Close   ↑ ↓ Mod   ← → Control   TAB Settings   ENTER Change":notice,50,651,850,27,15,ivory);
    text(g,L"Game continues while open",900,651,250,27,14,muted,false,StringAlignmentFar);
}
BOOL CALLBACK enumerate(HWND candidate,LPARAM param) {
    DWORD pid=0;GetWindowThreadProcessId(candidate,&pid);
    if(pid!=GetCurrentProcessId() || candidate==window || !IsWindowVisible(candidate) || GetWindow(candidate,GW_OWNER) || (GetWindowLongPtrW(candidate,GWL_EXSTYLE)&WS_EX_TOOLWINDOW)) return TRUE;
    RECT r{};if(GetClientRect(candidate,&r) && r.right>=320 && r.bottom>=200){*reinterpret_cast<HWND*>(param)=candidate;return FALSE;}return TRUE;
}
RECT display_bounds(HWND target) {
    RECT physical{};if(remastered::game_client_bounds(target,physical))return physical;
    RECT r{}; GetClientRect(target,&r);POINT o{};ClientToScreen(target,&o);OffsetRect(&r,o.x,o.y);
    // DS2LE can leave the logical client at the internal render resolution. A
    // borderless game anchored to its monitor presents over that entire monitor.
    const auto style=GetWindowLongPtrW(target,GWL_STYLE);
    MONITORINFO mi{sizeof(mi)};GetMonitorInfoW(MonitorFromWindow(target,MONITOR_DEFAULTTONEAREST),&mi);
    if(!(style&WS_CAPTION) && std::abs(r.left-mi.rcMonitor.left)<=2 && std::abs(r.top-mi.rcMonitor.top)<=2) r=mi.rcMonitor;
    return r;
}
float le_scale() {
    static uint64_t checked=0;static float cached=1;const auto now=GetTickCount64();if(now-checked<1000)return cached;checked=now;
    wchar_t value[64]{};const auto file=root_dir.parent_path()/"ds2le_user_settings.ini";
    GetPrivateProfileStringW(L"UserEditorSettings",L"mod_ui_default_font_size",L"12",value,64,file.c_str());
    wchar_t* end=nullptr;float v=wcstof(value,&end);
    cached=end && !*end && std::isfinite(v) && v>=8 && v<=48 ? v/12.f:1.f;return cached;
}
void placement() {
    if(!game) return;
    const auto bounds=display_bounds(game);
    const auto p=editor?Layout{static_cast<int>(bounds.left),static_cast<int>(bounds.top),static_cast<int>(bounds.right-bounds.left),static_cast<int>(bounds.bottom-bounds.top),1.f}:layout(bounds,scale_percent,le_scale());
    if(GetWindow(window,GW_OWNER)!=game) SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));
    static RECT previous{};static ULONGLONG promoted=0;
    const RECT next{p.x,p.y,p.x+p.width,p.y+p.height};
    const bool moved=!EqualRect(&previous,&next),hidden=!IsWindowVisible(window);
    // Reassert the order without demoting the layered window first. Demotion
    // forces a full repaint every second and can stall the HUD artwork worker.
    if(moved || hidden || GetTickCount64()-promoted>=1000) {
        UINT flags=SWP_NOACTIVATE|SWP_NOOWNERZORDER;
        if(!moved && !hidden) flags|=SWP_NOMOVE|SWP_NOSIZE|SWP_NOREDRAW;
        SetWindowPos(window,HWND_TOPMOST,p.x,p.y,p.width,p.height,flags|SWP_SHOWWINDOW);
        previous=next;promoted=GetTickCount64();
    }
    static int previous_opacity=-1;
    const int desired=editor?-2:opacity;
    if(previous_opacity!=desired){
        const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);
        SetWindowLongPtrW(window,GWL_EXSTYLE,style&~WS_EX_LAYERED);SetWindowLongPtrW(window,GWL_EXSTYLE,style|WS_EX_LAYERED);
        if(!editor)SetLayeredWindowAttributes(window,0,static_cast<BYTE>(opacity),LWA_ALPHA);
        previous_opacity=desired;InvalidateRect(window,nullptr,FALSE);
    }
}
void close_menu(bool restore) {
    if(!opened) return;if(editor)finish_editor(false);opened=false;ShowWindow(window,SW_HIDE);
    if(restore && game && IsWindow(game) && !IsIconic(game)) SetForegroundWindow(game);
    log("Closed");
}
void toggle_jump() {
    if(catalog.empty() || !catalog[selection].jump_option) return;
    if(save_setting(L"modern-souls-2",L"DedicatedJump",!desired_jump)) {desired_jump=!desired_jump;notice=L"Saved. Restart the game to apply the jump setting.";log(desired_jump?"Jump ON saved for next launch":"Jump OFF saved for next launch");}
    else notice=L"Could not save settings. Your current setting is unchanged.";
}
void toggle_estus() {
    if(catalog.empty() || !catalog[selection].estus_option) return;
    if(save_setting(L"modern-souls-2",L"WalkingEstus",!desired_estus)) {desired_estus=!desired_estus;notice=L"Saved. Restart the game to apply Walking Estus.";log(desired_estus?"Walking Estus ON saved for next launch":"Walking Estus OFF saved for next launch");}
    else notice=L"Could not save settings. Your current setting is unchanged.";
}
void presentation(int action) {
    int next_scale=scale_percent,next_opacity=opacity;
    if(action==0) next_scale=std::max(70,scale_percent-5);
    if(action==1) next_scale=std::min(125,scale_percent+5);
    if(action==2) next_opacity=std::max(204,opacity-8);
    if(action==3) next_opacity=std::min(255,opacity+8);
    if(action==4) {next_scale=100;next_opacity=238;}
    if(save_setting(L"Forge",L"ScalePercent",next_scale) && save_setting(L"Forge",L"Opacity",next_opacity)) {scale_percent=next_scale;opacity=next_opacity;notice=L"Presentation saved.";placement();}
    else notice=L"Could not save presentation settings.";
}
void change_option(int index,int direction) {
    if(catalog.empty())return;const auto& mod=catalog[selection];const auto controls=options(mod.id);
    if(index<0 || index>=static_cast<int>(controls.size()))return;
    const auto& o=controls[index];const int old=option_value(mod.id,o);
    int next=std::clamp(old+direction*o.step,o.low,o.high);
    if(o.high<=2 && old==next)next=direction>0?o.low:o.high;
    if(save_setting(wide(mod.id).c_str(),o.key,next))notice=o.live?L"Saved. Applying live.":L"Saved. Restart to apply.";
    else notice=L"Could not save setting.";
}
BOOL CALLBACK find_hud(HWND candidate,LPARAM output) {
    DWORD pid=0;GetWindowThreadProcessId(candidate,&pid);wchar_t name[64]{};GetClassNameW(candidate,name,64);
    if(pid==GetCurrentProcessId() && wcscmp(name,L"DS2FX.RemasteredHUD.1")==0){*reinterpret_cast<HWND*>(output)=candidate;return FALSE;}return TRUE;
}
bool send_layout(bool editing) {
    if(!hud_window || !IsWindow(hud_window)){hud_window=nullptr;EnumWindows(find_hud,reinterpret_cast<LPARAM>(&hud_window));}
    if(!hud_window)return false;
    remastered::LayoutPreview preview{editor_layout,editing?1:0};
    COPYDATASTRUCT data{remastered::layout_message,sizeof(preview),&preview};DWORD_PTR result=0;
    return SendMessageTimeoutW(hud_window,WM_COPYDATA,reinterpret_cast<WPARAM>(window.load()),reinterpret_cast<LPARAM>(&data),SMTO_ABORTIFHUNG,100,&result) && result;
}
void begin_editor() {
    const auto file=settings_file();
    for(int i=0;i<4;++i){const auto section=std::wstring(L"HUD.Layout.")+std::to_wstring(i);
        editor_layout.items[i]={setting(file,section.c_str(),L"X",-1,-1,10000),setting(file,section.c_str(),L"Y",-1,-1,10000),setting(file,section.c_str(),L"Size",i==3?50:100,25,400),setting(file,section.c_str(),L"Length",100,25,300)};}
    editor_saved=editor_layout;
    editor_scale=setting(file,L"ds2-remastered-hud",L"ScalePercent",100,50,200);editor_margin=setting(file,L"ds2-remastered-hud",L"Margin",24,4,128);
    editor_snap=setting(file,L"HUD.Editor",L"Snap",1,0,1)!=0;grid_cells=setting(file,L"HUD.Editor",L"GridVersion",0,0,2)==2?setting(file,L"HUD.Editor",L"Grid",100,20,200):100;
    editor_stats=setting(file,L"ds2-remastered-hud",L"StatBarLengths",1,0,1);
    if(!send_layout(true)){notice=L"HUD editor unavailable: renderer is not responding.";return;}
    DWORD_PTR count=0;if(SendMessageTimeoutW(hud_window,WM_APP+41,0,0,SMTO_ABORTIFHUNG,100,&count))editor_effects=std::clamp(static_cast<unsigned>(count),1u,20u);
    DWORD_PTR value=0;if(SendMessageTimeoutW(hud_window,WM_APP+42,0,0,SMTO_ABORTIFHUNG,100,&value)&&value>0)editor_hp=static_cast<int>(value);
    if(SendMessageTimeoutW(hud_window,WM_APP+43,0,0,SMTO_ABORTIFHUNG,100,&value)&&value>0)editor_stamina=static_cast<int>(value/100);
    if(SendMessageTimeoutW(hud_window,WM_APP+44,0,0,SMTO_ABORTIFHUNG,100,&value))editor_native_hp=value/1000000.f;
    if(SendMessageTimeoutW(hud_window,WM_APP+45,0,0,SMTO_ABORTIFHUNG,100,&value))editor_native_sp=value/1000000.f;
    editor=true;cursor::editor_window=window.load();ClipCursor(nullptr);placement();InvalidateRect(window,nullptr,FALSE);log("HUD editor opened; normalized coordinates, aspect-preserving handles");
}
bool persist_layout(const remastered::HudLayout& value) {
    try {
        const auto file=settings_file();fs::create_directories(file.parent_path());
        const auto stage=file.parent_path()/(L"layout-"+std::to_wstring(GetCurrentProcessId())+L".tmp");no_links(stage);
        if(fs::exists(file))fs::copy_file(file,stage,fs::copy_options::overwrite_existing);
        bool saved=true;for(int i=0;i<4;++i){const auto section=std::wstring(L"HUD.Layout.")+std::to_wstring(i);
            auto write=[&](const wchar_t* key,int value){const auto text=std::to_wstring(value);return WritePrivateProfileStringW(section.c_str(),key,text.c_str(),stage.c_str())!=FALSE;};
            saved=write(L"X",value.items[i].x)&&saved;saved=write(L"Y",value.items[i].y)&&saved;saved=write(L"Size",value.items[i].size)&&saved;saved=write(L"Length",value.items[i].length)&&saved;}
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,stage.c_str());
        if(saved)saved=MoveFileExW(stage.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
        if(!saved)DeleteFileW(stage.c_str());return saved;
    }catch(...){return false;}
}
void finish_editor(bool save) {
    char cursor_report[180]{};std::snprintf(cursor_report,sizeof(cursor_report),"Editor cursor guard slots=%zu warps=%llu clips=%llu passed=%llu",cursor::slots.size(),cursor::warps.load(),cursor::clips.load(),cursor::passed.load());log(cursor_report);
    cursor::editor_window=nullptr;ReleaseCapture();dragging=false;
    if(save && !persist_layout(editor_layout)){persist_layout(editor_saved);notice=L"Could not save layout; previous layout restored.";editor_layout=editor_saved;}
    else if(save){save_setting(L"HUD.Editor",L"Snap",editor_snap?1:0);save_setting(L"HUD.Editor",L"Grid",grid_cells);save_setting(L"HUD.Editor",L"GridVersion",2);notice=L"HUD layout saved.";}
    else {editor_layout=editor_saved;notice=L"HUD layout cancelled.";}
    send_layout(false);editor=false;placement();InvalidateRect(window,nullptr,FALSE);
}
void editor_mouse(int x,int y,bool begin) {
    RECT area{};GetClientRect(window,&area);const int width=area.right,height=area.bottom;
    if(width<=0 || height<=0)return;
    const float u=std::min(width/1600.f,height/900.f);const int step=std::max(8,width/grid_cells);
    if(begin){
        const float bx=x/u,by=(y-height+82*u)/u;
        if(by>=32 && by<=62){
            if(bx>=30&&bx<=140){finish_editor(true);return;}if(bx>=150&&bx<=260){finish_editor(false);return;}
            if(bx>=270&&bx<=440){editor_snap=!editor_snap;InvalidateRect(window,nullptr,FALSE);return;}
            if(bx>=450&&bx<=494)grid_cells=std::max(20,grid_cells-10);
            if(bx>=620&&bx<=664)grid_cells=std::min(200,grid_cells+10);
            if(bx>=680&&bx<=825){editor_layout={};send_layout(true);}
            InvalidateRect(window,nullptr,FALSE);return;
        }
        for(int j=0;j<4;++j){const int i=(editor_selected+4-j)%4;const auto b=remastered::part_box(i,width,height,editor_scale,editor_margin,editor_layout,editor_effects,editor_hp,static_cast<float>(editor_stamina),editor_stats!=0,editor_native_hp,editor_native_sp);
            if(x>=b.x-6*u&&x<=b.x+b.w+6*u&&y>=b.y-6*u&&y<=b.y+b.h+6*u){editor_selected=i;drag_start=b;drag_point={x,y};drag_size=editor_layout.items[(i==1 || i==2)?1:i].size;drag_length=editor_layout.items[i].length;
                dragging=true;resizing=x>=b.x+b.w-12*u&&y>=b.y+b.h-std::min(6*u,b.h/4);length_resize=!resizing&&!(editor_stats && editor_native_hp>0 && editor_native_sp>0)&&(i==1 || i==2)&&x>=b.x+b.w-12*u;SetCapture(window);break;}}
    } else if(dragging){
        auto& item=editor_layout.items[editor_selected];
        const bool snap=editor_snap && !(GetKeyState(VK_SHIFT)&0x8000);
        if(resizing || length_resize){const float aspect=drag_start.h/drag_start.w;
            float w=std::max(4.f,drag_start.w+((x-drag_point.x)+(y-drag_point.y)*aspect)/(1+aspect*aspect));if(snap)w=std::max(4.f,remastered::magnetic(w,step,std::min(step*.3f,8*u)));
            if(length_resize)item.length=std::clamp(static_cast<int>(std::lround(drag_length*w/drag_start.w)),25,300);
            else {const bool automatic=editor_stats && editor_native_hp>0 && editor_native_sp>0 && (editor_selected==1 || editor_selected==2);
                const float ratio=automatic?std::max(4.f,drag_start.h+y-drag_point.y)/drag_start.h:w/drag_start.w;
                item.size=std::clamp(static_cast<int>(std::lround(drag_size*ratio)),25,400);
                if(editor_selected==1 || editor_selected==2)editor_layout.items[1].size=editor_layout.items[2].size=item.size;}
            item.x=static_cast<int>(std::lround(drag_start.x/width*10000));item.y=static_cast<int>(std::lround(drag_start.y/height*10000));
        } else remastered::move_part(item,drag_start,static_cast<float>(x-drag_point.x),static_cast<float>(y-drag_point.y),width,height,snap,step);
        send_layout(true);
    }
    InvalidateRect(window,nullptr,FALSE);
}
void click(int px,int py) {
    if(editor){editor_mouse(px,py,true);return;}
    RECT r{};GetClientRect(window,&r);if(r.right<=0 || r.bottom<=0)return;
    const float x=px*1200.f/r.right,y=py*720.f/r.bottom;
    if(!settings_page && !catalog.empty() && catalog[selection].id=="ds2-remastered-hud" && x>=959 && x<=1079 && y>=197 && y<=231){begin_editor();return;}
    if(!settings_page && !catalog.empty() && catalog[selection].id=="ds2-remastered-hud" && x>=818 && x<=948 && y>=197 && y<=231){preset_page=!preset_page;notice.clear();InvalidateRect(window,nullptr,FALSE);return;}
    if(x>1123 && y>40 && y<74) {close_menu(true);return;}
    if(x>50 && x<290 && y>570 && y<610){settings_page=true;focus_control=0;notice.clear();}
    else if(x>40 && x<300 && y>=182 && y<542){int i=scroll+static_cast<int>((y-182)/45);if(i<static_cast<int>(catalog.size())){selection=i;settings_page=false;option_page=mod_control=0;notice.clear();}}
    else if(settings_page) {
        if(y>=277 && y<=315){if(x>=844 && x<=902)presentation(0);if(x>=1028 && x<=1086)presentation(1);}
        if(y>=355 && y<=393){if(x>=844 && x<=902)presentation(2);if(x>=1028 && x<=1086)presentation(3);}
        if(y>=501 && y<=541 && x>=346 && x<=592)presentation(4);
    } else if(!catalog.empty() && catalog[selection].id=="ds2-remastered-hud" && preset_page){
        if(y>=379 && y<=413 && x>=348 && x<1088){const int theme=static_cast<int>((x-348)/185);notice=hud_theme(theme)?L"Theme applied. Layout preserved.":L"Could not apply theme.";}
        if(y>=498 && y<=532 && x>=348 && x<1086)preset_slot=std::clamp(1+static_cast<int>((x-348)/123),1,6);
        if(y>=563 && y<=597){if(x>=348 && x<=580)notice=hud_preset_save(preset_slot)?L"Current UI saved to slot "+std::to_wstring(preset_slot)+L".":L"Could not save preset.";
            if(x>=600 && x<=832)notice=hud_preset_load(preset_slot)?L"Preset loaded. Applies live within one second.":L"Preset empty or invalid; current UI preserved.";}
    } else if(!catalog.empty() && !options(catalog[selection].id).empty()) {
        const auto controls=options(catalog[selection].id);const int pages=(static_cast<int>(controls.size())+4)/5;
        if(y>=340 && y<540) {const int i=option_page*5+static_cast<int>((y-340)/40);mod_control=i;
            if(x>=844&&x<=892)change_option(i,-1);if(x>=1028&&x<=1076)change_option(i,1);}
        if(y>=582&&y<=614){if(x>=348&&x<=468)option_page=std::max(0,option_page-1);if(x>=958&&x<=1078)option_page=std::min(pages-1,option_page+1);mod_control=option_page*5;}
    } else if(x>=929 && x<=1087 && y>=349 && y<=387) {mod_control=0;toggle_jump();}
    else if(x>=929 && x<=1087 && y>=425 && y<=463) {mod_control=1;toggle_estus();}
    InvalidateRect(window,nullptr,FALSE);
}
void tick() {
    if(stopping){DestroyWindow(window);return;}
    if(!game || !IsWindow(game)){game=nullptr;EnumWindows(enumerate,reinterpret_cast<LPARAM>(&game));}
    const auto fg=GetForegroundWindow();const bool relevant=game && (fg==game || fg==window) && !IsIconic(game);
    const bool down=(GetAsyncKeyState(VK_END)&0x8000)!=0;
    if(down && !end_down && relevant) {
        if(opened)close_menu(true);
        else {opened=true;notice.clear();placement();ShowWindow(window,SW_SHOW);SetForegroundWindow(window);SetFocus(window);log("Opened: output-relative bounds, END edge, keyboard and mouse enabled");}
    }
    end_down=down;
    if(opened && !relevant)close_menu(false);
    if(opened){placement();}
}
LRESULT CALLBACK procedure(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
    switch(msg) {
    case WM_TIMER: tick();return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:if(dragging){dragging=false;ReleaseCapture();}InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_LBUTTONDOWN:click(GET_X_LPARAM(lp),GET_Y_LPARAM(lp));return 0;
    case WM_MOUSEMOVE:if(editor && dragging)editor_mouse(GET_X_LPARAM(lp),GET_Y_LPARAM(lp),false);return 0;
    case WM_LBUTTONUP:if(editor){dragging=false;ReleaseCapture();}return 0;
    case WM_CAPTURECHANGED:dragging=false;return 0;
    case WM_MOUSEWHEEL:
        if(editor)return 0;
        if(!settings_page && !catalog.empty()){selection=std::clamp(selection-(GET_WHEEL_DELTA_WPARAM(wp)>0?1:-1),0,static_cast<int>(catalog.size())-1);scroll=std::clamp(selection-7,0,std::max(0,static_cast<int>(catalog.size())-8));option_page=mod_control=0;InvalidateRect(w,nullptr,FALSE);}return 0;
    case WM_KEYDOWN:
        if(lp&(1LL<<30)) return 0;
        if(editor){
            if(wp==VK_ESCAPE){finish_editor(false);return 0;}
            if(wp==VK_RETURN){finish_editor(true);return 0;}
            if(wp==VK_TAB)editor_selected=(editor_selected+1)%4;
            if(wp==VK_LEFT || wp==VK_RIGHT || wp==VK_UP || wp==VK_DOWN){RECT r{};GetClientRect(w,&r);
                auto b=remastered::part_box(editor_selected,r.right,r.bottom,editor_scale,editor_margin,editor_layout,editor_effects,editor_hp,static_cast<float>(editor_stamina),editor_stats!=0,editor_native_hp,editor_native_sp);
                const int step=editor_snap?std::max(8,static_cast<int>(r.right)/grid_cells):1;
                remastered::move_part(editor_layout.items[editor_selected],b,wp==VK_LEFT?-step:wp==VK_RIGHT?step:0,wp==VK_UP?-step:wp==VK_DOWN?step:0,r.right,r.bottom,editor_snap,step);send_layout(true);}
            InvalidateRect(w,nullptr,FALSE);return 0;
        }
        if(wp==VK_ESCAPE){close_menu(true);return 0;}
        if(wp=='E' && !settings_page && !catalog.empty() && catalog[selection].id=="ds2-remastered-hud"){begin_editor();return 0;}
        if(!settings_page && !catalog.empty() && catalog[selection].id=="ds2-remastered-hud"){
            if(wp=='P'){preset_page=!preset_page;notice.clear();InvalidateRect(w,nullptr,FALSE);return 0;}
            if(preset_page && (wp==VK_LEFT || wp==VK_RIGHT || wp=='S' || wp==VK_RETURN)){
                if(wp==VK_LEFT || wp==VK_RIGHT)preset_slot=(preset_slot-1+(wp==VK_RIGHT?1:5))%6+1;
                if(wp=='S')notice=hud_preset_save(preset_slot)?L"Current UI saved to selected slot.":L"Could not save preset.";
                if(wp==VK_RETURN)notice=hud_preset_load(preset_slot)?L"Preset loaded. Applies live within one second.":L"Preset empty or invalid; current UI preserved.";
                InvalidateRect(w,nullptr,FALSE);return 0;}
        }
        if(wp==VK_TAB){settings_page=!settings_page;focus_control=0;notice.clear();}
        else if(wp==VK_UP || wp==VK_DOWN) {
            int d=wp==VK_UP?-1:1;
            if(settings_page)focus_control=(focus_control+d+5)%5;
            else if(!catalog.empty()){selection=std::clamp(selection+d,0,static_cast<int>(catalog.size())-1);scroll=std::clamp(selection-7,0,std::max(0,static_cast<int>(catalog.size())-8));option_page=mod_control=0;}
        } else if(wp==VK_RETURN || wp==VK_SPACE) {if(settings_page)presentation(focus_control);else if(!catalog.empty()&&!options(catalog[selection].id).empty())change_option(mod_control,(GetKeyState(VK_SHIFT)&0x8000)?-1:1);else if(mod_control==1)toggle_estus();else toggle_jump();}
        else if(settings_page && (wp==VK_LEFT || wp==VK_RIGHT)){int action=focus_control<2?0:2;presentation(action+(wp==VK_RIGHT?1:0));}
        else if(!settings_page && (wp==VK_LEFT || wp==VK_RIGHT)){if(!catalog.empty()&&!options(catalog[selection].id).empty()) {
            const int count=static_cast<int>(options(catalog[selection].id).size());mod_control=(mod_control+(wp==VK_RIGHT?1:count-1))%count;option_page=mod_control/5;
        } else mod_control=!catalog.empty()&&catalog[selection].estus_option?1-mod_control:0;}
        InvalidateRect(w,nullptr,FALSE);return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps{};auto dc=BeginPaint(w,&ps);RECT r{};GetClientRect(w,&r);
        if(editor){
            if(editor_surface.resize(r.right,r.bottom)){
                draw_editor(editor_surface.context(),r.right,r.bottom);
                RECT bounds{};GetWindowRect(w,&bounds);editor_surface.present(w,bounds,255);
            }
            EndPaint(w,&ps);return 0;
        }
        auto memory=CreateCompatibleDC(dc);auto bmp=CreateCompatibleBitmap(dc,std::max(1L,r.right),std::max(1L,r.bottom));auto old=SelectObject(memory,bmp);
        {Graphics g(memory);draw(g,r.right,r.bottom);}BitBlt(dc,0,0,r.right,r.bottom,memory,0,0,SRCCOPY);
        SelectObject(memory,old);DeleteObject(bmp);DeleteDC(memory);EndPaint(w,&ps);return 0;
    }
    case WM_CLOSE:DestroyWindow(w);return 0;
    case WM_DESTROY:if(editor)finish_editor(false);KillTimer(w,1);PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(w,msg,wp,lp);
}
DWORD WINAPI run(void*) {
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GdiplusStartupInput startup;
    if(GdiplusStartup(&gdiplus_token,&startup,nullptr)!=Ok){SetEvent(ready);return 1;}
    try {
        resources(root_dir/"Forge"/"assets");
        // Decode/scale the static artwork once at startup, before a character
        // can enter gameplay. Reopening and controls retain this physical-size
        // cache instead of stalling GDI+ (and the HUD worker) on each repaint.
        EnumWindows(enumerate,reinterpret_cast<LPARAM>(&game));
        if(game){const auto p=layout(display_bounds(game),scale_percent,le_scale());prepare_background(p.width,p.height);}
        WNDCLASSW cls{};cls.hInstance=instance;cls.lpfnWndProc=procedure;cls.lpszClassName=class_name;cls.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        require(RegisterClassW(&cls)!=0,"Forge window registration failed");
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,class_name,L"Forge - DS2FX",WS_POPUP,0,0,1200,720,nullptr,nullptr,instance,nullptr);
        require(window && SetTimer(window,1,40,nullptr),"Forge window creation failed");
        log(cursor::install()?"Authenticated game/graphics cursor imports guarded only during focused HUD editing":"Cursor guard unavailable for this process; original input retained");
        healthy=true;SetEvent(ready);log("Ready; press END. No renderer or gameplay input hooks.");
        MSG message{};while(GetMessageW(&message,nullptr,0,0)>0){TranslateMessage(&message);DispatchMessageW(&message);}
        window=nullptr;UnregisterClassW(class_name,instance);
    } catch(const std::exception& e){log(e.what());SetEvent(ready);}
    cursor::uninstall();editor_surface.clear();release_resources();GdiplusShutdown(gdiplus_token);return 0;
}
}
bool start(HMODULE module,const fs::path& root,std::vector<Mod> mods,bool seamless,void(*report)(const char*,const char*)) {
    if(thread_handle) return false;
    instance=module;root_dir=root;catalog=std::move(mods);seamless_mode=seamless;logger=report;
    desired_jump=jump_enabled();desired_estus=estus_enabled();mod_control=0;const auto cfg=settings_file();
    scale_percent=setting(cfg,L"Forge",L"ScalePercent",100,70,125);opacity=setting(cfg,L"Forge",L"Opacity",238,204,255);
    ready=CreateEventW(nullptr,TRUE,FALSE,nullptr);if(!ready)return false;
    thread_handle=CreateThread(nullptr,0,run,nullptr,0,nullptr);if(!thread_handle){CloseHandle(ready);ready=nullptr;return false;}
    const bool signaled=WaitForSingleObject(ready,5000)==WAIT_OBJECT_0;
    if(signaled){CloseHandle(ready);ready=nullptr;}
    return signaled && healthy;
}
bool shutdown() {
    stopping=true;if(window)PostMessageW(window,WM_CLOSE,0,0);
    if(thread_handle){if(WaitForSingleObject(thread_handle,5000)!=WAIT_OBJECT_0)return false;CloseHandle(thread_handle);thread_handle=nullptr;}
    if(ready){CloseHandle(ready);ready=nullptr;}return true;
}
bool preview(const fs::path& assets,const fs::path& output,int width,int height,const std::string& mod_id,int page) {
    GdiplusStartupInput startup;if(GdiplusStartup(&gdiplus_token,&startup,nullptr)!=Ok)return false;
    resources(assets);seamless_mode=true;
    editor=mod_id=="hud-editor";preset_page=mod_id=="ds2-remastered-hud" && page==-1;
    catalog={{"modern-souls-2","0.2.0","6f9194cc1e6c439c",L"Modern Souls 2",L"Movement and combat, refined. Dedicated jump and Walking Estus, configured through Forge.",L"FROST FX additions · Combat base: Balanced DS3 Combat / Vanilla ADP by Fentiman.",L"Active / shared content",379,true,true,true,true}};
    if(!mod_id.empty()) {Mod entry;entry.id=mod_id;entry.name=wide(mod_id);entry.version="preview";entry.status=L"Configuration";
        entry.description=L"Adjust presentation and performance. Changes are saved automatically.";
        catalog={entry};selection=0;option_page=page;}
    Status status=GenericError;
    {Bitmap bitmap(width,height,PixelFormat32bppARGB);{Graphics g(&bitmap);draw(g,width,height);}
     CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};status=bitmap.Save(output.c_str(),&png,nullptr);}
    editor=false;release_resources();GdiplusShutdown(gdiplus_token);return status==Ok;
}
bool title_texture(const fs::path& assets,const fs::path& output) {
    GdiplusStartupInput startup;if(GdiplusStartup(&gdiplus_token,&startup,nullptr)!=Ok)return false;
    resources(assets);Status result=GenericError;
    {Bitmap bitmap(4096,512,PixelFormat32bppARGB);Graphics g(&bitmap);
     g.Clear(Color(255,0,0,0));g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
     auto letters=[&](const std::wstring& s,float y,float size,float spacing,Color color) {
        Font font(title_family.get(),size,FontStyleRegular,UnitPixel);StringFormat fmt(StringFormat::GenericTypographic());
        float total=0;std::vector<float> widths;
        for(auto ch:s){RectF b;g.MeasureString(&ch,1,&font,PointF(0,0),&fmt,&b);widths.push_back(b.Width);total+=b.Width+spacing;}total-=spacing;
        float x=(4096-total)/2;SolidBrush brush(color);
        for(size_t i=0;i<s.size();i++){g.DrawString(&s[i],1,&font,PointF(x,y),&fmt,&brush);x+=widths[i]+spacing;}
     };
     letters(L"REMASTERED",0,210,31,Color(255,229,218,191));
     letters(L"Scholar of the First Sin",273,161,21,Color(255,221,211,187));
     CLSID png{0x557cf406,0x1a04,0x11d3,{0x9a,0x73,0x00,0x00,0xf8,0x1e,0xf3,0x2e}};result=bitmap.Save(output.c_str(),&png,nullptr);}
    release_resources();GdiplusShutdown(gdiplus_token);return result==Ok;
}
}
