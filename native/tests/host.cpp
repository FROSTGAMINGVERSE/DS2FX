#include "../src/common.h"
#include <array>
#include <cstring>
namespace { std::array<unsigned char,0x200> base{},player{}; uintptr_t slot = 0; }
extern "C" __declspec(dllexport) uintptr_t DS2FX_TestBaseSlot() { return reinterpret_cast<uintptr_t>(&slot); }
extern "C" __declspec(dllexport) __declspec(noinline) void DS2FX_TestStaminaCost(void* controller,float cost,uint32_t reason) {
    auto actor=*reinterpret_cast<uintptr_t*>(controller);
    if(reason==0) *reinterpret_cast<float*>(actor+0x1ac)-=cost;
}
int main() {
#ifdef DS2FX_BRIDGE_HOST
    if(!LoadLibraryW((fx::module_path().parent_path()/"SeamlessCoop"/"ds2sc.dll").c_str())) return 9;
    Sleep(4500); // Let the bridge attach after normal process startup.
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
