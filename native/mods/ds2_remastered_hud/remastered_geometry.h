#pragma once
#include <windows.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>

namespace remastered {
// DWM frame bounds are physical pixels. In a DPI-virtualized game process,
// GetClientRect/GetWindowRect can instead report logical pixels. Map the
// client relative to the game window so a borderless 4K game at 150% scaling
// does not leave a 2560x1440 overlay in its upper-left corner.
inline RECT physical_client_bounds(const RECT& logical_window,const RECT& physical_frame,
                                   const RECT& logical_client,POINT logical_origin) noexcept {
    const double lw=double(logical_window.right)-logical_window.left;
    const double lh=double(logical_window.bottom)-logical_window.top;
    const double fw=double(physical_frame.right)-physical_frame.left;
    const double fh=double(physical_frame.bottom)-physical_frame.top;
    if(lw<=0 || lh<=0 || fw<=0 || fh<=0) return {};
    const double sx=fw/lw,sy=fh/lh;
    if(sx<0.75 || sx>2.5 || sy<0.75 || sy>2.5 ||
       std::abs(sx-sy)>0.05*std::max(sx,sy)) return {};
    const int x=physical_frame.left+static_cast<int>(std::lround((logical_origin.x-logical_window.left)*sx));
    const int y=physical_frame.top+static_cast<int>(std::lround((logical_origin.y-logical_window.top)*sy));
    const int w=static_cast<int>(std::lround((logical_client.right-logical_client.left)*sx));
    const int h=static_cast<int>(std::lround((logical_client.bottom-logical_client.top)*sy));
    return {x,y,x+w,y+h};
}
inline bool game_client_bounds(HWND game,RECT& output) noexcept {
    RECT client{},window{},frame{}; POINT origin{};
    if(!GetClientRect(game,&client) || !ClientToScreen(game,&origin) ||
       !GetWindowRect(game,&window)) return false;
    if(DwmGetWindowAttribute(game,DWMWA_EXTENDED_FRAME_BOUNDS,&frame,sizeof(frame))==S_OK) {
        const auto physical=physical_client_bounds(window,frame,client,origin);
        if(physical.right>physical.left && physical.bottom>physical.top) {
            output=physical; return true;
        }
    }
    output={origin.x,origin.y,origin.x+client.right-client.left,origin.y+client.bottom-client.top};
    return true;
}
}
