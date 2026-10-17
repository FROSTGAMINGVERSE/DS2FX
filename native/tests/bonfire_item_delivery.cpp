#include "../src/bonfire_item_delivery.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>
namespace {
namespace grant=fx::bonfire_item_delivery;
template<class A,class T>void put(A& a,size_t off,T v){std::memcpy(a.data()+off,&v,sizeof(v));}
struct Fixture;
Fixture* current=nullptr;
uint8_t __fastcall give(uintptr_t,fx::lantern_oil_award::ItemRequest*,int32_t,int32_t);
struct Fixture {
    alignas(16) std::array<unsigned char,0x200> manager{},player{},data{},inventory{},context{},other_data{};
    alignas(16) std::array<unsigned char,40*3841> bag{};
    alignas(16) std::array<unsigned char,40*257> coffer{};
    uintptr_t slot=0,image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    grant::Delivery delivery;uint64_t seq=0;int calls=0;bool accepted=true,reenter=false;
    Fixture(){
        current=this;slot=reinterpret_cast<uintptr_t>(manager.data());
        put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));put(manager,0xa8,reinterpret_cast<uintptr_t>(data.data()));
        put(player,0x168,int32_t{100});put(player,0x170,int32_t{100});
        put(data,0x10,reinterpret_cast<uintptr_t>(inventory.data()));put(other_data,0x10,reinterpret_cast<uintptr_t>(inventory.data()));
        put(inventory,0x10,reinterpret_cast<uintptr_t>(context.data()));
        put(context,0x10,reinterpret_cast<uintptr_t>(bag.data()));put(context,0x18,reinterpret_cast<uintptr_t>(coffer.data()));
        put(bag,0,image+0x10c4408);put(coffer,0,image+0x10c4528);
        fx::require(delivery.prepare_owned_fixture(reinterpret_cast<uintptr_t>(&slot),image,give,true),"Fixture setup");tick();
    }
    void tick(bool continuous=true){delivery.on_engine_tick(slot,continuous);}
    fx::lantern_bonfire::Receipt receipt(){return{GetTickCount64(),++seq,reinterpret_cast<uintptr_t>(player.data()),fx::lantern_bonfire::bonfire_rest_effect};}
    void rest(){delivery.on_bonfire(receipt());}
    void item(bool storage,uint16_t count,uint32_t id=grant::lantern_item){if(storage){put(coffer,40+0x14,id);put(coffer,40+0x20,count);}else{put(bag,40+0x14,id);put(bag,40+0x20,count);}}
};
uint8_t __fastcall give(uintptr_t context,fx::lantern_oil_award::ItemRequest* r,int32_t count,int32_t mode) {
    auto& f=*current;++f.calls;
    fx::require(context==reinterpret_cast<uintptr_t>(f.context.data())&&count==1&&mode==0&&r->kind==0&&r->item==grant::lantern_item&&r->quantity==1&&r->upgrade==0&&r->infusion==0,"Exact native Lantern request; no equipment/Estus request");
    if(f.reenter)f.tick();
    if(f.accepted)f.item(false,1);
    return f.accepted?1:0;
}
}
int main(){try{
    {Fixture f;const auto reads=f.delivery.owner_reads(),checks=f.delivery.inventory_checks();for(int i=0;i<10000;++i)f.tick();fx::require(f.delivery.owner_reads()==reads&&f.delivery.inventory_checks()==checks&&!f.calls,"Ten thousand idle ticks do not read inventory owners, scan slots or grant");}
    {Fixture f;f.rest();f.tick();f.item(false,0);for(int i=0;i<100;++i)f.tick();fx::require(f.calls==1,"Losing Lantern does not trigger grants during idle ticks");f.rest();f.tick();fx::require(f.calls==2&&f.delivery.status().verified==2&&f.delivery.status().last_carried==1,"A later rest restores one missing Lantern after an earlier successful delivery");f.rest();f.tick();fx::require(f.calls==2,"Recovery does not duplicate Lantern on following rest");}
    {Fixture f;f.rest();fx::require(!f.calls,"Inline receipt only queues");f.tick();fx::require(f.calls==1&&f.delivery.status().verified==1,"Next game tick grants and verifies exactly one Lantern");f.rest();f.tick();f.tick();fx::require(f.calls==1&&f.delivery.status().present==1,"Repeated rest and ticks never duplicate");}
    {Fixture f;f.item(true,1);f.rest();f.tick();fx::require(!f.calls&&f.delivery.status().last_stored==1,"Storage ownership prevents duplicate grant");}
    {Fixture f;f.item(false,1,60010000);f.rest();f.tick();fx::require(f.calls==1,"Other goods do not impersonate Lantern");}
    {Fixture f;auto r=f.receipt();f.delivery.on_bonfire(r);f.delivery.on_bonfire(r);f.tick();fx::require(f.calls==1,"Receipt duplicate fence");}
    {Fixture f;auto r=f.receipt();r.owner_token+=8;f.delivery.on_bonfire(r);f.tick();fx::require(!f.calls,"Foreign player cannot receive item");}
    {Fixture f;auto r=f.receipt();r.effect_id=60425000;f.delivery.on_bonfire(r);f.tick();fx::require(!f.calls,"Oil effect cannot authorize delivery");}
    {Fixture f;f.rest();put(f.manager,0xa8,reinterpret_cast<uintptr_t>(f.other_data.data()));f.tick();fx::require(!f.calls&&f.delivery.status().discarded==1,"Same player pointer with different character data discards old receipt");f.rest();f.tick();fx::require(f.calls==1,"Fresh receipt can grant newly loaded owner");}
    {Fixture f;f.rest();f.slot=0;f.tick();f.slot=reinterpret_cast<uintptr_t>(f.manager.data());f.tick();f.tick();fx::require(!f.calls,"Unload/reload with identical pointers cannot replay queued grant");}
    {Fixture f;f.rest();put(f.player,0x168,int32_t{0});f.tick();put(f.player,0x168,int32_t{100});f.tick();f.tick();fx::require(!f.calls,"Death invalidates queued owner");}
    {Fixture f;f.slot=0;f.rest();f.tick();fx::require(!f.calls&&f.delivery.status().rejected==1,"Unreadable canonical owner on rest never means missing");}
    {Fixture f;auto r=f.receipt();r.sampled_at_ms-=2000;f.delivery.on_bonfire(r);f.tick();fx::require(!f.calls,"Stale rest cannot authorize recovery");}
    {Fixture f;f.rest();f.item(false,1);f.tick();fx::require(!f.calls&&f.delivery.status().present==1,"Lantern acquired between receipt and next tick prevents duplicate");}
    {Fixture f;f.accepted=false;f.rest();f.tick();f.accepted=true;f.rest();f.tick();fx::require(f.calls==2&&f.delivery.status().verified==1,"Another genuine rest may recover after native refusal");}
    {Fixture f;f.rest();f.tick(false);f.tick();fx::require(!f.calls,"Paused/discontinuous frame cannot grant");}
    {Fixture f;put(f.bag,0,f.image+0x10c4528);f.rest();f.tick();fx::require(!f.calls,"Unknown inventory layout never counts as missing");}
    {Fixture f;f.accepted=false;f.rest();f.tick();f.tick();fx::require(f.calls==1&&f.delivery.status().unverified==1&&!f.delivery.status().verified,"Native refusal is never retried every frame or marked verified");}
    {Fixture f;auto r=f.receipt();std::thread worker([&]{f.delivery.on_bonfire(r);});worker.join();f.tick();fx::require(!f.calls,"Worker cannot queue grants");std::thread bad_tick([&]{f.tick();});bad_tick.join();f.rest();f.tick();fx::require(!f.calls&&f.delivery.status().fault,"Wrong tick thread disables native writes");}
    {Fixture f;f.rest();f.reenter=true;f.tick();f.tick();fx::require(f.calls==1&&f.delivery.status().fault,"Native reentry stops without duplicate or deadlock");}
    {Fixture f;f.rest();f.delivery.disable();f.tick();fx::require(!f.calls,"Disable cancels pending actions");}
    std::cout<<"PASS bonfire item delivery: carried/storage ownership, one native grant, later engine tick, verified quantity, duplicate/thread/lifetime/reentry gates\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
