#include "../src/common.h"
#include "../sdk/ds2fx_api.h"
#include "../sdk/ds2fx_lantern_api.h"
#include <iostream>
#include <cstring>
namespace {
void log(const char*,const char* message){std::cout<<message<<'\n';}
uint32_t query(DS2FX_PlayerState*){return 0;}
}
int wmain(int argc,wchar_t** argv){try{
    fx::require(argc==4,"Expected production Core DLL, plugin DLL and assets paths");
    // Check the actual linked DLL rather than accepting a partially rebuilt
    // artifact whose bootstrap is newer than its observation/coordinator code.
    std::ifstream payload(argv[1],std::ios::binary);
    const std::string bytes{std::istreambuf_iterator<char>(payload),std::istreambuf_iterator<char>()};
    for(const char* marker:{fx::package_version,"flame_created=","lantern_mode=simple-toggle native_ui_hooks=0"})
        fx::require(bytes.find(marker)!=std::string::npos,"Production Core lacks final version/presentation diagnostics");
    const auto native_ui_name="DS2FX_HasNativeLanternQuickslot";
    fx::require(bytes.find(native_ui_name)!=std::string::npos,"Production native quick-slot export missing");
    // Core is loaded but Bootstrap is never called: no native game hooks/actions.
    const auto core=LoadLibraryW(argv[1]);fx::require(core!=nullptr,"Production Core load failed");
    const auto charges=reinterpret_cast<DS2FX_GetLanternChargesFn>(GetProcAddress(core,"DS2FX_GetLanternCharges"));
    fx::require(charges!=nullptr,"Production charge export missing");
    DS2FX_LanternCharges state{sizeof(state),DS2FX_LANTERN_CHARGES_VERSION};
    fx::require(charges(&state)==0,"Unbooted Core cannot fabricate a resource");
    const auto root=fx::fs::temp_directory_path()/(L"DS2FX-LanternPlugin-"+std::to_wstring(GetCurrentProcessId()));
    fx::fs::create_directories(root/L"assets");
    fx::fs::copy_file(argv[2],root/L"plugin.dll");
    for(auto name:{L"hud-frame.png",L"lantern-lit.png"})fx::fs::copy_file(fx::fs::path(argv[3])/name,root/L"assets"/name);
    fx::require(!fx::fs::exists(root/L"assets"/L"oil-flask.png"),"Smoke fixture must have no Oil asset");
    const auto foreground=GetForegroundWindow();
    const auto module=LoadLibraryW((root/L"plugin.dll").c_str());fx::require(module!=nullptr,"Production plugin load failed");
    const auto initialize=reinterpret_cast<DS2FX_InitializePluginFn>(GetProcAddress(module,"DS2FX_InitializePlugin"));
    fx::require(initialize!=nullptr,"Production plugin init export missing");
    DS2FX_Api api{sizeof(api),DS2FX_API_VERSION,fx::version,log,query,nullptr};DS2FX_Plugin plugin{sizeof(plugin)};
    fx::require(initialize(&api,&plugin)==1,"Production simple-toggle module initializes");
    fx::require(std::strcmp(plugin.id,"wayfarers-lantern")==0&&std::strcmp(plugin.version,"0.4.0")==0,"Production identity must match the staged manifest");
    DS2FX_PlayerState player{sizeof(player),1,1,100,100,1};plugin.on_event(DS2FX_TICK,&player);
    Sleep(250);fx::require(GetForegroundWindow()==foreground,"Hidden HUD must not take foreground");
    // Resident worker modules are intentionally not unloaded while they run.
    std::cout<<"PASS actual production DLL load/export/identity/no-HUD initialization; no Bootstrap or game integration\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
