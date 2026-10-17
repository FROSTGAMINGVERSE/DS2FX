#pragma once
#include "forge_options.h"
#include <climits>
namespace fx::forge {
struct HudPresetField {std::wstring section,key;int fallback,low,high;};
inline std::vector<HudPresetField> hud_preset_fields(){
    std::vector<HudPresetField> fields;
    for(const auto& o:options("ds2-remastered-hud"))fields.push_back({L"ds2-remastered-hud",o.key,o.fallback,o.low,o.high});
    for(int i=0;i<4;++i){const auto section=L"HUD.Layout."+std::to_wstring(i);
        fields.push_back({section,L"X",-1,-1,10000});fields.push_back({section,L"Y",-1,-1,10000});
        fields.push_back({section,L"Size",100,25,400});fields.push_back({section,L"Length",100,25,300});}
    fields.push_back({L"HUD.Editor",L"Snap",1,0,1});fields.push_back({L"HUD.Editor",L"Grid",100,20,200});
    fields.push_back({L"HUD.Editor",L"GridVersion",2,2,2});return fields;
}
inline fs::path hud_preset_file(int slot){require(slot>=1 && slot<=6,"Invalid HUD preset slot");auto path=settings_file().parent_path()/"HUD-presets"/(L"preset-"+std::to_wstring(slot)+L".ini");no_links(path);return path;}
inline bool hud_preset_exists(int slot){try{return fs::is_regular_file(hud_preset_file(slot));}catch(...){return false;}}
// Replace one complete validated configuration, so a failed write cannot partially apply a preset.
inline bool hud_write_atomic(const fs::path& destination,const std::vector<HudPresetField>& fields,const std::vector<int>& values,bool preserve){
    auto temporary=destination;temporary+=L".tmp";
    try{no_links(destination);no_links(temporary);fs::create_directories(destination.parent_path());
        if(fs::exists(temporary))fs::remove(temporary);
        if(preserve && fs::exists(destination))fs::copy_file(destination,temporary);
        require(fields.size()==values.size(),"HUD preset field count mismatch");
        for(size_t i=0;i<fields.size();++i){const auto& f=fields[i];require(values[i]>=f.low && values[i]<=f.high,"Invalid HUD preset value");
            require(WritePrivateProfileStringW(f.section.c_str(),f.key.c_str(),std::to_wstring(values[i]).c_str(),temporary.c_str())!=FALSE,"HUD preset write failed");}
        if(!preserve)require(WritePrivateProfileStringW(L"Preset",L"Version",L"1",temporary.c_str())!=FALSE,"HUD preset version write failed");
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,temporary.c_str());
        require(MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE,"HUD preset replace failed");
        WritePrivateProfileStringW(nullptr,nullptr,nullptr,destination.c_str());return true;
    }catch(...){std::error_code ec;fs::remove(temporary,ec);return false;}
}
inline bool hud_preset_save(int slot){try{auto fields=hud_preset_fields();std::vector<int> values;
    for(const auto& f:fields)values.push_back(setting(settings_file(),f.section.c_str(),f.key.c_str(),f.fallback,f.low,f.high));
    return hud_write_atomic(hud_preset_file(slot),fields,values,false);}catch(...){return false;}}
inline bool hud_preset_load(int slot){try{const auto file=hud_preset_file(slot);require(fs::is_regular_file(file)&&fs::file_size(file)<16384,"HUD preset missing or oversized");
    require(setting(file,L"Preset",L"Version",0,1,1)==1,"Unsupported HUD preset");
    auto fields=hud_preset_fields();std::vector<int> values;for(const auto& f:fields){const int value=setting(file,f.section.c_str(),f.key.c_str(),INT_MIN,f.low,f.high);
        require(value!=INT_MIN,"Incomplete or malformed HUD preset");values.push_back(value);}
    return hud_write_atomic(settings_file(),fields,values,true);}catch(...){return false;}}
inline bool hud_theme(int theme){if(theme<0 || theme>3)return false;
    // Automatic native form, cold Hollow iron, warm Human ember, subdued Human ember.
    const int mode[]{0,1,2,2},embers[]{60,0,60,25},pulse[]{100,100,100,65};
    return hud_write_atomic(settings_file(),{{L"ds2-remastered-hud",L"SkinMode",0,0,2},{L"ds2-remastered-hud",L"EmberStrength",60,0,100},{L"ds2-remastered-hud",L"PulseStrength",100,0,100}},
        {mode[theme],embers[theme],pulse[theme]},true);
}
}
