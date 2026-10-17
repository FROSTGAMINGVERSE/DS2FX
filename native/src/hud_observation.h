#pragma once
#include "../sdk/ds2fx_api.h"
#include <cstdint>

namespace fx::hud_observation {
template<class Read>
DS2FX_PlayerForm sample_form(uintptr_t base_slot,uintptr_t expected_player,
                            const DS2FX_PlayerState& state,Read read) {
    DS2FX_PlayerForm out{sizeof(out)};
    out.sample=state.sample;out.character_epoch=state.character_epoch;
    if(!state.available || !state.character_epoch || !expected_player)return out;
    uintptr_t base=0,player=0,param=0,again=0;
    uint8_t level=255,check=255;
    if(!read(base_slot,base)||!base || !read(base+0xd0,player)||player!=expected_player ||
       !read(player+0x490,param)||!param || !read(param+0x1ac,level)||level>32 ||
       !read(param+0x1ac,check)||check!=level ||
       !read(player+0x490,again)||again!=param ||
       !read(base+0xd0,again)||again!=player || !read(base_slot,again)||again!=base)return out;
    out.available=1;out.hollow_level=level;out.human=level==0;
    return out;
}
// The caller provides a guarded, read-only memory reader. Every exposed field
// is independently valid and the canonical player chain is checked again at
// the end. This runs on the observation worker, never the game thread.
template<class Read>
DS2FX_HudState sample(uintptr_t base_slot,uintptr_t expected_player,
                      const DS2FX_PlayerState& player_state,Read read) {
    DS2FX_HudState result{sizeof(DS2FX_HudState)};
    result.sample=player_state.sample;
    result.character_epoch=player_state.character_epoch;
    if(!player_state.available || !expected_player) return result;
    uintptr_t base=0,player=0,base_after=0,player_after=0;
    if(!read(base_slot,base) || !base || !read(base+0xd0,player) ||
       player!=expected_player) return result;
    result.player_available=1;
    // DS2S-META Scholar: GameState at BaseA+0x24ac, LoadingState via
    // BaseA+0x80 -> +0x8 -> +0xbb4. Both changed across two live bonfire
    // transitions on 2026-09-30. Publish only a stable, fully read pair.
    uintptr_t scene_root=0,scene_leaf=0,root_after=0,leaf_after=0;
    int32_t game_state=0,state_after=0;uint8_t loading=2,loading_after=2;
    if(read(base+0x24ac,game_state) && game_state>=0 && game_state<=64 &&
       read(base+0x80,scene_root) && scene_root &&
       read(scene_root+0x8,scene_leaf) && scene_leaf &&
       read(scene_leaf+0xbb4,loading) && loading<=1 &&
       read(base+0x24ac,state_after) && state_after==game_state &&
       read(base+0x80,root_after) && root_after==scene_root &&
       read(scene_root+0x8,leaf_after) && leaf_after==scene_leaf &&
       read(scene_leaf+0xbb4,loading_after) && loading_after==loading) {
        result.game_state=game_state;result.loading=loading;
        result.available_fields|=DS2FX_HUD_SCENE;
    }
    // DS2S-META Scholar PlayerParam: PlayerCtrl+0x490 -> Souls+0xec,
    // CurrentCovenant+0x1ad.
    uintptr_t param=0,param_after=0;
    if(read(player+0x490,param) && param) {
        int32_t souls=0;uint8_t covenant=0;
        const bool souls_read=read(param+0xec,souls);
        const bool covenant_read=read(param+0x1ad,covenant);
        if(read(player+0x490,param_after) && param_after==param) {
            if(souls_read && souls>=0 && souls<=999999999) {
                result.souls=static_cast<uint32_t>(souls);result.available_fields|=DS2FX_HUD_SOULS;
            }
            if(covenant_read && covenant<=32) {
                result.covenant_id=covenant;result.available_fields|=DS2FX_HUD_COVENANT;
            }
        }
    }
    // Bob Edition Scholar table: PlayerCtrl+0x378 -> equipment; active
    // hand indices +0x5a4/+0x5a8, IDs at 0x9c/0xc4/0xec and
    // 0xb0/0xd8/0x100. Each hand can fail independently.
    uintptr_t equipment=0,equipment_after=0;
    if(read(player+0x378,equipment) && equipment) {
        int32_t left_index=-1,right_index=-1,left_id=-1,right_id=-1;
        const bool left_read=read(equipment+0x5a4,left_index) && left_index>=0 && left_index<3 &&
            read(equipment+0x9c+static_cast<uintptr_t>(left_index)*0x28,left_id);
        const bool right_read=read(equipment+0x5a8,right_index) && right_index>=0 && right_index<3 &&
            read(equipment+0xb0+static_cast<uintptr_t>(right_index)*0x28,right_id);
        if(read(player+0x378,equipment_after) && equipment_after==equipment) {
            if(left_read && left_id>=-1 && left_id<=999999999) {
                result.left_hand_item_id=left_id;result.available_fields|=DS2FX_HUD_LEFT_HAND;
            }
            if(right_read && right_id>=-1 && right_id<=999999999) {
                result.right_hand_item_id=right_id;result.available_fields|=DS2FX_HUD_RIGHT_HAND;
            }
        }
    }
    if(!read(base_slot,base_after) || base_after!=base ||
       !read(base+0xd0,player_after) || player_after!=player) {
        result.player_available=0;result.available_fields=0;
    }
    return result;
}
}
