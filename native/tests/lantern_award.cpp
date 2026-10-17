#include "../src/lantern_award.h"
#include "../src/common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>
#pragma comment(linker,"/export:DS2FX_AwardFixtureTarget")
extern "C" {
void DS2FX_AwardFixtureTarget();
uint64_t DS2FX_AwardInvoke(void* target,uintptr_t owner,uint32_t count);
alignas(16) unsigned char DS2FX_AwardObserved[128]{};
}
namespace {
alignas(16) std::array<unsigned char,0x200> manager{},player{},data{},inventory{},bag{},holder{},other_holder{},value{},other_value{};
uintptr_t slot=0,seen_owner=0;uint32_t seen_quantity=0;
bool fail_award=false,nest=false,inside_nested=false;
template<class A,class T>void put(A& a,size_t o,T v){std::memcpy(a.data()+o,&v,sizeof(v));}
uint64_t qword(size_t o){uint64_t v;std::memcpy(&v,DS2FX_AwardObserved+o,8);return v;}
float torch(){float v;std::memcpy(&v,value.data(),4);return v;}
}
extern "C" void DS2FX_AwardFixtureApply(uintptr_t owner,uint32_t quantity) {
    seen_owner=owner;seen_quantity=quantity;
    if(fail_award || !quantity || quantity>65535)return;
    if(nest && !inside_nested){inside_nested=true;DS2FX_AwardInvoke(reinterpret_cast<void*>(&DS2FX_AwardFixtureTarget),owner,1);inside_nested=false;return;}
    uintptr_t v=0;std::memcpy(&v,reinterpret_cast<void*>(owner+0x10),8);
    float previous=0;std::memcpy(&previous,reinterpret_cast<void*>(v),4);
    previous+=float(quantity)*300;std::memcpy(reinterpret_cast<void*>(v),&previous,4);
}
int main(){try {
    namespace a=fx::lantern_award;
    slot=reinterpret_cast<uintptr_t>(manager.data());
    const auto owner=reinterpret_cast<uintptr_t>(holder.data());
    put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));put(player,0x168,int32_t{100});put(player,0x170,int32_t{100});
    put(manager,0xa8,reinterpret_cast<uintptr_t>(data.data()));put(data,0x10,reinterpret_cast<uintptr_t>(inventory.data()));
    put(inventory,0x10,reinterpret_cast<uintptr_t>(bag.data()));put(bag,0x28,owner);put(holder,0x10,reinterpret_cast<uintptr_t>(value.data()));
    put(other_holder,0x10,reinterpret_cast<uintptr_t>(other_value.data()));
    const auto base_slot=reinterpret_cast<uintptr_t>(&slot);auto target=reinterpret_cast<void*>(&DS2FX_AwardFixtureTarget);
    fx::require(!a::install(base_slot,false) && !a::status().installed,"Award default disabled");
    fx::require(a::install(base_slot,true),"Owned award install");
    fx::require(DS2FX_AwardInvoke(target,owner,2)==0xbadc0ffee0ddf00dULL && seen_owner==owner && seen_quantity==2 && torch()==600,"Original receives unmodified owner/count and runs first");
    fx::require(qword(0)==0xbadc0ffee0ddf00dULL && qword(8)==0x1020304050607080ULL && qword(16)==0xa0b0c0d0e0f01020ULL && (qword(24)&0xcd5)==(0x247&0xcd5),"Original return/GPR/flags preserved after callback");
    for(size_t n=0;n<6;n++)fx::require(qword(32+n*16)==0x123456789abcdef0ULL && qword(40+n*16)==0xfedcba9876543210ULL,"Original XMM results preserved");
    a::Receipt receipt{};fx::require(a::pop(receipt) && receipt.quantity==2 && receipt.native_torch_before==0 && receipt.native_torch_after==600 && receipt.game_thread==GetCurrentThreadId() && receipt.sequence==1,"Validated successful receipt");
    fx::require(!a::pop(receipt),"Receipt delivered once");
    fail_award=true;DS2FX_AwardInvoke(target,owner,1);fail_award=false;fx::require(!a::pop(receipt),"Failed/no-increase award produces no receipt");
    DS2FX_AwardInvoke(target,owner,0);DS2FX_AwardInvoke(target,owner,0xffffffffU);fx::require(!a::pop(receipt),"Invalid quantity produces no receipt; original still called");
    DS2FX_AwardInvoke(target,reinterpret_cast<uintptr_t>(other_holder.data()),1);fx::require(!a::pop(receipt),"Nonlocal owner never generates receipt");
    put(value,0,float{9000});fx::require(!a::pop(receipt),"Raw timer increase is not a loot event");
    nest=true;DS2FX_AwardInvoke(target,owner,1);nest=false;
    fx::require(a::pop(receipt) && receipt.quantity==1 && !a::pop(receipt) && a::status().rejected>=2,"Nested native award cannot duplicate receipt");
    std::thread second([&]{DS2FX_AwardInvoke(target,owner,1);});second.join();
    fx::require(a::status().thread_fault && !a::status().observing && !a::pop(receipt),"Unexpected thread stops receipt provider");
    fx::require(DS2FX_AwardInvoke(target,owner,1)==0xbadc0ffee0ddf00dULL,"Fault leaves original native function running");
    a::disable();fx::require(MH_DisableHook(target)==MH_OK && MH_RemoveHook(target)==MH_OK,"Owned cleanup");
    std::cout<<"PASS actual MinHook+MASM award receipt: original-first, arguments/return/flags/GPR/XMM preserved; canonical owner, exact quantity, no duplicate/no-increase/nonlocal/raw-delta awards; reentry and thread-fault guards\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
