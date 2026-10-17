#pragma once
#include "../../src/common.h"
#include <gdiplus.h>
#include <memory>

namespace remastered {
class Assets {
    std::map<std::pair<uint32_t,uint32_t>,std::unique_ptr<Gdiplus::Bitmap>> images;
    std::map<std::pair<uint32_t,uint32_t>,Gdiplus::Rect> bounds;
public:
    void clear() noexcept {images.clear();bounds.clear();}
    void load(const std::filesystem::path& folder) {
        clear();const auto manifest=folder/L"icons.tsv";
        if(!std::filesystem::exists(manifest))return;
        fx::no_links(manifest);fx::require(std::filesystem::file_size(manifest)<65536,"Icon manifest too large");
        std::ifstream input(manifest);std::string line;
        fx::require(std::getline(input,line) && fx::trim(line)=="DS2FX-HUD-ASSETS-1","Unknown icon manifest");
        unsigned count=0;
        while(std::getline(input,line)) {
            std::istringstream fields(fx::trim(line));std::string name,hash,extra;
            if(!(fields>>name>>hash) || (fields>>extra) || name.size()!=7 ||
               (name.substr(0,3)!="vi_" && name.substr(0,3)!="ei_") ||
               !std::all_of(name.begin()+3,name.end(),[](char c){return c>='0'&&c<='9';}) ||
               hash.size()!=64 || ++count>512)throw std::runtime_error("Invalid icon entry");
            const auto path=folder/(name+".png");fx::no_links(path);
            if(!std::filesystem::is_regular_file(path) || std::filesystem::file_size(path)>131072 || fx::hash_file(path)!=hash)continue;
            auto bitmap=std::make_unique<Gdiplus::Bitmap>(path.c_str(),FALSE);
            if(bitmap->GetLastStatus()!=Gdiplus::Ok || bitmap->GetWidth()!=64 || bitmap->GetHeight()!=64)continue;
            const auto key=std::pair{uint32_t(name[0]=='v'?16:15),uint32_t(std::stoul(name.substr(3)))};
            fx::require(!images.contains(key),"Duplicate icon key");
            int left=64,top=64,right=0,bottom=0;
            for(int y=0;y<64;++y)for(int x=0;x<64;++x){Gdiplus::Color pixel{};bitmap->GetPixel(x,y,&pixel);
                if(pixel.GetA()>8){left=std::min(left,x);top=std::min(top,y);right=std::max(right,x+1);bottom=std::max(bottom,y+1);}}
            bounds[key]=right>left&&bottom>top?Gdiplus::Rect(left,top,right-left,bottom-top):Gdiplus::Rect(0,0,64,64);
            images.emplace(key,std::move(bitmap));
        }
    }
    Gdiplus::Bitmap* get(uint32_t type,uint32_t id) const {
        const auto found=images.find({type,id});return found==images.end()?nullptr:found->second.get();
    }
    size_t size() const {return images.size();}
    Gdiplus::Rect content(uint32_t type,uint32_t id) const {
        const auto found=bounds.find({type,id});return found==bounds.end()?Gdiplus::Rect(0,0,64,64):found->second;
    }
};
}
