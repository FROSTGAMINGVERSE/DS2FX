// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fx::lantern_pendulum {
struct Vec3 { double x=0,y=0,z=0; };
enum class ResetReason { none, first_sample, disabled, character_epoch, invalid_sample, frame_gap, teleport };
struct Pose {
    // Small-angle world X/Z lean, in radians, about the current hip pivot.
    // No game coordinates, bone indices, pointers, or memory writes live here.
    double lean_x=0,lean_z=0,velocity_x=0,velocity_z=0;
    Vec3 direction{0,-1,0}; // Unit pivot-to-lantern direction in world Y-up space.
    ResetReason reset_reason=ResetReason::first_sample;
    bool tracking=false;
};

// Deterministic bounded two-axis pendulum. Native integration must own a proven
// joint/palette and supply the actual hip pivot in metres on the engine thread.
// This standalone solver is NOT a live physics integration or a cloth modifier.
class Solver final {
public:
    static constexpr double length_metres=.22, gravity=9.81, damping_ratio=.30;
    // A short, time-based suspension response rejects pose quantization without
    // inventing idle sway. Starts/stops still supply all the pendulum's energy.
    static constexpr double acceleration_response_seconds=.065;
    static constexpr double max_angle=12.0*3.14159265358979323846/180.0;
    static constexpr double max_dt=.1, max_substep=1.0/240.0, teleport_metres=.75;
    const Pose& pose() const noexcept { return pose_; }
    void reset() noexcept { *this=Solver{}; }
    // Lost pose derivatives are discarded without throwing away the bob's
    // angle/velocity. Used only for a short gap with the same authenticated rig.
    void rebase(Vec3 pivot) noexcept {
        if(!finite(pivot)||norm({pivot.x-last_position_.x,pivot.y-last_position_.y,pivot.z-last_position_.z})>teleport_metres) return;
        last_position_=pivot;last_velocity_={};last_dt_=0;
        filtered_acceleration_={};velocity_valid_=false;
    }

    Pose update(Vec3 pivot,double dt,std::uint64_t epoch,bool enabled=true) noexcept {
        if(!enabled||epoch==0) { clear(ResetReason::disabled); return pose_; }
        if(!finite(pivot)||!std::isfinite(dt)||dt<=0) { clear(ResetReason::invalid_sample); return pose_; }
        if(dt>max_dt) { bind(pivot,epoch,ResetReason::frame_gap); return pose_; }
        if(!position_valid_) { bind(pivot,epoch,ResetReason::first_sample); return pose_; }
        if(epoch!=epoch_) { bind(pivot,epoch,ResetReason::character_epoch); return pose_; }
        Vec3 delta{pivot.x-last_position_.x,pivot.y-last_position_.y,pivot.z-last_position_.z};
        if(norm(delta)>teleport_metres) { bind(pivot,epoch,ResetReason::teleport); return pose_; }
        Vec3 velocity{delta.x/dt,delta.y/dt,delta.z/dt};
        if(!finite(velocity)) { clear(ResetReason::invalid_sample); return pose_; }
        // Consecutive secants are measured at interval midpoints. Their time
        // separation is half the sum of the intervals, including variable FPS.
        Vec3 acceleration{};
        if(velocity_valid_) {
            const double separation=(dt+last_dt_)*.5;
            acceleration={(velocity.x-last_velocity_.x)/separation,
                (velocity.y-last_velocity_.y)/separation,(velocity.z-last_velocity_.z)/separation};
            if(!finite(acceleration)) { clear(ResetReason::invalid_sample); return pose_; }
        }
        last_position_=pivot;last_velocity_=velocity;last_dt_=dt;velocity_valid_=true;
        pose_.reset_reason=ResetReason::none;pose_.tracking=true;
        // Impacts cannot produce unbounded energy. Vertical acceleration adjusts
        // effective gravity but never inverts the lantern or disables restoring force.
        const double horizontal=std::hypot(acceleration.x,acceleration.z);
        if(horizontal>25.0) { acceleration.x*=25.0/horizontal;acceleration.z*=25.0/horizontal; }
        acceleration.y=std::clamp(acceleration.y,-gravity*.75,gravity);
        const int steps=static_cast<int>(std::ceil(dt/max_substep));
        const double h=dt/steps;
        const double response=std::exp(-h*.5/acceleration_response_seconds);
        for(int i=0;i<steps;++i) {
            // Evaluate force at the midpoint and finish the exact exponential
            // filter step. This keeps the response independent of frame rate.
            auto approach=[&](double& value,double target){value=target+(value-target)*response;};
            approach(filtered_acceleration_.x,acceleration.x);
            approach(filtered_acceleration_.y,acceleration.y);
            approach(filtered_acceleration_.z,acceleration.z);
            const double g=gravity+filtered_acceleration_.y;
            integrate(pose_.lean_x,pose_.velocity_x,filtered_acceleration_.x,g,h);
            integrate(pose_.lean_z,pose_.velocity_z,filtered_acceleration_.z,g,h);
            constrain();
            approach(filtered_acceleration_.x,acceleration.x);
            approach(filtered_acceleration_.y,acceleration.y);
            approach(filtered_acceleration_.z,acceleration.z);
        }
        const double angle=std::hypot(pose_.lean_x,pose_.lean_z);
        const double s=angle>1e-12?std::sin(angle)/angle:1.0;
        pose_.direction={pose_.lean_x*s,-std::cos(angle),pose_.lean_z*s};
        return pose_;
    }
private:
    Pose pose_{};
    Vec3 last_position_{},last_velocity_{},filtered_acceleration_{};
    double last_dt_=0;
    std::uint64_t epoch_=0;
    bool position_valid_=false,velocity_valid_=false;
    static bool finite(Vec3 v) noexcept {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    static double norm(Vec3 v) noexcept {return std::hypot(v.x,v.y,v.z);}
    void clear(ResetReason reason) noexcept { reset();pose_.reset_reason=reason; }
    void bind(Vec3 p,std::uint64_t epoch,ResetReason reason) noexcept {
        clear(reason);last_position_=p;epoch_=epoch;position_valid_=true;
    }
    static void integrate(double& q,double& v,double acceleration,double g,double dt) noexcept {
        // Exact underdamped oscillator solution for constant acceleration over a
        // substep: q''+2*zeta*omega*q'+omega^2*q=-pivotAcceleration/length.
        const double omega=std::sqrt(g/length_metres), decay=damping_ratio*omega;
        const double frequency=omega*std::sqrt(1-damping_ratio*damping_ratio);
        const double target=-acceleration/g, a=q-target, b=(v+decay*a)/frequency;
        const double c=std::cos(frequency*dt),s=std::sin(frequency*dt),e=std::exp(-decay*dt);
        const double wave=a*c+b*s;
        q=target+e*wave;
        v=e*(-decay*wave+frequency*(-a*s+b*c));
    }
    void constrain() noexcept {
        const double angle=std::hypot(pose_.lean_x,pose_.lean_z);
        if(angle>max_angle) {
            const double nx=pose_.lean_x/angle,nz=pose_.lean_z/angle;
            pose_.lean_x=nx*max_angle;pose_.lean_z=nz*max_angle;
            // Remove only outward angular velocity; preserve inward settling
            // and tangential motion so the angular stop cannot add energy.
            const double outward=pose_.velocity_x*nx+pose_.velocity_z*nz;
            if(outward>0){pose_.velocity_x-=outward*nx;pose_.velocity_z-=outward*nz;}
        }
        const double speed=std::hypot(pose_.velocity_x,pose_.velocity_z);
        if(speed>3.0){pose_.velocity_x*=3.0/speed;pose_.velocity_z*=3.0/speed;}
    }
};
}
