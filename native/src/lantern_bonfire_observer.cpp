#include "lantern_bonfire_observer.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <array>
#include <atomic>
#ifdef DS2FX_LANTERN_RESEARCH
#include "lantern_runtime.h"
#endif

extern "C" { void* DS2FX_LanternBonfireOriginal = nullptr; }

namespace fx::lantern_bonfire {
namespace {
std::atomic<bool> installed{false}, observing{false};
std::atomic<uint64_t> calls{0}, bonfire_effects{0}, accepted{0}, rejected{0}, sequence{0};
std::atomic<DWORD> first_thread{0};
std::atomic_flag publishing = ATOMIC_FLAG_INIT;
Receipt current{};
uintptr_t slot = 0, image_base = 0;

struct RequestPrefix { int32_t effect_id; int32_t quantity; };
static_assert(sizeof(RequestPrefix) == 8);

bool pointer(uintptr_t base, uintptr_t offset, uintptr_t& output) noexcept {
    output = 0;
    return base && offset <= UINTPTR_MAX - base && fx::read(base + offset, output) &&
           output >= 0x10000 && output <= 0x00007fffffffffffULL && !(output & 7);
}

bool native_entries_match(uintptr_t image) noexcept {
    constexpr std::array<unsigned char, 5> entry{0xe9, 0x1c, 0x0d, 0x9f, 0x01};
    constexpr std::array<unsigned char, 30> thunk{
        0x48, 0x89, 0x6c, 0x24, 0xf8, 0x48, 0x8d, 0x64, 0x24, 0xf8,
        0x48, 0x8d, 0x2d, 0x33, 0xa7, 0x0a, 0x00, 0x48, 0x87, 0x2c,
        0x24, 0x48, 0x8d, 0x64, 0x24, 0x08, 0xff, 0x64, 0x24, 0xf8};
    std::array<unsigned char, 5> actual_entry{};
    std::array<unsigned char, 30> actual_thunk{};
    return fx::read(image + 0x14bec0, actual_entry) && actual_entry == entry &&
           fx::read(image + 0x1b3cbe1, actual_thunk) && actual_thunk == thunk;
}

bool canonical_player(uintptr_t expected_player) noexcept {
    uintptr_t manager = 0, player = 0, control = 0, vtable = 0, verify = 0;
    int32_t health = 0, maximum = 0;
    if (!slot || !expected_player || !fx::read(slot, manager) || !manager ||
        !pointer(manager, 0xd0, player) || player != expected_player || !pointer(player, 0x3e0, control) ||
        !fx::read(control, vtable) || vtable != image_base + 0x10bfee0 ||
        !fx::read(player + 0x168, health) || !fx::read(player + 0x170, maximum) ||
        health <= 0 || health > maximum || maximum > 1000000)
        return false;
    return fx::read(slot, verify) && verify == manager && pointer(manager, 0xd0, verify) && verify == player &&
           pointer(player, 0x3e0, verify) && verify == control;
}

bool local_owner(uintptr_t effect_control, uintptr_t& player) noexcept {
    uintptr_t manager = 0, control = 0;
    if (!effect_control || !fx::read(slot, manager) || !manager) return false;
    // Resolve the player through the already-validated canonical chain rather
    // than trusting the observer's incoming control pointer.
    if (!pointer(manager, 0xd0, player) || !pointer(player, 0x3e0, control) || control != effect_control) return false;
    return canonical_player(player);
}

bool install_impl(uintptr_t base_slot, uintptr_t image, void* target, bool explicit_opt_in) {
    if (!explicit_opt_in) return false;
    fx::require(!installed.load() && base_slot && image && target, "Invalid or repeated bonfire observer setup");
    slot = base_slot;
    image_base = image;
    const auto initialized = MH_Initialize();
    fx::require(initialized == MH_OK || initialized == MH_ERROR_ALREADY_INITIALIZED, "Cannot initialize bonfire observer hook backend");
    fx::require(MH_CreateHook(target, reinterpret_cast<void*>(&DS2FX_LanternBonfireDetour), &DS2FX_LanternBonfireOriginal) == MH_OK,
                "Cannot prepare bonfire observer");
    if (MH_EnableHook(target) != MH_OK) {
        MH_RemoveHook(target);
        DS2FX_LanternBonfireOriginal = nullptr;
        fx::require(false, "Cannot enable bonfire observer");
    }
    installed.store(true);
    observing.store(true);
    return true;
}
} // namespace

bool install(uintptr_t base_slot, bool explicit_offline_opt_in) {
    if (!explicit_offline_opt_in) return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;
    fx::require(false, "Bonfire observer is offline-only");
    return false;
#else
    const auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path()) == fx::game_hash, "Unsupported executable for bonfire observer");
    fx::require(base_slot == image + 0x16148f0 && native_entries_match(image),
                "ApplySpEffect entry mismatch; bonfire observer refused");
    return install_impl(base_slot, image, reinterpret_cast<void*>(image + 0x14bec0), true);
#endif
}

void disable() noexcept { observing.store(false); }

bool snapshot(Receipt& output) noexcept {
    output = Receipt{};
    if (!observing.load() || publishing.test_and_set(std::memory_order_acquire)) return false;
    output = current;
    publishing.clear(std::memory_order_release);
    if (!observing.load() || !output.sampled_at_ms || GetTickCount64() - output.sampled_at_ms > 1500) {
        output = Receipt{};
        return false;
    }
    if (!canonical_player(output.owner_token)) {
        output = Receipt{};
        return false;
    }
    return true;
}

Status status() noexcept {
    return {calls.load(), bonfire_effects.load(), accepted.load(), rejected.load(), first_thread.load(),
            installed.load(), observing.load()};
}

void observe(uintptr_t effect_control, uintptr_t request) noexcept {
    ++calls;
    if (!observing.load()) return;
    RequestPrefix prefix{};
    if (!request || !fx::read(request, prefix) || prefix.effect_id != bonfire_rest_effect) return;
    ++bonfire_effects;
    uintptr_t player = 0;
    if (!local_owner(effect_control, player)) {
        ++rejected;
        return;
    }
    DWORD expected = 0;
    first_thread.compare_exchange_strong(expected, GetCurrentThreadId());
    if(first_thread.load()!=GetCurrentThreadId()) {++rejected;return;}
    if (publishing.test_and_set(std::memory_order_acquire)) {
        ++rejected;
        return;
    }
    const Receipt receipt{GetTickCount64(), sequence.fetch_add(1) + 1, player, prefix.effect_id};
    current = receipt;
    publishing.clear(std::memory_order_release);
    ++accepted;
#ifdef DS2FX_LANTERN_RESEARCH
    // Capture provenance here, before returning to ApplySpEffect. The native
    // award itself is deferred until a later validated game update.
    fx::lantern_runtime::bonfire_rest(receipt);
#endif
}

#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot, uintptr_t image, void* target, bool explicit_opt_in) {
    return install_impl(base_slot, image, target, explicit_opt_in);
}
#endif

} // namespace fx::lantern_bonfire

extern "C" void DS2FX_ObserveLanternBonfire(uintptr_t effect_control, uintptr_t request) noexcept {
    fx::lantern_bonfire::observe(effect_control, request);
}
