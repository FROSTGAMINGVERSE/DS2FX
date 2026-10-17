#pragma once
#include <windows.h>
#include <objidl.h>
#include <cstdint>
#include <gdiplus.h>
#include <memory>
namespace remastered {
// Persistent top-down premultiplied DIB: GDI+ draws into the memory used by
// UpdateLayeredWindow. No full-screen staging bitmap or per-frame pixel copy.
class Surface {
    HDC dc=nullptr;HBITMAP dib=nullptr;HGDIOBJ previous=nullptr;
    int w=0,h=0;void* pixel_data=nullptr;
    uint64_t promoted_at=0;
    std::unique_ptr<Gdiplus::Bitmap> image;
    std::unique_ptr<Gdiplus::Graphics> graphics;
public:
    ~Surface(){clear();}
    void clear() {
        graphics.reset();image.reset();
        if(dc && previous)SelectObject(dc,previous);
        if(dib)DeleteObject(dib);if(dc)DeleteDC(dc);
        dc=nullptr;dib=nullptr;previous=nullptr;pixel_data=nullptr;w=h=0;
    }
    bool resize(int width,int height) {
        if(width==w && height==h && graphics)return true;
        clear();if(width<=0 || height<=0)return false;
        dc=CreateCompatibleDC(nullptr);if(!dc)return false;
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;dib=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(!dib || !pixels){clear();return false;}
        previous=SelectObject(dc,dib);w=width;h=height;pixel_data=pixels;
        image=std::make_unique<Gdiplus::Bitmap>(w,h,w*4,PixelFormat32bppPARGB,static_cast<BYTE*>(pixels));
        graphics=std::make_unique<Gdiplus::Graphics>(image.get());
        if(image->GetLastStatus()!=Gdiplus::Ok || graphics->GetLastStatus()!=Gdiplus::Ok){clear();return false;}
        return true;
    }
    Gdiplus::Graphics& context(){return *graphics;}
    Gdiplus::Bitmap& bitmap(){return *image;}
    const BYTE* pixels() const{return static_cast<const BYTE*>(pixel_data);}
    DWORD present(HWND window,const RECT& bounds,BYTE opacity,bool show=true) {
        graphics->Flush(Gdiplus::FlushIntentionSync);
        POINT dest{bounds.left,bounds.top},source{};SIZE size{w,h};
        BLENDFUNCTION blend{AC_SRC_OVER,0,opacity,AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(window,nullptr,&dest,&size,dc,&source,0,&blend,ULW_ALPHA))return GetLastError();
        if(!show){ShowWindow(window,SW_HIDE);return ERROR_SUCCESS;}
        const auto now=GetTickCount64();
        if((!IsWindowVisible(window) || now-promoted_at>=1000) && !SetWindowPos(window,HWND_TOPMOST,bounds.left,bounds.top,w,h,
            SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW))return GetLastError();
        if(now-promoted_at>=1000)promoted_at=now;
        return ERROR_SUCCESS;
    }
};
}
