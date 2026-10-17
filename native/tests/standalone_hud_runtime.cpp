#include <d3d11.h>
#include <wrl/client.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
using Microsoft::WRL::ComPtr;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int wmain(){try {
    check(!GetModuleHandleW(L"ReShade68.dll"),"Fixture must run without ReShade");
    HWND window=CreateWindowExW(0,L"STATIC",L"DS2FX standalone fixture",WS_POPUP,0,0,64,64,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    check(window!=nullptr,"Window failed");
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=64;
    desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> chain;
    check(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,nullptr,&context)),"Hardware swap chain failed");
    wchar_t path[32768]{};GetModuleFileNameW(nullptr,path,32768);
    auto plugin=LoadLibraryW((std::filesystem::path(path).parent_path()/L"ds2fx_ingame_hud_fixture.dll").c_str());
    check(plugin!=nullptr,"Renderer fixture failed to load");
    auto publish=reinterpret_cast<bool(*)(HWND,bool)>(GetProcAddress(plugin,"FixturePublish"));
    auto grants=reinterpret_cast<unsigned(*)()>(GetProcAddress(plugin,"FixtureGrants"));
    auto revokes=reinterpret_cast<unsigned(*)()>(GetProcAddress(plugin,"FixtureRevokes"));
    auto detach=reinterpret_cast<void(*)()>(GetProcAddress(plugin,"FixtureDetach"));
    check(publish && grants && revokes && detach,"Fixture exports missing");
    ComPtr<ID3D11Texture2D> back;check(SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&back))),"Backbuffer failed");
    ComPtr<ID3D11RenderTargetView> view;device->CreateRenderTargetView(back.Get(),nullptr,&view);
    D3D11_TEXTURE2D_DESC data{};back->GetDesc(&data);data.BindFlags=0;data.Usage=D3D11_USAGE_STAGING;data.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;device->CreateTexture2D(&data,nullptr,&readback);
    auto red=[&](){context->CopyResource(readback.Get(),back.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&mapped)),"Readback failed");
        const unsigned value=*(static_cast<BYTE*>(mapped.pData)+32*mapped.RowPitch+32*4);context->Unmap(readback.Get(),0);return value;};
    const float blue[]{0,0,1,1};
    publish(window,false);context->ClearRenderTargetView(view.Get(),blue);chain->Present(0,DXGI_PRESENT_TEST);
    check(grants()==0 && red()==0,"TEST Present acquired suppression");
    publish(window,false);chain->Present(0,0);check(grants()==1 && red()==0,"New lease failed priming");
    chain->Present(0,0);check(grants()==2 && red()==255,"Standalone callback failed pixel composition");
    HWND foreign=CreateWindowExW(0,L"STATIC",L"Foreign fixture",WS_POPUP,0,0,2,2,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    publish(foreign,false);chain->Present(0,0);check(grants()==2,"Foreign window acquired suppression");DestroyWindow(foreign);
    context->ClearRenderTargetView(view.Get(),blue);publish(window,true);chain->Present(0,0);
    check(grants()==2 && revokes()>0 && red()==0,"Stale frame did not restore native fallback");
    publish(window,false);chain->Present(0,0);check(grants()==3 && red()==0,"Recovery skipped priming");
    chain->Present(0,0);check(grants()==4 && red()==255,"Recovery did not composite");
    view.Reset();back.Reset();context->ClearState();
    check(SUCCEEDED(chain->ResizeBuffers(1,96,96,DXGI_FORMAT_UNKNOWN,0)),"Renderer retained a back buffer during resize");
    detach();chain->Present(0,0);check(grants()==4,"Detached renderer still acquired leases");
    DestroyWindow(window);
    std::cout<<"PASS standalone DXGI: pixels, TEST, foreign HWND, expiry, recovery, ResizeBuffers, detach; no ReShade or modlist\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
