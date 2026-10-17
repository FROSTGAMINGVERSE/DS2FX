#include "../src/lantern_oil_award.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
namespace oil=fx::lantern_oil_award;
struct Call {uintptr_t bag;oil::ItemRequest request;int32_t count,mode;DWORD thread;};
std::vector<Call> calls;
bool validation=true,reenter=false;
oil::Delivery* current_delivery=nullptr;
uint8_t __fastcall owned_item_give(uintptr_t bag,oil::ItemRequest* request,int32_t count,int32_t mode) {
    calls.push_back({bag,*request,count,mode,GetCurrentThreadId()});
    if(reenter)current_delivery->on_engine_tick(1);
    return validation?1:0;
}
template<class A,class T> void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
struct Fixture {
    alignas(16) std::array<unsigned char,0x200> manager{},player{},data{},inventory{},bag{},other_bag{},holder{};
    uintptr_t slot=0;
    oil::Delivery delivery;
    Fixture() {
        calls.clear();validation=true;reenter=false;current_delivery=&delivery;
        slot=reinterpret_cast<uintptr_t>(manager.data());
        put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
        put(player,0x168,int32_t{100});put(player,0x170,int32_t{100});
        put(manager,0xa8,reinterpret_cast<uintptr_t>(data.data()));
        put(data,0x10,reinterpret_cast<uintptr_t>(inventory.data()));
        put(inventory,0x10,reinterpret_cast<uintptr_t>(bag.data()));
        put(bag,0x28,reinterpret_cast<uintptr_t>(holder.data()));
        put(other_bag,0x28,reinterpret_cast<uintptr_t>(holder.data()));
    }
    uintptr_t base(){return reinterpret_cast<uintptr_t>(&slot);}
    void start(){fx::require(delivery.prepare_owned_fixture(base(),owned_item_give,true),"Owned preparation");delivery.on_engine_tick(1);}
    fx::lantern_award::Receipt receipt(uint64_t sequence,uint32_t quantity=1) {
        return {sequence,GetTickCount64(),quantity,600,600+300.0f*quantity,GetCurrentThreadId()};
    }
    void queue(uint64_t sequence,uint32_t quantity=1,uint64_t epoch=1) {
        fx::require(delivery.on_successful_torch_award(receipt(sequence,quantity),epoch),"Queue typed successful award");
    }
};
}
int main(){try {
    {
        Fixture f;
        fx::require(!f.delivery.prepare(f.base(),false) && !f.delivery.status().prepared,"Native default disabled");
        bool unsupported_refused=false;
        try{f.delivery.prepare(f.base(),true);}catch(const std::exception&){unsupported_refused=true;}
        fx::require(unsupported_refused && !f.delivery.status().prepared && calls.empty(),"Unsupported host cannot prepare the real native address");
        fx::require(!f.delivery.prepare_owned_fixture(f.base(),owned_item_give,false),"Fixture requires opt-in");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(1),1),"No receipt before preparation");
        f.delivery.prepare_owned_fixture(f.base(),owned_item_give,true);
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(1),1),"No receipt before engine thread established");
        f.delivery.on_engine_tick(1);
        const auto original_receipt=f.receipt(1,2);auto unchanged_receipt=original_receipt;
        fx::require(f.delivery.on_successful_torch_award(unchanged_receipt,1) &&
            std::memcmp(&original_receipt,&unchanged_receipt,sizeof(original_receipt))==0,"Source receipt inputs preserved");
        f.queue(2,7);
        fx::require(calls.empty() && f.delivery.status().pending==2,"Queueing never invokes inventory function");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(2,7),1),"Duplicate queued receipt rejected");
        f.delivery.on_engine_tick(1);
        fx::require(calls.size()==1 && calls[0].request.quantity==2 && f.delivery.status().pending==1,"One call on next engine tick");
        const auto& c=calls[0];
        fx::require(c.bag==reinterpret_cast<uintptr_t>(f.bag.data()) && c.request.kind==0 &&
            c.request.item==60425000 && c.request.durability==(std::numeric_limits<float>::max)() &&
            c.request.upgrade==0 && c.request.infusion==0 && c.count==1 && c.mode==0 &&
            c.thread==GetCurrentThreadId(),"Exact canonical bag, request bytes, registers and engine thread");
        f.delivery.on_engine_tick(1);f.delivery.on_engine_tick(1);
        fx::require(calls.size()==2 && calls[1].request.quantity==7,"Queued quantities preserved, no replay on extra ticks");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(1),1),"Consumed old receipt rejected");
        validation=false;f.queue(3);f.delivery.on_engine_tick(1);f.delivery.on_engine_tick(1);
        fx::require(calls.size()==3 && f.delivery.status().native_validation_true==2,"Native false is issued once and never retried");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(4,0),1) &&
            !f.delivery.on_successful_torch_award(f.receipt(5,65536),1),"Invalid quantities cannot truncate");
        auto no_award=f.receipt(6);no_award.native_torch_after=no_award.native_torch_before;
        fx::require(!f.delivery.on_successful_torch_award(no_award,1),"No-increase receipt rejected");
        auto stale=f.receipt(7);stale.observed_at_ms-=6000;
        fx::require(!f.delivery.on_successful_torch_award(stale,1),"Stale receipt rejected");
        f.queue(8,65535);f.delivery.on_engine_tick(1);
        fx::require(calls.back().request.quantity==65535,"Native uint16 quantity boundary preserved");
    }
    {
        Fixture f;f.start();auto receipt=f.receipt(1);bool accepted=true;
        std::thread worker([&]{accepted=f.delivery.on_successful_torch_award(receipt,1);});worker.join();
        fx::require(!accepted && calls.empty() && f.delivery.status().pending==0,"Worker cannot bind or deliver a receipt");
        f.queue(1);std::thread wrong_tick([&]{f.delivery.on_engine_tick(1);});wrong_tick.join();
        f.delivery.on_engine_tick(1);
        fx::require(calls.empty() && f.delivery.status().thread_fault && !f.delivery.status().enabled,"Wrong-thread update permanently stops delivery");
    }
    {
        Fixture f;f.start();f.queue(1);
        put(f.inventory,0x10,reinterpret_cast<uintptr_t>(f.other_bag.data()));
        f.delivery.on_engine_tick(1);
        fx::require(calls.empty() && f.delivery.status().discarded==1,"Bag changed before dispatch discards award");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(2),1),"Same epoch cannot rebind changed bag");
        f.delivery.on_engine_tick(2);f.queue(3,4,2);f.delivery.on_engine_tick(2);
        fx::require(calls.size()==1 && calls[0].bag==reinterpret_cast<uintptr_t>(f.other_bag.data()),"Explicit new epoch permits new canonical bag");
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(1),2),"Epoch change does not reset duplicate fence");
        f.queue(4,1,2);f.delivery.on_engine_tick(3);
        fx::require(calls.size()==1 && f.delivery.status().pending==0,"Epoch change with identical pointers still discards old award");
    }
    {
        Fixture f;f.start();f.queue(1);put(f.player,0x168,int32_t{0});f.delivery.on_engine_tick(1);
        put(f.player,0x168,int32_t{100});f.delivery.on_engine_tick(1);
        fx::require(calls.empty() && !f.delivery.on_successful_torch_award(f.receipt(2),1),"Death clears queue and requires new lifetime epoch");
        f.delivery.on_engine_tick(2);f.queue(3,1,2);f.slot=0;f.delivery.on_engine_tick(2);
        fx::require(calls.empty() && f.delivery.status().pending==0,"Unavailable manager prevents all writes");
    }
    {
        Fixture f;f.start();f.queue(1);f.delivery.disable();f.delivery.on_engine_tick(1);
        fx::require(calls.empty() && !f.delivery.status().enabled,"Disable prevents pending calls");
    }
    {
        Fixture f;f.start();for(uint64_t n=1;n<=64;++n)f.queue(n);
        fx::require(!f.delivery.on_successful_torch_award(f.receipt(65),1),"Bounded queue overflow rejected");
        f.delivery.on_engine_tick(1);
        fx::require(calls.empty() && f.delivery.status().queue_fault && !f.delivery.status().enabled,"Overflow stops rather than silently loses awards");
    }
    {
        Fixture f;f.start();f.queue(1);reenter=true;f.delivery.on_engine_tick(1);f.delivery.on_engine_tick(1);
        fx::require(calls.size()==1 && f.delivery.status().queue_fault && !f.delivery.status().enabled,"Reentrant native callback never duplicates or blocks");
    }
    std::cout<<"PASS isolated companion oil: exact native request/quantity, next engine tick only, no worker writes, identity/epoch/liveness gates, duplicate fence, no native-failure retry, bounded queue, reentry/thread-stop guards\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
