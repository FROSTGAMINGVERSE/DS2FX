#include "lantern_world_light.h"
#include "lantern_item_toggle.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <array>
#include <cmath>

extern "C" { void* DS2FX_LanternWorldLightOriginal=nullptr; }
namespace fx::lantern_world_light {
namespace {
uintptr_t slot=0,image=0;
ReadIntensity read_intensity=nullptr;
float flicker_amount=0,specular_factor=1,diffuse_factor=1,radius_factor=1;
std::atomic<bool> installed{false},enabled{false},fault{false},pending{false};
std::atomic<DWORD> thread{0};
std::atomic<uint64_t> authorized_at{0},calls{0},eligible{0},applied{0},restored{0},rejected{0};
std::atomic<uint64_t> unavailable{0},invalid{0},provider_faults{0},lost_owner{0};
std::atomic<float> last_multiplier{1},last_native{0},last_output{0},last_x{0},last_y{0},last_z{0};
std::atomic<bool> last_render_enabled{true};
std::atomic<float> last_native_radius{0},last_output_radius{0},last_flicker_gain{1};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
// Accessed only under busy on the verified engine thread. Never dereferenced
// until matched against freshly validated canonical controller/resource pointers.
uintptr_t changed_control=0,changed_player=0,changed_resource=0;
uint8_t changed_native_enable=1;
struct Packet { float diffuse[4],specular[4],unknown30,shadow34,marker38; };
static_assert(sizeof(Packet)==44);
struct NativeColor {uint8_t rgb[3],padding;};
struct Context {
    uintptr_t player=0,resource=0;
    Packet packet{};
    std::array<float,4> position{};
    float diffuse=0,specular=0,radius=0;
    uint8_t render_enabled=0;
    bool active=false,suppressible=false;
};
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out) noexcept {
    return base && offset<=UINTPTR_MAX-base && fx::read(base+offset,out) &&
        out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
bool context(uintptr_t control,Context& out) noexcept {
    uintptr_t manager=0,scene=0,owner=0,table=0,flags=0,selected=0;
    int32_t hp=0,max_hp=0,value=0,id=-1,index=-1;uint16_t diffuse=0,specular=0;
    NativeColor rgb{},spec{};
    if(!control || (control&15) || !pointer(slot,0,manager) || !pointer(manager,0xbc8,scene) ||
       !pointer(manager,0xd0,out.player) || !pointer(control,8,owner) || owner!=out.player ||
       !fx::read(control,table) || table!=image+0x10bfb78 ||
       !pointer(control,0x2b8,out.resource) || !fx::read(out.resource,table) || table!=image+0x11e24d0 ||
       !fx::read(control+0x200,rgb) || !fx::read(control+0x204,spec) ||
       !fx::read(control+0x208,diffuse) || !fx::read(control+0x20a,specular) ||
       !fx::read(control+0x20c,out.radius) ||
       !fx::read(out.resource+0x10,out.packet) || !fx::read(out.resource+0x50,out.position) ||
       !fx::read(out.resource+0x48,out.render_enabled) || out.render_enabled>1)return false;
    for(unsigned i=0;i<3;++i) {
        // Prove that the packet still belongs to this native controller's output.
        // Only the alpha brightness lanes may contain our previous override.
        if(!std::isfinite(out.packet.diffuse[i]) || !std::isfinite(out.packet.specular[i]) ||
           std::abs(out.packet.diffuse[i]-float(rgb.rgb[i])/255.0f)>0.00001f ||
           std::abs(out.packet.specular[i]-float(spec.rgb[i])/255.0f)>0.00001f ||
           !std::isfinite(out.position[i]) || std::abs(out.position[i])>10000000.0f)return false;
    }
    if(!std::isfinite(out.position[3]) || out.position[3]<0 || out.position[3]>1000000 ||
       !std::isfinite(out.radius) || out.radius<0 || out.radius>1000000)return false;
    // Exact native142DF8..142EC1 uses these current interpolated u16/255 values.
    // Reconstructing the source avoids multiplying last frame's modified packet.
    out.diffuse=float(diffuse)/255.0f;out.specular=float(specular)/255.0f;
    const bool player_ok=fx::read(out.player+0x168,hp) && fx::read(out.player+0x170,max_hp) &&
        hp>0 && hp<=max_hp && max_hp<=1000000 && pointer(out.player,0xb8,flags);
    const bool toggle_mode=fx::lantern_item_toggle::is_enabled();
    const bool toggle_active=toggle_mode?fx::lantern_item_toggle::active_for(out.player):
        (player_ok&&fx::read(flags+0x368,value)&&value==51);
    out.active=player_ok&&toggle_active&&fx::read(control+0x1c,id) && id==51 &&
        fx::read(control+0x1f0,index) && index==3 && fx::read(control+0x2b0,selected) && selected==control+0xf0;
    // Torch slot4 and other native light sources retain their native output.
    // Baseline slots0/1 are selected independently from Lantern slot3. Live
    // stowed output selected row202460 in slot1 (cool RGB110/130/140), matrix70.
    // Slot2 is unproved and remains native, as does Torch slot4.
    int32_t default_id=-1;
    out.suppressible=player_ok&&fx::read(control+0x1c,id)&&fx::read(control+0x1f0,index)&&
        (((index==0||index==1)&&fx::read(control+0x10+index*4,default_id)&&default_id>=0&&
          fx::read(control+0x2b0,selected)&&selected==control+0x30+index*0x40)||
         (index==3&&(id==51||id==-1)&&fx::read(control+0x2b0,selected)&&selected==control+0xf0));
    return true;
}
bool write_packet(uintptr_t resource,const Packet& packet,uint8_t render_enable,float radius) noexcept {
    // Native B7E550 skips submission when GX light byte48 is zero. Disable before
    // changing brightness; re-enable only after the complete packet is restored.
    SIZE_T copied=0;
    auto enable=[&]{copied=0;return WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(resource+0x48),&render_enable,1,&copied)&&copied==1;};
    if(!render_enable&&!enable())return false;
    copied=0;
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(resource+0x10),&packet,sizeof(packet),&copied)||copied!=sizeof(packet))return false;
    // Hash-gated ChrLightCtrl142ED4..142EED copies controller20c to GX50.W.
    // Change only that radius lane; XYZ remains the native animated wick anchor.
    copied=0;
    if(!WriteProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(resource+0x5c),&radius,sizeof(radius),&copied)||copied!=sizeof(radius))return false;
    if(render_enable&&!enable())return false;
    last_render_enabled.store(render_enable!=0);return true;
}
void clear_owned() noexcept {changed_control=changed_player=changed_resource=0;changed_native_enable=1;pending.store(false);}
bool same_owned(uintptr_t control,const Context& c) noexcept {
    return pending.load() && changed_control==control && changed_player==c.player && changed_resource==c.resource;
}
void acquire(uintptr_t control,const Context& c) noexcept {
    if(!same_owned(control,c))changed_native_enable=c.render_enabled;
    changed_control=control;changed_player=c.player;changed_resource=c.resource;pending.store(true);
}
bool restore(uintptr_t control,Context& c) noexcept {
    if(!same_owned(control,c))return true;
    if(c.packet.marker38!=2.0f) {++lost_owner;clear_owned();return true;}
    c.packet.marker38=0.5f;c.packet.diffuse[3]=c.diffuse;c.packet.specular[3]=c.specular;
    if(!write_packet(c.resource,c.packet,changed_native_enable,c.radius)){fault.store(true);enabled.store(false);return false;}
    last_multiplier.store(1);last_native.store(c.diffuse);last_output.store(c.diffuse);
    last_native_radius.store(c.radius);last_output_radius.store(c.radius);last_flicker_gain.store(1);
    ++restored;clear_owned();return true;
}
bool suppress(uintptr_t control,Context& c) noexcept {
    // The reusable Lantern owns the canonical character point-light output.
    // Stowing removes render submission, including its independent shadow pass.
    // Both brightness lanes are also zeroed for native ambient/fade residue.
    // context() already proves controller, player, renderer resource and RGB;
    // no NPC, world light, LE global or status flag is modified.
    if(c.packet.marker38!=0.5f && !(same_owned(control,c)&&c.packet.marker38==2.0f))return true;
    c.packet.diffuse[3]=0;c.packet.specular[3]=0;c.packet.marker38=2.0f;
    acquire(control,c);
    if(!write_packet(c.resource,c.packet,0,c.radius)){fault.store(true);enabled.store(false);return false;}
    last_multiplier.store(0);last_native.store(c.diffuse);last_output.store(0);
    last_native_radius.store(c.radius);last_output_radius.store(c.radius);last_flicker_gain.store(1);++applied;return true;
}
bool invoke_provider(float& value,bool& exception) noexcept {
    // Keep SEH outside any C++ automatic object requiring unwinding.
    __try { return read_intensity && read_intensity(value); }
    __except(EXCEPTION_EXECUTE_HANDLER) {exception=true;return false;}
}
std::string hash_bytes(uintptr_t address,size_t length) {
    std::vector<unsigned char> bytes(length);SIZE_T copied=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),bytes.size(),&copied) && copied==bytes.size(),"Cannot read native world-light code");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"World-light SHA256 provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"World-light SHA256 init failed");
    unsigned char digest[32];fx::require(BCryptHashData(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)==0 && BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"World-light SHA256 failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* target,ReadIntensity provider,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!installed.load() && base_slot && executable && target && provider,"Invalid/repeated lantern world-light setup");
    slot=base_slot;image=executable;read_intensity=provider;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize world-light hook backend");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_LanternWorldLightDetour),&DS2FX_LanternWorldLightOriginal)==MH_OK,"Cannot prepare lantern world-light hook");
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);DS2FX_LanternWorldLightOriginal=nullptr;fx::require(false,"Cannot enable lantern world-light hook");}
    installed.store(true);enabled.store(true);return true;
}
}
float flicker_gain(uint64_t milliseconds,float amount) noexcept {
    const double t=double(milliseconds)*0.001;
    return 1.0f+amount*float(0.55*std::sin(t*2.2)+0.30*std::sin(t*5.7)+0.15*std::sin(t*13.1));
}
void configure_presentation(unsigned flicker_percent,unsigned specular_percent,unsigned diffuse_percent,unsigned radius_percent) {
    fx::require(!installed.load()&&flicker_percent<=12&&specular_percent<=100&&diffuse_percent<=100&&
        radius_percent>=100&&radius_percent<=150,"Invalid or late Lantern presentation settings");
    flicker_amount=float(flicker_percent)/100;specular_factor=float(specular_percent)/100;
    diffuse_factor=float(diffuse_percent)/100;radius_factor=float(radius_percent)/100;
}
#ifdef DS2FX_FIXTURE
void configure_presentation_for_fixture(unsigned flicker_percent,unsigned specular_percent,unsigned diffuse_percent,unsigned radius_percent) {
    flicker_amount=float(flicker_percent)/100;specular_factor=float(specular_percent)/100;
    diffuse_factor=float(diffuse_percent)/100;radius_factor=float(radius_percent)/100;
}
#endif
bool install(uintptr_t base_slot,bool opt_in,ReadIntensity provider) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;(void)provider;fx::require(false,"Lantern world-light adapter is offline-only");return false;
#else
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==executable+0x16148f0,"Unsupported executable/BaseA for lantern world light");
    fx::require(hash_bytes(executable+0x1427f0,2481)=="e7964e5bfd72f7017aa65d234f0f94459b1b4356c27d47fdeb4bbab99d715eb7","ChrLightCtrl world-light update body mismatch");
    // Native constructor and renderer marker predicate are independently gated.
    fx::require(hash_bytes(executable+0xba55f0,92)=="2081133fa0e3db8676ce82f8addb6bedaffcd17c0646cfdc250d8a0a6539bfa3" &&
        hash_bytes(executable+0xba0435,72)=="165c61432f9ee208792f6f52a97c7b68247d9cce3f2cfa7f8ab1fa6bca63f6b6","Native ordinary point-light marker contract mismatch");
    fx::require(hash_bytes(executable+0xb7e550,97)=="3d5bec4e68a0d5463c859ee0292fe0007b78e50824dd4fca147ed0365be4078f","Native light submission enable contract mismatch");
    return install_impl(base_slot,executable,reinterpret_cast<void*>(executable+0x1427f0),provider,true);
#endif
}
void authorize_engine_tick(uintptr_t manager) noexcept {
    if(!installed.load() || (!enabled.load()&&!pending.load()))return;
    uintptr_t actual=0;if(!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();thread.compare_exchange_strong(expected,tid);
    if(thread.load()!=tid){fault.store(true);enabled.store(false);return;}
    authorized_at.store(GetTickCount64());
}
void disable() noexcept {enabled.store(false);}
Status status() noexcept {
    return {calls.load(),eligible.load(),applied.load(),restored.load(),rejected.load(),unavailable.load(),invalid.load(),provider_faults.load(),lost_owner.load(),thread.load(),installed.load(),enabled.load(),fault.load(),pending.load(),last_multiplier.load(),last_native.load(),last_output.load(),last_x.load(),last_y.load(),last_z.load(),last_render_enabled.load(),last_native_radius.load(),last_output_radius.load(),last_flicker_gain.load()};
}
void after_original(uintptr_t control) noexcept {
    ++calls;if(!enabled.load()&&!pending.load())return;
    const auto now=GetTickCount64(),when=authorized_at.load();
    if(!when||now<when||now-when>250||thread.load()!=GetCurrentThreadId()){++rejected;return;}
    if(busy.test_and_set(std::memory_order_acquire)){fault.store(true);enabled.store(false);return;}
    struct Unlock {~Unlock(){busy.clear(std::memory_order_release);}} unlock;
    Context c{};
    if(!context(control,c))return;
    if(pending.load()&&!same_owned(control,c)) {++lost_owner;clear_owned();}
    if(!enabled.load()||fault.load()){restore(control,c);return;}
    if(!c.active){if(fx::lantern_item_toggle::is_enabled()&&c.suppressible)suppress(control,c);else restore(control,c);return;}
    ++eligible;
    float multiplier=1;bool exception=false;
    if(!invoke_provider(multiplier,exception)) {
        if(exception)++provider_faults;else ++unavailable;
        restore(control,c);return;
    }
    if(!std::isfinite(multiplier)||multiplier<0||multiplier>10) {++invalid;restore(control,c);return;}
    Context after{};
    if(!context(control,after)||after.player!=c.player||after.resource!=c.resource)return;
    if(!after.active||!enabled.load()||fault.load()){restore(control,after);return;}
    // Only acquire the native0.5 character marker or our own previous2.0.
    // Never commandeer another adapter's marker or scale.
    if(after.packet.marker38!=0.5f && !(same_owned(control,after)&&after.packet.marker38==2.0f))return;
    const float flicker=flicker_gain(now,flicker_amount);
    multiplier*=flicker;
    after.packet.diffuse[3]=after.diffuse*multiplier*diffuse_factor;
    after.packet.specular[3]=after.specular*multiplier*specular_factor;
    after.packet.marker38=2.0f;
    acquire(control,after);
    if(!write_packet(after.resource,after.packet,changed_native_enable,after.radius*radius_factor)){fault.store(true);enabled.store(false);return;}
    last_multiplier.store(multiplier);last_native.store(after.diffuse);last_output.store(after.packet.diffuse[3]);
    last_native_radius.store(after.radius);last_output_radius.store(after.radius*radius_factor);last_flicker_gain.store(flicker);
    last_x.store(after.position[0]);last_y.store(after.position[1]);last_z.store(after.position[2]);++applied;
}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t executable,void* target,ReadIntensity provider,bool opt_in){return install_impl(base_slot,executable,target,provider,opt_in);}
void after_original_for_fixture(uintptr_t control) noexcept {after_original(control);}
#endif
}
extern "C" void DS2FX_AfterLanternWorldLight(uintptr_t controller) noexcept {fx::lantern_world_light::after_original(controller);}
