#include "../src/seamless_contract.h"
#include <iostream>
int main(int argc,char** argv) { try {
 if(argc==2) {
  fx::Module current{};current.id="ds2-remastered-hud";current.version="0.1.28";current.hash=fx::hash_file(fx::utf8_path(argv[1]));
  fx::bridge::modules({current});
  current.hash[0]=current.hash[0]=='0'?'1':'0';bool changed=false;
  try{fx::bridge::modules({current});}catch(...){changed=true;}
  fx::require(changed,"Changed in-game HUD accepted by Seamless");
 }
 for(const auto& official:fx::bridge::official_releases) {
  const auto accepted=fx::bridge::supported_release(official.launcher,official.dll);
  fx::require(accepted && std::string(accepted->version)==official.version,"Official exact pair rejected");
  auto changed_launcher=std::string(official.launcher),changed_dll=std::string(official.dll);
  changed_launcher[0]=changed_launcher[0]=='0'?'1':'0';changed_dll[0]=changed_dll[0]=='0'?'1':'0';
  fx::require(!fx::bridge::supported_release(changed_launcher,official.dll),"Changed official launcher accepted");
  fx::require(!fx::bridge::supported_release(official.launcher,changed_dll),"Changed official DLL accepted");
 }
 fx::require(!fx::bridge::supported_release("", ""),"Missing official identities accepted");
 fx::require(!fx::bridge::supported_release(std::string(64,'0'),std::string(64,'0')),"Unknown official pair accepted");
 fx::Module lantern{}; lantern.id="wayfarers-lantern";lantern.version="0.4.0";lantern.hash="f4d92dcd5400a33ea222b6c813bf6f6692d3c2838f20ee00f682bf2b15abc386";
 fx::Module remastered{};remastered.id="ds2-remastered-hud";remastered.version="0.1.2";remastered.hash="7d742f1c611e8f827fc61aa13d2a2e15f18fe97179cd526b21ac7105e7e6583e";
 fx::Module new_hud=remastered;new_hud.version="0.1.5";new_hud.hash="aeb17a84200d6299d8493c263acea8dfd88ae90edf96928f66316f6b2f1ad307";
 fx::Module candidate_hud=new_hud;candidate_hud.version="0.1.6";candidate_hud.hash="f0b26335b94abfc20359c66ddf9fddd9295909b9578138e58608d0f107efd19b";
 fx::bridge::modules({candidate_hud});
 fx::Module frame_hud=candidate_hud;frame_hud.version="0.1.7";frame_hud.hash="71c70ef867a8b023ed441ea684ca2d4be3bf8414d5c71aa246908870e38a0feb";
 fx::bridge::modules({frame_hud});
 fx::Module styled_hud=frame_hud;styled_hud.version="0.1.8";styled_hud.hash="a7c18ae921043c40026b2d9a28ba028d75701d14b174a62784827f19c9a9f599";
 fx::bridge::modules({styled_hud});
 fx::Module form_hud=styled_hud;form_hud.version="0.1.9";form_hud.hash="8f1bb8eee5998ddb105c5cd5c5b62fa4b4f7e9fdae5a8c2a1eb4e1641a16a397";
 fx::bridge::modules({form_hud});
 fx::Module ember_hud=form_hud;ember_hud.version="0.1.10";ember_hud.hash="d6c1eddb061107c5ad808d35d4145cdeaf9f66bcfee95ede4d16ab6dd04bcb65";
 fx::bridge::modules({ember_hud});
 fx::Module animation_hud=ember_hud;animation_hud.version="0.1.11";animation_hud.hash="d649da914387787ad397969d50bbe7aa74919c269cf3221a42eeefb16d8c15b2";
 fx::bridge::modules({animation_hud});
 fx::Module efficient_hud=animation_hud;efficient_hud.version="0.1.12";efficient_hud.hash="e00cf87211e1ac41bbe64b6f0408978f3fc9c5514b4f94f1e06d616235441327";
 fx::bridge::modules({efficient_hud});
 fx::Module paced_hud=efficient_hud;paced_hud.version="0.1.13";paced_hud.hash="2d78005883555b9cc19d3a6f9b5e3eb09b1e473a3124187b25b8052a46ae67ad";
 fx::bridge::modules({paced_hud});
  fx::Module waitable_hud=paced_hud;waitable_hud.version="0.1.14";waitable_hud.hash="a8f1340eacf9c99a4818a7dc5dd91f9b1404a3fc405779f7a858d92e9e5f138b";
  fx::bridge::modules({waitable_hud});
  fx::Module editor_hud=waitable_hud;editor_hud.version="0.1.15";editor_hud.hash="a40802482653075607064bcc61d5b49a32fb5484a3605e4850b6fba6cba6384d";
  fx::bridge::modules({editor_hud});
  fx::Module refined_hud=editor_hud;refined_hud.version="0.1.16";refined_hud.hash="da971324f57bdb973d1faa87761ec686c822f31e0a65df8464107e596272ead0";
  fx::bridge::modules({refined_hud});
  fx::Module gauge_hud=refined_hud;gauge_hud.version="0.1.17";gauge_hud.hash="beb64b935feb6106fbfbdbb655efa2029a486b6fe78a89280cba63ad45173df2";
  fx::bridge::modules({gauge_hud});gauge_hud.hash[0]='0';bool gauge_rejected=false;try{fx::bridge::modules({gauge_hud});}catch(...){gauge_rejected=true;}
  fx::require(gauge_rejected,"Changed gauge HUD accepted");
  fx::Module compact_hud=refined_hud;compact_hud.version="0.1.18";compact_hud.hash="3f05aad755b8436a7fec42c4b3e8d4c55f3074b9aed669ef03cea88e3e853977";
  fx::bridge::modules({compact_hud});compact_hud.hash[0]='0';bool compact_rejected=false;try{fx::bridge::modules({compact_hud});}catch(...){compact_rejected=true;}
  fx::require(compact_rejected,"Changed compact HUD accepted");
  fx::Module motion_hud=refined_hud;motion_hud.version="0.1.19";motion_hud.hash="b3fe55dfd0271e5bc3fa56b38050a066e58c321be58478b02e427390807b43e2";
  fx::bridge::modules({motion_hud});motion_hud.hash[0]='0';bool motion_rejected=false;try{fx::bridge::modules({motion_hud});}catch(...){motion_rejected=true;}
  fx::require(motion_rejected,"Changed motion HUD accepted");
  fx::Module preset_hud=refined_hud;preset_hud.version="0.1.20";preset_hud.hash="c61e814ddce7e68e8c2aa6c299de541c30b08061177d508d8a1eb9273e626bb9";fx::bridge::modules({preset_hud});preset_hud.hash[0]='1';bool preset_rejected=false;try{fx::bridge::modules({preset_hud});}catch(...){preset_rejected=true;}fx::require(preset_rejected,"Changed preset HUD accepted");
  refined_hud.hash[0]='0';bool refined_rejected=false;try{fx::bridge::modules({refined_hud});}catch(...){refined_rejected=true;}
  fx::require(refined_rejected,"Changed refined HUD accepted");
  editor_hud.hash[0]='0';bool editor_rejected=false;try{fx::bridge::modules({editor_hud});}catch(...){editor_rejected=true;}
  fx::require(editor_rejected,"Changed editor HUD accepted");
  waitable_hud.hash[0]='0';bool waitable_rejected=false;try{fx::bridge::modules({waitable_hud});}catch(...){waitable_rejected=true;}
  fx::require(waitable_rejected,"Changed waitable HUD accepted");
 paced_hud.hash[0]='0';bool paced_rejected=false;try{fx::bridge::modules({paced_hud});}catch(...){paced_rejected=true;}
 fx::require(paced_rejected,"Changed paced HUD accepted");
 efficient_hud.hash[0]='0';bool efficient_rejected=false;try{fx::bridge::modules({efficient_hud});}catch(...){efficient_rejected=true;}
 fx::require(efficient_rejected,"Changed efficient HUD accepted");
 animation_hud.hash[0]='0';bool animation_rejected=false;try{fx::bridge::modules({animation_hud});}catch(...){animation_rejected=true;}
 fx::require(animation_rejected,"Changed animated HUD accepted");
 ember_hud.hash[0]='0';bool ember_rejected=false;try{fx::bridge::modules({ember_hud});}catch(...){ember_rejected=true;}
 fx::require(ember_rejected,"Changed ember HUD accepted");
 form_hud.hash[0]='0';bool form_rejected=false;try{fx::bridge::modules({form_hud});}catch(...){form_rejected=true;}
 fx::require(form_rejected,"Changed Human/Hollow HUD accepted");
 styled_hud.hash[0]='0';bool styled_rejected=false;try {fx::bridge::modules({styled_hud});}catch(...){styled_rejected=true;}
 fx::require(styled_rejected,"Changed styled HUD accepted");
 frame_hud.hash[0]='0';bool frame_rejected=false;try {fx::bridge::modules({frame_hud});}catch(...){frame_rejected=true;}
 fx::require(frame_rejected,"Changed shared frame HUD accepted");
 fx::bridge::modules({new_hud});
 auto changed_hud=new_hud;changed_hud.hash=remastered.hash;
 bool changed_rejected=false;try {fx::bridge::modules({changed_hud});} catch(...) {changed_rejected=true;}
 if(!changed_rejected)throw std::runtime_error("HUD version/hash mismatch must reject");
 fx::Module offline{};offline.id="frost-training-stamina";
 fx::require(fx::bridge::select({offline,lantern}).size()==1,"Lantern selection/offline filtering failed");
 fx::require(fx::bridge::select({offline,remastered}).size()==1,"Read-only HUD selection/offline filtering failed");
 remastered.hash[0]='0';bool hud_rejected=false;try {fx::bridge::select({remastered});}catch(...){hud_rejected=true;}
 fx::require(hud_rejected,"Altered Remastered HUD DLL accepted");
 lantern.hash[0]='0';bool rejected=false;try {fx::bridge::select({lantern});}catch(...){rejected=true;}
 fx::require(rejected,"Altered Lantern DLL accepted");
 fx::require(fx::bridge::select({offline}).empty(),"Offline module accepted");
 std::cout<<"PASS official 0.0.1/0.0.3 exact identities accepted; unknown/tampered pairs rejected; pinned Lantern and local Remastered HUD selection preserved\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what();return 1;} }

