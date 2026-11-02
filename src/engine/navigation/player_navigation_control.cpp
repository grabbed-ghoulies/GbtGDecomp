#include "player_navigation.h"
#include "../entities/actor_transform.h"
#include "../entities/body_shape_push.h"
#include "gbtg/anim.h"
#include "../entities/body_fields.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00136620
u32 handle_runtime_player_navigation_request(RuntimePlayerNavigationControl& context,
    std::span<const u8> message)
{
    const Vec3 target{read_f32(message, 0x10), read_f32(message, 0x14), read_f32(message, 0x18)};
    u32 failed;
    const u32 result = drive_runtime_player_navigation(context, target, -1,
        read_f32(message, 0x24) + 2.5f, read_f32(message, 0x28), read_f32(message, 0x2C), 0, failed);
    if (result == 0 && runtime_player_navigation_fallback(context, target))
    {
        return 1;
    }
    return result;
}

// Xbox 0x00137500
bool runtime_player_motion_allows_navigation(RuntimePlayerNavigationControl& context,
    u32& forced, float& alignment)
{
    if ((context.flags.bytes[1] & 0x10) != 0)
    {
        forced = 1;
        return true;
    }
    forced = 0;
    float x = context.body_actor.anchor.x;
    float z = context.body_actor.anchor.z;
    float y = 0.0f;
    if (x != 0.0f || z != 0.0f)
    {
        const float squared = x * x + z * z;
        if (squared != 0.0f)
        {
            const long double reciprocal = retail_reciprocal_sqrt(squared);
            x = float(static_cast<long double>(x) * reciprocal);
            y = float(0.0L * reciprocal);
            z = float(static_cast<long double>(z) * reciprocal);
        }
        const auto body = context.navigation.body;
        alignment = (read_f32(body, 0x1E0) * z + read_f32(body, 0x1D8) * x) +
            read_f32(body, 0x1DC) * y;
        return alignment < std::bit_cast<float>(u32(0xBF333333)); // DAT_0032B2EC.
    }
    return false;
}

// Xbox 0x00137610
bool runtime_player_input_aligns_with_motion(RuntimePlayerNavigationControl& context,
    float& alignment)
{
    Vec3 motion = context.body_actor.anchor;
    if (motion.x != 0.0f || motion.y != 0.0f || motion.z != 0.0f)
    {
        std::array<float, 2> axes;
        runtime_input_axes(context.input, axes, context.input_state);
        if (axes[0] != 0.0f || axes[1] != 0.0f)
        {
            motion.y = 0.0f;
            const float motion_squared = motion.z * motion.z + motion.x * motion.x;
            if (motion_squared != 0.0f)
            {
                const long double reciprocal = retail_reciprocal_sqrt(motion_squared);
                motion.x = float(static_cast<long double>(motion.x) * reciprocal);
                motion.y = float(0.0L * reciprocal);
                motion.z = float(static_cast<long double>(motion.z) * reciprocal);
            }
            float input_y = 0.0f;
            const float input_squared = axes[1] * axes[1] + axes[0] * axes[0];
            if (input_squared != 0.0f)
            {
                const long double reciprocal = retail_reciprocal_sqrt(input_squared);
                axes[1] = float(static_cast<long double>(axes[1]) * reciprocal);
                input_y = float(0.0L * reciprocal);
                axes[0] = float(reciprocal * static_cast<long double>(axes[0]));
            }
            alignment = (axes[0] * motion.z + axes[1] * motion.x) + input_y * motion.y;
            return alignment >= 0.5f;
        }
    }
    return false;
}

// Xbox 0x00137460
bool runtime_player_navigation_motion(RuntimePlayerNavigationControl& context,
    u32& forced, float& motion_alignment, float& input_alignment)
{
    forced = 0;
    if ((context.flags.bytes[2] & 0x20) != 0 &&
        runtime_player_motion_allows_navigation(context, forced, motion_alignment) &&
        runtime_player_input_aligns_with_motion(context, input_alignment))
    {
        return true;
    }
    const auto body = context.navigation.body;
    if (get(body, body_field::motion).x == 0.0f && get(body, body_field::motion).y == 0.0f &&
        get(body, body_field::motion).z == 0.0f)
    {
        return false;
    }
    motion_alignment = -1.0f; // DAT_0032B120.
    input_alignment = motion_alignment;
    forced = 0;
    return true;
}

// Xbox 0x00137790
bool runtime_player_navigation_target_is_near(RuntimePlayerNavigationControl& context,
    const Vec3& target)
{
    const Vec3 position = actor_world_position(context.navigation.actor);
    const float dx = position.x - target.x;
    const float dz = position.z - target.z;
    const float distance = float(retail_square_root(dz * dz + dx * dx));
    float cell;
    if (entity_detail::g_nav_grids[0] == nullptr)
    {
        cell = 0.0f;
    }
    else
    {
        cell = float(entity_detail::g_nav_grids[0]->cell);
    }
    const float reach = (read_f32(context.navigation.body, 0x888) * context.body_actor.radius_xz) *
        std::bit_cast<float>(u32(0x3FCCCCCD)) + cell; // DAT_0032B4B0.
    return distance - reach < 0.0f;
}

// Xbox 0x001373F0
bool runtime_player_navigation_fallback(RuntimePlayerNavigationControl& context, const Vec3& target)
{
    const i32 cell = entity_detail::navigation_cell_index(context.navigation.floor, target);
    if (cell != -1)
    {
        u32 forced;
        float motion_alignment;
        float input_alignment;
        if (!runtime_player_navigation_motion(context, forced, motion_alignment, input_alignment))
        {
            return false;
        }
        if (forced == 0 && !runtime_player_navigation_target_is_near(context, target))
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00137100
entity_detail::NavigationCellRegistration* suspend_runtime_navigation_attachment(
    RuntimePlayerNavigationAttachment* attachment, i32& floor)
{
    if (attachment != nullptr && attachment->state == 0)
    {
        entity_detail::NavigationCellRegistration* const registration = attachment->registration;
        floor = attachment->floor;
        if (registration != nullptr)
        {
            entity_detail::decrement_navigation_cell_counts(*registration, u32(floor));
        }
        return registration;
    }
    return nullptr;
}

// Xbox 0x00136B00
u32 drive_runtime_player_navigation(RuntimePlayerNavigationControl& context,
    const Vec3& target, i32 target_floor, float radius, float approach,
    float speed, u32 keep_attachment, u32& failed)
{
    using namespace entity_detail;
    if (target_floor < 0)
    {
        target_floor = navigation_floor_index(target, context.navigation.floor, 0);
    }
    failed = 0;
    u32 changed_floor = 0;
    if (approach < 2.5f)
    {
        approach = 2.5f;
    }
    const auto body = context.navigation.body;
    if (get(body, body_field::move_holds) != 0 || get(body, body_field::move_hold_time) != 0.0f)
    {
        return 0;
    }
    Vec3 direction = actor_world_position(context.navigation.actor);
    const float dx = target.x - direction.x;
    const float dz = target.z - direction.z;
    if (radius * radius >= dz * dz + dx * dx)
    {
        return 1;
    }
    i32 attached_floor;
    NavigationCellRegistration* attached_registration;
    if (keep_attachment == 0)
    {
        attached_registration = suspend_runtime_navigation_attachment(context.attachment, attached_floor);
    }
    const u32 path = request_runtime_player_navigation_path(context.navigation,
        target, target_floor, direction, changed_floor);
    if (keep_attachment == 0 && attached_registration != nullptr)
    {
        increment_navigation_cell_counts(*attached_registration, u32(attached_floor));
    }
    if (path == 0)
    {
        failed = 1;
        return 0;
    }
    float magnitude = speed;
    if (changed_floor == 0)
    {
        float minimum = std::bit_cast<float>(u32(0x3E19999A)); // DAT_0032B168.
        if (!(speed > minimum))
        {
            minimum = speed;
        }
        const float distance = float(retail_square_root(direction.z * direction.z + direction.x * direction.x));
        // 0x00136C8F reloads approach while the square-root argument is still
        // on the stack; 0x00136CB7 reloads speed after ADD ESP,4.
        if (approach != 2.5f)
        {
            magnitude = ((distance - 2.5f) / (approach - 2.5f)) * (speed - minimum) + minimum;
            if (speed > minimum)
            {
                if (magnitude > speed)
                {
                    magnitude = speed;
                }
                else if (minimum > magnitude)
                {
                    magnitude = minimum;
                }
            }
            else
            {
                if (speed > magnitude)
                {
                    magnitude = speed;
                }
                else if (magnitude > minimum)
                {
                    magnitude = minimum;
                }
            }
        }
    }
    direction.y = 0.0f;
    scale_runtime_collision_vector(direction, magnitude);
    set_runtime_input_axes(context.writable, {direction.z, direction.x}, context.keyboard);
    return 0;
}
}
