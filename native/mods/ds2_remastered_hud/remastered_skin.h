#pragma once
#include "remastered_assets.h"
#include "../../sdk/ds2fx_api.h"
#include <array>
#include <chrono>
#include <cstring>

namespace remastered {
enum class Skin : unsigned { Hollow,Human };
// A form observation is cosmetic: loss must not revoke healthy draw leases.
// Retain the last proved form briefly only for the same character generation.
class FormSelector {
    uint64_t epoch=0,last_valid=0;
    Skin current=Skin::Hollow;
public:
    Skin select(const DS2FX_PlayerState& player,const DS2FX_PlayerForm& form,uint64_t now,int override_mode=0) {
        if(epoch!=player.character_epoch){epoch=player.character_epoch;last_valid=0;current=Skin::Hollow;}
        if(form.available && form.sample==player.sample && form.character_epoch==epoch &&
           form.hollow_level<=32 && form.human==(form.hollow_level==0)) {
            current=form.human?Skin::Human:Skin::Hollow;last_valid=now;
        } else if(!last_valid || now<last_valid || now-last_valid>1500)current=Skin::Hollow;
        return override_mode==1?Skin::Hollow:override_mode==2?Skin::Human:current;
    }
};
enum class FramePart : unsigned { Covenant,Health,Stamina,Effect };
struct PhysicalSprite {
    std::unique_ptr<Gdiplus::Bitmap> image,shadow;
    std::array<std::unique_ptr<Gdiplus::Bitmap>,64> contour;int alpha=-1;
    inline static uint64_t builds=0;
    inline static double build_total_ms=0,build_max_ms=0;
    void decorate(bool flames){
        const auto started=std::chrono::steady_clock::now();
        struct Timing {std::chrono::steady_clock::time_point start;~Timing(){const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();++builds;build_total_ms+=ms;build_max_ms=std::max(build_max_ms,ms);}} timing{started};
        using namespace Gdiplus;const int w=image->GetWidth(),h=image->GetHeight(),ew=w+6,eh=h+6;
        BitmapData bits{};Rect bounds(0,0,w,h);if(image->LockBits(&bounds,ImageLockModeRead,PixelFormat32bppPARGB,&bits)!=Ok)return;
        std::vector<BYTE> solid(ew*eh),outside(ew*eh),distance(ew*eh,255);std::vector<int> queue;
        std::vector<unsigned> raw(ew*eh),horizontal(ew*eh);
        shadow=std::make_unique<Bitmap>(ew,eh,PixelFormat32bppPARGB);BitmapData shade{};Rect expanded(0,0,ew,eh);
        if(shadow->LockBits(&expanded,ImageLockModeWrite,PixelFormat32bppPARGB,&shade)!=Ok){image->UnlockBits(&bits);shadow.reset();return;}
        for(int y=0;y<h;++y)for(int x=0;x<w;++x){const auto a=static_cast<BYTE*>(bits.Scan0)[y*bits.Stride+x*4+3];solid[(y+3)*ew+x+3]=a>=24;
            raw[(y+3)*ew+x+3]=a;}
        image->UnlockBits(&bits);
        // Separable seven-tap Gaussian, cached at physical resolution. Transparent padding
        // permits a natural fade outside the silhouette without a hard offset copy.
        constexpr unsigned kernel[]{1,6,15,20,15,6,1};
        for(int y=0;y<eh;++y)for(int x=0;x<ew;++x){unsigned sum=0;for(int k=-3;k<=3;++k)if(x+k>=0 && x+k<ew)sum+=raw[y*ew+x+k]*kernel[k+3];horizontal[y*ew+x]=sum;}
        for(int y=0;y<eh;++y)for(int x=0;x<ew;++x){unsigned sum=0;for(int k=-3;k<=3;++k)if(y+k>=0 && y+k<eh)sum+=horizontal[(y+k)*ew+x]*kernel[k+3];
            auto* pixel=static_cast<BYTE*>(shade.Scan0)+y*shade.Stride+x*4;pixel[0]=pixel[1]=pixel[2]=0;pixel[3]=static_cast<BYTE>(sum/8192);}
        shadow->UnlockBits(&shade);
        if(!flames)return;
        queue.push_back(0);outside[0]=1;for(size_t n=0;n<queue.size();++n){const int i=queue[n],x=i%ew,y=i/ew;
            for(const auto d:std::array<POINT,4>{{{1,0},{-1,0},{0,1},{0,-1}}}){const int nx=x+d.x,ny=y+d.y;if(nx<0||ny<0||nx>=ew||ny>=eh)continue;const int j=ny*ew+nx;if(!outside[j]&&!solid[j]){outside[j]=1;queue.push_back(j);}}}
        for(int y=0;y<eh;++y)for(int x=0;x<ew;++x){const int i=y*ew+x;if(!outside[i])continue;
            for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx){const int nx=x+dx,ny=y+dy,d=dx*dx+dy*dy;if(d>9||nx<0||ny<0||nx>=ew||ny>=eh)continue;if(solid[ny*ew+nx])distance[i]=std::min(distance[i],static_cast<BYTE>(d));}}
        // Precompute the six spatially blended harmonic coefficients once.
        // Only exterior edge pixels can contain fire; the rest are cleared in bulk.
        struct Edge {int x,y;float d;std::array<float,6> coefficients;};
        std::vector<Edge> edges;edges.reserve(ew*6);
        auto random=[](unsigned seed){seed^=seed>>16;seed*=0x7feb352dU;seed^=seed>>15;seed*=0x846ca68bU;seed^=seed>>16;return (seed&65535)/65535.f;};
        const int columns=ew/11+2,rows=eh/9+2;
        std::vector<std::array<float,6>> cells(columns*rows);
        for(int cy=0;cy<rows;++cy)for(int cx=0;cx<columns;++cx){
            const unsigned seed=static_cast<unsigned>(cx)*73856093U^static_cast<unsigned>(cy)*19349663U^static_cast<unsigned>(w+h)*83492791U;
            auto& c=cells[cy*columns+cx];
            for(int k=0;k<3;++k){const float phase=random(seed+k)*6.283185f,amplitude=k==0?.28f:k==1?.14f:.08f;c[k*2]=amplitude*std::cos(phase);c[k*2+1]=amplitude*std::sin(phase);}
        }
        for(int y=0;y<eh;++y)for(int x=0;x<ew;++x)if(distance[y*ew+x]<=9){
            const float sx=x/11.f,sy=y/9.f;const int cx=static_cast<int>(sx),cy=static_cast<int>(sy);
            float tx=sx-cx,ty=sy-cy;tx=tx*tx*(3-2*tx);ty=ty*ty*(3-2*ty);
            Edge edge{x,y,std::sqrt(static_cast<float>(distance[y*ew+x])),{}};
            for(int k=0;k<6;++k)edge.coefficients[k]=(1-ty)*((1-tx)*cells[cy*columns+cx][k]+tx*cells[cy*columns+cx+1][k])+ty*((1-tx)*cells[(cy+1)*columns+cx][k]+tx*cells[(cy+1)*columns+cx+1][k]);
            edges.push_back(edge);
        }
        for(int f=0;f<64;++f){
            contour[f]=std::make_unique<Bitmap>(ew,eh,PixelFormat32bppPARGB);Rect rect(0,0,ew,eh);BitmapData data{};
            if(contour[f]->LockBits(&rect,ImageLockModeWrite,PixelFormat32bppPARGB,&data)!=Ok){contour[f].reset();continue;}
            for(int y=0;y<eh;++y)std::memset(static_cast<BYTE*>(data.Scan0)+y*data.Stride,0,ew*4);
            const float t=f*6.283185f/64;std::array<float,6> phase{};
            for(int k=0;k<3;++k){phase[k*2]=std::sin((2*k+1)*t);phase[k*2+1]=std::cos((2*k+1)*t);}
            for(const auto& edge:edges){
                float wave=.5f;for(int k=0;k<6;++k)wave+=edge.coefficients[k]*phase[k];wave=std::clamp(wave,0.f,1.f);
                const float reach=.65f+2.35f*wave*wave;
                const BYTE a=static_cast<BYTE>(std::max(0.f,1-edge.d/(reach+.5f))*(55+135*wave));
                auto* pixel=static_cast<BYTE*>(data.Scan0)+edge.y*data.Stride+edge.x*4;
                pixel[0]=29*a/255;pixel[1]=105*a/255;pixel[2]=239*a/255;pixel[3]=a;
            }
            contour[f]->UnlockBits(&data);
        }
    }
    bool draw(Gdiplus::Graphics& g,Gdiplus::Bitmap& source,Gdiplus::RectF dest,BYTE opacity,Gdiplus::Rect source_rect={},bool rail=false,uint64_t elapsed=0,int ember=0,int shade=0) {
        using namespace Gdiplus;
        Matrix transform;REAL e[6]{};g.GetTransform(&transform);transform.GetElements(e);
        if(e[0]<=0 || e[3]<=0 || e[1]!=0 || e[2]!=0)return false;
        const int w=std::max(1,static_cast<int>(std::lround(dest.Width*e[0]))),h=std::max(1,static_cast<int>(std::lround(dest.Height*e[3])));
        if(!image || image->GetWidth()!=w || image->GetHeight()!=h || alpha!=opacity) {
            image=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);alpha=opacity;
            Graphics target(image.get());target.Clear(Color(0,0,0,0));target.SetInterpolationMode(InterpolationModeHighQualityBicubic);
            ColorMatrix matrix{};for(int i=0;i<5;++i)matrix.m[i][i]=1;matrix.m[3][3]=opacity/255.f;
            ImageAttributes attributes;attributes.SetColorMatrix(&matrix);attributes.SetWrapMode(WrapModeTileFlipXY);
            const Rect src=source_rect.Width>0?source_rect:Rect(0,0,source.GetWidth(),source.GetHeight());
            target.SetPixelOffsetMode(PixelOffsetModeHalf);
            auto patch=[&](Rect dst,Rect from){target.DrawImage(&source,dst,from.X,from.Y,from.Width,from.Height,UnitPixel,opacity==255?nullptr:&attributes);};
            if(rail){
                const int left=std::max(1,static_cast<int>(std::lround(h*.45f))),right=std::max(1,static_cast<int>(std::lround(h*.85f)));
                const int sl=std::max(1,static_cast<int>(std::lround(src.Height*.45f))),sr=std::max(1,static_cast<int>(std::lround(src.Height*.85f)));
                patch(Rect(0,0,left,h),Rect(src.X,src.Y,sl,src.Height));
                patch(Rect(left,0,std::max(1,w-left-right),h),Rect(src.X+sl,src.Y,src.Width-sl-sr,src.Height));
                patch(Rect(w-right,0,right,h),Rect(src.X+src.Width-sr,src.Y,sr,src.Height));
            }else patch(Rect(0,0,w,h),src);
            target.Flush(FlushIntentionSync);shadow.reset();for(auto& frame:contour)frame.reset();
        }
        if((ember>0 || shade>0) && (!shadow || (ember>0 && !contour[0])))decorate(ember>0);
        const auto state=g.Save();g.ResetTransform();g.SetInterpolationMode(InterpolationModeNearestNeighbor);
        const int x=static_cast<int>(std::lround(dest.X*e[0]+e[4])),y=static_cast<int>(std::lround(dest.Y*e[3]+e[5]));
        auto layer=[&](Bitmap* bitmap,int px,int py,int strength){if(!bitmap || !strength)return;ColorMatrix matrix{};for(int i=0;i<5;++i)matrix.m[i][i]=1;matrix.m[3][3]=strength/100.f;ImageAttributes a;a.SetColorMatrix(&matrix);
            g.DrawImage(bitmap,Rect(px,py,bitmap->GetWidth(),bitmap->GetHeight()),0,0,bitmap->GetWidth(),bitmap->GetHeight(),UnitPixel,&a);};
        layer(shadow.get(),x-3+std::max(1,h/32),y-3+std::max(1,h/32),shade);
        const auto frame=(elapsed/100)%64;const float blend=(elapsed%100)/100.f;
        layer(contour[frame].get(),x-3,y-3,static_cast<int>(ember*(1-blend)));
        layer(contour[(frame+1)%64].get(),x-3,y-3,static_cast<int>(ember*blend));
        const auto status=g.DrawImage(image.get(),x,y);
        g.Restore(state);return status==Ok;
    }
};
class Skins {
    std::array<std::unique_ptr<Gdiplus::Bitmap>,2> atlases;
    std::array<std::array<std::unique_ptr<Gdiplus::Bitmap>,4>,2> sprites;
    std::array<std::array<std::unique_ptr<Gdiplus::Bitmap>,16>,3> ember_frames;
    mutable std::array<std::array<PhysicalSprite,4>,2> physical_sprites;
    mutable std::array<std::array<PhysicalSprite,16>,3> physical_embers;
    // Sprite rectangles are measured in the unmodified generated RGBA atlases.
    // Both render to identical destination geometry; no layout jump on change.
    const std::array<std::array<Gdiplus::Rect,4>,2> regions{{
        {{{77,142,364,369},{75,591,1131,130},{75,591,1131,130},{75,993,199,202}}},
        {{{62,100,372,371},{64,546,1142,133},{64,546,1142,133},{64,944,220,218}}}
    }};
    void prepare_embers() {
        using namespace Gdiplus;
        const SIZE sizes[]{{64,64},{360,26},{274,20}};
        for(unsigned part=0;part<3;++part) {
            const int w=sizes[part].cx+6,h=sizes[part].cy+6;
            Bitmap sprite(w,h,PixelFormat32bppPARGB);
            {Graphics g(&sprite);g.Clear(Color(0,0,0,0));draw(g,Skin::Human,static_cast<FramePart>(part),RectF(3,3,static_cast<float>(sizes[part].cx),static_cast<float>(sizes[part].cy)));g.Flush(FlushIntentionSync);}
            std::vector<bool> solid(w*h),outside(w*h);
            for(int y=0;y<h;++y)for(int x=0;x<w;++x){Color c;sprite.GetPixel(x,y,&c);solid[y*w+x]=c.GetA()>=24;}
            // Flood only exterior transparency: covenant holes and bar interiors
            // never receive flames. Geometry comes from the real rendered atlas.
            std::vector<int> queue{0};outside[0]=true;
            for(size_t q=0;q<queue.size();++q) {
                const int at=queue[q],x=at%w,y=at/w;
                for(const auto d:{POINT{-1,0},POINT{1,0},POINT{0,-1},POINT{0,1}}) {
                    const int nx=x+d.x,ny=y+d.y;if(nx<0||ny<0||nx>=w||ny>=h)continue;
                    const int next=ny*w+nx;if(!solid[next]&&!outside[next]){outside[next]=true;queue.push_back(next);}
                }
            }
            for(unsigned frame=0;frame<16;++frame) {
                auto image=std::make_unique<Bitmap>(w,h,PixelFormat32bppPARGB);
                for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
                    if(!outside[y*w+x])continue;
                    int nearest=10;
                    for(int dy=-3;dy<=3;++dy)for(int dx=-3;dx<=3;++dx) {
                        if(x+dx<0||y+dy<0||x+dx>=w||y+dy>=h)continue;
                        if(solid[(y+dy)*w+x+dx])nearest=std::min(nearest,dx*dx+dy*dy);
                    }
                    if(nearest>9)continue;
                    const float wave=0.5f+0.5f*std::sin(frame*6.283185f/16+x*0.37f+y*0.51f+part);
                    const float reach=1.1f+1.8f*wave*wave;
                    const float distance=std::sqrt(static_cast<float>(nearest));
                    const float alpha=std::max(0.f,1-distance/(reach+0.5f))*(90+100*wave);
                    image->SetPixel(x,y,Color(static_cast<BYTE>(alpha),239,105,29));
                }
                ember_frames[part][frame]=std::move(image);
            }
        }
    }
public:
    void clear() noexcept {for(auto& atlas:atlases)atlas.reset();for(auto& skin:sprites)for(auto& sprite:skin)sprite.reset();for(auto& part:ember_frames)for(auto& frame:part)frame.reset();for(auto& skin:physical_sprites)for(auto& sprite:skin){sprite.image.reset();sprite.shadow.reset();for(auto& frame:sprite.contour)frame.reset();}for(auto& part:physical_embers)for(auto& frame:part){frame.image.reset();frame.shadow.reset();for(auto& c:frame.contour)c.reset();}}
    void load(const std::filesystem::path& folder) {
        clear();
        const char* names[]{"hollow.png","human.png"};
        const char* hashes[]{"891d114b6c926f75775ac948795c01f260bcb2ec5957e8e18364f1faa10f23c3",
                             "41fa577658ef1d29910869b6d86dec8a72d3860867d9b56cce92b1cac37192a5"};
        for(unsigned i=0;i<2;++i) {
            const auto path=folder/names[i];fx::no_links(path);
            fx::require(std::filesystem::is_regular_file(path)&&std::filesystem::file_size(path)<8388608&&
                        fx::hash_file(path)==hashes[i],"Missing or changed grimdark skin atlas");
            auto image=std::make_unique<Gdiplus::Bitmap>(path.c_str(),FALSE);
            fx::require(image->GetLastStatus()==Gdiplus::Ok&&image->GetWidth()==1254&&image->GetHeight()==1254,
                        "Invalid grimdark atlas dimensions");
            Gdiplus::Color outside{},hole{};
            image->GetPixel(1200,1200,&outside);image->GetPixel(i?247:257,i?286:328,&hole);
            fx::require(outside.GetA()<8&&hole.GetA()<8,"Grimdark atlas must have transparent backdrop and sockets");
            atlases[i]=std::move(image);
        }
        const SIZE sizes[]{{64,64},{360,26},{274,20},{42,42}};
        for(unsigned skin=0;skin<2;++skin)for(unsigned part=0;part<4;++part) {
            auto image=std::make_unique<Gdiplus::Bitmap>(sizes[part].cx,sizes[part].cy,PixelFormat32bppPARGB);
            {Gdiplus::Graphics g(image.get());g.Clear(Gdiplus::Color(0,0,0,0));
            draw(g,static_cast<Skin>(skin),static_cast<FramePart>(part),Gdiplus::RectF(0,0,static_cast<float>(sizes[part].cx),static_cast<float>(sizes[part].cy)));
            g.Flush(Gdiplus::FlushIntentionSync);}
            sprites[skin][part]=std::move(image);
        }
    }
    bool ready() const {return atlases[0]&&atlases[1];}
    void embers(Gdiplus::Graphics& g,FramePart part,Gdiplus::RectF dest,uint64_t time,int strength) const {
        using namespace Gdiplus;
        const unsigned i=static_cast<unsigned>(part);if(i>=3 || strength<=0)return;
        const auto& image=ember_frames[i][(time/50)%16];if(!image)return;
        if(physical_embers[i][(time/50)%16].draw(g,*image,RectF(dest.X-3,dest.Y-3,dest.Width+6,dest.Height+6),static_cast<BYTE>(std::clamp(strength,0,100)*255/100)))return;
        ColorMatrix matrix{};for(int j=0;j<5;++j)matrix.m[j][j]=1;matrix.m[3][3]=std::clamp(strength,0,100)/100.f;
        ImageAttributes attributes;attributes.SetColorMatrix(&matrix);attributes.SetWrapMode(WrapModeClamp);
        g.DrawImage(image.get(),RectF(dest.X-3,dest.Y-3,dest.Width+6,dest.Height+6),0,0,
            static_cast<REAL>(image->GetWidth()),static_cast<REAL>(image->GetHeight()),UnitPixel,&attributes);
    }
    bool draw(Gdiplus::Graphics& g,Skin skin,FramePart part,Gdiplus::RectF dest,BYTE opacity=255,uint64_t elapsed=0,int ember=0,int shade=0) const {
        using namespace Gdiplus;
        const unsigned index=static_cast<unsigned>(skin);if(!atlases[index])return false;
        const auto& cached=sprites[index][static_cast<unsigned>(part)];
        if(cached && physical_sprites[index][static_cast<unsigned>(part)].draw(g,*atlases[index],dest,opacity,regions[index][static_cast<unsigned>(part)],part==FramePart::Health || part==FramePart::Stamina,elapsed,skin==Skin::Human?ember:0,shade))return true;
        auto* source=cached?cached.get():atlases[index].get();
        const auto rect=cached?Rect(0,0,cached->GetWidth(),cached->GetHeight()):regions[index][static_cast<unsigned>(part)];
        ColorMatrix matrix{};for(int i=0;i<5;++i)matrix.m[i][i]=1;matrix.m[3][3]=opacity/255.0f;
        ImageAttributes attributes;attributes.SetColorMatrix(&matrix);attributes.SetWrapMode(WrapModeTileFlipXY);
        const auto state=g.Save();g.SetInterpolationMode(cached?InterpolationModeBilinear:InterpolationModeHighQualityBicubic);
        const auto result=g.DrawImage(source,dest,static_cast<REAL>(rect.X),static_cast<REAL>(rect.Y),
            static_cast<REAL>(rect.Width),static_cast<REAL>(rect.Height),UnitPixel,opacity==255?nullptr:&attributes);
        g.Restore(state);return result==Ok;
    }
};
inline constexpr float bar_x=70,health_y=15,health_w=332,health_h=14,
    stamina_y=44,stamina_w=244,stamina_h=10,effect_size=42,effect_step=46;
}
