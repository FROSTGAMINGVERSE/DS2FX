#include "../../src/common.h"
#include <tlhelp32.h>
#include <array>
#include <chrono>
#include <iostream>
#include <optional>
#include <thread>

// External read-only research tool. It never writes to DarkSoulsII.exe and
// refuses a process without the supported executable hash and offline Core.
namespace {
constexpr uintptr_t base_a_rva=0x16148f0; // Supported Scholar SHA-256 only.
struct Process {
    DWORD pid=0; uintptr_t image=0; bool offline_core=false,seamless=false;
};
std::optional<Process> find_game() {
    HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);
    if(snapshot==INVALID_HANDLE_VALUE) return std::nullopt;
    PROCESSENTRY32W process{};process.dwSize=sizeof(process);
    Process found{};
    if(Process32FirstW(snapshot,&process)) do {
        if(_wcsicmp(process.szExeFile,L"DarkSoulsII.exe")==0) { found.pid=process.th32ProcessID;break; }
    } while(Process32NextW(snapshot,&process));
    CloseHandle(snapshot);
    if(!found.pid) return std::nullopt;
    snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,found.pid);
    if(snapshot==INVALID_HANDLE_VALUE) return std::nullopt;
    MODULEENTRY32W module{};module.dwSize=sizeof(module);
    if(Module32FirstW(snapshot,&module)) do {
        if(_wcsicmp(module.szModule,L"DarkSoulsII.exe")==0) found.image=reinterpret_cast<uintptr_t>(module.modBaseAddr);
        if(_wcsicmp(module.szModule,L"ds2fx_core.dll")==0) found.offline_core=true;
        if(_wcsicmp(module.szModule,L"ds2sc.dll")==0 || _wcsicmp(module.szModule,L"ds2fx_bridge_core.dll")==0) found.seamless=true;
    } while(Module32NextW(snapshot,&module));
    CloseHandle(snapshot);
    return found.image ? std::optional<Process>(found) : std::nullopt;
}
template<class T> bool read(HANDLE process,uintptr_t address,T& value) {
    SIZE_T count=0;
    return ReadProcessMemory(process,reinterpret_cast<void*>(address),&value,sizeof(T),&count) && count==sizeof(T);
}
template<size_t N> bool read_words(HANDLE process,uintptr_t address,std::array<int32_t,N>& words) {
    SIZE_T count=0;
    return ReadProcessMemory(process,reinterpret_cast<void*>(address),words.data(),sizeof(words),&count) && count==sizeof(words);
}
template<size_t N> void changes(std::ostream& output,const char* region,uintptr_t offset,
                                const std::array<int32_t,N>& before,const std::array<int32_t,N>& after,
                                uint64_t elapsed) {
    for(size_t i=0;i<N;++i) if(before[i]!=after[i])
        output<<elapsed<<','<<region<<",0x"<<std::hex<<offset+i*4<<std::dec<<','<<before[i]<<','<<after[i]<<'\n';
}
}
int wmain(int argc,wchar_t** argv) { try {
    fx::require(argc==2,"Usage: ds2fx_hud_probe.exe <absolute-output.csv>");
    const auto game=find_game();
    fx::require(game.has_value(),"DarkSoulsII.exe is not running");
    fx::require(game->offline_core && !game->seamless,"Probe requires offline DS2FX Core and refuses Seamless sessions");
    HANDLE handle=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,game->pid);
    fx::require(handle!=nullptr,"Cannot open game for read-only observation");
    fx::Handle process(handle);
    std::array<wchar_t,32768> path{};DWORD length=static_cast<DWORD>(path.size());
    fx::require(QueryFullProcessImageNameW(handle,0,path.data(),&length)!=0,"Cannot identify game executable");
    const fx::fs::path executable(std::wstring(path.data(),length));
    fx::require(fx::hash_file(executable)==fx::game_hash,"Unsupported Scholar executable hash");
    const auto output_path=fx::fs::absolute(fx::fs::path(argv[1]));
    fx::fs::create_directories(output_path.parent_path());
    fx::require(!fx::fs::exists(output_path),"Output already exists; use a fresh CSV path");
    std::ofstream output(output_path,std::ios::binary);
    fx::require(output.good(),"Cannot create output CSV");
    output<<"elapsed_ms,region,offset,old_dec,new_dec\n";
    std::cout<<"Read-only offline HUD probe started for PID "<<game->pid<<". Cycle one D-pad direction at a time for 90 seconds.\n";
    const auto start=GetTickCount64();
    uintptr_t last_player=0,last_equipment=0;
    std::array<int32_t,48> before_player{},current_player{};
    std::array<int32_t,76> before_equipment{},current_equipment{};
    bool have_baseline=false;
    while(GetTickCount64()-start<90000) {
        uintptr_t base=0,player=0,equipment=0;
        int32_t hp=0,max_hp=0;
        const bool ready=read(handle,game->image+base_a_rva,base) && base &&
            read(handle,base+0xd0,player) && player &&
            read(handle,player+0x168,hp) && read(handle,player+0x170,max_hp) &&
            hp>=0 && max_hp>0 && hp<=max_hp && max_hp<=1000000 &&
            read(handle,player+0x378,equipment) && equipment &&
            read_words(handle,player+0xa80,current_player) &&
            read_words(handle,equipment+0x4d0,current_equipment);
        if(ready) {
            const auto elapsed=GetTickCount64()-start;
            if(!have_baseline || player!=last_player || equipment!=last_equipment) {
                output<<elapsed<<",OWNER,0x0,"<<last_player<<','<<player<<'\n';
                before_player=current_player;before_equipment=current_equipment;
                last_player=player;last_equipment=equipment;have_baseline=true;
            } else {
                changes(output,"PlayerCtrl",0xa80,before_player,current_player,elapsed);
                changes(output,"Equipment",0x4d0,before_equipment,current_equipment,elapsed);
                before_player=current_player;before_equipment=current_equipment;
            }
            output.flush();
        } else have_baseline=false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout<<"Probe complete: "<<output_path.string()<<'\n';
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; } }
