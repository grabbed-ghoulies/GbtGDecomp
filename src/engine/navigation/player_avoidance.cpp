#include "player_avoidance.h"
#include "../entities/player_mind_control.h"
#include "../entities/actor_transform.h"
#include "../entities/body_damage_motion.h"
#include "../entities/player_mind_events.h"
#include "gbtg/anim.h"
#include "../entities/body_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0013AC80
bool probe_runtime_navigation_heading(RuntimeNavigationHeadingSearch& context, float heading)
{
    const float radius = (context.maximum - context.minimum) * .5f + context.minimum;
    const Vec3 center{float(retail_sin(heading) * static_cast<long double>(radius) + context.origin.x),
        context.origin.y, float(retail_cos(heading) * static_cast<long double>(radius) + context.origin.z)};
    if (context.output != nullptr)
    {
        if (entity_detail::select_navigation_spawn_position(context.floor, &context.origin, center,
            0.f, radius, context.required, context.allowed, context.flags, context.group,
            *context.output, context.random, context.occupancy_initial))
        {
            if (context.keep_sample == 0)
            {
                *context.output = center;
            }
            return true;
        }
    }
    return false;
}

// Xbox 0x0013AA50
bool search_runtime_navigation_headings(RuntimeNavigationHeadingSearch& context,
    float heading, float increment, i32 attempts)
{
    if (probe_runtime_navigation_heading(context, heading))
    {
        return true;
    }
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    const i32 limit = std::bit_cast<i32>(u32(attempts) + 1);
    for (i32 index = 1; index < limit; ++index)
    {
        const float delta = float(index) * increment;
        if (fraction > .5f)
        {
            if (probe_runtime_navigation_heading(context, delta + heading))
            {
                return true;
            }
            if (probe_runtime_navigation_heading(context, heading - delta))
            {
                return true;
            }
        }
        else
        {
            if (probe_runtime_navigation_heading(context, heading - delta))
            {
                return true;
            }
            if (probe_runtime_navigation_heading(context, delta + heading))
            {
                return true;
            }
        }
    }
    return false;
}

// Xbox 0x00100C70
float randomize_runtime_navigation_heading(float heading, float width, u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float half = width * .5f;
    const float negative = 0.f - half;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    const long double remainder = animation_remainder((half - negative) * fraction + negative + heading,
        std::bit_cast<float>(0x40C90FDBu));
    float result = float(remainder);
    if (0.L > remainder)
    {
        result += std::bit_cast<float>(0x40C90FDBu);
    }
    return result;
}

// Xbox 0x0007F980
bool avoid_runtime_player_navigation(RuntimePlayerNavigationAvoidance& context)
{
    if (body_record(context.body).move_holds != 0 || body_record(context.body).move_hold_time != 0.f)
    {
        return false;
    }
    std::array<float, 2> axes;
    runtime_input_axes(context.input, axes, context.input_state);
    const float squared = axes[1] * axes[1] + axes[0] * axes[0];
    if (squared > std::bit_cast<float>(0x3C23D70Bu))
    {
        return false;
    }
    const Vec3 position = actor_world_position(context.actor);
    const float heading = player_mind_record(context.mind).unk_194 == 0 ? context.actor.euler.y
        : player_mind_record(context.mind).unk_190;
    const float randomized = randomize_runtime_navigation_heading(heading, 0.f, context.random);
    Vec3 output;
    RuntimeNavigationHeadingSearch search{position, context.floor, 12.f, 15.5f,
        nullptr, nullptr, &output, 1, 9, 0, context.random, context.occupancy_initial};
    if (!search_runtime_navigation_headings(search, randomized,
        std::bit_cast<float>(0x3E9C61AAu), 10))
    {
        return false;
    }
    const Vec3 delta{output.x - position.x, output.y - position.y, output.z - position.z};
    runtime_direction_angle(delta, context.retained_direction);
    Vec3 direction;
    runtime_horizontal_direction(direction, context.retained_direction, 1.f);
    set_runtime_input_axes(context.writable, {direction.x, direction.z}, context.keyboard);
    const u32 angle = std::bit_cast<u32>(context.retained_direction);
    const u32 enabled = 1;
    std::memcpy(context.mind.data() + 0x190, &angle, 4);
    std::memcpy(context.mind.data() + 0x194, &enabled, 4);
    return true;
}
}
