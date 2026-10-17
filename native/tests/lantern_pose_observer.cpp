#include "../src/lantern_pose_observer.h"
#include "../src/common.h"
#include <array>
#include <cstring>
#include <future>
#include <iostream>
#include <limits>
#include <thread>

extern "C" void DS2FX_PoseObserverFixtureTarget();
namespace fx::lantern_pose_observer {
// Fixture-only access lets an actual diagnostic reader hold the publication
// lock while the engine-thread callback runs. No shipped API is introduced.
void lock_sample_owned_fixture();
void unlock_sample_owned_fixture();
}
struct alignas(16) Capture {uint64_t regs[8]{};unsigned char fx[512]{};};
extern "C" void DS2FX_PoseObserverFixtureCapture(void*,uintptr_t,Capture*);
namespace {
alignas(16) std::array<unsigned char,0x200> manager{},model{};
alignas(16) std::array<unsigned char,0x500> player{};
alignas(16) std::array<unsigned char,0x40> skeleton{};
alignas(16) std::array<unsigned char,6*16> bones{};
alignas(16) std::array<unsigned char,6*48> local{};
// Intentionally only2-aligned UTF16 is legal in a native bone name.
alignas(8) wchar_t pelvis_storage[]=L"xPelvis";
const wchar_t* names[]={L"Root",L"Bone1",L"Bone2",pelvis_storage+1,L"DS2FX_LanternPivot",L"Unused"};
uintptr_t slot=0;
uint64_t handler_calls=0;
void owned_handler(uintptr_t player_address,uintptr_t pose_address,const fx::lantern_pose_observer::Sample& value) noexcept {
    if(player_address==reinterpret_cast<uintptr_t>(player.data())&&pose_address==reinterpret_cast<uintptr_t>(model.data())+0x80&&value.pose_valid&&value.on_authorized_thread)++handler_calls;
}
template<class A,class T> void put(A& a,size_t offset,T value){std::memcpy(a.data()+offset,&value,sizeof(value));}
template<class T,class A> T get(const A& a,size_t offset){T value{};std::memcpy(&value,a.data()+offset,sizeof(value));return value;}
void initialize(){
    slot=reinterpret_cast<uintptr_t>(manager.data());put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(player,0xf0,reinterpret_cast<uintptr_t>(model.data()));put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(model,0x88,reinterpret_cast<uintptr_t>(skeleton.data()));put(model,0x90,uint32_t{6});put(model,0xa0,reinterpret_cast<uintptr_t>(local.data()));
    put(skeleton,0x18,uint16_t{6});put(skeleton,0x20,reinterpret_cast<uintptr_t>(bones.data()));
    for(unsigned i=0;i<6;++i){put(bones,i*16,static_cast<int16_t>(i==4?3:-1));put(bones,i*16+8,reinterpret_cast<uintptr_t>(names[i]));}
    put(local,4*48,std::array<float,4>{.05f,.27f,.055f,0});put(local,4*48+16,std::array<float,4>{0,0,0,1});put(local,4*48+32,std::array<float,4>{1,1,1,0});
}
}
int main(){try{
    using namespace fx::lantern_pose_observer;initialize();
    const auto pose=reinterpret_cast<uintptr_t>(model.data())+0x80;
    const auto target=reinterpret_cast<void*>(&DS2FX_PoseObserverFixtureTarget);
    Capture before{},after{};Sample sample{};
    DS2FX_PoseObserverFixtureCapture(target,pose,&before);
    fx::require(!install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),target,false),"Opt-in required");
    fx::require(install_owned_fixture(reinterpret_cast<uintptr_t>(&slot),reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),target,true),"Read-only pose observer hook installed");
    auto tick=[&]{authorize_engine_tick(slot);DS2FX_PoseObserverFixtureCapture(target,pose,&after);};
    const auto expected_local=local;
    const auto expected_bones=bones;
    set_owned_pose_handler(owned_handler);
    tick();fx::require(snapshot(sample)&&sample.on_authorized_thread&&sample.bone_count==6&&sample.pivot_index==4&&sample.parent_index==3&&sample.parent_matches&&sample.pose_valid,"Canonical owned pivot and Pelvis bind found");
    fx::require(sample.translation[0]==.25f && get<uint32_t>(model,0xb8)==2,"Observes original updated QST, original called once");
    fx::require(std::memcmp(before.regs,after.regs,sizeof(before.regs))==0,"Original return, arguments, flags and registers preserved");
    fx::require(std::memcmp(before.fx,after.fx,8)==0&&std::memcmp(before.fx+24,after.fx+24,8)==0&&std::memcmp(before.fx+160,after.fx+160,96)==0,"Original XMM and FX controls preserved");
    fx::require(local==expected_local&&bones==expected_bones,"Observer never writes skeleton or transforms");
    fx::require(handler_calls==1,"Optional owned handler is called only after native mapping and validation");
    std::thread worker([&]{DS2FX_PoseObserverFixtureCapture(target,pose,&after);});worker.join();
    fx::require(snapshot(sample)&&!sample.on_authorized_thread&&status().foreign_thread==1,"Worker scheduling is observed and reported, never authorized to write");
    fx::require(handler_calls==1,"Foreign thread cannot dispatch owned handler");
    put(bones,4*16+8,reinterpret_cast<uintptr_t>(L"NoOwnedBone"));tick();fx::require(snapshot(sample)&&sample.pivot_index==-1&&!sample.pose_valid,"Unmodified base model remains observable with missing owned bone");put(bones,4*16+8,reinterpret_cast<uintptr_t>(names[4]));
    put(bones,4*16,int16_t{2});tick();fx::require(snapshot(sample)&&!sample.parent_matches&&!sample.pose_valid,"Unexpected parent cannot qualify");put(bones,4*16,int16_t{3});
    put(local,4*48+16,std::numeric_limits<float>::quiet_NaN());tick();fx::require(snapshot(sample)&&!sample.pose_valid,"Invalid quaternion reported unavailable for physics");put(local,4*48+16,0.0f);
    fx::require(handler_calls==1,"Missing, wrong parent and invalid pose cannot dispatch owned handler");
    const auto old=status().samples;put(bones,5*16+8,reinterpret_cast<uintptr_t>(names[4]));tick();fx::require(status().samples==old&&status().invalid>0,"Duplicate owned bone is rejected");put(bones,5*16+8,reinterpret_cast<uintptr_t>(names[5]));
    put(model,0x90,uint32_t{513});tick();fx::require(status().samples==old,"Overbound pose count is rejected");put(model,0x90,uint32_t{6});
    tick();put(manager,0xd0,uintptr_t{0});fx::require(!snapshot(sample),"Character transition hides previous snapshot");put(manager,0xd0,reinterpret_cast<uintptr_t>(player.data()));
    put(player,0x168,int32_t{0});const auto valid=status().canonical;tick();fx::require(status().canonical==valid&&!snapshot(sample),"Dead character not sampled");put(player,0x168,int32_t{100});
    std::promise<void> reader_locked,release_reader;
    auto locked=reader_locked.get_future(),release=release_reader.get_future();
    std::thread reader([&]{lock_sample_owned_fixture();reader_locked.set_value();release.wait();unlock_sample_owned_fixture();});
    locked.wait();const auto before_contention=status();const auto before_handler=handler_calls;
    tick();const auto after_contention=status();const auto after_handler=handler_calls;
    release_reader.set_value();reader.join();
    fx::require(after_contention.dropped==before_contention.dropped+1&&after_contention.samples==before_contention.samples,"Contended diagnostic publication is dropped without blocking the native callback");
    fx::require(after_handler==before_handler+1&&after_contention.canonical==before_contention.canonical+1,"A diagnostic reader cannot suppress the validated engine-thread handler");
    fx::require(local==expected_local&&bones==expected_bones,"Contended observer still never writes transforms or skeleton");
    const auto before_disable=handler_calls;disable();tick();fx::require(!snapshot(sample)&&!status().fault&&handler_calls==before_disable,"Disable stops observer and optional handler cleanly");
    std::cout<<"PASS read-only native pose observer: MinHook/MASM ABI/FX, canonical destination, exact unique bone and parent, missing model, quaternion/count/transition/death gates, worker-thread observation, diagnostic contention independent of handler, zero pose writes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
