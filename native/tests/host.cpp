#include "../src/common.h"
#include <array>
#include <atomic>
#include <cstring>
namespace { std::array<unsigned char,0x200> base{},player{}; uintptr_t slot = 0; }
#ifdef DS2FX_BRIDGE_HOST
// Match the production DLString ABI without linking the host to the hook
// implementation. Calls go through these exported functions and the actual
// Core detour, rather than a second copy of the router in the fixture host.
struct FixtureGameString { wchar_t* data; void* allocator; uint64_t length,capacity; };
static_assert(sizeof(FixtureGameString)==32);
namespace { std::atomic<unsigned> resolver_calls{0}; bool content_at_crt=false; }
extern "C" __declspec(dllexport) __declspec(noinline) uintptr_t DS2FX_TestResolve(uintptr_t context,FixtureGameString*) {
    resolver_calls.fetch_add(1,std::memory_order_relaxed);return context+17;
}
extern "C" __declspec(dllexport) void DS2FX_TestGrow(FixtureGameString* path,size_t capacity,size_t) {
    auto data=new wchar_t[capacity];std::copy_n(path->data,path->length+1,data);
    delete[] path->data;path->data=data;path->capacity=capacity;
}
namespace {
bool probe_content() {
    const auto name=L"gamedata:/Param/preentry.param";const size_t count=wcslen(name);
    FixtureGameString path{new wchar_t[count+1],nullptr,count,count+1};std::copy_n(name,count+1,path.data);
    const auto before=resolver_calls.load(std::memory_order_relaxed);
    auto volatile resolver=&DS2FX_TestResolve;const auto result=resolver(25,&path);
    const std::wstring routed(path.data,path.length);delete[] path.data;
    const auto expected=L"./Content/vanilla-adp/assets/Param/preentry.param";
    if(result!=42 || resolver_calls.load(std::memory_order_relaxed)!=before+1 || routed!=expected)return false;
    std::ifstream file(fx::module_path().parent_path()/"DS2FX"/fx::fs::path(routed.substr(2)),std::ios::binary);
    const std::string body{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    return body=="owned pre-entry content fixture\n";
}
bool runtime_content_ready() {
    wchar_t local[32768]{};const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    if(!count || count>=32768)return false;
    const auto log=fx::fs::path(local)/"DS2FX"/"logs"/("runtime-"+std::to_string(GetCurrentProcessId())+".log");
    for(int attempt=0;attempt<200;++attempt) {
        std::ifstream file(log,std::ios::binary);
        const std::string body{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        if(body.find("Verified pre-entry content routing retained for runtime initialization")!=body.npos)return true;
        Sleep(50);
    }
    return false;
}
struct CrtReceipt {
    CrtReceipt() {
        if(GetModuleHandleW(L"ds2fx_bridge_core.dll")) {
            try{content_at_crt=probe_content();}catch(...){content_at_crt=false;}
            std::ofstream(fx::module_path().parent_path()/"content-at-crt.txt")<<(content_at_crt?"PASS":"FAIL")<<" actual Core content route during host CRT\n";
        }
        std::ofstream(fx::module_path().parent_path()/"crt-ran.txt")<<"owned host CRT started\n";
    }
} crt_receipt;
}
#endif
extern "C" __declspec(dllexport) uintptr_t DS2FX_TestBaseSlot() { return reinterpret_cast<uintptr_t>(&slot); }
extern "C" __declspec(dllexport) __declspec(noinline) void DS2FX_TestStaminaCost(void* controller,float cost,uint32_t reason) {
    auto actor=*reinterpret_cast<uintptr_t*>(controller);
    if(reason==0) *reinterpret_cast<float*>(actor+0x1ac)-=cost;
}
int main() {
#ifdef DS2FX_BRIDGE_HOST
    if(!LoadLibraryW((fx::module_path().parent_path()/"SeamlessCoop"/"ds2sc.dll").c_str())) return 9;
    if(GetModuleHandleW(L"ds2fx_bridge_core.dll") && !content_at_crt)return 10;
    Sleep(4500); // Let the bridge attach after normal process startup.
    if(GetModuleHandleW(L"ds2fx_bridge_core.dll")) {
        const bool retained=runtime_content_ready() && probe_content();
        std::ofstream(fx::module_path().parent_path()/"content-after-bootstrap.txt")<<(retained?"PASS":"FAIL")<<" actual Core content route after runtime bootstrap\n";
        if(!retained)return 11;
    }
#endif
    std::ofstream(fx::module_path().parent_path()/"entrypoint-ran.txt") << "owned fixture reached entrypoint\n";
    // Synthetic pointer layout, deliberately unavailable until after entrypoint.
    Sleep(300);
    uintptr_t p = reinterpret_cast<uintptr_t>(player.data());
    std::memcpy(base.data()+0xd0,&p,sizeof(p));
    int32_t hp = 100, maximum = 120;
    std::memcpy(player.data()+0x168,&hp,4); std::memcpy(player.data()+0x170,&maximum,4);
    float stamina = 90.5f, max_stamina = 100.0f;
    std::memcpy(player.data()+0x1ac,&stamina,4); std::memcpy(player.data()+0x1b4,&max_stamina,4);
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&slot),reinterpret_cast<LONG64>(base.data()));
    Sleep(400);
    hp = 75; InterlockedExchange(reinterpret_cast<volatile LONG*>(player.data()+0x168),hp);
    stamina = 40.25f; std::memcpy(player.data()+0x1ac,&stamina,4);
    Sleep(400);
    auto volatile cost_function=&DS2FX_TestStaminaCost;
    cost_function(&p,20,0);
    Sleep(400);
    hp = 0; InterlockedExchange(reinterpret_cast<volatile LONG*>(player.data()+0x168),hp);
    Sleep(400);
    hp = 120; InterlockedExchange(reinterpret_cast<volatile LONG*>(player.data()+0x168),hp);
    Sleep(400);
    InterlockedExchange64(reinterpret_cast<volatile LONG64*>(&slot),0);
    Sleep(400);
#ifdef DS2FX_BRIDGE_HOST
    const wchar_t* core_name=L"ds2fx_bridge_core.dll";
#else
    const wchar_t* core_name=L"ds2fx_core.dll";
#endif
    auto stop = reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(GetModuleHandleW(core_name),"DS2FX_Stop"));
    if (!stop || stop(nullptr) != 1) return 3;
    return 0;
}
