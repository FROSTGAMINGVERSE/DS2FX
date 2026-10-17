#include "lantern_interact_observer.h"
#include "common.h"
#include <atomic>
namespace fx::lantern_interact {
namespace { std::atomic<bool> enabled{false}; uintptr_t slot=0,image=0; bool ptr(uintptr_t p,uintptr_t o,uintptr_t& v) noexcept{return p&&fx::read(p+o,v)&&v>=0x10000&&v<=0x00007fffffffffffULL&&!(v&7);} }
bool prepare(uintptr_t base_slot,bool opt) {
    if(!opt) return false;
#ifdef DS2FX_FIXTURE
    (void)base_slot;
    return false;
#else
    const auto exe=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==exe+0x16148f0,"Unsupported executable for Interact observer");
    slot=base_slot;image=exe;enabled.store(true);return true;
#endif
}
bool read(Sample& out) noexcept {
    out={};if(!enabled.load())return false;uintptr_t manager=0,player=0,wrapper=0,pad=0,logical=0;uint64_t held=0;uint8_t blocked=0;uint32_t frozen=0;
    if(!fx::read(slot,manager)||!ptr(manager,0xd0,player)||!ptr(player,0xe8,wrapper)||!ptr(wrapper,0x18,pad)||!ptr(pad,0x20,logical))return false;
    uintptr_t v=0;if(!fx::read(wrapper,v)||v!=image+0x10cabe8||!fx::read(pad,v)||v!=image+0x10e3f18||!fx::read(logical,v)||v!=image+0x10e3ae0||!fx::read(logical+0x12c,held))return false;
    uintptr_t chr=0;if(ptr(player,0xc8,chr)){fx::read(chr+0xc5,blocked);fx::read(chr+0x100,frozen);} // converter suppression only; not a context approval.
    out={true,(held&(1ull<<14))!=0,blocked!=0||frozen!=0,player,held};return true;
}
void disable() noexcept{enabled.store(false);slot=image=0;}
#ifdef DS2FX_FIXTURE
bool prepare_fixture(uintptr_t base_slot,uintptr_t exe,bool opt){if(!opt||!base_slot||!exe)return false;slot=base_slot;image=exe;enabled.store(true);return true;}
#endif
}
