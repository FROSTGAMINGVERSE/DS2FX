#include "lantern_quickslot_ui.h"
#include "lantern_native_hash.h"
#include "lantern_item_toggle.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <array>
#include <intrin.h>
namespace fx::lantern_quickslot_ui {
namespace {
using Predicate=bool(__fastcall*)(uintptr_t);
using Property=int32_t(__fastcall*)(uintptr_t,int32_t);
using Icon=bool(__fastcall*)(uintptr_t,int32_t*,int32_t*);
using Format=uintptr_t(__fastcall*)(uintptr_t,uintptr_t,wchar_t*,uint32_t,int32_t,bool,int32_t,uint32_t);
using Integer=int32_t(__fastcall*)(wchar_t*,uint32_t,int32_t,bool,bool);
Predicate predicate_original=nullptr;Property property_original=nullptr;
Icon icon_original=nullptr;Format format_original=nullptr;Integer integer_original=nullptr;
uintptr_t slot=0,image=0;
std::atomic<bool> installed{false},enabled{false};
std::atomic<DWORD> thread{0};
std::atomic<uint64_t> when{0},counts{0},icons{0},depleted{0},rejected{0},inventory_counts{0};
thread_local int32_t formatting_charge=-1;
#ifdef DS2FX_FIXTURE
bool fixture=false;
#endif
bool caller(uintptr_t address,uintptr_t rva) noexcept {
#ifdef DS2FX_FIXTURE
    if(fixture)return true;
#endif
    return address==image+rva;
}
bool ptr(uintptr_t p,uintptr_t off,uintptr_t& value){return p&&off<=UINTPTR_MAX-p&&fx::read(p+off,value)&&value>=0x10000&&!(value&7);}
bool current_player(uintptr_t& player) {
    const auto now=GetTickCount64(),auth=when.load();
    if(!enabled.load()||!auth||now<auth||now-auth>250||thread.load()!=GetCurrentThreadId()){++rejected;return false;}
    uintptr_t manager=0;int32_t hp=0;
    return ptr(slot,0,manager)&&ptr(manager,0xd0,player)&&fx::read(player+0x168,hp)&&hp>0;
}
bool owned_data(uintptr_t data) {
    int32_t id=0,icon=0;uint8_t valid=0;
    return data>=0x10000&&fx::read(data+0x7c,id)&&id==60425001&&fx::read(data+0x80,icon)&&icon==60425001&&fx::read(data+0xac,valid)&&valid==1;
}
bool owned_ref(uintptr_t ref) {
    uintptr_t data=0,spell=0,other=0,repository=0;
    return ptr(ref,0,data)&&fx::read(ref+8,spell)&&!spell&&fx::read(ref+0x10,other)&&!other&&ptr(ref,0x30,repository)&&owned_data(data);
}
int32_t remaining(uintptr_t player) {
    fx::custom_item_resource::Snapshot state{};
    if(fx::lantern_item_toggle::charge_snapshot(state)&&state.owner_token==player&&state.item_id==60425001)return static_cast<int32_t>(state.current);
    // Read-only preview; an icon must never create/refill a resource ledger.
    return 3;
}
bool __fastcall predicate_detour(uintptr_t ref) {
    const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool native=predicate_original(ref);uintptr_t player=0;
    if(caller(address,0x50e384)&&current_player(player)&&owned_ref(ref))return true;
    return native;
}
int32_t __fastcall property_detour(uintptr_t ref,int32_t property) {
    const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto native=property_original(ref,property);
    // Only selected-consumable UI callers; never gameplay, shops or save data.
    const bool count=property==0x50&&caller(address,0x50e3a8);
    const bool icon=property==0x45&&caller(address,0x50d53b);
    uintptr_t player=0;
    if((count||icon)&&current_player(player)&&owned_ref(ref)) {
        const auto charge=remaining(player);
        if(count){++counts;return charge;}
        ++icons;if(!charge)++depleted;return charge?60425001:60425002;
    }
    return native;
}
bool __fastcall icon_detour(uintptr_t ref,int32_t* kind,int32_t* id) {
    const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const bool native=icon_original(ref,kind,id);
    uintptr_t player=0;int32_t native_kind=0,native_id=0;
    // The inventory's generic icon renderer owns these output arguments.
    if(native&&caller(address,0x40bc6)&&current_player(player)&&owned_ref(ref)&&
       fx::read(reinterpret_cast<uintptr_t>(kind),native_kind)&&native_kind==6&&
       fx::read(reinterpret_cast<uintptr_t>(id),native_id)&&native_id==60425001) {
        const auto empty=remaining(player)==0;const int32_t value=empty?60425002:60425001;
        SIZE_T written=0;
        if(WriteProcessMemory(GetCurrentProcess(),id,&value,sizeof(value),&written)&&written==sizeof(value)){++icons;if(empty)++depleted;}
    }
    return native;
}
uintptr_t __fastcall format_detour(uintptr_t data,uintptr_t result,wchar_t* buffer,uint32_t digits,int32_t padding,bool floating,int32_t property,uint32_t flags) {
    const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());uintptr_t player=0;
    if(property!=0x50||!caller(address,0x3bfd2)||!current_player(player)||!owned_data(data))
        return format_original(data,result,buffer,digits,padding,floating,property,flags);
    // Retain native output objects/allocators/widgets. Suppress only this
    // reusable item's infinity branch; the scoped integer formatter reads charges.
    const auto previous=formatting_charge;formatting_charge=remaining(player);
    struct Reset {int32_t previous;~Reset(){formatting_charge=previous;}} reset{previous};
    ++inventory_counts;
    return format_original(data,result,buffer,digits,padding,false,0x3c,flags&~uint32_t{8});
}
int32_t __fastcall integer_detour(wchar_t* buffer,uint32_t digits,int32_t value,bool padding,bool terminate) {
    const auto address=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(formatting_charge>=0&&caller(address,0x37a82))value=formatting_charge;
    return integer_original(buffer,digits,value,padding,terminate);
}
bool setup(uintptr_t base_slot,uintptr_t exe,const std::array<void*,5>& entries) {
    fx::require(!installed.load()&&base_slot&&exe,"Invalid/repeated Lantern item UI setup");slot=base_slot;image=exe;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"Item UI hook init failed");
    const std::array<void*,5> replacements{reinterpret_cast<void*>(&predicate_detour),reinterpret_cast<void*>(&property_detour),reinterpret_cast<void*>(&icon_detour),reinterpret_cast<void*>(&format_detour),reinterpret_cast<void*>(&integer_detour)};
    const std::array<void**,5> originals{reinterpret_cast<void**>(&predicate_original),reinterpret_cast<void**>(&property_original),reinterpret_cast<void**>(&icon_original),reinterpret_cast<void**>(&format_original),reinterpret_cast<void**>(&integer_original)};
    size_t created=0;
    try {
        for(size_t i=0;i<entries.size();++i){fx::require(entries[i]&&MH_CreateHook(entries[i],replacements[i],originals[i])==MH_OK,"Item UI entry hook preparation failed");++created;}
        for(auto entry:entries)fx::require(MH_QueueEnableHook(entry)==MH_OK,"Item UI hook activation queue failed");
        fx::require(MH_ApplyQueued()==MH_OK,"Item UI hook activation failed");
    }catch(...){for(size_t i=0;i<created;++i){MH_DisableHook(entries[i]);MH_RemoveHook(entries[i]);}throw;}
    installed.store(true);enabled.store(true);return true;
}
}
bool install(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;const auto exe=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==exe+0x16148f0,"Unsupported executable for native item UI");
    struct Gate {uintptr_t rva;size_t length;const char* hash;};
    const Gate gates[]{
        {0x39390,0x43,"77fe2203c3c10315dcd9ddbdcf3b18a96510bf48fe9e58475d935f47fba837cb"},
        {0x373f0,0x31,"b7a52374adaa3d55c0ce8783fe2bbacf62f9f0428558427ffe3eb2bcd8c123e6"},
        {0x37c50,0x57,"cdd155b10ca359eb76625482bd68b23bb10c499d7c18d7214534bd2092b5066c"},
        {0x37720,0x395,"d3edcdf8abc9dd7966eeb919705b929b5bb65bad3c576f97d8bdded95a91bf68"},
        {0x40c50,0x134,"e5e330825e6aa8947bb7ca6d2f273c2e36cad24f52bfa050f10f867ba13683d8"}};
    std::array<void*,5> entries{};
    for(size_t i=0;i<entries.size();++i){fx::require(fx::lantern_native_hash(exe+gates[i].rva,gates[i].length)==gates[i].hash,"Native item UI entry changed");entries[i]=reinterpret_cast<void*>(exe+gates[i].rva);}
    fx::require(fx::lantern_native_hash(exe+0x50e1b0,0x300)=="52b5e85928100fd02f3193add751afb8ce6d2ffce533b0a05a08cfff526e6983","Native quick-slot count caller changed");
    fx::require(fx::lantern_native_hash(exe+0x50d3d0,0x25e)=="8269bcdbb46e42d87b348fd1d42f967743a9201d49cbc61ac4ac20df24a52575","Native quick-slot icon caller changed");
    return setup(base_slot,exe,entries);
}
void disable() noexcept {enabled.store(false);}
void authorize_engine_tick(uintptr_t manager) noexcept {
    if(!enabled.load())return;uintptr_t actual=0;if(!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;thread.compare_exchange_strong(expected,GetCurrentThreadId());
    if(thread.load()!=GetCurrentThreadId()){enabled.store(false);return;}when.store(GetTickCount64());
}
Status status() noexcept {return {counts.load(),icons.load(),depleted.load(),rejected.load(),thread.load(),installed.load(),enabled.load(),inventory_counts.load()};}
#ifdef DS2FX_FIXTURE
bool install_owned_fixture(uintptr_t base_slot,uintptr_t exe,const std::array<void*,5>& entries){fixture=true;return setup(base_slot,exe,entries);}
void allow_owned_fixture_callers(bool allowed) noexcept {fixture=allowed;}
#endif
}
