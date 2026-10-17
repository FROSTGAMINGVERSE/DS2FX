#include "../mods/ds2_remastered_hud/remastered_view.h"
#include "../mods/ds2_remastered_hud/remastered_geometry.h"
#include "../mods/ds2_remastered_hud/remastered_surface.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <atomic>

namespace {
std::atomic<uint32_t> revoke_calls{0};
uint32_t update_replacement(uint64_t,uint32_t visible) {
    if(!visible) ++revoke_calls;
    return visible ? 0u : 1u;
}
void check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
void log(const char*,const char* message) { std::cout << message << '\n'; }
uint32_t query(DS2FX_PlayerState*) { return 0; }
uint32_t query_icons(DS2FX_HudIcons*) {return 0;}
uint32_t query_form(DS2FX_PlayerForm*) {return 0;}
uint32_t update_regions(uint64_t epoch,uint64_t,uint32_t regions) {return update_replacement(epoch,regions);}
uint32_t query_presentation(DS2FX_HudPresentation* output) {
    if(!output || output->size!=sizeof(*output)) return 0;
    *output={sizeof(*output),1,1,1,255,0};return 1;
}
uint32_t query_hud(DS2FX_HudState* output) {
    if(!output || output->size!=sizeof(DS2FX_HudState)) return 0;
    DS2FX_HudState state{sizeof(DS2FX_HudState),1,1,1,DS2FX_HUD_SOULS|DS2FX_HUD_COVENANT};
    state.souls=3200; state.covenant_id=2; *output=state; return 1;
}
void save_png(Gdiplus::Bitmap& image,const std::filesystem::path& path) {
    UINT bytes=0,count=0;
    check(Gdiplus::GetImageEncodersSize(&count,&bytes)==Gdiplus::Ok && bytes>0,"No image encoders");
    std::vector<BYTE> buffer(bytes);
    auto* codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    check(Gdiplus::GetImageEncoders(count,bytes,codecs)==Gdiplus::Ok,"Cannot inspect encoders");
    for(UINT i=0;i<count;++i) if(std::wstring(codecs[i].MimeType)==L"image/png") {
        check(image.Save(path.c_str(),&codecs[i].Clsid,nullptr)==Gdiplus::Ok,"PNG save failed"); return;
    }
    throw std::runtime_error("PNG encoder missing");
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        check(argc==2 || argc==3,"Expected HUD DLL and optional original asset folder");
        remastered::DrainTrail trail;
        check(trail.update(1,1000,260,0.25f)==1,"Trail initial value");
        check(trail.update(0.6f,1100,260,0.25f)==1,"Damage must retain lost segment");
        check(trail.update(0.6f,1360,260,0.25f)==1,"Damage hold interval");
        const auto draining=trail.update(0.6f,1460,260,0.25f);
        check(draining>0.6f && draining<1,"Damage must drain gradually");
        check(trail.update(1,1500,260,0.25f)==1,"Healing must update immediately");
        trail.reset();check(trail.update(0.2f,1550,260,0.25f)==0.2f,"Hidden/character reset must remove old trail");
        remastered::BarAnimation animated;
        animated.update(1,100,100,1,1,1000);
        animated.update(1,100,100,0.5f,0.5f,1100);
        animated.update(1,100,100,0.5f,0.5f,1320);
        check(animated.health.value==1 && animated.stamina.value<1,"Stamina must drain sooner than health");
        animated.update(2,100,100,0.4f,0.3f,1400);
        check(animated.health.value==0.4f && animated.stamina.value==0.3f,"Trails must not leak between characters");
        check(remastered::Heartbeat::rate(.1f)>2*remastered::Heartbeat::rate(1) &&
              remastered::Heartbeat::strength(.1f)>4*remastered::Heartbeat::strength(1),"Low health must accelerate and strengthen heartbeat");
        check(remastered::Heartbeat::wave(.08f)>.99f && remastered::Heartbeat::wave(.23f)>.54f && remastered::Heartbeat::wave(.6f)<.001f,"Heartbeat must have two beats and a quiet interval");
        remastered::Heartbeat heart;heart.update(1,1000);heart.update(1,1100);const float before=heart.phase;
        heart.update(.1f,1110);check(heart.phase>before && heart.phase-before<.03f,"Changing health must not jump heartbeat phase");
        heart.update(.1f,4000);check(heart.phase==0,"Long hidden interval must reset heartbeat");
        check(remastered::fraction(5,10)==0.5f,"Fraction mismatch");
        check(remastered::fraction(-5,10)==0.0f,"Negative fraction must clamp");
        check(remastered::fraction(5,0)==0.0f,"Zero maximum must reject");
        for(const auto scale:{0.5f,1.0f,2.4f,4.8f}) {
            const float edge=24*scale;
            const float bottom=remastered::quickmenu_anchor_y(2160,edge,scale)+
                (remastered::slot_step+remastered::slot_radius+remastered::slot_halo)*scale;
            check(bottom<=2160-edge+0.01f,"Quickmenu frame and halo must retain bottom margin");
        }
        DS2FX_PlayerState player{sizeof(DS2FX_PlayerState),1,1,700,1000};
        player.character_epoch=1; player.stamina_available=1; player.stamina=60; player.max_stamina=100;
        remastered::FormSelector selector;
        DS2FX_PlayerForm form{sizeof(form),1,1,1,0,1};
        check(selector.select(player,form,100)==remastered::Skin::Human,"Human selector failed");
        form.available=0;
        check(selector.select(player,form,200)==remastered::Skin::Human,"Transient form loss must not change skin");
        check(selector.select(player,form,1700)==remastered::Skin::Hollow,"Stale form must default Hollow");
        form={sizeof(form),1,1,1,1,0};
        check(selector.select(player,form,1800)==remastered::Skin::Hollow,"Hollow selector failed");
        form.hollow_level=0;form.human=1;selector.select(player,form,1900);
        player.character_epoch=2;
        check(selector.select(player,form,2000)==remastered::Skin::Hollow,"Human skin leaked across characters");
        player.character_epoch=1;
        check(remastered::visible(player,100,true,false),"Fresh player must display");
        check(!remastered::visible(player,1501,true,false),"Stale player must hide");
        check(remastered::visible(player,100,false,false),"Visible unfocused game must retain its in-game HUD");
        check(!remastered::visible(player,100,true,true),"Minimized game must hide");
        DS2FX_HudState scene{sizeof(DS2FX_HudState)};
        check(!remastered::gameplay_scene(scene),"Unknown scene must hide HUD");
        scene.available_fields=DS2FX_HUD_SCENE;scene.game_state=31;scene.loading=1;
        check(!remastered::gameplay_scene(scene),"Loading scene must hide HUD");
        DS2FX_HudPresentation owned_native{sizeof(owned_native)};owned_native.available=1;owned_native.sample=player.sample;owned_native.character_epoch=player.character_epoch;owned_native.opacity=255;
        scene.sample=player.sample;scene.character_epoch=player.character_epoch;
        check(remastered::replacement_owned(player,scene,owned_native,100,true),"Loading scene must retain authenticated replacement ownership");
        const auto living_hp=player.health;player.health=0;
        check(remastered::replacement_owned(player,scene,owned_native,100,true),"Death must retain authenticated replacement ownership");
        check(!remastered::replacement_owned(player,scene,owned_native,1501,true),"Stale blank renderer must not suppress native HUD");
        check(!remastered::replacement_owned(player,scene,owned_native,100,false),"Disabled HUD must restore native drawing");
        ++owned_native.character_epoch;check(!remastered::replacement_owned(player,scene,owned_native,100,true),"Blank renderer accepted foreign native UI");
        --owned_native.character_epoch;player.health=living_hp;
        scene.game_state=30;scene.loading=0;
        check(remastered::gameplay_scene(scene),"Playable scene must allow HUD");
        check(!remastered::scene_settled(2199,1000) && remastered::scene_settled(2200,1000),
              "HUD must wait for the post-loading settle interval");
        const RECT logical_window{0,0,2560,1440},physical_frame{0,0,3840,2160};
        const RECT logical_client{0,0,2560,1440};
        const auto corrected=remastered::physical_client_bounds(logical_window,physical_frame,logical_client,{0,0});
        check(corrected.left==0 && corrected.top==0 && corrected.right==3840 && corrected.bottom==2160,
              "150 percent DPI mapping must cover the physical game client");
        const RECT moved_window{100,80,1700,980},moved_frame{300,200,1900,1100};
        const auto moved=remastered::physical_client_bounds(moved_window,moved_frame,{0,0,1500,800},{108,112});
        check(moved.left==308 && moved.top==232 && moved.right==1808 && moved.bottom==1032,
              "Windowed client must retain its relative origin");
        DS2FX_HudState hud{sizeof(DS2FX_HudState),1,1,1,DS2FX_HUD_SOULS|DS2FX_HUD_COVENANT};
        for(const auto dimensions:{SIZE{1280,720},SIZE{1920,1080},SIZE{3840,2160},SIZE{3440,1440},SIZE{900,1600}}){
            remastered::HudLayout layout;layout.items[1]={5000,4000,125};
            const auto r=remastered::part_box(1,dimensions.cx,dimensions.cy,100,24,layout);
            check(std::abs(r.x/dimensions.cx-.5f)<.001f && std::abs(r.y/dimensions.cy-.4f)<.001f,"Normalized layout anchor changed across resolutions");
            check(std::abs(r.w/r.h-360.f/16.f)<.001f,"Resize must preserve slim reference aspect ratio");
            layout.items[1]={10000,10000,400};const auto clamped=remastered::part_box(1,dimensions.cx,dimensions.cy,200,128,layout);
            check(clamped.x>=0 && clamped.y>=0 && clamped.x+clamped.w<=dimensions.cx && clamped.y+clamped.h<=dimensions.cy,"Editor asset must remain on screen");
        }
        check(remastered::magnetic(96,100,8)==100 && remastered::magnetic(85,100,8)==85,"Magnetic grid must capture nearby lines only");
        remastered::HudLayout stats;
        const auto short_bar=remastered::part_box(1,3840,2160,100,24,stats,2,1000,100,true,.06f,.05f);
        const auto large=remastered::part_box(1,3840,2160,100,24,stats,2,2000,100,true,.09f,.05f);
        check(large.w>short_bar.w && large.h==short_bar.h,"Stats must change length without stretching thickness");
        const auto stamina_equal=remastered::part_box(2,3840,2160,100,24,stats,2,1000,946,true,.06f,.06f);
        check(std::abs(stamina_equal.w-short_bar.w)<.01f,"Native equal spans must ignore original atlas length differences");
        stats.items[1].size=50;stats.items[1].length=250;
        const auto thinner=remastered::part_box(1,3840,2160,100,24,stats,2,1000,946,true,.06f,.05f);
        check(std::abs(thinner.w-short_bar.w)<.01f && std::abs(thinner.h-short_bar.h*.5f)<.01f,"Automatic native length must survive artwork thickness changes");
        // Captured live 0.1.17 values previously both saturated to 800px.
        // Cover the actual range, not only tiny synthetic fixture fractions.
        for(const auto resolution:{SIZE{1280,720},SIZE{1920,1080},SIZE{3840,2160},SIZE{3440,1440}}){
            remastered::HudLayout actual;actual.items[1].size=102;actual.items[2].size=93;
            const auto hp_bar=remastered::part_box(1,resolution.cx,resolution.cy,100,24,actual,2,1771,946,true,.405148f,.239194f);
            const auto sp_bar=remastered::part_box(2,resolution.cx,resolution.cy,100,24,actual,2,1771,946,true,.405148f,.239194f);
            const float unit=std::min(resolution.cx/1600.f,resolution.cy/900.f);
            check(std::abs(hp_bar.w-324.1184f*unit)<.01f && std::abs(sp_bar.w-191.3552f*unit)<.01f,"Live native spans lost compact screen conversion");
            check(std::abs(hp_bar.w/sp_bar.w-.405148f/.239194f)<.0001f,"Native HP/SP length ratio saturated or changed with resolution");
            check(std::abs(hp_bar.h-sp_bar.h)<.001f && hp_bar.h<20*unit,"Health and stamina must share a slim thickness despite old saved scales");
            const auto increased=remastered::part_box(1,resolution.cx,resolution.cy,100,24,actual,2,2000,946,true,.45f,.239194f);
            check(increased.w>hp_bar.w && increased.h==hp_bar.h,"Stat increase must visibly extend a normal live gauge");
        }
        stats.items[1].size=100;
        stats.items[1].length=150;
        const auto manual=remastered::part_box(1,3840,2160,100,24,remastered::HudLayout{},2,1000,100,false);
        const auto longer=remastered::part_box(1,3840,2160,100,24,stats,2,1000,100,false);
        check(std::abs(longer.w/manual.w-1.5f)<.001f && longer.h==manual.h,"Independent manual length resize failed");
        hud.souls=3200; hud.covenant_id=2;
        Gdiplus::GdiplusStartupInput startup{}; ULONG_PTR token=0;
        check(Gdiplus::GdiplusStartup(&token,&startup,nullptr)==Gdiplus::Ok,"GDI+ startup failed");
        struct GdiLifetime {ULONG_PTR token;~GdiLifetime(){Gdiplus::GdiplusShutdown(token);}} lifetime{token};
        {
            const auto foreground=GetForegroundWindow();
            HWND blank_window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,L"STATIC",L"Owned blank HUD surface fixture",WS_POPUP,0,0,1,1,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
            check(blank_window!=nullptr,"Blank fixture window creation failed");
            remastered::Surface blank_surface;check(blank_surface.resize(1,1),"Blank surface allocation failed");
            blank_surface.context().Clear(Gdiplus::Color(0,0,0,0));
            const RECT blank_bounds{0,0,1,1};
            check(blank_surface.present(blank_window,blank_bounds,0,false)==ERROR_SUCCESS,"Hidden blank surface update failed");
            check(!IsWindowVisible(blank_window) && GetForegroundWindow()==foreground,"Blank lease refresh showed a window or took focus");
            DestroyWindow(blank_window);
        }

        {
            Gdiplus::Bitmap gradient(120,24,PixelFormat32bppPARGB),sweep_a(120,24,PixelFormat32bppPARGB),sweep_b(120,24,PixelFormat32bppPARGB);
            Gdiplus::Graphics gg(&gradient),ga(&sweep_a),gb(&sweep_b);
            remastered::bar(gg,0,0,100,16,1,Gdiplus::Color(255,60,8,14),Gdiplus::Color(255,180,38,46),Gdiplus::Color(255,237,77,79),0);
            gg.Flush(Gdiplus::FlushIntentionSync);Gdiplus::Color left{},right{};gradient.GetPixel(10,8,&left);gradient.GetPixel(85,8,&right);
            check(right.GetR()>left.GetR()+50,"Fill must visibly brighten from left to right");
            remastered::bar(ga,0,0,100,16,.65f,Gdiplus::Color(255,29,58,23),Gdiplus::Color(255,113,145,59),Gdiplus::Color(255,143,166,93),1,true,.25f);
            remastered::bar(gb,0,0,100,16,.65f,Gdiplus::Color(255,29,58,23),Gdiplus::Color(255,113,145,59),Gdiplus::Color(255,143,166,93),1,true,.65f);
            ga.Flush(Gdiplus::FlushIntentionSync);gb.Flush(Gdiplus::FlushIntentionSync);
            sweep_a.GetPixel(10,8,&left);sweep_b.GetPixel(10,8,&right);check(left.GetG()>right.GetG()+10,"Stamina energy sweep must travel across the fill");
            sweep_a.GetPixel(85,8,&left);sweep_b.GetPixel(85,8,&right);check(left.GetValue()==right.GetValue(),"Energy sweep must not light unavailable stamina");
        }
        remastered::Skins skins;skins.load(std::filesystem::path(argv[1]).parent_path()/L"Skins");
        check(skins.ready(),"Both grimdark atlases must validate");
        Gdiplus::Bitmap image(1600,900,PixelFormat32bppPARGB);
        Gdiplus::Graphics graphics(&image);
        remastered::Settings full;full.show_quickslots=1;full.show_souls=1;full.stat_lengths=0;
        remastered::draw(graphics,1600,900,player,hud,full,500);
        graphics.Flush(Gdiplus::FlushIntentionSync);
        Gdiplus::Color pixel{};
        check(image.GetPixel(800,450,&pixel)==Gdiplus::Ok && pixel.GetA()==0,"Center must remain transparent");
        check(image.GetPixel(150,41,&pixel)==Gdiplus::Ok && pixel.GetR()>pixel.GetG(),"Health fill must be red");
        check(image.GetPixel(150,61,&pixel)==Gdiplus::Ok && pixel.GetG()>pixel.GetR(),"Stamina fill must be green");
        check(image.GetPixel(50,50,&pixel)==Gdiplus::Ok && pixel.GetA()==0,
              "Native covenant area must remain transparent until original artwork is mapped");
        for(const BYTE opacity:{BYTE(0),BYTE(127)}) {
            Gdiplus::Bitmap menu(1600,900,PixelFormat32bppPARGB);
            Gdiplus::Graphics menu_g(&menu);
            remastered::draw(menu_g,1600,900,player,hud,full,500,opacity);
            menu_g.Flush(Gdiplus::FlushIntentionSync);
            Gdiplus::Color original{},faded{};
            image.GetPixel(150,41,&original);menu.GetPixel(150,41,&faded);
            check(opacity==0 ? faded.GetA()==0 : faded.GetA()>0 && faded.GetA()<original.GetA(),
                  "Gameplay regions must follow native health opacity");
            menu.GetPixel(78,718,&faded);
            check(opacity!=0 || faded.GetA()==0,"Quickmenu must hide in menus");
            image.GetPixel(1370,840,&original);menu.GetPixel(1370,840,&faded);
            check(original.GetA()>0 && original.GetValue()==faded.GetValue(),
                  "Souls plate must retain opacity when gameplay HUD fades or hides");
        }
        {
            Gdiplus::Bitmap scoped(1600,900,PixelFormat32bppPARGB);
            Gdiplus::Graphics scoped_g(&scoped);
            remastered::draw(scoped_g,1600,900,player,hud,{},500);
            scoped_g.Flush(Gdiplus::FlushIntentionSync);
            scoped.GetPixel(150,41,&pixel);
            check(pixel.GetA()>0,"Health-only scope must retain health bar");
            scoped.GetPixel(1370,840,&pixel);
            check(pixel.GetA()==0,"Custom souls must be disabled by default");
            scoped.GetPixel(78,718,&pixel);
            check(pixel.GetA()==0,"Custom quickmenu must be disabled by default");
        }
        auto folder=std::filesystem::path(argv[1]).parent_path()/L"remastered-hud-preview";
        std::filesystem::create_directories(folder);
        save_png(image,folder/L"preview.png");
        if(argc==3) {
            remastered::Assets originals;originals.load(argv[2]);check(originals.size()==39,"Original local icons did not validate");
            DS2FX_HudIcons icons{sizeof(icons),6,1,1};icons.covenant={16,8,0xffffffff,255};
            icons.effect_count=2;icons.effect_opacity=255;icons.effects[0]={15,40,0xffffffff,255};icons.effects[1]={15,10,0xffffffff,255};
            check(remastered::available_icon_regions(icons,originals)==6,"Mapped original assets rejected");
            {
                remastered::Settings custom;custom.stat_lengths=0;custom.layout.items[0]={2000,3000,120};custom.layout.items[1]={2400,3100,115};custom.layout.items[2]={2600,3700,90};custom.layout.items[3]={3000,4200,80};
                const auto bounds=remastered::layout_bounds(1600,900,custom.scale,custom.margin,custom.layout,2);
                Gdiplus::Bitmap full(1600,900,PixelFormat32bppPARGB);Gdiplus::Graphics fg(&full);
                remastered::draw(fg,1600,900,player,hud,custom,500,255,&icons,&originals,remastered::Skin::Human,&skins);fg.Flush(Gdiplus::FlushIntentionSync);
                remastered::Surface cropped;check(cropped.resize(bounds.right-bounds.left,bounds.bottom-bounds.top),"Moved layout crop allocation failed");
                custom.offset_x=bounds.left;custom.offset_y=bounds.top;
                remastered::draw(cropped.context(),1600,900,player,hud,custom,500,255,&icons,&originals,remastered::Skin::Human,&skins,nullptr,bounds.right-bounds.left,bounds.bottom-bounds.top);cropped.context().Flush(Gdiplus::FlushIntentionSync);
                for(int y=0;y<bounds.bottom-bounds.top;y+=3)for(int x=0;x<bounds.right-bounds.left;x+=3){Gdiplus::Color a,b;full.GetPixel(x+bounds.left,y+bounds.top,&a);cropped.bitmap().GetPixel(x,y,&b);check(std::abs(int(a.GetA())-int(b.GetA()))<=2 && std::abs(int(a.GetR())-int(b.GetR()))<=2 && std::abs(int(a.GetG())-int(b.GetG()))<=2 && std::abs(int(a.GetB())-int(b.GetB()))<=2,"Moved crop exceeded GDI subpixel quantization tolerance");}
                save_png(full,folder/L"editor-layout.png");
            }
            Gdiplus::Bitmap framed(1600,900,PixelFormat32bppPARGB);Gdiplus::Graphics framed_g(&framed);
            remastered::draw(framed_g,1600,900,player,hud,{},500,255,&icons,&originals,remastered::Skin::Hollow,&skins);framed_g.Flush(Gdiplus::FlushIntentionSync);
            save_png(framed,folder/L"original-icons-frame.png");
            {
                remastered::Settings config;
                const auto bounds=remastered::render_bounds(3840,2160,config,2);
                check(bounds.cx*bounds.cy<3840*2160/10,"Default HUD surface must stay below 10 percent of 4K pixels");
                remastered::Surface cropped;check(cropped.resize(bounds.cx,bounds.cy),"Persistent DIB allocation failed");
                for(const BYTE opacity:{BYTE(255),BYTE(128)}) {
                    Gdiplus::Bitmap full(3840,2160,PixelFormat32bppPARGB);Gdiplus::Graphics fg(&full);
                    remastered::draw(fg,3840,2160,player,hud,config,500,opacity,&icons,&originals,remastered::Skin::Human,&skins);
                    remastered::draw(cropped.context(),3840,2160,player,hud,config,500,opacity,&icons,&originals,remastered::Skin::Human,&skins,nullptr,bounds.cx,bounds.cy);
                    fg.Flush(Gdiplus::FlushIntentionSync);cropped.context().Flush(Gdiplus::FlushIntentionSync);
                    for(int y=0;y<bounds.cy;y+=3)for(int x=0;x<bounds.cx;x+=3) {
                        Gdiplus::Color a,b;full.GetPixel(x,y,&a);cropped.bitmap().GetPixel(x,y,&b);
                        check(a.GetValue()==b.GetValue(),"Cropped persistent surface differs from full-screen renderer");
                    }
                }
                LARGE_INTEGER begin,end,hz;QueryPerformanceFrequency(&hz);QueryPerformanceCounter(&begin);
                for(unsigned i=0;i<120;++i)remastered::draw(cropped.context(),3840,2160,player,hud,config,500+i*16,255,&icons,&originals,remastered::Skin::Human,&skins,nullptr,bounds.cx,bounds.cy);
                cropped.context().Flush(Gdiplus::FlushIntentionSync);QueryPerformanceCounter(&end);
                std::cout<<"4K cropped renderer "<<bounds.cx<<'x'<<bounds.cy<<" CPU ms/frame="<<1000.0*(end.QuadPart-begin.QuadPart)/hz.QuadPart/120<<" (presentation excluded)\n";
            }
            // A readable, cropped design preview, rendered by the actual HUD
            // code over bright and dark backgrounds at the same pixel scale.
            Gdiplus::Bitmap board(1120,560,PixelFormat32bppPARGB);Gdiplus::Graphics board_g(&board);
            board_g.Clear(Gdiplus::Color(255,15,18,21));
            Gdiplus::SolidBrush light(Gdiplus::Color(255,195,183,157));
            board_g.FillRectangle(&light,0,280,1120,280);
            board_g.DrawImage(&framed,Gdiplus::Rect(0,0,1120,300),0,0,560,150,Gdiplus::UnitPixel);
            board_g.DrawImage(&framed,Gdiplus::Rect(0,280,1120,300),0,0,560,150,Gdiplus::UnitPixel);
            board_g.Flush(Gdiplus::FlushIntentionSync);save_png(board,folder/L"frame-design-preview.png");
            Gdiplus::Bitmap human(1600,900,PixelFormat32bppPARGB);Gdiplus::Graphics human_g(&human);
            remastered::draw(human_g,1600,900,player,hud,{},500,255,&icons,&originals,remastered::Skin::Human,&skins);
            human_g.Flush(Gdiplus::FlushIntentionSync);save_png(human,folder/L"human-frame.png");
            {
                remastered::Settings measured;measured.native_hp=.405148f;measured.native_sp=.239194f;
                measured.layout.items[1].size=102;measured.layout.items[2].size=93;
                Gdiplus::Bitmap compact(1280,300,PixelFormat32bppPARGB);Gdiplus::Graphics cg(&compact);
                remastered::draw(cg,3840,2160,player,hud,measured,500,255,&icons,&originals,remastered::Skin::Human,&skins);
                cg.Flush(Gdiplus::FlushIntentionSync);save_png(compact,folder/L"human-native-span.png");
            }
            remastered::BarAnimation loss;
            loss.update(1,1000,100,1,1,1000);
            const auto hp=remastered::fraction(static_cast<float>(player.health),static_cast<float>(player.max_health));
            const auto sp=remastered::fraction(player.stamina,player.max_stamina);
            loss.update(1,1000,100,hp,sp,1100);
            check(loss.stamina_flash>0.5f,"Consumption should highlight grey trail immediately");
            const float immediate_flash=loss.stamina_flash;
            auto faded=loss;faded.update(1,1000,100,hp,sp,1200);
            check(faded.stamina_flash>0 && faded.stamina_flash<immediate_flash,"Consumption highlight must fade");
            remastered::BarAnimation independent;independent.update(1,1000,100,1,1,1000);independent.update(1,1000,100,.5f,1,1100);
            check(independent.stamina_flash==0,"Health loss must not flash stamina");
            independent.update(1,1000,100,.5f,.85f,1200);check(independent.stamina_flash>0,"Repeated costs retrigger feedback");
            independent.reset();check(independent.stamina_flash==0,"Pause/reset clears feedback");
            for(const auto time:{1100ull,1400ull,1700ull,2100ull}) {
                loss.update(1,1000,100,hp,sp,time);
                remastered::draw(human_g,1600,900,player,hud,{},time,255,&icons,&originals,remastered::Skin::Human,&skins,&loss);
                human_g.Flush(Gdiplus::FlushIntentionSync);
                save_png(human,folder/(L"drain-"+std::to_wstring(time)+L".png"));
            }
            Gdiplus::Bitmap pair(1000,700,PixelFormat32bppPARGB);Gdiplus::Graphics pair_g(&pair);
            pair_g.Clear(Gdiplus::Color(255,15,18,21));
            pair_g.DrawImage(&framed,Gdiplus::Rect(0,0,1000,340),0,0,500,170,Gdiplus::UnitPixel);
            pair_g.DrawImage(&human,Gdiplus::Rect(0,350,1000,340),0,0,500,170,Gdiplus::UnitPixel);
            pair_g.Flush(Gdiplus::FlushIntentionSync);save_png(pair,folder/L"hollow-human-preview.png");
            const auto content=originals.content(15,10);
            check(content.Width<64 && content.Height<64,"Transparent effect padding must be removed");
            // No old-width rail, and no frame/skin switch can put pixels in the scene center.
            framed.GetPixel(470,45,&pixel);check(!pixel.GetA(),"Old wide bar pixels remain");
            human.GetPixel(800,450,&pixel);check(!pixel.GetA(),"Human skin obscures game center");
            icons.effects[1].id=9999;check(remastered::available_icon_regions(icons,originals)==2,"Missing status asset authorized suppression");
            icons.covenant.id=0;check(remastered::available_icon_regions(icons,originals)==2,"Empty covenant socket rejected");
        }
        {
            remastered::Settings still;still.pulse=0;
            Gdiplus::Bitmap first(1600,900,PixelFormat32bppPARGB),second(1600,900,PixelFormat32bppPARGB);
            Gdiplus::Graphics first_g(&first),second_g(&second);
            remastered::draw(first_g,1600,900,player,hud,still,100);
            remastered::draw(second_g,1600,900,player,hud,still,2100);
            first_g.Flush(Gdiplus::FlushIntentionSync);second_g.Flush(Gdiplus::FlushIntentionSync);
            for(int y=0;y<110;++y) for(int x=0;x<550;++x) {
                Gdiplus::Color a{},b{};first.GetPixel(x,y,&a);second.GetPixel(x,y,&b);
                check(a.GetValue()==b.GetValue(),"Pulse=0 must stop decorative bar animation");
            }
        }
        HMODULE dll=LoadLibraryW(argv[1]); check(dll!=nullptr,"HUD DLL failed to load");
        auto init=reinterpret_cast<DS2FX_InitializePluginFn>(GetProcAddress(dll,"DS2FX_InitializePlugin"));
        check(init!=nullptr,"HUD export missing");
        DS2FX_Api api{sizeof(DS2FX_Api),DS2FX_API_VERSION,"0.3.0-alpha.1",log,query,nullptr,query_hud,query_presentation,update_replacement,query_icons,update_regions,update_regions,query_form};
        DS2FX_Plugin plugin{sizeof(DS2FX_Plugin)};
        check(init(&api,&plugin)==1,"HUD initialization failed");
        check(std::string(plugin.id)=="ds2-remastered-hud" && plugin.on_event,"Plugin identity invalid");
        HWND window=FindWindowW(remastered::window_class,L"DS2 Remastered HUD");
        check(window!=nullptr,"Hidden Forge IPC window missing");
        const auto styles=GetWindowLongPtrW(window,GWL_EXSTYLE);
        check((styles&(WS_EX_LAYERED|WS_EX_TOPMOST))==0,"HUD must not use a layered or topmost desktop window");
        check((styles&(WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW))==
              (WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW),"IPC window must pass input through");
        check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT,"Hit test must pass through");
        plugin.on_event(DS2FX_PLAYER_AVAILABLE,&player);
        Sleep(200);
        check(!IsWindowVisible(window),"No game window must hide overlay");
        check(revoke_calls.load()>0,"Hidden overlay must revoke native suppression");
        SendMessageW(window,WM_CLOSE,0,0);
        check(!IsWindow(window),"Overlay teardown failed");
        std::cout << "PASS: fields, player gating, transparent rendering, colors, plugin ABI and click-through overlay\n";
        return 0;
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
