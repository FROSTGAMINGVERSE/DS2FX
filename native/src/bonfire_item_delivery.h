#pragma once
#include "lantern_bonfire_observer.h"
#include "lantern_oil_award.h"
#include <atomic>

namespace fx::bonfire_item_delivery {
inline constexpr uint32_t lantern_item = 60425001;
struct Status {
    uint64_t queued=0, issued=0, verified=0, present=0, rejected=0, discarded=0, unverified=0;
    uint32_t last_carried=0,last_stored=0;
    DWORD thread=0;
    bool enabled=false,fault=false;
};
// An authenticated local bonfire receipt captures its owner inline. Only a
// later, continuous native update may award one missing utility Good.
class Delivery final {
public:
    bool prepare(uintptr_t base_slot,bool opt_in);
    void on_engine_tick(uintptr_t manager,bool continuous_frame) noexcept;
    void on_bonfire(const lantern_bonfire::Receipt&) noexcept;
    void disable() noexcept;
    Status status() const noexcept;
#ifdef DS2FX_FIXTURE
    bool prepare_owned_fixture(uintptr_t base_slot,uintptr_t image,lantern_oil_award::ItemGive,bool opt_in);
    uint64_t owner_reads() const noexcept {return owner_reads_;}
    uint64_t inventory_checks() const noexcept {return inventory_checks_;}
#endif
private:
    struct Owner {
        uintptr_t manager=0,player=0,data=0,inventory=0,context=0,bag=0,coffer=0;
        bool operator==(const Owner&) const=default;
    };
    bool read_owner(Owner&) const noexcept;
    bool quantities(const Owner&,uint32_t&,uint32_t&) const noexcept;
    bool prepare_impl(uintptr_t,uintptr_t,lantern_oil_award::ItemGive,bool);
    uintptr_t slot_=0,image_=0;
    lantern_oil_award::ItemGive give_=nullptr;
    std::array<unsigned char,300> body_{};
    bool verify_body_=false,pending_=false,busy_=false;
    Owner queued_owner_{};
    uintptr_t tick_manager_=0;
    uint64_t epoch_=0,queued_epoch_=0,tick_=0,queued_tick_=0,last_sequence_=0,last_tick_ms_=0,queued_ms_=0;
    std::atomic<DWORD> thread_{0};
    std::atomic<bool> enabled_{false},fault_{false};
    std::atomic<uint64_t> queued_{0},issued_{0},verified_{0},present_{0},rejected_{0},discarded_{0},unverified_{0};
    std::atomic<uint32_t> carried_{0},stored_{0};
#ifdef DS2FX_FIXTURE
    mutable uint64_t owner_reads_=0,inventory_checks_=0;
#endif
};
}
