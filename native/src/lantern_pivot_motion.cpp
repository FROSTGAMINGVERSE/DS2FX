#include "lantern_pivot_motion.h"
#include "lantern_pendulum.h"
#include "lantern_clearance.h"
#include "common.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>

namespace fx::lantern_pivot_motion {
namespace {
using Vec=fx::lantern_pendulum::Vec3;
struct alignas(16) QST {float t[4],q[4],s[4];};
static_assert(sizeof(QST)==48);
struct Bone {int16_t parent,child,next,previous;uintptr_t name;};
static_assert(sizeof(Bone)==16);
struct Matrix {double m[3][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0}};};
using Setter=void(*)(uintptr_t,uint32_t,const void*);
uintptr_t slot=0;Setter setter=nullptr;
std::atomic<bool> prepared{false},enabled{false},fault{false};
std::atomic<DWORD> engine_thread{0};
std::atomic<uint64_t> frames{0},steps{0},applied{0},repeated{0},rejected{0},resets{0};
std::atomic<float> last_world[3]{},last_lean[2]{},last_quat[4]{};
std::atomic<uint64_t> contacts{0};
std::atomic<float> last_clearance[3]{};
uint64_t authorized_at=0,simulated_frame=0,epoch=0;
float seconds=0;
double pending_seconds=0;
QST last_owned{};
bool last_owned_valid=false;
uintptr_t written_player=0,written_pose=0,written_skeleton=0;
uintptr_t tick_player=0,last_player=0,last_pose=0,last_skeleton=0;
fx::lantern_pendulum::Solver solver;
fx::lantern_pendulum::Pose cached{};
Vec clearance_local{};
fx::lantern_clearance::SoftContact soft_contact;
double simulated_seconds=0;
thread_local bool executing=false;
bool pointer(uintptr_t base,uintptr_t offset,uintptr_t& out) noexcept {
    return base&&base<=UINTPTR_MAX-offset&&fx::read(base+offset,out)&&out>=0x10000&&out<=0x00007fffffffffffULL&&!(out&7);
}
template<size_t N> bool name_is(uintptr_t name,const wchar_t (&text)[N]) noexcept {
    if(name<0x10000||name>0x00007fffffffff00ULL||(name&1))return false;
    std::array<wchar_t,N> value{};
    return fx::read(name,value)&&!std::memcmp(value.data(),text,sizeof(text));
}
bool living(uintptr_t& player,uintptr_t& pose) noexcept {
    uintptr_t manager=0,model=0,scene=0;int32_t hp=0,max_hp=0;
    return pointer(slot,0,manager)&&fx::read(manager+0xbc8,scene)&&scene&&pointer(manager,0xd0,player)&&pointer(player,0xf0,model)&&
        fx::read(player+0x168,hp)&&fx::read(player+0x170,max_hp)&&hp>0&&hp<=max_hp&&max_hp<=1000000&&
        ((pose=model+0x80),!(pose&15));
}
void reset_simulation() noexcept {
    // Keep last-write provenance separate from simulation. A mapper may retain
    // our exact leaf after a hitch; forgetting it would reject that rig forever.
    // context() still authenticates the live owner/rig before accepting it.
    solver.reset();cached={};clearance_local={};soft_contact.reset();simulated_seconds=0;pending_seconds=0;
    simulated_frame=0;last_player=last_pose=last_skeleton=0;++resets;
}
bool finite_unit_qst(const QST& q) noexcept {
    for(float x:q.t)if(!std::isfinite(x)||std::abs(x)>100000)return false;
    double n=0;for(float x:q.q){if(!std::isfinite(x))return false;n+=double(x)*x;}
    if(std::abs(n-1)>0.002)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(q.s[i])||std::abs(q.s[i]-1)>0.002)return false;
    return std::isfinite(q.s[3]);
}
bool owned_rest(const QST& q) noexcept {
    constexpr float xyz[3]{.049999952f,-.23999955f,-.16000067f};
    if(!finite_unit_qst(q))return false;
    for(unsigned i=0;i<3;++i)if(std::abs(q.t[i]-xyz[i])>0.000005f||q.q[i]!=0||q.s[i]!=1)return false;
    return q.q[3]==1;
}
Matrix qst_matrix(const QST& q) noexcept {
    const double x=q.q[0],y=q.q[1],z=q.q[2],w=q.q[3];
    Matrix a{{{1-2*y*y-2*z*z,2*x*y-2*z*w,2*x*z+2*y*w,q.t[0]},
        {2*x*y+2*z*w,1-2*x*x-2*z*z,2*y*z-2*x*w,q.t[1]},
        {2*x*z-2*y*w,2*y*z+2*x*w,1-2*x*x-2*y*y,q.t[2]}}};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<3;++c)a.m[r][c]*=q.s[c];return a;
}
Matrix multiply(const Matrix& a,const Matrix& b) noexcept {
    Matrix out{};
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c){out.m[r][c]=c==3?a.m[r][3]:0;for(unsigned k=0;k<3;++k)out.m[r][c]+=a.m[r][k]*b.m[k][c];}
    return out;
}
bool rotation_valid(const Matrix& a) noexcept {
    for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)if(!std::isfinite(a.m[r][c])||std::abs(a.m[r][c])>1000000)return false;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j){double v=0;for(unsigned k=0;k<3;++k)v+=a.m[k][i]*a.m[k][j];if(std::abs(v-(i==j?1:0))>.006)return false;}
    const double det=a.m[0][0]*(a.m[1][1]*a.m[2][2]-a.m[1][2]*a.m[2][1])-a.m[0][1]*(a.m[1][0]*a.m[2][2]-a.m[1][2]*a.m[2][0])+a.m[0][2]*(a.m[1][0]*a.m[2][1]-a.m[1][1]*a.m[2][0]);
    return std::abs(det-1)<.006;
}
struct Context {QST rest{};Matrix parent_world{},body_world{};Vec pivot{};uintptr_t skeleton=0,bones=0,local=0,flags=0,bind=0;};
bool context(uintptr_t player,uintptr_t pose,Context& out) noexcept {
    uintptr_t actual_player=0,actual_pose=0;uint32_t count=0;uint16_t skeleton_count=0;
    if(!living(actual_player,actual_pose)||player!=actual_player||pose!=actual_pose||
        !pointer(pose,8,out.skeleton)||!pointer(out.skeleton,0x20,out.bones)||!pointer(out.skeleton,0x28,out.bind)||
        !pointer(pose,0x20,out.local)||!pointer(pose,0x18,out.flags)||
        !fx::read(pose+0x10,count)||count!=74||!fx::read(out.skeleton+0x18,skeleton_count)||skeleton_count!=74)return false;
    std::array<Bone,74> bones{};if(!fx::read(out.bones,bones)||bones[73].parent!=3||bones[73].child!=-1||
        !name_is(bones[73].name,L"DS2FX_LanternPivot")||!name_is(bones[3].name,L"Pelvis"))return false;
    for(unsigned i=0;i<73;++i)if(name_is(bones[i].name,L"DS2FX_LanternPivot"))return false;
    QST current{};if(!fx::read(out.bind+73*48,out.rest)||!owned_rest(out.rest)||!fx::read(out.local+73*48,current))return false;
    // A mapper may retain our exact previous leaf pose. Accept only that known
    // owned value or the exact native bind, never an arbitrary animated QST.
    if(std::memcmp(&current,&out.rest,48)&&!(last_owned_valid&&player==written_player&&pose==written_pose&&out.skeleton==written_skeleton&&!std::memcmp(&current,&last_owned,48)))return false;
    // Match the actual dummy world transform (312750 -> AF5200): player+60 is
    // a row-vector4x4 transposed by native code into column-vector3x4.
    std::array<float,16> world{};if(!fx::read(player+0x60,world)||std::abs(world[3])>1e-5||std::abs(world[7])>1e-5||std::abs(world[11])>1e-5||std::abs(world[15]-1)>1e-5)return false;
    Matrix combined{};for(unsigned r=0;r<3;++r)for(unsigned c=0;c<4;++c)combined.m[r][c]=world[c*4+r];
    if(!rotation_valid(combined))return false;out.body_world=combined;
    std::array<int16_t,74> chain{};std::array<bool,74> visited{};unsigned n=0;int16_t index=3;
    while(index>=0){if(index>=74||visited[index]||n>=74)return false;visited[index]=true;chain[n++]=index;index=bones[index].parent;}
    while(n){const auto i=chain[--n];QST q{};uint8_t flags=0;if(!fx::read(out.flags+i,flags)||(flags&1)||!fx::read(out.local+i*48,q)||!finite_unit_qst(q))return false;combined=multiply(combined,qst_matrix(q));}
    if(!rotation_valid(combined))return false;out.parent_world=combined;
    double p[3]{};for(unsigned r=0;r<3;++r){p[r]=combined.m[r][3];for(unsigned c=0;c<3;++c)p[r]+=combined.m[r][c]*out.rest.t[c];}
    out.pivot={p[0],p[1],p[2]};return true;
}
bool native_call(uintptr_t pose,const QST& q) noexcept {
    __try {setter(pose,73,&q);return true;} __except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
std::string code_hash(uintptr_t address,size_t size) {
    std::vector<unsigned char> bytes(size);SIZE_T copied=0;
    fx::require(ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),bytes.data(),size,&copied)&&copied==size,"Cannot read native pose setter");
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;
    fx::require(BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)==0,"Pose setter SHA provider failed");
    struct Cleanup{BCRYPT_ALG_HANDLE a;BCRYPT_HASH_HANDLE& h;~Cleanup(){if(h)BCryptDestroyHash(h);BCryptCloseAlgorithmProvider(a,0);}} cleanup{a,h};
    fx::require(BCryptCreateHash(a,&h,nullptr,0,nullptr,0,0)==0,"Pose setter SHA failed");unsigned char digest[32];
    fx::require(BCryptHashData(h,bytes.data(),static_cast<ULONG>(size),0)==0&&BCryptFinishHash(h,digest,sizeof(digest),0)==0,"Pose setter SHA failed");
    std::ostringstream out;for(auto x:digest)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(x);return out.str();
}
bool prepare_impl(uintptr_t base_slot,bool opt_in,Setter call) {
    if(!opt_in)return false;
    fx::require(!prepared.load()&&base_slot&&call,"Invalid/repeated pivot motion setup");
    slot=base_slot;setter=call;last_quat[3].store(1);prepared.store(true);enabled.store(true);return true;
}
}
bool prepare(uintptr_t base_slot,bool opt_in) {
    if(!opt_in)return false;
#if defined(DS2FX_SEAMLESS_BRIDGE) && !defined(DS2FX_SEAMLESS_LANTERN)
    (void)base_slot;fx::require(false,"Lantern motion is offline-only");return false;
#else
    const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    fx::require(fx::hash_file(fx::module_path())==fx::game_hash&&base_slot==image+0x16148f0,"Unsupported executable for lantern motion");
    fx::require(code_hash(image+0xb0d610,223)=="4e033ed6690b85547439bf807fecf8fe6c3990c16ce77f4aa23239ba006425fe","Native pose setter changed");
    return prepare_impl(base_slot,true,reinterpret_cast<Setter>(image+0xb0d610));
#endif
}
void authorize_engine_tick(uintptr_t manager,uintptr_t dt_address) noexcept {
    if(!enabled.load())return;
    uintptr_t actual=0;if(!manager||!fx::read(slot,actual)||actual!=manager)return;
    DWORD expected=0;const auto tid=GetCurrentThreadId();engine_thread.compare_exchange_strong(expected,tid);
    if(engine_thread.load()!=tid){fault.store(true);enabled.store(false);return;}
    ++frames;authorized_at=GetTickCount64();seconds=0;tick_player=0;uintptr_t pose=0;float dt=0;
    if(!living(tick_player,pose)||!dt_address||!fx::read(dt_address,dt)||!std::isfinite(dt)||dt<=0||dt>.1f){reset_simulation();tick_player=0;return;}
    seconds=dt;pending_seconds+=dt;
}
void on_owned_pose(uintptr_t player,uintptr_t pose,const fx::lantern_pose_observer::Sample& sample) noexcept {
    if(!enabled.load()||fault.load()||executing)return;
    const auto now=GetTickCount64();
    if(!sample.on_authorized_thread||!sample.pose_valid||sample.bone_count!=74||sample.pivot_index!=73||sample.parent_index!=3||
        GetCurrentThreadId()!=engine_thread.load()||!authorized_at||now<authorized_at||now-authorized_at>250||tick_player!=player||seconds<=0){++rejected;return;}
    executing=true;struct Guard{~Guard(){executing=false;}} guard;
    Context c{};if(!context(player,pose,c)){++rejected;return;}
    if(player!=last_player||pose!=last_pose||c.skeleton!=last_skeleton){reset_simulation();pending_seconds=seconds;last_player=player;last_pose=pose;last_skeleton=c.skeleton;++epoch;}
    const auto frame=frames.load();
    const bool new_frame=frame!=simulated_frame;
    if(new_frame){
        if(pending_seconds>fx::lantern_pendulum::Solver::max_dt){solver.reset();clearance_local={};soft_contact.reset();pending_seconds=seconds;++resets;}
        else if(simulated_frame&&frame!=simulated_frame+1)solver.rebase(c.pivot);
        simulated_seconds=pending_seconds;cached=solver.update(c.pivot,pending_seconds,epoch);pending_seconds=0;
        if(cached.reset_reason!=fx::lantern_pendulum::ResetReason::none){clearance_local={};soft_contact.reset();}
        simulated_frame=frame;++steps;
    }else ++repeated;
    // The authored leaf's local -X points down. Align that animated direction
    // to the gravity-based pendulum direction, rather than adding swing to a
    // tilted pelvis. This cancels body pitch/roll while preserving native yaw.
    const Vec from{-c.parent_world.m[0][0],-c.parent_world.m[1][0],-c.parent_world.m[2][0]};
    const Vec to=cached.direction;
    double world_q[3]{from.y*to.z-from.z*to.y,from.z*to.x-from.x*to.z,from.x*to.y-from.y*to.x};
    double w=1+from.x*to.x+from.y*to.y+from.z*to.z;
    double norm=std::sqrt(world_q[0]*world_q[0]+world_q[1]*world_q[1]+world_q[2]*world_q[2]+w*w);
    if(norm<1e-7){
        // Opposite directions have no unique shortest axis. The parent's Y
        // axis is perpendicular to its -X and supplies a deterministic choice.
        for(unsigned i=0;i<3;++i)world_q[i]=c.parent_world.m[i][1];w=0;norm=1;
    }
    if(!std::isfinite(norm)){++rejected;reset_simulation();return;}
    for(double& x:world_q)x/=norm;w/=norm;QST next=c.rest;
    for(unsigned i=0;i<3;++i){double v=0;for(unsigned j=0;j<3;++j)v+=c.parent_world.m[j][i]*world_q[j];next.q[i]=static_cast<float>(v);}
    next.q[3]=static_cast<float>(w);
    // World swing remains bounded at12 degrees by the solver. Local correction
    // may be larger during rolls because it compensates the native body tilt.
    if(!finite_unit_qst(next)){++rejected;reset_simulation();return;}
    // Presentation-only capsule contact: no game collision filters, native body
    // shapes, armor, or remote characters are modified. The lamp capsule bounds
    // the owned mesh (59mm half-width, 276mm below pivot in rest pose).
    // Pelvis local -X is the authored lamp's downward axis. The body proxy stays
    // upright in the actual character transform, so rolls do not drag it away.
    using namespace fx::lantern_clearance;
    auto point=[](const Matrix& m,Vec p){return Vec{
        m.m[0][3]+m.m[0][0]*p.x+m.m[0][1]*p.y+m.m[0][2]*p.z,
        m.m[1][3]+m.m[1][0]*p.x+m.m[1][1]*p.y+m.m[1][2]*p.z,
        m.m[2][3]+m.m[2][0]*p.x+m.m[2][1]*p.y+m.m[2][2]*p.z};};
    // Sample the unshifted assembly. Feeding the already corrected pose back
    // into contact detection would alternate between penetration and release.
    const auto lamp_world=multiply(c.parent_world,qst_matrix(next));
    const Capsule lamp{point(lamp_world,{-.09,0,0}),point(lamp_world,{-.216,0,0}),.061};
    const Capsule body{point(c.body_world,{0,.60,0}),point(c.body_world,{0,1.12,0}),.212};
    const auto contact=separate(lamp,body,sub(c.pivot,point(c.body_world,{0,1.02,0})));
    if(new_frame){
        if(contact.touching)++contacts;
        soft_contact.update(contact.touching?contact.correction:Vec{},simulated_seconds);
    }
    // Preserve contact momentum in world space across character turns. Convert
    // only the final displacement into the current authenticated parent basis.
    const auto shift=soft_contact.position();const double correction[3]{shift.x,shift.y,shift.z};double local_shift[3]{};
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)local_shift[i]+=c.parent_world.m[j][i]*correction[j];
    clearance_local={local_shift[0],local_shift[1],local_shift[2]};
    next.t[0]=c.rest.t[0]+static_cast<float>(clearance_local.x);
    next.t[1]=c.rest.t[1]+static_cast<float>(clearance_local.y);
    next.t[2]=c.rest.t[2]+static_cast<float>(clearance_local.z);
    if(!finite_unit_qst(next)){++rejected;reset_simulation();return;}
    // Revalidate the source chain and exact bind/current QST immediately before
    // the one native setter. No previous player/pose pointer is written later.
    Context after{};if(!enabled.load()||!context(player,pose,after)||after.skeleton!=c.skeleton||after.local!=c.local||after.bind!=c.bind||after.flags!=c.flags||after.bones!=c.bones||
        std::memcmp(&after.rest,&c.rest,48)){++rejected;return;}
    if(!native_call(pose,next)){fault.store(true);enabled.store(false);reset_simulation();return;}
    QST actual{};if(!fx::read(c.local+73*48,actual)||std::memcmp(&actual,&next,48)){fault.store(true);enabled.store(false);reset_simulation();return;}
    last_owned=next;last_owned_valid=true;
    written_player=player;written_pose=pose;written_skeleton=c.skeleton;
    ++applied;last_world[0].store(static_cast<float>(c.pivot.x));last_world[1].store(static_cast<float>(c.pivot.y));last_world[2].store(static_cast<float>(c.pivot.z));
    last_lean[0].store(static_cast<float>(cached.lean_x));last_lean[1].store(static_cast<float>(cached.lean_z));for(unsigned i=0;i<4;++i)last_quat[i].store(next.q[i]);
    last_clearance[0].store(static_cast<float>(clearance_local.x));last_clearance[1].store(static_cast<float>(clearance_local.y));last_clearance[2].store(static_cast<float>(clearance_local.z));
}
Status status() noexcept {
    Status out{frames.load(),steps.load(),applied.load(),repeated.load(),rejected.load(),resets.load(),engine_thread.load(),prepared.load(),enabled.load(),fault.load()};
    for(unsigned i=0;i<3;++i){out.world_pivot[i]=last_world[i].load();out.clearance[i]=last_clearance[i].load();}out.contacts=contacts.load();for(unsigned i=0;i<2;++i)out.lean[i]=last_lean[i].load();for(unsigned i=0;i<4;++i)out.local_rotation[i]=last_quat[i].load();return out;
}
void disable() noexcept {enabled.store(false);}
#ifdef DS2FX_FIXTURE
bool prepare_owned_fixture(uintptr_t base_slot,bool opt_in,FixtureSetter call){return prepare_impl(base_slot,opt_in,call);}
#endif
}
