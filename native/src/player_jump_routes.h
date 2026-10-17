#pragma once
#include <cmath>
#include <cstdint>

namespace fx::jump_routes {
enum class Route : uint32_t { native=0, idle=1, moving=2, sprint=3 };
struct Movement {
    uint32_t mask=0;
    float speed=0, x=0, z=0;
    uint8_t moving=0;
    bool known=false;
};
// Called only for a canonical native Jump8 request. Unknown/inconsistent
// observations retain the original message; this never synthesizes input.
inline Route classify(const Movement& m) noexcept {
    if(!m.known || !(m.mask&0x40) || !std::isfinite(m.speed) ||
       !std::isfinite(m.x) || !std::isfinite(m.z) || m.speed<0 || m.speed>20 ||
       m.moving>1 || std::abs(m.x)>1.1f || std::abs(m.z)>1.1f) return Route::native;
    const float length=m.x*m.x+m.z*m.z;
    // Only the paired sprint bits observed in the actual jump test qualify.
    // A sprint acceleration/uncertain sample falls back to the native jump.
    if((m.mask&0x10020)==0x10020)
        return m.moving && length>.0025f && m.speed>=5.8f ? Route::sprint : Route::native;
    if(m.mask&0x10020) return Route::native;
    if(m.speed<=.05f && !m.moving && length<=.0025f) return Route::idle;
    if(m.speed>.05f && m.moving && length>.0025f) return Route::moving;
    return Route::native;
}
}
