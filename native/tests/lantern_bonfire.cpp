#include "../src/lantern_bonfire_observer.h"
#include "../src/common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <cstring>
#include <iostream>

#pragma comment(linker, "/export:DS2FX_EffectFixtureTarget")
extern "C" {
__declspec(dllexport) void DS2FX_EffectFixtureTarget();
uint64_t DS2FX_EffectInvoke(void* target, uintptr_t control, const float* request);
alignas(16) unsigned char DS2FX_EffectObserved[352]{};
}

namespace {
alignas(16) std::array<unsigned char, 0x200> manager{}, control{}, request{};
alignas(16) std::array<unsigned char, 0x500> player{};
uintptr_t slot = 0;

template<class T, class A> void put(A& area, size_t offset, T value) { std::memcpy(area.data() + offset, &value, sizeof(value)); }

void initialize(int32_t effect) {
    const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    manager.fill(0);
    player.fill(0);
    control.fill(0);
    request.fill(0);
    slot = reinterpret_cast<uintptr_t>(manager.data());
    put(manager, 0xd0, reinterpret_cast<uintptr_t>(player.data()));
    put(player, 0x168, int32_t{100});
    put(player, 0x170, int32_t{200});
    put(player, 0x3e0, reinterpret_cast<uintptr_t>(control.data()));
    put(control, 0, image + 0x10bfee0);
    put(request, 0, effect);
    put(request, 4, int32_t{1});
}
}

int main() {
    try {
        namespace observer = fx::lantern_bonfire;
        const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        const auto target = reinterpret_cast<void*>(&DS2FX_EffectFixtureTarget);

        initialize(60425000);
        fx::require(!observer::install_owned_fixture(reinterpret_cast<uintptr_t>(&slot), image, target, false) &&
                    !observer::status().installed, "Default observer setup is a no-op");
        const auto baseline = DS2FX_EffectInvoke(target, reinterpret_cast<uintptr_t>(control.data()),
                                                  reinterpret_cast<const float*>(request.data()));
        std::array<unsigned char, 352> preserved{};
        std::memcpy(preserved.data(), DS2FX_EffectObserved, preserved.size());

        initialize(60425000);
        fx::require(observer::install_owned_fixture(reinterpret_cast<uintptr_t>(&slot), image, target, true),
                    "Owned bonfire observer installs");
        const auto unrelated = DS2FX_EffectInvoke(target, reinterpret_cast<uintptr_t>(control.data()),
                                                   reinterpret_cast<const float*>(request.data()));
        fx::require(unrelated == baseline, "Unrelated effect preserves the original return");
        fx::require(std::memcmp(preserved.data(), DS2FX_EffectObserved, preserved.size()) == 0,
                    "Unrelated effect preserves GPR/flags/stack/16XMM/x87/MXCSR state");
        fx::require(observer::status().calls == 1 && observer::status().bonfire_effects == 0,
                    "Unrelated effect is ignored by the bonfire observer");

        initialize(observer::bonfire_rest_effect);
        const auto observed = DS2FX_EffectInvoke(target, reinterpret_cast<uintptr_t>(control.data()),
                                                  reinterpret_cast<const float*>(request.data()));
        observer::Receipt receipt{};
        fx::require(observed == baseline && observer::snapshot(receipt) && receipt.sequence == 1 &&
                    receipt.owner_token == reinterpret_cast<uintptr_t>(player.data()) &&
                    receipt.effect_id == observer::bonfire_rest_effect && observer::status().accepted == 1,
                    "Canonical local BonfireRest special effect publishes a receipt");

        initialize(observer::bonfire_rest_effect);
        put(player, 0x3e0, reinterpret_cast<uintptr_t>(manager.data()));
        DS2FX_EffectInvoke(target, reinterpret_cast<uintptr_t>(control.data()), reinterpret_cast<const float*>(request.data()));
        fx::require(observer::status().rejected == 1, "Foreign or stale effect controller is rejected");
        observer::disable();
        fx::require(!observer::snapshot(receipt), "Disabled observer publishes no receipt");
        fx::require(MH_DisableHook(target) == MH_OK && MH_RemoveHook(target) == MH_OK, "Fixture cleanup");

        std::cout << "PASS bonfire observer: exact special effect, canonical local controller, receipt publication, no added writes, and register/FP preservation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
