#include "../src/observation.h"
#include <limits>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        check(fx::valid_health(-602,1771) && fx::observed_health(-602)==0,"Observed lethal overkill remains a zero-HP player");
        check(fx::valid_health(0,1771) && fx::observed_health(0)==0,"Cliff death zero HP");
        check(fx::observed_health(1732)==1732,"Living HP unchanged");
        check(!fx::valid_health(1772,1771) && !fx::valid_health(-1000001,1771) && !fx::valid_health(0,0),"Corrupt HP bounds refused");
        check(fx::valid_stamina(50.25f,100),"Fractional stamina");
        check(fx::valid_stamina(-10,100),"Exhaustion can be negative");
        check(!fx::valid_stamina(101,100) && !fx::valid_stamina(0,0),"Invalid ranges");
        check(!fx::valid_stamina(std::numeric_limits<float>::quiet_NaN(),100),"NaN refused");
        check(!fx::valid_stamina(5,std::numeric_limits<float>::infinity()),"Infinity refused");
        DS2FX_PlayerState old{sizeof(DS2FX_PlayerState),1,1,100,120,1,1,90,100,0}, next=old;
        std::vector<uint32_t> events;
        auto emit=[&](uint32_t e,const DS2FX_PlayerState&) { events.push_back(e); };
        next.health=fx::observed_health(-602); next.stamina=0;
        fx::transitions(old,next,emit);
        check(events == std::vector<uint32_t>{DS2FX_HEALTH_CHANGED,DS2FX_PLAYER_DIED,DS2FX_STAMINA_CHANGED,DS2FX_TICK},"Simultaneous events must all be emitted in order");
        events.clear(); std::swap(old,next);
        fx::transitions(old,next,emit);
        check(events[1]==DS2FX_PLAYER_REVIVED,"Zero to positive transition");
        events.clear(); next.character_epoch=2;
        fx::transitions(old,next,emit);
        check(events==std::vector<uint32_t>{DS2FX_PLAYER_UNAVAILABLE,DS2FX_PLAYER_AVAILABLE,DS2FX_TICK},"Replacement must not generate false healing/death");
        events.clear(); next.available=0;
        fx::transitions(old,next,emit);
        check(events==std::vector<uint32_t>{DS2FX_PLAYER_UNAVAILABLE,DS2FX_TICK},"Unload event");
        static_assert(sizeof(DS2FX_PlayerState)==48);
        std::cout << "PASS stamina bounds/NaN, simultaneous events, death/revive, replacement/unload, ABI2 layout\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what(); return 1; }
}
