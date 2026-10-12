#include "mind_navigation_angles.h"
#include "../entities/body_damage_motion.h"
#include "../entities/player_mind_feedback.h"
#include "gbtg/anim.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00105DD0
void scale_runtime_planar_direction(std::array<float, 2>& direction, float length)
{
    const float square = direction[0] * direction[0] + direction[1] * direction[1];
    if (square != 0.f)
    {
        const long double scale = static_cast<long double>(length) / retail_square_root(square);
        direction[0] = float(scale * direction[0]);
        direction[1] = float(scale * direction[1]);
    }
}

// Xbox 0x00100E90
float interpolate_runtime_mind_angle(float angle, float reference, float fraction)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const float pi = std::bit_cast<float>(0x40490FDBu);
    const float negative_pi = std::bit_cast<float>(0xC0490FDBu);
    const long double remainder = animation_remainder(angle - reference, tau);
    float difference = float(remainder);
    if (remainder < static_cast<long double>(negative_pi))
    {
        difference += tau;
    }
    if (difference >= pi)
    {
        difference -= tau;
    }
    const long double result = animation_remainder(difference * fraction + reference, tau);
    float output = float(result);
    if (result < static_cast<long double>(negative_pi))
    {
        output += tau;
    }
    if (output >= pi)
    {
        output -= tau;
    }
    return output;
}

namespace
{
// The 0x00152BE0 remainder wrapped into [-pi, pi) as the retail angle helpers inline it.
float wrap_signed_angle(float value)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu);
    const long double remainder = animation_remainder(value, tau);
    float result = float(remainder);
    if (remainder < static_cast<long double>(std::bit_cast<float>(0xC0490FDBu)))
    {
        result += tau;
    }
    if (result >= std::bit_cast<float>(0x40490FDBu))
    {
        result -= tau;
    }
    return result;
}
}

// Xbox 0x00100D00
float reflect_runtime_angle(float angle, float reference)
{
    return wrap_signed_angle(wrap_signed_angle(angle - reference) + angle);
}

// Xbox 0x00100B80
float nearest_runtime_angle(float reference, float first, float second)
{
    const float first_difference = wrap_signed_angle(first - reference);
    const float second_difference = wrap_signed_angle(second - reference);
    const float second_size = float(engine_absolute(second_difference));
    if (second_size > static_cast<long double>(engine_absolute(first_difference)))
    {
        return first;
    }
    return second;
}

// Xbox 0x00136030
void randomize_runtime_mind_escape_heading(const Actor& actor,
    std::span<const u8> target, const Vec3& destination, float& heading, u32& random)
{
    const float target_angle = runtime_actor_bound_angle(target, actor);
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    const float destination_angle = runtime_actor_direction_to_point(actor, destination);
    const float selected = (fraction * 60.f * std::bit_cast<float>(0x3C8EFA35u)
        + (target_angle + std::bit_cast<float>(0x40490FDBu))) - std::bit_cast<float>(0x3F060A92u);
    heading = interpolate_runtime_mind_angle(selected, destination_angle, .5f);
}
}
