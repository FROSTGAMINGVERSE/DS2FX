#include "lantern_flame.h"
#include "lantern_native_hash.h"
#include "lantern_item_toggle.h"
#include <atomic>
#include <cmath>
namespace fx::lantern_flame {
namespace {
using Create=int32_t(__fastcall*)(uintptr_t,uint16_t,int32_t,uint16_t,bool,float,int32_t);
using Remove=void(__fastcall*)(uintptr_t,int32_t);
struct alignas(16) Matrix {float rows[4][4];};
using Resolve=bool(__fastcall*)(uintptr_t,int32_t,void*,bool);
Create create=nullptr;Remove remove=nullptr;Resolve resolve=nullptr;
uintptr_t slot=0,image=0,last_player=0,last_control=0,owned_node=0;
uintptr_t last_scene=0;
uint32_t owned_packed=0;bool attempted=false;
std::atomic<bool> enabled{false},fault{false};std::atomic<DWORD> thread{0};
std::atomic<int32_t> handle{-1};std::atomic<uint64_t> created{0},removed{0},missing{0},stale{0},rejected{0};
std::atomic_flag busy=ATOMIC_FLAG_INIT;
bool ptr(uintptr_t p,uintptr_t offset,uintptr_t& result){return p&&offset<=UINTPTR_MAX-p&&fx::read(p+offset,result)&&result>=0x10000&&!(result&7);}
bool scoped(uintptr_t manager,uintptr_t& player,uintptr_t& control) {
    uintptr_t current=0,vt=0,owner=0;
    return fx::read(slot,current)&&manager&&current==manager&&ptr(manager,0xd0,player)&&ptr(player,0x248,control)&&
        fx::read(control,vt)&&vt==image+0x10e08d8&&ptr(control,0x58,owner)&&owner==player;
}
// Engine SfxSlotCtrl linked list: +4 effect, +70 handle, +78 next.
// Never remove by effect ID or slot category: only the exact handle returned
// to this adapter, still pointing at its own effect/dummy, may be released.
bool find_handle(uintptr_t control,int32_t id,bool capture=false) {
    uintptr_t node=0;if(!fx::read(control+0x20,node))return false;
    for(unsigned n=0;node&&n<4096;++n) {
        uint32_t packed=0;int32_t effect=0,serial=-1;uintptr_t next=0;
        if(!fx::read(node,packed)||!fx::read(node+4,effect)||!fx::read(node+0x70,serial)||!fx::read(node+0x78,next))return false;
        if(serial==id) {
            if(effect!=6042||(packed&0x7fff)!=0x4087)return false;
            if(capture){owned_node=node;owned_packed=packed;return true;}
            return node==owned_node&&packed==owned_packed;
        }
        if(next==node)return false;node=next;
    }
    return false;
}
void setup(uintptr_t base_slot,uintptr_t exe,Create c,Remove r,Resolve d) {
    fx::require(!enabled.load()&&base_slot&&exe&&c&&r&&d,"Invalid Lantern flame preparation");
    slot=base_slot;image=exe;create=c;remove=r;resolve=d;enabled.store(true);
}
}
void prepare(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return;
    const auto exe=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==exe+0x16148f0,"Unsupported executable for Lantern flame");
    fx::require(fx::lantern_native_hash(exe+0x21cf20,0xa5)=="a7e8f4848c0ae7015dcc1ca3543df27119e15648aea299d7280345148edcc9e5","Native flame call ABI changed");
    fx::require(fx::lantern_native_hash(exe+0x312750,375)=="57235a92e66c8c1a7687b0f2f7aae12f45ff598f664dec8d8807fe3696b03ee9","Wick resolver changed");
    // Whole executable hash covers the expanded/protected native add/remove
    // bodies as well as the call sites and CharacterSfxSlotCtrl owner layout.
    setup(base_slot,exe,reinterpret_cast<Create>(exe+0x4c7ca0),reinterpret_cast<Remove>(exe+0x4c8120),reinterpret_cast<Resolve>(exe+0x312750));
}
void disable() noexcept {enabled.store(false);}
Status status() noexcept {return {created.load(),removed.load(),missing.load(),stale.load(),rejected.load(),handle.load(),thread.load(),enabled.load(),fault.load()};}
void native_tick(uintptr_t manager) noexcept {
    if(!enabled.load()||fault.load())return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();thread.compare_exchange_strong(expected,tid);
    if(thread.load()!=tid){++rejected;return;}
    if(busy.test_and_set()){fault.store(true);enabled.store(false);return;}
    struct Unlock {~Unlock(){busy.clear();}} unlock;
    uintptr_t player=0,control=0;
    // Keep the token through a temporarily unavailable manager. Never read old
    // pointers until the same canonical owner/control is authenticated again.
    // A different authenticated owner discards it below; the engine destroys
    // the former character and its SFX controller.
    if(!scoped(manager,player,control))return;
    if(player!=last_player||control!=last_control) {last_player=player;last_control=control;handle.store(-1);owned_node=0;attempted=false;}
    // Loading can destroy native SFX without replacing the character/control.
    // Re-arm once per authenticated scene change; never allocate every frame
    // when a native result is untrackable or a handle has been recycled.
    uintptr_t scene=0;
    if(!fx::read(manager+0xbc8,scene)||!scene) {last_scene=0;return;}
    if(scene!=last_scene) {last_scene=scene;attempted=false;}
    const auto id=handle.load();
    if(id!=-1&&!find_handle(control,id)){++stale;handle.store(-1);}
    if(!fx::lantern_item_toggle::active_for(player)) {
        const auto owned=handle.exchange(-1);
        if(owned!=-1&&find_handle(control,owned)){remove(control,owned);++removed;}
        owned_node=0;attempted=false;return;
    }
    if(handle.load()!=-1||attempted)return;
    Matrix wick{};
    if(!resolve(player,247,&wick,true)){++missing;return;}
    for(auto& row:wick.rows)for(float value:row)if(!std::isfinite(value)){++missing;return;}
    uintptr_t check_player=0,check_control=0;
    if(!scoped(manager,check_player,check_control)||check_player!=player||check_control!=control||!fx::lantern_item_toggle::active_for(player))return;
    // Same native pool selector and seven arguments used by Basic_Sfx's
    // EffectTimeExpired path. Engine owns allocation, animation and lifetime.
    attempted=true; // One allocation attempt per activation; never retry a lost handle every frame.
    const auto result=create(control,0xc087,6042,247,false,0.0f,0);
    if(result!=-1){++created;if(find_handle(control,result,true))handle.store(result);else ++stale;}
}
#ifdef DS2FX_FIXTURE
void prepare_owned_fixture(uintptr_t base_slot,uintptr_t exe,Create c,Remove r,Resolve d){setup(base_slot,exe,c,r,d);}
#endif
}
