#include <reshade.hpp>
#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
using Microsoft::WRL::ComPtr;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int wmain(){try {
    // Owned, hidden, unsaved WARP swap chain. Never opens the game or OBS.
    SetEnvironmentVariableW(L"RESHADE_DISABLE_GRAPHICS_HOOK",L"1");
    HWND window=CreateWindowExW(0,L"STATIC",L"DS2FX owned in-game HUD fixture",WS_POPUP,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(window!=nullptr,"Fixture window failed");
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=320;desc.BufferDesc.Height=240;desc.BufferDesc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> chain;
    check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,nullptr,&context)),"Fixture swap chain failed");
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);const auto folder=std::filesystem::path(path).parent_path();
    HMODULE runtime_module=LoadLibraryW((folder/L"ReShade68.dll").c_str());
    if(!runtime_module)throw std::runtime_error("Installed manual ReShade runtime failed to load; Win32="+std::to_string(GetLastError()));
    using Create=bool(*)(uint32_t,void*,void*,void*,const char*,reshade::api::effect_runtime**);
    using Update=void(*)(reshade::api::effect_runtime*);
    using Destroy=void(*)(reshade::api::effect_runtime*);
    auto create=reinterpret_cast<Create>(GetProcAddress(runtime_module,"ReShadeCreateEffectRuntime"));
    auto update=reinterpret_cast<Update>(GetProcAddress(runtime_module,"ReShadeUpdateAndPresentEffectRuntime"));
    auto destroy=reinterpret_cast<Destroy>(GetProcAddress(runtime_module,"ReShadeDestroyEffectRuntime"));
    check(create && update && destroy,"Manual API exports missing");
    reshade::api::effect_runtime* runtime=nullptr;
    const auto config=(folder/L"fixture.ini").string();
    check(create(0xb000,device.Get(),context.Get(),chain.Get(),config.c_str(),&runtime),"Manual runtime creation failed");
    HMODULE plugin=LoadLibraryW((folder/L"DS2FX"/L"Plugins"/L"ds2-remastered-hud"/L"plugin.dll").c_str());
    check(plugin!=nullptr,"Fixture renderer DLL failed to load");
    auto publish=reinterpret_cast<bool(*)(HWND,bool)>(GetProcAddress(plugin,"FixturePublish"));
    auto grants=reinterpret_cast<unsigned(*)()>(GetProcAddress(plugin,"FixtureGrants"));
    auto revokes=reinterpret_cast<unsigned(*)()>(GetProcAddress(plugin,"FixtureRevokes"));
    check(publish && grants && revokes,"Fixture exports missing");
    ComPtr<ID3D11Texture2D> back;chain->GetBuffer(0,IID_PPV_ARGS(&back));
    ComPtr<ID3D11RenderTargetView> view;device->CreateRenderTargetView(back.Get(),nullptr,&view);
    D3D11_TEXTURE2D_DESC data{};back->GetDesc(&data);data.BindFlags=0;data.Usage=D3D11_USAGE_STAGING;data.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;device->CreateTexture2D(&data,nullptr,&readback);
    auto red=[&](){context->CopyResource(readback.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE m{};
        check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&m)),"Readback failed");
        uint32_t packed;std::memcpy(&packed,static_cast<BYTE*>(m.pData)+120*m.RowPitch+160*4,4);
        const auto value=(packed&1023)*255/1023;context->Unmap(readback.Get(),0);return value;};
    const float blue[]{0,0,1,1};context->ClearRenderTargetView(view.Get(),blue);
    check(publish(window,false),"Bootstrap publish failed");update(runtime);
    check(GetModuleHandleW((folder/L"reshade-addons"/L"DS2FX-Remastered-HUD.addon64").c_str())!=nullptr,"Render-thread bootstrap did not load the adapter");
    check(publish(window,false),"Authenticated adapter unavailable");update(runtime);
    check(grants()==1 && red()==0,"New lease drew over the current native HUD frame");
    update(runtime);
    check(grants()==2 && red()==255,"Actual ReShade callback did not composite the HUD into the next game buffer");
    auto foreign=reinterpret_cast<void(*)(HWND,ID3D11DeviceContext*,ID3D11Texture2D*,BOOL)>(GetProcAddress(plugin,"DS2FX_RenderRemasteredHud"));
    context->ClearRenderTargetView(view.Get(),blue);foreign(window,context.Get(),back.Get(),TRUE);
    check(grants()==2 && red()==0,"Foreign caller bypassed adapter authentication");
    context->ClearRenderTargetView(view.Get(),blue);check(publish(window,true),"Stale frame publish failed");update(runtime);
    check(grants()==2 && revokes()>0 && red()==0,"Stale frame renewed suppression or drew pixels");
    check(publish(window,false),"Recovery publish failed");update(runtime);
    check(grants()==3 && red()==0,"Recovery drew replacement over already-rendered native bars");
    update(runtime);check(grants()==4 && red()==255,"Recovery failed to draw on the following frame");
    view.Reset();back.Reset();context->ClearState();destroy(runtime);DestroyWindow(window);
    std::cout<<"PASS: installed ReShade68 manual runtime loaded actual adapter; authenticated HUD callback drew captured back-buffer pixels; stale frame revoked native lease; effects disabled/empty still render HUD\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
