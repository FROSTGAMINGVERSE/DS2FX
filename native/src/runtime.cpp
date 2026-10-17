#include "common.h"
#include "forge_options.h"
#include "../sdk/ds2fx_api.h"
#include "graphics_bridge.h"
#include "observation.h"
#include "hud_observation.h"
#include "hud_presentation.h"
#include "hud_replacement.h"
#include "stamina_hook.h"
#include "content_hook.h"
#include "player_states.h"
#include "player_message_observer.h"
#ifdef DS2FX_LANTERN_RESEARCH
#include "lantern_probe.h"
#include "lantern_award.h"
#include "lantern_effect.h"
#include "lantern_bonfire_observer.h"
#include "lantern_light_anchor.h"
#include "lantern_le_brightness.h"
#include "lantern_world_light.h"
#include "lantern_pose_observer.h"
#include "lantern_pivot_motion.h"
#include "lantern_model_visibility.h"
#include "lantern_item_toggle.h"
#include "lantern_light_selector.h"
#include "lantern_flame.h"
#include "lantern_runtime.h"
#include "../sdk/ds2fx_lantern_api.h"
#include "offline_network.h"
#endif
#ifdef DS2FX_SEAMLESS_BRIDGE
#include "seamless_contract.h"
#endif
#include <mutex>
#include <atomic>
#include <thread>
#include <intrin.h>

namespace {
HMODULE self = nullptr;
std::mutex log_mutex, state_mutex;
int observation_interval=100;
bool verbose_lantern_telemetry=false; // Set before the observation worker starts.
bool message_observer_installed=false;
std::ofstream logfile;
DS2FX_PlayerState snapshot{sizeof(DS2FX_PlayerState),0,0,0,0};
DS2FX_HudState hud_snapshot{sizeof(DS2FX_HudState)};
DS2FX_HudPresentation presentation_snapshot{sizeof(DS2FX_HudPresentation)};
DS2FX_HudIcons icons_snapshot{sizeof(DS2FX_HudIcons)};
DS2FX_PlayerForm form_snapshot{sizeof(DS2FX_PlayerForm)};
uintptr_t snapshot_player=0;
std::atomic<bool> stop{false};
HANDLE worker = nullptr;
LONG booted = 0;
uintptr_t base_slot = 0;
uintptr_t last_player = 0;
uint64_t epoch = 0;
struct Loaded { HMODULE library; DS2FX_Plugin plugin; std::string id; };
std::vector<Loaded> loaded;
std::atomic<HMODULE> authorized_hud{nullptr};
std::atomic<HMODULE> initializing_state_owner{nullptr};
std::atomic<DWORD> initializing_state_thread{0};
#ifdef DS2FX_LANTERN_RESEARCH
// Hooks and Core remain resident until process exit. Keep the observed LE
// module reference alive too; never call FreeLibrary under DLL detach lock.
fx::lantern_le_brightness::Adapter& lantern_brightness=*new fx::lantern_le_brightness::Adapter;
bool read_lantern_brightness(float& multiplier) noexcept {
    const auto value=lantern_brightness.sample();
    multiplier=value.multiplier;
    return value.status==fx::lantern_le_brightness::Status::ready;
}
#endif

void log_message(const char* id, const char* message) {
    std::lock_guard lock(log_mutex);
    if (logfile) { logfile << GetTickCount64() << " [" << (id ? id : "?") << "] " << (message ? message : "") << '\n'; logfile.flush(); }
}
void open_log() {
    wchar_t local[32768];
    auto n = GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
    fx::require(n && n < 32768,"LOCALAPPDATA unavailable");
    auto folder = fx::fs::path(local) / "DS2FX" / "logs";
    fx::no_links(folder);
    fx::fs::create_directories(folder);
    logfile.open(folder / ("runtime-" + std::to_string(GetCurrentProcessId()) + ".log"), std::ios::trunc);
    fx::require(logfile.good(),"Cannot create runtime log");
}
uint32_t get_state(DS2FX_PlayerState* output) {
    if (!output || output->size != sizeof(DS2FX_PlayerState)) return 0;
    std::lock_guard lock(state_mutex);
    *output = snapshot;
    return output->available;
}
uint32_t get_hud_state(DS2FX_HudState* output) {
    if (!output || output->size != sizeof(DS2FX_HudState)) return 0;
    std::lock_guard lock(state_mutex);
    *output = hud_snapshot;
    return output->player_available;
}
uint32_t get_hud_presentation(DS2FX_HudPresentation* output) {
    if(!output || (output->size!=32 && output->size!=sizeof(DS2FX_HudPresentation))) return 0;
    const auto requested=output->size;
    std::lock_guard lock(state_mutex);std::memcpy(output,&presentation_snapshot,requested);output->size=requested;
    return output->available;
}
uint32_t get_hud_icons(DS2FX_HudIcons* output) {
    if(!output || output->size!=sizeof(*output))return 0;
    std::lock_guard lock(state_mutex);*output=icons_snapshot;return output->available_regions;
}
uint32_t get_player_form(DS2FX_PlayerForm* output) {
    if(!output || output->size!=sizeof(*output))return 0;
    std::lock_guard lock(state_mutex);*output=form_snapshot;return output->available;
}
__declspec(noinline) uint32_t update_hud_regions(uint64_t character_epoch,uint64_t sample,uint32_t regions) noexcept {
    const auto authorized=authorized_hud.load();HMODULE caller=nullptr;
    if(!authorized || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(_ReturnAddress()),&caller) || caller!=authorized)return 0;
    const auto result=fx::hud_replacement::present_regions(character_epoch,sample,regions);
    if(!result) {
        static std::atomic<uint64_t> last_report{0};const auto now=GetTickCount64();auto previous=last_report.load();
        if(now-previous>=1000 && last_report.compare_exchange_strong(previous,now)) {
            DS2FX_HudIcons observed{sizeof(observed)};get_hud_icons(&observed);
            const auto status=fx::hud_replacement::status();std::ostringstream report;
            report<<"Region lease rejected requested="<<regions<<" epoch="<<character_epoch<<" sample="<<sample
                <<" observed_epoch="<<observed.character_epoch<<" observed_sample="<<observed.sample
                <<" available="<<observed.available_regions<<" hidden="<<status.hidden
                <<" identity_rejected="<<status.identity_rejected<<" serial_rejected="<<status.serial_rejected
                <<" lease_rejected="<<status.lease_rejected<<" gap="<<status.max_grant_gap_ms;
            log_message("hud-replacement",report.str().c_str());
        }
    }
    return result;
}
__declspec(noinline) uint32_t update_hud_replacement(uint64_t character_epoch,uint32_t visible) noexcept {
    const auto authorized=authorized_hud.load();
    if(!authorized)return 0;
    HMODULE caller=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(_ReturnAddress()),&caller) || caller!=authorized)return 0;
    return fx::hud_replacement::present(character_epoch,visible);
}
__declspec(noinline) uint32_t update_hud_region_mask(uint64_t character_epoch,uint64_t sample,uint32_t regions) noexcept {
    const auto authorized=authorized_hud.load();HMODULE caller=nullptr;
    if(!authorized || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(_ReturnAddress()),&caller) || caller!=authorized)return 0;
    const auto granted=fx::hud_replacement::present_region_mask(character_epoch,sample,regions);
    if(granted!=regions) {
        static std::atomic<uint64_t> last_report{0};const auto now=GetTickCount64();auto previous=last_report.load();
        if(now-previous>=1000 && last_report.compare_exchange_strong(previous,now)) {
            std::ostringstream report;report<<"Partial region grant requested="<<regions<<" granted="<<granted
                <<" epoch="<<character_epoch<<" sample="<<sample;
            log_message("hud-replacement",report.str().c_str());
        }
    }
    return granted;
}
HMODULE state_caller(void* address) noexcept {
    HMODULE caller=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(address),&caller))return nullptr;
    return caller;
}
__declspec(noinline) uint32_t register_player_state(const DS2FX_StateDefinition* d) {
    const auto caller=state_caller(_ReturnAddress());
    if(stop || !d || !caller || caller!=initializing_state_owner.load() ||
       GetCurrentThreadId()!=initializing_state_thread.load())return 0;
    return fx::player_states::service().add(reinterpret_cast<uintptr_t>(caller),*d);
}
__declspec(noinline) uint32_t register_player_jump_routes(const DS2FX_JumpRoutes* routes) {
    const auto caller=state_caller(_ReturnAddress());
    if(stop || !routes || !caller || caller!=initializing_state_owner.load() ||
       GetCurrentThreadId()!=initializing_state_thread.load())return DS2FX_STATE_INVALID;
    return fx::player_states::service().routes(reinterpret_cast<uintptr_t>(caller),*routes);
}
__declspec(noinline) uint32_t request_player_state(const DS2FX_StateRequest* request) {
    const auto caller=state_caller(_ReturnAddress());
    if(stop || !request || !caller)return DS2FX_STATE_INVALID;
    return fx::player_states::service().request(reinterpret_cast<uintptr_t>(caller),*request,GetTickCount64());
}
__declspec(noinline) uint32_t get_player_state_service(DS2FX_StateServiceStatus* output) {
    const auto caller=state_caller(_ReturnAddress());
    if(stop || !caller || !output || output->size!=sizeof(*output))return 0;
    *output=fx::player_states::service().status(reinterpret_cast<uintptr_t>(caller));return 1;
}
__declspec(noinline) uint64_t register_graphics_callback(const DS2FX_GraphicsCallback* callback) {
    const auto caller=state_caller(_ReturnAddress());
    if(stop || !caller || caller!=initializing_state_owner.load() || GetCurrentThreadId()!=initializing_state_thread.load())return 0;
    return fx::graphics::subscribe(caller,callback);
}
__declspec(noinline) uint32_t unregister_graphics_callback(uint64_t handle) {
    return fx::graphics::unsubscribe(state_caller(_ReturnAddress()),handle);
}
#ifdef DS2FX_SEAMLESS_BRIDGE
uint32_t reject_stamina_policy(uint32_t) { return 0; }
const DS2FX_Api api{sizeof(DS2FX_Api),DS2FX_API_VERSION,fx::version,log_message,get_state,reject_stamina_policy,get_hud_state,get_hud_presentation,update_hud_replacement,get_hud_icons,update_hud_regions,update_hud_region_mask,get_player_form,register_player_state,request_player_state,get_player_state_service,register_player_jump_routes,register_graphics_callback,unregister_graphics_callback};
#else
const DS2FX_Api api{sizeof(DS2FX_Api),DS2FX_API_VERSION,fx::version,log_message,get_state,fx::stamina_hook::register_policy,get_hud_state,get_hud_presentation,update_hud_replacement,get_hud_icons,update_hud_regions,update_hud_region_mask,get_player_form,register_player_state,request_player_state,get_player_state_service,register_player_jump_routes,register_graphics_callback,unregister_graphics_callback};
#endif

uintptr_t find_base_slot() {
    auto image = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    auto section = IMAGE_FIRST_SECTION(nt);
    // DS2S-META MIT research signature. Unique executable-section match required.
    const int signature[]{0x48,0x8b,0x05,-1,-1,-1,-1,0x48,0x8b,0x58,0x38,0x48,0x85,0xdb,0x74,-1,0xf6};
    uintptr_t match = 0;
    for (unsigned s = 0; s < nt->FileHeader.NumberOfSections; ++s) {
        if (!(section[s].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const size_t size = section[s].Misc.VirtualSize;
        fx::require(size <= nt->OptionalHeader.SizeOfImage && section[s].VirtualAddress <= nt->OptionalHeader.SizeOfImage - size, "Invalid PE section");
        std::vector<unsigned char> data(size);
        SIZE_T count = 0;
        fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(image+section[s].VirtualAddress),data.data(),size,&count) && count == size,"Cannot scan game section");
        for (size_t i = 0; i + sizeof(signature)/sizeof(int) <= data.size(); ++i) {
            bool equal = true;
            for (size_t j = 0; j < sizeof(signature)/sizeof(int); ++j) if (signature[j] >= 0 && signature[j] != data[i+j]) { equal = false; break; }
            if (!equal) continue;
            fx::require(!match,"Ambiguous BaseA signature; runtime refused");
            match = image + section[s].VirtualAddress + i;
        }
    }
    fx::require(match != 0,"BaseA signature not found; runtime refused");
    int32_t displacement = 0;
    fx::require(fx::read(match+3,displacement),"Cannot read BaseA displacement");
    const auto slot = match + 7 + displacement;
    fx::require(slot >= image && slot + sizeof(uintptr_t) <= image + nt->OptionalHeader.SizeOfImage,"BaseA outside game image");
    std::ostringstream details; details << "BaseA slot RVA 0x" << std::hex << slot-image << "; signature resolved; player fields require live validation";
    log_message("core",details.str().c_str());
    return slot;
}
DS2FX_PlayerState sample(uint64_t number) {
    DS2FX_PlayerState result{sizeof(DS2FX_PlayerState),0,number,0,0};
    uintptr_t base = 0, player = 0, base_after = 0, player_after = 0;
    int32_t hp = 0, maximum = 0;
    float stamina = 0, max_stamina = 0;
    // Read only. Verify that the chain survived the read, then sanity-check values.
    if (fx::read(base_slot,base) && base && fx::read(base+0xd0,player) && player &&
        fx::read(player+0x168,hp) && fx::read(player+0x170,maximum) &&
        fx::read(base_slot,base_after) && base_after == base && fx::read(base_after+0xd0,player_after) && player_after == player &&
        fx::valid_health(hp,maximum)) {
        result.available = 1; result.health = fx::observed_health(hp); result.max_health = maximum;
        if (last_player != player) ++epoch;
        last_player = player;
        // Optional field failure never hides a valid health observation.
        if (fx::read(player+0x1ac,stamina) && fx::read(player+0x1b4,max_stamina) &&
            fx::valid_stamina(stamina,max_stamina) && fx::read(base_slot,base_after) && base_after == base &&
            fx::read(base+0xd0,player_after) && player_after == player) {
            result.stamina_available = 1; result.stamina = stamina; result.max_stamina = max_stamina;
        }
    }
    if (!result.available) last_player = 0;
    result.character_epoch = epoch;
    return result;
}
DS2FX_HudState sample_hud(const DS2FX_PlayerState& player_state) {
    return fx::hud_observation::sample(base_slot,last_player,player_state,
        [](uintptr_t address,auto& value) { return fx::read(address,value); });
}
// Narrow SEH wrappers contain no objects requiring C++ unwinding.
uint32_t initialize_guarded(DS2FX_InitializePluginFn fn, DS2FX_Plugin* output) {
    __try { return fn(&api,output); } __except(EXCEPTION_EXECUTE_HANDLER) { return 0; }
}
bool event_guarded(void (*callback)(uint32_t,const DS2FX_PlayerState*), uint32_t event, const DS2FX_PlayerState* state) {
    __try { callback(event,state); return true; } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool dispatch(uint32_t event,const DS2FX_PlayerState& state) {
    for (const auto& module : loaded) if (!event_guarded(module.plugin.on_event,event,&state)) {
        log_message(module.id.c_str(),"Plugin callback fault; ALL observation callbacks stopped. Native code is not sandboxed.");
        stop = true;
        fx::player_states::service().revoke_all();
        fx::hud_replacement::present(0,0);
        fx::stamina_hook::percent=100;
        return false;
    }
    return true;
}
DWORD WINAPI observe(void*) {
    try {
        DS2FX_PlayerState previous{sizeof(DS2FX_PlayerState),0,0,0,0};
        uint64_t number = 0;
        while (!stop) {
            auto next = sample(++number);
            auto next_hud = sample_hud(next);
            const auto next_form=fx::hud_observation::sample_form(base_slot,last_player,next,
                [](uintptr_t address,auto& value){return fx::read(address,value);});
            auto next_presentation=fx::hud_presentation::sample(
                reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),base_slot,last_player,next,
                [](uintptr_t address,auto& value) { return fx::read(address,value); });
            // Drawing ownership is independent of gameplay/death/loading visibility.
            // A healthy blank HUD frame can retain suppression for this exact UI.
            const bool owned=next.available && next_presentation.available;
            fx::hud_replacement::observe(last_player,next.character_epoch,owned);
            auto next_icons=fx::hud_icons::sample(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),base_slot,last_player,next,
                [](uintptr_t address,auto& value){return fx::read(address,value);});
            if(!owned)next_icons.available_regions=0;
            fx::hud_replacement::observe_icons(next_icons);
            // Bounded, transition-only evidence; no engine actions or per-draw logging.
            static uint64_t death_until=0,death_report=0;
            const auto trace_now=GetTickCount64();
            if((next.available && next.health==0) || (previous.available && !next.available))death_until=trace_now+15000;
            if(trace_now<death_until && trace_now-death_report>=250){death_report=trace_now;
                uintptr_t raw_base=0,raw_player=0;int32_t raw_hp=0,raw_max=0;
                const bool raw_ok=fx::read(base_slot,raw_base)&&raw_base&&fx::read(raw_base+0xd0,raw_player)&&raw_player&&fx::read(raw_player+0x168,raw_hp)&&fx::read(raw_player+0x170,raw_max);
                const auto status=fx::hud_replacement::status();std::ostringstream trace;
                trace<<"Death trace available="<<next.available<<" hp="<<next.health<<" raw_ok="<<raw_ok<<" raw_hp="<<raw_hp<<" raw_max="<<raw_max
                    <<" native="<<next_presentation.available<<" scene="<<next_hud.game_state<<" loading="<<next_hud.loading<<" epoch="<<next.character_epoch
                    <<" hidden="<<status.hidden<<" lease_rejected="<<status.lease_rejected<<" identity_rejected="<<status.identity_rejected<<" identity_failure="<<status.identity_failure
                    <<" serial_rejected="<<status.serial_rejected<<" owners="<<status.owner_invalidations<<" revokes="<<status.revokes<<" grants="<<status.grants<<" lease_age="<<status.lease_age_ms;
                log_message("hud-replacement",trace.str().c_str());}

            { std::lock_guard lock(state_mutex); snapshot = next; hud_snapshot=next_hud;
              presentation_snapshot=next_presentation; icons_snapshot=next_icons;form_snapshot=next_form; snapshot_player=last_player; }
            // Observation can revoke authority, never authorize a native call.
            fx::player_states::service().observe(next.character_epoch,
                next.available && next.health>0 && (next_hud.available_fields&DS2FX_HUD_SCENE) &&
                next_hud.game_state==30 && next_hud.loading==0);
            // Native receipt only: the observation worker never invokes a state.
            static uint64_t message_sequence=0;
            jump_observer::Receipt message_receipt{};
            if(jump_observer::snapshot(message_receipt) && message_receipt.sequence!=message_sequence) {
                message_sequence=message_receipt.sequence;
                std::ostringstream evidence;
                evidence<<"receipt="<<message_receipt.sequence<<" message="<<message_receipt.message
                    <<" status="<<unsigned(message_receipt.status)<<" frame="<<message_receipt.frame
                    <<" native_thread="<<message_receipt.thread<<" observer_thread="<<GetCurrentThreadId();
                evidence<<" caller=0x"<<std::hex<<message_receipt.caller<<std::dec
                    <<" proposed_route="<<static_cast<uint32_t>(message_receipt.proposed_route)
                    <<" dispatched_message="<<message_receipt.dispatched_message
                    <<" movement_known="<<message_receipt.movement.known
                    <<" speed="<<message_receipt.movement.speed
                    <<" input_mask=0x"<<std::hex<<message_receipt.movement.mask<<std::dec;
#ifdef DS2FX_LANTERN_RESEARCH
                evidence<<" update_thread="<<fx::lantern_probe::status().first_thread;
#endif
                log_message("player-message-observer",evidence.str().c_str());
            }
            fx::transitions(previous,next,[](uint32_t event,const DS2FX_PlayerState& state) { if (!stop) dispatch(event,state); });
            previous = next;
#ifdef DS2FX_LANTERN_RESEARCH
            // Detection/hash work belongs to this worker. Native light ticks
            // only sample the already-validated four-byte slider value.
            if((number%10)==0) lantern_brightness.initialize();
            if(verbose_lantern_telemetry && (number%10)==0 && fx::lantern_probe::status().installed) {
                const auto status=fx::lantern_probe::status();
                fx::lantern_probe::Sample value{}; const bool available=fx::lantern_probe::snapshot(value);
                const auto awards=fx::lantern_award::status();
                std::ostringstream message;
                message<<"probe calls="<<status.calls<<" manager="<<status.manager_calls<<" samples="<<status.samples
                    <<" thread="<<status.first_thread<<" observer="<<GetCurrentThreadId()<<" fault="<<status.thread_fault
                    <<" available="<<available<<" torch_valid="<<value.native_torch_available<<" torch_raw="<<value.native_torch_time_raw
                    <<" slot_valid="<<value.save_slot_available<<" slot="<<value.save_slot_index<<" update_raw="<<value.update_value_raw
                    <<" award_calls="<<awards.calls<<" receipts="<<awards.receipts<<" award_thread="<<awards.first_thread
                    <<" award_fault="<<awards.thread_fault<<" queue_fault="<<awards.queue_fault;
                message<<" network_blocked="<<fx::offline_network::blocked_calls();
                const auto effect_status=fx::lantern_effect::status();
                fx::lantern_effect::Sample effect{};const bool effect_available=fx::lantern_effect::snapshot(effect);
                message<<" effect_calls="<<effect_status.calls<<" effect_matches="<<effect_status.matched_calls
                    <<" status_actions="<<effect_status.status_actions<<" effect_reject="<<effect_status.last_rejection
                    <<" effect_thread="<<effect_status.first_thread<<" effect_fault="<<effect_status.thread_fault
                    <<" oil_available="<<effect_available<<" oil_seconds="<<effect.remaining_seconds
                    <<" oil_active="<<effect.active<<" effect_metadata="<<effect.metadata_10<<','<<effect.metadata_14<<','<<effect.metadata_18<<','<<effect.metadata_1c;
                const auto bonfire=fx::lantern_bonfire::status();
                fx::lantern_bonfire::Receipt bonfire_receipt{};
                const bool bonfire_available=fx::lantern_bonfire::snapshot(bonfire_receipt);
                message<<" bonfire_calls="<<bonfire.calls<<" bonfire_effects="<<bonfire.bonfire_effects
                    <<" bonfire_accepted="<<bonfire.accepted<<" bonfire_rejected="<<bonfire.rejected
                    <<" bonfire_thread="<<bonfire.first_thread<<" bonfire_available="<<bonfire_available
                    <<" bonfire_sequence="<<bonfire_receipt.sequence;
                const auto test=fx::lantern_runtime::visual_test_status();
                const auto interact=fx::lantern_runtime::interact_status();
                const auto delivery=fx::lantern_runtime::delivery_status();
                const auto bonfire_delivery=fx::lantern_runtime::bonfire_delivery_status();
                message<<" bonfire_grant_queued="<<bonfire_delivery.queued<<" bonfire_grant_issued="<<bonfire_delivery.issued
                    <<" bonfire_grant_verified="<<bonfire_delivery.verified<<" bonfire_grant_present="<<bonfire_delivery.present
                    <<" bonfire_grant_rejected="<<bonfire_delivery.rejected<<" bonfire_grant_discarded="<<bonfire_delivery.discarded
                    <<" bonfire_grant_unverified="<<bonfire_delivery.unverified<<" lantern_carried="<<bonfire_delivery.last_carried
                    <<" lantern_stored="<<bonfire_delivery.last_stored<<" bonfire_grant_fault="<<bonfire_delivery.fault;
                const auto anchor=fx::lantern_light_anchor::status();
                const auto brightness=lantern_brightness.sample();
                const auto world=fx::lantern_world_light::status();
                const auto pose=fx::lantern_pose_observer::status();
                fx::lantern_pose_observer::Sample pose_sample{};
                const bool pose_available=fx::lantern_pose_observer::snapshot(pose_sample);
                const auto motion=fx::lantern_pivot_motion::status();
                const auto model_visibility=fx::lantern_model_visibility::status();
                const auto item_toggle=fx::lantern_item_toggle::status();
                const auto selector=fx::lantern_light_selector::status();
                message<<" selector_calls="<<selector.calls<<" selector_selected="<<selector.selected<<" selector_rejected="<<selector.rejected;
                fx::lantern_item_toggle::Receipt item_receipt{};
                const bool item_receipt_available=fx::lantern_item_toggle::snapshot(item_receipt);
                message<<" le_brightness="<<brightness.multiplier
                    <<" le_status="<<fx::lantern_le_brightness::status_name(brightness.status);
                message<<" world_calls="<<world.calls<<" world_eligible="<<world.eligible
                    <<" world_applied="<<world.applied<<" world_restored="<<world.restored
                    <<" world_thread_reject="<<world.rejected_thread<<" world_unavailable="<<world.unavailable
                    <<" world_invalid="<<world.invalid_value<<" world_fault="<<world.fault
                    <<" world_pending="<<world.restore_pending<<" world_multiplier="<<world.last_multiplier
                    <<" world_native="<<world.last_native_brightness<<" world_output="<<world.last_output_brightness
                    <<" world_radius_native="<<world.last_native_radius<<" world_radius_output="<<world.last_output_radius<<" world_flicker="<<world.last_flicker_gain
                    <<" world_render_enabled="<<world.last_render_enabled
                    <<" world_xyz="<<world.last_x<<','<<world.last_y<<','<<world.last_z;
                message<<" pose_calls="<<pose.calls<<" pose_canonical="<<pose.canonical
                    <<" pose_samples="<<pose.samples<<" pose_owned="<<pose.owned_bone
                    <<" pose_foreign="<<pose.foreign_thread<<" pose_invalid="<<pose.invalid
                    <<" pose_fault="<<pose.fault<<" pose_available="<<pose_available
                    <<" pose_bones="<<pose_sample.bone_count<<" pose_pivot="<<pose_sample.pivot_index
                    <<" pose_parent="<<pose_sample.parent_index<<" pose_parent_ok="<<pose_sample.parent_matches
                    <<" pose_valid="<<pose_sample.pose_valid<<" pose_authorized="<<pose_sample.on_authorized_thread
                    <<" pose_xyz="<<pose_sample.translation[0]<<','<<pose_sample.translation[1]<<','<<pose_sample.translation[2];
                message<<" motion_enabled="<<motion.enabled<<" motion_frames="<<motion.frames
                    <<" motion_steps="<<motion.steps<<" motion_applied="<<motion.applied<<" motion_repeated="<<motion.repeated
                    <<" motion_rejected="<<motion.rejected<<" motion_resets="<<motion.resets<<" motion_fault="<<motion.fault
                    <<" motion_xyz="<<motion.world_pivot[0]<<','<<motion.world_pivot[1]<<','<<motion.world_pivot[2]
                    <<" motion_lean="<<motion.lean[0]<<','<<motion.lean[1]
                    <<" motion_q="<<motion.local_rotation[0]<<','<<motion.local_rotation[1]<<','<<motion.local_rotation[2]<<','<<motion.local_rotation[3]
                    <<" motion_revision=6 motion_contacts="<<motion.contacts
                    <<" motion_clearance="<<motion.clearance[0]<<','<<motion.clearance[1]<<','<<motion.clearance[2];
                message<<" model_calls="<<model_visibility.calls<<" model_owned="<<model_visibility.owned
                    <<" model_local="<<model_visibility.local<<" model_hidden="<<model_visibility.hidden
                    <<" model_unknown="<<model_visibility.unknown<<" model_transitions="<<model_visibility.transitions;
                message<<" item_toggle_enabled="<<item_toggle.enabled<<" item_toggle_active="<<item_toggle.active
                    <<" item_toggle_signals="<<item_toggle.signals<<" item_toggle_count="<<item_toggle.toggles
                    <<" item_toggle_rejected="<<item_toggle.rejected<<" item_toggle_receipt_skipped="<<item_toggle.receipt_skipped
                    <<" item_receipt_available="<<item_receipt_available<<" item_receipt_sequence="<<item_receipt.sequence
                    <<" item_receipt_owner="<<item_receipt.owner_token<<" item_receipt_metadata="
                    <<item_receipt.metadata_10<<','<<item_receipt.metadata_14<<','<<item_receipt.metadata_18<<','<<item_receipt.metadata_1c;
                message<<" anchor_calls="<<anchor.calls<<" anchor_eligible="<<anchor.eligible
                    <<" anchor_relocated="<<anchor.relocated<<" anchor_missing="<<anchor.missing_dummy
                    <<" anchor_invalid="<<anchor.invalid_matrix<<" anchor_thread_reject="<<anchor.rejected_thread
                    <<" anchor_fault="<<anchor.fault;
                message<<" interact_available="<<interact.available<<" interact_held="<<interact.held<<" interact_held_mask=0x"<<std::hex<<interact.held_mask<<std::dec<<" interact_converter_blocked="<<interact.conversion_blocked;
                message<<" light_value="<<effect.light_value<<" visual_test_calls="<<test.calls<<" visual_test_issued="<<test.issued
                    <<" visual_test_fault="<<test.fault<<" delivery_queued="<<delivery.queued<<" delivery_issued="<<delivery.calls_issued
                    <<" delivery_validated="<<delivery.native_validation_true;
                const auto flame=fx::lantern_flame::status();
                message<<" flame_created="<<flame.created<<" flame_removed="<<flame.removed<<" flame_handle="<<flame.handle
                    <<" flame_missing="<<flame.missing_dummy<<" flame_stale="<<flame.stale_handle<<" flame_fault="<<flame.fault<<" lantern_mode=simple-toggle native_ui_hooks=0";
                log_message("lantern-research",message.str().c_str());
            }
            fx::lantern_award::Receipt receipt{};
            while(fx::lantern_award::pop(receipt)) {
                std::ostringstream message; message<<"native_torch_receipt sequence="<<receipt.sequence<<" quantity="<<receipt.quantity
                    <<" before="<<receipt.native_torch_before<<" after="<<receipt.native_torch_after<<" game_thread="<<receipt.game_thread;
                log_message("lantern-research",message.str().c_str());
            }
#endif
            if (fx::stamina_hook::claimed && (number%10)==0) {
                namespace hook=fx::stamina_hook;
                std::ostringstream message;
                message<<"Stamina hook calls="<<hook::calls<<" local="<<hook::local_calls<<" eligible="<<hook::eligible
                    <<" modified="<<hook::modified<<" rejected="<<hook::rejected<<" thread="<<hook::first_thread
                    <<" observer_thread="<<GetCurrentThreadId()<<" thread_fault="<<hook::fault;
                log_message("core",message.str().c_str());
            }
            Sleep(observation_interval);
        }
    } catch (const std::exception& e) {
        stop=true;
        fx::player_states::service().revoke_all();
        jump_observer::disable();
        authorized_hud.store(nullptr);
        fx::hud_replacement::present(0,0);
        fx::stamina_hook::percent=100;
        log_message("core",e.what());
    }
    log_message("core","Observation thread stopped");
    return 0;
}
}

#ifdef DS2FX_LANTERN_RESEARCH
DS2FX_EXPORT uint32_t DS2FX_GetLanternState(DS2FX_LanternState* output) {
    if(!output||output->size!=sizeof(DS2FX_LanternState)||output->version!=DS2FX_LANTERN_STATE_VERSION) return 0;
    *output={sizeof(DS2FX_LanternState),DS2FX_LANTERN_STATE_VERSION};
    fx::lantern_effect::Sample effect{};
    if(stop||!fx::lantern_effect::snapshot(effect)) return 0;
    std::lock_guard lock(state_mutex);
    if(!snapshot.available||snapshot.health<=0||snapshot_player!=effect.owner_token||!snapshot.character_epoch) return 0;
    output->sampled_at_ms=effect.sampled_at_ms;
    output->character_epoch=snapshot.character_epoch;
    output->remaining_seconds=effect.remaining_seconds;
    output->capacity_seconds=effect.capacity_seconds;
    output->active=effect.active;
    return 1;
}
DS2FX_EXPORT uint32_t DS2FX_HasNativeLanternQuickslot() {
#ifdef DS2FX_LANTERN_RESEARCH
    return 0; // Retained ABI: the simple Lantern installs no UI hooks.
#else
    return 0;
#endif
}
DS2FX_EXPORT uint32_t DS2FX_GetLanternCharges(DS2FX_LanternCharges* output) {
    if(!output||output->size!=sizeof(*output)||output->version!=DS2FX_LANTERN_CHARGES_VERSION)return 0;
    *output={sizeof(*output),DS2FX_LANTERN_CHARGES_VERSION};
    return 0; // Charges retired. Export stays present for API2 compatibility.
}
#endif
DS2FX_EXPORT DWORD WINAPI DS2FX_Bootstrap(void*) {
    if (InterlockedCompareExchange(&booted,1,0) != 0) return 0;
    try {
        open_log();
        log_message("core",("Package "+std::string(fx::package_version)+"; API 2 contract "+fx::version).c_str());
        const auto executable = fx::module_path();
#ifdef DS2FX_FIXTURE
        auto fixture = reinterpret_cast<uintptr_t(*)()>(GetProcAddress(GetModuleHandleW(nullptr),"DS2FX_TestBaseSlot"));
        fx::require(fixture != nullptr,"Fixture runtime requires owned fixture host");
        base_slot = fixture();
        log_message("core","FIXTURE BUILD: synthetic host, not evidence of DS2 gameplay");
#else
        fx::require(executable.filename() == L"DarkSoulsII.exe" && fx::hash_file(executable) == fx::game_hash,"Unsupported process/executable hash");
#ifndef DS2FX_SEAMLESS_BRIDGE
        fx::require(!GetModuleHandleW(L"ds2sc.dll"),"Seamless requires the separate experimental bridge");
#endif
        base_slot = find_base_slot();
#endif
        const auto root = fx::module_path(self).parent_path();
        const auto diagnostics=root/L"diagnostics.ini";
        observation_interval=fx::forge::setting(fx::forge::settings_file(),L"ds2fx-core",L"ObservationIntervalMs",100,50,100);
        verbose_lantern_telemetry=fx::forge::setting(fx::forge::settings_file(),L"ds2fx-core",L"VerboseLanternTelemetry",GetPrivateProfileIntW(L"Diagnostics",L"LanternTelemetry",0,diagnostics.c_str()),0,1)==1;
        fx::content::trace_redirects=GetPrivateProfileIntW(L"Diagnostics",L"ContentRedirects",0,diagnostics.c_str())==1;
#ifdef DS2FX_SEAMLESS_BRIDGE
        // Authenticate the vendor before any optional native adapter is prepared.
        fx::bridge::loaded_official(executable.parent_path());
        fx::bridge::select(fx::discover(root / "Plugins")); // Reject altered recognized DLLs before hooks.
#endif
#ifndef DS2FX_FIXTURE
        if(GetPrivateProfileIntW(L"Research",L"ObservePlayerMessages",0,diagnostics.c_str())==1) {
            jump_observer::install(base_slot,true);
            message_observer_installed=true;
            log_message("player-message-observer","Opt-in canonical jump message observer; preserves original calls; no state actions or physics writes");
        }
#endif
#ifdef DS2FX_LANTERN_RESEARCH
        const auto research=root/L"lantern-research.ini";
        const bool light_anchor=GetPrivateProfileIntW(L"Research",L"HipLightAnchor",0,research.c_str())==1;
        const bool pivot_motion=GetPrivateProfileIntW(L"Research",L"LanternPhysics",0,research.c_str())==1;
        const bool local_model=GetPrivateProfileIntW(L"Research",L"LocalLanternModel",0,research.c_str())==1;
        const bool observe_pose=pivot_motion||GetPrivateProfileIntW(L"Research",L"ObserveLanternPose",0,research.c_str())==1;
        const bool bonfire_delivery=GetPrivateProfileIntW(L"Research",L"BonfireLanternDelivery",0,research.c_str())==1;
        const bool observe_bonfire=bonfire_delivery||GetPrivateProfileIntW(L"Research",L"ObserveBonfire",0,research.c_str())==1;
        fx::lantern_runtime::Settings lantern_settings{};
        lantern_settings.bonfire_delivery=bonfire_delivery;
        lantern_settings.companion_oil=GetPrivateProfileIntW(L"Research",L"CompanionOil",0,research.c_str())==1;
        lantern_settings.visual_effect_test=GetPrivateProfileIntW(L"Research",L"VisualEffectTest",0,research.c_str())==1;
        lantern_settings.observe_interact=GetPrivateProfileIntW(L"Research",L"ObserveInteract",0,research.c_str())==1;
        lantern_settings.quick_slot_toggle=GetPrivateProfileIntW(L"Research",L"QuickSlotLantern",0,research.c_str())==1;
        lantern_settings.test_save_slot=static_cast<int32_t>(GetPrivateProfileIntW(L"Research",L"TestSaveSlot",0xffffffffu,research.c_str()));
        lantern_settings.test_delay_seconds=static_cast<float>(GetPrivateProfileIntW(L"Research",L"TestDelaySeconds",20,research.c_str()));
        if(lantern_settings.companion_oil||lantern_settings.visual_effect_test||lantern_settings.observe_interact||lantern_settings.quick_slot_toggle||observe_bonfire||light_anchor||observe_pose||local_model) {
#ifdef DS2FX_SEAMLESS_LANTERN
            fx::require(!lantern_settings.companion_oil&&!lantern_settings.visual_effect_test&&!lantern_settings.observe_interact,
                "Seamless Lantern permits only the reusable quick-slot and local bonfire route");
            fx::require(GetPrivateProfileIntW(L"Research",L"ObserveTick",0,research.c_str())==1,"Seamless Lantern requires its guarded native tick");
#else
            fx::require(GetPrivateProfileIntW(L"Research",L"BlockNetwork",0,research.c_str())==1 &&
                GetPrivateProfileIntW(L"Research",L"ObserveTick",0,research.c_str())==1,"Lantern engine actions require guarded native tick and offline initialization");
#endif
            // Companion grants and fixed ItemLot companions must never be combined.
            if(bonfire_delivery) {
                wchar_t route[64]{},lot[32]{},starter[32]{};
                const auto metadata=root/L"Content"/L"wayfarers-lantern"/L"forge.ini";
                GetPrivateProfileStringW(L"LanternFixture",L"LanternDelivery",L"",route,64,metadata.c_str());
                GetPrivateProfileStringW(L"LanternFixture",L"LanternPickupLot",L"",lot,32,metadata.c_str());
                GetPrivateProfileStringW(L"LanternFixture",L"LanternStarterGrant",L"",starter,32,metadata.c_str());
                fx::require(_wcsicmp(route,L"ds2fx-bonfire-missing-item")==0&&_wcsicmp(lot,L"none")==0&&_wcsicmp(starter,L"false")==0,
                    "Bonfire delivery requires the audited pack with Lantern corpse/starter grants removed");
                fx::require(!lantern_settings.companion_oil&&!lantern_settings.visual_effect_test,"Bonfire Lantern delivery cannot enable retired test/Oil grant routes");
            }
            if(lantern_settings.companion_oil) {
                wchar_t paired[32]{};
                const auto metadata=root/L"Content"/L"wayfarers-lantern"/L"forge.ini";
                GetPrivateProfileStringW(L"LanternFixture",L"OilAwardHookRequired",L"",paired,32,metadata.c_str());
                fx::require(_wcsicmp(paired,L"True")==0,"Companion oil requires a pack with fixed oil companions removed");
            }
        }
#ifndef DS2FX_SEAMLESS_LANTERN
        if(GetPrivateProfileIntW(L"Research",L"ObserveTick",0,research.c_str())==1 ||
           GetPrivateProfileIntW(L"Research",L"ObserveTorchAwards",0,research.c_str())==1 ||
           GetPrivateProfileIntW(L"Research",L"ObserveOilEffect",0,research.c_str())==1 || observe_bonfire)
            fx::require(GetPrivateProfileIntW(L"Research",L"BlockNetwork",0,research.c_str())==1,"Lantern research requires its process-local offline guard");
        if(GetPrivateProfileIntW(L"Research",L"BlockNetwork",0,research.c_str())==1) {
            fx::offline_network::install();
            log_message("lantern-research","Process-local WSAStartup guard installed before game entrypoint; offline sockets only");
        }
#else
        fx::require(GetPrivateProfileIntW(L"Research",L"ObserveTorchAwards",0,research.c_str())==0,
            "Seamless Lantern cannot enable the retired Torch award probe");
        log_message("lantern-research","Seamless local Lantern adapters enabled; no WSAStartup guard; remote Lantern state is not replicated");
#endif
        fx::lantern_runtime::prepare(base_slot,lantern_settings);
        if(bonfire_delivery) log_message("lantern-research","Missing Lantern delivery enabled: authenticated local bonfire receipt, inventory/storage check, later native tick; no automatic equip");
        if(GetPrivateProfileIntW(L"Research",L"LanternFlame",0,research.c_str())==1) {
            fx::require(lantern_settings.quick_slot_toggle,"Owned flame requires the authenticated reusable Lantern toggle");
            fx::lantern_flame::prepare(base_slot,true);
            log_message("lantern-research","Owned compact flame6042 enabled at animated wick247; native SFX handle lifecycle");
        }
        if(local_model) {
            fx::lantern_model_visibility::install(base_slot,true);
            log_message("lantern-research","Owned base lantern visibility filter installed; exact nonlocal lamp draws only, unknown models retain native draw");
        }
        if(light_anchor) {
            if(lantern_settings.quick_slot_toggle)fx::lantern_light_selector::install(base_slot,true);
            fx::lantern_light_anchor::install(base_slot,true);
            fx::lantern_world_light::configure_presentation(
                fx::forge::setting(fx::forge::settings_file(),L"wayfarers-lantern",L"LanternFlickerPercent",GetPrivateProfileIntW(L"Research",L"LanternFlickerPercent",0,research.c_str()),0,12),
                fx::forge::setting(fx::forge::settings_file(),L"wayfarers-lantern",L"LanternSpecularPercent",GetPrivateProfileIntW(L"Research",L"LanternSpecularPercent",100,research.c_str()),0,100),
                fx::forge::setting(fx::forge::settings_file(),L"wayfarers-lantern",L"LanternDiffusePercent",GetPrivateProfileIntW(L"Research",L"LanternDiffusePercent",100,research.c_str()),0,100),
                fx::forge::setting(fx::forge::settings_file(),L"wayfarers-lantern",L"LanternRadiusPercent",GetPrivateProfileIntW(L"Research",L"LanternRadiusPercent",100,research.c_str()),100,150));
            fx::lantern_world_light::install(base_slot,true,read_lantern_brightness);
            log_message("lantern-research","Opt-in hip light anchor installed for owned light51 and wick dummy247");
            log_message("lantern-research","Owned native point-light adapter installed; exact supported LE brightness observed read-only");
        }
        if(observe_pose) {
            if(pivot_motion) {
                fx::require(fx::lantern_pivot_motion::prepare(base_slot,true),"Cannot prepare owned lantern pivot motion");
                fx::lantern_pose_observer::set_owned_pose_handler(fx::lantern_pivot_motion::on_owned_pose);
            }
            fx::lantern_pose_observer::install(base_slot,true);
            log_message("lantern-research",pivot_motion?
                "Canonical lantern pivot motion revision6 installed; soft contact, bounded gravity suspension and hitch recovery; owned leaf bone only":
                "Read-only canonical lantern pivot pose observer installed; no pose setters or motion writes");
        }
        if(lantern_settings.observe_interact) log_message("lantern-research","Native logical Interact observer enabled; telemetry only, no lantern action dispatch");
        if(lantern_settings.quick_slot_toggle) log_message("lantern-research","Reusable Lantern quick-slot event toggle enabled; no input hook or inventory write");
        if(lantern_settings.visual_effect_test) log_message("lantern-research","VISUAL TEST DRIVER enabled for configured save slot; native effect application only, no oil consumption");
        if(GetPrivateProfileIntW(L"Research",L"ObserveTick",0,research.c_str())==1) {
            fx::lantern_probe::install(base_slot,true);
            log_message("lantern-research","Opt-in native update observation installed; no engine actions or oil resource writes");
        }
        if(GetPrivateProfileIntW(L"Research",L"ObserveTorchAwards",0,research.c_str())==1) {
            fx::lantern_award::install(base_slot,true);
            log_message("lantern-research","Opt-in native torch award observation installed; receipts only, no oil grant");
        }
        if(lantern_settings.quick_slot_toggle||GetPrivateProfileIntW(L"Research",L"ObserveOilEffect",0,research.c_str())==1) {
            fx::lantern_effect::install(base_slot,true);
            log_message("lantern-research","Exact active-item instruction observer installed; unlimited Lantern uses its owned zero-duration receipt");
        }
        if(observe_bonfire) {
            fx::lantern_bonfire::install(base_slot,true);
            log_message("lantern-research","Opt-in local BonfireRest special-effect observer installed; receipts only, no custom-resource refill");
        }
#endif
        auto modules = fx::discover(root / "Plugins");
#ifdef DS2FX_SEAMLESS_BRIDGE
        fx::bridge::loaded_official(executable.parent_path());
#ifdef DS2FX_AUTO_SEAMLESS
        const auto all=modules;
        modules=fx::bridge::select(all);
        for(const auto& m:all) if(std::none_of(modules.begin(),modules.end(),[&](const auto& active){return active.id==m.id;}))
            log_message("core",("Seamless mode skipped offline-only module: "+m.id).c_str());
#else
        fx::bridge::modules(modules);
#endif
        log_message("core",(std::string("SEAMLESS BRIDGE ")+fx::bridge::name+"; pinned modules and local Lantern adapters; gameplay policy API disabled; peer replication unimplemented").c_str());
#endif
        for (const auto& m : modules) {
            // Restrict dependency DLL search to this module's directory and System32.
            const auto library = LoadLibraryExW((m.directory / "plugin.dll").c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
            fx::require(library != nullptr,"Plugin LoadLibrary failed: " + m.id + " Win32=" + std::to_string(GetLastError()));
            auto entry = reinterpret_cast<DS2FX_InitializePluginFn>(GetProcAddress(library,"DS2FX_InitializePlugin"));
            fx::require(entry != nullptr,"Plugin missing DS2FX_InitializePlugin: " + m.id);
            DS2FX_Plugin plugin{};
            plugin.size = sizeof(plugin);
#ifndef DS2FX_SEAMLESS_BRIDGE
            fx::stamina_hook::accepting=true;
#endif
            initializing_state_thread.store(GetCurrentThreadId());
            initializing_state_owner.store(library);
            const auto initialized=initialize_guarded(entry,&plugin);
            initializing_state_owner.store(nullptr);
            initializing_state_thread.store(0);
            fx::stamina_hook::accepting=false;
            fx::require(initialized == 1,"Plugin initialization failed: " + m.id);
            fx::require(plugin.size == sizeof(plugin) && plugin.api_version == DS2FX_API_VERSION && plugin.id && plugin.version && plugin.on_event,"Invalid plugin API response: " + m.id);
            fx::require(m.id == plugin.id && m.version == plugin.version,"Plugin identity differs from manifest: " + m.id);
            fx::player_states::service().commit(reinterpret_cast<uintptr_t>(library));
            loaded.push_back({library,plugin,m.id});
            log_message("core",("Loaded " + m.id + " " + m.version).c_str());
        }
        log_message("player-states","Owner-scoped state service available; generic module executor unavailable; built-in jump selector configured separately");
        const auto new_hud=std::find_if(loaded.begin(),loaded.end(),[](const Loaded& m) {
            return m.id=="ds2-remastered-hud" &&
                (std::string(m.plugin.version)=="0.1.6" || std::string(m.plugin.version)=="0.1.7" || std::string(m.plugin.version)=="0.1.8" || std::string(m.plugin.version)=="0.1.9" || std::string(m.plugin.version)=="0.1.10" || std::string(m.plugin.version)=="0.1.11" || std::string(m.plugin.version)=="0.1.12" || std::string(m.plugin.version)=="0.1.13" || std::string(m.plugin.version)=="0.1.14" || std::string(m.plugin.version)=="0.1.15" || std::string(m.plugin.version)=="0.1.16" || std::string(m.plugin.version)=="0.1.17" || std::string(m.plugin.version)=="0.1.18" || std::string(m.plugin.version)=="0.1.19" || std::string(m.plugin.version)=="0.1.20" || std::string(m.plugin.version)=="0.1.21" || std::string(m.plugin.version)=="0.1.22" || std::string(m.plugin.version)=="0.1.23" || std::string(m.plugin.version)=="0.1.24" || std::string(m.plugin.version)=="0.1.25" || std::string(m.plugin.version)=="0.1.26" || std::string(m.plugin.version)=="0.1.27" || std::string(m.plugin.version)=="0.1.28");
        });
        if(new_hud!=loaded.end()) {
#ifndef DS2FX_FIXTURE
            try {
                fx::hud_replacement::install(base_slot);
                authorized_hud.store(new_hud->library);
                log_message("core","Selective HUD draw adapter installed; independent bars, covenant and effects renderer leases required");
            } catch(const std::exception& e) {
                log_message("core",("HUD draw adapter unavailable; native drawing retained: "+std::string(e.what())).c_str());
            }
#endif
        }
#ifndef DS2FX_SEAMLESS_BRIDGE
        if(fx::stamina_hook::claimed) {
            fx::stamina_hook::install(base_slot);
            log_message("core","Stamina call-site hook installed; original code owns writes; no general engine-thread scheduler");
        }
#endif
        fx::content::install(root,log_message);
#if defined(DS2FX_LANTERN_RESEARCH) && !defined(DS2FX_FIXTURE)
        const auto jump_config=root/L"jump-states.ini";
        if(GetPrivateProfileIntW(L"JumpStates",L"Enabled",0,jump_config.c_str())==1) {
            const auto asset=fx::content::routes.find(L"morpheme4/chr/c0001.anibnd.dcx");
            // Require the exact independently audited full network/animation
            // archive, already selected by the actual content resolver.
            if(asset==fx::content::routes.end() ||
               asset->second.hash!="2118b0ebafe1a5a30007933d0f367e07fe41947782798428cc1726d8e82fe1b1") {
                log_message("player-jump-states","Disabled: exact three-jump content archive is not selected");
            } else {
                // Core installs the native update authority even without a
                // Lantern module. Reuse the existing hook if already installed.
                if(!fx::lantern_probe::status().installed) fx::lantern_probe::install(base_slot,true);
                if(!message_observer_installed) {
                    jump_observer::install(base_slot,true);message_observer_installed=true;
                }
                jump_observer::configure_jump_routes(true,[]() noexcept {
                    const auto tick=fx::lantern_probe::status();
                    return tick.installed && tick.observing && !tick.thread_fault &&
                        tick.first_thread && tick.first_thread==GetCurrentThreadId() && !stop.load();
                });
                log_message("player-jump-states","Experimental native selector active: idle9, walking59, sprint8; original dispatch once; no input/physics writes; peer gameplay unverified");
            }
        }
#endif
#ifndef DS2FX_FIXTURE
        auto forge_mods=fx::content::active_mods;
        for(const auto& m:modules) {
            if(std::any_of(forge_mods.begin(),forge_mods.end(),[&](const auto& item){return item.id==m.id;}))continue;
            fx::forge::Mod entry;
            entry.id=m.id;entry.version=m.version;entry.hash=m.hash;
            entry.name=fx::utf8_path(m.id).wstring();
            entry.description=L"DS2FX native module. See the module documentation for its configuration.";
            entry.credits=L"Author and license: see the installed module documentation.";
            entry.status=L"Active / native module";forge_mods.push_back(std::move(entry));
        }
        fx::forge::Mod core_entry;core_entry.id="ds2fx-core";core_entry.version=fx::package_version;
        core_entry.name=L"DS2FX Core";core_entry.status=L"Active / shared runtime";
        core_entry.description=L"Observation and diagnostics. Lower intervals increase sampling work; 100ms is the efficient default.";
        forge_mods.push_back(std::move(core_entry));
        const bool with_seamless=GetModuleHandleW(L"ds2sc.dll")!=nullptr;
        if(!fx::forge::start(self,root,std::move(forge_mods),with_seamless,log_message))
            log_message("forge","UI unavailable; gameplay content remains active. See startup diagnostics.");
#endif
        worker = CreateThread(nullptr,0,observe,nullptr,0,nullptr);
        fx::require(worker != nullptr,"Cannot start observation thread");
        log_message("core",("READY modules=" + std::to_string(loaded.size()) + "; experimental; "+std::to_string(observation_interval)+"ms observed transitions; stamina cost percent="+std::to_string(fx::stamina_hook::percent.load())).c_str());
        return 1;
    } catch (const std::exception& e) {
        initializing_state_owner.store(nullptr);
        initializing_state_thread.store(0);
        fx::player_states::service().revoke_all();
        jump_observer::disable();
        authorized_hud.store(nullptr);
        fx::hud_replacement::present(0,0);
#ifdef DS2FX_LANTERN_RESEARCH
        // A vendor conflict may reject a later hook after earlier hooks exist.
        // Retain resident trampolines, but remove all feature authority.
        fx::lantern_world_light::disable(); fx::lantern_light_selector::disable();
        fx::lantern_flame::disable(); fx::lantern_model_visibility::disable();
        fx::lantern_pivot_motion::disable(); fx::lantern_pose_observer::set_owned_pose_handler(nullptr);
        fx::lantern_pose_observer::disable(); fx::lantern_probe::disable();
        fx::lantern_award::disable(); fx::lantern_effect::disable(); fx::lantern_bonfire::disable();
        fx::lantern_light_anchor::disable(); fx::lantern_runtime::disable();
#endif
        fx::graphics::shutdown();
        log_message("core",("BOOT FAILED: " + std::string(e.what())).c_str()); return 0;
    }
}

// Stops callbacks; DLLs remain resident until process exit. Never call from a plugin callback.
DS2FX_EXPORT DWORD WINAPI DS2FX_Stop(void*) {
    stop = true;
    fx::graphics::shutdown();
    fx::player_states::service().revoke_all();
    jump_observer::disable();
    authorized_hud.store(nullptr);
    fx::hud_replacement::present(0,0);
#ifdef DS2FX_LANTERN_RESEARCH
    fx::lantern_world_light::disable();
    fx::lantern_light_selector::disable();
    fx::lantern_flame::disable();
    fx::lantern_model_visibility::disable();
    fx::lantern_pivot_motion::disable();
    fx::lantern_pose_observer::set_owned_pose_handler(nullptr);
    fx::lantern_pose_observer::disable();
    fx::lantern_probe::disable();
    fx::lantern_award::disable();
    fx::lantern_effect::disable();
    fx::lantern_bonfire::disable();
    fx::lantern_light_anchor::disable();
    fx::lantern_runtime::disable();
#endif
#ifndef DS2FX_FIXTURE
    if(!fx::forge::shutdown()) return 0;
#endif
    fx::stamina_hook::percent=100;
    if (worker) { if (WaitForSingleObject(worker,5000) != WAIT_OBJECT_0) return 0; CloseHandle(worker); worker = nullptr; }
    return 1;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { self = module; DisableThreadLibraryCalls(module); }
    return TRUE;
}
