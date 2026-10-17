#include "../src/stamina_hook.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>
namespace { std::array<unsigned char,0x200> base{},player{},enemy{}; uintptr_t slot=0; float received=0; }
extern "C" __declspec(dllexport) __declspec(noinline) uint64_t DS2FX_TestStaminaCost(void* controller,float cost,uint32_t reason,uint64_t fourth,uint64_t fifth) {
    received=cost;
    auto owner=*reinterpret_cast<uintptr_t*>(controller);
    auto stamina=reinterpret_cast<float*>(owner+0x1ac);
    *stamina-=cost;
    return fourth^fifth^reason;
}
int main() {
    try {
        namespace h=fx::stamina_hook;
        uintptr_t owner=reinterpret_cast<uintptr_t>(player.data()),other=reinterpret_cast<uintptr_t>(enemy.data());
        slot=reinterpret_cast<uintptr_t>(base.data()); std::memcpy(base.data()+0xd0,&owner,8);
        int32_t hp=100; std::memcpy(player.data()+0x168,&hp,4);
        float sp=100; std::memcpy(player.data()+0x1ac,&sp,4); std::memcpy(enemy.data()+0x1ac,&sp,4);
        fx::require(!h::register_policy(50),"Registration outside initialization refused");
        h::accepting=true; fx::require(!h::register_policy(0) && !h::register_policy(101),"Invalid percentage refused");
        fx::require(h::register_policy(50)==1 && h::register_policy(25)==0,"Exactly one owner"); h::accepting=false;
        h::install(reinterpret_cast<uintptr_t>(&slot));
        auto volatile original=&DS2FX_TestStaminaCost;
        const auto result=original(&owner,20,0,0xabcdef0123456789ULL,0x123456789abcdef0ULL);
        fx::require(result==(0xabcdef0123456789ULL^0x123456789abcdef0ULL) && received==10,"Argument filter must preserve return, fourth and stack arguments");
        float actual=0; std::memcpy(&actual,player.data()+0x1ac,4); fx::require(actual==90,"Original function applies reduced cost");
        original(&other,20,0,0,0); fx::require(received==20,"Enemy unaffected");
        original(&owner,20,1,0,0); fx::require(received==20,"Unknown reason unchanged");
        original(&owner,-20,0,0,0); fx::require(received==-20,"Regeneration unchanged");
        h::percent=100; original(&owner,20,0,0,0); fx::require(received==20,"Observe-only unchanged");
        h::percent=50; hp=0; std::memcpy(player.data()+0x168,&hp,4); original(&owner,20,0,0,0); fx::require(received==20,"Dead player unchanged");
        hp=100; std::memcpy(player.data()+0x168,&hp,4);
        std::thread second([&]{original(&owner,20,0,0,0);}); second.join();
        fx::require(h::fault && received==20,"Unexpected second thread disables modification");
        original(&owner,20,0,0,0); fx::require(received==20,"Fault stays pass-through");
        std::cout<<"PASS real MinHook+MASM trampoline in owned host; reduced cost, return/argument preservation, exclusions, init ownership, observe-only, thread fault\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what();return 1;}
}
