#include <windows.h>
#include "../../sdk/ds2fx_api.h"
#include <atomic>
namespace {std::atomic<bool> initialized{false};void event(uint32_t,const DS2FX_PlayerState*) {}}
// Identity-only module. Core owns the reusable item, model, flame and lighting.
// No overlay, charge ledger, timer, input or native UI hooks are created.
DS2FX_EXPORT uint32_t DS2FX_InitializePlugin(const DS2FX_Api* api,DS2FX_Plugin* output) {
    if(!api||api->size<sizeof(DS2FX_Api)||api->version!=DS2FX_API_VERSION||!api->log||!output||output->size!=sizeof(DS2FX_Plugin)||initialized.exchange(true))return 0;
    *output={sizeof(DS2FX_Plugin),DS2FX_API_VERSION,"wayfarers-lantern","0.4.0",event};
    api->log("wayfarers-lantern","Reusable on/off Lantern ready; Core owns wick flame and character light; no charges or HUD");return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(module);return TRUE;}
