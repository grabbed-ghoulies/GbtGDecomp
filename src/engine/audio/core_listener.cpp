#include "core_listener.h"

namespace gbtg
{
// Xbox 0x0015BCB0
void set_audio_listener_position(const Vec3& position, AudioCoreListener& listener)
{
    listener.set_position({listener.unit_scale * position.x, listener.unit_scale * position.y,
        listener.unit_scale * position.z}, listener.apply);
    listener.position = position;
}

// Xbox 0x0015B7F0
void set_audio_listener_velocity(const Vec3& velocity, AudioCoreListener& listener)
{
    listener.set_velocity(velocity, listener.apply);
    listener.velocity = velocity;
}

// Xbox 0x0015B840
void set_audio_listener_orientation(const Vec3* front, const Vec3* top, AudioCoreListener& listener)
{
    const Vec3& used_front = front == nullptr ? listener.front : *front;
    const Vec3& used_top = top == nullptr ? listener.top : *top;
    listener.set_orientation(used_front, used_top, listener.apply);
    if (front != nullptr)
    {
        listener.front = *front;
    }
    if (top != nullptr)
    {
        listener.top = *top;
    }
}

// Xbox 0x0003A040
void update_audio_listener_from_view(const Vec3& position, const Quat& rotation, const Vec3& forward,
    const Vec3& velocity, AudioCoreListener& listener)
{
    const Vec3 front = forward;
    const float x = rotation.x;
    const float z = rotation.z;
    const float y = rotation.y;
    const float w = rotation.w;
    const float a = (w * 0.f - z * -1.f) + y * 0.f;
    const float c = (w * 0.f - y * 0.f) + x * -1.f;
    const float b = (w * -1.f - x * 0.f) + z * 0.f;
    const float d = y * -1.f + z * 0.f + x * 0.f;
    const Vec3 top{(y * c + d * x + w * a) - z * b, (w * b + y * d + z * a) - c * x,
        (w * c + z * d + b * x) - y * a};
    set_audio_listener_position(position, listener);
    set_audio_listener_velocity(velocity, listener);
    set_audio_listener_orientation(&front, &top, listener);
}
}
