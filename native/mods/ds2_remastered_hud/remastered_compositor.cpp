#include "remastered_ingame.h"
#include <d3dcompiler.h>
#include <cstring>
#include <optional>
#include <cstdio>
#include "remastered_pipeline_state.h"

namespace remastered::ingame {
bool Compositor::resource_failure(const char* operation,HRESULT result) {
    std::snprintf(failure_details,sizeof(failure_details),"%s failed HRESULT=0x%08lx",operation,static_cast<unsigned long>(result));
    failure_reason=failure_details;return false;
}
void Compositor::reset() noexcept {
    for(auto& p:views)p.Reset();for(auto& p:textures)p.Reset();
    isolated.Reset();sampler.Reset();raster.Reset();blend.Reset();depth.Reset();constants.Reset();ps.Reset();vs.Reset();device.Reset();
    revision=0;width=height=0;
}
bool Compositor::prepare(ID3D11DeviceContext* context,const Frame& frame) {
    failure_reason="context unavailable or not immediate";
    if(!context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    Ptr<ID3D11Device> actual;context->GetDevice(&actual);
    if(actual.Get()!=device.Get())reset();
    if(!device) {
        device=actual;
        Ptr<ID3D11Device1> extended;
        if(!device){failure_reason="context returned no device";return false;}
        const D3D_FEATURE_LEVEL levels[]{D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0};
        D3D_FEATURE_LEVEL chosen{};
        if(allow_context_state && SUCCEEDED(device.As(&extended)))
            extended->CreateDeviceContextState(0,levels,2,D3D11_SDK_VERSION,__uuidof(ID3D11Device),&chosen,&isolated);
        constexpr char shader[]=R"(
cbuffer Placement : register(b0) { float4 rect; float4 options; };
struct Vertex { float4 position:SV_Position; float2 uv:TEXCOORD0; };
Vertex vertex(uint id:SV_VertexID) {
    float2 uv=float2(id&1,id>>1);
    Vertex v;v.uv=uv;float2 p=rect.xy+uv*rect.zw;
    v.position=float4(p.x*2-1,1-p.y*2,0,1);return v;
}
Texture2D artwork:register(t0); SamplerState linearClamp:register(s0);
float4 pixel(Vertex v):SV_Target { return artwork.Sample(linearClamp,v.uv).bgra*options.x; }
)";
        Ptr<ID3DBlob> vertex,pixel,errors;
        failure_reason="shader compile/create failed";
        if(FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"vertex","vs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&vertex,&errors)) ||
           FAILED(D3DCompile(shader,sizeof(shader)-1,nullptr,nullptr,nullptr,"pixel","ps_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&pixel,&errors)) ||
           FAILED(device->CreateVertexShader(vertex->GetBufferPointer(),vertex->GetBufferSize(),nullptr,&vs)) ||
           FAILED(device->CreatePixelShader(pixel->GetBufferPointer(),pixel->GetBufferSize(),nullptr,&ps))) {reset();return false;}
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=32;buffer.Usage=D3D11_USAGE_DEFAULT;buffer.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        D3D11_BLEND_DESC blending{};auto& b=blending.RenderTarget[0];
        b.BlendEnable=TRUE;b.SrcBlend=b.SrcBlendAlpha=D3D11_BLEND_ONE;
        b.DestBlend=b.DestBlendAlpha=D3D11_BLEND_INV_SRC_ALPHA;b.BlendOp=b.BlendOpAlpha=D3D11_BLEND_OP_ADD;
        b.RenderTargetWriteMask=D3D11_COLOR_WRITE_ENABLE_ALL;
        D3D11_RASTERIZER_DESC r{};r.FillMode=D3D11_FILL_SOLID;r.CullMode=D3D11_CULL_NONE;r.DepthClipEnable=TRUE;
        D3D11_SAMPLER_DESC s{};s.Filter=D3D11_FILTER_MIN_MAG_MIP_LINEAR;s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_CLAMP;s.MaxLOD=D3D11_FLOAT32_MAX;
        D3D11_DEPTH_STENCIL_DESC d{};d.DepthEnable=FALSE;d.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;d.DepthFunc=D3D11_COMPARISON_ALWAYS;
        failure_reason="pipeline resource creation failed";
        if(FAILED(device->CreateBuffer(&buffer,nullptr,&constants)) || FAILED(device->CreateBlendState(&blending,&blend)) ||
           FAILED(device->CreateDepthStencilState(&d,&depth)) || FAILED(device->CreateRasterizerState(&r,&raster)) || FAILED(device->CreateSamplerState(&s,&sampler))) {reset();return false;}
    }
    if(frame.blank)return SUCCEEDED(device->GetDeviceRemovedReason());
    failure_reason="invalid frame dimensions or pixels";
    if(frame.width<=0 || frame.height<=0 || uint64_t(frame.width)*frame.height>16777216 ||
       frame.canvas_width<=0 || frame.canvas_height<=0)return false;
    if(frame.width!=width || frame.height!=height) {
        for(auto& p:views)p.Reset();for(auto& p:textures)p.Reset();
        width=frame.width;height=frame.height;revision=0;
    }
    constexpr uint32_t bits[]{DS2FX_HUD_REGION_COVENANT,DS2FX_HUD_REGION_BARS,DS2FX_HUD_REGION_EFFECTS};
    for(unsigned i=0;i<3;++i)if(frame.regions&bits[i]) {
        if(frame.pixels[i].size()!=size_t(width)*height*4)return false;
        const bool create=!textures[i] || !views[i];
        if(create) {
            views[i].Reset();textures[i].Reset();
            D3D11_TEXTURE2D_DESC t{};t.Width=width;t.Height=height;t.MipLevels=t.ArraySize=1;
            // Keep the CPU's BGRA bytes; the shader swizzles them after sampling.
            // RGBA8 and an explicit SRV avoid optional BGRA/default-view paths.
            t.Format=DXGI_FORMAT_R8G8B8A8_UNORM;t.SampleDesc.Count=1;t.Usage=D3D11_USAGE_DEFAULT;t.BindFlags=D3D11_BIND_SHADER_RESOURCE;
            auto result=device->CreateTexture2D(&t,nullptr,&textures[i]);
            if(FAILED(result))return resource_failure("artwork CreateTexture2D RGBA8",result);
            D3D11_SHADER_RESOURCE_VIEW_DESC v{};v.Format=t.Format;v.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;v.Texture2D.MipLevels=1;
            result=device->CreateShaderResourceView(textures[i].Get(),&v,&views[i]);
            if(FAILED(result)){textures[i].Reset();return resource_failure("artwork CreateShaderResourceView RGBA8",result);}
        }
        if(create || frame.revision!=revision)context->UpdateSubresource(textures[i].Get(),0,nullptr,frame.pixels[i].data(),width*4,0);
    }
    revision=frame.revision;
    return SUCCEEDED(device->GetDeviceRemovedReason());
}
bool Compositor::draw(ID3D11DeviceContext* context,ID3D11RenderTargetView* target,const Frame& frame,uint32_t regions) {
    failure_reason="draw context, target or bars grant unavailable";
    if(!device || !context || !target || !(regions&DS2FX_HUD_REGION_BARS))return false;
    if(frame.blank)return SUCCEEDED(device->GetDeviceRemovedReason());
    Ptr<ID3D11DeviceContext1> extended;
    if(isolated && FAILED(context->QueryInterface(IID_PPV_ARGS(&extended))))isolated.Reset();
    Ptr<ID3D11Resource> resource;target->GetResource(&resource);
    Ptr<ID3D11Texture2D> output;const auto texture_result=resource.As(&output);
    if(FAILED(texture_result))return resource_failure("RTV resource texture interface",texture_result);
    D3D11_TEXTURE2D_DESC dimensions{};output->GetDesc(&dimensions);
    Ptr<ID3D11Device> target_device;output->GetDevice(&target_device);
    D3D11_RENDER_TARGET_VIEW_DESC view{};target->GetDesc(&view);
    if(dimensions.SampleDesc.Count!=1 ||
       (view.Format!=DXGI_FORMAT_R8G8B8A8_UNORM && view.Format!=DXGI_FORMAT_B8G8R8A8_UNORM && view.Format!=DXGI_FORMAT_R10G10B10A2_UNORM)){
        std::snprintf(failure_details,sizeof(failure_details),"draw descriptor rejected view=%u texture=%u samples=%u",view.Format,dimensions.Format,dimensions.SampleDesc.Count);
        failure_reason=failure_details;return false;
    }
    Ptr<ID3D11RenderTargetView> validated;
    if(target_device.Get()!=device.Get()) {
        // A wrapper may return an underlying device through Resource::GetDevice.
        // Let the actual context device validate ownership instead of comparing
        // proxy addresses. Foreign-device resources fail this API validation.
        const auto result=device->CreateRenderTargetView(output.Get(),&view,&validated);
        if(FAILED(result))return resource_failure("context device target ownership",result);
        target=validated.Get();
    }
    // A private D3D11.1 context state preserves every shader, resource, UAV,
    // constant buffer, IA/SO binding, viewport, scissor, OM and predication state.
    std::optional<PipelineState> legacy;
    if(!isolated) {
        failure_reason="legacy context has active stream output or pixel UAVs";
        if(!PipelineState::supported(context))return false;
        legacy.emplace(context);legacy->applied=true;
    }
    Ptr<ID3DDeviceContextState> previous;
    if(isolated)extended->SwapDeviceContextState(isolated.Get(),&previous);
    struct Restore {ID3D11DeviceContext1* context;ID3DDeviceContextState* state;~Restore(){if(context){context->ClearState();context->SwapDeviceContextState(state,nullptr);}}} restore{isolated?extended.Get():nullptr,previous.Get()};
    if(isolated)context->ClearState();
    context->HSSetShader(nullptr,nullptr,0);context->DSSetShader(nullptr,nullptr,0);context->GSSetShader(nullptr,nullptr,0);
    context->IASetInputLayout(nullptr);context->SetPredication(nullptr,FALSE);
    const float placement[]{float(frame.x)/frame.canvas_width,float(frame.y)/frame.canvas_height,
        float(frame.width)/frame.canvas_width,float(frame.height)/frame.canvas_height,frame.opacity/255.f,0,0,0};
    context->UpdateSubresource(constants.Get(),0,nullptr,placement,0,0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    context->VSSetShader(vs.Get(),nullptr,0);context->PSSetShader(ps.Get(),nullptr,0);
    ID3D11Buffer* buffer=constants.Get();context->VSSetConstantBuffers(0,1,&buffer);context->PSSetConstantBuffers(0,1,&buffer);
    ID3D11SamplerState* sample=sampler.Get();context->PSSetSamplers(0,1,&sample);
    context->RSSetState(raster.Get());
    const D3D11_VIEWPORT viewport{0,0,float(dimensions.Width),float(dimensions.Height),0,1};context->RSSetViewports(1,&viewport);
    context->OMSetRenderTargets(1,&target,nullptr);context->OMSetBlendState(blend.Get(),nullptr,0xffffffff);
    context->OMSetDepthStencilState(depth.Get(),0);
    // ClearState's default depth state has depth enabled, but no DSV is bound.
    constexpr uint32_t bits[]{DS2FX_HUD_REGION_COVENANT,DS2FX_HUD_REGION_BARS,DS2FX_HUD_REGION_EFFECTS};
    for(unsigned i=0;i<3;++i)if(regions&bits[i]) {
        ID3D11ShaderResourceView* image=views[i].Get();if(!image){failure_reason="draw artwork view unavailable";return false;}
        context->PSSetShaderResources(0,1,&image);context->Draw(4,0);
    }
    ID3D11ShaderResourceView* empty=nullptr;context->PSSetShaderResources(0,1,&empty);
    return SUCCEEDED(device->GetDeviceRemovedReason());
}
}
