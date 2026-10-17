#include "player_message_observer.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
extern "C" {
void* DS2FX_MessageOriginal=nullptr;
void DS2FX_MessageDetour();
}
namespace jump_observer {
namespace {
uintptr_t slot=0, image=0;
std::atomic<bool> enabled{false};
std::atomic_flag publication=ATOMIC_FLAG_INIT;
std::atomic<uint64_t> sequence{0};
std::atomic<bool> jump_routes_enabled{false};
std::atomic<ThreadAuthority> thread_authority{nullptr};
std::atomic<uintptr_t> native_jump_caller{0};
Receipt receipt{};
bool pointer(uintptr_t base, uintptr_t offset, uintptr_t& out) noexcept {
    out=0;
    return base && offset<=UINTPTR_MAX-base && fx::read(base+offset,out) &&
        out>=0x10000 && out<=0x7fffffffffffULL && !(out&7);
}
bool owner(uintptr_t controller, uintptr_t& player) noexcept {
    uintptr_t manager=0, motion=0, morph=0, network=0, ctrl=0, vt=0, verify=0;
    int32_t hp=0,max=0;
    if(!fx::read(slot,manager) || !pointer(manager,0xd0,player) ||
       !fx::read(player,vt) || vt!=image+0x10e4bb8 ||
       !pointer(player,0xf8,ctrl) || !fx::read(ctrl,vt) || vt!=image+0x10dff98 ||
       !pointer(ctrl,0x40,motion) || !fx::read(motion,vt) || vt!=image+0x10e52a8 ||
       !pointer(motion,0x18,morph) || morph!=controller ||
       !fx::read(morph,vt) || vt!=image+0x10f7f28 ||
       !pointer(morph,0x20,network) || network!=morph+0x60 ||
       !fx::read(player+0x168,hp) || !fx::read(player+0x170,max) || hp<=0 || hp>max || max>1000000)
        return false;
    return fx::read(slot,verify) && verify==manager && pointer(manager,0xd0,verify) && verify==player &&
        pointer(player,0xf8,verify) && verify==ctrl && pointer(ctrl,0x40,verify) && verify==motion &&
        pointer(motion,0x18,verify) && verify==morph;
}
bool movement_sample(uintptr_t player,uintptr_t controller,fx::jump_routes::Movement& out) noexcept {
    out={};
    uintptr_t op=0,pad=0,logical=0,vt=0,definition=0,bins=0,speed_pin=0,moving_pin=0;
    uintptr_t speed_attrib=0,moving_attrib=0,verify=0;
    uint32_t count=0,speed_type=0,moving_type=0;
    float vector[4]{};
    if(!pointer(player,0xe8,op) || !fx::read(op,vt) || vt!=image+0x10cabe8 ||
       !pointer(op,0x18,pad) || !fx::read(pad,vt) || vt!=image+0x10e3f18 ||
       !pointer(pad,0x20,logical) || !fx::read(logical,vt) || vt!=image+0x10e3ae0 ||
       !pointer(controller,0x60,definition) || !fx::read(definition+144,count) || (count!=9190 && count!=9197) ||
       !pointer(controller,0x78,bins) || !pointer(bins,48+32,speed_pin) ||
       !pointer(bins,130*48+32,moving_pin) ||
       !pointer(speed_pin,8,speed_attrib) || !pointer(moving_pin,8,moving_attrib) ||
       !fx::read(speed_attrib+8,speed_type) || (speed_type>>16)!=3 ||
       !fx::read(moving_attrib+8,moving_type) || (moving_type>>16)!=2 ||
       !fx::read(speed_attrib+16,out.speed) || !fx::read(moving_attrib+16,out.moving) ||
       !fx::read(logical+0x12c,out.mask) || !fx::read(pad+0x40,vector)) return false;
    auto same=[](uintptr_t parent,uintptr_t offset,uintptr_t expected) noexcept {
        uintptr_t actual=0;return pointer(parent,offset,actual) && actual==expected;
    };
    if(!owner(controller,verify) || verify!=player || !same(player,0xe8,op) ||
       !same(op,0x18,pad) || !same(pad,0x20,logical) || !same(controller,0x60,definition) ||
       !same(controller,0x78,bins) || !same(bins,48+32,speed_pin) ||
       !same(bins,130*48+32,moving_pin) || !same(speed_pin,8,speed_attrib) ||
       !same(moving_pin,8,moving_attrib)) {out={};return false;}
    out.x=vector[0];out.z=vector[2];out.known=true;return true;
}
bool jump_graph(uintptr_t controller) noexcept {
    uintptr_t definition=0,nodes=0,table=0,distributor=0,subscribers=0,tags=0;
    uint32_t count=0,id=0,type=0,subcount=0;uint16_t subscriber=0,tag_nodes=0;
    if(!pointer(controller,0x60,definition) || !fx::read(definition+144,count) || count!=9197 ||
       !pointer(definition,152,nodes) || !pointer(definition,224,tags) ||
       !fx::read(tags+26,tag_nodes) || tag_nodes!=9197 ||
       !fx::read(definition+248,count) || count!=60 ||
       !pointer(definition,256,table) || !pointer(table,59*8,distributor) ||
       !fx::read(distributor,id) || id!=59 || !fx::read(distributor+4,type) || type!=10 ||
       !fx::read(distributor+8,subcount) || subcount!=1 || !pointer(distributor,16,subscribers) ||
       !fx::read(subscribers,subscriber) || subscriber!=210) return false;
    for(const auto node_id:{9190u,9192u,9193u}) {
        uintptr_t node=0,handles=0,attrib=0;uint16_t actual=0,num=0;
        if(!pointer(nodes,node_id*8,node) || !fx::read(node,type) || type!=104 ||
           !fx::read(node+6,actual) || actual!=node_id || !fx::read(node+16,num) || num<2 ||
           !pointer(node,48,handles) || !pointer(handles,24,attrib) ||
           !fx::read(attrib+8,type) || (type>>16)!=21 || !fx::read(attrib+116,id) ||
           id!=(node_id==9193?948u:995u)) return false;
    }
    return true;
}
bool playable() noexcept {
    uintptr_t manager=0,root=0,leaf=0,verify=0;
    int32_t state=0,after=0;uint8_t loading=2,loading_after=2;
    return fx::read(slot,manager) && manager && fx::read(manager+0x24ac,state) && state==30 &&
        pointer(manager,0x80,root) && pointer(root,8,leaf) &&
        fx::read(leaf+0xbb4,loading) && loading==0 &&
        fx::read(manager+0x24ac,after) && after==state &&
        fx::read(leaf+0xbb4,loading_after) && loading_after==loading &&
        pointer(manager,0x80,verify) && verify==root && pointer(root,8,verify) && verify==leaf &&
        fx::read(slot,verify) && verify==manager;
}
bool setup(uintptr_t base_slot,uintptr_t base_image,void* target,bool opt_in) {
    if(!opt_in) return false;
    fx::require(!enabled.load() && base_slot && base_image && target,"Invalid observer setup");
    slot=base_slot; image=base_image;
    auto result=MH_Initialize();
    fx::require(result==MH_OK || result==MH_ERROR_ALREADY_INITIALIZED,"Observer backend initialization failed");
    fx::require(MH_CreateHook(target,reinterpret_cast<void*>(&DS2FX_MessageDetour),&DS2FX_MessageOriginal)==MH_OK,
                "Observer trampoline creation failed");
    if(MH_EnableHook(target)!=MH_OK) {
        MH_RemoveHook(target); DS2FX_MessageOriginal=nullptr;
        fx::require(false,"Observer installation failed");
    }
    enabled.store(true);
    return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in) return false;
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==base+0x16148f0,
                "Unsupported message-observer executable/slot");
    constexpr std::array<unsigned char,73> expected{
      0x48,0x83,0xec,0x48,0x48,0x8b,0x49,0x20,0x48,0x85,0xc9,0x74,0x35,0x83,0xfa,0xff,
      0x74,0x30,0x33,0xc0,0x89,0x54,0x24,0x2c,0x48,0x8d,0x54,0x24,0x20,0xc7,0x44,0x24,
      0x30,0x0a,0,0,0,0x44,0x88,0x44,0x24,0x34,0x48,0x89,0x44,0x24,0x20,0x89,0x44,
      0x24,0x28,0xe8,0x58,0x14,0x11,0,0x85,0xc0,0x0f,0x9f,0xc0,0x48,0x83,0xc4,0x48,
      0xc3,0x32,0xc0,0x48,0x83,0xc4,0x48,0xc3};
    std::array<unsigned char,73> bytes{};
    fx::require(fx::read(base+0x4da9e0,bytes) && bytes==expected,"Message-dispatch instruction mismatch");
    // The same canonical jump state action runs on native animation job threads.
    // Authenticate its exact call site and tail-call converter before accepting
    // an alternate thread. This scope changes only the existing dispatch message;
    // it is not authority for an API callback to call an engine action.
    constexpr std::array<unsigned char,14> callsite{
        0x48,0x8d,0x4b,0x30,0x41,0xb0,0x01,0x33,0xd2,0xe8,0xdd,0x1b,0x18,0x00};
    constexpr std::array<unsigned char,18> converter{
        0x48,0x8b,0x41,0x08,0x48,0x8b,0x09,0x48,0x63,0xd2,0x8b,0x14,0x90,0xe9,0xde,0x7c,0xff,0xff};
    std::array<unsigned char,14> site{};std::array<unsigned char,18> convert{};
    fx::require(fx::read(base+0x361105,site) && site==callsite &&
        fx::read(base+0x4e2cf0,convert) && convert==converter,"Native jump call-context mismatch");
    native_jump_caller.store(base+0x361113,std::memory_order_release);
    return setup(base_slot,base,reinterpret_cast<void*>(base+0x4da9e0),true);
}
void disable() noexcept {jump_routes_enabled.store(false);enabled.store(false);}
void configure_jump_routes(bool active,ThreadAuthority authority) noexcept {
    jump_routes_enabled.store(false);
    thread_authority.store(authority,std::memory_order_release);
    jump_routes_enabled.store(active && authority,std::memory_order_release);
}
bool snapshot(Receipt& out) noexcept {
    out={};
    if(!enabled.load() || publication.test_and_set(std::memory_order_acquire)) return false;
    out=receipt; publication.clear(std::memory_order_release);
    uintptr_t player=0;
    if(!enabled.load() || !out.sequence || GetTickCount64()-out.timestamp>1500 ||
        !owner(out.controller,player) || player!=out.player) {out={};return false;}
    return true;
}
uint32_t observe(uintptr_t controller,uint32_t message,uint8_t status,uintptr_t caller) noexcept {
    if(!enabled.load() || (message!=8 && message!=9)) return message;
    uintptr_t player=0;
    uint32_t frame=0;
    if(!owner(controller,player) || !fx::read(controller+0x88,frame)) return message;
    fx::jump_routes::Movement movement{};
    const bool sampled=message==8 && status==1 && movement_sample(player,controller,movement);
    uint32_t after=0;
    if(sampled && (!fx::read(controller+0x88,after) || after!=frame)) movement={};
    uint32_t selected=message;
    const auto proposed=fx::jump_routes::classify(movement);
    const auto authority=thread_authority.load(std::memory_order_acquire);
    uintptr_t owner_after=0;
    uint32_t frame_after=0;
    const auto authenticated_caller=native_jump_caller.load(std::memory_order_acquire);
    const bool native_scope=authority && (authority() ||
        (authenticated_caller && caller==authenticated_caller));
    if(message==8 && status==1 && jump_routes_enabled.load() && native_scope &&
       playable() && jump_graph(controller) && owner(controller,owner_after) && owner_after==player &&
       fx::read(controller+0x88,frame_after) && frame_after==frame) {
        if(proposed==fx::jump_routes::Route::idle) selected=9;
        else if(proposed==fx::jump_routes::Route::moving) selected=59;
    }
    // No waiting, logging or extra engine calls on the native dispatch path.
    // Under contention retain the original native jump.
    if(publication.test_and_set(std::memory_order_acquire)) return message;
    receipt={sequence.fetch_add(1)+1,GetTickCount64(),player,controller,message,frame,GetCurrentThreadId(),status};
    receipt.caller=caller;receipt.movement=movement;
    receipt.proposed_route=proposed;receipt.dispatched_message=selected;
    publication.clear(std::memory_order_release);
    return selected;
}
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t base_slot,uintptr_t base_image,void* target,bool opt_in) {
    return setup(base_slot,base_image,target,opt_in);
}
void authorize_fixture_caller(uintptr_t caller) noexcept {native_jump_caller.store(caller);}
#endif
}
extern "C" uint32_t DS2FX_ObserveMessage(uintptr_t controller,uint32_t message,uint8_t status,uintptr_t caller) noexcept {
    return jump_observer::observe(controller,message,status,caller);
}
