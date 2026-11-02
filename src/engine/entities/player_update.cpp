#include "player_update.h"
#include "actor_attachment_requests.h"
#include "actor_transform.h"
#include "actor_world_lists.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "../navigation/navigation.h"
#include "body_fields.h"
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

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The actor messages in this file are a single stack word holding the id.
void send_word(RuntimeBodyAttachedActor& self, u32 id)
{
    const u32 message[1] = {id};
    self.message(message);
}

void tick_status_timer(RuntimeBodyAttachedActor& self, const RuntimeActorClock& clock, size_t offset,
    u32 id)
{
    if (!runtime_decay_enabled(clock))
    {
        return;
    }
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(*self.actor);
    const float timer = read_f32(self.bytes, offset);
    if (timer > 0.f)
    {
        const float remaining = timer - rate * step;
        put_float(self.bytes, offset, remaining);
        if (remaining <= 0.f)
        {
            put_float(self.bytes, offset, 0.f);
            send_word(self, id);
        }
    }
}
}

// Xbox 0x00019520
void tick_runtime_player_status_timers(RuntimeBodyAttachedActor& self, const RuntimeActorClock& clock)
{
    tick_status_timer(self, clock, 0x974, 0x55);
    tick_status_timer(self, clock, 0x978, 0x56);
}

// Xbox 0x0002B450
void track_runtime_player_room(RuntimePlayerRoomTracking& context)
{
    std::span<u8> actor = context.self.bytes;
    const Vec3 position = actor_world_position(*context.self.actor);
    const OccupancyRecord* record = find_occupancy_record(position, 0.0f, 4, nullptr, 0);
    context.current = record;
    if (read_u32(actor, 0xCB8) == 0)
    {
        if (record != nullptr && record != context.allowed &&
            (record->group == 0 || u16(record->group) == read_u16(actor, 6)))
        {
            put(actor, 0xCB8, 1);
        }
        else if (get(context.body, body_field::move_holds) == 0 &&
                 get(context.body, body_field::move_hold_time) == 0.0f)
        {
            put_float(actor, 0xCAC, position.x);
            put_float(actor, 0xCB0, position.y);
            put_float(actor, 0xCB4, position.z);
        }
    }
    if (context.strategy_present && read_u16(actor, 0xCDC) == 1 && read_u32(actor, 0x93C) == 0 &&
        read_u32(actor, 0xCB8) != 0)
    {
        send_word(context.self, 0x5D);
    }
}

// Xbox 0x0002C980
void check_runtime_player_bounds(RuntimePlayerBoundsCheck& context)
{
    const RuntimeWorldBounds& bounds = context.bounds;
    std::span<const u8> actor = context.self.bytes;
    const Vec3 position = actor_world_position(*context.self.actor);
    bool inside;
    if (bounds.enabled == 0)
    {
        inside = true;
    }
    else
    {
        float top = bounds.floor_y;
        if (!(bounds.floor_y >= position.y))
        {
            top = position.y;
        }
        top = top + 1.0f; // DAT_0032B0F8.
        inside = !(bounds.min_x > position.x || position.x >= bounds.max_x ||
            bounds.min_y > position.y || position.y >= top ||
            bounds.min_z > position.z || position.z >= bounds.max_z);
    }
    bool off_navigation = false;
    if (read_u16(actor, 0xCDE) == 1 && (context.body_bits->bytes[4] & 0x10) == 0 &&
        navigation_cell_index(i32(read_u32(actor, 0xCEC)), position) == -1)
    {
        off_navigation = true;
    }
    if (inside)
    {
        if (!off_navigation)
        {
            return;
        }
        const RuntimePlayerGoalRing* ring = context.ring;
        if (ring != nullptr)
        {
            u16 kind;
            if (ring->tail == ring->capacity)
            {
                kind = read_u16(context.address_zero, 0);
            }
            else
            {
                kind = read_u16(ring->rows, size_t(ring->stride) * ring->head);
            }
            if (kind != 0)
            {
                return;
            }
        }
    }
    send_word(context.self, 0x33);
}

// Xbox 0x0002C2B0
void update_runtime_player(RuntimePlayerUpdate& context)
{
    RuntimeBodyAttachedActor& self = context.self;
    Actor& actor = *self.actor;
    std::span<u8> bytes = self.bytes;
    std::span<u8> body = context.body.body;
    Actor& owner = context.body.owner;
    const RuntimeActorClock& clock = context.clock;
    const u32 paused = i32(read_u32(bytes, 0xCD8)) > 0 ? 1 : 0;
    if (read_u32(context.input, 0) == 1)
    {
        reset_runtime_analog_input(context.input);
    }
    else if (read_u32(context.input, 4) != 0)
    {
        bool reset = false;
        const RuntimePlayerGoalRing* ring = context.ring;
        if (ring != nullptr)
        {
            u16 kind;
            if (ring->tail == ring->capacity)
            {
                kind = read_u16(context.address_zero, 0);
            }
            else
            {
                kind = read_u16(ring->rows, size_t(ring->stride) * ring->head);
            }
            reset = kind != 0;
        }
        if (reset)
        {
            reset_runtime_input_device(context.input, context.input_release);
        }
        else
        {
            refresh_runtime_input_device(context.input_address, context.input_frame, context.input_activation,
                context.input_release);
        }
    }
    if (paused == 0)
    {
        if (context.strategy.bytes != nullptr)
        {
            context.strategy_tick(context.strategy);
        }
        if (context.mind.bytes != nullptr)
        {
            context.mind_tick(context.mind, read_u32(bytes, 0x960));
        }
    }
    context.body_tick(context.body_object, read_u32(bytes, 0x960));
    context.body_bits->bytes[0] &= 0xFB;
    context.body_bits->bytes[0] &= 0xF7;
    float step = runtime_frame_step(clock);
    float rate = runtime_actor_rate(actor);
    const bool root_bone = read_u16(bytes, 0x194) == 0;
    if (context.animation.controller != nullptr)
    {
        tick_animation_controller(*context.animation.controller, rate * step, root_bone,
            context.frame.effects.models.registry);
    }
    context.body_animate(context.body_object);
    if (context.animation.controller != nullptr)
    {
        apply_controller_to_model(*context.animation.controller, context.model, root_bone,
            context.frame.pose_callbacks, context.frame.node_cache, context.frame.reset_matrix);
    }
    context.body_bits->bytes[0] &= 0xFD;
    if (paused == 0)
    {
        if (get(body, body_field::holds) == 0)
        {
            const u32 filter = actor.model_flags;
            actor.model_flags = 0;
            const i32 shape_mode = actor.model_mode;
            actor.model_mode = 0;
            register_actor_shape_probe(actor, context.shape_probe);
            push_runtime_body_spheres(context.body);
            const Vec3 position = actor_world_position(actor);
            put_float(body, 0x24, position.x);
            put_float(body, 0x28, position.y);
            put_float(body, 0x2C, position.z);
            if (context.body_supported(context.body_object) == 0)
            {
                context.body_bits->bytes[1] |= 0x10;
            }
            else
            {
                context.body_bits->bytes[1] &= 0xEF;
            }
            actor.model_flags = filter;
            actor.model_mode = shape_mode;
            register_actor_shape_probe(actor, context.shape_probe);
        }
        else
        {
            body[0x716] = 0;
            const Vec3 local = get(body, body_field::position);
            const Vec3 world = actor_world_position(actor);
            const Vec3 delta{local.x - world.x, local.y - world.y, local.z - world.z};
            record_runtime_body_motion(body.subspan(0x340, 4 + 0x50 * 12), delta, owner, clock);
            put_float(body, 0x704, delta.x);
            put_float(body, 0x708, delta.y);
            put_float(body, 0x70C, delta.z);
            actor_set_world_position(actor, local);
            const Vec3 settled = actor_world_position(owner);
            put_float(body, 0x18, settled.x);
            put_float(body, 0x1C, settled.y);
            put_float(body, 0x20, settled.z);
        }
    }
    actor_refresh_transform(actor);
    ActorFrameAttachment* update = context.updates;
    while (update != nullptr)
    {
        ActorFrameAttachment* next = update->next;
        update->update(*update, actor);
        update = next;
    }
    update_actor_attachment_effects(actor, context.attachments, context.pending, context.frame.effects);
    float z = read_f32(bytes, 0x32C);
    float y = read_f32(bytes, 0x328);
    float x = read_f32(bytes, 0x324);
    if (context.frame.ease != 0)
    {
        constexpr float kFraction = std::bit_cast<float>(0x3DCCCCCDu); // DAT_0032B254.
        x = (x - read_f32(bytes, 0x318)) * kFraction + read_f32(bytes, 0x318);
        y = (y - read_f32(bytes, 0x31C)) * kFraction + read_f32(bytes, 0x31C);
        z = (z - read_f32(bytes, 0x320)) * kFraction + read_f32(bytes, 0x320);
    }
    put_float(bytes, 0x318, x);
    put_float(bytes, 0x31C, y);
    put_float(bytes, 0x320, z);
    if (actor.model != nullptr && actor.model->runtime->shader_animations != nullptr)
    {
        tick_runtime_model_shader_animations(*actor.model->runtime->shader_animations, 1.f,
            context.frame.shader_matrices);
    }
    context.body_settle(context.body_object);
    if (paused == 0 && context.collision != nullptr)
    {
        RuntimeBodyContactWalk walk{owner, *context.collision, context.contact_callback, context.contact_actor,
            context.players, clock};
        update_runtime_body_contacts(walk);
    }
    u16 key;
    if (context.actions.second.bytes == nullptr)
    {
        key = 0;
    }
    else
    {
        key = context.action_key(context.actions.second, read_u32(context.owner_bytes, 0x960));
    }
    if (key != 0 && context.actions.table != nullptr)
    {
        u32 parameters;
        const u8* row = find_runtime_script_table_row(context.actions.table, key, parameters);
        if (row != nullptr)
        {
            // 0x00027510 writes a zero optional-template token on every match.
            select_runtime_body_action_row(body, row, nullptr, context.actions);
        }
    }
    const Vec3 settled = actor_world_position(owner);
    put_float(body, 0x18, settled.x);
    put_float(body, 0x1C, settled.y);
    put_float(body, 0x20, settled.z);
    put_float(body, 0x320, 0.0f);
    put_float(body, 0x31C, 0.0f);
    put_float(body, 0x318, 0.0f);
    put_float(body, 0x33C, owner.euler.y);
    const Vec3 euler = actor_world_euler(owner);
    put_float(body, 0x72C, euler.x);
    put_float(body, 0x730, euler.y);
    put_float(body, 0x734, euler.z);
    tick_runtime_player_status_timers(self, clock);
    track_runtime_player_room(context.room);
    if (entity_detail::g_nav_grids[0] != nullptr)
    {
        const Vec3 position = actor_world_position(actor);
        float radius;
        if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
        {
            radius = 0.0f;
        }
        else
        {
            radius = owner.radius_xz * get(body, body_field::radius_scale);
        }
        entity_detail::NavigationPathSearch* search;
        i32 preferred;
        if (context.mind.bytes == nullptr)
        {
            search = nullptr;
            preferred = -1;
        }
        else
        {
            search = context.path_search;
            preferred = context.floor_hint != nullptr ? i32(context.floor_hint[0x19]) : -1;
        }
        i32 floor = i32(read_u32(bytes, 0xCEC));
        const u32 changed = entity_detail::update_navigation_floor(position, floor, context.navigation, search,
            preferred, read_u16(bytes, 0xCDE) == 1 ? 1 : 0);
        put(bytes, 0xCEC, u32(floor));
        if (changed != 0)
        {
            send_word(self, 0x24);
        }
        if (radius == 0.0f)
        {
            if (context.navigation != nullptr)
            {
                entity_detail::release_navigation_cell_registration(context.navigation, read_u32(bytes, 0xCEC));
            }
        }
        else
        {
            entity_detail::register_navigation_cells(context.navigation, position, i32(read_u32(bytes, 0xCEC)),
                radius, i32(read_u32(bytes, 0xCF0)));
        }
    }
    RuntimePlayerBoundsCheck bounds{self, context.bounds, context.body_bits, context.ring, context.address_zero};
    check_runtime_player_bounds(bounds);
    if (runtime_decay_enabled(clock))
    {
        step = runtime_frame_step(clock);
        if (actor.animation_pause > 0.0f)
        {
            const float remaining = actor.animation_pause - step;
            actor.animation_pause = remaining;
            if (remaining <= 0.0f)
            {
                actor.animation_pause = 0.0f;
            }
        }
    }
    step = runtime_frame_step(clock);
    rate = runtime_actor_rate(actor);
    put_float(bytes, 0x924, rate * step + read_f32(bytes, 0x924));
    put(bytes, 0x928, read_u32(bytes, 0x928) + 1);
}
}
