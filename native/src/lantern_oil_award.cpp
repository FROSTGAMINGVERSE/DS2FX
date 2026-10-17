#include "lantern_oil_award.h"
#include "common.h"
#include <cmath>
#include <cstring>

namespace fx::lantern_oil_award {
namespace {
bool pointer(uintptr_t source,uintptr_t offset,uintptr_t& out) noexcept {
    out=0;
    return source && offset<=UINTPTR_MAX-source && fx::read(source+offset,out) &&
        out>=0x10000 && out<=0x00007fffffffffffULL && !(out&7);
}
std::string body_hash(const std::array<unsigned char,300>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Oil award SHA256 provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{algorithm,hash};
    fx::require(BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)==0,"Oil award SHA256 init failed");
    unsigned char digest[32];
    fx::require(BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0)==0 &&
        BCryptFinishHash(hash,digest,sizeof(digest),0)==0,"Oil award SHA256 failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
}
bool Delivery::prepare_impl(uintptr_t base_slot,ItemGive function,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!prepared_.load() && base_slot && function,"Invalid/repeated oil award preparation");
    base_slot_=base_slot;item_give_=function;prepared_.store(true);enabled_.store(true);return true;
}
bool Delivery::prepare(uintptr_t base_slot,bool explicit_offline_opt_in) {
    if(!explicit_offline_opt_in)return false;
#ifdef DS2FX_SEAMLESS_BRIDGE
    (void)base_slot;fx::require(false,"Companion oil delivery is offline-only");return false;
#else
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash && base_slot==image+0x16148f0,"Unsupported executable/BaseA for oil delivery");
    const auto address=image+0x1a7470;
    fx::require(fx::read(address,native_body_) && body_hash(native_body_)==
        "f633b7d3ebf0934742875fc36b68fab1d80fddbd3b85f1be157bca686f820e35","ItemGive body changed; oil delivery refused");
    verify_body_=true;
    return prepare_impl(base_slot,reinterpret_cast<ItemGive>(address),true);
#endif
}
#ifdef DS2FX_FIXTURE
bool Delivery::prepare_owned_fixture(uintptr_t base_slot,ItemGive owned_function,bool explicit_opt_in) {
    return prepare_impl(base_slot,owned_function,explicit_opt_in);
}
#endif
void Delivery::disable() noexcept {enabled_.store(false);}
bool Delivery::claim() noexcept {
    if(lock_.test_and_set(std::memory_order_acquire)) {
        queue_fault_.store(true);enabled_.store(false);return false;
    }
    return true;
}
void Delivery::release() noexcept {lock_.clear(std::memory_order_release);}
void Delivery::clear_queue() noexcept {
    discarded_.fetch_add(count_);head_=tail_=count_=0;pending_.store(0);
}
bool Delivery::read_character(Character& c) const noexcept {
    c={};int32_t hp=0,maximum=0;
    return pointer(base_slot_,0,c.manager) && pointer(c.manager,0xd0,c.player) &&
        fx::read(c.player+0x168,hp) && fx::read(c.player+0x170,maximum) &&
        hp>0 && maximum>=hp && maximum<=1000000 &&
        pointer(c.manager,0xa8,c.data) && pointer(c.data,0x10,c.inventory) &&
        pointer(c.inventory,0x10,c.bag) && pointer(c.bag,0x28,c.torch_owner);
}
bool Delivery::on_successful_torch_award(const lantern_award::Receipt& receipt,uint64_t epoch) noexcept {
    if(!enabled_.load())return false;
    const auto thread=GetCurrentThreadId();
    if(!engine_thread_.load() || thread!=engine_thread_.load() || receipt.game_thread!=thread) {
        ++rejected_;return false; // Worker calls never bind a receipt to a new character.
    }
    if(!claim())return false;
    struct Unlock {Delivery* self;~Unlock(){self->release();}} unlock{this};
    if(!enabled_.load())return false;
    if(receipt.sequence && receipt.sequence<=last_sequence_){++duplicates_;return false;}
    // The high-water mark is never reset at an epoch boundary, so an old
    // receipt cannot be replayed against a newly loaded character.
    last_sequence_=(std::max)(last_sequence_,receipt.sequence);
    Character current{};const auto now=GetTickCount64();
    if(!receipt.sequence || !receipt.quantity || receipt.quantity>65535 ||
        !std::isfinite(receipt.native_torch_before) || !std::isfinite(receipt.native_torch_after) ||
        receipt.native_torch_before<0 || receipt.native_torch_after<=receipt.native_torch_before ||
        receipt.observed_at_ms>now || now-receipt.observed_at_ms>5000 ||
        !epoch || epoch!=epoch_ || needs_new_epoch_ || !read_character(current) || !(current==character_)) {
        ++rejected_;return false;
    }
    if(count_==queue_.size()){queue_fault_.store(true);enabled_.store(false);++rejected_;return false;}
    queue_[tail_]={current,epoch,tick_,receipt.sequence,receipt.observed_at_ms,static_cast<uint16_t>(receipt.quantity)};
    tail_=(tail_+1)%queue_.size();++count_;pending_.store(static_cast<uint32_t>(count_));++queued_;return true;
}
void Delivery::on_engine_tick(uint64_t epoch) noexcept {
    if(!enabled_.load())return;
    DWORD expected=0;const auto thread=GetCurrentThreadId();engine_thread_.compare_exchange_strong(expected,thread);
    if(engine_thread_.load()!=thread){thread_fault_.store(true);enabled_.store(false);return;}
    if(!claim())return;
    struct Unlock {Delivery* self;~Unlock(){self->release();}} unlock{this};
    if(!enabled_.load())return;
    Character current{};
    if(!epoch || epoch<epoch_ || !read_character(current)) {
        clear_queue();character_={};needs_new_epoch_=epoch_!=0;++rejected_;return;
    }
    if(epoch>epoch_) {
        clear_queue();epoch_=epoch;character_=current;needs_new_epoch_=false;
    } else if(needs_new_epoch_ || !(current==character_)) {
        clear_queue();character_={};needs_new_epoch_=true;++rejected_;return;
    }
    ++tick_;
    if(!count_)return;
    const auto pending=queue_[head_];
    // Only a later native tick may dispatch. Consume before calling the engine:
    // its return/exception cannot cause a duplicate retry of an ambiguous award.
    if(pending.tick>=tick_)return;
    head_=(head_+1)%queue_.size();--count_;pending_.store(static_cast<uint32_t>(count_));
    const auto now=GetTickCount64();
    if(pending.epoch!=epoch_ || !(pending.character==current) ||
        pending.observed_at_ms>now || now-pending.observed_at_ms>5000) {++discarded_;return;}
    std::array<unsigned char,300> body{};
    if(verify_body_ && (!fx::read(reinterpret_cast<uintptr_t>(item_give_),body) || body!=native_body_)) {
        ++discarded_;enabled_.store(false);return;
    }
    Character immediately_before{};
    if(!enabled_.load() || !read_character(immediately_before) || !(immediately_before==pending.character)) {
        ++discarded_;return;
    }
    ItemRequest request{};request.quantity=pending.quantity;
    ++issued_;
    // Holding a try-only lock rejects recursive callbacks without waiting. All
    // native state writes occur inside this engine function on this engine thread.
    if(item_give_(immediately_before.bag,&request,1,0))++validation_true_;
}
Status Delivery::status() const noexcept {
    return {queued_.load(),issued_.load(),validation_true_.load(),duplicates_.load(),rejected_.load(),discarded_.load(),
        pending_.load(),engine_thread_.load(),prepared_.load(),enabled_.load(),thread_fault_.load(),queue_fault_.load()};
}
}
