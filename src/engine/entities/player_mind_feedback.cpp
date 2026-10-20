#include "player_mind_feedback.h"
#include "actor_transform.h"
#include "entity_heap.h"
#include "gbtg/entity.h"
#include <new>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x000419D0
float runtime_actor_direction_to_point(const Actor& actor, const Vec3& point)
{
    const Vec3 position = actor_world_position(actor);
    const Vec3 direction{point.x - position.x, point.y - position.y, point.z - position.z};
    float angle;
    runtime_direction_angle(direction, angle);
    return angle;
}

// Xbox 0x0002B560
void set_runtime_player_actor_rotation(Actor& actor, const Actor& body_actor,
    std::span<u8> body, const Vec3& rotation)
{
    normalize_euler_angles(actor.euler, rotation);
    actor.xform_valid = 0;
    const Vec3 world_rotation = actor_world_euler(body_actor);
    std::memcpy(body.data() + 0x72C, &world_rotation, sizeof(world_rotation));
}

// Xbox 0x00140A40
bool clear_runtime_player_mind_reference(std::span<u8> mind, size_t offset, u32 actor)
{
    if (read_u32(mind, offset) == actor)
    {
        put(mind, offset, 0);
        return true;
    }
    return false;
}

// Xbox 0x00123E50
bool runtime_player_mind_uses_special_event(u32 kind)
{
    return kind == 4 || kind == 0xE || kind == 0x11;
}

// Xbox 0x00028CA0
u8 runtime_player_mind_active(const std::span<const u8>* mind)
{
    if (mind != nullptr)
    {
        return (*mind)[0x20];
    }
    return 0;
}

// Xbox 0x00137340
void select_runtime_player_mind_alert(std::span<u8> mind, std::span<const u8> body,
    const EntityBitBlock& flags, u32 player_link)
{
    if (read_f32(mind, 0x50) > 0.f)
    {
        put(mind, 0x4C, 0x41);
        return;
    }
    if (mind[0x20] != 0)
    {
        put(mind, 0x4C, 0x42);
        return;
    }
    const u16 maximum = read_u16(body, 0x96);
    const bool low_health = maximum != 0 &&
        float(read_u16(body, 0x94)) / float(maximum) <= 0.25f;
    if (player_link == 0)
    {
        if ((flags.bytes[4] & 0x40) != 0)
        {
            put(mind, 0x4C, 0x45);
            return;
        }
        if (!low_health)
        {
            put(mind, 0x4C, 0x44);
            return;
        }
    }
    else if (!low_health)
    {
        put(mind, 0x4C, 0x40);
        return;
    }
    put(mind, 0x4C, 0x46);
}

// Xbox 0x00136700
bool initialize_runtime_player_mind_navigation(std::span<u8> mind, i32 floor,
    std::span<entity_detail::NavGrid* const> grids, u8* nodes, RuntimeMindStorage*& storage,
    const std::function<u32(const void*)>& address_of)
{
    const auto* grid = grids[size_t(floor)];
    if (grid != nullptr)
    {
        const u32 count = u32(grid->width) * u32(grid->depth);
        auto* allocation = static_cast<RuntimeMindStorage*>(engine_heap_allocate(sizeof(RuntimeMindStorage)));
        if (allocation != nullptr)
        {
            auto* search = new (allocation) RuntimeMindStorage{};
            entity_detail::reset_navigation_path_search(*search, nodes, grid, count);
            storage = search;
            put(mind, 0x1C, address_of(search));
            return true;
        }
    }
    return false;
}

// Xbox 0x0007FC30
void update_runtime_player_mind_health_display(std::span<const u8> mind,
    std::span<const u8> body, const EntityBitBlock& flags, RuntimePlayerHealthDisplay& display)
{
    if (mind[0x20] != 0 || (flags.bytes[5] & 1) != 0 || display.override_health != 0 ||
        read_u16(body, 0x94) <= 10)
    {
        display.intensity = 2.f;
        acquire_runtime_player_health_textures(display);
    }
    set_runtime_player_health_display(display, read_u16(body, 0x94), read_f32(mind, 0x15C) > 0.f ? 1 : 0);
}

// Xbox 0x0007FB10
void tick_runtime_player_mind(RuntimePlayerMindTick& context, std::span<u8> input)
{
    auto& base = context.base;
    if (read_u32(base.events.mind, 0x7C) != 0)
    {
        switch (read_u32(input, 0))
        {
        case 1:
        case 2:
            clear_runtime_input_states(input);
            break;
        case 4:
            reset_runtime_composite_input(input, context.input_release);
            break;
        case 5:
            clear_runtime_actor_input_states(input);
            break;
        default:
            break;
        }
    }
    select_runtime_player_mind_alert(base.events.mind, base.body, *context.flags, context.player_link);
    update_runtime_mind_requests(base);
    update_runtime_mind_events(base);
    const float step = runtime_frame_step(base.clock);
    const float rate = runtime_actor_rate(base.actor);
    put(base.events.mind, 0x34,
        std::bit_cast<u32>(rate * step + read_f32(base.events.mind, 0x34)));
    update_runtime_mind_alert(base);
    const size_t group = read_u16(context.actor_bytes, 0xCDC) == 2 ||
        read_u32(context.actor_bytes, 0x93C) != 0 ? 1 : 0;
    if ((context.references[group] == 0 && base.clock.decay_enabled[base.clock.decay_index] != 0) ||
        (context.flags->bytes[3] & 8) != 0)
    {
        update_runtime_player_mind_health_display(base.events.mind, base.body, *context.flags, context.health);
    }
}

// Xbox 0x0007DAC0
void clear_runtime_player_mind_body_event(std::span<u8> body)
{
    if (body[0x6A] == 0)
    {
        put(body, 0x88, 0);
    }
}

// Xbox 0x0007DAE0
void notify_runtime_player_mind_release(RuntimePlayerMindFeedback& context)
{
    if (read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            dispatch_runtime_body_position_event(position, 0x1F, context.events);
        }
    }
}

// Xbox 0x0007DC40
void notify_runtime_player_mind_impact(RuntimePlayerMindFeedback& context)
{
    if (read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            dispatch_runtime_body_position_event(position, 0x1B, context.events);
        }
    }
}

// Xbox 0x0007DC80
void notify_runtime_player_mind_activation(RuntimePlayerMindFeedback& context)
{
    if (read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            dispatch_runtime_body_position_event(position, 0x23, context.events);
        }
    }
}

// Xbox 0x0007E420
void notify_runtime_player_mind_event(RuntimePlayerMindFeedback& context, i32 kind)
{
    if (read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            dispatch_runtime_body_position_event(position,
                read_u16(context.event_ids, size_t(kind) * 2), context.events);
        }
    }
}

// Xbox 0x0007DBE0
void notify_runtime_player_mind_contact(RuntimePlayerMindFeedback& context, u32 payload, u32 target)
{
    if (payload != 0)
    {
        if (read_u32(context.input, 4) != 0)
        {
            if (runtime_input_is_selected(context.input, context.input_address, context.activation))
            {
                const Vec3 position = actor_world_position(context.actor);
                dispatch_runtime_body_position_event(position, 0x2A, context.events);
            }
        }
        return;
    }
    notify_runtime_player_mind_event(context, i32(target != 0) + 1);
}

// Xbox 0x0007DCC0
void notify_runtime_player_mind_source(RuntimePlayerMindFeedback& context, std::span<const u8> message)
{
    if (read_u32(message, 0x1C) != 0)
    {
        notify_runtime_player_mind_event(context, i32(read_u32(message, 0x10) != 0) + 1);
    }
}

// Xbox 0x0007DCE0
void notify_runtime_player_mind_target(RuntimePlayerMindFeedback& context, std::span<const u8> message)
{
    if (read_u32(message, 0x10) != 0 && read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            dispatch_runtime_body_position_event(position, 0x24, context.events);
        }
    }
}

// Xbox 0x0007E470
void shake_runtime_player_mind_camera(RuntimePlayerMindFeedback& context, const char* name)
{
    if (read_u32(context.input, 4) != 0)
    {
        if (runtime_input_is_selected(context.input, context.input_address, context.activation))
        {
            const Vec3 position = actor_world_position(context.actor);
            load_actor_camera_shake(name, position, 1.f, context.registry, context.error);
        }
    }
}

// Xbox 0x0007DB20
void reset_runtime_player_mind_body_event(RuntimePlayerMindFeedback& context,
    std::span<const u8> target, const std::span<const u8>* actor_mind)
{
    if (read_u32(context.mind, 0x10) != 0)
    {
        const Vec3 position = actor_world_position(context.actor);
        const float z = position.z - read_f32(target, 0x28);
        const float x = position.x - read_f32(target, 0x20);
        const float y = position.y - read_f32(target, 0x24);
        if (900.f > (z * z + x * x) + y * y)
        {
            put(context.body, 0x88, 0);
            return;
        }
    }
    const u8 state = actor_mind == nullptr ? 0 : (*actor_mind)[0x20];
    if (read_f32(context.mind, 0x9C) == 0.f && state == 0)
    {
        put(context.mind, 0xA0, 0x3F666666u);
    }
    put(context.body, 0x88, 0);
}

// Xbox 0x0007DEE0
void track_runtime_player_mind_source(std::span<u8> mind,
    const std::span<const u8>* body_state, u32 present, u32 source)
{
    if (present != 0)
    {
        // Body +0x84 zero (an empty view) reads as 0.
        const u8 state = body_state == nullptr || body_state->empty() ? 0 : (*body_state)[3];
        if (state == 3 || state == 5 ||
            (read_u32(mind, 0x84) != 0 && source != read_u32(mind, 0x84)))
        {
            put(mind, 0x8C, 0);
            mind[0x90] = 0;
        }
        put(mind, 0x84, source);
        put(mind, 0x8C, 0x3FC00000u);
    }
}

// Xbox 0x0007E5C0
void record_runtime_player_mind_distance(std::span<u8> mind, float distance)
{
    if (!(distance > 50.f))
    {
        if (read_u32(mind, 0x124) == 0)
        {
            put(mind, 0x124, 1);
            put(mind, 0x120, std::bit_cast<u32>(distance));
            return;
        }
        if (read_f32(mind, 0x120) > distance)
        {
            put(mind, 0x120, std::bit_cast<u32>(distance));
        }
    }
}

// Xbox 0x0007F100
void record_runtime_player_mind_position(std::span<u8> mind, const Actor& actor,
    u32 source, const Vec3& position)
{
    if (read_u32(mind, 0x134) != 0)
    {
        const Vec3 world = actor_world_position(actor);
        const float x = position.x - world.x;
        const float y = position.y - world.y;
        const float z = position.z - world.z;
        const float old_x = read_f32(mind, 0x138) - world.x;
        const float old_y = read_f32(mind, 0x13C) - world.y;
        const float old_z = read_f32(mind, 0x140) - world.z;
        if ((old_z * old_z + old_y * old_y) + old_x * old_x < (z * z + x * x) + y * y)
        {
            return;
        }
    }
    put(mind, 0x134, source);
    put(mind, 0x138, std::bit_cast<u32>(position.x));
    put(mind, 0x13C, std::bit_cast<u32>(position.y));
    put(mind, 0x140, std::bit_cast<u32>(position.z));
}
}
