#include "lantern_pose_observer.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <array>
#include <cmath>
#include <cstring>

extern "C" {void* DS2FX_LanternPoseObserverOriginal=nullptr;}
namespace fx::lantern_pose_observer {
namespace {
uintptr_t slot=0,image=0;
std::atomic<bool> installed{false},enabled{false},fault{false};
std::atomic<uint64_t> calls{0},canonical{0},samples{0},owned{0},foreign{0},invalid{0},dropped{0},authorized_at{0};
std::atomic<DWORD> engine_thread{0};
std::atomic<OwnedPoseHandler> owned_handler{nullptr};
SRWLOCK sample_lock=SRWLOCK_INIT;
Sample current{};uintptr_t current_player=0,current_pose=0;
struct alignas(16) QST {float translation[4],rotation[4],scale[4];};
static_assert(sizeof(QST)==48);
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out) noexcept {
    return base && offset<=UINTPTR_MAX-base && fx::read(base+offset,out) && out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
bool name_pointer(uintptr_t address,uintptr_t& out) noexcept {
    return fx::read(address,out)&&out>=0x10000&&out<=0x00007fffffffffffULL&&!(out&1);
}
bool owner(uintptr_t pose,uintptr_t& player) noexcept {
    uintptr_t manager=0,model=0;int32_t hp=0,max_hp=0;
    return pointer(slot,0,manager) && pointer(manager,0xd0,player) && pointer(player,0xf0,model) &&
        model<=UINTPTR_MAX-0x80 && pose==model+0x80 && !(pose&15) &&
        fx::read(player+0x168,hp) && fx::read(player+0x170,max_hp) && hp>0 && hp<=max_hp && max_hp<=1000000;
}
template<size_t N> bool name_equals(uintptr_t name,const wchar_t (&expected)[N]) noexcept {
    if(!name || name>0x00007fffffffffffULL-128 || (name&1))return false;
    std::array<wchar_t,N> value{};
    return fx::read(name,value)&&!std::memcmp(value.data(),expected,sizeof(expected));
}
bool qst_valid(const QST& q) noexcept {
    for(auto v:q.translation)if(!std::isfinite(v)||std::abs(v)>1000000)return false;
    float norm=0;for(auto v:q.rotation){if(!std::isfinite(v))return false;norm+=v*v;}
    if(norm<0.5f||norm>1.5f)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(q.scale[i])||q.scale[i]<.001f||q.scale[i]>100)return false;
    return std::isfinite(q.scale[3]);
}
bool read_pose(uintptr_t pose,Sample& sample) noexcept {
    uintptr_t skeleton=0,bones=0,local=0;uint32_t count=0;uint16_t skeleton_count=0;
    if(!pointer(pose,8,skeleton)||!pointer(skeleton,0x20,bones)||!pointer(pose,0x20,local)||
       !fx::read(pose+0x10,count)||!fx::read(skeleton+0x18,skeleton_count)||count<1||count>512||count!=skeleton_count)return false;
    sample.bone_count=count;
    for(uint32_t i=0;i<count;++i) {
        uintptr_t name=0;if(!name_pointer(bones+i*16+8,name))return false;
        if(!name_equals(name,L"DS2FX_LanternPivot"))continue;
        if(sample.pivot_index>=0)return false; // Exact unique owned identity.
        sample.pivot_index=static_cast<int32_t>(i);int16_t parent=-1;
        if(!fx::read(bones+i*16,parent))return false;
        sample.parent_index=parent;
    }
    if(sample.pivot_index<0)return true;
    if(sample.parent_index>=0 && static_cast<uint32_t>(sample.parent_index)<count) {
        uintptr_t name=0;sample.parent_matches=name_pointer(bones+sample.parent_index*16+8,name)&&name_equals(name,L"Pelvis");
    }
    QST q{};if(!fx::read(local+sample.pivot_index*sizeof(QST),q))return false;
    std::memcpy(sample.translation,q.translation,sizeof(q.translation));std::memcpy(sample.rotation,q.rotation,sizeof(q.rotation));std::memcpy(sample.scale,q.scale,sizeof(q.scale));
    sample.pose_valid=sample.parent_matches&&qst_valid(q);return true;
}
std::string hash_bytes(uintptr_t address,size_t length) {
    std::vector<unsigned char> bytes(length);SIZE_T copied=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),bytes.size(),&copied)&&copied==bytes.size(),"Cannot read pose mapper code");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Pose mapper SHA256 provider failed");
    struct Cleanup{BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Pose mapper SHA256 init failed");
    unsigned char digest[32];fx::require(BCryptHashData(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)==0&&BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Pose mapper SHA256 failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* target,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!installed.load()&&base_slot&&executable&&target,"Invalid/repeated pose observer setup");
    slot=base_slot;image=executable;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize pose observer hook backend");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_LanternPoseObserverDetour),&DS2FX_LanternPoseObserverOriginal)==MH_OK,"Cannot prepare pose observer hook");
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);DS2FX_LanternPoseObserverOriginal=nullptr;fx::require(false,"Cannot enable pose observer hook");}
    installed.store(true);enabled.store(true);return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;fx::require(false,"Pose observer is offline-only");return false;
#else
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==executable+0x16148f0,"Unsupported executable/BaseA for pose observer");
    fx::require(hash_bytes(executable+0x4e3fa0,828)=="60d1e3caa3b188e88960eeaee16ca0143dbcaf56a9cf1c5c5bf729306bdafd94","Morpheme pose mapper body mismatch");
    return install_impl(base_slot,executable,reinterpret_cast<void*>(executable+0x4e3fa0),true);
#endif
}
void authorize_engine_tick(uintptr_t manager) noexcept {
    if(!enabled.load())return;
    uintptr_t actual=0;if(!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();engine_thread.compare_exchange_strong(expected,tid);
    if(engine_thread.load()!=tid){fault.store(true);enabled.store(false);return;}
    authorized_at.store(GetTickCount64());
}
void after_original(uintptr_t pose) noexcept {
    ++calls;if(!enabled.load()||fault.load())return;
    uintptr_t player=0;if(!owner(pose,player))return;++canonical;
    Sample next{};next.sampled_at_ms=GetTickCount64();next.callback_thread=GetCurrentThreadId();
    const auto when=authorized_at.load();next.on_authorized_thread=when&&next.sampled_at_ms>=when&&next.sampled_at_ms-when<=250&&next.callback_thread==engine_thread.load();
    if(!next.on_authorized_thread)++foreign; // Observe scheduling, never write a pose.
    if(!read_pose(pose,next)){++invalid;return;}
    uintptr_t after=0;if(!owner(pose,after)||after!=player||!enabled.load())return;
    // Diagnostic readers may defer publication, but must never defer the
    // validated engine-thread handler or reset its frame-to-frame simulation.
    if(TryAcquireSRWLockExclusive(&sample_lock)) {
        current=next;current_player=player;current_pose=pose;ReleaseSRWLockExclusive(&sample_lock);
        ++samples;if(next.pivot_index>=0)++owned;
    } else ++dropped;
    if(next.on_authorized_thread&&next.pose_valid) {
        const auto handler=owned_handler.load();
        if(handler&&enabled.load()&&owner(pose,after)&&after==player)handler(player,pose,next);
    }
}
void set_owned_pose_handler(OwnedPoseHandler handler) noexcept {owned_handler.store(handler);}
bool snapshot(Sample& out) noexcept {
    out={};if(!enabled.load()||fault.load())return false;
    uintptr_t player=0,pose=0;
    AcquireSRWLockShared(&sample_lock);out=current;player=current_player;pose=current_pose;ReleaseSRWLockShared(&sample_lock);
    const auto now=GetTickCount64();uintptr_t actual=0;
    if(!out.sampled_at_ms||now<out.sampled_at_ms||now-out.sampled_at_ms>250||!owner(pose,actual)||actual!=player){out={};return false;}
    return true;
}
Status status() noexcept {return{calls.load(),canonical.load(),samples.load(),owned.load(),foreign.load(),invalid.load(),dropped.load(),engine_thread.load(),installed.load(),enabled.load(),fault.load()};}
void disable() noexcept {enabled.store(false);}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t executable,void* target,bool opt_in){return install_impl(base_slot,executable,target,opt_in);}
void lock_sample_owned_fixture(){AcquireSRWLockShared(&sample_lock);}
void unlock_sample_owned_fixture(){ReleaseSRWLockShared(&sample_lock);}
#endif
}
extern "C" void DS2FX_AfterLanternPoseObserver(uintptr_t pose) noexcept {fx::lantern_pose_observer::after_original(pose);}
