#include "lantern_effect.h"
#include "lantern_item_toggle.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <cmath>

extern "C" { void* DS2FX_LanternEffectOriginal = nullptr; }
namespace fx::lantern_effect {
namespace {
std::atomic<bool> installed{false}, observing{false}, fault{false};
std::atomic<DWORD> first_thread{0};
std::atomic<uint64_t> calls{0}, matched_calls{0}, samples{0}, skipped{0};
std::atomic<uint64_t> status_actions{0};
std::atomic<uint32_t> last_rejection{0};
std::atomic_flag publishing = ATOMIC_FLAG_INIT;
Sample current{};
uintptr_t slot = 0, image_base = 0;

bool pointer(uintptr_t parent, uintptr_t offset, uintptr_t& out, uintptr_t alignment=8) noexcept {
    out = 0;
    return parent && offset <= UINTPTR_MAX-parent && fx::read(parent+offset,out) &&
           out >= 0x10000 && out <= 0x00007fffffffffffULL && !(out & (alignment-1));
}
std::string routine_hash(uintptr_t address) {
    unsigned char bytes[35]; SIZE_T count = 0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes,sizeof(bytes),&count) && count==sizeof(bytes),"Cannot read lantern effect entry");
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Effect SHA256 provider failed");
    struct Cleanup { BCRYPT_ALG_HANDLE a; BCRYPT_HASH_HANDLE& h; ~Cleanup(){if(h) BCryptDestroyHash(h); BCryptCloseAlgorithmProvider(a,0);} } cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Effect SHA256 init failed");
    unsigned char digest[32];
    fx::require(BCryptHashData(hash,bytes,sizeof(bytes),0)==0 && BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Effect SHA256 failed");
    std::ostringstream result; for(auto b:digest) result<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);
    return result.str();
}
}

bool read_native_sample(uintptr_t base_slot, uintptr_t action, uintptr_t image, Sample& out, uint32_t* rejection) noexcept {
    out=Sample{};
    const auto stage=[&](uint32_t value) noexcept {if(rejection) *rejection=value;};
    stage(1);
    uintptr_t manager=0, player=0, flags=0, vtable=0, owner=0, owner_vtable=0, owner_player=0, override_flags=0, params=0;
    if(!action || !fx::read(action,vtable) || vtable!=image+0x10ccbd8) return false;
    stage(6); if(!fx::read(base_slot,manager) || !pointer(manager,0xd0,player)) return false;
    stage(7); if(!pointer(player,0xb8,flags)) return false;
    stage(8); if(!pointer(action,0x48,owner)) return false;
    stage(9); if(!fx::read(owner,owner_vtable) || owner_vtable!=image+0x10af2b8) return false;
    stage(10); if(!pointer(owner,8,owner_player) || owner_player!=player) return false;
    stage(11); if(!fx::read(owner+0x18,override_flags) || (override_flags && override_flags!=flags)) return false;
    stage(12); if(!pointer(action,0x40,params,4)) return false;
    stage(2);
    int32_t health=0,maximum=0;
    if(!fx::read(player+0x168,health) || !fx::read(player+0x170,maximum) ||
       maximum<=0 || maximum>1000000 || health<=0 || health>maximum) return false;
    struct Parameters { int32_t status; float duration; int32_t amount; } instruction{};
    struct NativeState { float remaining; float elapsed; float unknown28; uint32_t unknown2c; int8_t mode; } state{};
    int32_t light=0;
    stage(3);
    if(!fx::read(params,instruction) || instruction.status!=19 || instruction.amount!=51 || instruction.duration!=300.0f) return false;
    stage(4);
    if(!fx::read(action+0x20,state) || state.mode!=0 ||
       !std::isfinite(state.remaining) || state.remaining < -10 || state.remaining>300.0f ||
       !std::isfinite(state.elapsed) || state.elapsed<0 || state.elapsed>3600 ||
       !fx::read(flags+0x368,light)) return false;
    Sample next{};
    next.sampled_at_ms=GetTickCount64(); next.owner_token=player;
    next.remaining_seconds=(std::max)(0.0f,state.remaining); next.capacity_seconds=instruction.duration;
    next.available=1; next.active=state.remaining>0 ? 1u : 0u; next.light_value=static_cast<uint32_t>(light);
    stage(5);
    if(!fx::read(action+0x10,next.metadata_10) || !fx::read(action+0x14,next.metadata_14) ||
       !fx::read(action+0x18,next.metadata_18) || !fx::read(action+0x1c,next.metadata_1c)) return false;
    uintptr_t check=0;
    if(!fx::read(base_slot,check) || check!=manager || !pointer(manager,0xd0,check) || check!=player ||
       !pointer(player,0xb8,check) || check!=flags || !pointer(action,0x48,check) || check!=owner ||
       !pointer(action,0x40,check,4) || check!=params || !pointer(owner,8,check) || check!=player) return false;
    stage(0); out=next; return true;
}

bool install(uintptr_t base_slot,bool explicit_opt_in) {
    if(!explicit_opt_in) return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;
    fx::require(false,"Lantern research effect is offline-only");
    return false;
#else
    fx::require(!installed.load(),"Lantern effect already installed");
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    uintptr_t address=0;
#ifdef DS2FX_FIXTURE
    address=reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_EffectFixtureTarget"));
    fx::require(address!=0,"Owned lantern effect fixture missing");
#else
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash,"Unsupported executable for lantern effect");
    fx::require(base_slot==image+0x16148f0,"Unexpected BaseA slot for lantern effect");
    address=image+0x21e060;
    fx::require(routine_hash(address)=="0c4794488e4af4e3ee0298adea6189166ed1eeb3b3749f8623fe0c5181c99f40","SpEffectActionCommon::Update bytes changed; lantern effect refused");
#endif
    (void)image;
    fx::require(base_slot!=0,"BaseA slot missing");
    slot=base_slot; image_base=image;
    const auto initialized=MH_Initialize();
    fx::require(initialized==MH_OK || initialized==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize hook backend");
    fx::require(MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&DS2FX_LanternEffectDetour),&DS2FX_LanternEffectOriginal)==MH_OK,"Cannot prepare lantern effect");
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(address));
    if(enabled!=MH_OK) { MH_RemoveHook(reinterpret_cast<void*>(address)); DS2FX_LanternEffectOriginal=nullptr; fx::require(false,"Cannot enable lantern effect"); }
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
    uintptr_t manager=0,player=0,flags=0; int32_t health=0,light=0;
    if(!fx::read(slot,manager) || !pointer(manager,0xd0,player) || player!=output.owner_token ||
       !fx::read(player+0x168,health) || health<=0 || !pointer(player,0xb8,flags) ||
       !fx::read(flags+0x368,light)) {output=Sample{};return false;}
    // Status values are summed by native effect application. A second light
    // effect can produce an unsupported row (e.g. 51+50), so remaining time
    // remains real but "active" requires the owned row to be selected now.
    output.light_value=static_cast<uint32_t>(light);
    output.active=output.remaining_seconds>0 && light==51 ? 1u : 0u;
    return output.available!=0;
}
Status status() noexcept {
    return {calls.load(),matched_calls.load(),samples.load(),skipped.load(),first_thread.load(),installed.load(),observing.load(),fault.load(),status_actions.load(),last_rejection.load()};
}
void observe(uintptr_t action) noexcept {
    ++calls;
    if(!observing.load() || fault.load()) return;
    uintptr_t vtable=0;
    if(!fx::read(action,vtable) || vtable!=image_base+0x10ccbd8) return;
    ++status_actions;
    fx::lantern_item_toggle::observe_active_item(action,image_base);
    Sample next{};
    uint32_t rejection=0;
    const bool valid=read_native_sample(slot,action,image_base,next,&rejection);
    last_rejection.store(rejection);
    if(!valid) return;
    ++matched_calls;
    DWORD expected=0; const DWORD tid=GetCurrentThreadId();
    first_thread.compare_exchange_strong(expected,tid);
    if(first_thread.load()!=tid) {fault.store(true);observing.store(false);return;}
    if(publishing.test_and_set(std::memory_order_acquire)) {++skipped;return;}
    current=next;
    publishing.clear(std::memory_order_release);
    ++samples;
}
}
extern "C" void DS2FX_ObserveLanternEffect(uintptr_t action) noexcept {
    fx::lantern_effect::observe(action);
}
