#include "../src/player_states.h"
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace fx::player_states;
void check(bool yes,const char* message) { if(!yes)throw std::runtime_error(message); }
struct Fixture : Executor {
    bool available=true, reject=false, complete=false;
    unsigned starts=0,polls=0;
    uint32_t selected=0;
    bool ready() const noexcept override { return available; }
    Receipt start(const DS2FX_StateDefinition& d,uint32_t,const Context&) noexcept override {
        ++starts;selected=d.motion;return reject?Receipt::rejected:Receipt::running;
    }
    Receipt poll(uint32_t,const Context&) noexcept override {
        ++polls;return complete?Receipt::completed:Receipt::running;
    }
};
struct NativeFixture : NativeAdapter {
    Fixture executor;
    Context context{44,1000,true,true,true,true,true,true,true,false,false,false};
    bool ready() const noexcept override { return executor.ready(); }
    bool sample(uintptr_t manager,Context& output) noexcept override {
        if(manager!=123)return false;output=context;return true;
    }
    Receipt start(const DS2FX_StateDefinition& d,uint32_t h,const Context& c) noexcept override {
        return executor.start(d,h,c);
    }
    Receipt poll(uint32_t h,const Context& c) noexcept override { return executor.poll(h,c); }
};
struct Setup {
    Service s;Fixture executor;uint32_t idle,moving,sprint;
    Context c{7,1000,true,true,true,true,true,true,true,false,false,false};
    Setup(bool bind=true) {
        idle=s.add(11,{sizeof(DS2FX_StateDefinition),1,DS2FX_STATE_VERTICAL_JUMP,948,2000,0});
        moving=s.add(11,{sizeof(DS2FX_StateDefinition),2,DS2FX_STATE_FORWARD_JUMP,947,2000,0});
        sprint=s.add(11,{sizeof(DS2FX_StateDefinition),3,DS2FX_STATE_NATIVE_PASS,0,2000,0});
        check(idle && moving && sprint,"state registration");
        check(s.routes(11,{sizeof(DS2FX_JumpRoutes),idle,moving,sprint})==DS2FX_STATE_OK,"routes");
        s.commit(11);if(bind)s.bind(&executor);s.observe(7,true);s.tick(c);
    }
    void press(bool moving_now=false,bool sprinting=false) {
        c.now+=16;c.moving=moving_now;c.sprinting=sprinting;c.jump_held=true;s.tick(c);
    }
    DS2FX_StateRequest request(uint32_t handle,uint64_t epoch=7) {
        return {sizeof(DS2FX_StateRequest),handle,epoch,100,0};
    }
};
int main() {
    try {
        { Setup t(false);
          check(t.s.status(11).registered_states==3 && !t.s.status(11).executor_available,"honest unavailable capability");
          check(t.s.request(11,t.request(t.idle),1000)==DS2FX_STATE_EXECUTOR_UNAVAILABLE,"never queue without native executor");
          check(t.s.request(22,t.request(t.idle),1000)==DS2FX_STATE_INVALID,"foreign handle rejected");
          check(t.s.request(11,t.request(t.idle,8),1000)==DS2FX_STATE_STALE,"stale epoch rejected");
          check(t.s.request(11,t.request(t.sprint),1000)==DS2FX_STATE_NATIVE_PASSTHROUGH,"original sprint works without adapter");
          check(!t.s.add(11,{sizeof(DS2FX_StateDefinition),4,2,948,2000,0}),"initialization only"); }
        { Setup t;t.press();check(t.executor.starts==1 && t.executor.selected==2,"idle vertical jump");
          t.c.moving=true;for(int i=0;i<10;++i){t.c.now+=16;t.s.tick(t.c);}
          check(t.executor.starts==1 && t.executor.selected==2,"hold neither repeats nor switches animation mid-flight");
          t.executor.complete=true;t.c.now+=16;t.s.tick(t.c);
          check(t.s.status(11).phase==DS2FX_STATE_COMPLETED,"only native completion receipt completes state");
          t.c.now+=16;t.s.tick(t.c);check(t.executor.starts==1,"holding after landing cannot repeat");
          t.c.jump_held=false;t.s.tick(t.c);t.executor.complete=false;t.press(true);
          check(t.executor.starts==2 && t.executor.selected==3,"release then walk uses forward jump"); }
        { Setup t;t.press(true,true);
          check(t.executor.starts==0 && t.s.status(11).result==DS2FX_STATE_NATIVE_PASSTHROUGH,"sprint preserves original action"); }
        { Setup t;auto r=t.request(t.idle);
          check(t.s.request(11,r,1000)==DS2FX_STATE_OK,"queue accepted");
          check(t.s.status(11).phase==DS2FX_STATE_QUEUED && !t.executor.starts,"worker cannot execute");
          check(t.s.request(11,r,1001)==DS2FX_STATE_BUSY,"concurrent action rejected");
          t.c.game_thread=false;t.s.tick(t.c);check(!t.executor.starts,"foreign thread cannot execute");
          t.c.game_thread=true;t.c.canonical=false;t.s.tick(t.c);check(!t.executor.starts,"foreign player cannot execute");
          t.c.canonical=true;t.c.now=1100;t.s.tick(t.c);
          check(!t.executor.starts && t.s.status(11).result==DS2FX_STATE_EXPIRED,"expired requests cannot execute"); }
        for(int gap=0;gap<5;++gap) { Setup t;t.s.request(11,t.request(t.idle),1000);
          if(gap==0)t.c.grounded=false;if(gap==1)t.c.playable=false;
          if(gap==2)t.c.context_known=false;if(gap==3)t.c.alive=false;if(gap==4)t.c.epoch=8;
          t.s.tick(t.c);check(!t.executor.starts && t.s.status(11).phase==DS2FX_STATE_ABORTED,"air/menu/ladder/death/character gap rejected"); }
        { Setup t;t.executor.reject=true;t.press();
          check(t.s.status(11).phase==DS2FX_STATE_ABORTED && t.s.status(11).result==DS2FX_STATE_EXECUTION_REJECTED,"native refusal never becomes active"); }
        { Setup t;t.press();t.s.observe(8,false);
          check(t.s.status(11).phase==DS2FX_STATE_ABORTED,"character change revokes active state");
          t.s.observe(8,true);t.c.epoch=8;t.s.tick(t.c);check(t.executor.starts==1,"held input cannot retrigger after travel");
          t.s.revoke_all();check(!t.s.status(11).registered_states && !t.s.status(11).executor_available,"stop/fault revokes owners"); }
        { Setup t;t.c.input_known=false;t.press();check(!t.executor.starts,"unknown input cannot trigger");
          t.c.input_known=true;t.s.tick(t.c);check(!t.executor.starts,"known-held after gap requires release"); }
        { Setup t;t.press();t.c.now+=2000;t.s.tick(t.c);
          check(t.s.status(11).phase==DS2FX_STATE_ABORTED,"missing completion receipt cannot retain active state indefinitely"); }
        { Setup t;uint32_t a=0,b=0;
          std::thread first([&]{a=t.s.request(11,t.request(t.idle),1000);});
          std::thread second([&]{b=t.s.request(11,t.request(t.moving),1000);});
          first.join();second.join();check((a==DS2FX_STATE_OK && b==DS2FX_STATE_BUSY) ||
              (b==DS2FX_STATE_OK && a==DS2FX_STATE_BUSY),"concurrent requests retain one canonical action"); }
        { Service s;check(!s.add(1,{sizeof(DS2FX_StateDefinition),1,99,947,2000,0}),"unknown motion rejected");
          check(!s.add(1,{sizeof(DS2FX_StateDefinition),1,2,0,2000,0}),"missing animation rejected");
          auto h=s.add(1,{sizeof(DS2FX_StateDefinition),1,2,948,2000,0});check(h,"definition");
          check(!s.add(1,{sizeof(DS2FX_StateDefinition),1,2,948,2000,0}),"duplicate local ID rejected");
          check(s.add(2,{sizeof(DS2FX_StateDefinition),1,2,948,2000,0})!=h,"owner namespaces separate");
          check(s.routes(2,{sizeof(DS2FX_JumpRoutes),h,h,h})==DS2FX_STATE_INVALID,"cross-owner routes rejected"); }
        { Service s;
          for(uint32_t i=1;i<=32;++i)check(s.add(1,{sizeof(DS2FX_StateDefinition),i,1,947,1000,0}),"fixed registry capacity");
          check(!s.add(1,{sizeof(DS2FX_StateDefinition),33,1,947,1000,0}),"bounded capacity rejection"); }
        { NativeFixture adapter;auto& s=service();
          const auto handle=s.add(33,{sizeof(DS2FX_StateDefinition),1,1,947,1000,0});s.commit(33);
          bind_native_adapter(&adapter);s.observe(44,true);
          check(s.request(33,{sizeof(DS2FX_StateRequest),handle,44,100,0},1000)==DS2FX_STATE_OK,"bound Core adapter queue");
          native_tick(999);check(!adapter.executor.starts,"adapter rejects foreign manager");
          native_tick(123);check(adapter.executor.starts==1 && s.status(33).phase==DS2FX_STATE_ACTIVE,"native seam consumes queue through Core adapter");
          adapter.executor.complete=true;adapter.context.now+=16;native_tick(123);
          check(s.status(33).phase==DS2FX_STATE_COMPLETED,"native seam completion receipt");
          bind_native_adapter(nullptr);s.revoke_all(); }
        std::cout<<"PASS owned fixture: state ownership, ABI, idle/walk/sprint selection, edge input, expiry, receipts and lifecycle. No live-game or multiplayer proof.\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
