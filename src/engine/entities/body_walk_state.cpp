#include "body_walk_state.h"
#include "actor_bind_tree.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "player_attachment_helpers.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/anim.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = walk_state_field;
namespace parameter = walk_state_parameter;

std::function<RuntimeWalkStateContext&(u32)> walk_context_of;

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void copy_name(std::span<u8> destination, size_t offset, const u8* source)
{
    char* output = reinterpret_cast<char*>(destination.data() + offset);
    const char* input = reinterpret_cast<const char*>(source);
    char character;
    do
    {
        character = *input++;
        *output++ = character;
    } while (character != '\0');
}

size_t slot_offset(i32 index)
{
    return field::kSlots + size_t(index) * 0xC;
}

float roll(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C.
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// x87: (angle + step) through 0x00152BE0 into [0, 2pi); a remainder below zero gains 2pi.
float wrap_angle(float value)
{
    const float tau = std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
    const long double remainder = animation_remainder(value, tau);
    float result = float(remainder);
    if (0.0L > remainder) // DAT_0032B100.
    {
        result += tau;
    }
    return result;
}

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

bool construct_walk_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_walk_state(state, parameters, walk_context_of(body_address_of(state)));
}

void destroy_walk_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_walk_state(state, walk_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* walk_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C80, walk_state_object_sizes, construct_walk_state_view,
        destroy_walk_state_view};
    return &table;
}

void set_bit(std::span<const u8> state, RuntimeWalkStateContext& context)
{
    const u32 bit = get(state, field::bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
}
}

// Xbox 0x000A4CD0
void walk_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A54D0
void place_runtime_walk_object(std::span<const u8> state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler)
{
    const float reach = actor.radius_xz * read_f32(body, 0x888) + 2.f; // DAT_0032B110.
    const float height = actor.radius_y * read_f32(body, 0x884) * std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
    position = Vec3{0.f, height, 0.f};
    const float orbit = read_f32(state, slot_offset(index));
    const float side = float(retail_sin(orbit));
    const float forward = float(retail_cos(orbit));
    position.x = side * reach + position.x;
    position.z = forward * reach + position.z;
    const float bob = read_f32(state, slot_offset(index) + 4);
    const long double lift = retail_sin(bob);
    (void)retail_cos(bob);
    position.y = float((lift + lift) + position.y);
    euler = Vec3{0.f, orbit + std::bit_cast<float>(0x3FC90FDBu), 0.f}; // DAT_0032B140.
}

// Xbox 0x000A4D50
bool initialize_runtime_walk_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeWalkStateContext& context)
{
    Actor& actor = context.actor;
    set(state, field::bit, get(parameters, parameter::bit));
    set(state, field::next, get(parameters, parameter::next));
    set(state, field::start_yaw, actor.euler.y); // Actor +0x18.
    const float least = get(parameters, parameter::least_turn);
    const float unit = roll(context.random);
    set(state, field::turn, ((get(parameters, parameter::most_turn) - least) * unit + least)
        * 0.01745329238474369f); // DAT_0032B0FC.
    set(state, field::time, 0.f);
    set(state, field::length, get(parameters, parameter::length));
    set(state, field::orbit, get(parameters, parameter::orbit));
    set(state, field::size, get(parameters, parameter::size));
    copy_name(state, field::first_name.offset, parameters.data() + parameter::first_name.offset);
    copy_name(state, field::second_name.offset, parameters.data() + parameter::second_name.offset);
    set(state, field::clockwise, 0.5f < roll(context.random) ? 0u : 1u); // DAT_0032B0F4.
    if (get(state, field::orbit) == 0)
    {
        return true;
    }
    std::array<u8, 0x334> object{};
    initialize_actor_effect_parameters(object.data(), context.empty_name); // 0x000411A0
    put_half(object, 2, 0x3F);
    copy_name(object, 0x28, state.data() + field::first_name.offset);
    copy_name(object, 0xA8, state.data() + field::second_name.offset);
    put_float(object, 0x20, actor.radius_xz * get(state, field::size) * 0.2f); // DAT_0032B1AC.
    for (i32 index = 0; index < 3; ++index)
    {
        const size_t slot = slot_offset(index);
        put_float(state, slot, float(index) * std::bit_cast<float>(0x40060A92u)); // DAT_0032B558.
        put_float(state, slot + 4, roll(context.random) * std::bit_cast<float>(0x40C90FDBu)); // DAT_0032B298.
        Vec3 position;
        Vec3 euler;
        place_runtime_walk_object(state, context.body, actor, index, position, euler); // 0x000A54D0
        std::memcpy(object.data() + 8, &position, 12);
        std::memcpy(object.data() + 0x14, &euler, 12);
        const u32 created = context.objects.create(object); // 0x00107520
        put_word(state, slot + 8, created);
        if (created == 0)
        {
            continue;
        }
        AnimationController* controller = context.objects.animation(created).controller.get(); // +0x50.
        if (controller != nullptr)
        {
            retime_mina_playback(controller->slots.at(0).player.get(), 0.2f, controller->dirty); // 0x00030DB0
        }
        Actor& placed = context.objects.actor(created);
        if (ensure_runtime_actor_bind(placed) != nullptr) // 0x0003B9D0
        {
            placed.bind->mode_parent = 1;
            placed.bind->parent = &actor;
            placed.bind->flags = 0;
        }
        if (ActorBind* bind = placed.bind; bind != nullptr)
        {
            bind->mode_local = bind->mode_parent;
            if (bind->parent == nullptr)
            {
                throw Error("walk state object bind 0x0003BB20 has no parent");
            }
            bind_actor_under_parent(placed, *bind->parent, context.default_position, context.default_euler);
            bind->parent = nullptr;
        }
    }
    orbit_runtime_walk_objects(state, 0.f, context); // 0x000A5390, XMM0 0
    return true;
}

// Xbox 0x000A5060
void destroy_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context)
{
    const u32 bit = get(state, field::bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    for (i32 index = 0; index < 3; ++index)
    {
        const size_t slot = slot_offset(index) + 8;
        const u32 placed = read_u32(state, slot);
        if (placed == 0)
        {
            continue;
        }
        Actor& actor = context.objects.actor(placed);
        activate_actor_bind_children(actor.link == nullptr ? nullptr : actor.link->child, actor); // 0x00112D10
        ActorRenderRegistration& render = context.objects.render(placed);
        if (render.child.prev == nullptr)
        {
            // The message is a stack slot holding 0x80; the receiver reads only its id.
            std::array<u32, 0x42> message{};
            message[0] = 0x80;
            context.objects.message(placed, message);
            if (render.child.prev == nullptr)
            {
                register_actor_child_list(render); // 0x0003ED70
            }
        }
        put_word(state, slot, 0);
    }
}

// Xbox 0x000A5100
void tick_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context)
{
    Actor& actor = context.actor;
    if (context.strategy.empty() || read_u16(context.strategy, 0xE) == 1000)
    {
        end_runtime_walk_state(state, context); // 0x000A5340
        return;
    }
    const float time = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + get(state, field::time);
    set(state, field::time, time);
    const float fraction = time / get(state, field::length);
    if (fraction > 1.f) // DAT_0032B0F8.
    {
        end_runtime_walk_state(state, context);
        return;
    }
    if (get(state, field::orbit) != 0)
    {
        orbit_runtime_walk_objects(state, runtime_actor_rate(actor) * runtime_frame_step(context.clock),
            context); // 0x000A5390
    }
    const float turn = get(state, field::turn) * fraction;
    const float yaw = get(state, field::clockwise) != 0 ? get(state, field::start_yaw) - turn
                                                        : turn + get(state, field::start_yaw);
    const Vec3 world = actor_world_euler(actor); // 0x00041820, EDI body +0x72C
    std::memcpy(context.body.data() + 0x72C, &world, sizeof(world));
    Vec3 euler = actor_world_euler(actor); // 0x00041820
    euler.y = yaw;
    wrap_runtime_euler(euler, actor.euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // Actor +0x2A8.
}

// Xbox 0x000A5270
void message_runtime_walk_state(std::span<u8> state, std::span<const u32> message, RuntimeWalkStateContext& context)
{
    switch (message[0])
    {
    case 0x27:
        set_bit(state, context);
        set(state, body_state_field::next_key, context.primary_key()); // 0x00026950
        return;
    case 0x7C:
        for (i32 index = 0; index < 3; ++index)
        {
            if (message[1] == read_u32(state, slot_offset(index) + 8))
            {
                put_word(state, slot_offset(index) + 8, 0);
            }
        }
        return;
    case 0x87:
        if (get(state, field::orbit) != 0 && message[2] == 2)
        {
            end_runtime_walk_state(state, context); // 0x000A5340
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000A5340, ESI state, EDI actor
void end_runtime_walk_state(std::span<u8> state, RuntimeWalkStateContext& context)
{
    set_bit(state, context);
    u16 next = get(state, field::next);
    if (next == 0)
    {
        next = context.primary_key(); // 0x00026950
    }
    set(state, body_state_field::next_key, next);
}

// Xbox 0x000A5390
void orbit_runtime_walk_objects(std::span<u8> state, float step, RuntimeWalkStateContext& context)
{
    const float angle_step = step * 0.0872664675116539f; // DAT_0032B474.
    for (i32 index = 0; index < 3; ++index)
    {
        const size_t slot = slot_offset(index);
        const u32 placed = read_u32(state, slot + 8);
        if (placed == 0)
        {
            continue;
        }
        put_float(state, slot, wrap_angle(angle_step + read_f32(state, slot)));
        put_float(state, slot + 4, wrap_angle(angle_step + read_f32(state, slot + 4)));
        Vec3 position;
        Vec3 euler;
        place_runtime_walk_object(state, context.body, context.actor, index, position, euler); // 0x000A54D0
        Actor& actor = context.objects.actor(placed);
        actor.position = position;
        actor.xform_valid = 0;
        wrap_runtime_euler(euler, actor.euler); // 0x00105AA0
        actor.xform_valid = 0;
    }
}

void install_runtime_walk_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeWalkStateContext&(u32 body_address)> context_of)
{
    walk_context_of = std::move(context_of);
    types[0x8D] = ObjectFactoryType{9, walk_state_lifecycle}; // Getter 0x000A4CF0.
}
}
