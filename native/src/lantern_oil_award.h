#pragma once
#include "lantern_award.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <limits>

namespace fx::lantern_oil_award {
inline constexpr uint32_t oil_item_id=60425000;
// DS2S-META GiveItemSilently + exact Scholar ItemGive disassembly. The native
// routine iterates 16-byte writable requests and returns its validator's AL.
struct ItemRequest {
    uint32_t kind=0, item=oil_item_id;
    float durability=std::numeric_limits<float>::max();
    uint16_t quantity=0;
    uint8_t upgrade=0, infusion=0;
};
static_assert(sizeof(ItemRequest)==16 && offsetof(ItemRequest,item)==4 &&
    offsetof(ItemRequest,durability)==8 && offsetof(ItemRequest,quantity)==12 &&
    offsetof(ItemRequest,upgrade)==14 && offsetof(ItemRequest,infusion)==15);
using ItemGive=uint8_t(__fastcall*)(uintptr_t bag,ItemRequest* requests,int32_t count,int32_t mode);

struct Status {
    uint64_t queued=0, calls_issued=0, native_validation_true=0,
        duplicates=0, rejected=0, discarded=0;
    uint32_t pending=0;
    DWORD engine_thread=0;
    bool prepared=false, enabled=false, thread_fault=false, queue_fault=false;
};

// NOT WIRED to any runtime callback. Source integration must first demonstrate
// the torch receipt in a real offline pickup and use content without fixed oil
// companions. A successful native call is not proof of inventory quantity.
class Delivery final {
public:
    bool prepare(uintptr_t base_slot,bool explicit_offline_opt_in);
    void disable() noexcept;
    // Caller supplies a nonzero, monotonically increasing character-lifetime
    // epoch. Advance it after load/unload, death, or canonical identity changes.
    // Invoke only from the proven native engine update hook, never a worker.
    void on_engine_tick(uint64_t character_epoch) noexcept;
    // Invoke INLINE at the end of the successful torch-award callback, while
    // its original canonical owner is still current. Do not bind a Receipt
    // popped on a worker later: that loses its character-lifetime provenance.
    // This captures the canonical bag and cannot issue ItemGive itself.
    bool on_successful_torch_award(const lantern_award::Receipt&,uint64_t character_epoch) noexcept;
    Status status() const noexcept;
#ifdef DS2FX_FIXTURE
    bool prepare_owned_fixture(uintptr_t base_slot,ItemGive owned_function,bool explicit_opt_in);
#endif
private:
    struct Character {
        uintptr_t manager=0, player=0, data=0, inventory=0, bag=0, torch_owner=0;
        bool operator==(const Character&) const=default;
    };
    struct Pending {
        Character character{};
        uint64_t epoch=0,tick=0,sequence=0,observed_at_ms=0;
        uint16_t quantity=0;
    };
    bool read_character(Character&) const noexcept;
    bool claim() noexcept;
    void release() noexcept;
    void clear_queue() noexcept;
    bool prepare_impl(uintptr_t,ItemGive,bool);
    uintptr_t base_slot_=0;
    ItemGive item_give_=nullptr;
    std::array<unsigned char,300> native_body_{};
    bool verify_body_=false,needs_new_epoch_=false;
    Character character_{};
    uint64_t epoch_=0,tick_=0,last_sequence_=0;
    std::array<Pending,64> queue_{};
    size_t head_=0,tail_=0,count_=0;
    std::atomic_flag lock_=ATOMIC_FLAG_INIT;
    std::atomic<bool> prepared_{false},enabled_{false},thread_fault_{false},queue_fault_{false};
    std::atomic<DWORD> engine_thread_{0};
    std::atomic<uint32_t> pending_{0};
    std::atomic<uint64_t> queued_{0},issued_{0},validation_true_{0},duplicates_{0},rejected_{0},discarded_{0};
};
}
