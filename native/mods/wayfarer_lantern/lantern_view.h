#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include "lantern_state.h"

// Display-only, reusable native renderer. The caller owns GDI+ startup, snapshot
// freshness, focus/minimize visibility, and the canonical game resource state.
// No engine API, independent oil clock, or input observation exists here.
namespace lantern::view {
inline constexpr int logical_width = 480;
inline constexpr int logical_height = 132;
enum class Anchor { top_left, top_right, bottom_left, bottom_right };
struct Settings {
    Anchor anchor = Anchor::bottom_right;
    int margin_x = 24, margin_y = 24;
    int scale_percent = 100;
    BYTE opacity = 240;
};
struct Assets {
    std::unique_ptr<Gdiplus::Bitmap> lantern, oil, frame;
    bool load(const std::filesystem::path& directory) {
        lantern.reset(Gdiplus::Bitmap::FromFile((directory/L"lantern-lit.png").c_str(),FALSE));
        oil.reset(Gdiplus::Bitmap::FromFile((directory/L"oil-flask.png").c_str(),FALSE));
        frame.reset(Gdiplus::Bitmap::FromFile((directory/L"hud-frame.png").c_str(),FALSE));
        return ready();
    }
    bool ready() const {
        return lantern && frame && lantern->GetLastStatus()==Gdiplus::Ok &&
            frame->GetLastStatus()==Gdiplus::Ok && lantern->GetWidth()>0 && frame->GetWidth()>0;
    }
};
inline RECT placement(RECT client,const Settings& settings,int dpi) {
    const int cw=std::max(0L,client.right-client.left), ch=std::max(0L,client.bottom-client.top);
    if (!cw || !ch) return {client.left,client.top,client.left,client.top};
    dpi=std::clamp(dpi,48,768);
    int w=std::min(cw,MulDiv(logical_width,std::clamp(settings.scale_percent,50,200)*dpi,9600));
    int h=std::max(1,MulDiv(logical_height,w,logical_width));
    if (h>ch) { h=ch; w=std::max(1,std::min(w,MulDiv(logical_width,h,logical_height))); }
    const int mx=MulDiv(std::clamp(settings.margin_x,0,4096),dpi,96);
    const int my=MulDiv(std::clamp(settings.margin_y,0,4096),dpi,96);
    const bool right=settings.anchor==Anchor::top_right || settings.anchor==Anchor::bottom_right;
    const bool bottom=settings.anchor==Anchor::bottom_left || settings.anchor==Anchor::bottom_right;
    LONG x=right ? client.right-w-mx : client.left+mx;
    LONG y=bottom ? client.bottom-h-my : client.top+my;
    x=std::clamp(x,client.left,client.right-w);
    y=std::clamp(y,client.top,client.bottom-h);
    return {x,y,x+w,y+h};
}
inline bool visible(const Snapshot& s,bool enabled,bool foreground,bool minimized,uint64_t age_ms) {
    return enabled && foreground && !minimized && age_ms<=1500 &&
        valid(s) && s.alive && s.equipped;
}
inline double fraction(const Snapshot& s) {
    return valid(s) ? std::clamp(s.oil_seconds/s.capacity_seconds,0.0,1.0) : 0.0;
}
inline std::wstring timer_text(const Snapshot& s) {
    if (status(s)==Status::unavailable) return L"--:--";
    // Rounding up preserves a nonempty timer until the canonical resource is zero.
    if (s.oil_seconds>=360000.0) return L"99h+";
    const auto seconds=static_cast<uint64_t>(std::ceil(s.oil_seconds));
    wchar_t result[24]{};
    if (seconds>=3600) swprintf_s(result,L"%llu:%02llu:%02llu",seconds/3600,(seconds/60)%60,seconds%60);
    else swprintf_s(result,L"%02llu:%02llu",seconds/60,seconds%60);
    return result;
}
inline void text(Gdiplus::Graphics& g,const wchar_t* value,Gdiplus::RectF bounds,
                 float size,Gdiplus::Color color,bool right=false,bool bold=false) {
    Gdiplus::FontFamily family(L"Segoe UI");
    Gdiplus::Font font(&family,size,bold?Gdiplus::FontStyleBold:Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
    Gdiplus::StringFormat format;
    format.SetAlignment(right?Gdiplus::StringAlignmentFar:Gdiplus::StringAlignmentNear);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    format.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    format.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    Gdiplus::SolidBrush brush(color);
    g.DrawString(value,-1,&font,bounds,&format,&brush);
}
inline void icon(Gdiplus::Graphics& g,Gdiplus::Bitmap& image,Gdiplus::RectF bounds,float alpha) {
    Gdiplus::ColorMatrix matrix{};
    for (int i=0;i<5;++i) matrix.m[i][i]=1.0f;
    matrix.m[3][3]=alpha;
    Gdiplus::ImageAttributes attributes;
    attributes.SetColorMatrix(&matrix,Gdiplus::ColorMatrixFlagsDefault,Gdiplus::ColorAdjustTypeBitmap);
    g.DrawImage(&image,bounds,0,0,static_cast<float>(image.GetWidth()),
        static_cast<float>(image.GetHeight()),Gdiplus::UnitPixel,&attributes);
}
// Draws into a transparent PARGB bitmap supplied by the caller. Snapshot and
// hold_fraction are sampled together by the caller; drawing never advances them.
inline bool draw(Gdiplus::Bitmap& target,const Assets& assets,const Snapshot& snapshot,double hold_fraction) {
    if (!assets.ready() || !target.GetWidth() || !target.GetHeight()) return false;
    using namespace Gdiplus;
    Graphics g(&target);
    g.SetCompositingMode(CompositingModeSourceCopy);
    g.Clear(Color(0,0,0,0));
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const auto saved=g.Save();
    g.ScaleTransform(static_cast<float>(target.GetWidth())/logical_width,
                     static_cast<float>(target.GetHeight())/logical_height);
    // Crop only while drawing; the generated full-resolution PNG stays unchanged.
    g.DrawImage(assets.frame.get(),RectF(0,1,480,110),0.0f,96.0f,2172.0f,500.0f,UnitPixel);
    const auto state=status(snapshot);
    const bool unavailable=state==Status::unavailable;
    const bool warning=state==Status::low_oil || state==Status::empty;
    const Color ivory(255,232,221,199), muted(255,170,155,132);
    const Color accent=warning ? Color(255,219,133,82) : Color(255,215,163,74);
    const Color state_color=(state==Status::lit || state==Status::low_oil || state==Status::refilling) ? accent : muted;
    const float icon_alpha=unavailable?0.18f:(state==Status::off?0.42f:(state==Status::empty?0.25f:1.0f));
    icon(g,*assets.lantern,RectF(35,14,82,82),icon_alpha);
    text(g,L"WAYFARER'S LANTERN",RectF(131,31,197,16),10,ivory,false,true);
    text(g,snapshot.charge_mode&&state==Status::low_oil?L"LIT":label(state),RectF(326,31,112,16),10,state_color,true,true);
    if(!snapshot.charge_mode&&assets.oil&&assets.oil->GetLastStatus()==Ok)icon(g,*assets.oil,RectF(130,45,28,28),unavailable?0.25f:0.85f);
    const auto time=snapshot.charge_mode?std::to_wstring(static_cast<int>(snapshot.oil_seconds))+L" / "+std::to_wstring(static_cast<int>(snapshot.capacity_seconds)):timer_text(snapshot);
    text(g,time.c_str(),RectF(165,44,130,29),20,unavailable?muted:ivory,false,true);
    text(g,snapshot.charge_mode?L"USES":L"OIL",RectF(289,48,55,19),10,muted,true);
    wchar_t percent[16]{};
    if (unavailable) wcscpy_s(percent,L"--");
    else swprintf_s(percent,L"%d%%",static_cast<int>(std::lround(fraction(snapshot)*100)));
    text(g,percent,RectF(349,45,88,26),15,state_color,true,true);
    const RectF track(132,73,304,4);
    SolidBrush track_brush(Color(230,52,45,34));
    g.FillRectangle(&track_brush,track);
    const float oil_width=unavailable?0.0f:static_cast<float>(fraction(snapshot))*track.Width;
    if (oil_width>0) {
        SolidBrush oil_brush(state==Status::off?Color(255,131,112,75):accent);
        g.FillRectangle(&oil_brush,RectF(track.X,track.Y,oil_width,track.Height));
    }
    const wchar_t* hint=snapshot.context_known?L"Hold Interact for 2 seconds":L"Use Lantern Oil to refill";
    if (unavailable) hint=L"Lantern state unavailable";
    else if (state==Status::off) hint=L"Oil preserved while unlit";
    else if (state==Status::empty) hint=L"Refill lantern oil to light";
    else if (state==Status::refilling) hint=L"Refilling lantern oil";
    if(snapshot.charge_mode)hint=unavailable?L"Lantern state unavailable":
        (state==Status::empty?L"Rest at a bonfire to refill":L"Use Lantern to light / stow. Bonfire refills.");
    double hold=std::isfinite(hold_fraction)?std::clamp(hold_fraction,0.0,1.0):0.0;
    if (!unavailable && state!=Status::empty && state!=Status::refilling && hold>0) {
        Pen ring_track(Color(180,88,75,52),2.0f),ring(accent,2.3f);
        g.DrawEllipse(&ring_track,RectF(42,20,68,68));
        g.DrawArc(&ring,RectF(42,20,68,68),-90.0f,static_cast<float>(hold*360.0));
        hint=snapshot.lit?L"Release early to cancel extinguishing":L"Release early to cancel lighting";
    }
    text(g,hint,RectF(130,106,309,17),10,muted);
    g.Restore(saved);
    return g.GetLastStatus()==Ok;
}
inline LRESULT CALLBACK overlay_procedure(HWND window,UINT message,WPARAM wparam,LPARAM lparam) {
    if (message==WM_NCHITTEST) return HTTRANSPARENT;
    if (message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (message==WM_ERASEBKGND) return 1;
    return DefWindowProcW(window,message,wparam,lparam);
}
// RAII fixture/production helper. Its owning UI thread must create, update and
// destroy it, and retain an ordinary message loop. It never requests foreground.
class Overlay {
    HWND window_=nullptr;
public:
    Overlay()=default;
    Overlay(const Overlay&)=delete;
    Overlay& operator=(const Overlay&)=delete;
    ~Overlay() { if (window_) DestroyWindow(window_); }
    HWND handle() const { return window_; }
    bool create(HWND owner,HINSTANCE instance) {
        if (window_ || !IsWindow(owner)) return false;
        constexpr wchar_t name[]=L"DS2FX.WayfarerLantern.View.1";
        WNDCLASSW cls{}; cls.hInstance=instance; cls.lpfnWndProc=overlay_procedure; cls.lpszClassName=name;
        if (!RegisterClassW(&cls) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return false;
        window_=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW|WS_EX_TOPMOST,
            name,L"DS2FX Wayfarer's Lantern",WS_POPUP,0,0,1,1,owner,nullptr,instance,nullptr);
        return window_!=nullptr;
    }
    void hide() { if (window_) ShowWindow(window_,SW_HIDE); }
    DWORD present(Gdiplus::Bitmap& image,RECT bounds,BYTE opacity,bool show) {
        if (!window_ || bounds.right-bounds.left!=static_cast<LONG>(image.GetWidth()) ||
            bounds.bottom-bounds.top!=static_cast<LONG>(image.GetHeight()) ||
            !image.GetWidth() || !image.GetHeight() ||
            uint64_t(image.GetWidth())*image.GetHeight()>16777216) return ERROR_INVALID_PARAMETER;
        HDC dc=CreateCompatibleDC(nullptr);
        if (!dc) return GetLastError();
        struct Surface {
            HDC dc; HBITMAP bitmap=nullptr; HGDIOBJ previous=nullptr;
            ~Surface() { if (previous) SelectObject(dc,previous); if (bitmap) DeleteObject(bitmap); DeleteDC(dc); }
        } surface{dc};
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=static_cast<LONG>(image.GetWidth());
        info.bmiHeader.biHeight=-static_cast<LONG>(image.GetHeight());
        info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        void* bits=nullptr;
        surface.bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
        if (!surface.bitmap || !bits) return GetLastError();
        surface.previous=SelectObject(dc,surface.bitmap);
        const Gdiplus::Rect area(0,0,image.GetWidth(),image.GetHeight());
        Gdiplus::BitmapData data{};
        if (image.LockBits(&area,Gdiplus::ImageLockModeRead,PixelFormat32bppPARGB,&data)!=Gdiplus::Ok) return ERROR_INVALID_DATA;
        const auto row_bytes=static_cast<size_t>(image.GetWidth())*4;
        for (UINT y=0;y<image.GetHeight();++y)
            std::memcpy(static_cast<BYTE*>(bits)+y*row_bytes,
                static_cast<BYTE*>(data.Scan0)+static_cast<ptrdiff_t>(y)*data.Stride,row_bytes);
        image.UnlockBits(&data);
        POINT dest{bounds.left,bounds.top},source{};
        SIZE size{static_cast<LONG>(image.GetWidth()),static_cast<LONG>(image.GetHeight())};
        BLENDFUNCTION blend{AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(window_,nullptr,&dest,&size,dc,&source,0,&blend,ULW_ALPHA)) return GetLastError();
        const auto flags=SWP_NOACTIVATE|SWP_NOOWNERZORDER|(show?SWP_SHOWWINDOW:SWP_HIDEWINDOW);
        if (!SetWindowPos(window_,HWND_TOPMOST,bounds.left,bounds.top,size.cx,size.cy,flags)) return GetLastError();
        return ERROR_SUCCESS;
    }
};
}
