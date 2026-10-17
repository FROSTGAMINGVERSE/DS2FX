#include "seamless_contract.h"
#include "content_pack.h"
#ifdef DS2FX_NUCLEUS
#include "nucleus_context.h"
#endif
#include <tlhelp32.h>
#include <iostream>

namespace {
std::ofstream journal;
void log(const std::string& s) { std::cout << s << std::endl; if(journal) { journal<<s<<'\n'; journal.flush(); } }
uint64_t creation(HANDLE process) {
    FILETIME c{},e{},k{},u{};
    fx::require(GetProcessTimes(process,&c,&e,&k,&u)!=0,"Cannot check process creation time");
    return (uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime;
}
fx::fs::path image_path(HANDLE process) {
    wchar_t buffer[32768]; DWORD size=32768;
    fx::require(QueryFullProcessImageNameW(process,0,buffer,&size)!=0,"Cannot check process image");
    return std::wstring(buffer,size);
}
std::vector<PROCESSENTRY32W> processes() {
    fx::Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0));
    fx::require(snapshot.value!=INVALID_HANDLE_VALUE,"Cannot enumerate processes");
    PROCESSENTRY32W p{}; p.dwSize=sizeof(p);
    std::vector<PROCESSENTRY32W> list;
    if(Process32FirstW(snapshot.value,&p)) do { list.push_back(p); } while(Process32NextW(snapshot.value,&p));
    return list;
}
struct Module { uintptr_t base=0; fx::fs::path path; };
Module module(DWORD pid,const wchar_t* name) {
    fx::Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,pid));
    if(snapshot.value==INVALID_HANDLE_VALUE) return {};
    MODULEENTRY32W m{}; m.dwSize=sizeof(m);
    if(Module32FirstW(snapshot.value,&m)) do {
        if(_wcsicmp(m.szModule,name)==0) return {reinterpret_cast<uintptr_t>(m.modBaseAddr),m.szExePath};
    } while(Module32NextW(snapshot.value,&m));
    return {};
}
uintptr_t remote_load_library(DWORD pid) {
    auto function=GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"LoadLibraryW"); HMODULE owner=nullptr;
    fx::require(function && GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(function),&owner),"Cannot find Windows loader");
    auto remote=module(pid,fx::module_path(owner).filename().c_str());
    fx::require(remote.base!=0,"Windows loader missing from official child");
    return remote.base+reinterpret_cast<uintptr_t>(function)-reinterpret_cast<uintptr_t>(owner);
}
DWORD call(HANDLE process,uintptr_t function,void* parameter) {
    fx::Handle thread(CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),parameter,0,nullptr));
    fx::require(thread.value!=nullptr,"DS2FX initialization thread failed; Win32="+std::to_string(GetLastError()));
    // A timeout does not free a path still in use or terminate a live game.
    fx::require(WaitForSingleObject(thread.value,20000)==WAIT_OBJECT_0,"DS2FX initialization timed out; close the game normally");
    DWORD result=0; fx::require(GetExitCodeThread(thread.value,&result)!=0,"Cannot read DS2FX initialization result");
    return result;
}
void inject(HANDLE process,DWORD pid,const fx::fs::path& core) {
    fx::require(!module(pid,L"ds2fx_core.dll").base && !module(pid,L"ds2fx_bridge_core.dll").base,"DS2FX is already loaded; refusing a second runtime");
    HMODULE mapped=LoadLibraryExW(core.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fx::require(mapped!=nullptr,"Cannot inspect bridge core");
    auto entry=GetProcAddress(mapped,"DS2FX_Bootstrap");
    auto offset=reinterpret_cast<uintptr_t>(entry)-reinterpret_cast<uintptr_t>(mapped); FreeLibrary(mapped);
    fx::require(entry!=nullptr,"Bridge bootstrap export missing");
    const auto path=core.wstring(); SIZE_T bytes=(path.size()+1)*sizeof(wchar_t),written=0;
    auto remote=VirtualAllocEx(process,nullptr,bytes,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    fx::require(remote!=nullptr,"Cannot allocate DS2FX path");
    fx::require(WriteProcessMemory(process,remote,path.c_str(),bytes,&written) && written==bytes,"Cannot pass DS2FX path");
    call(process,remote_load_library(pid),remote);
    VirtualFreeEx(process,remote,0,MEM_RELEASE);
    auto loaded=module(pid,L"ds2fx_bridge_core.dll");
    fx::require(loaded.base && fx::fs::equivalent(loaded.path,core),"Bridge core did not load from the expected path");
    fx::require(call(process,loaded.base+offset,nullptr)==1,"DS2FX bridge bootstrap refused; inspect runtime log and close the game");
}
BOOL CALLBACK find_window(HWND hwnd,LPARAM argument) {
    auto pair=reinterpret_cast<std::pair<DWORD,bool>*>(argument); DWORD pid=0; GetWindowThreadProcessId(hwnd,&pid);
    if(pid==pair->first && IsWindowVisible(hwnd) && GetWindow(hwnd,GW_OWNER)==nullptr) { pair->second=true; return FALSE; }
    return TRUE;
}
bool visible(DWORD pid) { std::pair<DWORD,bool> value{pid,false}; EnumWindows(find_window,reinterpret_cast<LPARAM>(&value)); return value.second; }
void preflight(const fx::fs::path& game,const fx::fs::path& root,const fx::fs::path& official_dir) {
    fx::content::load(root);
    fx::no_links(game); fx::no_links(root);
#ifndef DS2FX_FIXTURE
    fx::require(game.filename()==L"DarkSoulsII.exe" && fx::hash_file(game)==fx::game_hash,"Unsupported Scholar executable");
#else
    fx::require(game.filename()==L"ds2fx_bridge_fixture_host.exe","Fixture bridge only accepts its owned host");
#endif
    const auto official_version=fx::bridge::official_files(official_dir);
    log(std::string("Authenticated official Seamless release ")+official_version);
    for(const auto& name : {L"dinput8.dll",L"ds2sc.dll"})
        fx::require(!fx::fs::exists(game.parent_path()/name),"Unexpected root loader; use the isolated bridge profile");
    const auto core=root/"ds2fx_bridge_core.dll"; fx::no_links(core);
    std::string seal; std::ifstream(root/"bridge.sha256")>>seal;
    fx::require(fx::is_hash(seal) && fx::hash_file(core)==seal,"Bridge core seal mismatch");
    const auto all=fx::discover(root/"Plugins");
#ifdef DS2FX_UNIFIED_ENTRY
    const auto active=fx::bridge::select(all);
    for(const auto& m:all) if(std::none_of(active.begin(),active.end(),[&](const auto& a){return a.id==m.id;}))
        log("Seamless mode: skipping offline-only module "+m.id+"; restored on offline launch");
#else
    fx::bridge::modules(all);
#endif
}
}
#ifdef DS2FX_UNIFIED_ENTRY
int seamless_main(int argc,wchar_t** argv) {
#else
int wmain(int argc,wchar_t** argv) {
#endif
    fx::Handle launcher_process, game_process;
    bool check=false;
    try {
        fx::fs::path game,check_root,check_official;
#ifdef DS2FX_NUCLEUS
        fx::fs::path nucleus_root;
#endif
        for(int i=1;i<argc;++i) {
            std::wstring arg=argv[i];
            if(arg==L"--game" && i+1<argc) game=fx::fs::absolute(argv[++i]).lexically_normal();
            else if(arg==L"--check") check=true;
            else if(arg==L"--check-core" && i+1<argc) check_root=fx::fs::absolute(argv[++i]).lexically_normal();
            else if(arg==L"--check-official" && i+1<argc) check_official=fx::fs::absolute(argv[++i]).lexically_normal();
#ifdef DS2FX_NUCLEUS
            else if(arg==L"--nucleus-root" && i+1<argc) nucleus_root=fx::fs::absolute(argv[++i]).lexically_normal();
#endif
            else if(arg==L"--offline-confirmed") {} // Unified entry already selected the verified Seamless mode.
            else throw std::runtime_error("Usage: ds2fx_seamless --game <full executable> [--check [--check-core <DS2FX>] [--check-official <Game payload>]]");
        }
        fx::require(!game.empty(),"Game path required");
        fx::require(check || (check_root.empty() && check_official.empty()),"Alternate paths are read-only check options");
        const auto dir=game.parent_path(),root=check_root.empty()?dir/"DS2FX":check_root;
        preflight(game,root,check_official.empty()?dir:check_official);
#ifdef DS2FX_NUCLEUS
        fx::nucleus::validate(game,nucleus_root);
#endif
        log(std::string("DS2FX Seamless Bridge ")+fx::bridge::name+" preflight passed; pinned module allowlist");
        if(check) return 0;
        for(const auto& p:processes()) {
            if(_wcsicmp(p.szExeFile,game.filename().c_str())!=0 && _wcsicmp(p.szExeFile,L"ds2sc_launcher.exe")!=0) continue;
#ifdef DS2FX_NUCLEUS
            fx::Handle other(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,p.th32ProcessID));
            fx::require(other.value!=nullptr,"Cannot identify another DS2 process");
            fx::nucleus::verify_other(image_path(other.value),game,nucleus_root);
#else
            throw std::runtime_error("A game/Seamless launcher is already running; close it before this experiment");
#endif
        }
        wchar_t local[32768]; auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        fx::require(count && count<32768,"LOCALAPPDATA missing");
        const auto logs=fx::fs::path(local)/"DS2FX"/"logs"; fx::no_links(logs); fx::fs::create_directories(logs);
        journal.open(logs/("bridge-"+std::to_string(GetCurrentProcessId())+".log"));
        log("Launching unmodified official Seamless launcher; optional local Lantern hooks, no automatic session join");
        const auto official=dir/"ds2sc_launcher.exe";
        std::wstring command=L"\""+official.wstring()+L"\"";
        STARTUPINFOW si{}; si.cb=sizeof(si); PROCESS_INFORMATION pi{};
        fx::require(CreateProcessW(official.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,dir.c_str(),&si,&pi),"Official launcher could not start; Win32="+std::to_string(GetLastError()));
        launcher_process.value=pi.hProcess; CloseHandle(pi.hThread);
        const auto launched=creation(launcher_process.value); const auto deadline=GetTickCount64()+120000;
        log("Official launcher pid="+std::to_string(pi.dwProcessId));
        DWORD game_pid=0;
        while(GetTickCount64()<deadline && !game_process.value) {
            for(const auto& p:processes()) {
                if(p.th32ParentProcessID!=pi.dwProcessId || _wcsicmp(p.szExeFile,game.filename().c_str())!=0) continue;
                HANDLE h=OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ|PROCESS_VM_WRITE|PROCESS_VM_OPERATION|PROCESS_CREATE_THREAD|SYNCHRONIZE,FALSE,p.th32ProcessID);
                fx::require(h!=nullptr,"Cannot open official game child; Win32="+std::to_string(GetLastError()));
                game_process.value=h; game_pid=p.th32ProcessID;
                fx::require(creation(h)>=launched && fx::fs::equivalent(image_path(h),game),"Official child identity mismatch");
                break;
            }
            if(!game_process.value) Sleep(100);
        }
        fx::require(game_process.value!=nullptr,"No direct game child found from official launcher within 120 seconds; no arbitrary process attached");
        log("Verified official game child pid="+std::to_string(game_pid));
        uint64_t stable=0;
        while(GetTickCount64()<deadline) {
            fx::require(WaitForSingleObject(game_process.value,0)==WAIT_TIMEOUT,"Official game exited before bridge initialization");
            auto seam=module(game_pid,L"ds2sc.dll");
            bool ready=seam.base && fx::fs::equivalent(seam.path,dir/"SeamlessCoop"/"ds2sc.dll");
#ifndef DS2FX_FIXTURE
            ready=ready && visible(game_pid);
#endif
            if(ready) { if(!stable) stable=GetTickCount64(); if(GetTickCount64()-stable>=3000) break; }
            else stable=0;
            Sleep(100);
        }
        fx::require(stable && GetTickCount64()-stable>=3000,"Official Seamless module/window did not become ready; no injection attempted");
        preflight(game,root,dir);
        log("Official Seamless DLL present; loading DS2FX pinned bridge after game startup");
        inject(game_process.value,game_pid,root/"ds2fx_bridge_core.dll");
        log("DS2FX bridge READY game pid="+std::to_string(game_pid)+"; this is load evidence, not multiplayer certification");
        WaitForSingleObject(game_process.value,INFINITE);
        DWORD result=0; GetExitCodeProcess(game_process.value,&result); log("Game exited: "+std::to_string(result));
        if(launcher_process.value) WaitForSingleObject(launcher_process.value,INFINITE);
        return static_cast<int>(result);
    } catch(const std::exception& e) {
        log(std::string("BRIDGE FAILED: ")+e.what());
        // Preserve Root Builder's lifetime even when initialization fails. Never kill a live game.
        if(game_process.value && WaitForSingleObject(game_process.value,0)==WAIT_TIMEOUT) {
            log("Close DS2 normally. Bridge is waiting so Root Builder retains deployment.");
            WaitForSingleObject(game_process.value,INFINITE);
        }
        if(launcher_process.value) WaitForSingleObject(launcher_process.value,INFINITE);
        return 1;
    }
}
