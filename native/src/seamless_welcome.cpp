#include "seamless_welcome.h"
#include "common.h"
#include "seamless_welcome_contract.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#include <cstring>
namespace fx::seamless_welcome {
namespace {
using Initialize=void(*)(uintptr_t);
Initialize original=nullptr;
uintptr_t image=0,global_slot=0;
std::atomic<bool> active{false};
std::atomic<uint64_t> count{0};
constexpr wchar_t welcome[]=L"#c[FF5BCEFA][Announcement -- This is a free mod]#c\n"
    L"#c[FFF5A9B8]Welcome to Dark Souls II: Seamless Co-op!#c\n"
    L"#c[FFFFFFFF]If you have paid any money for this then you have been scammed!#c";
bool accept(uintptr_t event) noexcept {
    if(!active.load(std::memory_order_relaxed) || !event || (event&7))return false;
    struct Event {uintptr_t vtable;uint32_t flags;uint32_t id;int16_t next,cancel;float timeout,elapsed;uint32_t pad;uintptr_t ok,no;uint16_t state;uint8_t reserved[6];uintptr_t text;};
    static_assert(sizeof(Event)==0x40 && offsetof(Event,state)==0x30 && offsetof(Event,text)==0x38);
    Event before{},after{};
    uintptr_t global=0,verify=0,text=0;uint64_t length=0,capacity=0;
    // Native title error event 0x3f -> 0x2a; one OK button, no cancel/callback.
    // Its text must be the canonical title-message buffer, not an arbitrary UI.
    if(!read(event,before) || before.vtable!=image+0x10bcff8 || before.id!=0x3f ||
       before.next!=0x2a || before.cancel!=-1 || before.timeout!=0 || before.elapsed!=0 ||
       before.state!=0 || before.no!=0 || !read(global_slot,global) || !global ||
       !read(global+0x128,length) || length!=std::size(welcome)-1 ||
       !read(global+0x130,capacity) || capacity<length || capacity>511 ||
       !read(global+0x118,text) || before.text!=text)return false;
    std::array<wchar_t,std::size(welcome)> current{};
    if(!read(text,current) || std::memcmp(current.data(),welcome,sizeof(welcome))!=0 ||
       !read(global_slot,verify) || verify!=global || !read(event,after) ||
       std::memcmp(&before,&after,sizeof(before))!=0)return false;
    MEMORY_BASIC_INFORMATION memory{};
    if(!VirtualQuery(reinterpret_cast<void*>(event),&memory,sizeof(memory)) || memory.State!=MEM_COMMIT ||
       (memory.Protect&0xff)!=PAGE_READWRITE || (memory.Protect&PAGE_GUARD) ||
       event+sizeof(Event)>reinterpret_cast<uintptr_t>(memory.BaseAddress)+memory.RegionSize)return false;
    // Native modal update writes choice=1, then state=3 after accepted close.
    // Registered native transitions consume state=3 to reach the same next event.
    // We never create the modal, so there is no input capture or close to wait for.
    // The exact class's accepted callback is an authenticated no-op (ret 0).
    // This runs only inside its game-owned initializer, never on our observer.
    *reinterpret_cast<volatile uint16_t*>(event+0x30)=0x0103;
    count.fetch_add(1,std::memory_order_relaxed);
    return true;
}
void initialize(uintptr_t event) {if(!accept(event))original(event);}
bool setup(uintptr_t base,uintptr_t slot,void* target) {
    require(!original && base && slot && target,"Invalid Seamless welcome adapter setup");
    auto result=MH_Initialize();require(result==MH_OK || result==MH_ERROR_ALREADY_INITIALIZED,"Welcome hook backend unavailable");
    require(MH_CreateHook(target,initialize,reinterpret_cast<void**>(&original))==MH_OK,"Welcome initializer hook unavailable");
    image=base;global_slot=slot;active.store(true);
    if(MH_EnableHook(target)!=MH_OK){active.store(false);MH_RemoveHook(target);original=nullptr;require(false,"Welcome initializer hook failed");}
    return true;
}
template<size_t N> bool exact(uintptr_t at,const std::array<unsigned char,N>& expected) {
    std::array<unsigned char,N> bytes{};return read(at,bytes) && bytes==expected;
}
}
bool install(bool enabled) {
    if(!enabled)return false;
    const auto vendor=GetModuleHandleW(L"ds2sc.dll");if(!vendor)return false;
    const auto exe=module_path();
    require(hash_file(exe)==game_hash,"Unsupported welcome adapter executable");
    require(fs::equivalent(module_path(vendor),exe.parent_path()/L"SeamlessCoop"/L"ds2sc.dll") &&
        hash_file(module_path(vendor))=="5df6c51fae1c0a177090e39831402120a7913fe42c2625da32767de562d17a71",
        "Welcome presentation supports authenticated official Seamless 0.0.5 only");
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    require(exact(base+0x104db0,contract::initialize) && exact(base+0x105150,contract::update) &&
        exact(base+0x104f30,contract::transitions) && exact(base+0x1050a0,contract::cleanup) &&
        exact(base+0xf89d0,contract::accepted) && exact(base+0xf89b0,contract::text),
        "Welcome native dialog instructions changed; original notice retained");
    uintptr_t init=0,callback=0,get_text=0;
    require(read(base+0x10bcff8+8,init) && init==base+0x104db0 &&
        read(base+0x10bcff8+0x40,callback) && callback==base+0xf89d0 &&
        read(base+0x10bcff8+0x58,get_text) && get_text==base+0xf89b0,"Welcome native dialog class changed");
    return setup(base,base+0x160de10,reinterpret_cast<void*>(base+0x104db0));
}
uint64_t suppressed() noexcept {return count.load(std::memory_order_relaxed);}
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t base,uintptr_t slot,void* target){return setup(base,slot,target);}
void set_enabled_fixture(bool enabled) noexcept {active.store(enabled);}
#endif
}
