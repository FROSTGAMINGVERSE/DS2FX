#pragma once
#include "../../sdk/ds2fx_api.h"
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cwchar>
#include "remastered_assets.h"
#include "remastered_skin.h"
#include "remastered_animation.h"
#include "remastered_layout.h"

namespace remastered {
inline constexpr wchar_t window_class[] = L"DS2FX.RemasteredHUD.1";
inline constexpr float slot_radius=48.0f,slot_step=64.0f,slot_halo=6.0f;
inline float quickmenu_anchor_y(int height,float edge,float scale) {
    return height-edge-(slot_step+slot_radius+slot_halo)*scale;
}
struct Settings {
    HudLayout layout;
    int offset_x=0,offset_y=0;
    int enabled=1;
    int scale=100;
    int margin=24;
    int opacity=245;
    int pulse=1;
    int diagnostic_ids=0;
    int show_quickslots=0;
    int show_souls=0;
    int pulse_strength=100,ember_strength=60,animation_fps=60,icon_size=20;
    int health_hold=260,health_drain=250,stamina_hold=160,stamina_drain=180;
    int stat_lengths=1,shadow_strength=40;float native_hp=0,native_sp=0;
    int skin_mode=0; // 0 native form, 1 Hollow preview, 2 Human preview
};
inline SIZE render_bounds(int width,int height,const Settings& settings,unsigned effects) {
    if(settings.show_quickslots || settings.show_souls)return {width,height};
    const float scale=std::clamp(settings.scale/100.f,0.5f,2.f)*std::min(width/1600.f,height/900.f);
    const unsigned rows=effects?std::min(3u,(effects+7)/8):0;
    const float bottom=rows?70.f+(rows-1)*46.f+42.f:68.f;
    return {std::min(width,static_cast<int>(std::ceil((settings.margin+428)*scale))),
        std::min(height,static_cast<int>(std::ceil((settings.margin+bottom+5)*scale)))};
}
inline bool visible(const DS2FX_PlayerState& player,uint64_t age_ms,bool /*focused*/,bool minimized) {
    return player.available && player.max_health>0 && player.health>=0 &&
        player.health<=player.max_health && age_ms<=1500 && !minimized;
}
inline bool replacement_owned(const DS2FX_PlayerState& player,const DS2FX_HudState& hud,
                              const DS2FX_HudPresentation& native,uint64_t age,bool enabled) {
    return enabled && visible(player,age,true,false) && player.character_epoch &&
        hud.character_epoch==player.character_epoch && hud.sample==player.sample &&
        native.available && native.character_epoch==player.character_epoch && native.sample==player.sample && native.opacity<=255;
}
inline bool gameplay_scene(const DS2FX_HudState& hud) {
    return (hud.available_fields & DS2FX_HUD_SCENE) && hud.game_state==30 && hud.loading==0;
}
inline bool scene_settled(uint64_t now,uint64_t ready_since) {
    return ready_since && now>=ready_since && now-ready_since>=1200;
}
inline float fraction(float value,float maximum) {
    return std::isfinite(value) && std::isfinite(maximum) && maximum>0 ?
        std::clamp(value/maximum,0.0f,1.0f) : 0.0f;
}
inline void text(Gdiplus::Graphics& g,const wchar_t* value,float x,float y,float w,float h,
                 float size,Gdiplus::Color color,bool centered=false) {
    Gdiplus::FontFamily family(L"Georgia");
    Gdiplus::Font font(&family,size,Gdiplus::FontStyleRegular,Gdiplus::UnitPixel);
    Gdiplus::SolidBrush brush(color);
    Gdiplus::StringFormat format;
    format.SetAlignment(centered ? Gdiplus::StringAlignmentCenter : Gdiplus::StringAlignmentNear);
    format.SetLineAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(value,-1,&font,Gdiplus::RectF(x,y,w,h),&format,&brush);
}
inline void slot(Gdiplus::Graphics& g,float cx,float cy,const wchar_t* label,
                 bool selected,bool known,int32_t item_id,float pulse,bool diagnostic_ids=false) {
    using namespace Gdiplus;
    const float r=slot_radius;
    PointF points[4]{{cx,cy-r},{cx+r,cy},{cx,cy+r},{cx-r,cy}};
    Color edge=selected ? Color(static_cast<BYTE>(145+70*pulse),223,137,58) : Color(170,111,100,83);
    SolidBrush shadow(Color(130,5,5,7)),body(Color(195,17,17,19));
    Pen outer(edge,2.2f),inner(Color(105,199,181,139),1.0f);
    if(selected) {
        PointF halo[4]{{cx,cy-r-5},{cx+r+5,cy},{cx,cy+r+5},{cx-r-5,cy}};
        Pen glow(Color(static_cast<BYTE>(28+34*pulse),255,142,53),3.0f);
        g.DrawPolygon(&glow,halo,4);
    }
    g.FillPolygon(&shadow,points,4);
    g.DrawPolygon(&outer,points,4);
    PointF inside[4]{{cx,cy-r+7},{cx+r-7,cy},{cx,cy+r-7},{cx-r+7,cy}};
    g.FillPolygon(&body,inside,4);
    g.DrawPolygon(&inner,inside,4);
    text(g,label,cx-31,cy-18,62,19,11,Color(245,211,199,173),true);
    if (known && diagnostic_ids) {
        wchar_t value[32]{};
        if (item_id<0) wcscpy_s(value,L"EMPTY");
        else swprintf_s(value,L"%d",item_id);
        text(g,value,cx-31,cy+3,62,19,10,Color(230,228,213,181),true);
    } else if (!known) {
        text(g,L"--",cx-31,cy+3,62,19,11,Color(150,162,153,138),true);
    }
}
inline void bar(Gdiplus::Graphics& g,float x,float y,float width,float height,float value,
                Gdiplus::Color dark,Gdiplus::Color bright,Gdiplus::Color glow,float pulse,
                bool energy=false,float phase=0.0f,float trailing=0.0f,float consumption=0.0f) {
    using namespace Gdiplus;
    const float filled=width*std::clamp(value,0.0f,1.0f);
    SolidBrush track(Color(235,9,10,11));
    Pen rim(Color(240,48,44,38),1.0f),shade(Color(230,5,5,6),1.0f);
    RectF outer(x,y,width,height);
    g.FillRectangle(&track,outer);
    g.DrawRectangle(&rim,x,y,width,height);
    const float lost=width*std::clamp(trailing,value,1.0f);
    if(lost>filled+0.1f) {
        const BYTE grey=static_cast<BYTE>(112+60*std::clamp(consumption,0.f,1.f));
        SolidBrush delayed(energy?Color(255,grey,grey,grey):Color(255,101,44,48));
        g.FillRectangle(&delayed,x+std::max(2.0f,filled-2),y+2,
            std::max(0.0f,lost-2-std::max(2.0f,filled-2)),height-4);
    }
    if (filled>4.0f) {
        LinearGradientBrush gradient(PointF(x,y),PointF(x+width,y),dark,bright);
        g.FillRectangle(&gradient,x+2,y+2,std::max(0.0f,filled-4),height-4);
        const auto saved=g.Save();
        g.SetClip(RectF(x+2,y+2,std::max(0.0f,filled-4),height-4),CombineModeIntersect);
        Color tones[4]{Color(75,255,245,221),Color(8,255,255,255),Color(5,0,0,0),Color(100,0,0,0)};
        REAL stops[4]{0.0f,0.28f,0.65f,1.0f};
        LinearGradientBrush depth(PointF(x,y),PointF(x,y+height),tones[0],tones[3]);
        depth.SetInterpolationColors(tones,stops,4);
        g.FillRectangle(&depth,x+2,y+2,filled-4,height-4);
        if(energy && pulse>0) {
            // A dim travelling highlight suggests energy without particles
            // or motion outside the currently available stamina fill.
            const float band=20.0f;
            const float center=x+std::fmod(phase*(width+band),width+band)-band;
            Color flow[3]{Color(0,205,255,175),Color(static_cast<BYTE>(35+45*pulse),205,255,175),Color(0,205,255,175)};
            REAL positions[3]{0.0f,0.5f,1.0f};
            LinearGradientBrush current(PointF(center-band,y),PointF(center+band,y),flow[0],flow[2]);
            current.SetInterpolationColors(flow,positions,3);
            g.FillRectangle(&current,center-band,y,band*2,height);
        }
        if(pulse>0) {
            SolidBrush breath(Color(static_cast<BYTE>(energy?4+10*pulse:64*pulse),glow.GetR(),glow.GetG(),glow.GetB()));
            g.FillRectangle(&breath,x+2,y+2,filled-4,height-4);
        }
        g.Restore(saved);
        Pen sheen(Color(static_cast<BYTE>(115+35*pulse),glow.GetR(),glow.GetG(),glow.GetB()),0.8f);
        g.DrawLine(&sheen,x+3,y+3,x+std::max(3.0f,filled-3),y+3);
        Pen end(Color(160,glow.GetR(),glow.GetG(),glow.GetB()),0.7f);
        g.DrawLine(&end,x+filled-2,y+3,x+filled-2,y+height-3);
    }
    g.DrawLine(&shade,x,y+height-1,x+width,y+height-1);
}
inline void soul_glyph(Gdiplus::Graphics& g,float cx,float cy,float radius,float pulse) {
    using namespace Gdiplus;
    Pen aura(Color(static_cast<BYTE>(16+12*pulse),230,192,123),3.0f);
    Pen thread(Color(235,227,204,158),1.35f);
    for(int arm=0;arm<3;++arm) {
        PointF points[20]{};
        for(int i=0;i<20;++i) {
            const float t=i/19.0f;
            const float angle=arm*2.094395f+t*4.3f;
            const float r=radius*(0.15f+0.85f*t);
            points[i]={cx+r*std::cos(angle),cy+r*std::sin(angle)};
        }
        g.DrawLines(&aura,points,20);g.DrawLines(&thread,points,20);
    }
}
inline void housing(Gdiplus::Graphics& g,bool covenant,Skin skin,const Skins* skins,uint64_t elapsed_ms=0,int ember_strength=60) {
    using namespace Gdiplus;
    if(skin==Skin::Human && skins && skins->ready()) {
        skins->embers(g,FramePart::Health,RectF(60,9,360,26),elapsed_ms,ember_strength);
        skins->embers(g,FramePart::Stamina,RectF(60,39,274,20),elapsed_ms,ember_strength);
        if(covenant)skins->embers(g,FramePart::Covenant,RectF(0,0,64,64),elapsed_ms,ember_strength);
    }
    if(skins && skins->ready()) {
        skins->draw(g,skin,FramePart::Health,RectF(60,9,360,26));
        skins->draw(g,skin,FramePart::Stamina,RectF(60,39,274,20));
        if(covenant)skins->draw(g,skin,FramePart::Covenant,RectF(0,0,64,64),255,elapsed_ms,ember_strength,0);
        return;
    }
    // Minimal owned fixture fallback. Live initialization requires both atlases.
    Pen rim(skin==Skin::Hollow?Color(255,112,111,104):Color(255,118,87,58),2);
    g.DrawRectangle(&rim,60,9,360,26);g.DrawRectangle(&rim,60,39,274,20);
    if(covenant)g.DrawEllipse(&rim,0,0,64,64);
}
inline bool draw_icon(Gdiplus::Graphics& g,Gdiplus::Bitmap* image,const DS2FX_HudIcon& item,
    float x,float y,float size,uint32_t opacity=255,const Assets* assets=nullptr) {
    using namespace Gdiplus;
    if(!image || item.opacity>255 || opacity>255)return false;
    ColorMatrix matrix{};matrix.m[0][0]=(item.tint&255)/255.0f;
    matrix.m[1][1]=((item.tint>>8)&255)/255.0f;matrix.m[2][2]=((item.tint>>16)&255)/255.0f;
    matrix.m[3][3]=((item.tint>>24)&255)/255.0f*item.opacity/255.0f*opacity/255.0f;matrix.m[4][4]=1;
    ImageAttributes attributes;attributes.SetColorMatrix(&matrix);attributes.SetWrapMode(WrapModeClamp);
    const auto rect=assets?assets->content(item.type,item.id):Rect(0,0,64,64);
    const float factor=size/std::max(rect.Width,rect.Height);
    const float w=rect.Width*factor,h=rect.Height*factor;
    const auto saved=g.Save();g.SetInterpolationMode(InterpolationModeBilinear);
    const auto status=g.DrawImage(image,RectF(x+(size-w)/2,y+(size-h)/2,w,h),static_cast<REAL>(rect.X),static_cast<REAL>(rect.Y),
        static_cast<REAL>(rect.Width),static_cast<REAL>(rect.Height),UnitPixel,&attributes);
    g.Restore(saved);return status==Ok;
}
inline uint32_t available_icon_regions(const DS2FX_HudIcons& icons,const Assets& assets) {
    uint32_t mask=0;
    if((icons.available_regions&DS2FX_HUD_REGION_COVENANT) && icons.covenant.opacity<=255 &&
       icons.covenant.type==16 && (!icons.covenant.id || assets.get(icons.covenant.type,icons.covenant.id)))mask|=DS2FX_HUD_REGION_COVENANT;
    if((icons.available_regions&DS2FX_HUD_REGION_EFFECTS) && icons.effect_count<=20 && icons.effect_opacity<=255) {
        bool ready=true;
        for(unsigned i=0;i<icons.effect_count;++i)ready&=icons.effects[i].opacity<=255 && assets.get(icons.effects[i].type,icons.effects[i].id)!=nullptr;
        if(ready)mask|=DS2FX_HUD_REGION_EFFECTS;
    }
    return mask;
}
inline void draw_regions(Gdiplus::Graphics& g,int width,int height,
                 const DS2FX_PlayerState& player,const DS2FX_HudState& hud,
                 const Settings& settings,uint64_t elapsed_ms,bool gameplay,bool souls_region,
                 const DS2FX_HudIcons* icons=nullptr,const Assets* assets=nullptr,
                 Skin skin=Skin::Hollow,const Skins* skins=nullptr,const BarAnimation* animation=nullptr,uint32_t regions=7) {
    using namespace Gdiplus;
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const float scale=std::clamp(settings.scale/100.0f,0.5f,2.0f)*
        std::min(width/1600.0f,height/900.0f);
    const float edge=static_cast<float>(settings.margin)*scale;
    const float pulse=settings.pulse ? (0.5f+0.5f*std::sin(static_cast<float>(elapsed_ms%120000)*0.0021f))*settings.pulse_strength/100.f : 0.0f;
    const float phase=settings.pulse ? static_cast<float>(elapsed_ms%1800)/1800.0f : 0.5f;
    g.SetPageUnit(UnitPixel);
    if(gameplay) {
    const bool covenant=icons && assets && (available_icon_regions(*icons,*assets)&DS2FX_HUD_REGION_COVENANT);
    auto place=[&](int part){const auto r=part_box(part,width,height,settings.scale,settings.margin,settings.layout,20,player.max_health,player.max_stamina,settings.stat_lengths!=0,settings.native_hp,settings.native_sp);
        g.ResetTransform();g.TranslateTransform(r.x-settings.offset_x,r.y-settings.offset_y);g.ScaleTransform(r.h/part_h[part],r.h/part_h[part]);return r.w/(r.h/part_h[part]);};
    if(covenant && (regions&DS2FX_HUD_REGION_COVENANT)) {
        place(0);SolidBrush well(Color(250,8,10,12));g.FillEllipse(&well,9,9,46,46);
        if(icons->covenant.id){auto icon=icons->covenant;icon.opacity=255;
            const auto saved=g.Save();GraphicsPath clip;clip.AddEllipse(9.f,9.f,46.f,46.f);g.SetClip(&clip,CombineModeIntersect);
            draw_icon(g,assets->get(icon.type,icon.id),icon,9,9,46,255,assets);g.Restore(saved);}
        if(skins && skins->ready()){
            
            skins->draw(g,skin,FramePart::Covenant,RectF(0,0,64,64),255,elapsed_ms,settings.ember_strength,settings.shadow_strength);}
    }
    if(regions&DS2FX_HUD_REGION_BARS) {
    const float hp=animation?animation->health_fill:fraction(static_cast<float>(player.health),static_cast<float>(player.max_health));
    const float stamina=animation?animation->stamina_fill:player.stamina_available?fraction(player.stamina,player.max_stamina):0.f;
    const bool human=skin==Skin::Human;
    const float actual_hp=fraction(static_cast<float>(player.health),static_cast<float>(player.max_health));
    const float beat=settings.pulse ? (animation?animation->heartbeat.signal:
        Heartbeat::wave(std::fmod(elapsed_ms/1000.f*Heartbeat::rate(actual_hp)/60.f,1.f))*Heartbeat::strength(actual_hp))*settings.pulse_strength/100.f:0;
    const float hp_width=place(1);
    bar(g,4,3,hp_width-10,10,hp,human?Color(255,68,8,14):Color(255,52,15,21),
        human?Color(255,187,39,46):Color(255,149,46,52),Color(255,237,77,79),beat,false,phase,animation?animation->health.value:hp);
    if(skins && skins->ready()){
        
        skins->draw(g,skin,FramePart::Health,RectF(0,0,hp_width,16),255,elapsed_ms,settings.ember_strength,settings.shadow_strength);}
    const float sp_width=place(2);
    bar(g,4,3,sp_width-10,10,stamina,human?Color(255,29,58,23):Color(255,35,45,29),
        human?Color(255,113,145,59):Color(255,83,105,63),Color(255,143,166,93),pulse,true,phase,animation?animation->stamina.value:stamina,animation?animation->stamina_flash:0);
    if(skins && skins->ready()){
        
        skins->draw(g,skin,FramePart::Stamina,RectF(0,0,sp_width,16),255,elapsed_ms,settings.ember_strength,settings.shadow_strength);}
    g.ResetTransform();
    // Keep the original four-way input mapping: up spell, left offhand,
    // right main hand, down item. IDs are diagnostics until icon extraction.
    if(settings.show_quickslots) {
    g.TranslateTransform(edge+(slot_radius+slot_halo)*scale,quickmenu_anchor_y(height,edge,scale));
    g.ScaleTransform(scale,scale);
    slot(g,slot_step,-slot_step,L"SPELL",false,(hud.available_fields&DS2FX_HUD_SPELL)!=0,hud.spell_id,pulse,settings.diagnostic_ids!=0);
    slot(g,0,0,L"LEFT",false,(hud.available_fields&DS2FX_HUD_LEFT_HAND)!=0,hud.left_hand_item_id,pulse,settings.diagnostic_ids!=0);
    slot(g,slot_step*2,0,L"RIGHT",false,(hud.available_fields&DS2FX_HUD_RIGHT_HAND)!=0,hud.right_hand_item_id,pulse,settings.diagnostic_ids!=0);
    slot(g,slot_step,slot_step,L"ITEM",true,(hud.available_fields&DS2FX_HUD_QUICK_ITEM)!=0,hud.quick_item_id,pulse,settings.diagnostic_ids!=0);
    g.ResetTransform();
    }
    }
    }
    if(!souls_region || !settings.show_souls || !(regions&DS2FX_HUD_REGION_BARS)) return;
    const float soul_width=210.0f*scale,soul_height=45.0f*scale;
    const float sx=width-edge-soul_width,sy=height-edge-soul_height;
    SolidBrush plate(Color(185,12,11,12));
    Pen plate_rim(Color(208,127,109,82),1.5f*scale);
    g.FillRectangle(&plate,sx,sy,soul_width,soul_height);
    g.DrawRectangle(&plate_rim,sx,sy,soul_width,soul_height);
    g.DrawEllipse(&plate_rim,sx+7*scale,sy+7*scale,31*scale,31*scale);
    const auto glyph_state=g.Save();
    g.TranslateTransform(sx+22.5f*scale,sy+22.5f*scale);g.ScaleTransform(scale,scale);
    soul_glyph(g,0,0,11,pulse);g.Restore(glyph_state);
    wchar_t souls[32]{};
    if (hud.available_fields & DS2FX_HUD_SOULS) swprintf_s(souls,L"%u",hud.souls);
    else wcscpy_s(souls,L"--");
    text(g,souls,sx+50*scale,sy+6*scale,150*scale,33*scale,24*scale,Color(245,237,226,204),true);
}
inline void draw(Gdiplus::Graphics& g,int width,int height,
                 const DS2FX_PlayerState& player,const DS2FX_HudState& hud,
                 const Settings& settings,uint64_t elapsed_ms,BYTE gameplay_opacity=255,
                 const DS2FX_HudIcons* icons=nullptr,const Assets* assets=nullptr,
                 Skin skin=Skin::Hollow,const Skins* skins=nullptr,const BarAnimation* animation=nullptr,int render_width=0,int render_height=0,uint32_t regions=7) {
    using namespace Gdiplus;
    g.Clear(Color(0,0,0,0));
    if(gameplay_opacity==0 || gameplay_opacity==255) {
        draw_regions(g,width,height,player,hud,settings,elapsed_ms,gameplay_opacity!=0,true,icons,assets,skin,skins,animation,regions);
    } else {
    // Only animated intermediate fades need a temporary gameplay layer.
    Bitmap layer(render_width?render_width:width,render_height?render_height:height,PixelFormat32bppPARGB);
    Graphics layer_graphics(&layer);
    layer_graphics.Clear(Color(0,0,0,0));
    draw_regions(layer_graphics,width,height,player,hud,settings,elapsed_ms,true,false,icons,assets,skin,skins,animation,regions);
    layer_graphics.Flush(FlushIntentionSync);
    ColorMatrix matrix{};
    for(int i=0;i<5;++i) matrix.m[i][i]=1.0f;
    matrix.m[3][3]=gameplay_opacity/255.0f;
    ImageAttributes attributes;
    attributes.SetColorMatrix(&matrix);
    g.DrawImage(&layer,Rect(0,0,layer.GetWidth(),layer.GetHeight()),0,0,layer.GetWidth(),layer.GetHeight(),UnitPixel,&attributes);
    draw_regions(g,width,height,player,hud,settings,elapsed_ms,false,true,nullptr,nullptr,Skin::Hollow,nullptr,nullptr,regions);
    }
    if((regions&DS2FX_HUD_REGION_EFFECTS) && icons && assets && (available_icon_regions(*icons,*assets)&DS2FX_HUD_REGION_EFFECTS) && icons->effect_count && icons->effect_opacity) {
        const float scale=std::clamp(settings.scale/100.0f,0.5f,2.0f)*std::min(width/1600.0f,height/900.0f);
        const auto r=part_box(3,width,height,settings.scale,settings.margin,settings.layout,icons->effect_count);
        const float effect_scale=r.w/(42+46*(std::min(8u,icons->effect_count)-1));
        g.TranslateTransform(r.x-settings.offset_x,r.y-settings.offset_y);g.ScaleTransform(effect_scale,effect_scale);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        for(unsigned i=0;i<icons->effect_count;++i) {
            const float x=(i%8)*effect_step,y=(i/8)*effect_step;
            const auto alpha=static_cast<BYTE>(icons->effect_opacity*icons->effects[i].opacity/255);
            SolidBrush backing(Color(static_cast<BYTE>(alpha*0.90f),8,10,12));
            g.FillRectangle(&backing,x+4,y+4,effect_size-8,effect_size-8);
            const float inset=(effect_size-settings.icon_size)/2;
            const auto saved=g.Save();g.SetClip(RectF(x+inset,y+inset,static_cast<float>(settings.icon_size),static_cast<float>(settings.icon_size)),CombineModeIntersect);
            draw_icon(g,assets->get(icons->effects[i].type,icons->effects[i].id),icons->effects[i],
                      x+inset,y+inset,static_cast<float>(settings.icon_size),icons->effect_opacity,assets);
            g.Restore(saved);
            if(skins && skins->ready())skins->draw(g,skin,FramePart::Effect,RectF(x,y,effect_size,effect_size),alpha,elapsed_ms,0,settings.shadow_strength);
            else {Pen rim(Color(alpha,112,111,104),1);g.DrawRectangle(&rim,x,y,effect_size,effect_size);}
        }
        g.ResetTransform();
    }
}
}
