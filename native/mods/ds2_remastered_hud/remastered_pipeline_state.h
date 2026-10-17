#pragma once
#include <d3d11.h>
#include <array>

namespace remastered::ingame {
// Base D3D11 path for translation devices which do not expose ContextState.
// Touch only the graphics bindings below; never ClearState on this path.
// All acquired COM references are released before returning from Present.
struct PipelineState {
    ID3D11DeviceContext* context;
    ID3D11VertexShader* vs=nullptr; ID3D11PixelShader* ps=nullptr;
    ID3D11HullShader* hs=nullptr; ID3D11DomainShader* ds=nullptr; ID3D11GeometryShader* gs=nullptr;
    std::array<ID3D11ClassInstance*,256> vc{},pc{},hc{},dc{},gc{};
    UINT vn=256,pn=256,hn=256,dn=256,gn=256;
    ID3D11Buffer* vb=nullptr; ID3D11Buffer* pb=nullptr;
    ID3D11SamplerState* sampler=nullptr;
    std::array<ID3D11ShaderResourceView*,128> vr{},pr{},hr{},dr{},gr{},cr{};
    ID3D11BlendState* blend=nullptr; FLOAT factor[4]{}; UINT mask=0;
    ID3D11DepthStencilState* depth=nullptr; UINT stencil=0;
    ID3D11RasterizerState* raster=nullptr;
    D3D11_VIEWPORT viewport[16]{}; UINT viewports=16;
    ID3D11RenderTargetView* targets[8]{}; ID3D11DepthStencilView* dsv=nullptr;
    ID3D11InputLayout* layout=nullptr; D3D11_PRIMITIVE_TOPOLOGY topology{};
    ID3D11Predicate* predicate=nullptr; BOOL predicate_value=FALSE;
    bool applied=false;
    static bool supported(ID3D11DeviceContext* c) {
        // Stream-output offsets and UAV counters have no lossless getter in
        // base D3D11. Refuse those unusual Present states rather than alter them.
        ID3D11Buffer* stream[4]{}; c->SOGetTargets(4,stream);
        bool okay=true; for(auto* p:stream)if(p){okay=false;p->Release();}
        ID3D11UnorderedAccessView* uav[8]{};
        c->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,uav);
        for(auto* p:uav)if(p){okay=false;p->Release();}
        return okay;
    }
    explicit PipelineState(ID3D11DeviceContext* c):context(c) {
        c->VSGetShader(&vs,vc.data(),&vn); c->PSGetShader(&ps,pc.data(),&pn);
        c->HSGetShader(&hs,hc.data(),&hn); c->DSGetShader(&ds,dc.data(),&dn); c->GSGetShader(&gs,gc.data(),&gn);
        c->VSGetConstantBuffers(0,1,&vb); c->PSGetConstantBuffers(0,1,&pb); c->PSGetSamplers(0,1,&sampler);
        c->VSGetShaderResources(0,128,vr.data()); c->PSGetShaderResources(0,128,pr.data());
        c->HSGetShaderResources(0,128,hr.data()); c->DSGetShaderResources(0,128,dr.data());
        c->GSGetShaderResources(0,128,gr.data()); c->CSGetShaderResources(0,128,cr.data());
        c->OMGetBlendState(&blend,factor,&mask); c->OMGetDepthStencilState(&depth,&stencil);
        c->OMGetRenderTargets(8,targets,&dsv); c->RSGetState(&raster); c->RSGetViewports(&viewports,viewport);
        c->IAGetInputLayout(&layout); c->IAGetPrimitiveTopology(&topology); c->GetPredication(&predicate,&predicate_value);
    }
    PipelineState(const PipelineState&)=delete;
    template<class T> static void release(T*& p){if(p){p->Release();p=nullptr;}}
    template<class T,size_t N> static void release(std::array<T*,N>& a){for(auto*& p:a)release(p);}
    ~PipelineState() {
        if(applied) {
            context->OMSetRenderTargets(8,targets,dsv);
            context->VSSetShader(vs,vc.data(),vn); context->PSSetShader(ps,pc.data(),pn);
            context->HSSetShader(hs,hc.data(),hn); context->DSSetShader(ds,dc.data(),dn); context->GSSetShader(gs,gc.data(),gn);
            context->VSSetConstantBuffers(0,1,&vb); context->PSSetConstantBuffers(0,1,&pb); context->PSSetSamplers(0,1,&sampler);
            context->VSSetShaderResources(0,128,vr.data()); context->PSSetShaderResources(0,128,pr.data());
            context->HSSetShaderResources(0,128,hr.data()); context->DSSetShaderResources(0,128,dr.data());
            context->GSSetShaderResources(0,128,gr.data()); context->CSSetShaderResources(0,128,cr.data());
            context->OMSetBlendState(blend,factor,mask); context->OMSetDepthStencilState(depth,stencil);
            context->RSSetState(raster); context->RSSetViewports(viewports,viewport);
            context->IASetInputLayout(layout); context->IASetPrimitiveTopology(topology); context->SetPredication(predicate,predicate_value);
        }
        release(vs);release(ps);release(hs);release(ds);release(gs);
        release(vc);release(pc);release(hc);release(dc);release(gc);
        release(vb);release(pb);release(sampler);
        release(vr);release(pr);release(hr);release(dr);release(gr);release(cr);
        release(blend);release(depth);release(raster);release(dsv);release(layout);release(predicate);
        for(auto*& p:targets)release(p);
    }
};
}
