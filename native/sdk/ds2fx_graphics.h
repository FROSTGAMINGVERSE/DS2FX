#pragma once
#include <windows.h>
#include <cstdint>
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
// Borrowed objects valid only inside the callback. Never retain the back buffer.
// Runs on the presentation thread, NOT the engine/game thread. No gameplay calls.
// Callbacks preserve all touched D3D state and must not block, throw, or unload.
// API v1 targets the unmodified Scholar SDR D3D11 renderer. HDR is unsupported.
struct DS2FX_GraphicsFrame {
    uint32_t size,version;
    HWND window;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* back_buffer;
    uint32_t width,height,format,sdr;
    uint64_t sequence;
};
using DS2FX_GraphicsCallbackFn=void(*)(const DS2FX_GraphicsFrame*,void* user);
struct DS2FX_GraphicsCallback {
    uint32_t size,version;
    DS2FX_GraphicsCallbackFn present;
    void* user;
};
