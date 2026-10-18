#include "../src/seamless_welcome.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
namespace {
constexpr wchar_t welcome[]=L"#c[FF5BCEFA][Announcement -- This is a free mod]#c\n"
    L"#c[FFF5A9B8]Welcome to Dark Souls II: Seamless Co-op!#c\n"
    L"#c[FFFFFFFF]If you have paid any money for this then you have been scammed!#c";
template<class T,size_t N> void put(std::array<unsigned char,N>& data,size_t at,T value){std::memcpy(data.data()+at,&value,sizeof(value));}
template<class T,size_t N> T get(std::array<unsigned char,N>& data,size_t at){T value{};std::memcpy(&value,data.data()+at,sizeof(value));return value;}
}
int main(){try{
    using namespace fx::seamless_welcome;
    constexpr uintptr_t image=0x100000000;
    alignas(8) std::array<unsigned char,0x600> global{};
    uintptr_t slot=reinterpret_cast<uintptr_t>(global.data());
    alignas(8) std::array<unsigned char,0x40> event{};
    std::array<wchar_t,std::size(welcome)> text{};
    auto reset=[&]{
        event.fill(0);global.fill(0);std::memcpy(text.data(),welcome,sizeof(welcome));
        put(event,0,image+0x10bcff8);put(event,0xc,0x3fu);put<int16_t>(event,0x10,0x2a);put<int16_t>(event,0x12,-1);
        put(event,0x38,reinterpret_cast<uintptr_t>(text.data()));
        put(global,0x118,reinterpret_cast<uintptr_t>(text.data()));
        put<uint64_t>(global,0x128,std::size(welcome)-1);put<uint64_t>(global,0x130,511);
        slot=reinterpret_cast<uintptr_t>(global.data());
    };
    // Owned executable stub: original initializer records its invocation in padding.
    // The real detour/trampoline is exercised, not merely the classifier.
    auto target=static_cast<unsigned char*>(VirtualAlloc(nullptr,64,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    fx::require(target!=nullptr,"Owned fixture allocation failed");
    std::memset(target,0x90,64);const unsigned char body[]{0xff,0x41,0x1c,0xc3};std::memcpy(target,body,sizeof(body));
    using Init=void(*)(uintptr_t);Init volatile call=reinterpret_cast<Init>(target);
    fx::require(install_fixture(image,reinterpret_cast<uintptr_t>(&slot),target),"Fixture hook not installed");
    reset();auto expected=event;put<uint16_t>(expected,0x30,0x0103);call(reinterpret_cast<uintptr_t>(event.data()));
    fx::require(event==expected && suppressed()==1,"Welcome failed to take native accepted transition or changed unrelated bytes");
    // The native transition descriptor compares state=3 to select event+0x10.
    fx::require(get<uint8_t>(event,0x30)==3 && get<int16_t>(event,0x10)==0x2a,"Accepted native transition lost");
    auto passes=[&](const char* why){auto before=event;put<uint32_t>(before,0x1c,1);call(reinterpret_cast<uintptr_t>(event.data()));fx::require(event==before,why);};
    reset();text[20]=L'X';passes("Changed text was hidden");
    reset();text.back()=L'X';passes("Unterminated text was hidden");
    reset();put(event,0,uintptr_t{1});passes("Foreign class was hidden");
    reset();put(event,0xc,0x3eu);passes("Different error event was hidden");
    reset();put<int16_t>(event,0x10,0x40);passes("Different next event was hidden");
    reset();put<int16_t>(event,0x12,0x41);passes("Cancelable prompt was hidden");
    reset();put<float>(event,0x14,1);passes("Timed prompt was hidden");
    reset();put<uint16_t>(event,0x30,1);passes("Already active native dialog was changed");
    reset();put<uint64_t>(global,0x128,5);passes("Wrong text length was accepted");
    reset();put<uint64_t>(global,0x130,512);passes("Unknown string capacity was accepted");
    reset();put(event,0x38,uintptr_t{1});passes("Foreign text pointer was accepted");
    reset();slot=1;passes("Invalid owner was accepted");
    reset();set_enabled_fixture(false);passes("Disabled setting was ignored");
    reset();set_enabled_fixture(true);call(reinterpret_cast<uintptr_t>(event.data()));fx::require(suppressed()==2,"Repeated boot notice not handled");
    fx::require(!install(false),"Disabled production install mutated hooks");
    std::cout<<"PASS actual detour: exact welcome, native accepted transition, original-call passthrough, changed text, foreign owner/class, error/cancel/timer, stale state, bad memory, opt-out, repeated notice; only two event bytes changed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what();return 1;}}
