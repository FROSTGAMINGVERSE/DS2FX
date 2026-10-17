#include "../src/lantern_model_visibility.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>
extern "C" void DS2FX_ModelVisibilityFixtureTarget();
struct alignas(16) Capture {uint64_t regs[8]{};unsigned char fx[512]{};};
extern "C" void DS2FX_ModelVisibilityFixtureCapture(void*,uintptr_t,Capture*);
extern "C" void DS2FX_ModelVisibilityFixtureThrow(){RaiseException(0xe0007391,0,0,nullptr);}
namespace {
alignas(16) std::array<unsigned char,0x200> manager{};
alignas(16) std::array<unsigned char,0x800> local_control{},foreign_control{};
alignas(16) std::array<unsigned char,0x500> local_owner{},foreign_owner{};
alignas(16) std::array<unsigned char,0x30> adapter{};
alignas(16) std::array<unsigned char,0x240> data{};
alignas(16) std::array<unsigned char,4*0x58> meshes{};
alignas(16) std::array<unsigned char,0x40> skeleton{};
alignas(16) std::array<unsigned char,74*16> bones{};
alignas(16) std::array<std::array<unsigned char,0x90>,3> materials{};
std::array<uintptr_t,3> material_ptrs{};
constexpr wchar_t names[3][16]{L"DS2FX_Lantern_0",L"DS2FX_Lantern_1",L"DS2FX_Lantern_4"};
uintptr_t slot=0,image=0;
template<class A,class T>void put(A& a,size_t off,T v){std::memcpy(a.data()+off,&v,sizeof(v));}
template<class T,class A>T get(const A& a,size_t off){T v{};std::memcpy(&v,a.data()+off,sizeof(v));return v;}
template<class A>uintptr_t address(A& a){return reinterpret_cast<uintptr_t>(a.data());}
void initialize(){
    image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));slot=address(manager);put(manager,0xd0,address(local_owner));
    put(local_owner,0xf0,address(local_control));put(foreign_owner,0xf0,address(foreign_control));
    for(auto* c:{&local_control,&foreign_control}){put(*c,0,image+0x10dfa68);put(*c,0x10,image+0x10db650);put(*c,0x60,address(adapter));put(*c,0x88,address(skeleton));put(*c,0x90,uint32_t{74});}
    put(local_control,8,address(local_owner));put(foreign_control,8,address(foreign_owner));
    put(adapter,0,image+0x11d9d08);put(adapter,0x10,address(data));put(data,0xa0,int32_t{3});put(data,0xa8,address(meshes));
    for(unsigned i=0;i<3;++i){material_ptrs[i]=address(materials[i]);put(materials[i],0,image+0x10dda98);put(materials[i],0x60,reinterpret_cast<uintptr_t>(names[i]));put(materials[i],0x70,uint64_t{15});put(materials[i],0x78,uint64_t{15});put(meshes,i*0x58,static_cast<uint16_t>(i));}
    put(data,0x1f0,address(material_ptrs));put(data,0x1f8,address(material_ptrs)+24);put(skeleton,0x18,uint16_t{74});put(skeleton,0x20,address(bones));
    put(bones,73*16,int16_t{3});put(bones,73*16+8,reinterpret_cast<uintptr_t>(L"DS2FX_LanternPivot"));put(bones,3*16+8,reinterpret_cast<uintptr_t>(L"Pelvis"));
}
bool same_fx(const unsigned char* a,const unsigned char* b){return !std::memcmp(a,b,8)&&!std::memcmp(a+24,b+24,8)&&!std::memcmp(a+160,b+160,256);}
bool original_exception(void* target,uintptr_t control,Capture* out) noexcept {__try{DS2FX_ModelVisibilityFixtureCapture(target,control,out);return false;}__except(GetExceptionCode()==0xe0007391?EXCEPTION_EXECUTE_HANDLER:EXCEPTION_CONTINUE_SEARCH){return true;}}
}
int main(){try{
    using namespace fx::lantern_model_visibility;initialize();const auto target=reinterpret_cast<void*>(&DS2FX_ModelVisibilityFixtureTarget);
    Capture baseline{},after{},entry_before{},entry_after{};
    DS2FX_ModelVisibilityFixtureCapture(target,address(local_control),&baseline);std::memcpy(&entry_before,local_control.data()+0x500,sizeof(Capture));
    fx::require(!install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),image,target,false),"Explicit opt-in required");
    fx::require(install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),image,target,true),"Native visibility hook installed");
    auto draw=[&](auto& c){DS2FX_ModelVisibilityFixtureCapture(target,address(c),&after);};
    auto passes=[&](const char* why){const auto old=get<uint32_t>(foreign_control,0x4e0);draw(foreign_control);fx::require(get<uint32_t>(foreign_control,0x4e0)==old+1,why);};
    draw(local_control);std::memcpy(&entry_after,local_control.data()+0x500,sizeof(Capture));
    fx::require(get<uint32_t>(local_control,0x4e0)==2&&status().local==1,"Local owned model calls original exactly once");
    fx::require(!std::memcmp(baseline.regs,after.regs,sizeof(baseline.regs))&&same_fx(baseline.fx,after.fx),"Original output registers/flags/FX preserved");
    fx::require(!std::memcmp(entry_before.regs,entry_after.regs,sizeof(entry_before.regs))&&same_fx(entry_before.fx,entry_after.fx),"Pre-check preserves all original input registers/flags/FX");
    const auto original_meshes=meshes;
    const auto original_bones=bones;
    const auto original_materials=materials;
    const auto original_data=data;
    draw(foreign_control);fx::require(get<uint32_t>(foreign_control,0x4e0)==0&&status().hidden==1,"Only proven owned noncanonical base lamp is skipped");
    fx::require(after.regs[0]==0x7122334455667788&&after.regs[2]==0x6122334455667788&&after.regs[3]==0x2122334455667788,"Skip preserves incoming volatile registers");
    put(materials[1],0x60,reinterpret_cast<uintptr_t>(L"VanillaMaterial"));passes("One nonowned material preserves whole native draw");put(materials[1],0x60,reinterpret_cast<uintptr_t>(names[1]));
    put(materials[2],0x70,uint64_t{14});passes("Wrong material string length passes through");put(materials[2],0x70,uint64_t{15});
    put(materials[2],0x78,uint64_t{7});passes("Unexpected short-string storage passes through");put(materials[2],0x78,uint64_t{15});
    put(materials[2],0,uintptr_t{0});passes("Unknown material class passes through");put(materials[2],0,image+0x10dda98);
    put(meshes,2*0x58,uint16_t{0});passes("Unexpected mesh material binding passes through");put(meshes,2*0x58,uint16_t{2});
    put(data,0xa0,int32_t{4});passes("Any additional potentially vanilla mesh passes through");put(data,0xa0,int32_t{3});
    put(data,0x1f8,address(material_ptrs)+16);passes("Partial material vector passes through");put(data,0x1f8,address(material_ptrs)+24);
    put(bones,73*16+8,reinterpret_cast<uintptr_t>(L"UnrelatedPivot"));passes("Unknown pivot identity passes through");put(bones,73*16+8,reinterpret_cast<uintptr_t>(L"DS2FX_LanternPivot"));
    put(foreign_control,0x90,uint32_t{73});passes("Unrecognized bone layout passes through");put(foreign_control,0x90,uint32_t{74});
    put(adapter,0,image+0x11d9d10);passes("Unknown model adapter passes through");put(adapter,0,image+0x11d9d08);
    put(foreign_control,0,image+0x10dfa70);passes("Derived or unknown controller passes through");put(foreign_control,0,image+0x10dfa68);
    put(foreign_owner,0xf0,address(local_control));passes("Unproven owner reverse relationship passes through");put(foreign_owner,0xf0,address(foreign_control));
    put(manager,0xd0,uintptr_t{0});passes("Unavailable canonical player passes through");put(manager,0xd0,address(local_owner));
    put(local_control,8,address(foreign_owner));passes("Unproven canonical controller owner passes through");put(local_control,8,address(local_owner));
    put(manager,0xd0,address(foreign_owner));passes("Canonical player transition immediately preserves new local model");put(manager,0xd0,address(local_owner));
    put(materials[0],0x60,uintptr_t{0x10000});passes("Unreadable owned name passes through");put(materials[0],0x60,reinterpret_cast<uintptr_t>(names[0]));
    const auto old_hidden=status().hidden;std::thread a([&]{Capture c{};for(int i=0;i<20;++i)DS2FX_ModelVisibilityFixtureCapture(target,address(foreign_control),&c);});std::thread b([&]{Capture c{};for(int i=0;i<20;++i)DS2FX_ModelVisibilityFixtureCapture(target,address(foreign_control),&c);});a.join();b.join();
    fx::require(status().hidden==old_hidden+40,"Independent draw threads safely share read-only identity checks");
    fx::require(meshes==original_meshes&&bones==original_bones&&materials==original_materials&&data==original_data,"No game model/material/hierarchy writes");
    put(local_control,0x4e4,uint32_t{1});fx::require(original_exception(target,address(local_control),&after),"Native draw exception passes through original tail jump");put(local_control,0x4e4,uint32_t{0});
    disable();passes("Disable restores all native draws immediately");
    std::cout<<"PASS native base visibility: exact owned3material/mesh identity, nonlocal only, all unknown/local pass-through, production MinHook/MASM input/output FX/register/flag preservation, exception path, concurrent draws, no game writes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
