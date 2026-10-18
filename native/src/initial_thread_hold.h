#pragma once
#include "common.h"
#include <tlhelp32.h>
#include <memory>

// Hold only the official launcher's existing CREATE_SUSPENDED primary thread.
// No thread context, executable bytes, vendor data, or suspend ownership is
// changed beyond our single, balanced extra SuspendThread/ResumeThread pair.
namespace fx::launch {
inline DWORD entry_rva(const fs::path& executable) {
    std::ifstream file(executable,std::ios::binary);IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
    require(bool(file.read(reinterpret_cast<char*>(&dos),sizeof(dos))) && dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>0 && dos.e_lfanew<1024*1024,"Invalid launch DOS header");
    file.seekg(dos.e_lfanew);
    require(bool(file.read(reinterpret_cast<char*>(&nt),sizeof(nt))) && nt.Signature==IMAGE_NT_SIGNATURE && nt.FileHeader.Machine==IMAGE_FILE_MACHINE_AMD64 && nt.OptionalHeader.Magic==IMAGE_NT_OPTIONAL_HDR64_MAGIC,"Invalid launch PE header");
    require(nt.OptionalHeader.AddressOfEntryPoint && nt.OptionalHeader.AddressOfEntryPoint<nt.OptionalHeader.SizeOfImage,"Invalid launch entrypoint");
    return nt.OptionalHeader.AddressOfEntryPoint;
}
class InitialThreadHold {
    HANDLE thread_=nullptr;bool held_=false;
public:
    DWORD id=0;uintptr_t entry=0;
    InitialThreadHold()=default;
    InitialThreadHold(const InitialThreadHold&)=delete;
    ~InitialThreadHold(){if(held_)ResumeThread(thread_);if(thread_)CloseHandle(thread_);}
    void release() {
        require(held_,"No owned startup suspension to release");
        const auto before=ResumeThread(thread_);
        require(before!=DWORD(-1) && before>=1,"Cannot release owned startup suspension");held_=false;
    }
    static std::unique_ptr<InitialThreadHold> acquire(HANDLE process,DWORD pid,DWORD entry_rva) {
        auto result=std::make_unique<InitialThreadHold>();
        Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0));
        require(snapshot.value!=INVALID_HANDLE_VALUE,"Cannot enumerate official child threads");
        THREADENTRY32 item{};item.dwSize=sizeof(item);uint64_t earliest=UINT64_MAX;
        if(Thread32First(snapshot.value,&item))do {
            if(item.th32OwnerProcessID!=pid)continue;
            Handle thread(OpenThread(THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,item.th32ThreadID));
            if(!thread.value)continue;
            FILETIME c{},e{},k{},u{};if(!GetThreadTimes(thread.value,&c,&e,&k,&u))continue;
            const auto when=(uint64_t(c.dwHighDateTime)<<32)|c.dwLowDateTime;
            if(when>=earliest)continue;
            if(result->thread_)CloseHandle(result->thread_);
            result->thread_=thread.value;thread.value=nullptr;result->id=item.th32ThreadID;earliest=when;
        }while(Thread32Next(snapshot.value,&item));
        require(result->thread_!=nullptr,"Official child primary thread unavailable");
        const auto previous=SuspendThread(result->thread_);
        require(previous!=DWORD(-1),"Cannot acquire owned startup suspension");result->held_=true;
        require(previous==1,"Missed official CREATE_SUSPENDED startup window; refusing late content injection. Close the game normally and relaunch");
        CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;
        require(GetThreadContext(result->thread_,&context)!=FALSE,"Cannot verify official primary-thread start context");
        require(context.Rcx>entry_rva,"Official child has an invalid initial entrypoint context");
        const auto base=context.Rcx-entry_rva;IMAGE_DOS_HEADER dos{};SIZE_T read=0;
        require(ReadProcessMemory(process,reinterpret_cast<void*>(base),&dos,sizeof(dos),&read) && read==sizeof(dos) &&
            dos.e_magic==IMAGE_DOS_SIGNATURE && dos.e_lfanew>0 && dos.e_lfanew<1024*1024,
            "Official primary thread has already left its initial entrypoint context; refusing late content injection");
        IMAGE_NT_HEADERS64 nt{};
        require(ReadProcessMemory(process,reinterpret_cast<void*>(base+dos.e_lfanew),&nt,sizeof(nt),&read) && read==sizeof(nt) &&
            nt.Signature==IMAGE_NT_SIGNATURE && nt.OptionalHeader.AddressOfEntryPoint==entry_rva,
            "Official primary-thread entrypoint does not match the verified executable");
        result->entry=context.Rcx;
        return result;
    }
};
}
