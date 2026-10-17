#include "../mods/ds2_remastered_hud/remastered_view.h"
#include <chrono>
#include <iostream>
#include <vector>
int wmain(int argc,wchar_t** argv){
 if(argc!=2)return 2;
 Gdiplus::GdiplusStartupInput input;ULONG_PTR token;Gdiplus::GdiplusStartup(&token,&input,nullptr);
 {remastered::Skins skins;skins.load(argv[1]);
 DS2FX_PlayerState player{};player.available=1;player.health=1771;player.max_health=1771;player.stamina_available=1;player.stamina=946;player.max_stamina=946;
 DS2FX_HudState hud{};remastered::Settings settings;settings.native_hp=.405148f;settings.native_sp=.239194f;
 for(int width:{3840,2560,3840}){int height=width*9/16;auto box=remastered::layout_bounds(width,height,settings.scale,settings.margin,settings.layout,0,1771,946,true,settings.native_hp,settings.native_sp);settings.offset_x=box.left;settings.offset_y=box.top;
 Gdiplus::Bitmap image(box.right-box.left,box.bottom-box.top,PixelFormat32bppPARGB);Gdiplus::Graphics g(&image);
 std::vector<double> times;
 for(int n=0;n<301;++n){auto start=std::chrono::steady_clock::now();remastered::draw(g,width,height,player,hud,settings,1000+n*16,255,nullptr,nullptr,remastered::Skin::Human,&skins,nullptr,image.GetWidth(),image.GetHeight());g.Flush(Gdiplus::FlushIntentionSync);times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());}
 double cold=times[0];times.erase(times.begin());double total=0;for(double t:times)total+=t;std::sort(times.begin(),times.end());std::cout<<"width="<<width<<" cold_ms="<<cold<<" avg_ms="<<total/times.size()<<" p95_ms="<<times[284]<<" p99_ms="<<times[296]<<" max_ms="<<times.back()<<"\n";
 }}Gdiplus::GdiplusShutdown(token);return 0;
}
