#include "../../sdk/ds2fx_api.h"
#include <windows.h>
#include <filesystem>
#include <cstdio>
namespace { HMODULE self=nullptr; void event(uint32_t,const DS2FX_PlayerState*) {} }
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* api,DS2FX_Plugin* output) {
    if(!api || api->version!=DS2FX_API_VERSION || api->size<sizeof(DS2FX_Api) || !api->register_stamina_cost_policy ||
       !output || output->size!=sizeof(DS2FX_Plugin)) return 0;
    wchar_t path[32768]; const auto n=GetModuleFileNameW(self,path,32768); if(!n || n>=32768) return 0;
    auto config=std::filesystem::path(path).parent_path()/L"training.ini";
    wchar_t mode[32]; GetPrivateProfileStringW(L"Training",L"Mode",L"observe",mode,32,config.c_str());
    const bool train=wcscmp(mode,L"training")==0;
    if(!train && wcscmp(mode,L"observe")!=0) { api->log("frost-training-stamina","Mode must be observe or training"); return 0; }
    const auto percent=GetPrivateProfileIntW(L"Training",L"CostPercent",50,config.c_str());
    if(percent<10 || percent>100 || !api->register_stamina_cost_policy(train ? percent : 100)) {
        api->log("frost-training-stamina","Invalid percentage or another module owns stamina policy"); return 0;
    }
    *output={sizeof(DS2FX_Plugin),DS2FX_API_VERSION,"frost-training-stamina","0.1.0",event};
    char message[180]; std::snprintf(message,sizeof(message),"Mode=%s cost_percent=%u; only positive local-player reason=0 costs; engine call-site filter",train?"training":"observe",train?percent:100);
    api->log("frost-training-stamina",message); return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) { if(reason==DLL_PROCESS_ATTACH){self=module;DisableThreadLibraryCalls(module);} return TRUE; }
