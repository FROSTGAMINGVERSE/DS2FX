#include "../src/initial_thread_hold.h"
#include <iostream>
int wmain(int argc,wchar_t** argv) {
    if(argc==3 && std::wstring(argv[1])==L"--child") {
        std::ofstream(fx::fs::path(argv[2]))<<"owned child entered main\n";Sleep(500);return 0;
    }
    try {
        const auto self=fx::module_path();const auto rva=fx::launch::entry_rva(self);
        const auto root=fx::fs::temp_directory_path()/("DS2FX-initial-hold-"+std::to_string(GetCurrentProcessId()));
        fx::fs::create_directories(root);
        for(int scenario=0;scenario<4;++scenario) {
            const auto receipt=root/(std::to_string(scenario)+".txt");
            auto command=L"\""+self.wstring()+L"\" --child \""+receipt.wstring()+L"\"";
            STARTUPINFOW si{};si.cb=sizeof(si);PROCESS_INFORMATION pi{};
            fx::require(CreateProcessW(self.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_SUSPENDED,nullptr,self.parent_path().c_str(),&si,&pi),"Owned child launch failed");
            fx::Handle process(pi.hProcess),primary(pi.hThread);
            if(scenario==0) {
                auto hold=fx::launch::InitialThreadHold::acquire(process.value,pi.dwProcessId,rva);
                bool duplicate=false;try{fx::launch::InitialThreadHold::acquire(process.value,pi.dwProcessId,rva);}catch(...){duplicate=true;}
                fx::require(duplicate,"Unexpected extra suspend owner accepted");
                fx::require(ResumeThread(primary.value)==2,"Rejected duplicate changed suspension ownership");
                Sleep(100);fx::require(!fx::fs::exists(receipt),"Owned hold allowed child main to execute");
                hold->release();
            }else if(scenario==1) {
                auto hold=fx::launch::InitialThreadHold::acquire(process.value,pi.dwProcessId,rva);
                hold->release();
                Sleep(100);fx::require(!fx::fs::exists(receipt),"Owned release consumed the official launcher's remaining suspension");
                fx::require(ResumeThread(primary.value)==1,"Reverse resume order did not retain exactly the official suspension");
            }else if(scenario==2) {
                bool wrong=false;try{fx::launch::InitialThreadHold::acquire(process.value,pi.dwProcessId,rva+1);}catch(...){wrong=true;}
                fx::require(wrong && ResumeThread(primary.value)==1,"Wrong entrypoint was accepted or suspension leaked");
            }else {
                fx::require(ResumeThread(primary.value)==1,"Cannot start owned late case");
                for(int n=0;n<100 && !fx::fs::exists(receipt);++n)Sleep(5);
                fx::require(fx::fs::exists(receipt),"Late-case host did not enter main");
                bool late=false;try{fx::launch::InitialThreadHold::acquire(process.value,pi.dwProcessId,rva);}catch(...){late=true;}
                fx::require(late,"Already running primary thread accepted");
            }
            fx::require(WaitForSingleObject(process.value,5000)==WAIT_OBJECT_0 && fx::fs::exists(receipt),"Suspension not balanced or owned child did not finish");
            DWORD exit=1;fx::require(GetExitCodeProcess(process.value,&exit) && exit==0,"Owned child failed");
        }
        fx::fs::remove_all(root);
        std::cout<<"PASS initial thread hold: pre-entry capture, both resume orders, duplicate owner, wrong entrypoint and already-running rejection\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
