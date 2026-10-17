#include "../mods/ds2_remastered_hud/remastered_ingame.h"
#include <iostream>
#include <stdexcept>
#include <cmath>
#include <cstring>
using Microsoft::WRL::ComPtr;
using namespace remastered::ingame;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
void run(bool allow_state,DXGI_FORMAT format) {
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;D3D_FEATURE_LEVEL level{};
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,&level,&context)),"WARP device failed");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=8;desc.MipLevels=desc.ArraySize=1;
    desc.Format=format;desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> target;ComPtr<ID3D11RenderTargetView> view;
    check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&target)) && SUCCEEDED(device->CreateRenderTargetView(target.Get(),nullptr,&view)),"Target creation failed");
    Frame f;f.epoch=2;f.created=1000;f.sample=1;f.revision=1;
    f.width=f.height=2;f.canvas_width=f.canvas_height=4;f.x=f.y=1;
    f.regions=DS2FX_HUD_REGION_BARS|DS2FX_HUD_REGION_COVENANT;
    for(int i=0;i<4;++i){f.pixels[0].insert(f.pixels[0].end(),{0,255,0,255});f.pixels[1].insert(f.pixels[1].end(),{0,0,128,128});}
    check(fresh(f,1200,2) && !fresh(f,1201,2) && !fresh(f,999,2) && !fresh(f,1100,3),"Stale/foreign frame accepted");
    Compositor compositor(allow_state);check(compositor.prepare(context.Get(),f),"Compositor prepare failed");
    if(!allow_state)check(!compositor.isolated_state(),"Legacy test used extended state");
    const D3D11_VIEWPORT sentinel{1,2,3,4,.2f,.8f};context->RSSetViewports(1,&sentinel);
    ID3D11RenderTargetView* rtv=view.Get();context->OMSetRenderTargets(1,&rtv,nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
    ComPtr<ID3D11Buffer> buffer;D3D11_BUFFER_DESC cb{};cb.ByteWidth=32;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    check(SUCCEEDED(device->CreateBuffer(&cb,nullptr,&buffer)),"State fixture CB failed");
    ID3D11Buffer* cbp=buffer.Get();context->VSSetConstantBuffers(7,1,&cbp);context->PSSetConstantBuffers(0,1,&cbp);
    const float blue[]{0,0,1,1};context->ClearRenderTargetView(view.Get(),blue);
    check(compositor.draw(context.Get(),view.Get(),f,DS2FX_HUD_REGION_BARS),"Bars draw failed");
    D3D11_VIEWPORT restored{};UINT count=1;context->RSGetViewports(&count,&restored);
    check(count==1 && restored.TopLeftX==1 && restored.TopLeftY==2 && restored.MinDepth==.2f,"Viewport not restored");
    D3D11_PRIMITIVE_TOPOLOGY topology{};context->IAGetPrimitiveTopology(&topology);check(topology==D3D11_PRIMITIVE_TOPOLOGY_LINELIST,"IA state not restored");
    ComPtr<ID3D11Buffer> restored_cb;context->VSGetConstantBuffers(7,1,&restored_cb);check(restored_cb.Get()==buffer.Get(),"VS CB state not restored");
    restored_cb.Reset();context->PSGetConstantBuffers(0,1,&restored_cb);check(restored_cb.Get()==buffer.Get(),"PS CB state not restored");
    ComPtr<ID3D11RenderTargetView> restored_rtv;context->OMGetRenderTargets(1,&restored_rtv,nullptr);check(restored_rtv.Get()==view.Get(),"OM target not restored");
    desc.BindFlags=0;desc.Usage=D3D11_USAGE_STAGING;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> readback;check(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&readback)),"Readback fixture failed");
    auto pixel=[&](int x,int y){context->CopyResource(readback.Get(),target.Get());D3D11_MAPPED_SUBRESOURCE map{};
        check(SUCCEEDED(context->Map(readback.Get(),0,D3D11_MAP_READ,0,&map)),"Fixture readback failed");
        const BYTE* p=static_cast<const BYTE*>(map.pData)+y*map.RowPitch+x*4;
        std::vector<int> result{p[0],p[1],p[2],p[3]};
        if(format==DXGI_FORMAT_B8G8R8A8_UNORM)std::swap(result[0],result[2]);
        if(format==DXGI_FORMAT_R10G10B10A2_UNORM){uint32_t packed;std::memcpy(&packed,p,4);
            result={int(std::lround((packed&1023)*255.0/1023)),int(std::lround(((packed>>10)&1023)*255.0/1023)),
                int(std::lround(((packed>>20)&1023)*255.0/1023)),int((packed>>30)*85)};}
        context->Unmap(readback.Get(),0);return result;};
    auto inside=pixel(3,3),outside=pixel(0,0);
    check(inside[0]==128 && inside[1]==0 && inside[2]==127,"Premultiplied blend or rejected optional layer failed");
    check(outside[0]==0 && outside[2]==255,"HUD crop corrupted outside pixels");
    context->ClearRenderTargetView(view.Get(),blue);
    check(compositor.draw(context.Get(),view.Get(),f,f.regions),"Accepted covenant draw failed");
    inside=pixel(3,3);check(inside[0]==128 && inside[1]==127 && inside[2]==0,"Covenant/bar layer order failed");
    f.opacity=128;context->ClearRenderTargetView(view.Get(),blue);
    check(compositor.draw(context.Get(),view.Get(),f,DS2FX_HUD_REGION_BARS),"Opacity draw failed");
    inside=pixel(3,3);check(std::abs(inside[0]-64)<=1 && std::abs(inside[2]-191)<=1,"Global opacity failed");
    Frame blank=f;blank.blank=true;context->ClearRenderTargetView(view.Get(),blue);
    check(compositor.prepare(context.Get(),blank) && compositor.draw(context.Get(),view.Get(),blank,1),"Blank frame failed");
    inside=pixel(3,3);check(inside[0]==0 && inside[2]==255,"Blank frame drew pixels");
    check(!compositor.draw(context.Get(),view.Get(),f,0),"Missing bars grant accepted");
    f.revision=2;f.width=0;check(!compositor.prepare(context.Get(),f),"Invalid dimensions accepted");
    compositor.reset();f.width=2;f.height=1;f.pixels[0].resize(8);f.pixels[1].resize(8);
    check(compositor.prepare(context.Get(),f),"Resource recreation after reset/resize failed");
    ComPtr<ID3D11Device> foreign_device;ComPtr<ID3D11DeviceContext> foreign_context;
    check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&foreign_device,nullptr,&foreign_context)),"Foreign device fixture failed");
    desc.Usage=D3D11_USAGE_DEFAULT;desc.CPUAccessFlags=0;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    ComPtr<ID3D11Texture2D> foreign_texture;ComPtr<ID3D11RenderTargetView> foreign_view;
    check(SUCCEEDED(foreign_device->CreateTexture2D(&desc,nullptr,&foreign_texture)) && SUCCEEDED(foreign_device->CreateRenderTargetView(foreign_texture.Get(),nullptr,&foreign_view)),"Foreign target fixture failed");
    check(!compositor.draw(context.Get(),foreign_view.Get(),f,DS2FX_HUD_REGION_BARS),"Foreign device target accepted");
    context->ClearState();compositor.reset();
}
int main(){try {
    for(auto format:{DXGI_FORMAT_R8G8B8A8_UNORM,DXGI_FORMAT_B8G8R8A8_UNORM,DXGI_FORMAT_R10G10B10A2_UNORM}){run(true,format);run(false,format);}
    std::cout<<"PASS: 8-bit RGBA/BGRA and 10-bit SDR WARP pixels and graphics state restored with D3D11.1 isolation and base D3D11 bindings; blend, layer filtering, opacity, stale/blank and reset\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
