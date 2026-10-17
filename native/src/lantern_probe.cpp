#include "lantern_probe.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cmath>
#ifdef DS2FX_LANTERN_RESEARCH
#include "lantern_runtime.h"
#include "player_states.h"
#include "lantern_light_anchor.h"
#include "lantern_world_light.h"
#include "lantern_light_selector.h"
#include "lantern_pose_observer.h"
#include "lantern_pivot_motion.h"
#endif

extern "C" { void* DS2FX_LanternProbeOriginal = nullptr; }
namespace fx::lantern_probe {
namespace {
std::atomic<bool> installed{false}, observing{false}, fault{false};
std::atomic<DWORD> first_thread{0};
std::atomic<uint64_t> calls{0}, manager_calls{0}, samples{0}, skipped{0};
std::atomic_flag publishing = ATOMIC_FLAG_INIT;
Sample current{};
uintptr_t slot = 0;
std::atomic<uint64_t> last_sample_ms{0};

bool pointer(uintptr_t parent, uintptr_t offset, uintptr_t& out) noexcept {
    out = 0;
    return parent && offset <= UINTPTR_MAX-parent && fx::read(parent+offset,out) &&
           out >= 0x10000 && out <= 0x00007fffffffffffULL && !(out & 7);
}
std::string routine_hash(uintptr_t address) {
    unsigned char bytes[67]; SIZE_T count = 0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes,sizeof(bytes),&count) && count==sizeof(bytes),"Cannot read lantern probe entry");
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Probe SHA256 provider failed");
    struct Cleanup { BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE& h; ~Cleanup(){if(h) BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a,0);} } cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Probe SHA256 init failed");
    unsigned char digest[32];
    fx::require(BCryptHashData(hash,bytes,sizeof(bytes),0)==0 && BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Probe SHA256 failed");
    std::ostringstream result; for(auto b:digest) result<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);
    return result.str();
}
}

bool read_native_sample(uintptr_t base_slot, uintptr_t manager, uintptr_t update_address, Sample& out) noexcept {
    out = Sample{};
    out.sampled_at_ms = GetTickCount64();
    uintptr_t before=0, after=0, player=0, player_after=0;
    int32_t health=0, maximum=0;
    if(!base_slot || !manager || !fx::read(base_slot,before) || before!=manager ||
       !pointer(manager,0xd0,player) || !fx::read(player+0x168,health) ||
       !fx::read(player+0x170,maximum) || maximum<=0 || maximum>1000000 || health<=0 || health>maximum)
        return false;
    out.character_available = 1;
    float update=0;
    if(update_address && fx::read(update_address,update) && std::isfinite(update) && update>=0 && update<=1000) {
        out.update_value_available=1; out.update_value_raw=update;
    }
    uintptr_t data=0, inventory=0, bag=0, holder=0, value=0, profile=0;
    if(pointer(manager,0xa8,data)) {
        float torch=0;
        if(pointer(data,0x10,inventory) && pointer(inventory,0x10,bag) &&
           pointer(bag,0x28,holder) && pointer(holder,0x10,value) &&
           fx::read(value,torch) && std::isfinite(torch) && torch>=0 && torch<=100000000) {
            out.native_torch_available=1; out.native_torch_time_raw=torch;
        }
        int32_t index=-1;
        if(pointer(data,0xc0,profile) && fx::read(profile+0x18a8,index) && index>=0 && index<10) {
            out.save_slot_available=1; out.save_slot_index=index;
        }
    }
    // Recheck every traversed pointer before publishing. A pointer-ABA is still
    // possible; this diagnostic snapshot is never authority for a gameplay write.
    auto same=[](uintptr_t parent,uintptr_t offset,uintptr_t expected) noexcept {
        uintptr_t actual=0; return pointer(parent,offset,actual) && actual==expected;
    };
    if(!fx::read(base_slot,after) || after!=manager || !pointer(manager,0xd0,player_after) || player_after!=player) {
        out=Sample{}; return false;
    }
    if(out.native_torch_available && (!same(manager,0xa8,data) || !same(data,0x10,inventory) ||
        !same(inventory,0x10,bag) || !same(bag,0x28,holder) || !same(holder,0x10,value))) {
        out.native_torch_available=0; out.native_torch_time_raw=0;
    }
    if(out.save_slot_available && (!same(manager,0xa8,data) || !same(data,0xc0,profile))) {
        out.save_slot_available=0; out.save_slot_index=-1;
    }
    return true;
}

bool install(uintptr_t base_slot,bool explicit_opt_in) {
    if(!explicit_opt_in) return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;
    fx::require(false,"Lantern research probe is offline-only");
    return false;
#else
    fx::require(!installed.load(),"Lantern probe already installed");
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    uintptr_t address=0;
#ifdef DS2FX_FIXTURE
    address=reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_ProbeFixtureTarget"));
    fx::require(address!=0,"Owned lantern probe fixture missing");
#else
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash,"Unsupported executable for lantern probe");
    fx::require(base_slot==image+0x16148f0,"Unexpected BaseA slot for lantern probe");
    address=image+0x1bec2b7;
    fx::require(routine_hash(address)=="1167b1e9ea9984ebe6e179d29ec38c00a4a0f7b3d42bb5259743845c35c95b74","GameManUpdate bytes changed; lantern probe refused");
#endif
    (void)image;
    fx::require(base_slot!=0,"BaseA slot missing");
    slot=base_slot;
    const auto initialized=MH_Initialize();
    fx::require(initialized==MH_OK || initialized==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize hook backend");
    fx::require(MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&DS2FX_LanternProbeDetour),&DS2FX_LanternProbeOriginal)==MH_OK,"Cannot prepare lantern probe");
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(address));
    if(enabled!=MH_OK) { MH_RemoveHook(reinterpret_cast<void*>(address)); DS2FX_LanternProbeOriginal=nullptr; fx::require(false,"Cannot enable lantern probe"); }
    installed.store(true); observing.store(true);
    return true;
#endif
}
void disable() noexcept { observing.store(false); }
bool snapshot(Sample& output) noexcept {
    output=Sample{};
    if(!observing.load() || fault.load() || publishing.test_and_set(std::memory_order_acquire)) return false;
    output=current;
    publishing.clear(std::memory_order_release);
    if(!observing.load() || fault.load() || !output.sampled_at_ms || GetTickCount64()-output.sampled_at_ms>350) {
        output=Sample{}; return false;
    }
    return output.character_available!=0;
}
Status status() noexcept {
    return {calls.load(),manager_calls.load(),samples.load(),skipped.load(),first_thread.load(),installed.load(),observing.load(),fault.load()};
}
void observe(uintptr_t manager,uintptr_t update_address) noexcept {
    ++calls;
    if(fault.load()) return;
#ifdef DS2FX_LANTERN_RESEARCH
    // Stop observations immediately, but retain this exact native-thread gate
    // long enough for an owned world-light override to restore itself.
    if(!observing.load() && !fx::lantern_world_light::status().restore_pending) return;
#else
    if(!observing.load()) return;
#endif
    uintptr_t actual=0;
    if(!manager || !fx::read(slot,actual) || actual!=manager) return;
    ++manager_calls;
    DWORD expected=0; const DWORD tid=GetCurrentThreadId();
    first_thread.compare_exchange_strong(expected,tid);
    if(first_thread.load()!=tid) {fault.store(true); observing.store(false); return;}
#ifdef DS2FX_LANTERN_RESEARCH
    fx::lantern_world_light::authorize_engine_tick(manager);
    fx::lantern_light_selector::authorize_engine_tick(manager);
    if(!observing.load()) return;
    fx::lantern_pose_observer::authorize_engine_tick(manager);
    fx::lantern_pivot_motion::authorize_engine_tick(manager,update_address);
    fx::lantern_light_anchor::authorize_engine_tick(manager);
    fx::lantern_runtime::native_tick(manager,update_address);
    fx::player_states::native_tick(manager);
#endif
    const auto now=GetTickCount64();
    const auto previous=last_sample_ms.load();
    if(previous && now-previous<100) return;
    last_sample_ms.store(now);
    Sample next{};
    read_native_sample(slot,manager,update_address,next);
    if(publishing.test_and_set(std::memory_order_acquire)) {++skipped; return;}
    current=next;
    publishing.clear(std::memory_order_release);
    ++samples;
}
}

extern "C" void DS2FX_ObserveLanternTick(uintptr_t manager,uintptr_t update_address) noexcept {
    fx::lantern_probe::observe(manager,update_address);
}
