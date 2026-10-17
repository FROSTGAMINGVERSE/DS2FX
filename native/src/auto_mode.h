#pragma once
#include "common.h"
namespace fx {
enum class LaunchMode { offline, seamless };
inline LaunchMode detect_mode(const fs::path& directory) {
    no_links(directory);
    const bool launcher=fs::is_regular_file(directory/"ds2sc_launcher.exe");
    const bool dll=fs::is_regular_file(directory/"SeamlessCoop"/"ds2sc.dll");
    bool payload=fs::exists(directory/"ds2sc_launcher.exe") || fs::exists(directory/"ds2sc.dll");
    const auto folder=directory/"SeamlessCoop";
    if(fs::exists(folder)) {
        no_links(folder); require(fs::is_directory(folder),"SeamlessCoop must be a directory");
        for(const auto& p:fs::recursive_directory_iterator(folder)) {
            no_links(p.path());
            if(p.is_regular_file()) {
                const auto ext=lower(p.path().extension().string());
                payload=payload || ext==".dll" || ext==".exe" || ext==".asi";
            }
        }
    }
    if(launcher && dll) return LaunchMode::seamless;
    require(!payload,"Incomplete Seamless deployment. Enable its complete MO2 mod, or disable it and let Root Builder Clear. If already disabled, check MO2 Overwrite for leftover SeamlessCoop DLL/EXE files. Refusing offline fallback.");
    return LaunchMode::offline;
}
}
