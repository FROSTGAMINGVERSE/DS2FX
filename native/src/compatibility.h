#pragma once
#include "common.h"

namespace fx {
// Root Builder also deploys Overwrite crash reports. A directory name is not
// evidence that a runtime is installed: inspect executable payloads instead.
inline std::vector<fs::path> runtime_conflicts(const fs::path& game_directory) {
    std::vector<fs::path> conflicts;
    for (const auto* name : {L"dinput8.dll",L"ds2sc_launcher.exe",L"ds2sc.dll"}) {
        auto path = game_directory / name;
        if (fs::exists(path)) conflicts.push_back(path);
    }
    auto seamless = game_directory / "SeamlessCoop";
    if (fs::exists(seamless)) {
        no_links(seamless);
        require(fs::is_directory(seamless),"SeamlessCoop is not a directory: " + seamless.string());
        for (const auto& entry : fs::recursive_directory_iterator(seamless)) {
            no_links(entry.path());
            if (!entry.is_regular_file()) continue;
            const auto extension = lower(entry.path().extension().string());
            if (extension == ".dll" || extension == ".exe" || extension == ".asi") conflicts.push_back(entry.path());
        }
    }
    std::sort(conflicts.begin(),conflicts.end());
    return conflicts;
}
}
