#include "auto_mode.h"
#include <iostream>
int offline_main(int,wchar_t**);
int seamless_main(int,wchar_t**);
int wmain(int argc,wchar_t** argv) {
    bool check=false;
    try {
        fx::fs::path game,check_official;
#ifdef DS2FX_NUCLEUS
        bool nucleus=false;
#endif
        for(int i=1;i<argc;++i) {
            std::wstring arg=argv[i];
            if(arg==L"--game" && i+1<argc) game=fx::fs::absolute(argv[++i]).lexically_normal();
            else if(arg==L"--check") check=true;
            else if(arg==L"--check-official" && i+1<argc) check_official=fx::fs::absolute(argv[++i]).lexically_normal();
            else if(arg==L"--check-core" && i+1<argc) ++i;
#ifdef DS2FX_NUCLEUS
            else if(arg==L"--nucleus-root" && i+1<argc) { ++i; nucleus=true; }
#endif
            else if(arg!=L"--offline-confirmed") throw std::runtime_error("DS2FX: use --game <full Scholar executable> [--check [--check-core <folder>] [--check-official <folder>]]");
        }
        fx::require(!game.empty(),"Game path required");
        fx::require(check || check_official.empty(),"--check-official is read-only; it cannot override a real launch");
        const auto mode=fx::detect_mode(check_official.empty()?game.parent_path():check_official);
#ifdef DS2FX_NUCLEUS
        fx::require(nucleus && mode==fx::LaunchMode::seamless,"Nucleus build requires --nucleus-root and official Seamless; use standard DS2FX for solo play");
#endif
        std::cout<<"DS2FX unified launcher "<<fx::package_version<<" | Mode: "<<(mode==fx::LaunchMode::seamless?"Seamless (automatic)":"Offline (automatic)")<<std::endl;
        if(mode==fx::LaunchMode::seamless) return seamless_main(argc,argv);
        // Offline backend does not need the inspection-only dependency directory.
        std::vector<wchar_t*> args;
        for(int i=0;i<argc;++i) {
            if(std::wstring(argv[i])==L"--check-official") { ++i; continue; }
            args.push_back(argv[i]);
        }
        return offline_main(static_cast<int>(args.size()),args.data());
    } catch(const std::exception& e) {
        std::cerr<<"DS2FX: "<<e.what()<<std::endl;
        // Detection can fail before either backend opens its own log or dialog.
        try {
            wchar_t local[32768];
            const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
            if(n>0 && n<32768) {
                const auto logs=fx::fs::path(local)/"DS2FX"/"logs";
                fx::fs::create_directories(logs);
                std::ofstream(logs/("launcher-"+std::to_string(GetCurrentProcessId())+".log"))<<"Auto detection failed: "<<e.what()<<'\n';
            }
        } catch(...) {}
#ifndef DS2FX_FIXTURE
        if(!check) MessageBoxA(nullptr,e.what(),"DS2FX could not launch",MB_OK | MB_ICONERROR);
#endif
        return 1;
    }
}
