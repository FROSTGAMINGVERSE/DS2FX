#include "../src/lantern_world_light.h"
#include "../src/common.h"
#include "../src/lantern_item_toggle.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <xmmintrin.h>

extern "C" void DS2FX_WorldLightFixtureTarget();
struct alignas(16) Capture {uint64_t regs[8]{};unsigned char fx[512]{};};
extern "C" void DS2FX_WorldLightFixtureCapture(void*,uintptr_t,Capture*);
extern "C" void DS2FX_WorldLightFixtureThrow(){RaiseException(0xe0412345,0,0,nullptr);}
namespace {
alignas(16) std::array<unsigned char,0xc00> manager{};
alignas(16) std::array<unsigned char,0x500> player{},flags{};
alignas(16) std::array<unsigned char,0x2e0> control{};
alignas(16) std::array<unsigned char,0xa0> resource{};
uintptr_t slot=0;float scale=0.5f;bool present=true,provider_exception=false,change_owner=false,change_selector=false;
unsigned reads=0;
template<class A,class T> void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
template<class T,class A> T get(const A& a,size_t offset){T value{};std::memcpy(&value,a.data()+offset,sizeof(value));return value;}
void initialize(){
    control.fill(0);flags.fill(0);player.fill(0);manager.fill(0);resource.fill(0);
    slot=reinterpret_cast<uintptr_t>(manager.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));put(manager,0xbc8,uintptr_t{0x123400});
    put(player,0xb8,reinterpret_cast<uintptr_t>(flags.data()));put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});put(flags,0x368,int32_t{51});
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    put(control,0,image+0x10bfb78);put(control,8,reinterpret_cast<uintptr_t>(player.data()));
    put(control,0x1c,int32_t{51});put(control,0x1f0,int32_t{3});put(control,0x2b0,reinterpret_cast<uintptr_t>(control.data())+0xf0);
    put(control,0x2b8,reinterpret_cast<uintptr_t>(resource.data()));
    put(control,0x200,std::array<uint8_t,4>{204,102,51,0});put(control,0x204,std::array<uint8_t,4>{102,51,0,0});
    put(control,0x208,uint16_t{51});put(control,0x20a,uint16_t{102});
    put(control,0x20c,float{5});
    put(resource,0,image+0x11e24d0);put(resource,0x10,std::array<float,4>{.8f,.4f,.2f,.2f});put(resource,0x20,std::array<float,4>{.4f,.2f,0,.4f});
    put(resource,0x30,float{1});put(resource,0x34,float{.75f});put(resource,0x38,float{.5f});
    put(resource,0x48,uint8_t{1});put(resource,0x49,uint8_t{1});
    put(resource,0x50,std::array<float,4>{2,3,4,5});
}
bool read_scale(float& value) noexcept {
    ++reads;
    if(provider_exception)RaiseException(0xe0412346,0,0,nullptr);
    if(change_owner)put(manager,0xd0,uintptr_t{0});
    if(change_selector)put(flags,0x368,int32_t{50});
    value=scale;_mm_setcsr(_mm_getcsr()|0x40);return present;
}
bool original_exception_passthrough(void* target,uintptr_t control_address) {
    Capture capture{};
    __try {DS2FX_WorldLightFixtureCapture(target,control_address,&capture);}
    __except(GetExceptionCode()==0xe0412345?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}
    return false;
}
void expect_native(const char* message){fx::require(get<float>(resource,0x38)==.5f && get<float>(resource,0x1c)==.2f && get<float>(resource,0x2c)==.4f && get<uint8_t>(resource,0x48)==1 && get<float>(resource,0x5c)==get<float>(control,0x20c),message);}
}
int main(){try{
    using namespace fx::lantern_world_light;initialize();
    const auto cp=reinterpret_cast<uintptr_t>(control.data()),target=reinterpret_cast<uintptr_t>(&DS2FX_WorldLightFixtureTarget);
    Capture before{},after{};
    DS2FX_WorldLightFixtureCapture(reinterpret_cast<void*>(target),cp,&before);
    fx::require(!install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),reinterpret_cast<void*>(target),&read_scale,false),"Opt-in required");
    fx::require(install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),reinterpret_cast<void*>(target),&read_scale,true),"Install real fixture hook");
    auto tick=[&]{authorize_engine_tick(slot);DS2FX_WorldLightFixtureCapture(reinterpret_cast<void*>(target),cp,&after);};
    DS2FX_WorldLightFixtureCapture(reinterpret_cast<void*>(target),cp,&after);expect_native("Unauthorized native output unchanged");
    present=false;tick();expect_native("NoLE stays100percent and native marker");present=true;
    put(resource,0x48,uint8_t{0});tick();
    fx::require(get<uint8_t>(resource,0x48)==0,"Adapter must not enable a native-disabled light");
    present=false;tick();fx::require(get<uint8_t>(resource,0x48)==0&&get<float>(resource,0x38)==.5f,"Restore preserves native disabled state");
    present=true;put(resource,0x48,uint8_t{1});
    const auto original_packet=resource;
    const auto native_calls_before=get<uint32_t>(control,0x2dc);
    tick();
    fx::require(std::memcmp(before.regs,after.regs,sizeof(before.regs))==0,"Original registers, flags and return unchanged");
    fx::require(std::memcmp(before.fx,after.fx,8)==0 && std::memcmp(before.fx+24,after.fx+24,8)==0 && std::memcmp(before.fx+160,after.fx+160,96)==0,"Original XMM/MXCSR/x87 controls preserved");
    fx::require(get<float>(control,0x2d0)==.016f && get<uint32_t>(control,0x2dc)==native_calls_before+1,"Original dt argument forwarded and called exactly once");
    fx::require(get<float>(resource,0x38)==2 && get<float>(resource,0x1c)==.1f && get<float>(resource,0x2c)==.2f,"Owned light ordinary marker and LE multiplier once");
    auto unchanged=resource;put(unchanged,0x38,.5f);put(unchanged,0x1c,.2f);put(unchanged,0x2c,.4f);
    fx::require(unchanged==original_packet,"Only marker and alpha brightness lanes changed; colors/radius/position/shadow unchanged");
    for(int i=0;i<120;++i)tick();
    fx::require(get<float>(resource,0x1c)==.1f && get<float>(resource,0x2c)==.2f,"Persistent native packet does not compound across frames");
    fx::require(status().last_x==2 && status().last_y==3 && status().last_z==4 && status().last_multiplier==.5f && status().last_native_brightness==.2f,"Diagnostic source brightness and actual world position");
    scale=0;tick();fx::require(get<float>(resource,0x1c)==0 && get<float>(resource,0x38)==2,"Zero slider suppresses actual point light");
    scale=1;tick();fx::require(get<float>(resource,0x1c)==.2f,"One multiplier restores full native intensity");
    scale=10;tick();fx::require(get<float>(resource,0x1c)==2,"Upper valid bound");
    scale=std::numeric_limits<float>::quiet_NaN();tick();expect_native("Invalid multiplier restores native packet");scale=.5f;
    tick();provider_exception=true;tick();expect_native("Reader SEH failure restores native output");fx::require(status().provider_faults==1,"Provider exception recorded");provider_exception=false;
    tick();put(flags,0x368,int32_t{50});tick();expect_native("CastLight selector restores original character marker");put(flags,0x368,int32_t{51});
    tick();put(flags,0x368,int32_t{101});tick();expect_native("Combined light selector restores original marker");put(flags,0x368,int32_t{51});
    tick();put(control,0x1f0,int32_t{4});tick();expect_native("Native torch priority restores original marker");put(control,0x1f0,int32_t{3});
    tick();put(player,0x168,int32_t{0});tick();expect_native("Death restores original marker");put(player,0x168,int32_t{100});
    const auto oldreads=reads;std::thread worker([&]{DS2FX_WorldLightFixtureCapture(reinterpret_cast<void*>(target),cp,&after);});worker.join();fx::require(reads==oldreads,"Foreign thread cannot sample/apply");
    put(control,0x2d4,uint32_t{1});fx::require(original_exception_passthrough(reinterpret_cast<void*>(target),cp),"Original exception propagates with valid detour unwind");put(control,0x2d4,uint32_t{0});
    change_owner=true;tick();expect_native("Ownership transition during provider call cannot write");change_owner=false;put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    tick();change_selector=true;tick();expect_native("Selector transition during provider call restores prior override");change_selector=false;put(flags,0x368,int32_t{51});
    fx::lantern_item_toggle::prepare(reinterpret_cast<uintptr_t>(&slot),true);
    alignas(16) std::array<unsigned char,0x100> action{},params{},owner{};
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    put(owner,0,image+0x10af2b8);put(owner,8,reinterpret_cast<uintptr_t>(player.data()));
    put(action,0,image+0x10ccbd8);put(action,0x48,reinterpret_cast<uintptr_t>(owner.data()));put(action,0x40,reinterpret_cast<uintptr_t>(params.data()));
    put(params,0,int32_t{19});put(params,4,float{0});put(params,8,int32_t{51});
    put(action,0x10,int32_t{60425001});put(action,0x14,int32_t{1000});put(action,0x18,int32_t{9});
    fx::lantern_item_toggle::on_engine_tick(slot);
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
    put(flags,0x368,int32_t{0});tick();
    fx::require(get<float>(resource,0x1c)==.1f&&get<float>(resource,0x38)==2,"Visible Lantern illuminates without Oil or native status19");
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);tick();
    fx::require(get<float>(resource,0x1c)==0&&get<float>(resource,0x2c)==0,"Stow immediately suppresses canonical character lighting");
    fx::require(get<uint8_t>(resource,0x48)==0&&!status().last_render_enabled,"Hidden light is excluded from native renderer submission");
    fx::require(get<uint8_t>(resource,0x49)==1&&get<float>(resource,0x34)==.75f,"Shadow settings are preserved while submission is disabled");
    for(unsigned i=0;i<5;++i)put(control,0x10+i*4,int32_t{-1});tick();
    fx::require(get<float>(resource,0x1c)==0,"Owned deselection residue cannot light a hidden model");
    put(control,0x10,int32_t{17});put(control,0x1c,int32_t{-1});put(control,0x1f0,int32_t{0});put(control,0x2b0,cp+0x30);tick();fx::require(get<float>(resource,0x1c)==0&&get<float>(resource,0x2c)==0,"Hidden Lantern disables default character ambient output too");
    // Real regression: selected slot1 row202460, independently deselected slot3.
    put(control,0x14,int32_t{202460});put(control,0x1f0,int32_t{1});put(control,0x2b0,cp+0x70);
    for(int i=0;i<120;++i)tick();
    fx::require(get<float>(resource,0x1c)==0&&get<float>(resource,0x2c)==0&&get<uint8_t>(resource,0x48)==0,"Live slot1 character glow must stay suppressed with Lantern slot3=-1");
    put(control,0x2b0,cp+0x30);tick();expect_native("Mismatched slot1 matrix cannot be suppressed");
    put(control,0x14,int32_t{-1});put(control,0x2b0,cp+0x70);tick();expect_native("Invalid slot1 row cannot be suppressed");
    put(control,0x18,int32_t{202500});put(control,0x1f0,int32_t{2});put(control,0x2b0,cp+0xb0);tick();expect_native("Unproved slot2 light remains native");
    put(control,0x14,int32_t{202460});put(control,0x1f0,int32_t{1});put(control,0x2b0,cp+0x70);tick();
    configure_presentation_for_fixture(10,40,65,125);
    put(control,0x20,int32_t{42});put(control,0x1c,int32_t{-1});put(control,0x1f0,int32_t{4});put(control,0x2b0,cp+0x130);
    put(control,0x20c,float{12});
    tick();expect_native("Torch restores submission when switching from suppressed character light");
    const auto torch_packet=resource;const auto torch_reads=reads;
    for(int i=0;i<120;++i)tick();fx::require(resource==torch_packet&&reads==torch_reads,"Hidden Lantern leaves torch packet and brightness unmodified");
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
    tick();fx::require(resource==torch_packet&&reads==torch_reads,"Visible Lantern also yields to native torch priority");
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
    put(control,0x1c,int32_t{17});put(control,0x1f0,int32_t{0});put(control,0x2b0,cp+0x30);tick();
    fx::require(get<uint8_t>(resource,0x48)==0,"Putting torch away resumes hidden-character suppression");
    configure_presentation_for_fixture(0,100);
    put(control,0x20c,float{5});
    put(control,0x2b0,cp+0xf0);
    put(control,0x1c,int32_t{51});put(control,0x1f0,int32_t{3});
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
    tick();fx::require(get<uint8_t>(resource,0x48)==1&&get<float>(resource,0x1c)==.1f,"Relighting restores native submission and brightness");
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);tick();
    fx::require(get<uint8_t>(resource,0x48)==0,"Repeated stow still excludes submission");
    float prior=flicker_gain(0,.10f),minimum=1,maximum=1;
    for(uint64_t ms=16;ms<60000;ms+=16){const float gain=flicker_gain(ms,.10f);fx::require(gain>=.90f&&gain<=1.10f&&std::abs(gain-prior)<.010f,"Flicker exceeds bounded smooth variation");prior=gain;minimum=std::min(minimum,gain);maximum=std::max(maximum,gain);}
    fx::require(maximum-minimum>.15f,"Flicker variation must be measurable over time");
    configure_presentation_for_fixture(10,40,65,125);
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);tick();
    for(int i=0;i<120;++i){tick();const float d=get<float>(resource,0x1c),s=get<float>(resource,0x2c);fx::require(d>=.0585f&&d<=.0715f&&std::abs(s/d-(.8f/.65f))<.00001f&&get<float>(resource,0x5c)==6.25f,"Broader dimmer presentation must not compound across frames");}
    fx::require(get<float>(resource,0x50)==2&&get<float>(resource,0x54)==3&&get<float>(resource,0x58)==4&&get<float>(resource,0x34)==.75f,"Presentation preserves wick XYZ and native shadow parameters");
    put(control,0x1f0,int32_t{4});put(control,0x2b0,cp+0x130);put(control,0x20c,float{14});tick();
    expect_native("Torch priority restores its fresh radius, never a cached Lantern radius");
    const auto bright_torch=resource;
    for(int i=0;i<120;++i)tick();fx::require(resource==bright_torch,"Active Lantern presentation must not alter Torch radius or brightness");
    put(control,0x1f0,int32_t{3});put(control,0x2b0,cp+0xf0);put(control,0x20c,float{5});tick();
    fx::lantern_item_toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);tick();fx::require(get<uint8_t>(resource,0x48)==0,"Flicker cannot relight stowed Lantern");
    fx::require(get<float>(resource,0x5c)==5,"Stow removes the Lantern radius override");
    disable();fx::require(status().restore_pending,"Disable defers writes to native engine thread");tick();expect_native("Disable restores resource on native tick");fx::require(!status().restore_pending && !status().fault,"Clean restoration completed");
    std::cout<<"PASS native world light: actual MinHook/MASM original-first, ABI/FX/flags/SEH, native point marker, noncompounding LE factor, unchanged colors/radius/position, selector/death/disable restoration and ownership/thread gates\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
