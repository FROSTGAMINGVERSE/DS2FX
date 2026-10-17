#include "../mods/wayfarer_lantern/lantern_view.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace lantern;
void check(bool condition,const char* message) {
    if (!condition) throw std::runtime_error(message);
}
CLSID png_encoder() {
    UINT count=0,bytes=0;
    check(Gdiplus::GetImageEncodersSize(&count,&bytes)==Gdiplus::Ok && bytes,"PNG encoder size");
    std::vector<BYTE> buffer(bytes);
    auto codecs=reinterpret_cast<Gdiplus::ImageCodecInfo*>(buffer.data());
    check(Gdiplus::GetImageEncoders(count,bytes,codecs)==Gdiplus::Ok,"Image encoders");
    for (UINT i=0;i<count;++i) if (wcscmp(codecs[i].MimeType,L"image/png")==0) return codecs[i].Clsid;
    throw std::runtime_error("PNG encoder unavailable");
}
Snapshot base() {
    Snapshot s;
    s.available=s.alive=s.equipped=s.context_known=s.context_clear=true;
    s.menu_open=false; s.character_epoch=1; s.revision=42;
    s.capacity_seconds=s.oil_seconds=1800; s.lit=true;
    return s;
}
struct Case { const wchar_t* name; Snapshot snapshot; double hold; };
std::array<Case,12> cases() {
    auto full=base(),lit=base(),low=base(),off=base(),empty=base(),refill=base(),unavailable=base(),hold=base();
    lit.oil_seconds=1117; low.oil_seconds=180; off.oil_seconds=1234; off.lit=false;
    empty.oil_seconds=0; empty.lit=false; refill.oil_seconds=540; refill.refilling=true;
    unavailable.available=false;
    auto charged=base(),charged_off=base(),charged_last=base(),charged_empty=base();
    for(auto* s:{&charged,&charged_off,&charged_last,&charged_empty}){s->charge_mode=true;s->capacity_seconds=3;s->oil_seconds=2;s->context_known=false;}
    charged_off.lit=false;charged_last.oil_seconds=0;charged_empty.oil_seconds=0;charged_empty.lit=false;
    return {{{L"full",full,0},{L"lit",lit,0},{L"low-oil",low,0},{L"off",off,0},
             {L"empty",empty,0},{L"refilling",refill,0},{L"unavailable",unavailable,0},{L"hold",hold,0.63},
             {L"charge-lit",charged,0},{L"charge-off",charged_off,0},{L"charge-last-lit",charged_last,0},{L"charge-empty",charged_empty,0}}};
}
void validate_placement() {
    view::Settings settings;
    const RECT client{100,200,1700,1100};
    const std::array<view::Anchor,4> anchors{view::Anchor::top_left,view::Anchor::top_right,
        view::Anchor::bottom_left,view::Anchor::bottom_right};
    for (const auto anchor:anchors) {
        settings.anchor=anchor;
        auto r=view::placement(client,settings,96);
        check(r.right-r.left==480 && r.bottom-r.top==132,"100% layout dimensions");
        const bool right=anchor==view::Anchor::top_right || anchor==view::Anchor::bottom_right;
        const bool bottom=anchor==view::Anchor::bottom_left || anchor==view::Anchor::bottom_right;
        check(right ? r.right==client.right-24 : r.left==client.left+24,"Corner x anchor");
        check(bottom ? r.bottom==client.bottom-24 : r.top==client.top+24,"Corner y anchor");
        r=view::placement(client,settings,192);
        check(r.right-r.left==960 && r.bottom-r.top==264,"200% DPI dimensions");
        for (const RECT client_bounds:std::array<RECT,3>{{{10,20,210,80},{0,0,1,1},{0,0,0,0}}}) {
            r=view::placement(client_bounds,settings,192);
            check(r.left>=client_bounds.left && r.top>=client_bounds.top && r.right<=client_bounds.right && r.bottom<=client_bounds.bottom,"Small client clamp");
        }
        settings.margin_x=settings.margin_y=4096;
        r=view::placement(client,settings,192);
        check(r.left>=client.left && r.top>=client.top && r.right<=client.right && r.bottom<=client.bottom,"Margin clamp");
        settings.margin_x=settings.margin_y=24;
    }
}
void validate_values() {
    auto s=base();
    check(view::timer_text(s)==L"30:00","Full timer");
    s.oil_seconds=0.1; check(view::timer_text(s)==L"00:01","Fractional seconds");
    s.oil_seconds=0; check(view::timer_text(s)==L"00:00","Empty timer");
    s.oil_seconds=s.capacity_seconds=3661; check(view::timer_text(s)==L"1:01:01","Hour timer");
    s.oil_seconds=s.capacity_seconds=1e100; check(view::timer_text(s)==L"99h+","Huge timer bound");
    s.oil_seconds=std::numeric_limits<double>::quiet_NaN();
    check(view::timer_text(s)==L"--:--" && view::fraction(s)==0,"Invalid timer");
    s=base();
    check(view::visible(s,true,true,false,1500),"Fresh display");
    check(!view::visible(s,true,true,false,1501),"Stale display");
    check(!view::visible(s,true,false,false,0),"Focus loss");
    check(!view::visible(s,true,true,true,0),"Minimized display");
    s.alive=false; check(!view::visible(s,true,true,false,0),"Death hides display");
}
void validate_pixels(Gdiplus::Bitmap& image) {
    Gdiplus::Color color;
    check(image.GetPixel(0,0,&color)==Gdiplus::Ok && color.GetA()==0,"Transparent canvas corner");
    check(image.GetPixel(image.GetWidth()/2,image.GetHeight()/2,&color)==Gdiplus::Ok && color.GetA()>150,"Visible panel interior");
}
void validate_resource_drawing(const view::Assets& assets) {
    auto full=base(),empty=base(),low=base();
    empty.oil_seconds=0; empty.lit=false; low.oil_seconds=180;
    Gdiplus::Bitmap full_image(480,132,PixelFormat32bppPARGB),empty_image(480,132,PixelFormat32bppPARGB),low_image(480,132,PixelFormat32bppPARGB);
    check(view::draw(full_image,assets,full,0) && view::draw(empty_image,assets,empty,0) &&
        view::draw(low_image,assets,low,0),"Resource comparison renders");
    Gdiplus::Color full_end,empty_end,low_start,low_end;
    full_image.GetPixel(420,75,&full_end); empty_image.GetPixel(420,75,&empty_end);
    low_image.GetPixel(140,75,&low_start); low_image.GetPixel(200,75,&low_end);
    check(full_end.GetR()>empty_end.GetR()+80,"Full oil bar reaches end while empty does not");
    check(low_start.GetR()>low_end.GetR()+80,"Low oil fills only the supplied fraction");
    auto unknown=full; unknown.available=false;
    check(view::draw(empty_image,assets,unknown,std::numeric_limits<double>::quiet_NaN()),"Unknown state and invalid hold render safely");
}
void validate_overlay(Gdiplus::Bitmap& image) {
    const auto instance=GetModuleHandleW(nullptr);
    HWND owner=CreateWindowExW(0,L"STATIC",L"DS2FX developer fixture owner",WS_POPUP,
        -10000,-10000,1000,700,nullptr,nullptr,instance,nullptr);
    check(owner!=nullptr,"Hidden owner creation");
    struct Owner { HWND window; ~Owner(){DestroyWindow(window);} } owner_raii{owner};
    const auto foreground=GetForegroundWindow();
    {
        view::Overlay overlay;
        check(overlay.create(owner,instance),"Overlay creation");
        const auto window=overlay.handle();
        const auto style=GetWindowLongPtrW(window,GWL_EXSTYLE);
        const auto expected=WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW;
        check((style&expected)==expected,"Layered clickthrough/nonactivation styles");
        check(GetWindow(window,GW_OWNER)==owner,"Game window ownership");
        check(SendMessageW(window,WM_NCHITTEST,0,0)==HTTRANSPARENT,"Transparent hit testing");
        check(SendMessageW(window,WM_MOUSEACTIVATE,0,0)==MA_NOACTIVATE,"Mouse never activates");
        const RECT bounds{-10000,-10000,-10000+static_cast<LONG>(image.GetWidth()),-10000+static_cast<LONG>(image.GetHeight())};
        const auto error=overlay.present(image,bounds,240,false);
        if (error) std::cerr<<"UpdateLayeredWindow error: "<<error<<"\n";
        check(error==ERROR_SUCCESS,"Actual per-pixel-alpha layered upload");
        check(!IsWindowVisible(window),"Fixture remains hidden");
        check(GetForegroundWindow()==foreground,"Foreground unchanged");
        check(overlay.present(image,RECT{0,0,1,1},255,false)==ERROR_INVALID_PARAMETER,"Mismatched upload rejected");
        overlay.hide();
    }
    check(GetForegroundWindow()==foreground,"Teardown leaves foreground unchanged");
}
}
int wmain(int argc,wchar_t** argv) {
    if (argc!=3) { std::wcerr<<L"Usage: ds2fx_lantern_visual <assets-directory> <preview-output-directory>\n"; return 2; }
    Gdiplus::GdiplusStartupInput input; ULONG_PTR token=0;
    if (Gdiplus::GdiplusStartup(&token,&input,nullptr)!=Gdiplus::Ok) return 2;
    int result=0;
    {
        try {
            std::cout<<"DEVELOPER FIXTURE ONLY: synthetic snapshots, no game process or inventory access.\n";
            view::Assets assets; check(assets.load(argv[1]),"Production PNG load");
            validate_values(); validate_placement(); validate_resource_drawing(assets);
            const auto encoder=png_encoder();
            const std::filesystem::path output(argv[2]); std::filesystem::create_directories(output);
            const auto examples=cases();
            Gdiplus::Bitmap contact(1016,1044,PixelFormat32bppPARGB);
            Gdiplus::Graphics sheet(&contact);
            sheet.Clear(Gdiplus::Color(255,24,27,31));
            sheet.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit);
            view::text(sheet,L"DS2FX  /  WAYFARER'S LANTERN",Gdiplus::RectF(28,17,960,25),19,Gdiplus::Color(255,232,221,199),false,true);
            view::text(sheet,L"Developer rendering fixture · synthetic values · no game integration proof",Gdiplus::RectF(28,45,960,22),12,Gdiplus::Color(255,174,180,185));
            for (size_t i=0;i<examples.size();++i) {
                for (const int scale:std::array<int,2>{1,2}) {
                    Gdiplus::Bitmap rendered(view::logical_width*scale,view::logical_height*scale,PixelFormat32bppPARGB);
                    check(view::draw(rendered,assets,examples[i].snapshot,examples[i].hold),"HUD render");
                    validate_pixels(rendered);
                    const auto path=output/(std::wstring(examples[i].name)+L"-"+std::to_wstring(scale*100)+L".png");
                    check(rendered.Save(path.c_str(),&encoder,nullptr)==Gdiplus::Ok,"PNG output");
                    if (scale==1) {
                        const int x=18+static_cast<int>(i%2)*500, y=89+static_cast<int>(i/2)*157;
                        view::text(sheet,examples[i].name,Gdiplus::RectF(static_cast<float>(x+12),static_cast<float>(y-13),460,18),10,Gdiplus::Color(255,126,137,146));
                        sheet.DrawImage(&rendered,x,y);
                        if (i==0) validate_overlay(rendered);
                    }
                }
            }
            sheet.Flush(Gdiplus::FlushIntentionSync);
            check(contact.Save((output/L"developer-contact-sheet.png").c_str(),&encoder,nullptr)==Gdiplus::Ok,"Contact sheet output");
            std::cout<<"PASS: 16 alpha-preserving PNG states at 100/200%; timer, bounds, all anchors, stale/focus/death guards, hidden owned layered upload, clickthrough and nonactivation.\n";
            std::wcout<<L"Preview output: "<<output.wstring()<<L"\n";
        } catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<"\n"; result=1; }
    }
    Gdiplus::GdiplusShutdown(token);
    return result;
}
