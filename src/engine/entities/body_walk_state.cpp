#include "body_walk_state.h"
#include "strategy_fields.h"
#include "actor_bind_tree.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "player_attachment_helpers.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <array>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
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

void copy_name(char* output, const char* input)
{
    char character;
    do
    {
        character = *input++;
        *output++ = character;
    } while (character != '\0');
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

bool construct_walk_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    WalkState& state = body_state_view<WalkState>({object.bytes.get(), object.size});
    return initialize_runtime_walk_state(state, parameters, walk_context_of(state.base.body));
}

void destroy_walk_state_view(FactoryObject& object)
{
    WalkState& state = body_state_view<WalkState>({object.bytes.get(), object.size});
    destroy_runtime_walk_state(state, walk_context_of(state.base.body));
}

const ObjectFactoryVtable* walk_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C80, walk_state_object_sizes, construct_walk_state_view,
        destroy_walk_state_view};
    return &table;
}

void set_bit(const WalkState& state, RuntimeWalkStateContext& context)
{
    const u32 bit = state.bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
}
}

// Xbox 0x000A4CD0
void walk_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(WalkState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000A54D0
void place_runtime_walk_object(const WalkState& state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler)
{
    const float reach = actor.radius_xz * get(body, body_field::radius_scale) + 2.f; // DAT_0032B110.
    const float height =
        actor.radius_y * get(body, body_field::height_scale) * std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
    position = Vec3{0.f, height, 0.f};
    const float orbit = state.slots[size_t(index)].orbit_angle;
    const float side = float(retail_sin(orbit));
    const float forward = float(retail_cos(orbit));
    position.x = side * reach + position.x;
    position.z = forward * reach + position.z;
    const float bob = state.slots[size_t(index)].bob_angle;
    const long double lift = retail_sin(bob);
    (void)retail_cos(bob);
    position.y = float((lift + lift) + position.y);
    euler = Vec3{0.f, orbit + std::bit_cast<float>(0x3FC90FDBu), 0.f}; // DAT_0032B140.
}

// Xbox 0x000A4D50
bool initialize_runtime_walk_state(WalkState& state, std::span<const u8> parameters,
    RuntimeWalkStateContext& context)
{
    Actor& actor = context.actor;
    state.bit = get(parameters, parameter::bit);
    state.next = get(parameters, parameter::next);
    state.start_yaw = actor.euler.y; // Actor +0x18.
    const float least = get(parameters, parameter::least_turn);
    const float unit = roll(context.random);
    // DAT_0032B0FC.
    state.turn = ((get(parameters, parameter::most_turn) - least) * unit + least) * 0.01745329238474369f;
    state.time = 0.f;
    state.length = get(parameters, parameter::length);
    state.orbit = get(parameters, parameter::orbit);
    state.size = get(parameters, parameter::size);
    copy_name(state.first_name.data(), field_data(parameters, parameter::first_name));
    copy_name(state.second_name.data(), field_data(parameters, parameter::second_name));
    state.clockwise = 0.5f < roll(context.random) ? 0u : 1u; // DAT_0032B0F4.
    if (state.orbit == 0)
    {
        return true;
    }
    std::array<u8, 0x334> object{};
    initialize_actor_effect_parameters(object.data(), context.empty_name); // 0x000411A0
    put_half(object, 2, 0x3F);
    copy_name(reinterpret_cast<char*>(object.data() + 0x28), state.first_name.data());
    copy_name(reinterpret_cast<char*>(object.data() + 0xA8), state.second_name.data());
    put_float(object, 0x20, actor.radius_xz * state.size * 0.2f); // DAT_0032B1AC.
    for (i32 index = 0; index < 3; ++index)
    {
        WalkObjectSlot& slot = state.slots[size_t(index)];
        slot.orbit_angle = float(index) * std::bit_cast<float>(0x40060A92u); // DAT_0032B558.
        slot.bob_angle = roll(context.random) * std::bit_cast<float>(0x40C90FDBu); // DAT_0032B298.
        Vec3 position;
        Vec3 euler;
        place_runtime_walk_object(state, context.body, actor, index, position, euler); // 0x000A54D0
        std::memcpy(object.data() + 8, &position, 12);
        std::memcpy(object.data() + 0x14, &euler, 12);
        const u32 created = context.objects.create(object); // 0x00107520
        slot.object = created;
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
void destroy_runtime_walk_state(WalkState& state, RuntimeWalkStateContext& context)
{
    const u32 bit = state.bit;
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    for (i32 index = 0; index < 3; ++index)
    {
        WalkObjectSlot& slot = state.slots[size_t(index)];
        const u32 placed = slot.object;
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
        slot.object = 0;
    }
}

// Xbox 0x000A5100
void tick_runtime_walk_state(WalkState& state, RuntimeWalkStateContext& context)
{
    Actor& actor = context.actor;
    if (context.strategy.empty() || strategy_base(context.strategy).state == 1000)
    {
        end_runtime_walk_state(state, context); // 0x000A5340
        return;
    }
    const float time = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + state.time;
    state.time = time;
    const float fraction = time / state.length;
    if (fraction > 1.f) // DAT_0032B0F8.
    {
        end_runtime_walk_state(state, context);
        return;
    }
    if (state.orbit != 0)
    {
        orbit_runtime_walk_objects(state, runtime_actor_rate(actor) * runtime_frame_step(context.clock),
            context); // 0x000A5390
    }
    const float turn = state.turn * fraction;
    const float yaw = state.clockwise != 0 ? state.start_yaw - turn
                                                        : turn + state.start_yaw;
    const Vec3 world = actor_world_euler(actor); // 0x00041820, EDI body +0x72C
    std::memcpy(context.body.data() + 0x72C, &world, sizeof(world));
    Vec3 euler = actor_world_euler(actor); // 0x00041820
    euler.y = yaw;
    wrap_runtime_euler(euler, actor.euler); // 0x00105AA0, EDI actor +0x14
    actor.xform_valid = 0; // Actor +0x2A8.
}

// Xbox 0x000A5270
void message_runtime_walk_state(WalkState& state, std::span<const u32> message, RuntimeWalkStateContext& context)
{
    switch (message[0])
    {
    case 0x27:
        set_bit(state, context);
        state.base.next_key = context.primary_key(); // 0x00026950
        return;
    case 0x7C:
        for (i32 index = 0; index < 3; ++index)
        {
            if (message[1] == state.slots[size_t(index)].object)
            {
                state.slots[size_t(index)].object = 0;
            }
        }
        return;
    case 0x87:
        if (state.orbit != 0 && message[2] == 2)
        {
            end_runtime_walk_state(state, context); // 0x000A5340
        }
        return;
    default:
        return;
    }
}

// Xbox 0x000A5340, ESI state, EDI actor
void end_runtime_walk_state(WalkState& state, RuntimeWalkStateContext& context)
{
    set_bit(state, context);
    u16 next = state.next;
    if (next == 0)
    {
        next = context.primary_key(); // 0x00026950
    }
    state.base.next_key = next;
}

// Xbox 0x000A5390
void orbit_runtime_walk_objects(WalkState& state, float step, RuntimeWalkStateContext& context)
{
    const float angle_step = step * 0.0872664675116539f; // DAT_0032B474.
    for (i32 index = 0; index < 3; ++index)
    {
        WalkObjectSlot& slot = state.slots[size_t(index)];
        const u32 placed = slot.object;
        if (placed == 0)
        {
            continue;
        }
        slot.orbit_angle = wrap_angle(angle_step + slot.orbit_angle);
        slot.bob_angle = wrap_angle(angle_step + slot.bob_angle);
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
