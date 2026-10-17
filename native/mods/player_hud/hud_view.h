#pragma once
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cwchar>
#include <cmath>
#include "../../sdk/ds2fx_api.h"

namespace hud {
inline constexpr wchar_t window_class[] = L"DS2FX.PlayerHUD.1";
struct Settings {
    int enabled = 1;
    int threshold = 30;
    int scale = 100;
    int x = 24;
    int y = 24;
    int anchor_right = 1;
    int opacity = 240;
    int show_stamina = 1;
};
inline bool valid(const DS2FX_PlayerState& state) {
    return state.available && state.max_health > 0 && state.health >= 0 && state.health <= state.max_health;
}
inline bool low(const DS2FX_PlayerState& state,int threshold) {
    return valid(state) && int64_t(state.health)*100 <= int64_t(state.max_health)*threshold;
}
inline bool visible(const DS2FX_PlayerState& state,bool enabled,bool foreground,bool minimized,uint64_t age_ms) {
    return enabled && valid(state) && foreground && !minimized && age_ms <= 1500;
}
inline RECT placement(RECT client,int width,int height,const Settings& settings,int dpi) {
    int margin_x = MulDiv(settings.x,dpi,96), margin_y = MulDiv(settings.y,dpi,96);
    int x = settings.anchor_right ? client.right-width-margin_x : client.left+margin_x;
    int y = client.top+margin_y;
    x = static_cast<int>(std::clamp<LONG>(x,client.left,std::max(client.left,client.right-width)));
    y = static_cast<int>(std::clamp<LONG>(y,client.top,std::max(client.top,client.bottom-height)));
    return {x,y,x+width,y+height};
}
// Explicit ownership keeps the HUD above this game's window even when the game
// or display-mode changes alter the desktop's topmost window band.
inline DWORD position_window(HWND overlay,HWND game,RECT bounds,bool show) {
    if (GetWindow(overlay,GW_OWNER) != game) {
        SetLastError(0);
        const auto previous = SetWindowLongPtrW(overlay,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(game));
        if (!previous && GetLastError()) return GetLastError();
    }
    const auto flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | (show ? SWP_SHOWWINDOW : 0);
    if (!SetWindowPos(overlay,HWND_TOPMOST,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,flags)) return GetLastError();
    return ERROR_SUCCESS;
}
inline void fill(HDC dc,RECT rectangle,COLORREF color) {
    auto brush = CreateSolidBrush(color); FillRect(dc,&rectangle,brush); DeleteObject(brush);
}
inline void label(HDC dc,const wchar_t* text,RECT rectangle,int pixels,int weight,COLORREF color,UINT align=DT_LEFT) {
    auto font = CreateFontW(-pixels,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    auto old = SelectObject(dc,font);
    SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color);
    DrawTextW(dc,text,-1,&rectangle,align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc,old); DeleteObject(font);
}
inline void draw(HDC dc,int width,int height,const DS2FX_PlayerState& state,const Settings& settings) {
    auto px = [width](int value) { return MulDiv(value,width,280); };
    const bool warning = low(state,settings.threshold);
    const auto accent = warning ? RGB(255,105,91) : RGB(111,220,195);
    fill(dc,{0,0,width,height},RGB(17,23,29));
    fill(dc,{0,0,px(3),height},accent);
    label(dc,L"HEALTH",{px(17),px(9),px(140),px(27)},px(11),FW_SEMIBOLD,RGB(175,187,198));
    label(dc,warning ? L"LOW HEALTH" : L"DS2FX",{px(145),px(9),width-px(17),px(27)},px(10),FW_SEMIBOLD,warning ? accent : RGB(133,151,167),DT_RIGHT);
    wchar_t text[64];
    swprintf_s(text,L"%d / %d",state.health,state.max_health);
    label(dc,text,{px(17),px(28),width-px(17),px(61)},px(26),FW_SEMIBOLD,RGB(242,246,248));
    const RECT track{px(17),px(69),width-px(17),px(74)};
    fill(dc,track,RGB(48,59,69));
    const int range = track.right-track.left;
    int bar = valid(state) ? static_cast<int>(int64_t(range)*state.health/state.max_health) : 0;
    fill(dc,{track.left,track.top,track.left+bar,track.bottom},accent);
    if (settings.show_stamina) {
        const bool stamina_valid = state.stamina_available && std::isfinite(state.stamina) &&
            std::isfinite(state.max_stamina) && state.max_stamina > 0;
        label(dc,L"STAMINA",{px(17),px(88),px(110),px(111)},px(11),FW_SEMIBOLD,RGB(175,187,198));
        if (stamina_valid) swprintf_s(text,L"%.0f / %.0f",std::max(0.0f,state.stamina),state.max_stamina);
        else wcscpy_s(text,L"Unavailable");
        label(dc,text,{px(115),px(85),width-px(17),px(114)},px(18),FW_SEMIBOLD,RGB(242,246,248),DT_RIGHT);
        const RECT sp_track{px(17),px(120),width-px(17),px(124)};
        fill(dc,sp_track,RGB(48,59,69));
        const float fraction = stamina_valid ? std::clamp(state.stamina/state.max_stamina,0.0f,1.0f) : 0.0f;
        const int sp_bar = static_cast<int>((sp_track.right-sp_track.left)*fraction);
        fill(dc,{sp_track.left,sp_track.top,sp_track.left+sp_bar,sp_track.bottom},RGB(164,208,113));
    }
}
}
