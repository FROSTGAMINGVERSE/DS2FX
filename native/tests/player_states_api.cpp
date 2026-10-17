#include "../src/common.h"
#include "../sdk/ds2fx_api.h"
#include <iostream>
namespace { uintptr_t slot=0; }
extern "C" __declspec(dllexport) uintptr_t DS2FX_TestBaseSlot() { return reinterpret_cast<uintptr_t>(&slot); }
int wmain(int argc,wchar_t** argv) {
    HMODULE core=nullptr;
    try {
        fx::require(argc==2,"Usage: fixture host <staged core DLL>");
        core=LoadLibraryW(argv[1]);fx::require(core!=nullptr,"fixture Core loading");
        auto bootstrap=reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(core,"DS2FX_Bootstrap"));
        auto stop=reinterpret_cast<DWORD(WINAPI*)(void*)>(GetProcAddress(core,"DS2FX_Stop"));
        fx::require(bootstrap && stop && bootstrap(nullptr)==1,"fixture bootstrap");
        auto plugin=GetModuleHandleW(L"plugin.dll");fx::require(plugin!=nullptr,"fixture plugin loaded");
        auto query=reinterpret_cast<const DS2FX_Api*(*)()>(GetProcAddress(plugin,"DS2FX_TestStateApi"));
        auto owner=reinterpret_cast<uint32_t(*)()>(GetProcAddress(plugin,"DS2FX_TestStateOwner"));
        fx::require(query && owner && owner()==1,"module-owned registration, committed routes, honest capability, init-only rejection");
        const auto api=query();
        fx::require(api && api->size==128,"ABI 2 prefix and appended state tails");
        DS2FX_StateServiceStatus status{sizeof(status)};
        fx::require(api->get_player_state_service(&status)==1 && status.registered_states==0,
                    "foreign caller cannot observe another owner's registration");
        const DS2FX_StateDefinition d{sizeof(d),1,DS2FX_STATE_VERTICAL_JUMP,948,2500,0};
        fx::require(api->register_player_state(&d)==0,"host cannot register as plugin");
        const DS2FX_StateRequest r{sizeof(r),1,1,100,0};
        fx::require(api->request_player_state(&r)==DS2FX_STATE_INVALID,"host cannot request stolen plugin handle");
        fx::require(stop(nullptr)==1,"fixture stop");
        fx::require(api->get_player_state_service(&status)==0,"stopped runtime revokes API");
        std::cout<<"PASS owned runtime DLL fixture: API tail, module ownership, committed jump routes, unavailable executor and shutdown. Not a gameplay test.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
