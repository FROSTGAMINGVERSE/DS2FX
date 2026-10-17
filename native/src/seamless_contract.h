#pragma once
#include "common.h"

namespace fx::bridge {
inline constexpr char name[] = "0.2.2-experimental.1";
struct OfficialRelease { const char* version; const char* launcher; const char* dll; };
// Official author release assets only. Keep the prior pair for a reversible rollback;
// unknown versions require a reviewed update, never a mutable user-provided hash.
inline constexpr OfficialRelease official_releases[] = {
    {"0.0.1", "4fb07cd36e17fba7755597395bc1b44a0809358128e8c8353e8add65583fa88c", "d5101a37e5155f9e4f685722d918c3c2b796b1a95a092005f0c20905b5459d7e"},
    {"0.0.3", "4fb07cd36e17fba7755597395bc1b44a0809358128e8c8353e8add65583fa88c", "17e4ae0355261308a5e8fdf50131aee3bb18ddb925adadfdf9d6b68c8b8bda8a"},
    {"0.0.5", "4fb07cd36e17fba7755597395bc1b44a0809358128e8c8353e8add65583fa88c", "5df6c51fae1c0a177090e39831402120a7913fe42c2625da32767de562d17a71"},
    {"0.0.4", "4fb07cd36e17fba7755597395bc1b44a0809358128e8c8353e8add65583fa88c", "70dfc68f740c9bbf474ba2c6569ca230131df95f3afdcae11bef309cad126b44"}
};
inline const OfficialRelease* supported_release(const std::string& launcher,const std::string& dll) noexcept {
    for(const auto& release:official_releases)
        if(launcher==release.launcher && dll==release.dll) return &release;
    return nullptr;
}
inline void modules(const std::vector<Module>& list) {
    // An explicit local experiment exception, not a change to blocked-online manifests.
    // Native DLLs are trusted; only these exact, reviewed local-presentation
    // builds may run. Candidate HUD suppression still needs live validation.
    require(!list.empty(), "Bridge requires a pinned supported module");
    for (const auto& m : list) {
        const bool observer = m.id == "frost-player-observer" && m.version == "0.2.0" &&
            m.hash == "6fa7b01e807b93c96fe75c16d3e17e0dc9e46bd20fecc8a2dc1a09d044bccda0";
        const bool hud = m.id == "frost-player-hud" && m.version == "0.2.0" &&
            m.hash == "97c8e10a2d3449acbe40beda8ab1c1fdc3c681945c49b88bfd9e7db2cfe00b0e";
        const bool lantern = m.id == "wayfarers-lantern" && m.version == "0.4.0" &&
            m.hash == "f4d92dcd5400a33ea222b6c813bf6f6692d3c2838f20ee00f682bf2b15abc386";
        const bool remastered_hud = m.id == "ds2-remastered-hud" &&
            ((m.version == "0.1.2" && m.hash == "7d742f1c611e8f827fc61aa13d2a2e15f18fe97179cd526b21ac7105e7e6583e") ||
             (m.version == "0.1.3" && m.hash == "f9180ac8f7e004b5c3c0b51a732362d7a2d3ad55f45ea65bf7155f5f6449b003") ||
             (m.version == "0.1.4" && m.hash == "2654efc381239f20f0269920efa4acb9ec390de607dadfb3fe069021a3574e56") ||
             (m.version == "0.1.5" && m.hash == "aeb17a84200d6299d8493c263acea8dfd88ae90edf96928f66316f6b2f1ad307") ||
             (m.version == "0.1.6" && m.hash == "f0b26335b94abfc20359c66ddf9fddd9295909b9578138e58608d0f107efd19b") ||
             (m.version == "0.1.7" && m.hash == "71c70ef867a8b023ed441ea684ca2d4be3bf8414d5c71aa246908870e38a0feb") ||
             (m.version == "0.1.8" && m.hash == "a7c18ae921043c40026b2d9a28ba028d75701d14b174a62784827f19c9a9f599") ||
             (m.version == "0.1.9" && m.hash == "8f1bb8eee5998ddb105c5cd5c5b62fa4b4f7e9fdae5a8c2a1eb4e1641a16a397") ||
             (m.version == "0.1.10" && m.hash == "d6c1eddb061107c5ad808d35d4145cdeaf9f66bcfee95ede4d16ab6dd04bcb65") ||
             (m.version == "0.1.11" && m.hash == "d649da914387787ad397969d50bbe7aa74919c269cf3221a42eeefb16d8c15b2") ||
             (m.version == "0.1.12" && m.hash == "e00cf87211e1ac41bbe64b6f0408978f3fc9c5514b4f94f1e06d616235441327") ||
               (m.version == "0.1.13" && m.hash == "2d78005883555b9cc19d3a6f9b5e3eb09b1e473a3124187b25b8052a46ae67ad") ||
               (m.version == "0.1.14" && m.hash == "a8f1340eacf9c99a4818a7dc5dd91f9b1404a3fc405779f7a858d92e9e5f138b") || (m.version == "0.1.15" && m.hash == "a40802482653075607064bcc61d5b49a32fb5484a3605e4850b6fba6cba6384d") || (m.version == "0.1.16" && m.hash == "da971324f57bdb973d1faa87761ec686c822f31e0a65df8464107e596272ead0") || (m.version == "0.1.17" && m.hash == "beb64b935feb6106fbfbdbb655efa2029a486b6fe78a89280cba63ad45173df2") || (m.version == "0.1.18" && m.hash == "3f05aad755b8436a7fec42c4b3e8d4c55f3074b9aed669ef03cea88e3e853977") || (m.version == "0.1.19" && m.hash == "b3fe55dfd0271e5bc3fa56b38050a066e58c321be58478b02e427390807b43e2") || (m.version == "0.1.20" && m.hash == "c61e814ddce7e68e8c2aa6c299de541c30b08061177d508d8a1eb9273e626bb9") || (m.version == "0.1.21" && m.hash == "3ba6937ef3df652f46975b8239a1cc496ca9b66527419f6f04088978013f4c7c") || (m.version == "0.1.22" && m.hash == "d77602756b8d22a12a6aea47c03918c2daca71445e619a6b5fc0f29f7a3f2140") || (m.version == "0.1.23" && m.hash == "9569f1a8b9421e110c844442450c3bc487fdd457ee5f79e19886df062b788634") || (m.version == "0.1.24" && m.hash == "4d47ce03108f6b78d5691d6cfbca22c2e8f9edc4e001bf8edc9fe2bc9df15bdb") || (m.version == "0.1.25" && m.hash == "bd24a4171235c8836529b78f6e590aeca7a35056e828c067ef4f69a235fabe7c") || (m.version == "0.1.26" && m.hash == "ad4af5ff9ac7b4d251e7864a5008294bbaa5015ce15a1f409be1b232bb183683") || (m.version == "0.1.27" && m.hash == "fb089bb7c8af0758a079fb07dfa710790332fa9df09772752f1b5fad5add531b") || (m.version == "0.1.28" && m.hash == "ea754b40669a20d12bd85c68a9492ea4b0f998dda383f3d292697afa267b209f"));
        require(observer || hud || lantern || remastered_hud, "Seamless experiment permits only pinned local display modules or identity-only Lantern: " + m.id);
    }
}
inline std::vector<Module> select(const std::vector<Module>& all) {
    std::vector<Module> active;
    for(const auto& m:all) {
        if(m.id=="frost-player-observer" || m.id=="frost-player-hud" || m.id=="wayfarers-lantern" || m.id=="ds2-remastered-hud") {
            modules({m}); // A changed build of a recognized module is an error, not a silent skip.
            active.push_back(m);
        }
    }
    for(const auto& m:active) for(const auto& [id,dependency_version]:m.depends)
        require(std::any_of(active.begin(),active.end(),[&](const auto& d){return d.id==id && d.version==dependency_version;}),"Seamless module depends on an inactive offline-only module: "+m.id);
    return active;
}
inline const char* official_files(const fs::path& game_dir) {
    auto launcher = game_dir / "ds2sc_launcher.exe";
    auto dll = game_dir / "SeamlessCoop" / "ds2sc.dll";
    no_links(launcher); no_links(dll);
#ifndef DS2FX_FIXTURE
    const auto launcher_sha=hash_file(launcher),dll_sha=hash_file(dll);
    require(std::any_of(std::begin(official_releases),std::end(official_releases),[&](const auto& r){return launcher_sha==r.launcher;}),
        "Unsupported official Seamless launcher hash (supported authenticated releases: 0.0.1, 0.0.3, 0.0.4, 0.0.5)");
    const auto release=supported_release(launcher_sha,dll_sha);
    require(release!=nullptr,"Unsupported official Seamless DLL hash or launcher/DLL pair (supported authenticated releases: 0.0.1, 0.0.3, 0.0.4, 0.0.5)");
    return release->version;
#else
    require(fs::is_regular_file(launcher) && fs::is_regular_file(dll), "Fixture launcher/DLL missing");
    return "owned-fixture";
#endif
}
inline void loaded_official(const fs::path& game_dir) {
    auto module = GetModuleHandleW(L"ds2sc.dll");
    require(module != nullptr, "Official Seamless DLL must already be loaded");
    require(fs::equivalent(module_path(module),game_dir / "SeamlessCoop" / "ds2sc.dll"), "Unexpected loaded Seamless DLL path");
    official_files(game_dir);
}
}

