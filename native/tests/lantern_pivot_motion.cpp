#include "../src/lantern_pivot_motion.h"
#include "../src/common.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>
namespace {
struct alignas(16) QST {float t[4]{},q[4]{0,0,0,1},s[4]{1,1,1,0};};
struct Bone {int16_t parent=-1,child=-1,next=-1,previous=-1;uintptr_t name=0;};
alignas(16) std::array<unsigned char,0xc00> manager{};
alignas(16) std::array<unsigned char,0x200> model{};
alignas(16) std::array<unsigned char,0x500> player{};
alignas(16) std::array<unsigned char,0x40> skeleton{};
alignas(16) std::array<Bone,74> bones{};
alignas(16) std::array<QST,74> local{},bind{};
alignas(16) std::array<uint8_t,80> flags{};
uintptr_t slot=0,pose=0,p=0;float dt=1.0f/60;
uint64_t setter_calls=0;bool throw_setter=false,reenter=false;
fx::lantern_pose_observer::Sample sample{};
template<class A,class T>void put(A& a,size_t off,const T& x){std::memcpy(reinterpret_cast<unsigned char*>(a.data())+off,&x,sizeof(x));}
void identity_world(float x=0,float y=0,float z=0){put(player,0x60,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,x,y,z,1});}
void mapper(){local[73]=bind[73];}
void native_setter(uintptr_t target,uint32_t index,const void* data){
    fx::require(target==pose&&index==73&&!(reinterpret_cast<uintptr_t>(data)&15),"Exact native setter ABI and aligned owned QST");++setter_calls;
    if(throw_setter)RaiseException(0xe0007351,0,0,nullptr);
    if(reenter)fx::lantern_pivot_motion::on_owned_pose(p,pose,sample);
    std::memcpy(&local[73],data,sizeof(QST));flags[73]=(flags[73]&~1u)|2u;
}
void initialize(){
    slot=reinterpret_cast<uintptr_t>(manager.data());p=reinterpret_cast<uintptr_t>(player.data());pose=reinterpret_cast<uintptr_t>(model.data())+0x80;
    put(manager,0xd0,p);put(manager,0xbc8,uintptr_t{1});put(player,0xf0,reinterpret_cast<uintptr_t>(model.data()));put(player,0x168,int32_t{100});put(player,0x170,int32_t{200});
    put(model,0x88,reinterpret_cast<uintptr_t>(skeleton.data()));put(model,0x90,uint32_t{74});put(model,0x98,reinterpret_cast<uintptr_t>(flags.data()));put(model,0xa0,reinterpret_cast<uintptr_t>(local.data()));
    put(skeleton,0x18,uint16_t{74});put(skeleton,0x20,reinterpret_cast<uintptr_t>(bones.data()));put(skeleton,0x28,reinterpret_cast<uintptr_t>(bind.data()));
    for(auto& b:bones)b.name=reinterpret_cast<uintptr_t>(L"Other");bones[73].name=reinterpret_cast<uintptr_t>(L"DS2FX_LanternPivot");bones[73].parent=3;
    bones[3].name=reinterpret_cast<uintptr_t>(L"Pelvis");bones[3].parent=2;bones[2].parent=0;
    bind[73].t[0]=.049999952f;bind[73].t[1]=-.23999955f;bind[73].t[2]=-.16000067f;local=bind;identity_world();
    sample.bone_count=74;sample.pivot_index=73;sample.parent_index=3;sample.parent_matches=sample.pose_valid=sample.on_authorized_thread=true;
}
void tick(){fx::lantern_pivot_motion::authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));mapper();fx::lantern_pivot_motion::on_owned_pose(p,pose,sample);}
void reset_tracking(){const auto saved=dt;dt=0;tick();dt=saved;}
std::array<double,3> rotate(const QST& q,std::array<double,3> v){
    const double x=q.q[0],y=q.q[1],z=q.q[2],w=q.q[3];
    return {(1-2*y*y-2*z*z)*v[0]+(2*x*y-2*z*w)*v[1]+(2*x*z+2*y*w)*v[2],
        (2*x*y+2*z*w)*v[0]+(1-2*x*x-2*z*z)*v[1]+(2*y*z-2*x*w)*v[2],
        (2*x*z-2*y*w)*v[0]+(2*y*z+2*x*w)*v[1]+(1-2*x*x-2*y*y)*v[2]};
}
std::array<double,3> hanging(){auto v=rotate(local[73],{-1,0,0});for(int i:{3,2,0})v=rotate(local[i],v);return v;}
double angle(){auto v=hanging();return std::atan2(std::hypot(v[0],v[2]),-v[1]);}
}
int main(){try{
    using namespace fx::lantern_pivot_motion;initialize();
    fx::require(!prepare_owned_fixture(reinterpret_cast<uintptr_t>(&slot),false,native_setter),"Explicit motion opt-in required");
    fx::require(prepare_owned_fixture(reinterpret_cast<uintptr_t>(&slot),true,native_setter),"Owned motion fixture prepared");
    const auto expected_bones=bones;const auto expected_bind=bind;const auto first_local=local;
    tick();fx::require(status().applied==1&&angle()<.0004,"First frame establishes a gravity-aligned lantern without force");
    for(int i=1;i<20;++i){identity_world(i*.001f);tick();}
    fx::require(angle()<1e-4,"Constant velocity does not create spurious oscillation");
    identity_world(.022f);tick();fx::require(status().lean[0]<0&&std::abs(local[73].q[2])>1e-5,"Acceleration gives physically opposing world swing");
    fx::require(angle()<=12*3.14159265358979323846/180+1e-5,"Motion respects12-degree bound");
    const auto stepped=status().steps,applied_before=status().applied;const auto q=local[73];mapper();on_owned_pose(p,pose,sample);
    fx::require(status().steps==stepped&&status().applied==applied_before+1&&status().repeated==1&&std::memcmp(&q,&local[73],48)==0,"Repeated mapper reuses same native-frame solution without double simulation");
    fx::require(std::memcmp(local.data(),first_local.data(),73*48)==0&&std::memcmp(bones.data(),expected_bones.data(),sizeof(bones))==0&&std::memcmp(bind.data(),expected_bind.data(),sizeof(bind))==0,"No existing bone, hierarchy or bind-pose writes");
    // Pelvis yaw90 degrees: world Z rotation must map to negative local X.
    reset_tracking();identity_world();local[3].q[1]=std::sqrt(.5f);local[3].q[3]=std::sqrt(.5f);tick();tick();identity_world(.003f);tick();
    fx::require(hanging()[0]<-1e-5&&angle()<.21,"World gravity and opposing lean survive parent yaw");
    // Pelvis roll90 degrees about X: world Z rotation maps to positive local Y.
    reset_tracking();identity_world();local[3].q[1]=0;local[3].q[0]=std::sqrt(.5f);tick();tick();identity_world(.003f);tick();
    fx::require(hanging()[0]<-1e-5&&angle()<.21,"World gravity and opposing lean survive parent roll");
    reset_tracking();identity_world();tick();
    for(int n=0;n<=180;n+=3){double r=n*3.141592653589793/180;local[3]=QST{};local[3].q[2]=float(std::sin(r/2));local[3].q[3]=float(std::cos(r/2));mapper();on_owned_pose(p,pose,sample);fx::require(angle()<.001,"Native body tilt cannot tilt an idle suspended lantern");}
    local[3]=QST{};reset_tracking();identity_world(10,20,30);tick();const auto location=status();
    fx::require(std::abs(location.world_pivot[0]-10.049999952f)<1e-5&&std::abs(location.world_pivot[1]-19.76000045f)<1e-5&&std::abs(location.world_pivot[2]-29.83999933f)<1e-5,"World pivot uses native player transform and rest right-rear hip offset");
    identity_world(12,20,30);tick();fx::require(angle()<1e-4,"Teleport resets motion");
    identity_world(12.003f,20,30);tick();identity_world(12.009f,20,30);tick();
    const auto moving_angle=angle();const auto moving_resets=status().resets;
    on_owned_pose(p,pose,sample);fx::require(status().resets==moving_resets&&std::abs(angle()-moving_angle)<1e-5,"Repeated owned pose remains accepted without bind rewrite");
    authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));mapper();on_owned_pose(p,pose,sample);
    fx::require(angle()>1e-5&&status().resets==moving_resets&&std::abs(angle()-moving_angle)<.04,"Brief mapper gap preserves swing instead of snapping to rest");
    flags[3]=1;tick();flags[3]=0;tick();fx::require(angle()>1e-5&&status().resets==moving_resets,"Transient unresolved ancestor preserves momentum without unsafe writes");
    authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));identity_world(14,20,30);tick();fx::require(angle()<1e-4,"Teleport during brief mapper gap still resets");
    identity_world(14.003f,20,30);tick();identity_world(14.009f,20,30);tick();
    for(int n=0;n<8;++n)authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));mapper();on_owned_pose(p,pose,sample);fx::require(angle()<1e-4,"Long mapper interruption clears stale swing");
    reenter=true;tick();reenter=false;fx::require(!status().fault,"Recursive observer entry cannot issue nested setters");
    const auto original_calls=setter_calls;mapper();std::thread foreign([]{on_owned_pose(p,pose,sample);});foreign.join();fx::require(setter_calls==original_calls,"Foreign thread cannot mutate pose");
    bones[73].parent=2;tick();fx::require(setter_calls==original_calls,"Wrong owned parent rejected");bones[73].parent=3;
    bones[73].child=4;tick();fx::require(setter_calls==original_calls,"Owned bone must be leaf; no native subtree writes");bones[73].child=-1;
    bones[5].name=bones[73].name;tick();fx::require(setter_calls==original_calls,"Duplicate owned name rejected");bones[5].name=reinterpret_cast<uintptr_t>(L"Other");
    bind[73].t[0]+=.1f;tick();fx::require(setter_calls==original_calls,"Unrecognized rest pose rejected");bind[73].t[0]-=.1f;
    flags[3]=1;tick();fx::require(setter_calls==original_calls,"Unresolved native local transform is rejected");flags[3]=0;
    local[3].q[0]=std::numeric_limits<float>::quiet_NaN();tick();fx::require(setter_calls==original_calls,"Nonfinite parent pose rejected");local[3]=QST{};
    bones[0].parent=3;tick();fx::require(setter_calls==original_calls,"Ancestor cycle rejected");bones[0].parent=-1;
    identity_world();put(player,0x60,2.0f);tick();fx::require(setter_calls==original_calls,"Unknown scaled world basis rejected");identity_world();
    put(player,0x168,int32_t{0});tick();fx::require(setter_calls==original_calls,"Death resets and prevents mutation");put(player,0x168,int32_t{100});tick();fx::require(angle()<1e-4,"Respawn starts without stale force");
    // Authored Pelvis basis: local -X points down; local -Y is rightward.
    // An inward swing must move the whole owned leaf away from the hip proxy.
    reset_tracking();identity_world();local[3]=QST{};
    local[3].q[0]=local[3].q[1]=std::sqrt(.5f);local[3].q[3]=0;local[3].t[1]=.97f;
    tick();tick();double largest_clearance=0;
    for(int n=1;n<=60;++n){const float t=n/60.0f;identity_world(-2*t*t);tick();
        const double shift=std::hypot(double(local[73].t[0]-bind[73].t[0]),double(local[73].t[1]-bind[73].t[1]),double(local[73].t[2]-bind[73].t[2]));
        largest_clearance=std::max(largest_clearance,shift);fx::require(shift<=.06501,"Body clearance remains bounded at65mm");
    }
    fx::require(largest_clearance>.005,"Inward swing engages body clearance");
    const auto contacted=local[73];mapper();on_owned_pose(p,pose,sample);
    fx::require(std::memcmp(&contacted,&local[73],48)==0,"Repeated pose does not decay or accumulate contact twice");
    auto contact_world=rotate(local[3],{local[73].t[0]-bind[73].t[0],local[73].t[1]-bind[73].t[1],local[73].t[2]-bind[73].t[2]});
    local[3]=QST{};local[3].q[1]=std::sqrt(.5f);local[3].q[3]=std::sqrt(.5f);mapper();on_owned_pose(p,pose,sample);
    auto turned_world=rotate(local[3],{local[73].t[0]-bind[73].t[0],local[73].t[1]-bind[73].t[1],local[73].t[2]-bind[73].t[2]});
    fx::require(std::hypot(contact_world[0]-turned_world[0],contact_world[1]-turned_world[1],contact_world[2]-turned_world[2])<1e-6,"Changing parent basis preserves world contact displacement");
    identity_world(100,0,0);tick();fx::require(std::memcmp(local[73].t,bind[73].t,16)==0,"Teleport discards contact offset");
    local[3]=QST{};reset_tracking();identity_world();
    const auto previous=setter_calls;dt=.5f;tick();fx::require(setter_calls==previous,"Long native timestep resets without mutation");dt=1.0f/60;
    // Unlike tick(), deliberately retain the adapter's last leaf through a
    // mapper/loading interruption. The old reset erased its provenance.
    tick();identity_world(.003f);tick();identity_world(.009f);tick();
    const auto retained=local[73];const auto before_hitch=setter_calls;
    dt=.5f;authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));
    on_owned_pose(p,pose,sample);fx::require(setter_calls==before_hitch,"Hitch never writes a stale rig");
    dt=1.0f/60;authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));
    on_owned_pose(p,pose,sample);
    fx::require(setter_calls==before_hitch+1&&angle()<.001,"Same live rig accepts retained owned leaf and recovers after hitch");
    local[73]=retained;local[73].t[0]+=.01f;
    const auto foreign_leaf=setter_calls;authorize_engine_tick(slot,reinterpret_cast<uintptr_t>(&dt));on_owned_pose(p,pose,sample);
    fx::require(setter_calls==foreign_leaf,"Recovery still rejects an arbitrary leaf value");
    throw_setter=true;tick();fx::require(status().fault&&!status().enabled,"Native setter exception faults capability without escaping callback");throw_setter=false;
    disable();const auto disabled=setter_calls;tick();fx::require(setter_calls==disabled&&local[73].q[3]==1,"Disable leaves native restored bind pose");
    std::cout<<"PASS native pivot motion fixture: owned leaf ABI, real dt/once perframe, acceleration/constantvelocity, yaw/roll, world pivot, transitions, reentry/thread/invalid gates, zero existingbone writes\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
