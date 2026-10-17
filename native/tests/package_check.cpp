#include "../src/compatibility.h"
#include "../src/content_pack.h"
#include <iostream>
int wmain(int argc,wchar_t** argv){try {
    fx::require(argc==2,"Supply the staged DS2FX directory");
    const auto modules=fx::discover(fx::fs::path(argv[1])/"Plugins");const auto assets=fx::content::load(argv[1]);
    std::cout<<"PASS production content selection: "<<modules.size()<<" modules, "<<assets.size()<<" hash-verified assets\n";
    for(const auto& asset:assets)std::wcout<<asset.key<<L"\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
