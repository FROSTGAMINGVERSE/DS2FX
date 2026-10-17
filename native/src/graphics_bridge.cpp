#include "graphics_bridge.h"
#include "common.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <array>
#include <mutex>
namespace fx::graphics {namespace {
using Microsoft::WRL::ComPtr;
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
struct Entry {HMODULE owner=nullptr;uint64_t handle=0;DS2FX_GraphicsCallback callback{};};
std::array<Entry,16> entries{};
std::recursive_mutex mutex;
Present original=nullptr;
void** slot=nullptr;
uint64_t next=1,sequence=0;
bool stopped=false;
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain* chain,UINT interval,UINT flags) {
    if(!(flags&DXGI_PRESENT_TEST)) {
        std::lock_guard lock(mutex);
        if(!stopped) {
            DXGI_SWAP_CHAIN_DESC desc{};DWORD pid=0;
            if(SUCCEEDED(chain->GetDesc(&desc)) && GetWindowThreadProcessId(desc.OutputWindow,&pid) && pid==GetCurrentProcessId()) {
                ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<ID3D11Texture2D> back;
                if(SUCCEEDED(chain->GetDevice(IID_PPV_ARGS(&device))) && SUCCEEDED(chain->GetBuffer(0,IID_PPV_ARGS(&back)))) {
                    device->GetImmediateContext(&context);D3D11_TEXTURE2D_DESC data{};back->GetDesc(&data);
                    if(data.SampleDesc.Count==1 && (data.Format==DXGI_FORMAT_R8G8B8A8_UNORM || data.Format==DXGI_FORMAT_B8G8R8A8_UNORM || data.Format==DXGI_FORMAT_R10G10B10A2_UNORM)) {
                        const DS2FX_GraphicsFrame frame{sizeof(frame),1,desc.OutputWindow,context.Get(),back.Get(),data.Width,data.Height,uint32_t(data.Format),1,++sequence};
                        for(auto& entry:entries)if(entry.handle) {
                            try{entry.callback.present(&frame,entry.callback.user);}catch(...){entry={};}
                        }
                    }
                }
            }
        }
    }
    return original(chain,interval,flags);
}
bool attach() {
    if(slot)return true;
    HWND window=CreateWindowExW(0,L"STATIC",L"DS2FX graphics probe",WS_POPUP,0,0,2,2,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return false;
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=desc.BufferDesc.Height=2;desc.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.BufferCount=1;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<IDXGISwapChain> chain;
    auto result=D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&chain,&device,nullptr,nullptr);
    if(SUCCEEDED(result)) {
        auto candidate=&(*reinterpret_cast<void***>(chain.Get()))[8];HMODULE owner=nullptr,table_owner=nullptr;
        wchar_t system[MAX_PATH]{};GetSystemDirectoryW(system,MAX_PATH);
        const auto expected=std::filesystem::path(system)/L"dxgi.dll";
        const auto module_at=[](void* address,HMODULE* output){return GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),output);};
        if(module_at(*candidate,&owner) && module_at(candidate,&table_owner) && owner==table_owner && _wcsicmp(module_path(owner).c_str(),expected.c_str())==0) {
            DWORD protect=0;
            if(VirtualProtect(candidate,sizeof(void*),PAGE_READWRITE,&protect)) {
                original=reinterpret_cast<Present>(*candidate);slot=candidate;
                InterlockedExchangePointer(candidate,reinterpret_cast<void*>(present));
                DWORD ignored=0;VirtualProtect(candidate,sizeof(void*),protect,&ignored);
            }
        }
    }
    chain.Reset();device.Reset();DestroyWindow(window);return slot!=nullptr;
}
}
uint64_t subscribe(HMODULE owner,const DS2FX_GraphicsCallback* callback) {
    std::lock_guard lock(mutex);
    if(stopped || !owner || !callback || callback->size!=sizeof(*callback) || callback->version!=1 || !callback->present)return 0;
    HMODULE code=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(callback->present),&code) || code!=owner)return 0;
    for(auto& entry:entries)if(!entry.handle) {
        if(!attach())return 0;
        entry={owner,next++,*callback};return entry.handle;
    }
    return 0;
}
uint32_t unsubscribe(HMODULE owner,uint64_t handle) {
    std::lock_guard lock(mutex);for(auto& entry:entries)if(handle && entry.handle==handle && entry.owner==owner){entry={};return 1;}return 0;
}
void shutdown() {
    std::lock_guard lock(mutex);stopped=true;entries={};
    if(slot) {
        DWORD protect=0;if(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protect)) {
            InterlockedCompareExchangePointer(slot,reinterpret_cast<void*>(original),reinterpret_cast<void*>(present));
            DWORD ignored=0;VirtualProtect(slot,sizeof(void*),protect,&ignored);
        }
    }
}
}
