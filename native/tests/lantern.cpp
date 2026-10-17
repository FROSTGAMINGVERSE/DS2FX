#include "../mods/wayfarer_lantern/lantern_state.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace lantern;
void check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
Snapshot sample() {
    Snapshot s;
    s.available = s.alive = s.equipped = s.context_known = s.context_clear = true;
    s.menu_open = false; s.character_epoch = 1; s.revision = 10;
    s.oil_seconds = 300; s.capacity_seconds = 1800;
    return s;
}
Request hold(HoldController& c, Snapshot s, int steps = 20) {
    s.interact_held = true; Request result;
    for (int i=0; i<steps; ++i) { auto r=c.update(s,0.1); if(r.action!=Action::none) result=r; }
    return result;
}
int main() {
    try {
        auto s=sample(); HoldController c;
        check(hold(c,s,40).action == Action::none,"Held at startup must not toggle");
        c.update(s,0.1); // observed native release
        check(hold(c,s,19).action == Action::none,"Short hold must not toggle");
        auto r=hold(c,s,1); check(r.action==Action::stow && accepts(r,s),"Two-second hold must stow a visible lantern");
        check(hold(c,s,50).action==Action::none,"Continued hold must not repeat");
        auto changed=s; changed.revision++; check(!accepts(r,changed),"Changed resource revision must reject stale action");
        changed=s; changed.character_epoch++; check(!accepts(r,changed),"Old character action must be rejected");
        changed=s; changed.context_clear=false; check(!accepts(r,changed),"Context must be checked again at execution");
        for(int reason=0;reason<7;++reason) {
            HoldController gate; gate.update(s,0.1); gate.update(s,0.1); hold(gate,s,10);
            auto blocked=s; blocked.interact_held=true;
            switch(reason) {
                case 0:blocked.context_known=false;break; case 1:blocked.context_clear=false;break;
                case 2:blocked.menu_open=true;break; case 3:blocked.alive=false;break;
                case 4:blocked.equipped=false;break; case 5:blocked.available=false;break;
                case 6:blocked.refilling=true;break;
            }
            check(gate.update(blocked,0.1).action==Action::none && gate.fraction()==0,"Blocked context must cancel progress");
            check(hold(gate,s,30).action==Action::none,"Returning to safety while held requires release");
            gate.update(s,0.1); check(hold(gate,s).action==Action::stow,"Fresh safe hold must recover");
        }
        HoldController pause; pause.update(s,0.1); pause.update(s,0.1); hold(pause,s,10);
        auto held=s; held.interact_held=true; pause.update(held,1.0);
        check(pause.fraction()==0 && hold(pause,s).action==Action::none,"Long sample gap must cancel and require release");
        c.update(s,0.1); s.lit=true; check(hold(c,s).action==Action::stow,"Lit lantern must request stow");
        c.update(s,0.1); s.lit=false; s.oil_seconds=0;
        check(hold(c,s).action==Action::stow && status(s)==Status::empty,"Empty visible lantern must stow on hold");
        s.visible=false; c.update(s,0.1); check(hold(c,s).action==Action::none,"Empty stowed lantern must not ignite");
        s.oil_seconds=300; c.update(s,0.1); check(hold(c,s).action==Action::ignite && status(s)==Status::off,"Refilled stowed lantern must ignite on hold");
        s.lit=true; check(status(s)==Status::low_oil,"Low oil threshold");
        s.oil_seconds=451; check(status(s)==Status::lit,"Above warning threshold");
        s.refilling=true; check(status(s)==Status::refilling,"Canonical refill state");
        s.oil_seconds=std::numeric_limits<double>::quiet_NaN(); check(status(s)==Status::unavailable,"Invalid resource must not render usable");
        s=sample(); auto original=s.oil_seconds; for(int i=0;i<1000;++i)c.update(s,0.1);
        check(s.oil_seconds==original,"HUD/controller must never run an independent oil clock");
        std::cout << "PASS: hold timing, one-shot/release, context interruption, stale revision/epoch, pause gap, empty-visible stow/refill and authoritative-resource contract. Owned fixtures only; no DS2 engine provider.\n";
        return 0;
    } catch(const std::exception& e) {std::cerr << e.what() << '\n';return 1;}
}
