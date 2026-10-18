#include "../src/content_hook.h"
#include <iostream>
std::wstring seen;
extern "C" __declspec(dllexport) __declspec(noinline) uintptr_t DS2FX_TestResolve(uintptr_t context,fx::content::GameString* s) {
    seen=s?std::wstring(s->data,s->length):L"null";
    // Owned relative-path fixture: require routes to resolve the staged file.
    // This fixture does not model the engine's archive-mount return semantics.
    if(seen.size()>1 && seen[1]==L':')return 0;
    if(seen.starts_with(L"./"))
        fx::require(fx::fs::is_regular_file(fx::content::mount_root/fx::fs::path(seen.substr(2))),"Relative mount did not resolve the intended fixture file");
    return context+17;
}
extern "C" __declspec(dllexport) void DS2FX_TestGrow(fx::content::GameString* s,size_t capacity,size_t) {
    auto p=new wchar_t[capacity]; std::copy_n(s->data,s->length+1,p); delete[] s->data; s->data=p; s->capacity=capacity;
}
int redirects=0;
void log(const char*,const char* message) { if(std::string(message).starts_with("REDIRECT ")) ++redirects; }
int main() {
    namespace c=fx::content;
    try {
        c::Pack seeker{"seeker-of-fire","2.1.5",c::seeker_manifest_hash,{},{},{}};
        fx::require(c::approved_seeker(seeker.id,seeker.version,seeker.manifest_hash),"Pinned Seeker identity rejected");
        for(auto name:{L"ezstate/event_m10_02_00_00.esd",L"map/m10_02_00_00/m10_02_00_00.ngp",L"model_lq/map/m10_02_00_00.mapbhd",L"sound/frpg_sm10.fsb",L"sfx/sfx10.ffxbnd"})
            fx::require(c::pack_asset_permitted(name,seeker),"Pinned Seeker data rejected");
        for(auto name:{L"../escape.esd",L"C:/escape.esd",L"/escape.esd",L"map/../escape.esd",L"dinput8.dll",L"launcher.exe",L"settings.ini",L"filter/original.fltbnd",L"script.ps1"})
            fx::require(!c::pack_asset_permitted(name,seeker),"Unsafe Seeker asset accepted");
        for(auto field:{0,1,2}) {
            auto changed=seeker;
            if(field==0) changed.id="other-overhaul";
            if(field==1) changed.version="2.1.6";
            if(field==2) changed.manifest_hash=std::string(64,'0');
            fx::require(!c::pack_asset_permitted(L"ezstate/event_m10_02_00_00.esd",changed),"Unknown overhaul identity accepted");
        }
        auto pairedSeeker=seeker;pairedSeeker.manifest_hash=c::seeker_lantern_manifest_hash;
        c::Pack pairedLantern{"wayfarers-lantern","1.0.7",c::lantern_seeker_manifest_hash,{},{},{}};
        c::verify_overhaul_pairs({pairedSeeker,pairedLantern});
        c::verify_overhaul_pairs({seeker});
        c::Pack modernIdentity{"modern-souls-2","0.2.1",{}, {},{}, {}};
        bool mixedOverhauls=false;try{c::verify_overhaul_pairs({seeker,modernIdentity});}catch(...){mixedOverhauls=true;}
        fx::require(mixedOverhauls,"Mutually exclusive overhauls accepted");
        fx::require(c::pack_asset_permitted(L"Param/SpEffectActiveItem.emevd",pairedLantern),"Audited Lantern event route rejected");
        auto changedLantern=pairedLantern;changedLantern.manifest_hash=std::string(64,'0');
        fx::require(!c::pack_asset_permitted(L"Param/SpEffectActiveItem.emevd",changedLantern),"Unreviewed Lantern event route accepted");
        for(auto selection:{std::vector<c::Pack>{pairedSeeker},std::vector<c::Pack>{pairedLantern},std::vector<c::Pack>{pairedSeeker,changedLantern},std::vector<c::Pack>{seeker,pairedLantern}}) {
            bool rejected=false;try{c::verify_overhaul_pairs(selection);}catch(...){rejected=true;}
            fx::require(rejected,"Incomplete or mixed Seeker/Lantern pair accepted");
        }
        fx::require(c::permitted(L"map/m10_02_00_00/m10_02_00_00.msb")&&c::permitted(L"MAP/M10_02_00_00/M10_02_00_00.MSB"),"Scholar map rejected");
        for(auto name:{L"map/m10_02_00_00/m10_04_00_00.msb",L"map/m10_02_00_00/m10_02_00_00.ngp",L"map/m10_02_00_00/m10_02_00_00.dll",L"map/m10_02_00_00/sub/m10_02_00_00.msb",L"map/m10_x2_00_00/m10_x2_00_00.msb",L"map/m10_02_00_00/../m10_02_00_00.msb"})
            fx::require(!c::permitted(name),"Unscoped map asset accepted");
        fx::require(c::permitted(L"menu/tex/icon/ic_0060425000.tpf")&&c::permitted(L"MENU/TEX/ICON/IC_0060425000.TPF"),"Native icon rejected");
        for(auto name:{L"menu/tex/icon/ic_0060425000.dll",L"menu/tex/icon/ic_0060425000/evil.tpf",L"menu/tex/icon/ic_006042500x.tpf",L"menu/tex/icon/ic_1.tpf",L"menu/tex/icon/../ic_0060425000.tpf",L"menu/tex/other.tpf"})
            fx::require(!c::permitted(name),"Unsafe icon path accepted");
        fx::require(c::permitted(L"sfx/sfx9999.ffxbnd.dcx"),"Global effect binder rejected");
        for(auto name:{L"sfx/sfx9999.dll",L"sfx/sfx0000.ffxbnd.dcx",L"sfx/sfx9999.ffx",L"sfx/../sfx9999.ffxbnd.dcx",L"sfx/sub/sfx9999.ffxbnd.dcx"})
            fx::require(!c::permitted(name),"Unscoped effect path accepted");
        fx::require(c::permitted(L"morpheme4/chr/c0001.anibnd.dcx")&&c::permitted(L"MORPHEME4/CHR/C0001.ANIBND.DCX"),"Player animation binder rejected");
        for(auto name:{L"morpheme4/chr/c0002.anibnd.dcx",L"morpheme4/chr/c0001.nmb",L"morpheme4/chr/c0001.dll",L"morpheme4/chr/c0001/evil.anibnd.dcx",L"morpheme4/chr/../c0001.anibnd.dcx"})
            fx::require(!c::permitted(name),"Unscoped animation path accepted");
        auto root=fx::fs::temp_directory_path()/("DS2FX-content-test-"+std::to_string(GetCurrentProcessId()));
        auto selectionRoot=root/"shared-selection"/"Content";
        fx::fs::create_directories(selectionRoot/"wayfarers-lantern");
        std::ofstream(selectionRoot/"wayfarers-lantern"/"files.tsv")<<"unreviewed donor";
        fx::require(!c::shared_seeker_variant(selectionRoot),"Embedded Seeker variant enabled without Seeker");
        fx::fs::create_directories(selectionRoot/"seeker-of-fire");
        std::ofstream(selectionRoot/"seeker-of-fire"/"files.tsv")<<"unreviewed overhaul";
        bool badDonor=false;try{c::shared_seeker_variant(selectionRoot);}catch(...){badDonor=true;}
        fx::require(badDonor,"Unreviewed shared variant donors accepted");
        fx::fs::create_directories(selectionRoot/"modern-souls-2");
        bool mixedSelection=false;try{c::shared_seeker_variant(selectionRoot);}catch(...){mixedSelection=true;}
        fx::require(mixedSelection,"Shared selection allowed Modern Souls plus Seeker");
        auto dir=root/"Content"/"vanilla-adp";
        fx::fs::create_directories(dir/"assets"/"Param");
        const auto file=dir/"assets"/"Param"/"test.param";
        std::ofstream(file)<<"fixture content";
        const std::string header="DS2FX-CONTENT-1 vanilla-adp 1.2b shared-content-experimental\n";
        const auto line=fx::hash_file(file)+"\tParam/test.param\n";
        auto manifest=[&](std::string body){std::ofstream(dir/"files.tsv")<<header<<body;};
        auto reject=[&](std::string body){manifest(body); bool refused=false;try {c::load(root);}catch(...){refused=true;}fx::require(refused,"Invalid manifest accepted");};
        fx::require(c::load(root/"absent").empty(),"Absent content not optional");
        reject(line+line);
        reject(fx::hash_file(file)+"\t../escape.param\n");
        reject(fx::hash_file(file)+"\tC:/test.param\n");
        reject(fx::hash_file(file)+"\tdinput8.dll\n");
        reject(std::string(64,'0')+"\tParam/test.param\n");
        manifest(line); fx::require(c::load(root).size()==1,"Valid manifest rejected");
        fx::fs::create_directories(dir/"assets"/"sfx");
        const auto sfx=dir/"assets"/"sfx"/"sfx9999.ffxbnd.dcx";
        std::ofstream(sfx)<<"owned effect binder fixture";
        manifest(line+fx::hash_file(sfx)+"\tsfx/sfx9999.ffxbnd.dcx\n");
        fx::require(c::load(root).size()==2,"Mixed parameter/effect manifest rejected");
        c::install(root,log);
        {
            const auto prior_original=c::original;const auto prior_mods=c::active_mods.size();
            c::install(root,log);
            fx::require(c::original==prior_original && c::active_mods.size()==prior_mods,"Runtime initialization replaced an early hook or duplicated its selected mods");
            std::ofstream(file)<<"changed after early content installation";
            manifest(fx::hash_file(file)+"\tParam/test.param\n"+fx::hash_file(sfx)+"\tsfx/sfx9999.ffxbnd.dcx\n");
            bool changed=false;try{c::install(root,log);}catch(...){changed=true;}
            fx::require(changed,"Changed pre-entry selection silently replaced the running routes");
            std::ofstream(file)<<"fixture content";
            manifest(line+fx::hash_file(sfx)+"\tsfx/sfx9999.ffxbnd.dcx\n");
        }
        fx::require(c::mounted_target(file).starts_with(L"./Content/"),"Native relative mount prefix missing");
        for(const auto& outside:{root.parent_path()/"outside.param",root.parent_path()/(root.filename().wstring()+L"-other")/"file.param"}) {
            bool rejected=false;try{c::mounted_target(outside);}catch(...){rejected=true;}
            fx::require(rejected,"Content target outside exact game mount accepted");
        }
        {
            const auto absolute=fx::fs::absolute(file).wstring();c::GameString path{new wchar_t[absolute.size()+1],nullptr,absolute.size(),absolute.size()+1};
            std::copy_n(absolute.c_str(),absolute.size()+1,path.data);
            fx::require(DS2FX_TestResolve(25,&path)==0,"Owned native mount fixture incorrectly accepted a Windows drive");delete[] path.data;
        }
        auto resolve=reinterpret_cast<c::Resolve>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_TestResolve"));
        for(auto name:{L"gamedata:/Param/test.param",L"title:/PARAM/TEST.PARAM",L"gamedata:/Param/other.param",L"save:/Param/test.param"}) {
            size_t n=wcslen(name); c::GameString s{new wchar_t[n+1],nullptr,n,n+1}; std::copy_n(name,n+1,s.data);
            auto result=resolve(25,&s); fx::require(result==42,"Original resolver not preserved");
            bool match=std::wstring(name).find(L"other")==std::wstring::npos && !std::wstring(name).starts_with(L"save:");
            fx::require(seen==(match?c::mounted_target(fx::fs::absolute(file)):std::wstring(name)),"Incorrect routing");
            delete[] s.data;
        }
        fx::require(resolve(25,nullptr)==42 && seen==L"null","Null passthrough broken");
        fx::require(redirects==0 && c::reported.empty(),"Default asset routing performed diagnostic logging");
        c::capture_requests=true;
        {
            const auto name=L"gamedata:/Param/test.param";size_t n=wcslen(name);
            c::GameString path{new wchar_t[n+1],nullptr,n,n+1};std::copy_n(name,n+1,path.data);
            fx::require(resolve(25,&path)==42 && seen==c::mounted_target(file),"Request capture changed native routing");delete[] path.data;
            const auto& event=c::request_trace[0];
            fx::require(c::request_count==1 && event.phase==2 && event.result==42 && std::wstring(event.input)==name && std::wstring(event.output)==c::mounted_target(file),"Bounded request capture omitted actual input/output/result");
        }
        c::capture_requests=false;
        c::trace_redirects=true;
        for(int attempt=0;attempt<2;++attempt) {
            const auto name=L"gamedata:/Param/test.param";size_t n=wcslen(name);
            c::GameString s{new wchar_t[n+1],nullptr,n,n+1};std::copy_n(name,n+1,s.data);
            fx::require(resolve(25,&s)==42 && seen==c::mounted_target(fx::fs::absolute(file)),"Tracing changed routing");delete[] s.data;
        }
        fx::require(redirects==1,"Opt-in tracing did not report exactly once per asset");
        c::trace_redirects=false;

        for(auto name:{L"gamedata:/sfx/sfx9999.ffxbnd.dcx",L"title:/SFX/SFX9999.FFXBND.DCX"}) {
            size_t n=wcslen(name);c::GameString s{new wchar_t[n+1],nullptr,n,n+1};std::copy_n(name,n+1,s.data);
            fx::require(resolve(25,&s)==42 && seen==c::mounted_target(fx::fs::absolute(sfx)),"Native global effect binder routing failed");
            delete[] s.data;
        }
        fx::require(MH_DisableHook(c::address)==MH_OK,"Cannot disable fixture hook");
        const auto map=dir/"assets"/"map"/"m10_02_00_00"/"m10_02_00_00.msb";
        fx::fs::create_directories(map.parent_path());std::ofstream(map)<<"owned map resolver fixture";
        manifest(line+fx::hash_file(sfx)+"\tsfx/sfx9999.ffxbnd.dcx\n"+fx::hash_file(map)+"\tmap/m10_02_00_00/m10_02_00_00.msb\n");
        c::routes=c::resolved_routes(root);fx::require(c::routes.size()==3,"Map did not enter audited route selection");
        fx::require(MH_EnableHook(c::address)==MH_OK,"Cannot enable map fixture hook");
        for(auto name:{L"gamedata:/map/m10_02_00_00/m10_02_00_00.msb",L"title:/MAP/M10_02_00_00/M10_02_00_00.MSB",L"gamedata:/map/m10_04_00_00/m10_04_00_00.msb",L"save:/map/m10_02_00_00/m10_02_00_00.msb"}) {
            size_t n=wcslen(name);c::GameString s{new wchar_t[n+1],nullptr,n,n+1};std::copy_n(name,n+1,s.data);
            fx::require(resolve(25,&s)==42,"Map routing changed original return");
            bool hit=!std::wstring(name).starts_with(L"save:")&&std::wstring(name).find(L"m10_04")==std::wstring::npos;
            fx::require(seen==(hit?c::mounted_target(fx::fs::absolute(map)):std::wstring(name)),"Map routing/passthrough failed");delete[] s.data;
        }
        fx::require(MH_DisableHook(c::address)==MH_OK,"Cannot disable map fixture hook");
        auto modernroot=root/"modern-test";auto modern=modernroot/"Content"/"modern-souls-2";
        fx::fs::create_directories(modern/"assets");fx::fs::create_directories(modern/"variants");
        std::ofstream(modern/"assets"/"enc_regulation.bnd.dcx")<<"jump on fixture";
        std::ofstream(modern/"variants"/"standard-jump.bnd.dcx")<<"jump off fixture";
        const auto onhash=fx::hash_file(modern/"assets"/"enc_regulation.bnd.dcx");
        const auto offhash=fx::hash_file(modern/"variants"/"standard-jump.bnd.dcx");
        std::ofstream(modern/"files.tsv")<<"DS2FX-CONTENT-2 modern-souls-2 0.1.0 shared-content-experimental\n"<<onhash<<"\tenc_regulation.bnd.dcx\n";
        std::ofstream(modern/"jump-off.tsv")<<offhash<<"\tvariants/standard-jump.bnd.dcx\n";
        wchar_t prior[32768]{};GetEnvironmentVariableW(L"LOCALAPPDATA",prior,32768);
        SetEnvironmentVariableW(L"LOCALAPPDATA",modernroot.c_str());
        auto packs=c::packs(modernroot);fx::require(packs.size()==1 && packs[0].ui.jump_active && packs[0].assets[0].hash==onhash,"V2 catalog/default jump failed");
        fx::require(fx::forge::save_setting(L"modern-souls-2",L"DedicatedJump",0),"Fixture setting write failed");
        packs=c::packs(modernroot);fx::require(!packs[0].ui.jump_active && packs[0].assets[0].hash==offhash,"Restart variant selection failed");
        std::ofstream(modern/"variants"/"estus-off-jump-on.bnd.dcx")<<"jump on estus off";
        std::ofstream(modern/"variants"/"estus-off-jump-off.bnd.dcx")<<"jump off estus off";
        const auto eon=fx::hash_file(modern/"variants"/"estus-off-jump-on.bnd.dcx");
        const auto eoff=fx::hash_file(modern/"variants"/"estus-off-jump-off.bnd.dcx");
        const auto elines="jump-on\t"+eon+"\tvariants/estus-off-jump-on.bnd.dcx\njump-off\t"+eoff+"\tvariants/estus-off-jump-off.bnd.dcx\n";
        std::ofstream(modern/"files.tsv")<<"DS2FX-CONTENT-2 modern-souls-2 0.2.0 shared-content-experimental\n"<<onhash<<"\tenc_regulation.bnd.dcx\n";
        bool missing=false;try{c::packs(modernroot);}catch(...){missing=true;}fx::require(missing,"0.2 pack missing Estus variants accepted");
        std::ofstream(modern/"estus-off.tsv")<<elines;
        for(int jump:{0,1})for(int estus:{0,1}) {
            fx::require(fx::forge::save_setting(L"modern-souls-2",L"DedicatedJump",jump)&&fx::forge::save_setting(L"modern-souls-2",L"WalkingEstus",estus),"Cannot save combination");
            packs=c::packs(modernroot);const auto expected=estus?(jump?onhash:offhash):(jump?eon:eoff);
            fx::require(packs[0].ui.estus_option&&packs[0].ui.estus_active==bool(estus)&&packs[0].ui.jump_active==bool(jump)&&packs[0].assets[0].hash==expected,"Independent jump/Estus routing failed");
        }
        auto lantern=modernroot/"Content"/"wayfarers-lantern";
        fx::fs::create_directories(lantern/"assets");fx::fs::create_directories(lantern/"integration");
        std::ofstream(lantern/"assets"/"enc_regulation.bnd.dcx")<<"standalone lantern";
        std::ofstream(lantern/"integration"/"merged.bnd.dcx")<<"audited merged lantern";
        const auto standalone=fx::hash_file(lantern/"assets"/"enc_regulation.bnd.dcx"),merged=fx::hash_file(lantern/"integration"/"merged.bnd.dcx");
        std::ofstream(lantern/"files.tsv")<<"DS2FX-CONTENT-2 wayfarers-lantern 1.0.0 shared-content-experimental\n"<<standalone<<"\tenc_regulation.bnd.dcx\n";
        const auto ih="DS2FX-INTEGRATION-1 wayfarers-lantern\n";
        std::string integrations=ih;for(auto hash:{onhash,offhash,eon,eoff})integrations+="modern-souls-2\tenc_regulation.bnd.dcx\t"+hash+"\t"+merged+"\tintegration/merged.bnd.dcx\n";
        std::ofstream(lantern/"compatibility.tsv")<<integrations;
        fx::require(MH_EnableHook(c::address)==MH_OK,"Cannot enable integration fixture hook");
        for(int jump:{0,1})for(int estus:{0,1}) {
            fx::forge::save_setting(L"modern-souls-2",L"DedicatedJump",jump);fx::forge::save_setting(L"modern-souls-2",L"WalkingEstus",estus);
            const auto routes=c::load(modernroot);fx::require(routes.size()==1&&routes[0].hash==merged,"Four-variant audited integration failed");
            c::routes=c::resolved_routes(modernroot);
            const wchar_t* name=L"gamedata:/enc_regulation.bnd.dcx";
            const size_t n=wcslen(name);c::GameString path{new wchar_t[n+1],nullptr,n,n+1};std::copy_n(name,n+1,path.data);
            fx::require(resolve(25,&path)==42&&seen==c::mounted_target(fx::fs::absolute(lantern/"integration"/"merged.bnd.dcx")),"Runtime hook discarded the audited Lantern integration");
            delete[] path.data;

        }
        fx::require(MH_DisableHook(c::address)==MH_OK,"Cannot disable integration fixture hook");
        std::ofstream(lantern/"compatibility.tsv")<<ih;
        bool unmatched=false;try{c::load(modernroot);}catch(...){unmatched=true;}fx::require(unmatched,"Unaudited duplicate route accepted");
        std::ofstream(lantern/"compatibility.tsv")<<integrations;
        auto standaloneRoot=root/"standalone-test";fx::fs::create_directories(standaloneRoot/"Content");
        fx::fs::copy(lantern,standaloneRoot/"Content"/"wayfarers-lantern",fx::fs::copy_options::recursive);
        const auto alone=c::load(standaloneRoot);fx::require(alone.size()==1&&alone[0].hash==standalone,"Standalone Lantern requires Modern Souls 2");
        std::ofstream(lantern/"integration"/"merged.bnd.dcx",std::ios::app)<<"tamper";
        bool tampered=false;try{c::load(modernroot);}catch(...){tampered=true;}fx::require(tampered,"Changed compatibility payload accepted");
        auto badEstus=[&](const std::string& value){std::ofstream(modern/"estus-off.tsv")<<value;bool bad=false;try{c::packs(modernroot);}catch(...){bad=true;}fx::require(bad,"Invalid Estus manifest accepted");};
        badEstus(elines+elines);badEstus("jump-on "+eon+" ../escape.bnd.dcx");badEstus("jump-on");
        badEstus("jump-on "+std::string(64,'0')+" variants/estus-off-jump-on.bnd.dcx\njump-off "+eoff+" variants/estus-off-jump-off.bnd.dcx");
        std::ofstream(modern/"estus-off.tsv")<<elines;
        std::ofstream(modern/"variants"/"standard-jump.bnd.dcx",std::ios::app)<<"tamper";
        bool refused=false;try{c::packs(modernroot);}catch(...){refused=true;}fx::require(refused,"Tampered variant accepted");
        SetEnvironmentVariableW(L"LOCALAPPDATA",prior);
        std::cout<<"PASS content: manifest integrity, path/extension restrictions, duplicate rejection, real hook routing, grow, title/gamedata and fallback\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what();return 1;}
}

