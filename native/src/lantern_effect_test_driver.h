#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <cstddef>

// VISUAL TEST ONLY. Does not consume oil or implement a gameplay activation.
namespace fx::lantern_effect_test {
struct Request {
    int32_t effect_id=60425000, quantity=1;
    float lifetime=-1.0f;
    uint8_t source=16, state=1, unknown_e=0, unknown_f=0;
};
static_assert(sizeof(Request)==16 && offsetof(Request,lifetime)==8 && offsetof(Request,source)==12);
using Apply=void(__fastcall*)(uintptr_t effect_control,Request* request);
struct Status {
    uint64_t calls=0, issued=0, character_epochs=0;
    DWORD engine_thread=0;
    bool prepared=false, enabled=false, fault=false;
};
class Driver final {
public:
    bool prepare(uintptr_t base_slot,bool explicit_offline_test_opt_in,int32_t expected_save_slot,float delay_seconds=5.0f);
    void disable() noexcept;
    // Must be called inline by the verified native GameManUpdate hook, BEFORE
    // its 100ms publication throttle. verified_thread comes from that hook's
    // thread latch, never the worker. An unavailable/dead character resets the
    // current diagnostic epoch; 5 consecutive gameplay seconds are required.
    void on_engine_tick(uintptr_t manager,uintptr_t update_address,DWORD verified_thread) noexcept;
    Status status() const noexcept;
#ifdef DS2FX_FIXTURE
    bool prepare_owned_fixture(uintptr_t base_slot,Apply owned_function,bool explicit_opt_in,int32_t expected_save_slot=5,float delay_seconds=5.0f);
#endif
private:
    bool prepare_impl(uintptr_t,Apply,bool,int32_t,float);
    uintptr_t base_slot_=0,image_=0,character_=0,control_=0;
    int32_t expected_slot_=-1; // additional QA scope limit, NOT unique save identity
    Apply apply_=nullptr;
    float delay_=0,required_delay_=5.0f;
    bool called_for_epoch_=false,verify_native_=false;
    std::atomic_flag lock_=ATOMIC_FLAG_INIT;
    std::atomic<bool> prepared_{false},enabled_{false},fault_{false};
    std::atomic<DWORD> engine_thread_{0};
    std::atomic<uint64_t> calls_{0},issued_{0},epochs_{0};
};
}
