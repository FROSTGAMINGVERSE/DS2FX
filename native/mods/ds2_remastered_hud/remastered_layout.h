#pragma once
#include <windows.h>
#include <array>
#include <algorithm>
#include <cmath>
namespace remastered {
inline constexpr ULONG_PTR layout_message=0x4453484c;
inline constexpr wchar_t forge_class[]=L"DS2FX.Forge.Menu.1";
struct LayoutItem {int x=-1,y=-1,size=100,length=100;};
struct HudLayout {std::array<LayoutItem,4> items{{{},{},{},{-1,-1,50,100}}};};
struct LayoutPreview {HudLayout layout;int editing=0;};
struct Box {float x,y,w,h;};
inline constexpr float part_x[]={0,60,60,60},part_y[]={0,9,31,58};
inline constexpr float part_w[]={64,360,274,364},part_h[]={64,16,16,134};
inline constexpr const wchar_t* part_names[]={L"Covenant",L"Health",L"Stamina",L"Status effects"};
inline bool valid_layout(const HudLayout& layout) {
    for(const auto& i:layout.items)if(i.x < -1 || i.x>10000 || i.y < -1 || i.y>10000 || i.size<25 || i.size>400 || i.length<25 || i.length>300)return false;
    return true;
}
inline float stat_length(int part,int hp,float stamina,bool enabled){
    if(!enabled)return 1.f;
    // Missing native gauge data keeps the last authored geometry; raw engine SP is not menu SP.
    return 1.f;
}
inline Box part_box(int part,int width,int height,int scale,int margin,const HudLayout& layout,unsigned effects=20,int hp=1400,float stamina=120,bool stats=false,float native_hp=0,float native_sp=0) {
    const float unit=std::min(width/1600.f,height/900.f)*std::clamp(scale/100.f,.5f,2.f);
    const auto& item=layout.items[part];const int size=(part==1 || part==2)?layout.items[1].size:item.size;
    float factor=unit*size/100.f;
    float w=part_w[part],h=part_h[part];
    if(part==1 || part==2){const float native=part==1?native_hp:native_sp;
        // Native spans are fractions of the reference screen. A compact half
        // of the 1600px reference gives both gauges the same 800px conversion.
        // Do not saturate ordinary spans to one common maximum length.
        if(stats && std::isfinite(native) && native>0){w=std::clamp(native*800.f,40.f,800.f)/(size/100.f);}
        else w*=stat_length(part,hp,stamina,stats)*item.length/100.f;}
    if(part==3){const auto count=std::clamp(effects,1u,20u);w=42+46*(std::min(8u,count)-1);h=42+46*((count-1)/8);}
    factor=std::min(factor,std::min((width-6.f)/w,(height-6.f)/h));
    const float x=item.x<0?(margin+part_x[part])*unit:width*item.x/10000.f;
    const float y=item.y<0?(margin+part_y[part])*unit:height*item.y/10000.f;
    return {std::clamp(x,3.f,std::max(3.f,width-w*factor-3)),std::clamp(y,3.f,std::max(3.f,height-h*factor-3)),w*factor,h*factor};
}
inline float magnetic(float value,int grid,float tolerance){
    const float nearest=std::round(value/grid)*grid;return std::abs(nearest-value)<=tolerance?nearest:value;
}
inline void move_part(LayoutItem& item,const Box& start,float dx,float dy,int width,int height,bool snap,int grid) {
    float x=start.x+dx,y=start.y+dy;
    if(snap){const float tolerance=std::min(grid*.3f,8.f*std::min(width/1600.f,height/900.f));x=magnetic(x,grid,tolerance);y=magnetic(y,grid,tolerance);}
    x=std::clamp(x,3.f,std::max(3.f,width-start.w-3));y=std::clamp(y,3.f,std::max(3.f,height-start.h-3));
    item.x=static_cast<int>(std::lround(x/width*10000));item.y=static_cast<int>(std::lround(y/height*10000));
}
inline RECT layout_bounds(int width,int height,int scale,int margin,const HudLayout& layout,unsigned effects,int hp=1400,float stamina=120,bool stats=false,float native_hp=0,float native_sp=0) {
    RECT result{width,height,0,0};
    for(int i=0;i<4;++i){if(i==3 && !effects)continue;const auto b=part_box(i,width,height,scale,margin,layout,effects,hp,stamina,stats,native_hp,native_sp);
        const float pad=std::max(6.f,b.h/20+4);
        result.left=std::min(result.left,static_cast<LONG>(std::floor(b.x-pad)));result.top=std::min(result.top,static_cast<LONG>(std::floor(b.y-pad)));
        result.right=std::max(result.right,static_cast<LONG>(std::ceil(b.x+b.w+pad)));result.bottom=std::max(result.bottom,static_cast<LONG>(std::ceil(b.y+b.h+pad)));}
    result.left=std::max(0L,result.left);result.top=std::max(0L,result.top);result.right=std::min(static_cast<LONG>(width),result.right);result.bottom=std::min(static_cast<LONG>(height),result.bottom);return result;
}
// A same-process Forge window owned by this game's window may hold focus.
inline bool forge_focus(HWND candidate,HWND game) {
    DWORD pid=0;GetWindowThreadProcessId(candidate,&pid);wchar_t name[64]{};GetClassNameW(candidate,name,64);
    return pid==GetCurrentProcessId() && GetWindow(candidate,GW_OWNER)==game && wcscmp(name,forge_class)==0 && IsWindowVisible(candidate);
}
}
