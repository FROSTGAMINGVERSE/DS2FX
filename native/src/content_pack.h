#pragma once
#include "common.h"
#include "forge_settings.h"
#include "forge_ui.h"
#include <map>
namespace fx::content {
struct Asset { std::wstring key, target; std::string hash; };
struct Pack { std::string id,version,manifest_hash; fs::path directory; std::vector<Asset> assets; forge::Mod ui; };
inline std::wstring asset_key(std::wstring s) {
    for(auto& c:s) { if(c==L'\\') c=L'/'; if(c>=L'A' && c<=L'Z') c+=L'a'-L'A'; } return s;
}
inline bool permitted(const fs::path& p) {
    const auto s=asset_key(p.generic_wstring());
    if(s.empty() || s.front()==L'/' || s.find(L':')!=s.npos) return false;
    for(const auto& part:p) if(part==L".." || part==L".") return false;
    const auto ext=lower(p.extension().string());
    // Scholar map layouts are scoped to one canonical map ID and filename.
    // Do not admit navigation, events, loose collision or arbitrary map files.
    bool map_layout=false;
    if(s.size()==33 && s.starts_with(L"map/") && s[16]==L'/' && s.ends_with(L".msb")) {
        const auto id=s.substr(4,12);
        bool valid=id[0]==L'm';
        for(size_t i=1;i<id.size();++i) valid=valid && ((i==3||i==6||i==9)?id[i]==L'_':id[i]>=L'0'&&id[i]<=L'9');
        map_layout=valid && s.substr(17,12)==id;
    }
    // Native item icons are loose TPF requests. Keep this exception narrower
    // than the complete menu tree and reject folders or arbitrary TPF names.
    constexpr std::wstring_view icon_prefix=L"menu/tex/icon/ic_";
    bool icon=false;
    if(s.starts_with(icon_prefix) && s.ends_with(L".tpf")) {
        const auto digits=s.substr(icon_prefix.size(),s.size()-icon_prefix.size()-4);
        icon=digits.size()==10 && std::all_of(digits.begin(),digits.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';});
    }
    return s==L"enc_regulation.bnd.dcx" || (s.starts_with(L"param/") && ext==".param") ||
        (s.starts_with(L"menu/text/english/") && ext==".fmg") ||
        icon || map_layout ||
        // Audited player network with the private Lantern item-use branch.
        s==L"morpheme4/chr/c0001.anibnd.dcx" ||
        // Global SFX binder used by the owned lantern flame. Do not admit
        // arbitrary loose effects or executable files under the SFX tree.
        s==L"sfx/sfx9999.ffxbnd.dcx" ||
        (s.starts_with(L"model/") && (ext==".bnd" || ext==".dcx")) || (s.starts_with(L"filter/") && ext==".fltbnd");
}
// Complete gameplay overhauls need additional data classes. Approve only the
// audited, unchanged Seeker 2.1.5 Seamless manifest, not arbitrary mod folders.
// The seal covers every asset path and SHA256. Asset bytes are verified below.
inline constexpr char seeker_manifest_hash[] = "589c3029d753ff75f01407c98e2b719f5b3f9ff6855d1e24a2ba6b3c1b2e45c5";
inline constexpr char seeker_lantern_manifest_hash[] = "cbe7e6089d6d8d5182f237c240e16b6510191c95fb10ff3a8943994b26878f1f";
inline constexpr char lantern_seeker_manifest_hash[] = "e037ecb419906b33fb55d771af24a47c45e9c9fe60f32cdeb8b2c9979f0c2f98";
inline constexpr char lantern_standalone_manifest_hash[] = "6310048c54c96a831d8686badc4e9c6c73663bbf6c1d6e74c05243844a5c5725";
// One Lantern directory contains the independently audited Seeker variant.
// Selection never changes deployed files and is shared by preflight/runtime.
inline bool shared_seeker_variant(const fs::path& content) {
    const auto seeker=content/"seeker-of-fire"/"files.tsv";
    const auto lantern=content/"wayfarers-lantern"/"files.tsv";
    if(!fs::exists(seeker)||!fs::exists(lantern)) return false;
    no_links(seeker);no_links(lantern);
    require(!fs::exists(content/"modern-souls-2"),"Select either Modern Souls 2 or Seeker of Fire, not both");
    const auto seal=hash_file(seeker),light=hash_file(lantern);
    // Retain existing sealed paired packages. The shared package must use the
    // original author pack and the exact standalone Lantern donor.
    if(seal==seeker_lantern_manifest_hash && light==lantern_seeker_manifest_hash) return false;
    require(seal==seeker_manifest_hash && light==lantern_standalone_manifest_hash,
        "Shared Seeker/Lantern selection requires the reviewed original packs");
    return true;
}
inline bool approved_seeker(const std::string& id,const std::string& ver,const std::string& seal) {
    return id=="seeker-of-fire" && ver=="2.1.5" && (seal==seeker_manifest_hash || seal==seeker_lantern_manifest_hash);
}
inline bool approved_seeker_lantern(const Pack& pack) {
    return pack.id=="wayfarers-lantern" && pack.version=="1.0.7" && pack.manifest_hash==lantern_seeker_manifest_hash;
}
inline void verify_overhaul_pairs(const std::vector<Pack>& selected) {
    const bool seeker=std::any_of(selected.begin(),selected.end(),[](const auto& p){return p.id=="seeker-of-fire" && p.version=="2.1.5" && p.manifest_hash==seeker_lantern_manifest_hash;});
    const bool lantern=std::any_of(selected.begin(),selected.end(),[](const auto& p){return approved_seeker_lantern(p);});
    require(seeker==lantern,"Seeker/Lantern compatibility requires both matching audited packs; select DS2R Seeker Of Fire");
    require(!(std::any_of(selected.begin(),selected.end(),[](const auto& p){return p.id=="seeker-of-fire";}) &&
        std::any_of(selected.begin(),selected.end(),[](const auto& p){return p.id=="modern-souls-2";})),
        "Select either Modern Souls 2 or Seeker of Fire, not both");
}
inline bool pack_asset_permitted(const fs::path& p,const Pack& pack) {
    if(approved_seeker(pack.id,pack.version,pack.manifest_hash) && asset_key(p.generic_wstring()).starts_with(L"filter/")) return false;
    if(permitted(p)) return true;
    if(approved_seeker_lantern(pack) && asset_key(p.generic_wstring())==L"param/speffectactiveitem.emevd") return true;
    if(!approved_seeker(pack.id,pack.version,pack.manifest_hash)) return false;
    const auto s=asset_key(p.generic_wstring());
    if(s.empty() || s.front()==L'/' || s.find(L':')!=s.npos || s.starts_with(L"filter/")) return false;
    for(const auto& part:p) if(part==L".." || part==L".") return false;
    // These paths are already pinned by the manifest, but exclude executable
    // and configuration payloads independently as defense in depth.
    const auto ext=lower(p.extension().string());
    return ext!=".dll" && ext!=".exe" && ext!=".ini" && ext!=".ps1" && ext!=".bat" && ext!=".cmd";
}
inline std::wstring metadata(const fs::path& file,const wchar_t* key,const std::wstring& fallback) {
    wchar_t s[1024]{}; GetPrivateProfileStringW(L"Mod",key,fallback.c_str(),s,1024,file.c_str()); return s;
}
inline std::vector<Pack> packs(const fs::path& root) {
    const auto content=root/"Content"; no_links(content);
    if(!fs::exists(content)) return {};
    require(fs::is_directory(content),"Content must be a directory");
    std::vector<fs::path> directories;
    for(const auto& entry:fs::directory_iterator(content)) {
        no_links(entry.path()); require(entry.is_directory(),"Content may contain only pack directories");
        directories.push_back(entry.path()); require(directories.size()<=128,"Too many content packs");
    }
    std::sort(directories.begin(),directories.end());
    const bool shared=shared_seeker_variant(content);
    const auto integration=content/"wayfarers-lantern"/"integration"/"seeker-of-fire";
    std::set<std::wstring> global_keys; std::vector<Pack> result;
    for(const auto& directory:directories) {
        const auto name=directory.filename().string();
        const bool selected=shared && (name=="seeker-of-fire"||name=="wayfarers-lantern");
        const auto payload=selected && name=="wayfarers-lantern"?integration:directory;
        const auto manifest=selected?(name=="seeker-of-fire"?integration/"seeker-files.tsv":integration/"files.tsv"):directory/"files.tsv";
        no_links(manifest);
        require(fs::is_regular_file(manifest) && fs::file_size(manifest)<1024*1024,"Content manifest missing or oversized");
        if(selected) require(hash_file(manifest)==(name=="seeker-of-fire"?seeker_lantern_manifest_hash:lantern_seeker_manifest_hash),
            "Shared Lantern Seeker variant manifest missing or changed");
        std::ifstream input(manifest); std::string line,contract,id,pack_version,classification,extra;
        require(static_cast<bool>(std::getline(input,line)),"Empty content manifest");
        std::istringstream header(trim(line)); header>>contract>>id>>pack_version>>classification;
        require(!(header>>extra) && identifier(id) && id==directory.filename().string(),"Invalid content identity/folder");
        const bool legacy=contract=="DS2FX-CONTENT-1" && id=="vanilla-adp" && pack_version=="1.2b";
        require((legacy || (contract=="DS2FX-CONTENT-2" && semver(pack_version))) && classification=="shared-content-experimental","Unsupported content contract");
        Pack pack{id,pack_version,hash_file(manifest),directory,{},{}};
        require(id!="seeker-of-fire" || approved_seeker(id,pack_version,pack.manifest_hash),"Seeker content differs from reviewed 2.1.5 Seamless archive; do not combine or repack it");
        while(std::getline(input,line)) {
            line=trim(line); if(line.empty()) continue;
            auto split=line.find('\t'); require(split==64,"Invalid content manifest line");
            const auto hash=line.substr(0,split); const auto relative=utf8_path(line.substr(split+1));
            require(is_hash(hash) && pack_asset_permitted(relative,pack),"Invalid content asset path/hash");
            const auto key=asset_key(relative.generic_wstring());
            require(global_keys.insert(key).second || (id=="wayfarers-lantern"&&fs::is_regular_file(directory/"compatibility.tsv")),"Conflicting content asset: "+relative.string());
            const auto path=payload/"assets"/relative; no_links(path);
            require(fs::is_regular_file(path) && hash_file(path)==hash,"Content asset missing or changed: "+relative.string());
            auto absolute=fs::absolute(path).lexically_normal().wstring(); require(absolute.size()<2048,"Content asset path too long");
            pack.assets.push_back({key,absolute,hash}); require(global_keys.size()<=8192,"Too many content assets");
        }
        require(!pack.assets.empty(),"Empty content pack");
        const auto meta=directory/"forge.ini"; no_links(meta);
        if(fs::exists(meta)) require(fs::file_size(meta)<=16384,"Forge metadata too large");
        pack.ui.id=id; pack.ui.version=pack_version; pack.ui.hash=pack.manifest_hash; pack.ui.asset_count=pack.assets.size();
        pack.ui.name=metadata(meta,L"Name",directory.filename().wstring());
        pack.ui.description=metadata(meta,L"Description",L"A DS2FX content module.");
        pack.ui.credits=metadata(meta,L"Credits",L"See the module documentation.");
        pack.ui.status=L"Active / shared content";
        if(id=="modern-souls-2") {
            const auto variant=directory/"jump-off.tsv"; no_links(variant);
            require(fs::is_regular_file(variant) && fs::file_size(variant)<512,"Modern Souls 2 jump variant manifest missing");
            std::ifstream v(variant); std::string hash,path; v>>hash>>path;
            require(is_hash(hash) && path=="variants/standard-jump.bnd.dcx" && !(v>>extra),"Invalid jump variant");
            auto off=directory/utf8_path(path); no_links(off);
            require(hash_file(off)==hash,"Jump-off variant hash mismatch");
            auto asset=std::find_if(pack.assets.begin(),pack.assets.end(),[](const auto& a){return a.key==L"enc_regulation.bnd.dcx";});
            require(asset!=pack.assets.end(),"Modern Souls 2 regulation missing");
            pack.ui.jump_option=true; pack.ui.jump_active=forge::jump_enabled();
            if(!pack.ui.jump_active) { asset->target=fs::absolute(off).wstring();asset->hash=hash; }
            pack.ui.hash += pack.ui.jump_active?" / jump-on":" / jump-off";
            const auto estus_manifest=directory/"estus-off.tsv";no_links(estus_manifest);
            // 0.1 packs remain compatible. The 0.2 release requires both independent variants.
            require(pack_version!="0.2.0" || fs::is_regular_file(estus_manifest),"Modern Souls 2 Estus variants missing");
            if(fs::exists(estus_manifest)) {
                require(fs::is_regular_file(estus_manifest)&&fs::file_size(estus_manifest)<1024,"Invalid Estus variant manifest");
                std::ifstream e(estus_manifest);std::string mode,ehash,epath;
                std::map<std::string,Asset> options;
                while(e>>mode) {
                    require(static_cast<bool>(e>>ehash>>epath),"Incomplete Estus variant");
                    require((mode=="jump-on"||mode=="jump-off")&&is_hash(ehash)&&epath=="variants/estus-off-"+mode+".bnd.dcx","Invalid Estus variant");
                    const auto target=directory/utf8_path(epath);no_links(target);
                    require(fs::is_regular_file(target)&&hash_file(target)==ehash,"Estus variant hash mismatch");
                    auto absolute=fs::absolute(target).wstring();require(absolute.size()<2048,"Estus variant path too long");
                    require(options.emplace(mode,Asset{asset->key,absolute,ehash}).second,"Duplicate Estus variant");
                }
                require(options.size()==2,"Both Estus variants required");
                pack.ui.estus_option=true;pack.ui.estus_active=forge::estus_enabled();
                if(!pack.ui.estus_active)*asset=options.at(pack.ui.jump_active?"jump-on":"jump-off");
                pack.ui.hash+=pack.ui.estus_active?" / estus-on":" / estus-off";
                // Show the selected regulation identity, not only the unmodified manifest.
                pack.ui.hash=asset->hash+pack.ui.hash.substr(pack.manifest_hash.size());
            }
        }
        result.push_back(std::move(pack));
    }
    verify_overhaul_pairs(result);
    return result;
}
inline std::vector<Asset> load(const fs::path& root) {
    std::vector<Asset> out;std::map<std::wstring,std::pair<size_t,std::string>> routes;
    for(auto& pack:packs(root))for(auto asset:pack.assets) {
        const auto found=routes.find(asset.key);
        if(found==routes.end()){routes.emplace(asset.key,std::make_pair(out.size(),pack.id));out.push_back(std::move(asset));continue;}
        require(pack.id=="wayfarers-lantern"&&found->second.second=="modern-souls-2","Conflicting content packs");
        const auto file=pack.directory/"compatibility.tsv";no_links(file);
        require(fs::is_regular_file(file)&&fs::file_size(file)<16384,"Compatibility manifest missing/oversized");
        std::ifstream input(file);std::string contract,owner,base,key,before,after,path;
        input>>contract>>owner;require(contract=="DS2FX-INTEGRATION-1"&&owner==pack.id,"Invalid integration contract");
        bool matched=false;
        while(input>>base){require(static_cast<bool>(input>>key>>before>>after>>path),"Incomplete compatibility record");
            require(base=="modern-souls-2"&&is_hash(before)&&is_hash(after)&&permitted(utf8_path(key)),"Invalid compatibility record");
            const auto relative=utf8_path(path);require(!relative.is_absolute()&&path.starts_with("integration/")&&path.find(':')==path.npos,"Unsafe integration path");
            for(const auto& part:relative)require(part!=L".."&&part!=L".","Unsafe integration traversal");
            if(asset_key(utf8_path(key).wstring())!=asset.key||before!=out[found->second.first].hash)continue;
            require(!matched,"Ambiguous compatibility route");const auto target=pack.directory/relative;no_links(target);
            require(fs::is_regular_file(target)&&hash_file(target)==after,"Compatibility asset missing or changed");
            asset.target=fs::absolute(target).lexically_normal().wstring();asset.hash=after;matched=true;
        }
        require(matched,"No audited compatibility route for selected base asset");out[found->second.first]=std::move(asset);
    }
    return out;
}
}

