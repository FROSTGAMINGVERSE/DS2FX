#include "../src/lantern_toggle_pipeline.h"
#include <iostream>
#include <stdexcept>
using namespace fx::lantern_toggle;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
NativeSample sample(){NativeSample s{};s.sampled_at_ms=100;s.lantern.available=s.lantern.alive=s.lantern.equipped=s.lantern.context_known=s.lantern.context_clear=true;s.lantern.menu_open=false;s.lantern.character_epoch=7;s.lantern.revision=9;s.lantern.oil_seconds=300;s.lantern.capacity_seconds=1800;s.lantern.visible=true;s.lantern.lit=true;return s;}
int main(){try{Pipeline p;auto s=sample();
    // Startup and continuous holds must never toggle without an observed release.
    for(int i=0;i<30;i++){s.lantern.interact_held=true;check(p.sample(s,.1).nonce==0,"startup hold issued command");}
    s.lantern.interact_held=false;p.sample(s,.1);
    Command command{};s.lantern.interact_held=true;for(int i=0;i<21;i++)if(auto c=p.sample(s,.1);c.nonce)command=c;
    check(command.nonce&&command.request.action==lantern::Action::stow,"two second hold did not issue stow");
    check(!p.acknowledge(command.nonce+1,s.lantern),"wrong acknowledgement was accepted");
    // A stowed empty lantern remains hidden until oil returns; it cannot ignite.
    p.invalidate();s=sample();s.lantern.visible=false;s.lantern.lit=false;s.lantern.oil_seconds=0;s.lantern.interact_held=false;p.sample(s,.1);p.sample(s,.1);s.lantern.interact_held=true;for(int i=0;i<25;i++)check(!p.sample(s,.1).nonce,"empty lantern ignited");
    p.invalidate();s=sample();s.lantern.visible=false;s.lantern.lit=false;s.lantern.interact_held=false;p.sample(s,.1);p.sample(s,.1);s.lantern.interact_held=true;for(int i=0;i<21;i++)if(auto c=p.sample(s,.1);c.nonce)command=c;
    check(command.request.action==lantern::Action::ignite,"stowed fueled lantern did not request ignite");
    auto applied=s.lantern;applied.visible=true;applied.lit=true;check(p.acknowledge(command.nonce,applied),"valid authoritative acknowledgement rejected");
    check(p.state().model_visible&&p.state().light_enabled&&!p.state().awaiting_ack,"acknowledged state not published");
    std::cout<<"PASS: DS2FX lantern toggle pipeline gates provider freshness, release, 2s hold, context, empty state, one-shot nonce and authoritative acknowledgement. No input/effect/model writes.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}