#include "../src/lantern_charge_router.h"
#include "../src/common.h"
#include <iostream>

int main() {
    try {
        using namespace fx::custom_item_resource;
        using fx::lantern_charge_router::Router;
        constexpr uint64_t owner = 0xA11CE, foreign_owner = 0xB0B;
        Router router;
        Snapshot state{};

        fx::require(!router.on_authenticated_bonfire(owner), "Router refuses bonfire before setup");
        fx::require(router.prepare(2) && !router.prepare(2), "Router defines one immutable Lantern resource");
        fx::require(!router.on_authenticated_bonfire(owner), "Bonfire cannot create an unowned Lantern");
        fx::require(router.on_authenticated_toggle(owner) == UseResult::activated &&
                    router.snapshot(owner, state) && state.current == 1 && state.active,
                    "Authenticated first use binds and spends one charge");
        fx::require(router.on_authenticated_toggle(owner) == UseResult::stowed &&
                    router.snapshot(owner, state) && state.current == 1 && !state.active,
                    "Stow remains free and reusable");
        fx::require(!router.on_authenticated_bonfire(foreign_owner), "Foreign bonfire cannot touch Lantern state");
        fx::require(router.on_authenticated_bonfire(owner) && router.snapshot(owner, state) &&
                    state.current == 2 && !state.active,
                    "Authenticated owner bonfire restores capacity and stows Lantern");
        fx::require(router.on_authenticated_toggle(owner) == UseResult::activated &&
                    router.on_authenticated_toggle(owner) == UseResult::stowed &&
                    router.on_authenticated_toggle(owner) == UseResult::activated &&
                    router.on_authenticated_toggle(owner) == UseResult::stowed &&
                    router.on_authenticated_toggle(owner) == UseResult::empty,
                    "Empty Lantern rejects activation without changing another resource");
        const auto status = router.status();
        fx::require(status.toggle_receipts == 7 && status.bonfire_receipts == 1 &&
                    status.rejected_receipts == 3 && status.refills == 1,
                    "Router reports authenticated and rejected routes separately");
        std::cout << "PASS lantern charge router: authenticated toggle, owner-scoped bonfire refill, reusable stow, and empty state\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
