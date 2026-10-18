#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <iomanip>

namespace fx {
namespace fs = std::filesystem;
inline constexpr char version[] = "0.3.0-alpha.1";
inline constexpr char package_version[] = "0.5.42-alpha.1";
inline fs::path utf8_path(const std::string& s) { return fs::path(std::u8string(s.begin(),s.end())); }
inline constexpr char game_hash[] = "0045931b8914504531b7864a9488d396dc50cbaf524964016e1d69c3d1173131";
inline void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h = nullptr) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
inline fs::path module_path(HMODULE module = nullptr) {
    std::vector<wchar_t> path(32768);
    auto n = GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
    require(n > 0 && n < path.size(), "Cannot resolve executable/module path");
    return fs::path(std::wstring(path.data(), n));
}
inline std::string lower(std::string s) {
    for (auto& c : s) if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
    return s;
}
inline std::string hash_file(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(input.good(), "Cannot read file for SHA256: " + path.string());
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    require(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0, "SHA256 provider failed");
    struct Cleanup { BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE& h; ~Cleanup() { if(h) BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a,0); } } cleanup{algorithm, hash};
    require(BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) == 0, "SHA256 init failed");
    unsigned char buffer[65536];
    while (input) {
        input.read(reinterpret_cast<char*>(buffer), sizeof(buffer));
        if (input.gcount()) require(BCryptHashData(hash, buffer, static_cast<ULONG>(input.gcount()), 0) == 0, "SHA256 update failed");
    }
    require(input.eof(), "File read failed");
    unsigned char result[32];
    require(BCryptFinishHash(hash, result, 32, 0) == 0, "SHA256 finish failed");
    std::ostringstream out;
    for (auto b : result) out << std::hex << std::setfill('0') << std::setw(2) << unsigned(b);
    return out.str();
}
inline void no_links(const fs::path& path) {
    auto p = fs::absolute(path).lexically_normal();
    while (!p.empty()) {
        auto attr = GetFileAttributesW(p.c_str());
        if (attr != INVALID_FILE_ATTRIBUTES) require(!(attr & FILE_ATTRIBUTE_REPARSE_POINT), "Links/reparse points are unsupported: " + p.string());
        auto parent = p.parent_path();
        if (parent == p) break;
        p = parent;
    }
}
inline std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
inline bool identifier(const std::string& s) {
    return !s.empty() && s.size() <= 80 && std::all_of(s.begin(), s.end(), [](char c) {return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_';});
}
inline bool semver(const std::string& s) {
    unsigned parts = 0; bool digit = false;
    for (char c : s) { if (c >= '0' && c <= '9') digit = true; else if (c == '.' && digit) {++parts; digit = false;} else return false; }
    return parts == 2 && digit;
}
inline bool is_hash(const std::string& s) {
    return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}
inline std::map<std::string,std::string> read_manifest(const fs::path& path) {
    require(fs::file_size(path) <= 16384, "Manifest too large");
    std::ifstream file(path);
    require(file.good(), "Cannot open manifest");
    std::map<std::string,std::string> fields;
    std::string line;
    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        const auto equal = line.find('=');
        require(equal != std::string::npos, "Manifest requires key=value lines");
        auto key = trim(line.substr(0,equal));
        require(fields.emplace(key,trim(line.substr(equal+1))).second, "Duplicate manifest key: " + key);
    }
    const std::set<std::string> allowed{"id","version","api","core","game_sha256","dll_sha256","classification","depends"};
    for (const auto& [key,value] : fields) require(allowed.contains(key), "Unknown manifest field: " + key);
    for (auto key : allowed) require(fields.contains(key), "Missing manifest field: " + key);
    return fields;
}
struct Module {
    fs::path directory;
    std::string id, version, hash;
    std::vector<std::pair<std::string,std::string>> depends;
};
inline std::vector<Module> discover(const fs::path& root) {
    no_links(root);
    if (!fs::exists(root)) return {};
    require(fs::is_directory(root), "DS2FX Plugins must be a directory");
    std::map<std::string,Module> modules;
    for (const auto& entry : fs::directory_iterator(root)) {
        no_links(entry.path());
        require(entry.is_directory(), "Plugins may contain only module directories");
        require(modules.size() < 128, "Too many modules");
        auto manifest_path = entry.path() / "module.ini";
        no_links(manifest_path);
        auto fields = read_manifest(manifest_path);
        Module m{entry.path(),fields.at("id"),fields.at("version"),fields.at("dll_sha256"),{}};
        require(identifier(m.id) && m.id == entry.path().filename().string(), "Invalid module id/folder");
        require(semver(m.version), "Version must be numeric major.minor.patch");
        require(fields.at("api") == "2", "Unsupported plugin API: " + m.id);
        require(fields.at("core") == fx::version, "Core version mismatch: " + m.id);
        require(fields.at("game_sha256") == game_hash, "Unsupported game manifest: " + m.id);
        require(fields.at("classification") == "blocked-online", "Runtime alpha requires blocked-online classification");
        require(is_hash(m.hash), "Invalid plugin SHA256");
        no_links(m.directory / "plugin.dll");
        require(hash_file(m.directory / "plugin.dll") == m.hash, "Plugin hash mismatch: " + m.id);
        std::istringstream dependencies(fields.at("depends"));
        std::string dependency;
        std::set<std::string> seen;
        while (std::getline(dependencies,dependency,',')) {
            dependency = trim(dependency);
            auto at = dependency.find('@');
            require(at != std::string::npos, "Dependency format is id@major.minor.patch");
            auto id = dependency.substr(0,at), dependency_version = dependency.substr(at+1);
            require(identifier(id) && semver(dependency_version) && seen.insert(id).second, "Invalid or duplicate dependency");
            m.depends.emplace_back(id,dependency_version);
        }
        require(modules.emplace(m.id,m).second, "Duplicate module id");
    }
    std::vector<Module> ordered;
    std::set<std::string> done;
    while (ordered.size() < modules.size()) {
        bool advanced = false;
        for (const auto& [id,m] : modules) {
            if (done.contains(id)) continue;
            bool ready = true;
            for (const auto& [dep,ver] : m.depends) {
                require(modules.contains(dep), "Missing dependency " + dep + " required by " + id);
                require(modules.at(dep).version == ver, "Dependency version mismatch: " + dep);
                if (!done.contains(dep)) ready = false;
            }
            if (ready) { ordered.push_back(m); done.insert(id); advanced = true; }
        }
        require(advanced, "Module dependency cycle");
    }
    return ordered;
}
template<class T> bool read(uintptr_t address, T& output) {
    SIZE_T count = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(address), &output, sizeof(T), &count) && count == sizeof(T);
}
}


