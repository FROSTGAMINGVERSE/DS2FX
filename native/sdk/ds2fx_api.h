#pragma once
#include <stdint.h>
#include <stddef.h>
#include "ds2fx_player_states.h"

/* DS2FX native ABI 2. Windows x64; no ownership crosses DLLs.
   Callbacks run serially on the DS2FX observation thread, NOT the game thread.
   Do not block, throw, retain event pointers, unload yourself or call engine functions.
   Native plugins are trusted code, not sandboxed. No multiplayer certification. */
#define DS2FX_API_VERSION 2u
#define DS2FX_EXPORT extern "C" __declspec(dllexport)

enum DS2FX_Event : uint32_t {
    DS2FX_PLAYER_AVAILABLE = 1,
    DS2FX_PLAYER_UNAVAILABLE = 2,
    DS2FX_HEALTH_CHANGED = 3,
    DS2FX_TICK = 4,
    DS2FX_STAMINA_CHANGED = 5,
    DS2FX_PLAYER_DIED = 6,
    DS2FX_PLAYER_REVIVED = 7
};
struct DS2FX_PlayerState {
    uint32_t size;
    uint32_t available;
    uint64_t sample;
    int32_t health;
    int32_t max_health;
    /* Monotonic observed character generation; never a game pointer. */
    uint64_t character_epoch;
    /* Stamina can be negative during exhaustion. Check stamina_available. */
    uint32_t stamina_available;
    float stamina;
    float max_stamina;
    uint32_t reserved;
};
/* Optional, read-only HUD observation. A bit in available_fields is the only
   authority for using that field; zero is a valid souls/covenant value.
   Item IDs are game IDs, not icon pixels or pointers. No game object escapes. */
enum DS2FX_HudField : uint32_t {
    DS2FX_HUD_SOULS = 1u << 0,
    DS2FX_HUD_COVENANT = 1u << 1,
    DS2FX_HUD_LEFT_HAND = 1u << 2,
    DS2FX_HUD_RIGHT_HAND = 1u << 3,
    DS2FX_HUD_QUICK_ITEM = 1u << 4,
    DS2FX_HUD_SPELL = 1u << 5,
    /* Scholar scene state and loading flag, validated as one read-only pair. */
    DS2FX_HUD_SCENE = 1u << 6
};
struct DS2FX_HudState {
    uint32_t size;
    uint32_t player_available;
    uint64_t sample;
    uint64_t character_epoch;
    uint32_t available_fields;
    uint32_t souls;
    uint32_t covenant_id;
    int32_t left_hand_item_id;
    int32_t right_hand_item_id;
    int32_t quick_item_id;
    int32_t quick_item_count;
    int32_t spell_id;
    int32_t spell_uses;
    /* A scene is playable only when game_state==30 and loading==0. */
    int32_t game_state;
    uint32_t loading;
};
/* Separate ABI tail query: existing HudState sizes remain unchanged.
   Read-only native health HUD presentation, shared by the replacement HUD.
   Unknown state must hide. opacity is the game's alpha byte, 0..255. */
struct DS2FX_HudPresentation {
    uint32_t size;
    uint32_t available;
    uint64_t sample;
    uint64_t character_epoch;
    uint32_t opacity;
    uint32_t reserved;
    /* Optional sized tail. The original 32-byte prefix remains supported. */
    uint32_t gauge_available;
    float health_length;
    float stamina_length;
};
/* Local presentation only. Original game icon keys, in native slot order.
   No pointers, effect inference or gameplay writes. Missing fields pass native
   drawing through. tint is native RGBA (R in the low byte); opacity 0..255. */
enum DS2FX_HudRegion : uint32_t {
    DS2FX_HUD_REGION_BARS=1u, DS2FX_HUD_REGION_COVENANT=2u,
    DS2FX_HUD_REGION_EFFECTS=4u
};
struct DS2FX_HudIcon {
    uint32_t type,id,tint,opacity;
};
struct DS2FX_HudIcons {
    uint32_t size,available_regions;
    uint64_t sample,character_epoch;
    DS2FX_HudIcon covenant;
    uint32_t effect_count,effect_opacity;
    DS2FX_HudIcon effects[20];
};
/* Read-only local character form. Separate tail query preserves every prior
   structure size. Hollow level is native PlayerParam+1ac, range 0..32;
   zero means full Human, positive means Hollow. Unknown is never Human. */
struct DS2FX_PlayerForm {
    uint32_t size,available;
    uint64_t sample,character_epoch;
    uint32_t hollow_level,human;
};
#include "ds2fx_graphics.h"
struct DS2FX_Api {
    uint32_t size;
    uint32_t version;
    const char* core_version;
    void (*log)(const char* module_id, const char* message);
    /* Returns 1 for an available snapshot, 0 when absent; copies to caller. */
    uint32_t (*get_player_state)(DS2FX_PlayerState* output);
    /* Initialization only, one owner. 10..100 percent. 100 is observe-only.
       Core filters reason=0 positive local-player stamina costs at the engine call.
       No plugin callbacks run inside that hook. Not a general engine task API. */
    uint32_t (*register_stamina_cost_policy)(uint32_t percent);
    /* ABI 2 tail extension. Older plugins use the original size prefix.
       New plugins must check host->size before reading this pointer. */
    uint32_t (*get_hud_state)(DS2FX_HudState* output);
    uint32_t (*get_hud_presentation)(DS2FX_HudPresentation* output);
    /* Client-local draw lease for the authenticated Remastered HUD module.
       visible=1 renews only after a successful replacement present;
       visible=0 revokes immediately. An absent/rejected capability returns 0. */
    uint32_t (*update_hud_replacement)(uint64_t character_epoch,uint32_t visible);
    uint32_t (*get_hud_icons)(DS2FX_HudIcons* output);
    /* Entire desired mask after a successful present; omitted regions revoke.
       A stale observation cannot acquire additional regions. */
    uint32_t (*update_hud_regions)(uint64_t character_epoch,uint64_t sample,uint32_t regions);
    /* Same transaction with explicit granted bits. Partial optional-region
       failure does not revoke healthy regions. The caller removes ungranted
       pixels immediately; a missing BARS grant still hides the entire frame.
       Appended capability retains the previous Boolean-return API contract.
       The in-game compositor validates/upload resources on its render callback,
       requests this mask immediately before submitting only granted layers,
       and revokes every lease if submission/device validation fails. */
    uint32_t (*update_hud_region_mask)(uint64_t character_epoch,uint64_t sample,uint32_t regions);
    uint32_t (*get_player_form)(DS2FX_PlayerForm* output);
    /* ABI 2 state-service tail. Registration during Initialize only; handles
       belong to the calling DLL. OK means queued, never already executed.
       Query executor_available before offering a gameplay feature. */
    uint32_t (*register_player_state)(const DS2FX_StateDefinition* definition);
    uint32_t (*request_player_state)(const DS2FX_StateRequest* request);
    uint32_t (*get_player_state_service)(DS2FX_StateServiceStatus* output);
    uint32_t (*register_player_jump_routes)(const DS2FX_JumpRoutes* routes);
    /* Core-owned optional D3D11 render bridge. Register during InitializePlugin.
       Check size before using these ABI 2 tail fields. Handle 0 means unavailable.
       Unregister is owner-scoped and waits for current callbacks to return.
       Registration does not grant native HUD suppression or gameplay authority. */
    uint64_t (*register_graphics_callback)(const DS2FX_GraphicsCallback* callback);
    uint32_t (*unregister_graphics_callback)(uint64_t handle);
};
static_assert(offsetof(DS2FX_Api,get_hud_state)==40, "ABI 2 prefix changed");
static_assert(offsetof(DS2FX_HudPresentation,gauge_available)==32, "Native presentation prefix changed");
static_assert(offsetof(DS2FX_Api,get_hud_presentation)==48, "ABI 2 HUD prefix changed");
static_assert(offsetof(DS2FX_Api,update_hud_replacement)==56, "ABI 2 HUD lease tail changed");
static_assert(offsetof(DS2FX_Api,get_hud_icons)==64, "ABI 2 icons tail changed");
static_assert(offsetof(DS2FX_Api,update_hud_regions)==72, "ABI 2 regions tail changed");
static_assert(offsetof(DS2FX_Api,update_hud_region_mask)==80, "ABI 2 region mask tail changed");
static_assert(offsetof(DS2FX_Api,get_player_form)==88, "ABI 2 form tail changed");
static_assert(offsetof(DS2FX_Api,register_player_state)==96, "ABI 2 state tail changed");
static_assert(offsetof(DS2FX_Api,request_player_state)==104);
static_assert(offsetof(DS2FX_Api,get_player_state_service)==112);
static_assert(offsetof(DS2FX_Api,register_player_jump_routes)==120);
static_assert(offsetof(DS2FX_Api,register_graphics_callback)==128);
static_assert(offsetof(DS2FX_Api,unregister_graphics_callback)==136);
struct DS2FX_Plugin {
    uint32_t size;
    uint32_t api_version;
    const char* id;
    const char* version;
    void (*on_event)(uint32_t event, const DS2FX_PlayerState* state);
};
/* Export this from a plugin. Return 1 on success; 0 refuses initialization. */
typedef uint32_t (*DS2FX_InitializePluginFn)(const DS2FX_Api*, DS2FX_Plugin*);
