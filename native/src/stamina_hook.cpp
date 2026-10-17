#include "stamina_hook.h"
extern "C" { void* DS2FX_StaminaOriginal=nullptr; }
extern "C" float DS2FX_FilterStamina(void* controller,float cost,uint32_t reason) noexcept {
    using namespace fx::stamina_hook;
    ++calls;
    // Never run plugin code, allocate, log or mutate player memory in this path.
    // Preserve all non-positive/invalid costs and nonzero reason codes.
    uintptr_t base=0,player=0,owner=0,after=0;
    int32_t health=0;
    if (!controller || !fx::read(base_slot,base) || !base || !fx::read(base+0xd0,player) || !player ||
        !fx::read(reinterpret_cast<uintptr_t>(controller),owner) || owner!=player) return cost;
    ++local_calls;
    DWORD expected=0; const DWORD thread=GetCurrentThreadId();
    first_thread.compare_exchange_strong(expected,thread);
    if (first_thread!=thread) { multiple_threads=true; fault=true; }
    if (fault || !fx::read(player+0x168,health) || health<=0 || !fx::read(base+0xd0,after) || after!=player ||
        !fx::read(base_slot,after) || after!=base || !std::isfinite(cost) || cost<=0 || cost>1000000 || reason!=0) {
        ++rejected; return cost;
    }
    ++eligible;
    const auto value=percent.load();
    if (value==100) return cost;
    ++modified;
    return scaled(cost,value);
}
