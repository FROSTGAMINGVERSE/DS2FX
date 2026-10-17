#include "../src/lantern_light_anchor.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
#include <xmmintrin.h>

extern "C" void DS2FX_LightAnchorFixtureTarget();
struct alignas(16) Capture {uint64_t regs[8]{};unsigned char fx[512]{};};
extern "C" void DS2FX_LightAnchorFixtureCapture(void*,uintptr_t,Capture*);
namespace {
using fx::lantern_light_anchor::Matrix;
alignas(16) std::array<unsigned char,0x200> manager{};
alignas(16) std::array<unsigned char,0x500> player{},flags{};
alignas(16) std::array<unsigned char,0x2e0> control{},foreign{};
uintptr_t slot=0;bool resolver_success=true,resolver_nan=false,resolver_change_owner=false;unsigned resolutions=0;
template<class A,class T> void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
template<class T,class A> T get(const A& a,size_t offset){T value{};std::memcpy(&value,a.data()+offset,sizeof(value));return value;}
Matrix expected_matrix(){Matrix m{};for(int i=0;i<4;++i)m.rows[i][i]=1;m.rows[3][0]=2;m.rows[3][1]=3;m.rows[3][2]=4;return m;}
void initialize(){
    control.fill(0);flags.fill(0);player.fill(0);manager.fill(0);
    slot=reinterpret_cast<uintptr_t>(manager.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(player,0xb8,reinterpret_cast<uintptr_t>(flags.data()));put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(flags,0x368,int32_t{51});
    put(control,0,reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+0x10bfb78);
    put(control,8,reinterpret_cast<uintptr_t>(player.data()));put(control,0x1c,int32_t{51});put(control,0x1f0,int32_t{3});
    put(control,0x2b0,reinterpret_cast<uintptr_t>(control.data())+0xf0);
}
bool __fastcall resolve(uintptr_t p,int32_t id,Matrix* matrix,bool world){
    ++resolutions;
    fx::require(p==reinterpret_cast<uintptr_t>(player.data()) && id==247 && world,"Exact native dummy arguments");
    fx::require(get<int32_t>(control,0x2dc)>0 && get<float>(control,0xf0)==3.0f,"Original transform runs first");
    *matrix=expected_matrix();if(resolver_nan)matrix->rows[0][0]=std::numeric_limits<float>::quiet_NaN();
    if(resolver_change_owner)put(manager,0xd0,uintptr_t{0});
    _mm_setcsr(_mm_getcsr()|0x40); // Detour must restore original post-call FX state.
    return resolver_success;
}
}
int main(){try{
    using namespace fx::lantern_light_anchor;initialize();
    const auto cp=reinterpret_cast<uintptr_t>(control.data());
    const auto target=reinterpret_cast<void*>(&DS2FX_LightAnchorFixtureTarget);
    Capture before{},after{};
    DS2FX_LightAnchorFixtureCapture(target,cp,&before);
    fx::require(!install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),target,&resolve,false),"Opt-in required");
    fx::require(install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),target,&resolve,true),"Fixture hook installed");
    DS2FX_LightAnchorFixtureCapture(target,cp,&after);fx::require(resolutions==0,"No authorization cannot resolve or write");
    authorize_engine_tick(slot);DS2FX_LightAnchorFixtureCapture(target,cp,&after);
    fx::require(std::memcmp(before.regs,after.regs,sizeof(before.regs))==0,"Original registers, returns and flags preserved");
    // FXSAVE registers0..5, MXCSR and x87 controls; reserved bytes/pointers are not state.
    fx::require(std::memcmp(before.fx,after.fx,8)==0 && std::memcmp(before.fx+24,after.fx+24,8)==0 && std::memcmp(before.fx+160,after.fx+160,96)==0,"Original XMM, MXCSR and x87 controls preserved");
    auto expected=expected_matrix();fx::require(get<Matrix>(control,0xf0).rows[3][1]==3 && std::memcmp(control.data()+0xf0,&expected,sizeof(expected))==0 && status().relocated==1,"Only eligible slot3 receives dummy247 transform");
    fx::require(get<uint32_t>(control,0x2dc)==3,"Original called once per invocation");
    auto tick=[&]{authorize_engine_tick(slot);DS2FX_LightAnchorFixtureCapture(target,cp,&after);};
    resolver_success=false;tick();fx::require(get<float>(control,0xf0)==3 && status().missing_dummy==1,"Missing owned dummy preserves native transform");resolver_success=true;
    resolver_nan=true;tick();fx::require(get<float>(control,0xf0)==3 && status().invalid_matrix==1,"Malformed matrix preserves native transform");resolver_nan=false;
    const auto old=resolutions;put(flags,0x368,int32_t{50});tick();put(flags,0x368,int32_t{101});tick();
    fx::require(resolutions==old,"Cast Light and combined status cannot be redirected");put(flags,0x368,int32_t{51});
    put(control,0x1f0,int32_t{4});tick();fx::require(resolutions==old,"Torch priority slot4 is preserved");put(control,0x1f0,int32_t{3});
    put(control,0x2b0,cp+0x130);tick();fx::require(resolutions==old,"Unexpected selected transform is preserved");put(control,0x2b0,cp+0xf0);
    put(player,0x168,int32_t{0});tick();fx::require(resolutions==old,"Dead character is excluded");put(player,0x168,int32_t{100});
    std::thread worker([&]{DS2FX_LightAnchorFixtureCapture(target,cp,&after);});worker.join();fx::require(resolutions==old,"Different thread cannot resolve or write");
    resolver_change_owner=true;tick();fx::require(get<float>(control,0xf0)==3 && status().relocated==1,"Ownership recheck prevents transition write");resolver_change_owner=false;
    initialize();disable();tick();fx::require(status().relocated==1,"Disabled adapter leaves native matrix");
    std::cout<<"PASS native light anchor: real MinHook/MASM, original first/exactly once, return/register/flags/FX preservation, owned dummy247, slot3/status51 only, owner/thread/matrix/missing/disable gates\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
