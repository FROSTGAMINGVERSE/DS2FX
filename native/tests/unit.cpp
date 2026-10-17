#include "../src/common.h"
#include "../src/compatibility.h"
#include <iostream>
int main() {
    try {
        fx::require(fx::identifier("frost-player-observer"),"identifier");
        fx::require(!fx::identifier("../escape") && !fx::identifier("C:evil"),"unsafe identifier");
        fx::require(fx::semver("0.1.0") && !fx::semver("0..1") && !fx::semver("latest"),"versions");
        fx::require(fx::is_hash(fx::game_hash) && !fx::is_hash("123"),"hash syntax");
        uintptr_t output = 42;
        fx::require(!fx::read(0,output) && !fx::read(1,output),"invalid reads must not fault");
        auto root = fx::fs::temp_directory_path() / ("ds2fx-conflict-test-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        fx::fs::create_directories(root / "SeamlessCoop" / "crashdumps" / "reports");
        std::ofstream(root / "SeamlessCoop" / "crashdumps" / "reports" / "old.dmp") << "report";
        std::ofstream(root / "SeamlessCoop" / "crashdumps" / "settings.dat") << "settings";
        fx::require(fx::runtime_conflicts(root).empty(),"crash-only Overwrite must not block launch");
        std::ofstream(root / "SeamlessCoop" / "ds2sc.DLL") << "runtime placeholder";
        auto found = fx::runtime_conflicts(root);
        fx::require(found.size() == 1 && found[0].filename() == "ds2sc.DLL","Seamless DLL detection is case insensitive");
        std::ofstream(root / "dinput8.dll") << "legacy placeholder";
        std::ofstream(root / "ds2sc_launcher.exe") << "launcher placeholder";
        fx::require(fx::runtime_conflicts(root).size() == 3,"legacy and Seamless launcher payloads remain blocked");
        // Preserve the tiny fixture for diagnosis; never recursively delete computed paths.
        std::cout << "PASS crash-report-only folder accepted; Seamless DLL, legacy proxy and launcher rejected\n";
        std::cout << "PASS identifiers, versions, hashes, inaccessible memory\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
