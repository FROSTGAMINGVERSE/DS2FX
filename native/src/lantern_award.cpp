#include "lantern_award.h"
#ifdef DS2FX_LANTERN_RESEARCH
#include "lantern_runtime.h"
#endif
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#include <cmath>

extern "C" { void* DS2FX_LanternAwardOriginal=nullptr; }
namespace fx::lantern_award {
struct Context {
    uintptr_t manager=0, player=0, owner=0, value=0;
    float before=0;
    uint32_t quantity=0;
    DWORD thread=0;
    bool claimed=false;
};
static_assert(sizeof(Context)<=64);
namespace {
uintptr_t slot=0;
std::atomic<bool> installed{false},observing{false},thread_fault{false},queue_fault{false};
std::atomic<DWORD> first_thread{0};
std::atomic<uint64_t> calls{0},local_calls{0},receipts{0},rejected{0};
std::atomic_flag active=ATOMIC_FLAG_INIT, queue_lock=ATOMIC_FLAG_INIT;
std::array<Receipt,64> queue{};
size_t head=0,tail=0,count=0;
bool ptr(uintptr_t p,uintptr_t offset,uintptr_t& out) noexcept {
    out=0;return p && offset<=UINTPTR_MAX-p && fx::read(p+offset,out) && out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
bool canonical(uintptr_t expected_owner,Context& c) noexcept {
    uintptr_t data=0,inventory=0,bag=0;
    int32_t hp=0,maximum=0;
    return fx::read(slot,c.manager) && ptr(c.manager,0xd0,c.player) &&
      fx::read(c.player+0x168,hp) && fx::read(c.player+0x170,maximum) && hp>0 && maximum>=hp && maximum<=1000000 &&
      ptr(c.manager,0xa8,data) && ptr(data,0x10,inventory) && ptr(inventory,0x10,bag) &&
      ptr(bag,0x28,c.owner) && c.owner==expected_owner && ptr(c.owner,0x10,c.value) &&
      fx::read(c.value,c.before) && std::isfinite(c.before) && c.before>=0 && c.before<=100000000;
}
std::string routine_hash(uintptr_t address) {
    std::array<unsigned char,233> bytes{};SIZE_T got=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),bytes.size(),&got) && got==bytes.size(),"Cannot read torch award routine");
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Award SHA256 provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Award SHA256 init failed");
    unsigned char digest[32];
    fx::require(BCryptHashData(hash,bytes.data(),static_cast<ULONG>(bytes.size()),0)==0 && BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Award SHA256 failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
}
bool install(uintptr_t base_slot,bool explicit_opt_in) {
    if(!explicit_opt_in)return false;
#ifdef DS2FX_SEAMLESS_BRIDGE
    (void)base_slot;fx::require(false,"Torch award probe is offline-only");return false;
#else
    fx::require(!installed.load(),"Torch award probe already installed");
    uintptr_t address=0;
#ifdef DS2FX_FIXTURE
    address=reinterpret_cast<uintptr_t>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_AwardFixtureTarget"));
    fx::require(address!=0,"Owned award fixture missing");
#else
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==image+0x16148f0,"Unsupported game/BaseA for torch award probe");
    address=image+0x1bba90;
    fx::require(routine_hash(address)=="5980282bcc89255b3211a3a5b9afdd151314582aede0ceb46101ad1293d3798a","Torch award code changed; refused");
#endif
    fx::require(base_slot!=0,"Missing BaseA slot");slot=base_slot;
    const auto initialized=MH_Initialize();fx::require(initialized==MH_OK || initialized==MH_ERROR_ALREADY_INITIALIZED,"Cannot initialize award hook backend");
    fx::require(MH_CreateHook(reinterpret_cast<void*>(address),reinterpret_cast<void*>(&DS2FX_LanternAwardDetour),&DS2FX_LanternAwardOriginal)==MH_OK,"Cannot prepare torch award probe");
    if(MH_EnableHook(reinterpret_cast<void*>(address))!=MH_OK) {
        MH_RemoveHook(reinterpret_cast<void*>(address));DS2FX_LanternAwardOriginal=nullptr;
        fx::require(false,"Cannot enable torch award probe");
    }
    installed.store(true);observing.store(true);return true;
#endif
}
void disable() noexcept {observing.store(false);}
Status status() noexcept {return {calls.load(),local_calls.load(),receipts.load(),rejected.load(),first_thread.load(),installed.load(),observing.load(),thread_fault.load(),queue_fault.load()};}
bool pop(Receipt& out) noexcept {
    out={};
    if(!observing.load() || thread_fault.load() || queue_fault.load() || queue_lock.test_and_set(std::memory_order_acquire))return false;
    bool available=count!=0;
    if(available){out=queue[head];head=(head+1)%queue.size();--count;}
    queue_lock.clear(std::memory_order_release);
    if(!observing.load() || thread_fault.load() || queue_fault.load()){out={};return false;}
    return available;
}
void begin(uintptr_t owner,uint32_t quantity,Context& context) noexcept {
    context={};++calls;
    if(!observing.load() || quantity==0 || quantity>65535 || !canonical(owner,context))return;
    ++local_calls;
    DWORD expected=0;const auto tid=GetCurrentThreadId();first_thread.compare_exchange_strong(expected,tid);
    if(first_thread.load()!=tid){thread_fault.store(true);observing.store(false);return;}
    if(active.test_and_set(std::memory_order_acquire)){++rejected;return;}
    context.claimed=true;context.thread=tid;context.quantity=quantity;
}
void end(Context& context) noexcept {
    if(!context.claimed)return;
    Context after{};
    const bool valid=observing.load() && !thread_fault.load() && GetCurrentThreadId()==context.thread &&
      canonical(context.owner,after) && after.manager==context.manager && after.player==context.player &&
      after.value==context.value && after.before>context.before;
    if(valid) {
        // No block/spin on the engine thread. Saturation/contention stops this
        // experiment, preventing silently lost receipts being treated as complete.
        if(queue_lock.test_and_set(std::memory_order_acquire)) {queue_fault.store(true);observing.store(false);}
        else {
            if(count==queue.size()){queue_fault.store(true);observing.store(false);}
            else {
                const auto sequence=++receipts;
                const Receipt receipt{sequence,GetTickCount64(),context.quantity,context.before,after.before,context.thread};
                queue[tail]=receipt;tail=(tail+1)%queue.size();++count;
#ifdef DS2FX_LANTERN_RESEARCH
                // Captures provenance inline; any ItemGive is deferred to a later
                // verified native tick. This never dispatches an engine action here.
                fx::lantern_runtime::successful_torch_award(receipt);
#endif
            }
            queue_lock.clear(std::memory_order_release);
        }
    } else ++rejected;
    context.claimed=false;active.clear(std::memory_order_release);
}
}
extern "C" void DS2FX_BeginLanternAward(uintptr_t owner,uint32_t quantity,fx::lantern_award::Context* context) noexcept {
    fx::lantern_award::begin(owner,quantity,*context);
}
extern "C" void DS2FX_EndLanternAward(fx::lantern_award::Context* context) noexcept {
    fx::lantern_award::end(*context);
}
