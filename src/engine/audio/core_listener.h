#pragma once
#include "gbtg/entity.h"
#include <functional>

namespace gbtg
{
// The XACT engine's 3D listener as the audio core keeps it.
struct AudioCoreListener
{
    float unit_scale = 0.f; // DAT_004D97F0, 0x0015B960 stores 1.0.
    u32 apply = 0; // DAT_004D97EC, deferred (1) when DAT_00336140 is set.
    Vec3 position{}; // DAT_004D9820
    Vec3 velocity{}; // DAT_004D982C
    Vec3 front{}; // DAT_004D9838
    Vec3 top{}; // DAT_004D9844
    std::function<void(const Vec3&, u32)> set_position; // IXACTEngine_SetListenerPosition
    std::function<void(const Vec3&, u32)> set_velocity; // IXACTEngine_SetListenerVelocity
    std::function<void(const Vec3&, const Vec3&, u32)> set_orientation; // IXACTEngine_SetListenerOrientation
};
void set_audio_listener_position(const Vec3& position, AudioCoreListener& listener); // Xbox 0x0015BCB0
void set_audio_listener_velocity(const Vec3& velocity, AudioCoreListener& listener); // Xbox 0x0015B7F0
// A null front or top keeps the stored one.
void set_audio_listener_orientation(const Vec3* front, const Vec3* top,
    AudioCoreListener& listener); // Xbox 0x0015B840
// The listener follows the current view (DAT_005077A8): its position (+0), the constant velocity DAT_00421C70,
// its forward (+0x1C) as front, and a top vector built from the view quaternion (+0xC).
void update_audio_listener_from_view(const Vec3& position, const Quat& rotation, const Vec3& forward,
    const Vec3& velocity, AudioCoreListener& listener); // Xbox 0x0003A040
}
