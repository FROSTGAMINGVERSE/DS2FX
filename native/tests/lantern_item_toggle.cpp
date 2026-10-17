#include "../src/lantern_item_toggle.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <iostream>
#include <thread>

namespace {
alignas(16) std::array<unsigned char, 0x200> manager{}, player{}, owner{}, action{}, params{}, flags{};
uintptr_t slot = 0;
template<class T, class A> void put(A& area, size_t offset, T value) { std::memcpy(area.data() + offset, &value, sizeof(value)); }

void initialize() {
    const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    manager.fill(0); player.fill(0); owner.fill(0); action.fill(0); params.fill(0); flags.fill(0);
    slot = reinterpret_cast<uintptr_t>(manager.data());
    put(manager, 0xd0, reinterpret_cast<uintptr_t>(player.data()));
    put(player, 0xb8, reinterpret_cast<uintptr_t>(flags.data()));
    put(player, 0x168, int32_t{100}); put(player, 0x170, int32_t{200});
    put(owner, 0, image + 0x10af2b8); put(owner, 8, reinterpret_cast<uintptr_t>(player.data()));
    put(action, 0, image + 0x10ccbd8); put(action, 0x48, reinterpret_cast<uintptr_t>(owner.data()));
    put(action, 0x40, reinterpret_cast<uintptr_t>(params.data() + 4));
    put(params, 4, int32_t{19}); put(params, 8, float{0}); put(params, 12, int32_t{51});
    put(action, 0x10, int32_t{60425001}); put(action, 0x14, int32_t{1000}); put(action, 0x18, int32_t{9}); put(action, 0x1c, int32_t{1});
}
}

int main() {
    try {
        namespace toggle = fx::lantern_item_toggle;
        initialize();
        const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        toggle::Receipt receipt{};
        toggle::prepare(reinterpret_cast<uintptr_t>(&slot), true);
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Use requires a fresh native tick");
        toggle::on_engine_tick(slot);
        put(action,0x10,int32_t{60425000});toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Oil metadata cannot toggle");put(action,0x10,int32_t{60425001});
        std::thread worker([&]{toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);});worker.join();
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Worker receipt rejected");
        Sleep(280);toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Expired tick receipt rejected");toggle::on_engine_tick(slot);
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(toggle::snapshot(receipt) && receipt.sequence == 1 &&
                    receipt.owner_token == reinterpret_cast<uintptr_t>(player.data()) &&
                    receipt.metadata_10 == 60425001 && receipt.metadata_14 == 1000 &&
                    receipt.metadata_18 == 9 && receipt.metadata_1c == 1,
                    "Authenticated signature publishes its owner-bound metadata receipt");
        fx::require(toggle::active_for(reinterpret_cast<uintptr_t>(player.data())), "First receipt toggles active");
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(toggle::snapshot(receipt) && receipt.sequence == 2 &&
                    !toggle::active_for(reinterpret_cast<uintptr_t>(player.data())), "Second receipt stows without losing metadata");
        put(params, 8, float{0.01f});
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()), image);
        fx::require(toggle::status().rejected == 5 && toggle::snapshot(receipt) && receipt.sequence == 2,
                    "Near-match action cannot forge a new Lantern receipt");
        put(params,8,float{0});toggle::on_engine_tick(slot);toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
        fx::require(toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Activate before load boundary");
        slot=0;toggle::on_engine_tick(0);slot=reinterpret_cast<uintptr_t>(manager.data());toggle::on_engine_tick(slot);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Pointer reuse after unavailable manager starts stowed");
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
        put(manager,0xa8,reinterpret_cast<uintptr_t>(flags.data()));toggle::on_engine_tick(slot);
        fx::require(toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"World-data replacement preserves same live controller intent");
        Sleep(520);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Paused native updates cannot show stale Lantern");
        toggle::on_engine_tick(slot);fx::require(toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Fresh same-owner tick resumes after loading gap");
        put(player,0xb8,reinterpret_cast<uintptr_t>(params.data()));toggle::on_engine_tick(slot);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Changed character controller cannot inherit intent");
        put(player,0xb8,reinterpret_cast<uintptr_t>(flags.data()));toggle::on_engine_tick(slot);
        toggle::observe_active_item(reinterpret_cast<uintptr_t>(action.data()),image);
        put(player,0x168,int32_t{0});toggle::on_engine_tick(slot);
        put(player,0x168,int32_t{100});toggle::on_engine_tick(slot);
        fx::require(!toggle::active_for(reinterpret_cast<uintptr_t>(player.data())),"Death/revive remains stowed");
        toggle::disable();
        fx::require(!toggle::snapshot(receipt), "Disabled toggle publishes no receipt");
        std::cout << "PASS lantern item toggle: canonical owner, exact signal, metadata receipt, free stow, and disabled boundary\n";
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
