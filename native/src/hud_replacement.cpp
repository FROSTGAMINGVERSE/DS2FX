// Authenticated local HUD draw replacement; game controllers remain untouched.
#include "hud_replacement.h"
#include "common.h"
#include "../vendor/minhook/include/MinHook.h"
#include <atomic>
#include <mutex>
#include <cstring>

extern "C" {
void* DS2FX_HudObjectDrawOriginal=nullptr;
void* DS2FX_HudObjectMaskOriginal=nullptr;
void* DS2FX_HudTextureOriginal=nullptr;
}
namespace fx::hud_replacement {
namespace {
uintptr_t image=0,slot=0;
std::atomic<bool> installed{false};
std::atomic<uintptr_t> owner{0};
std::atomic<uint64_t> generation{0},frame_epoch{0},last_frame{0},hidden{0};
std::atomic<uint64_t> lease_rejected{0},alternate_thread_draws{0},identity_rejected{0},
    serial_rejected{0},grants{0},revokes{0},owner_invalidations{0},max_grant_gap_ms{0};
std::atomic<uint64_t> lease_serial{1};
std::atomic<uint64_t> covenant_serial{1},effects_serial{1};
std::atomic<DWORD> draw_thread{0};
std::atomic<unsigned> identity_failure{0};
#ifdef DS2FX_FIXTURE
std::atomic<void(*)()> fixture_after_serial{nullptr};
#endif
std::mutex identity_mutex;
Identity cached{};
std::array<std::atomic<uintptr_t>,13> candidates{};
std::array<std::atomic<uintptr_t>,20> effect_candidates{};
std::array<std::atomic<uintptr_t>,20> effect_leaves{};
hud_icons::EffectsIdentity cached_effects{};
DS2FX_HudIcons observed_icons{sizeof(DS2FX_HudIcons)};
std::atomic<uint32_t> icon_regions{0};
std::atomic<uint64_t> icon_frame{0},icon_epoch{0};
bool checked_read(uintptr_t p,auto& value) noexcept {
    return p>=0x10000 && p<=0x00007fffffffffffULL-sizeof(value) && fx::read(p,value);
}
// Hash the complete reviewed routine, including chained pdata fragments.
std::string body_hash(uintptr_t p,size_t n) {
    std::vector<unsigned char> bytes(n);SIZE_T got=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),bytes.data(),n,&got)&&got==n,"HUD draw body unreadable");
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"HUD SHA provider failed");
    struct Cleanup { BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);} } cleanup{a,h};
    unsigned char digest[32]{};
    fx::require(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0&&BCryptHashData(h,bytes.data(),static_cast<ULONG>(n),0)==0&&BCryptFinishHash(h,digest,32,0)==0,"HUD draw SHA failed");
    std::ostringstream out;for(auto b:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();
}
bool install_impl(uintptr_t base_slot,uintptr_t executable,void* const targets[3]) {
    fx::require(!installed.load()&&base_slot&&executable,"Invalid HUD suppression setup");
    slot=base_slot;image=executable;
    const auto init=MH_Initialize();fx::require(init==MH_OK||init==MH_ERROR_ALREADY_INITIALIZED,"HUD hook backend failed");
    void* detours[]{reinterpret_cast<void*>(&DS2FX_HudObjectDrawDetour),reinterpret_cast<void*>(&DS2FX_HudObjectMaskDetour),reinterpret_cast<void*>(&DS2FX_HudTextureDetour)};
    void** originals[]{&DS2FX_HudObjectDrawOriginal,&DS2FX_HudObjectMaskOriginal,&DS2FX_HudTextureOriginal};
    unsigned created=0,enabled=0;
    try {
        for(;created<3;++created)fx::require(MH_CreateHook(targets[created],detours[created],originals[created])==MH_OK,"Cannot prepare HUD draw hook");
        for(;enabled<3;++enabled)fx::require(MH_EnableHook(targets[enabled])==MH_OK,"Cannot enable HUD draw hook");
    } catch(...) {
        while(enabled)MH_DisableHook(targets[--enabled]);
        while(created)MH_RemoveHook(targets[--created]);
        slot=0;image=0;
        DS2FX_HudObjectDrawOriginal=nullptr;
        DS2FX_HudObjectMaskOriginal=nullptr;
        DS2FX_HudTextureOriginal=nullptr;
        throw;
    }
    installed.store(true);return true;
}
}
bool install(uintptr_t base_slot) {
    const auto executable=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==executable+0x16148f0,"Unsupported executable for HUD replacement");
    constexpr uintptr_t rvas[]{0xb77d40,0xb77d80,0xb6f200};
    constexpr size_t lengths[]{63,63,474};
    constexpr const char* hashes[]{"10870aab25ce007b750f37ed70e9e80153f5e9de16fa2e28c6caff121d0c7f63","9d26820e8cd1144c2a34b49fbdd393fb79f5b9c1c660aa6eb68ad62d856b1156","0c8f249570d69274131b1b441962ff3cefa1021ed9adaa436731e4b05511980f"};
    void* targets[3]{};
    for(unsigned i=0;i<3;++i){fx::require(body_hash(executable+rvas[i],lengths[i])==hashes[i],"HUD draw instructions changed");targets[i]=reinterpret_cast<void*>(executable+rvas[i]);}
    return install_impl(base_slot,executable,targets);
}
void observe(uintptr_t player,uint64_t epoch,bool playable) noexcept {
    if(!installed.load())return;
    Identity before{},after{};unsigned reason=0;
    const auto read=[](uintptr_t p,auto& v){return checked_read(p,v);};
    if(!playable||!identify(image,slot,player,before,read,&reason)||!identify(image,slot,player,after,read,&reason)||before!=after) {
        std::lock_guard lock(identity_mutex);
        ++owner_invalidations;identity_failure.store(!playable?8:reason?reason:9);
        last_frame.store(0);++lease_serial;
        icon_regions.store(0);icon_frame.store(0);cached_effects={};observed_icons={sizeof(DS2FX_HudIcons)};
        for(auto& p:effect_candidates)p.store(0);
        for(auto& p:effect_leaves)p.store(0);
        for(auto& p:candidates)p.store(0);
        owner.store(0);generation.store(epoch);cached={};return;
    }
    {std::lock_guard lock(identity_mutex);
        if(cached==before&&generation.load()==epoch&&owner.load()==player)return;
        ++owner_invalidations;
        last_frame.store(0);++lease_serial;
        icon_regions.store(0);icon_frame.store(0);cached_effects={};observed_icons={sizeof(DS2FX_HudIcons)};
        for(auto& p:effect_candidates)p.store(0);
        for(auto& p:effect_leaves)p.store(0);
        for(auto& p:candidates)p.store(0);
        cached=before;owner.store(player);generation.store(epoch);
        for(unsigned i=0;i<13;++i)candidates[i].store(before.targets[i]);
    }
}
void observe_icons(const DS2FX_HudIcons& icons) noexcept {
    hud_icons::EffectsIdentity effects{};
    const auto read=[](uintptr_t p,auto& v){return checked_read(p,v);};
    const bool valid=(icons.available_regions&DS2FX_HUD_REGION_EFFECTS) &&
        hud_icons::identify_effects(image,slot,owner.load(),effects,read);
    std::lock_guard lock(identity_mutex);
    observed_icons=icons;
    if(!valid)observed_icons.available_regions&=~DS2FX_HUD_REGION_EFFECTS;
    if(cached_effects!=effects) {
        icon_regions.fetch_and(~DS2FX_HUD_REGION_EFFECTS);++effects_serial;
        cached_effects=effects;
        for(unsigned i=0;i<20;++i){effect_candidates[i].store(effects.rows[i]);effect_leaves[i].store(effects.leaves[i]);}
    }
    const auto lost=icon_regions.load()&~observed_icons.available_regions;
    if(lost){icon_regions.fetch_and(~lost);if(lost&2)++covenant_serial;if(lost&4)++effects_serial;}
}
uint32_t present_regions(uint64_t epoch,uint64_t sample,uint32_t regions) noexcept {
    const auto granted=present_region_mask(epoch,sample,regions);
    return !(regions&~7u) && granted==regions;
}
uint32_t present_region_mask(uint64_t epoch,uint64_t sample,uint32_t regions) noexcept {
    if(regions&~7u)return 0;
    const auto bars=present(epoch,(regions&DS2FX_HUD_REGION_BARS)!=0);
    std::lock_guard lock(identity_mutex);
    uint32_t desired=regions&(DS2FX_HUD_REGION_COVENANT|DS2FX_HUD_REGION_EFFECTS);
    if(desired && (!installed.load() || !owner.load() || epoch!=generation.load() ||
        epoch!=observed_icons.character_epoch || !sample || sample>observed_icons.sample ||
        observed_icons.sample-sample>2))desired=0;
    desired&=observed_icons.available_regions;
    const auto lost=icon_regions.load()&~desired;
    if(lost&2)++covenant_serial;if(lost&4)++effects_serial;
    icon_regions.store(desired);icon_epoch.store(epoch);icon_frame.store(desired?GetTickCount64():0);
    return (bars ? regions&DS2FX_HUD_REGION_BARS : 0u)|desired;
}
uint32_t present(uint64_t epoch,uint32_t visible) noexcept {
    std::lock_guard lock(identity_mutex);
    if(!visible){last_frame.store(0);if(!epoch){icon_regions.store(0);icon_frame.store(0);}++lease_serial;++revokes;return 1;}
    if(!installed.load()||!epoch||epoch!=generation.load()||!owner.load()||!candidates[0].load())return 0;
    // A healthy refresh extends the same lease. Only revocation or changed
    // ownership invalidates an in-flight draw decision.
    const auto now=GetTickCount64(),previous=last_frame.load();
    if(previous && now>=previous) {
        const auto gap=now-previous;
        auto maximum=max_grant_gap_ms.load();
        while(gap>maximum && !max_grant_gap_ms.compare_exchange_weak(maximum,gap)) {}
    }
    frame_epoch.store(epoch);last_frame.store(now);++grants;return 1;
}
bool should_hide(uintptr_t object) noexcept {
    if(!installed.load())return false;
    unsigned region=0,effect_index=20;
    for(unsigned i=0;i<13;++i)if(candidates[i].load()==object)region=i<11?DS2FX_HUD_REGION_BARS:DS2FX_HUD_REGION_COVENANT;
    if(!region)for(unsigned i=0;i<20;++i)if(effect_candidates[i].load()==object || effect_leaves[i].load()==object){region=DS2FX_HUD_REGION_EFFECTS;effect_index=i;break;}
    if(!region)return false;
    const bool icons=region!=DS2FX_HUD_REGION_BARS;
    const auto leased=[&] {return (!icons || (icon_regions.load()&region)) &&
        lease_valid(GetTickCount64(),icons?icon_frame.load():last_frame.load(),icons?icon_epoch.load():frame_epoch.load(),generation.load());};
    if(!leased()){
        ++lease_rejected;return false;
    }
    // Optional-region changes must not invalidate an in-flight bar draw.
    const auto global_serial=lease_serial.load();
    const auto serial=region==2?covenant_serial.load():region==4?effects_serial.load():global_serial;
#ifdef DS2FX_FIXTURE
    if(auto callback=fixture_after_serial.exchange(nullptr))callback();
#endif
    DWORD expected=0;draw_thread.compare_exchange_strong(expected,GetCurrentThreadId());
    // The same owned health objects draw on more than one live thread. This
    // thread ID is diagnostic only; ownership and lease checks authorize hides.
    if(draw_thread.load()!=GetCurrentThreadId())++alternate_thread_draws;
    const auto read=[](uintptr_t p,auto& v){return checked_read(p,v);};
    if(region==DS2FX_HUD_REGION_EFFECTS) {
        hud_icons::EffectsIdentity before{},after{},copy{};
        {std::lock_guard lock(identity_mutex);copy=cached_effects;}
        const auto same=[&](hud_icons::EffectsIdentity& value) {
            if(!hud_icons::effects_chain(image,slot,owner.load(),value,read) ||
               !hud_icons::effect_row(image,value,effect_index,read))return false;
            return value.base==copy.base && value.player==copy.player && value.chain==copy.chain &&
                value.header==copy.header && value.sprite==copy.sprite &&
                value.rows[effect_index]==copy.rows[effect_index] && value.leaves[effect_index]==copy.leaves[effect_index] &&
                (value.rows[effect_index]==object || value.leaves[effect_index]==object);
        };
        if(!same(before) || !same(after)){++identity_rejected;return false;}
    } else {
        Identity before{},after{},copy{};
        {std::lock_guard lock(identity_mutex);copy=cached;}
        unsigned reason=0;
        if(!identify(image,slot,owner.load(),before,read,&reason)||before!=copy||
           !identify(image,slot,owner.load(),after,read,&reason)||after!=before){identity_failure.store(reason?reason:9);++identity_rejected;return false;}
    }
    const auto current_serial=region==2?covenant_serial.load():region==4?effects_serial.load():lease_serial.load();
    if(global_serial!=lease_serial.load() || serial!=current_serial) {++serial_rejected;return false;}
    if(!leased()){
        ++lease_rejected;return false;
    }
    ++hidden;return true;
}
Status status() noexcept {
    const auto now=GetTickCount64(),last=last_frame.load();
    return {hidden.load(),lease_rejected.load(),alternate_thread_draws.load(),
        identity_rejected.load(),serial_rejected.load(),grants.load(),revokes.load(),
        owner_invalidations.load(),max_grant_gap_ms.load(),
        last && now>=last ? now-last : 0,identity_failure.load(),draw_thread.load(),installed.load()};
}
#ifdef DS2FX_FIXTURE
bool install_fixture(uintptr_t base_slot,uintptr_t executable,void* const targets[3]){return install_impl(base_slot,executable,targets);}
void set_fixture_after_serial(void(*callback)()) noexcept {fixture_after_serial.store(callback);}
#endif
}
extern "C" bool DS2FX_ShouldHideHudPrimitive(uintptr_t object) noexcept {return fx::hud_replacement::should_hide(object);}
