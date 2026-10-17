#include "../src/custom_item_resource.h"
#include "../src/common.h"
#include <iostream>

int main() {
    try {
        using namespace fx::custom_item_resource;
        constexpr uint32_t lantern = 60425001, one_shot = 60425002;
        constexpr uint64_t first_owner = 0x101, second_owner = 0x202;
        Ledger ledger;
        Snapshot state{};

        fx::require(!ledger.define({0, 3, true}) && !ledger.define({lantern, 0, true}) &&
                    !ledger.define({lantern, 100, true}), "Invalid definitions rejected");
        fx::require(ledger.define({lantern, 3, true}) && ledger.define({lantern, 3, true}) &&
                    !ledger.define({lantern, 4, true}), "Definitions are immutable after registration");
        fx::require(ledger.define({one_shot, 2, false}), "Independent resource registers");
        fx::require(ledger.use(first_owner, lantern) == UseResult::unavailable &&
                    !ledger.snapshot(first_owner, lantern, state), "No implicit ownership from an item id");

        fx::require(ledger.bind_owned(first_owner, lantern) && ledger.snapshot(first_owner, lantern, state) &&
                    state.available && state.current == 3 && state.capacity == 3 && !state.active && state.revision == 1,
                    "Authenticated binding starts a full, inactive resource");
        fx::require(ledger.use(first_owner, lantern) == UseResult::activated && ledger.snapshot(first_owner, lantern, state) &&
                    state.current == 2 && state.active && state.revision == 2, "Activation spends exactly one charge");
        fx::require(ledger.use(first_owner, lantern) == UseResult::stowed && ledger.snapshot(first_owner, lantern, state) &&
                    state.current == 2 && !state.active && state.revision == 3, "Stowing is reusable and free");
        fx::require(ledger.use(first_owner, lantern) == UseResult::activated &&
                    ledger.use(first_owner, lantern) == UseResult::stowed &&
                    ledger.use(first_owner, lantern) == UseResult::activated &&
                    ledger.use(first_owner, lantern) == UseResult::stowed &&
                    ledger.snapshot(first_owner, lantern, state) && state.current == 0 && !state.active,
                    "Repeated activations deplete only the custom resource");
        fx::require(ledger.use(first_owner, lantern) == UseResult::empty && ledger.snapshot(first_owner, lantern, state) &&
                    state.current == 0 && !state.active, "Empty resource cannot light the item");

        fx::require(ledger.bind_owned(first_owner, one_shot) && ledger.use(first_owner, one_shot) == UseResult::activated &&
                    ledger.bind_owned(second_owner, lantern) && ledger.use(second_owner, lantern) == UseResult::activated,
                    "Owners and resources remain independent");
        fx::require(ledger.refill_at_bonfire(first_owner) == 1 && ledger.snapshot(first_owner, lantern, state) &&
                    state.current == 3 && !state.active, "Bonfire refill restores only opted-in resources and stows them");
        fx::require(ledger.snapshot(first_owner, one_shot, state) && state.current == 1 && state.active,
                    "Non-bonfire resource remains unchanged");
        fx::require(ledger.snapshot(second_owner, lantern, state) && state.current == 2 && state.active,
                    "Bonfire event is scoped to its authenticated owner");
        fx::require(ledger.forget_owner(first_owner) == 2 && !ledger.snapshot(first_owner, lantern, state) &&
                    ledger.snapshot(second_owner, lantern, state), "Owner teardown cannot leak resource state into another character");

        std::cout << "PASS custom item resource: immutable definition, authenticated owner binding, reusable toggle, empty state, bonfire refill, and owner isolation\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
