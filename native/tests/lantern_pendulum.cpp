#include "../src/lantern_pendulum.h"
#include "../src/lantern_clearance.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace p=fx::lantern_pendulum;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool zero(const p::Pose& a){return a.lean_x==0&&a.lean_z==0&&a.velocity_x==0&&a.velocity_z==0;}
void valid(const p::Pose& a) {
    check(std::isfinite(a.lean_x)&&std::isfinite(a.lean_z),"Finite angles");
    check(std::hypot(a.lean_x,a.lean_z)<=p::Solver::max_angle+1e-12,"Combined12 degree limit");
    check(std::abs(std::hypot(a.direction.x,a.direction.y,a.direction.z)-1)<1e-12,"Unit bob direction");
}
p::Vec3 path(double t) {
    // Smooth acceleration/deceleration in two world axes; no discontinuous jumps.
    const double u=std::max(0.0,t-.5);
    return {.12*(1-std::cos(3*u)),1.0+.005*std::sin(6*u),.07*(1-std::cos(2*u))};
}
std::vector<p::Pose> run(int fps) {
    p::Solver solver;std::vector<p::Pose> output;
    solver.update(path(0),1.0/fps,1);
    for(int n=1;n<=fps*8;++n){auto a=solver.update(path(static_cast<double>(n)/fps),1.0/fps,1);valid(a);if(n%(fps/30)==0)output.push_back(a);}
    return output;
}
int main(){try {
    p::Solver s;
    for(int i=0;i<120;++i)check(zero(s.update({0,1,0},1.0/60,1)),"Stationary lantern rests exactly");
    // A constant positive X acceleration gives negative X lean; stable gain is -a/g.
    p::Pose a;
    for(int n=1;n<=240;++n){double t=n/120.0;a=s.update({.25*t*t,1,0},1.0/120,1);valid(a);}
    check(a.lean_x<-.03&&a.lean_x>-.07&&std::abs(a.lean_z)<1e-12,"Acceleration response direction/gain");
    // Constant velocity carries no persistent force: pendulum settles upright.
    for(int n=1;n<=1200;++n)a=s.update({1.0+n/120.0,1,0},1.0/120,1);
    check(std::hypot(a.lean_x,a.lean_z)<1e-7&&std::hypot(a.velocity_x,a.velocity_z)<1e-6,"Damped settling below a visible angle");
    auto sixty=run(60),one_twenty=run(120),two_forty=run(240);
    double max_difference=0;
    for(size_t i=0;i<sixty.size();++i) {
        max_difference=std::max(max_difference,std::hypot(sixty[i].lean_x-one_twenty[i].lean_x,sixty[i].lean_z-one_twenty[i].lean_z));
        check(std::hypot(one_twenty[i].lean_x-two_forty[i].lean_x,one_twenty[i].lean_z-two_forty[i].lean_z)<.002,"120/240 FPS/substep agreement");
    }
    check(max_difference<.004,"60/120 FPS agreement below0.23 degree");
    // Deterministic repeated same trace.
    auto repeat=run(60);
    for(size_t i=0;i<repeat.size();++i)check(repeat[i].lean_x==sixty[i].lean_x&&repeat[i].lean_z==sixty[i].lean_z,"Deterministic trace");
    // Strong diagonal acceleration tests the radial stop, not independent12+12 axes.
    s.reset();s.update({0,1,0},1.0/120,1);s.update({0,1,0},1.0/120,1);
    for(int n=1;n<=120;++n){double t=n/120.0;a=s.update({8*t*t,1,8*t*t},1.0/120,1);valid(a);}
    check(std::hypot(a.lean_x,a.lean_z)>p::Solver::max_angle*.99,"Large response reaches bounded stop");
    a=s.update({100,1,100},1.0/60,1);check(zero(a)&&a.reset_reason==p::ResetReason::teleport,"Teleport resets energy");
    a=s.update({100,1,100},1.0/60,2);check(zero(a)&&a.reset_reason==p::ResetReason::character_epoch,"Load/new character resets history");
    a=s.update({100,1,100},.5,2);check(zero(a)&&a.reset_reason==p::ResetReason::frame_gap,"Frame gap resets");
    a=s.update({100,1,100},0,2);check(zero(a)&&a.reset_reason==p::ResetReason::invalid_sample,"Zero dt resets");
    a=s.update({std::numeric_limits<double>::quiet_NaN(),1,0},.01,2);check(zero(a)&&a.reset_reason==p::ResetReason::invalid_sample,"NaN pivot resets");
    a=s.update({0,1,0},.01,2,false);check(zero(a)&&a.reset_reason==p::ResetReason::disabled,"Disabled/death resets");
    a=s.update({0,1,0},.01,0);check(zero(a)&&a.reset_reason==p::ResetReason::disabled,"Invalid epoch resets");
    a=s.update({0,1,0},std::numeric_limits<double>::infinity(),2);check(zero(a)&&a.reset_reason==p::ResetReason::invalid_sample,"Infinite dt resets");
    // Variable timestep secant acceleration remains stable and obeys bounds.
    s.reset();double t=0;s.update(path(0),.01,3);
    for(int n=0;n<1000;++n){double dt=n%2==0?1.0/60:1.0/120;t+=dt;valid(s.update(path(t),dt,3));}
    // Submillimetre pose quantization must not turn into visible idle shaking.
    s.reset();double peak_noise=0;
    for(int n=0;n<600;++n){a=s.update({n%2?.0001:-.0001,1,0},1.0/60,4);valid(a);if(n>60)peak_noise=std::max(peak_noise,std::abs(a.lean_x));}
    check(peak_noise<.001,"Idle pose noise stays below 0.06 degrees");
    using namespace fx::lantern_clearance;
    const Capsule body{{0,.60,0},{0,1.12,0},.212};
    const Capsule outside{{-.24,.93,.16},{-.24,.804,.16},.061};
    check(!separate(outside,body,{-1,0,0}).touching,"Resting assembly has clearance");
    Capsule inward{{-.19,.93,.13},{-.19,.804,.13},.061};
    auto contact=separate(inward,body,{-1,0,0});
    check(contact.touching&&contact.correction.x<0&&contact.correction.z>0,"Inward swing pushes away from body");
    inward.a=add(inward.a,contact.correction);inward.b=add(inward.b,contact.correction);
    check(!separate(inward,body,{-1,0,0}).touching,"Resolved capsules do not penetrate");
    contact=separate({{0,.8,0},{0,.8,0},.061},body,{-1,0,0});
    check(contact.touching&&std::isfinite(contact.correction.x)&&contact.correction.x<0,"Coincident/degenerate capsule has deterministic outward normal");
    SoftContact soft;auto initial=soft.update({.065,0,0},1.0/60);
    check(initial.x>0&&initial.x<.003,"Contact starts smoothly without immediate separation");
    Vec previous=initial;double peak_step=0;
    for(int n=0;n<240;++n){auto x=soft.update({n%2?.065:-.065,0,0},1.0/120);peak_step=std::max(peak_step,std::sqrt(dot(sub(x,previous),sub(x,previous))));check(std::sqrt(dot(x,x))<=.065001,"Rapid contact changes remain bounded");previous=x;}
    check(peak_step<.002,"Alternating contact normals do not cause visible position jumps");
    soft.reset();for(int n=0;n<120;++n)soft.update({.04,.02,0},1.0/60);
    const auto held=soft.position();auto release=soft.update({},1.0/60);
    check(std::sqrt(dot(sub(release,held),sub(release,held)))<.002,"Contact release retains position and settles smoothly");
    for(int n=0;n<180;++n)soft.update({},1.0/60);
    check(std::sqrt(dot(soft.position(),soft.position()))<1e-8,"Released contact naturally returns to rest");
    SoftContact low,high;for(int n=0;n<60;++n)low.update({.05,0,0},1.0/60);for(int n=0;n<240;++n)high.update({.05,0,0},1.0/240);
    check(std::abs(low.position().x-high.position().x)<1e-10,"Soft contact response is independent of frame rate");
    std::cout<<"PASS lantern pendulum: gravity, acceleration, damping, bounds, resets, deterministic60/120/240 FPS; max60/120 difference="<<max_difference<<" rad. No game binding.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
