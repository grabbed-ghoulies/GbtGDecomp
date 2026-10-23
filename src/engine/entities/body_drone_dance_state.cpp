#include "body_drone_dance_state.h"
#include "actor_transform.h"
#include "body_hit_state.h"
#include "body_look_for_weapon_state.h"
#include "body_shape_push.h"
#include "../camera/camera_body_slots.h"
#include "../navigation/spawn_candidates.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = drone_dance_state_field;
namespace parameter = drone_dance_state_parameter;

std::function<RuntimeDroneDanceStateContext&(u32)> drone_dance_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_body_vec3(std::span<u8> body, size_t offset, const Vec3& value)
{
    std::memcpy(body.data() + offset, &value, sizeof(value));
}

void copy_name(char* target, const char* source)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

float next_random_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u) - 1.f; // DAT_0032B0F8
}

// Counts a timer down by the actor's step, stopping at zero.
template <u32 Offset>
void count_down(std::span<u8> state, RetailField<float, Offset> timer, const RuntimeDroneDanceStateContext& context)
{
    const float step = runtime_actor_step(context.actor, context.clock);
    if (0.f < get(state, timer))
    {
        const float left = get(state, timer) - step;
        set(state, timer, left);
        if (left <= 0.f)
        {
            set(state, timer, 0.f);
        }
    }
}

// Body +0x318 takes the glide's velocity.
void copy_velocity(std::span<const u8> state, const RuntimeDroneDanceStateContext& context)
{
    put_body_vec3(context.body, 0x318, get(state, field::velocity));
}

bool construct_drone_dance_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_drone_dance_state(state, parameters, drone_dance_context_of(body_address_of(state)));
}

void destroy_drone_dance_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_drone_dance_state(state, drone_dance_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* drone_dance_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005057C0, drone_dance_state_object_sizes,
        construct_drone_dance_state_view, destroy_drone_dance_state_view};
    return &table;
}
}

// Xbox 0x0008DFF0
void drone_dance_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008E070
bool initialize_runtime_drone_dance_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDroneDanceStateContext& context)
{
    const auto body = context.body;
    put_half(body, 0x30, u16(read_u16(body, 0x30) + 1));
    body[0x38] = 0;
    body[0x39] = 0;
    copy_name(field_data(state, field::dance), context.string_at(get(parameters, parameter::dance)));
    copy_name(field_data(state, field::reaction), context.string_at(get(parameters, parameter::reaction)));
    set_runtime_drone_dance_phase(state, 1, context);
    // Slot 0 of the actor's animation starts at a random point.
    const float start = next_random_fraction(context.random);
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        MinaPlayer* player = controller->slots.empty() ? nullptr : controller->slots[0].player.get(); // +0x58.
        if (player != nullptr)
        {
            player->position = start; // +0x14.
            player->previous = start; // +0x10C.
        }
        controller->dirty = true; // +0x5C.
    }
    return true;
}

// Xbox 0x0008E120
void destroy_runtime_drone_dance_state(std::span<u8>, RuntimeDroneDanceStateContext& context)
{
    put_half(context.body, 0x30, u16(read_u16(context.body, 0x30) - 1));
}

// Xbox 0x0008E130
void tick_runtime_drone_dance_state(std::span<u8> state, RuntimeDroneDanceStateContext& context)
{
    Actor& actor = context.actor;
    set(state, field::clock, runtime_actor_step(actor, context.clock) + get(state, field::clock));
    const u32 phase = get(state, field::phase);
    if (phase == 1)
    {
        count_down(state, field::left, context);
        count_down(state, field::reaction_wait, context);
        if (get(state, field::left) == 0.f)
        {
            set_runtime_drone_dance_phase(state, 2, context);
        }
        copy_velocity(state, context);
        return;
    }
    if (phase == 2)
    {
        count_down(state, field::left, context);
        count_down(state, field::reaction_wait, context);
        const float fraction = 1.f - get(state, field::left) / get(state, field::glide_time); // DAT_0032B0F8
        const Vec3 from = get(state, field::start_euler);
        const Vec3 to = get(state, field::target_euler);
        const Vec3 euler{step_runtime_angle_toward(to.x, from.x, fraction),
            step_runtime_angle_toward(to.y, from.y, fraction),
            step_runtime_angle_toward(to.z, from.z, fraction)}; // 0x001013A0
        wrap_runtime_euler(euler, actor.euler); // 0x00105AA0
        actor.xform_valid = 0; // Actor +0x2A8.
        put_body_vec3(context.body, 0x72C, actor_world_euler(actor)); // 0x00041820 into body +0x72C
        copy_velocity(state, context);
        if (get(state, field::left) == 0.f) // DAT_0032B100
        {
            set_runtime_drone_dance_phase(state, 1, context);
        }
        return;
    }
    if (phase == 3)
    {
        const u32 facing = get(state, field::facing);
        if (facing != 0)
        {
            const Actor* target = context.actor_at(facing);
            if (target == nullptr)
            {
                throw Error("drone dance state faces an unregistered actor");
            }
            face_runtime_body_actor(context.body, actor, *target); // 0x001345B0, ESI state +0x4C
        }
        copy_velocity(state, context);
        if (actor_animation_completed(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            set_runtime_drone_dance_phase(state, 2, context);
        }
    }
}

// Xbox 0x0008E480
void message_runtime_drone_dance_state(std::span<u8> state, std::span<const u32> message,
    RuntimeDroneDanceStateContext& context)
{
    if (message[0] == 0x2D)
    {
        if (get(state, field::phase) != 3 && get(state, field::reaction_wait) == 0.f)
        {
            set(state, field::facing, message[2]);
            set(state, field::phase, 3u);
            set(state, field::velocity, Vec3{0.f, 0.f, 0.f});
            select_runtime_body_animation(field_data(state, field::reaction), context.actions); // 0x0001AF10
            set(state, field::reaction_wait, 5.f); // DAT_0032B184
        }
        return;
    }
    if (message[0] == 0x7C && message[1] == get(state, field::facing))
    {
        set(state, field::facing, 0u);
    }
}

// Xbox 0x0008E500, ECX state, EAX phase
void set_runtime_drone_dance_phase(std::span<u8> state, u32 phase, RuntimeDroneDanceStateContext& context)
{
    Actor& actor = context.actor;
    while (true)
    {
        set(state, field::phase, phase);
        if (phase == 1)
        {
            set(state, field::left, next_random_fraction(context.random) + 1.f); // DAT_0032B0F8
            set(state, field::velocity, Vec3{0.f, 0.f, 0.f});
            select_runtime_body_animation(field_data(state, field::dance), context.actions); // 0x0001AF10
            return;
        }
        if (phase != 2)
        {
            break;
        }
        Vec3 point;
        if (context.room != nullptr && entity_detail::select_occupancy_record_point(*context.room, point,
                context.random)) // 0x00110F50 with actor +0xCA0
        {
            const Vec3 position = actor_world_position(actor); // 0x000412A0
            set(state, field::start_euler, actor_world_euler(actor)); // 0x00041820 into state +0x34
            const float yaw = next_random_fraction(context.random) * 6.2831855f; // DAT_0032B298
            set(state, field::target_euler, Vec3{0.f, yaw, 0.f});
            const float dx = position.x - point.x;
            const float dz = position.z - point.z;
            const float distance = float(retail_square_root(dz * dz + dx * dx)); // 0x00152B30
            const float speed = next_random_fraction(context.random) * 10.f + 40.f; // DAT_0032B10C, DAT_0032B1B8
            Vec3 velocity{point.x - position.x, 0.f, point.z - position.z};
            scale_runtime_collision_vector(velocity, speed); // 0x00120250, ESI state +0x28
            set(state, field::velocity, velocity);
            const float time = distance / speed;
            set(state, field::left, time);
            set(state, field::glide_time, time);
            select_runtime_body_animation(field_data(state, field::dance), context.actions); // 0x0001AF10
            return;
        }
        phase = 1;
    }
    if (phase != 3)
    {
        return;
    }
    set(state, field::velocity, Vec3{0.f, 0.f, 0.f});
    select_runtime_body_animation(field_data(state, field::reaction), context.actions); // 0x0001AF10
    set(state, field::reaction_wait, 5.f); // DAT_0032B184
}

void install_runtime_drone_dance_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeDroneDanceStateContext&(u32 body_address)> context_of)
{
    drone_dance_context_of = std::move(context_of);
    types[0x67] = ObjectFactoryType{9, drone_dance_state_lifecycle}; // Getter 0x0008E010.
}
}
