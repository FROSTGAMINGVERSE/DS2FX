#include "../src/graphics_bridge.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
unsigned frames=0;uint64_t last=0;
void render(const DS2FX_GraphicsFrame* frame,void* user){
    check(user==&frames && frame && frame->size==sizeof(*frame) && frame->version==1 && frame->context && frame->back_buffer && frame->width==16 && frame->height==16 && frame->sequence>last,"Invalid public graphics frame");
    last=frame->sequence;++frames;
}
int main(){try {
    auto owner=GetModuleHandleW(nullptr);DS2FX_GraphicsCallback callback{sizeof(callback),1,render,&frames};
    check(!fx::graphics::subscribe(nullptr,&callback),"Missing owner accepted");
    auto bad=callback;bad.version=2;check(!fx::graphics::subscribe(owner,&bad),"Unknown API accepted");
    bad=callback;bad.size=0;check(!fx::graphics::subscribe(owner,&bad),"Truncated API accepted");
    check(!fx::graphics::subscribe(GetModuleHandleW(L"kernel32.dll"),&callback),"Foreign callback owner accepted");
    HWND window=CreateWindowExW(0,L"STATIC",L"DS2FX bridge test",WS_POPUP,0,0,16,16,nullptr,nullptr,owner,nullptr);
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=16;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<IDXGISwapChain> chain;
    check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,nullptr,nullptr)),"Device failed");
    auto handle=fx::graphics::subscribe(owner,&callback);check(handle!=0,"Public registration failed");
    chain->Present(0,DXGI_PRESENT_TEST);check(frames==0,"TEST invoked callback");
    chain->Present(0,0);check(frames==1,"Public callback missing");
    check(!fx::graphics::unsubscribe(GetModuleHandleW(L"kernel32.dll"),handle),"Foreign unsubscribe accepted");
    chain->Present(0,0);check(frames==2,"Foreign unsubscribe removed callback");
    check(fx::graphics::unsubscribe(owner,handle)==1,"Owner unsubscribe failed");chain->Present(0,0);check(frames==2,"Unsubscribed callback invoked");
    check(fx::graphics::subscribe(owner,&callback)!=0,"Re-registration failed");fx::graphics::shutdown();chain->Present(0,0);check(frames==2,"Shutdown callback invoked");
    check(!fx::graphics::subscribe(owner,&callback),"Shutdown accepted registration");
    DestroyWindow(window);std::cout<<"PASS public graphics bridge: sized ABI, ownership, frame callback, TEST filtering, unsubscribe, shutdown\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
