#include "lantern_model_visibility.h"
#include "lantern_item_toggle.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#include <cstring>

extern "C" {void* DS2FX_LanternModelVisibilityOriginal=nullptr;}
namespace fx::lantern_model_visibility {
namespace {
uintptr_t slot=0,image=0;
std::atomic<bool> installed{false},enabled{false};
std::atomic<uint64_t> calls{0},owned{0},local{0},hidden{0},unknown{0},transitions{0};
std::atomic<DWORD> first_draw_thread{0};
constexpr wchar_t material_names[3][16]{L"DS2FX_Lantern_0",L"DS2FX_Lantern_1",L"DS2FX_Lantern_4"};
bool pointer(uintptr_t parent,uintptr_t offset,uintptr_t& out) noexcept {
    return parent&&parent<=UINTPTR_MAX-offset&&fx::read(parent+offset,out)&&out>=0x10000&&out<=0x00007fffffffffffULL&&!(out&7);
}
bool is_class(uintptr_t object,uintptr_t rva) noexcept {uintptr_t v=0;return object&&fx::read(object,v)&&v==image+rva;}
template<size_t N>bool name_equals(uintptr_t name,const wchar_t(&expected)[N]) noexcept {
    std::array<wchar_t,N> text{};
    return name>=0x10000&&name<=0x00007fffffffffffULL-sizeof(text)&&!(name&1)&&fx::read(name,text)&&std::memcmp(text.data(),expected,sizeof(text))==0;
}
struct Identity {
    uintptr_t manager=0,canonical=0,canonical_control=0,owner=0,adapter=0,data=0,meshes=0,materials=0,skeleton=0,bones=0;
    std::array<uintptr_t,3> material{};
};
bool owner_identity(uintptr_t control,Identity& id) noexcept {
    uintptr_t reverse=0;
    // Constructor31A2A0 binds control+8 to Character;313178 binds Character+F0
    // back to the same controller. Neither a missing canonical player nor an
    // unmatched reverse pointer supplies authority to suppress another draw.
    return is_class(control,0x10dfa68)&&pointer(control,8,id.owner)&&pointer(id.owner,0xf0,reverse)&&reverse==control&&
        pointer(slot,0,id.manager)&&pointer(id.manager,0xd0,id.canonical)&&pointer(id.canonical,0xf0,id.canonical_control)&&
        is_class(id.canonical_control,0x10dfa68)&&pointer(id.canonical_control,8,reverse)&&reverse==id.canonical;
}
bool owned_model(uintptr_t control,Identity& id) noexcept {
    const auto model=control+0x10;uint32_t bone_count=0;uint16_t skeleton_count=0;int32_t mesh_count=0;uintptr_t end=0;
    if(!is_class(model,0x10db650)||!pointer(model,0x50,id.adapter)||!is_class(id.adapter,0x11d9d08)||!pointer(id.adapter,0x10,id.data)||
        !fx::read(id.data+0xa0,mesh_count)||mesh_count!=3||!pointer(id.data,0xa8,id.meshes)||
        !pointer(id.data,0x1f0,id.materials)||!fx::read(id.data+0x1f8,end)||end!=id.materials+3*sizeof(uintptr_t)||
        !fx::read(id.materials,id.material)||!pointer(model,0x78,id.skeleton)||!fx::read(model+0x80,bone_count)||bone_count!=74||
        !fx::read(id.skeleton+0x18,skeleton_count)||skeleton_count!=74||!pointer(id.skeleton,0x20,id.bones))return false;
    uintptr_t name=0;int16_t parent=-1;
    if(!fx::read(id.bones+73*16,parent)||parent!=3||!fx::read(id.bones+73*16+8,name)||!name_equals(name,L"DS2FX_LanternPivot")||
        !fx::read(id.bones+3*16+8,name)||!name_equals(name,L"Pelvis"))return false;
    // B32290 consumes mesh records at+A8, count+A0, stride58; first uint16 is
    // its material index. V13 has three generated lamp meshes: brass, iron and wick.
    for(unsigned i=0;i<3;++i){
        uint16_t material_index=0;uint64_t length=0,capacity=0;
        if(!fx::read(id.meshes+i*0x58,material_index)||material_index!=i||!is_class(id.material[i],0x10dda98)||
            !fx::read(id.material[i]+0x70,length)||length!=15||!fx::read(id.material[i]+0x78,capacity)||capacity<15||capacity>1024||
            !fx::read(id.material[i]+0x60,name)||!name_equals(name,material_names[i]))return false;
    }
    return true;
}
bool same_identity(const Identity& a,const Identity& b) noexcept {
    return a.manager==b.manager&&a.canonical==b.canonical&&a.canonical_control==b.canonical_control&&a.owner==b.owner&&
        a.adapter==b.adapter&&a.data==b.data&&a.meshes==b.meshes&&a.materials==b.materials&&a.skeleton==b.skeleton&&a.bones==b.bones&&a.material==b.material;
}
std::string code_hash(uintptr_t address,size_t size) {
    std::vector<unsigned char> bytes(size);SIZE_T copied=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),size,&copied)&&copied==size,"Cannot read base model draw body");
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Base draw SHA provider failed");
    struct Cleanup{BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{a,h};
    fx::require(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0,"Base draw SHA init failed");unsigned char digest[32];
    fx::require(BCryptHashData(h,bytes.data(),static_cast<ULONG>(size),0)==0&&BCryptFinishHash(h,digest,sizeof(digest),0)==0,"Base draw SHA failed");
    std::ostringstream out;for(auto x:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(x);return out.str();
}
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* target,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!installed.load()&&base_slot&&executable&&target,"Invalid/repeated base model visibility setup");slot=base_slot;image=executable;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize base draw hook backend");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_LanternModelVisibilityDetour),&DS2FX_LanternModelVisibilityOriginal)==MH_OK,"Cannot prepare base model draw hook");
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);DS2FX_LanternModelVisibilityOriginal=nullptr;fx::require(false,"Cannot enable base model draw hook");}
    installed.store(true);enabled.store(true);return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;fx::require(false,"Lantern base visibility is offline-only");return false;
#else
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==executable+0x16148f0,"Unsupported executable for lantern base visibility");
    fx::require(code_hash(executable+0x31a6b0,299)=="a6d2f4ecbf73fde67b1393e0d7ab4201eff7329d2dc70c5225d4edb43b1bec1d","Base model draw body changed");
    return install_impl(base_slot,executable,reinterpret_cast<void*>(executable+0x31a6b0),true);
#endif
}
bool should_hide(uintptr_t control) noexcept {
    ++calls;if(!enabled.load())return false;
    DWORD expected=0;first_draw_thread.compare_exchange_strong(expected,GetCurrentThreadId());
    Identity before{};
    if(!owner_identity(control,before)||!owned_model(control,before)){++unknown;return false;}
    ++owned;
    // New characters start hidden. Only the owned reusable Lantern event can
    // reveal the local player's base-model attachment.
    if(before.owner==before.canonical){++local;if(!fx::lantern_item_toggle::is_enabled()||fx::lantern_item_toggle::active_for(before.canonical))return false;++hidden;return true;}
    Identity after{};
    if(!enabled.load()||!owner_identity(control,after)||!owned_model(control,after)||!same_identity(before,after)||after.owner==after.canonical){++transitions;return false;}
    ++hidden;return true;
}
Status status() noexcept {return{calls.load(),owned.load(),local.load(),hidden.load(),unknown.load(),transitions.load(),first_draw_thread.load(),installed.load(),enabled.load()};}
void disable() noexcept {enabled.store(false);}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t executable,void* target,bool opt_in){return install_impl(base_slot,executable,target,opt_in);}
#endif
}
extern "C" bool DS2FX_ShouldHideLanternBaseModel(uintptr_t control) noexcept {return fx::lantern_model_visibility::should_hide(control);}
