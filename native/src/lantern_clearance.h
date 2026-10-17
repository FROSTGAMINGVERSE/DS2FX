// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "lantern_pendulum.h"

namespace fx::lantern_clearance {
using Vec=lantern_pendulum::Vec3;
inline Vec add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec sub(Vec a,Vec b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline Vec scale(Vec a,double s){return {a.x*s,a.y*s,a.z*s};}
inline double dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
struct Capsule {Vec a,b;double radius;};
struct Contact {Vec correction{},normal{};bool touching=false;};
// Compliant contact displacement in world space. A critically damped spring
// filters changing contact normals without projecting the visible mesh out in
// one frame. Release uses the same dynamics, retaining position and velocity.
class SoftContact final {
public:
    static constexpr double max_offset=.065, frequency=16.0;
    Vec position() const noexcept{return position_;}
    Vec velocity() const noexcept{return velocity_;}
    void reset() noexcept{position_={};velocity_={};}
    Vec update(Vec target,double dt) noexcept {
        if(!std::isfinite(target.x)||!std::isfinite(target.y)||!std::isfinite(target.z)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return position_;}
        const double n=std::sqrt(dot(target,target));if(n>max_offset)target=scale(target,max_offset/n);
        const double e=std::exp(-frequency*dt);
        auto advance=[&](double& x,double& v,double goal){
            const double y=x-goal,b=v+frequency*y;
            x=goal+(y+b*dt)*e;v=(v-frequency*b*dt)*e;
        };
        advance(position_.x,velocity_.x,target.x);advance(position_.y,velocity_.y,target.y);advance(position_.z,velocity_.z,target.z);
        return position_;
    }
private:
    Vec position_{},velocity_{};
};
// Closest points on finite segments, including parallel and degenerate cases.
inline Contact separate(Capsule lamp,Capsule body,Vec fallback) noexcept {
    const auto u=sub(lamp.b,lamp.a),v=sub(body.b,body.a),w=sub(lamp.a,body.a);
    const double a=dot(u,u),b=dot(u,v),c=dot(v,v),d=dot(u,w),e=dot(v,w);
    double s=0,t=0;
    if(a<1e-12)t=c>1e-12?std::clamp(e/c,0.0,1.0):0;
    else if(c<1e-12)s=std::clamp(-d/a,0.0,1.0);
    else {
        const double den=a*c-b*b;
        s=den>1e-12?std::clamp((b*e-c*d)/den,0.0,1.0):0;
        t=(b*s+e)/c;
        if(t<0){t=0;s=std::clamp(-d/a,0.0,1.0);}
        else if(t>1){t=1;s=std::clamp((b-d)/a,0.0,1.0);}
    }
    const Vec delta=sub(add(lamp.a,scale(u,s)),add(body.a,scale(v,t)));
    const double distance=std::sqrt(dot(delta,delta)),limit=lamp.radius+body.radius;
    if(distance>=limit-1e-7)return {};
    const double fallback_length=std::sqrt(dot(fallback,fallback));
    const Vec normal=distance>1e-9?scale(delta,1/distance):
        (fallback_length>1e-9?scale(fallback,1/fallback_length):Vec{1,0,0});
    return {scale(normal,limit-distance),normal,true};
}
}
