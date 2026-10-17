#include "../../src/common.h"
#include "../../src/hud_icons.h"
#include "../../src/hud_observation.h"
#include <iostream>
int wmain(int argc,wchar_t** argv) {try {
    fx::require(argc==3 || argc==4,"hud_icons_probe <pid> <image base> [sample count]");
    DWORD pid=std::stoul(argv[1]);uintptr_t image=std::stoull(argv[2],nullptr,0);
    fx::Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|PROCESS_VM_READ,FALSE,pid));
    fx::require(process.value!=nullptr,"Read-only process access failed");
    wchar_t path[32768]{};DWORD length=32768;
    fx::require(QueryFullProcessImageNameW(process.value,0,path,&length) && fx::hash_file(path)==fx::game_hash,"Unsupported game executable");
    const auto read=[&](uintptr_t address,auto& value) {
        SIZE_T got=0;return address>=0x10000 && ReadProcessMemory(process.value,reinterpret_cast<void*>(address),&value,sizeof(value),&got) && got==sizeof(value);
    };
    uintptr_t base=0,player=0;fx::require(read(image+0x16148f0,base)&&base&&read(base+0xd0,player)&&player,"No canonical player");
    DS2FX_PlayerState state{sizeof(state),1,1,1,1};state.character_epoch=1;
    if(argc==4) {
        unsigned failures=0,count=std::stoul(argv[3]);fx::require(count<=1200,"Too many samples");
        for(unsigned i=0;i<count;++i) {
            fx::hud_icons::SampleDiagnostics diagnostic{};
            auto observed=fx::hud_icons::sample(image,image+0x16148f0,player,state,read,&diagnostic);
            if(!(observed.available_regions&DS2FX_HUD_REGION_EFFECTS)) {
                ++failures;
                if(failures<8) {
                    std::cout<<"sample="<<i<<" mask="<<observed.available_regions<<" stage="<<diagnostic.stage<<" row="<<diagnostic.row<<" hidden="<<diagnostic.hidden<<" disabled="<<diagnostic.disabled;
                    fx::hud_icons::EffectsIdentity identity{};
                    if(diagnostic.stage==5 && fx::hud_icons::identify_effects(image,image+0x16148f0,player,identity,read)) {
                        const auto leaf=identity.leaves[diagnostic.row];uintptr_t provider=0,texture=0,definition=0,colors=0,table=0;
                        uint32_t type=0,id=0,width=0,height=0;uint16_t definitions=0;uint8_t selected=0;
                        read(leaf+0x68,provider);read(provider,table);read(provider+0x18,type);read(provider+0x1c,id);
                        read(provider+0x10,texture);read(texture+0x40,width);read(texture+0x44,height);
                        read(leaf+0x40,definition);read(definition+2,definitions);read(leaf+0x7a,selected);read(leaf+0x60,colors);
                        std::cout<<" provider_table="<<std::hex<<table-image<<std::dec<<" key="<<type<<":"<<id<<" size="<<width<<"x"<<height<<" definitions="<<definitions<<" selected="<<unsigned(selected)<<" colors="<<colors;
                    }
                    std::cout<<std::endl;
                }
            }
            Sleep(50);
        }
        std::cout<<"samples="<<count<<" failures="<<failures<<'\n';return 0;
    }
    auto icons=fx::hud_icons::sample(image,image+0x16148f0,player,state,read);
    const auto hud=fx::hud_observation::sample(image+0x16148f0,player,state,read);
    std::cout<<"player covenant="<<hud.covenant_id<<" fields="<<hud.available_fields<<'\n';
    fx::hud_replacement::Identity hp{};
    const bool owned=fx::hud_replacement::identify(image,image+0x16148f0,player,hp,read);
    uintptr_t leaf=0;
    std::cout<<"health_owned="<<owned<<" covenant_leaf="<<(owned && fx::hud_icons::covenant_leaf(image,hp.targets[11],leaf,0,read))<<'\n';
    if(owned) {
        uintptr_t node=hp.targets[11];
        for(unsigned depth=0;node && depth<8;++depth) {
            uintptr_t table=0,child=0,parent=0,sibling=0;
            read(node,table);read(node+0x38,child);read(child+0x18,parent);read(child+0x28,sibling);
            std::cout<<"branch "<<depth<<" table="<<std::hex<<table-image<<" child_parent="<<(parent==node)<<" child_sibling="<<sibling<<std::dec<<'\n';
            if(table==image+0x11dea18)break;node=child;
        }
    }
    std::cout<<"regions="<<icons.available_regions<<" covenant="<<icons.covenant.type<<":"<<icons.covenant.id<<" tint="<<std::hex<<icons.covenant.tint<<std::dec
        <<" opacity="<<icons.covenant.opacity<<" effects="<<icons.effect_count<<" effect_alpha="<<icons.effect_opacity<<'\n';
    for(unsigned i=0;i<icons.effect_count;++i)std::cout<<i<<" key="<<icons.effects[i].type<<":"<<icons.effects[i].id<<" tint="<<std::hex<<icons.effects[i].tint<<std::dec<<" opacity="<<icons.effects[i].opacity<<'\n';
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
