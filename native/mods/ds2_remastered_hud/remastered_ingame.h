#pragma once
#include "../../sdk/ds2fx_api.h"
#include <d3d11_1.h>
#include <wrl/client.h>
#include <memory>
#include <vector>

namespace remastered::ingame {
struct Frame {
    HWND game=nullptr;
    uint64_t epoch=0,sample=0,created=0,revision=0;
    uint32_t regions=0;
    int width=0,height=0,canvas_width=0,canvas_height=0,x=0,y=0;
    BYTE opacity=255;
    bool blank=false;
    DS2FX_HudIcons icons{};
    std::vector<BYTE> pixels[3]; // covenant, bars, effects; original layer order
};
inline bool fresh(const Frame& f,uint64_t now,uint64_t epoch) noexcept {
    return f.epoch && f.epoch==epoch && f.created && now>=f.created && now-f.created<=200;
}
// Own only compositor resources. Never retain the swap chain or its back buffer.
class Compositor {
    template<class T> using Ptr=Microsoft::WRL::ComPtr<T>;
    Ptr<ID3D11Device> device;
    Ptr<ID3D11VertexShader> vs;
    Ptr<ID3D11PixelShader> ps;
    Ptr<ID3D11Buffer> constants;
    Ptr<ID3D11BlendState> blend;
    Ptr<ID3D11DepthStencilState> depth;
    Ptr<ID3D11RasterizerState> raster;
    Ptr<ID3D11SamplerState> sampler;
    Ptr<ID3DDeviceContextState> isolated;
    Ptr<ID3D11Texture2D> textures[3];
    Ptr<ID3D11ShaderResourceView> views[3];
    uint64_t revision=0;
    int width=0,height=0;
    bool allow_context_state=true;
    const char* failure_reason="none";
    char failure_details[160]{};
    bool resource_failure(const char* operation,HRESULT result);
public:
    explicit Compositor(bool allow=true):allow_context_state(allow){}
    const char* failure() const noexcept {return failure_reason;}
    bool isolated_state() const noexcept {return isolated!=nullptr;}
    void reset() noexcept;
    bool prepare(ID3D11DeviceContext* context,const Frame& frame);
    bool draw(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,const Frame& frame,uint32_t regions);
};
void configure(HMODULE module,const DS2FX_Api* host);
bool attach();
void detach();
void publish(std::shared_ptr<const Frame> frame);
void clear();
}
