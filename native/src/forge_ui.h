#pragma once
#include "common.h"
namespace fx::forge {
struct Mod {
    std::string id,version,hash;
    std::wstring name,description,credits,status;
    size_t asset_count=0;
    bool jump_option=false,jump_active=true;
    bool estus_option=false,estus_active=true;
};
struct Layout { int x,y,width,height; float scale; };
inline Layout layout(RECT bounds,int percent,float le_scale=1.f) {
    const auto w=std::max(1L,bounds.right-bounds.left),h=std::max(1L,bounds.bottom-bounds.top);
    const float fit=std::min(w*.90f/1200.f,h*.90f/720.f);
    const float base=std::min(w/1600.f,h/1000.f);
    const float s=std::max(.10f,std::min(fit,base*std::clamp(percent,70,125)/100.f*std::clamp(le_scale,.75f,1.5f)));
    const int width=static_cast<int>(1200*s),height=static_cast<int>(720*s);
    return {static_cast<int>(bounds.left+(w-width)/2),static_cast<int>(bounds.top+(h-height)/2),width,height,s};
}
bool start(HMODULE module,const fs::path& root,std::vector<Mod> mods,bool seamless,void(*log)(const char*,const char*));
bool shutdown();
bool preview(const fs::path& assets,const fs::path& output,int width,int height,const std::string& mod_id="",int page=0);
bool title_texture(const fs::path& assets,const fs::path& output);
}
