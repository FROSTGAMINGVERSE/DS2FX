#include "bonfire_item_delivery.h"
#include "common.h"
#include <cstring>

namespace fx::bonfire_item_delivery {
namespace {
bool pointer(uintptr_t parent,uintptr_t offset,uintptr_t& out) noexcept {
    out=0;return parent&&offset<=UINTPTR_MAX-parent&&fx::read(parent+offset,out)&&out>=0x10000&&out<=0x00007fffffffffffULL&&!(out&7);
}
std::string hash_body(const std::array<unsigned char,300>& bytes) {
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Item award SHA provider failed");
    struct Cleanup {BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{a,h};
    unsigned char digest[32];
    fx::require(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0&&BCryptHashData(h,const_cast<PUCHAR>(bytes.data()),300,0)==0&&BCryptFinishHash(h,digest,32,0)==0,"Item award SHA failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
// Native bag accessor1B2290/coffer1B7A00: (index+1)*40. Normal-list
// writer1AE470 stores Good quantity as uint16 at record+20, ID at+14.
bool scan(uintptr_t list,uint32_t capacity,uint32_t& quantity) noexcept {
    quantity=0;
    std::array<unsigned char,40*64> block{};
    for(uint32_t first=0;first<capacity;first+=64) {
        const auto n=(std::min)(64u,capacity-first);SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(list+40+first*40),block.data(),n*40,&copied)||copied!=n*40)return false;
        for(uint32_t i=0;i<n;++i) {
            const auto r=block.data()+i*40;uint32_t id=0;uint16_t count=0;
            std::memcpy(&id,r+0x14,4);std::memcpy(&count,r+0x20,2);
            if(r[0x10]==0&&id==lantern_item&&count)quantity+=count;
        }
    }
    return true;
}
}
bool Delivery::prepare_impl(uintptr_t slot,uintptr_t image,lantern_oil_award::ItemGive give,bool opt_in) {
    if(!opt_in)return false;
    fx::require(!enabled_.load()&&!slot_&&slot&&image&&give,"Invalid/repeated bonfire item delivery setup");
    slot_=slot;image_=image;give_=give;enabled_.store(true);return true;
}
bool Delivery::prepare(uintptr_t slot,bool opt_in) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)slot;fx::require(false,"Bonfire delivery is offline-only");return false;
#else
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&slot==image+0x16148f0,"Unsupported executable for bonfire delivery");
    fx::require(fx::read(image+0x1a7470,body_)&&hash_body(body_)=="f633b7d3ebf0934742875fc36b68fab1d80fddbd3b85f1be157bca686f820e35","Native ItemGive body changed");
    verify_body_=true;return prepare_impl(slot,image,reinterpret_cast<lantern_oil_award::ItemGive>(image+0x1a7470),true);
#endif
}
#ifdef DS2FX_FIXTURE
bool Delivery::prepare_owned_fixture(uintptr_t slot,uintptr_t image,lantern_oil_award::ItemGive give,bool opt_in){return prepare_impl(slot,image,give,opt_in);}
#endif
bool Delivery::read_owner(Owner& o) const noexcept {
#ifdef DS2FX_FIXTURE
    ++owner_reads_;
#endif
    o={};int32_t hp=0,max=0;uintptr_t vt=0;
    return pointer(slot_,0,o.manager)&&pointer(o.manager,0xd0,o.player)&&fx::read(o.player+0x168,hp)&&fx::read(o.player+0x170,max)&&hp>0&&max>=hp&&max<=1000000&&
        pointer(o.manager,0xa8,o.data)&&pointer(o.data,0x10,o.inventory)&&pointer(o.inventory,0x10,o.context)&&
        pointer(o.context,0x10,o.bag)&&fx::read(o.bag,vt)&&vt==image_+0x10c4408&&
        pointer(o.context,0x18,o.coffer)&&fx::read(o.coffer,vt)&&vt==image_+0x10c4528;
}
bool Delivery::quantities(const Owner& o,uint32_t& carried,uint32_t& stored) const noexcept {
#ifdef DS2FX_FIXTURE
    ++inventory_checks_;
#endif
    Owner after{};
    return scan(o.bag,3840,carried)&&scan(o.coffer,256,stored)&&read_owner(after)&&after==o;
}
void Delivery::on_bonfire(const lantern_bonfire::Receipt& r) noexcept {
    if(!enabled_.load())return;
    // No worker/observation snapshot can bind a receipt to another character.
    if(GetCurrentThreadId()!=thread_.load()){++rejected_;return;}
    if(busy_){fault_.store(true);enabled_.store(false);++rejected_;return;}
    const auto now=GetTickCount64();Owner current{};
    if(!r.sequence||r.sequence<=last_sequence_){++rejected_;return;}
    last_sequence_=r.sequence;
    if(r.effect_id!=lantern_bonfire::bonfire_rest_effect||r.sampled_at_ms>now||now-r.sampled_at_ms>1500||
       !last_tick_ms_||now-last_tick_ms_>250||!epoch_||!read_owner(current)||current.manager!=tick_manager_||r.owner_token!=current.player){++rejected_;return;}
    if(pending_){++rejected_;return;}
    queued_owner_=current;queued_epoch_=epoch_;queued_tick_=tick_;queued_ms_=now;pending_=true;++queued_;
}
void Delivery::on_engine_tick(uintptr_t manager,bool continuous) noexcept {
    if(!enabled_.load())return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();thread_.compare_exchange_strong(expected,tid);
    if(thread_.load()!=tid){fault_.store(true);enabled_.store(false);return;}
    if(busy_){fault_.store(true);enabled_.store(false);return;}
    const auto now=GetTickCount64();
    // Idle ticks track only native callback continuity. Resolve inventory owners
    // on a genuine rest and again before acting; never walk inventory per frame.
    if(!continuous||!manager||manager!=tick_manager_||
       (last_tick_ms_&&now-last_tick_ms_>500)) {
        if(pending_)++discarded_;pending_=false;++epoch_;tick_manager_=manager;
        last_tick_ms_=continuous&&manager?now:0;return;
    }
    last_tick_ms_=now;++tick_;
    if(!pending_||queued_tick_>=tick_)return;
    pending_=false; // Consume before any engine call; ambiguous results never retry per frame.
    Owner current{};
    if(queued_epoch_!=epoch_||now<queued_ms_||now-queued_ms_>1500||
       !read_owner(current)||current.manager!=manager||!(queued_owner_==current)){++discarded_;return;}
    uint32_t carried=0,stored=0;
    if(!quantities(current,carried,stored)){++rejected_;return;}
    carried_.store(carried);stored_.store(stored);
    if(carried||stored){++present_;return;}
    std::array<unsigned char,300> body{};Owner immediately_before{};
    if((verify_body_&&(!fx::read(reinterpret_cast<uintptr_t>(give_),body)||body!=body_))||!read_owner(immediately_before)||!(immediately_before==current)){++discarded_;return;}
    lantern_oil_award::ItemRequest request{};request.item=lantern_item;request.quantity=1;
    busy_=true;++issued_;const auto accepted=give_(current.context,&request,1,0);busy_=false;
    if(accepted&&quantities(current,carried,stored)&&carried==1&&stored==0)++verified_;else ++unverified_;
    carried_.store(carried);stored_.store(stored);
}
void Delivery::disable() noexcept {enabled_.store(false);}
Status Delivery::status() const noexcept {return{queued_.load(),issued_.load(),verified_.load(),present_.load(),rejected_.load(),discarded_.load(),unverified_.load(),carried_.load(),stored_.load(),thread_.load(),enabled_.load(),fault_.load()};}
}
