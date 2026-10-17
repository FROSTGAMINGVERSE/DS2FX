#include "../src/lantern_interact_observer.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
template<class A,class T>void put(A&a,size_t o,T v){std::memcpy(a.data()+o,&v,sizeof(v));}
template<class A>uintptr_t adr(A&a){return reinterpret_cast<uintptr_t>(a.data());}
void check(bool x,const char*w){if(!x)throw std::runtime_error(w);}
int main(){try{std::array<unsigned char,0x200> manager{};std::array<unsigned char,0x300> player{},chr{};std::array<unsigned char,0x80> wrapper{},pad{};std::array<unsigned char,0x200> logical{};uintptr_t slot=adr(manager),image=0x100000;
put(manager,0xd0,adr(player));put(player,0xe8,adr(wrapper));put(player,0xc8,adr(chr));put(wrapper,0, image+0x10cabe8);put(wrapper,0x18,adr(pad));put(pad,0,image+0x10e3f18);put(pad,0x20,adr(logical));put(logical,0,image+0x10e3ae0);put(logical,0x12c,1ull<<14);
check(fx::lantern_interact::prepare_fixture(reinterpret_cast<uintptr_t>(&slot),image,true),"fixture setup");fx::lantern_interact::Sample s{};check(fx::lantern_interact::read(s)&&s.available&&s.held&&!s.conversion_blocked,"exact Interact held sample");put(logical,0x12c,1ull<<13);check(fx::lantern_interact::read(s)&&!s.held,"neighbor action must not read as Interact");put(chr,0xc5,uint8_t{1});check(fx::lantern_interact::read(s)&&s.conversion_blocked,"converter block is published, not a context approval");put(pad,0,image+0x10e3f19);check(!fx::lantern_interact::read(s),"unrecognized pad passes through");fx::lantern_interact::disable();check(!fx::lantern_interact::read(s),"disable stops reads");std::cout<<"PASS: exact DS2FX native Interact observer validates owner/vtables and logical held index; conversion suppression is telemetry only.\n";}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}