#include "../src/hud_replacement.h"
#include "../src/common.h"
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>

extern "C" void DS2FX_HudFixtureDraw();
extern "C" void DS2FX_HudFixtureMask();
extern "C" void DS2FX_HudFixtureTexture();
extern "C" void DS2FX_HudFixtureCall(void*,uintptr_t,void*);
extern "C" { uint64_t DS2FX_HudFixtureCounters[3]{}; }

namespace {
std::atomic<uint32_t> fixture_renewed{0};
struct alignas(16) Capture {
    uint64_t rcx{},rdx{},r8{},r9{},flags{},rax{},reserved{},padding{};
    alignas(16) std::array<unsigned char,512> fx{};
};
static_assert(offsetof(Capture,fx)==0x40);
template<size_t N,class T> void put(std::array<unsigned char,N>& data,size_t offset,T value) {
    std::memcpy(data.data()+offset,&value,sizeof(value));
}
struct Fixture {
    std::array<unsigned char,0x2400> base{};
    std::array<unsigned char,0x100> player{};
    std::array<std::array<unsigned char,0x2400>,8> chain{};
    std::array<unsigned char,0x100> sprite{};
    std::array<std::array<unsigned char,0x100>,13> row{};
    std::array<std::array<unsigned char,0x100>,13> resource{};
    std::array<std::array<unsigned char,0x800>,6> effect_chain{};
    std::array<unsigned char,0x100> effect_sprite{};
    std::array<std::array<unsigned char,0x100>,20> effect_rows{},effect_resources{},effect_leaves{};
    uintptr_t slot{};
    uintptr_t image{};
    uint64_t epoch{1};
    uintptr_t address(unsigned i) const {return reinterpret_cast<uintptr_t>(row[i].data());}
    Fixture() {
        image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        slot=reinterpret_cast<uintptr_t>(base.data());
        put(base,0xd0,reinterpret_cast<uintptr_t>(player.data()));
        constexpr uintptr_t offsets[]{0x22e0,0x10,0x10,8,0x28,0x30,0x38,0x38};
        constexpr uintptr_t tables[]{0x10c0768,0x10fa628,0x10fab70,0x11d6420,
            0x11d6408,0x11de158,0x11de318,0x11ddfa8};
        for(unsigned i=0;i<8;++i) {
            const auto target=reinterpret_cast<uintptr_t>(chain[i].data());
            if(i==0)put(base,offsets[i],target);
            else put(chain[i-1],offsets[i],target);
            put(chain[i],0,image+tables[i]);
        }
        put(chain[7],0x38,reinterpret_cast<uintptr_t>(sprite.data()));
        const auto header=reinterpret_cast<uintptr_t>(chain[2].data()+0x2e0);
        put(chain[2],0x2e0,header);
        put(chain[2],0x2e0+0x00,header); // Embedded header self pointer.
        put(chain[2],0x2e0+0x08,uint64_t{13});
        for(unsigned i=0;i<13;++i) {
            const auto target=address(i);
            const auto resource_address=reinterpret_cast<uintptr_t>(resource[i].data());
            put(chain[2],0x2e0+0x10+i*16,target);
            put(chain[2],0x2e0+0x1c+i*16,fx::hud_replacement::ids[i]);
            put(row[i],0,image+(i==10?0x11dea18:0x11ddfa8));
            put(row[i],0x18,reinterpret_cast<uintptr_t>(sprite.data()));
            if(i!=10) {
                put(row[i],0x48,resource_address);
                put(resource[i],0x1c,fx::hud_replacement::ids[i]);
            }
        }
        put(chain[2],0xd0,address(11));put(chain[2],0xd8,address(12));
        constexpr uintptr_t effect_offsets[]{0x38,8,0x28,0x30,0x38,0x38};
        constexpr uintptr_t effect_tables[]{0x10b1278,0x11d6420,0x11d6408,0x11de158,0x11de318,0x11ddfa8};
        for(unsigned i=0;i<6;++i) {
            const auto target=reinterpret_cast<uintptr_t>(effect_chain[i].data());
            if(!i)put(chain[1],effect_offsets[i],target);else put(effect_chain[i-1],effect_offsets[i],target);
            put(effect_chain[i],0,image+effect_tables[i]);
        }
        const auto effects_header=reinterpret_cast<uintptr_t>(effect_chain[0].data()+0x220);
        const auto effects_sprite=reinterpret_cast<uintptr_t>(effect_sprite.data());
        put(effect_chain[0],0xb8,uint32_t{20});put(effect_chain[0],0x220,effects_header);
        put(effect_chain[0],0x228,uint64_t{20});put(effect_chain[5],0x38,effects_sprite);
        put(effect_sprite,0,image+0x11de318);
        for(unsigned i=0;i<20;++i) {
            const auto row_address=reinterpret_cast<uintptr_t>(effect_rows[i].data());
            const auto effect_resource=reinterpret_cast<uintptr_t>(effect_resources[i].data());
            const auto leaf=reinterpret_cast<uintptr_t>(effect_leaves[i].data());
            put(effect_chain[0],0x230+i*16,row_address);put(effect_chain[0],0x23c+i*16,uint32_t{0x5f5c3e0+i});
            put(effect_rows[i],0,image+0x11ddfa8);put(effect_rows[i],0x18,effects_sprite);
            put(effect_rows[i],0x48,effect_resource);put(effect_resources[i],0x1c,uint32_t{0x5f5c3e0+i});
            put(effect_rows[i],0x38,leaf);put(effect_leaves[i],0,image+0x11dea18);put(effect_leaves[i],0x18,row_address);
        }
    }
};
Capture call(void* fn,uintptr_t object) {
    Capture capture{};
    DS2FX_HudFixtureCall(fn,object,&capture);
    return capture;
}
void same_input(const Capture& expected,const Capture& actual,uintptr_t object) {
    if(actual.rcx!=object||expected.rdx!=actual.rdx||expected.r9!=actual.r9||
       expected.rax!=actual.rax||expected.flags!=actual.flags)
        std::cerr<<std::hex<<"expected rcx="<<expected.rcx<<" rdx="<<expected.rdx
            <<" r9="<<expected.r9<<" rax="<<expected.rax<<" flags="<<expected.flags
            <<" actual rcx="<<actual.rcx<<" rdx="<<actual.rdx<<" r9="<<actual.r9
            <<" rax="<<actual.rax<<" flags="<<actual.flags<<std::dec<<'\n';
    fx::require(actual.rcx==object&&expected.rdx==actual.rdx&&
        expected.r8!=0&&actual.r8!=0&&expected.r9==actual.r9&&
        expected.rax==actual.rax&&expected.flags==actual.flags,
        "Original draw register/flag inputs changed");
    // R8 points at a distinct capture destination; compare the unaffected FX
    // control and XMM slots rather than the full FXSAVE area with timestamps.
    fx::require(std::memcmp(expected.fx.data(),actual.fx.data(),8)==0 &&
        std::memcmp(expected.fx.data()+24,actual.fx.data()+24,8)==0 &&
        std::memcmp(expected.fx.data()+160,actual.fx.data()+160,32)==0,
        "Original draw floating-point inputs changed");
}
}

int main() { try {
    Fixture fixture;
    void* targets[]{reinterpret_cast<void*>(&DS2FX_HudFixtureDraw),
        reinterpret_cast<void*>(&DS2FX_HudFixtureMask),
        reinterpret_cast<void*>(&DS2FX_HudFixtureTexture)};
    std::array<Capture,3> baseline{};
    for(unsigned i=0;i<3;++i)baseline[i]=call(targets[i],fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==1&&DS2FX_HudFixtureCounters[1]==1&&
        DS2FX_HudFixtureCounters[2]==1,"Baseline draw bodies did not run");
    fx::require(fx::hud_replacement::install_fixture(reinterpret_cast<uintptr_t>(&fixture.slot),
        fixture.image,targets),"Owned draw fixtures did not hook");
    // No renderer lease: every native body must remain callable.
    for(unsigned i=0;i<3;++i)same_input(baseline[i],call(targets[i],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==2&&DS2FX_HudFixtureCounters[1]==2&&
        DS2FX_HudFixtureCounters[2]==2,"Pass-through lost original side effects");
    fx::hud_replacement::observe(reinterpret_cast<uintptr_t>(fixture.player.data()),fixture.epoch,true);
    fx::require(fx::hud_replacement::present(fixture.epoch,1)==1,"Renderer lease rejected");
    for(unsigned i=0;i<3;++i)call(targets[i],fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==2&&DS2FX_HudFixtureCounters[1]==2&&
        DS2FX_HudFixtureCounters[2]==2,"Candidate native draw was not hidden");
    const auto draw_start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<32;++i)call(targets[0],fixture.address(0));
    const auto draw_micros=std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now()-draw_start).count();
    fx::require(DS2FX_HudFixtureCounters[0]==2,"Repeated candidate draw escaped suppression");
    // Preserve the separately owned covenant groups and foreign UI objects.
    for(unsigned i=0;i<3;++i) {
        same_input(baseline[i],call(targets[i],fixture.address(11)),fixture.address(11));
        same_input(baseline[i],call(targets[i],reinterpret_cast<uintptr_t>(fixture.sprite.data())),reinterpret_cast<uintptr_t>(fixture.sprite.data()));
    }
    fx::require(DS2FX_HudFixtureCounters[0]==4&&DS2FX_HudFixtureCounters[1]==4&&
        DS2FX_HudFixtureCounters[2]==4,"Covenant or foreign UI was suppressed");
    std::thread foreign([&] {
        call(targets[0],fixture.address(0));
        same_input(baseline[0],call(targets[0],reinterpret_cast<uintptr_t>(fixture.sprite.data())),
            reinterpret_cast<uintptr_t>(fixture.sprite.data()));
    });
    foreign.join();
    fx::require(DS2FX_HudFixtureCounters[0]==5&&DS2FX_HudFixtureCounters[1]==4&&
        DS2FX_HudFixtureCounters[2]==4,"Owned cross-thread draw or foreign object handling failed");
    put(fixture.base,0xd0,uintptr_t{0});
    same_input(baseline[0],call(targets[0],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==6,"Stale canonical player hid native drawing");
    put(fixture.base,0xd0,reinterpret_cast<uintptr_t>(fixture.player.data()));
    put(fixture.chain[2],0x2e0+0x1c,uint32_t{0xf6964});
    same_input(baseline[0],call(targets[0],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==7,"Stale health row hid native drawing");
    put(fixture.chain[2],0x2e0+0x1c,fx::hud_replacement::ids[0]);
    fx::hud_replacement::present(fixture.epoch,0);
    for(unsigned i=0;i<3;++i)same_input(baseline[i],call(targets[i],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==8&&DS2FX_HudFixtureCounters[1]==5&&
        DS2FX_HudFixtureCounters[2]==5,"Explicit lease revoke failed");
    fx::require(fx::hud_replacement::present(fixture.epoch+1,1)==0,
        "Stale renderer epoch obtained a lease");
    fx::hud_replacement::present(fixture.epoch,1);
    std::this_thread::sleep_for(std::chrono::milliseconds(375));
    for(unsigned i=0;i<3;++i)same_input(baseline[i],call(targets[i],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==9&&DS2FX_HudFixtureCounters[1]==6&&
        DS2FX_HudFixtureCounters[2]==6,"Expired lease hid native draw");
    fx::hud_replacement::observe(reinterpret_cast<uintptr_t>(fixture.player.data()),fixture.epoch,false);
    fx::require(fx::hud_replacement::present(fixture.epoch,1)==0,
        "Unplayable observation retained a renderer lease");
    fx::hud_replacement::observe(reinterpret_cast<uintptr_t>(fixture.player.data()),fixture.epoch,true);
    fx::require(fx::hud_replacement::present(fixture.epoch,1)==1,"Renderer lease not restored");
    const auto before_refresh=DS2FX_HudFixtureCounters[0];
    fx::hud_replacement::set_fixture_after_serial(+[] {
        fixture_renewed.store(fx::hud_replacement::present(1,1));
    });
    call(targets[0],fixture.address(0));
    fx::require(fixture_renewed.load()==1 && DS2FX_HudFixtureCounters[0]==before_refresh,
        "Concurrent healthy refresh exposed a native draw");
    fx::hud_replacement::set_fixture_after_serial(+[] {
        fx::hud_replacement::present(0,0);
    });
    same_input(baseline[0],call(targets[0],fixture.address(0)),fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==before_refresh+1,
        "Concurrent lease revocation hid a native draw");
    DS2FX_HudIcons icon_state{sizeof(icon_state),6,100,fixture.epoch};
    fx::hud_replacement::observe_icons(icon_state);
    fx::require(fx::hud_replacement::present_regions(fixture.epoch,100,7)==1,"Independent region grant failed");
    const auto before_icons=DS2FX_HudFixtureCounters[0];
    const auto effect_row=reinterpret_cast<uintptr_t>(fixture.effect_rows[0].data());
    const auto effect_leaf=reinterpret_cast<uintptr_t>(fixture.effect_leaves[0].data());
    const auto before_textures=DS2FX_HudFixtureCounters[2];
    call(targets[2],effect_leaf);
    fx::require(DS2FX_HudFixtureCounters[2]==before_textures,"Native effect texture escaped lease");
    call(targets[0],fixture.address(11));call(targets[0],fixture.address(12));call(targets[0],effect_row);
    fx::require(DS2FX_HudFixtureCounters[0]==before_icons,"Original covenant/effect draws escaped region lease");
    fx::require(fx::hud_replacement::present_regions(fixture.epoch,100,3)==1,"Effect-only revoke failed");
    call(targets[2],effect_leaf);
    fx::require(DS2FX_HudFixtureCounters[2]==before_textures+1,"Effect texture was not restored after revoke");
    call(targets[0],fixture.address(0));call(targets[0],fixture.address(11));call(targets[0],effect_row);
    fx::require(DS2FX_HudFixtureCounters[0]==before_icons+1,"Missing effect renderer disrupted bars or covenant");
    fx::require(!fx::hud_replacement::present_regions(fixture.epoch,97,7),"Stale icon snapshot obtained lease");
    fx::require(!fx::hud_replacement::should_hide(fixture.address(11)) && !fx::hud_replacement::should_hide(effect_row),"Stale icon grant kept suppression");
    fx::require(fx::hud_replacement::present_regions(fixture.epoch,100,7)==1,"Icon lease did not recover");
    icon_state.sample=101;icon_state.available_regions=2;
    const auto bar_count=DS2FX_HudFixtureCounters[0];
    fx::hud_replacement::set_fixture_after_serial(+[] {
        DS2FX_HudIcons changed{sizeof(changed),2,101,1};
        fx::hud_replacement::observe_icons(changed);
    });
    call(targets[0],fixture.address(0));
    fx::require(DS2FX_HudFixtureCounters[0]==bar_count,"Effect observation race exposed healthy native bars");
    fx::require(fx::hud_replacement::present_region_mask(fixture.epoch,100,7)==3,
        "Partial effects rejection did not retain bars and covenant grant");
    fx::require(fx::hud_replacement::should_hide(fixture.address(0)) &&
        fx::hud_replacement::should_hide(fixture.address(11)) && !fx::hud_replacement::should_hide(effect_leaf),
        "Partial region fallback disrupted healthy regions or suppressed unavailable effects");
    icon_state.available_regions=6;fx::hud_replacement::observe_icons(icon_state);
    fx::require(fx::hud_replacement::present_region_mask(fixture.epoch,101,7)==7,"Full region mask did not recover");
    put(fixture.effect_rows[0],0x18,uintptr_t{0});
    fx::require(!fx::hud_replacement::should_hide(effect_row) && !fx::hud_replacement::should_hide(effect_leaf) && fx::hud_replacement::should_hide(fixture.address(0)),"Foreign status ownership hid native effect or broke bars");
    fx::hud_replacement::present(0,0);
    fx::require(!fx::hud_replacement::should_hide(fixture.address(11)),"Global revoke kept covenant hidden");
    std::cout<<"PASS owned HUD detours: original draw ABI/FX and side effects, candidate-only hiding on multiple threads, covenant/foreign pass-through, owner/row rejection, renewal/revocation races, expiry; 32 hidden fixture draws="<<draw_micros<<"us\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;} }
