#include <winsock2.h>
#include "common.h"
#include "offline_network.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
namespace fx::offline_network {
namespace {
std::atomic<uint64_t> blocked{0};
bool installed=false;
void* original=nullptr;
int WSAAPI refuse_startup(WORD,LPWSADATA) noexcept {
    ++blocked;
    return WSASYSNOTREADY;
}
}
void install() {
    fx::require(!installed,"Offline network guard already installed");
    auto library=LoadLibraryExW(L"ws2_32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
    fx::require(library!=nullptr,"Cannot load Windows sockets for offline guard");
    auto address=GetProcAddress(library,"WSAStartup");
    fx::require(address!=nullptr,"Cannot resolve WSAStartup for offline guard");
    const auto ready=MH_Initialize();
    fx::require(ready==MH_OK||ready==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize offline guard backend");
    fx::require(MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&refuse_startup),&original)==MH_OK,"Cannot prepare offline network guard");
    fx::require(MH_EnableHook(reinterpret_cast<void*>(address))==MH_OK,"Cannot enable offline network guard");
    installed=true;
}
uint64_t blocked_calls() noexcept { return blocked.load(); }
}
