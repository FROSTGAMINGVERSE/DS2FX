#include "common.h"
#include "compatibility.h"
#include "content_pack.h"
#include <tlhelp32.h>
#include <iostream>

namespace {
void write_startup_byte(HANDLE process,uintptr_t address,unsigned char byte) {
    DWORD previous = 0, ignored = 0;
    fx::require(VirtualProtectEx(process,reinterpret_cast<void*>(address),1,PAGE_EXECUTE_READWRITE,&previous) != 0,"Cannot set temporary startup breakpoint protection");
    SIZE_T count = 0;
    const bool written = WriteProcessMemory(process,reinterpret_cast<void*>(address),&byte,1,&count) && count == 1;
    const bool restored = VirtualProtectEx(process,reinterpret_cast<void*>(address),1,previous,&ignored) != 0;
    fx::require(written && restored && FlushInstructionCache(process,reinterpret_cast<void*>(address),1),"Cannot write/restore temporary startup breakpoint");
}
void wait_for_loader(PROCESS_INFORMATION& pi) {
    // Hold the primary thread at the PE entrypoint AFTER loader-lock initialization.
    // A one-byte startup breakpoint is restored before game execution; no disk patch.
    // Some systems take longer than the original 20-second budget to deliver
    // the initial loader breakpoint after Steam starts the protected game
    // process. This is only a wait budget: no game code runs while waiting.
    const auto deadline = GetTickCount64()+60000;
    uintptr_t entrypoint = 0;
    unsigned char original = 0;
    bool armed = false;
    while (GetTickCount64() < deadline) {
        DEBUG_EVENT event{};
        if (!WaitForDebugEvent(&event,1000)) {
            fx::require(GetLastError() == ERROR_SEM_TIMEOUT,"Cannot wait for loader initialization");
            continue;
        }
        bool ready = false;
        DWORD continuation = DBG_CONTINUE;
        if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT) {
            if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
            IMAGE_DOS_HEADER dos{};
            IMAGE_NT_HEADERS64 nt{};
            SIZE_T count = 0;
            const auto image = reinterpret_cast<uintptr_t>(event.u.CreateProcessInfo.lpBaseOfImage);
            fx::require(ReadProcessMemory(pi.hProcess,reinterpret_cast<void*>(image),&dos,sizeof(dos),&count) && count == sizeof(dos),"Cannot inspect child DOS header");
            fx::require(dos.e_magic == IMAGE_DOS_SIGNATURE && dos.e_lfanew > 0 && dos.e_lfanew < 1048576,"Invalid child DOS header");
            fx::require(ReadProcessMemory(pi.hProcess,reinterpret_cast<void*>(image+dos.e_lfanew),&nt,sizeof(nt),&count) && count == sizeof(nt),"Cannot inspect child PE header");
            fx::require(nt.Signature == IMAGE_NT_SIGNATURE && nt.OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC,"Child is not x64 PE");
            entrypoint = image+nt.OptionalHeader.AddressOfEntryPoint;
        } else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT) {
            if (event.u.LoadDll.hFile) CloseHandle(event.u.LoadDll.hFile);
        } else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT) {
            auto code = event.u.Exception.ExceptionRecord.ExceptionCode;
            if (code == EXCEPTION_BREAKPOINT && !armed) {
                fx::require(entrypoint != 0,"Missing child entrypoint");
                SIZE_T count = 0;
                fx::require(ReadProcessMemory(pi.hProcess,reinterpret_cast<void*>(entrypoint),&original,1,&count) && count == 1 && original != 0xcc,"Cannot read original entrypoint instruction");
                write_startup_byte(pi.hProcess,entrypoint,0xcc);
                armed = true;
            } else if (code == EXCEPTION_BREAKPOINT && armed && event.dwThreadId == pi.dwThreadId && reinterpret_cast<uintptr_t>(event.u.Exception.ExceptionRecord.ExceptionAddress) == entrypoint) {
                write_startup_byte(pi.hProcess,entrypoint,original);
                CONTEXT context{}; context.ContextFlags = CONTEXT_CONTROL;
                fx::require(GetThreadContext(pi.hThread,&context) != 0,"Cannot read entrypoint context");
                context.Rip = entrypoint;
                fx::require(SetThreadContext(pi.hThread,&context) != 0,"Cannot restore entrypoint context");
                fx::require(SuspendThread(pi.hThread) != static_cast<DWORD>(-1),"Cannot hold game entrypoint");
                ready = true;
            } else continuation = DBG_EXCEPTION_NOT_HANDLED;
        }
        fx::require(ContinueDebugEvent(event.dwProcessId,event.dwThreadId,continuation) != 0,"Cannot continue loader initialization");
        fx::require(event.dwDebugEventCode != EXIT_PROCESS_DEBUG_EVENT,"Child exited before loader initialization");
        if (ready) {
            fx::require(DebugActiveProcessStop(pi.dwProcessId) != 0,"Cannot release startup debugger");
            return;
        }
    }
    throw std::runtime_error("Windows loader initialization timed out");
}
uintptr_t remote_module(DWORD pid,const std::wstring& name) {
    for (int attempt = 0; attempt < 10; ++attempt) {
        fx::Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32,pid));
        if (snapshot.value != INVALID_HANDLE_VALUE) {
            MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
            if (Module32FirstW(snapshot.value,&entry)) do {
                if (_wcsicmp(entry.szModule,name.c_str()) == 0) return reinterpret_cast<uintptr_t>(entry.modBaseAddr);
            } while (Module32NextW(snapshot.value,&entry));
        }
        Sleep(20);
    }
    return 0;
}
uintptr_t remote_function(DWORD pid,const wchar_t* module,const char* function) {
    auto local = GetProcAddress(GetModuleHandleW(module),function);
    fx::require(local != nullptr,"Cannot resolve Windows loader function");
    HMODULE owner = nullptr;
    fx::require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(local),&owner) != 0,"Cannot resolve loader owner");
    auto remote = remote_module(pid,fx::module_path(owner).filename().wstring());
    fx::require(remote != 0,"Loader owner missing in child process");
    return remote + reinterpret_cast<uintptr_t>(local) - reinterpret_cast<uintptr_t>(owner);
}
DWORD remote_call(HANDLE process,uintptr_t function,void* parameter) {
    fx::Handle thread(CreateRemoteThread(process,nullptr,0,reinterpret_cast<LPTHREAD_START_ROUTINE>(function),parameter,0,nullptr));
    fx::require(thread.value != nullptr,"Cannot start initialization thread; Win32=" + std::to_string(GetLastError()));
    auto wait = WaitForSingleObject(thread.value,20000);
    fx::require(wait == WAIT_OBJECT_0,"Initialization timed out; child will be terminated");
    DWORD code = 0;
    fx::require(GetExitCodeThread(thread.value,&code) != 0,"Cannot read initialization result");
    return code;
}
uintptr_t export_offset(const fx::fs::path& dll,const char* symbol) {
    // This mapping never executes DllMain. Used only to obtain this built export's RVA.
    auto local = LoadLibraryExW(dll.c_str(),nullptr,DONT_RESOLVE_DLL_REFERENCES);
    fx::require(local != nullptr,"Cannot inspect DS2FX core export table");
    auto address = GetProcAddress(local,symbol);
    auto offset = reinterpret_cast<uintptr_t>(address) - reinterpret_cast<uintptr_t>(local);
    FreeLibrary(local);
    fx::require(address != nullptr,"Missing DS2FX bootstrap export");
    return offset;
}
void preflight(const fx::fs::path& game,const fx::fs::path& core_root) {
    fx::content::load(core_root);
    fx::no_links(game); fx::no_links(core_root / "ds2fx_core.dll");
    fx::require(fx::fs::is_regular_file(game),"Game executable not found");
#ifndef DS2FX_FIXTURE
    fx::require(game.filename() == L"DarkSoulsII.exe" && fx::hash_file(game) == fx::game_hash,"Unsupported Scholar executable. Only the recorded 1.0.3.0 SHA256 is allowed.");
    const auto conflicts = fx::runtime_conflicts(game.parent_path());
    if (!conflicts.empty()) {
        std::string message = "Unsupported runtime payload found. Disable its mod and let Root Builder Clear/Build finish. Overwrite can also supply files:\n";
        for (const auto& path : conflicts) message += "\n" + path.string();
        throw std::runtime_error(message);
    }
#else
    fx::require(game.filename() == L"ds2fx_fixture_host.exe","Fixture launcher only starts its owned test host");
#endif
    const auto core = core_root / "ds2fx_core.dll";
    std::ifstream seal(core_root / "core.sha256");
    std::string expected; seal >> expected;
    fx::require(fx::is_hash(expected) && fx::hash_file(core) == expected,"Core payload missing or hash mismatch. Enable DS2FX Core and let Root Builder build it.");
    const auto modules = fx::discover(core_root / "Plugins");
    std::cout << "DS2FX " << fx::version << " preflight passed; modules=" << modules.size() << '\n';
    for (auto& m : modules) std::cout << "  " << m.id << " " << m.version << '\n';
}
}
#ifdef DS2FX_UNIFIED_ENTRY
int offline_main(int argc,wchar_t** argv) {
#else
int wmain(int argc,wchar_t** argv) {
#endif
    HANDLE child_process = nullptr;
    bool check = false, confirmed = false;
    try {
        fx::fs::path game,check_core;
        for (int i = 1; i < argc; ++i) {
            const std::wstring arg = argv[i];
            if (arg == L"--game" && i+1 < argc) game = fx::fs::absolute(argv[++i]).lexically_normal();
            else if (arg == L"--check") check = true;
            else if (arg == L"--check-core" && i+1 < argc) check_core = fx::fs::absolute(argv[++i]).lexically_normal();
            else if (arg == L"--offline-confirmed") confirmed = true;
            else throw std::runtime_error("Usage: ds2fx_loader --game <DarkSoulsII.exe> [--check [--check-core <DS2FX directory>]] [--offline-confirmed]");
        }
        fx::require(!game.empty(),"Supply --game with the full Scholar executable path");
        fx::require(check || check_core.empty(),"--check-core is for read-only package inspection only");
        const auto root = check_core.empty() ? game.parent_path()/"DS2FX" : check_core;
        preflight(game,root);
        if (check) return 0;
#ifndef DS2FX_FIXTURE
        if (!confirmed && MessageBoxW(nullptr,
            L"DS2FX runtime alpha is for OFFLINE testing only.\n\nVerify Offline Mode at the title screen before loading a character. This confirmation alone does not block networking or isolate saves.\n\nLaunch for offline testing?",
            L"DS2FX - Offline runtime test",MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) != IDYES) return 2;
#else
        fx::require(confirmed,"Fixture launch requires --offline-confirmed");
#endif
        const auto core = root / "ds2fx_core.dll";
        auto bootstrap_offset = export_offset(core,"DS2FX_Bootstrap");
        std::wstring command = L"\"" + game.wstring() + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup);
        PROCESS_INFORMATION pi{};
        fx::require(CreateProcessW(game.c_str(),command.data(),nullptr,nullptr,FALSE,DEBUG_ONLY_THIS_PROCESS,nullptr,game.parent_path().c_str(),&startup,&pi) != 0,
            "Cannot create game process; Win32=" + std::to_string(GetLastError()));
        fx::Handle process(pi.hProcess), main_thread(pi.hThread);
        child_process = process.value;
        // Any startup failure ends this owned, still-suspended child before the game entrypoint runs.
        struct FailedChild { HANDLE h; bool ready = false; ~FailedChild() { if (!ready) { TerminateProcess(h,10); WaitForSingleObject(h,5000); } } } failure{process.value};
        wait_for_loader(pi);
        const auto library_path = core.wstring();
        const SIZE_T bytes = (library_path.size()+1) * sizeof(wchar_t);
        auto remote_path = VirtualAllocEx(process.value,nullptr,bytes,MEM_RESERVE | MEM_COMMIT,PAGE_READWRITE);
        fx::require(remote_path != nullptr,"Cannot allocate core path in child");
        SIZE_T written = 0;
        fx::require(WriteProcessMemory(process.value,remote_path,library_path.c_str(),bytes,&written) && written == bytes,"Cannot pass core path to child");
        remote_call(process.value,remote_function(pi.dwProcessId,L"kernel32.dll","LoadLibraryW"),remote_path);
        VirtualFreeEx(process.value,remote_path,0,MEM_RELEASE);
        const auto remote_core = remote_module(pi.dwProcessId,L"ds2fx_core.dll");
        fx::require(remote_core != 0,"DS2FX DLL did not load in child");
        fx::require(remote_call(process.value,remote_core+bootstrap_offset,nullptr) == 1,"DS2FX initialization failed. See %LOCALAPPDATA%\\DS2FX\\logs\\runtime-<pid>.log");
        fx::require(ResumeThread(main_thread.value) != static_cast<DWORD>(-1),"Cannot resume game entrypoint");
        failure.ready = true;
        std::cout << "DS2FX initialized; game pid=" << pi.dwProcessId << ". Waiting for exit so Root Builder retains deployment.\n";
        fx::require(WaitForSingleObject(process.value,INFINITE) == WAIT_OBJECT_0,"Game wait failed");
        DWORD result = 0; GetExitCodeProcess(process.value,&result);
        child_process = nullptr;
        std::cout << "Game exited: " << result << '\n';
        return static_cast<int>(result);
    } catch (const std::exception& e) {
        (void)child_process;
        std::cerr << "DS2FX: " << e.what() << '\n';
#ifndef DS2FX_FIXTURE
        // MO2 can close the confirmed offline launcher's console immediately.
        // Preserve the actual failure even before the Core opens its own log.
        try {
            wchar_t local[32768]{};
            const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
            if(n && n<32768) {
                const auto logs=fx::fs::path(local)/"DS2FX"/"logs";
                fx::fs::create_directories(logs);
                std::ofstream(logs/("offline-launcher-"+std::to_string(GetCurrentProcessId())+".log"))<<e.what()<<'\n';
            }
        } catch(...) {}
#endif
        if (!check && !confirmed) MessageBoxA(nullptr,e.what(),"DS2FX could not launch",MB_OK | MB_ICONERROR);
        return 1;
    }
}
