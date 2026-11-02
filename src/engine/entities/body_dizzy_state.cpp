#include "body_dizzy_state.h"
#include "../frontend/dialogue_input.h"
#include "player_mind_control.h"
#include "body_orientation.h"
#include "actor_bind_tree.h"
#include "actor_live_attachments.h"
#include "actor_trail_requests.h"
#include "player_attachment_helpers.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "body_state_fields.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace gbtg
{
namespace field = dizzy_state_field;
namespace parameter = dizzy_state_parameter;
namespace star_field = dizzy_star_field;
namespace base = body_state_field;

namespace
{
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

void adjust_body_counters(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
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

const MinaPlayer* slot_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    if (controller == nullptr)
    {
        return nullptr;
    }
    return controller->slots.at(0).player.get();
}

// 0x00152BE0 into [0, 2pi): a remainder below zero (FCOMIP of 0 against it) gains 2pi.
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

}

// Xbox 0x0008C920
void dizzy_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008D230
void place_runtime_dizzy_star(std::span<const u8> state, std::span<const u8> body, const Actor& actor, i32 index,
    Vec3& position, Vec3& euler)
{
    const float reach = actor.radius_xz * get(body, body_field::radius_scale) + 2.f; // DAT_0032B110.
    const float height =
        actor.radius_y * get(body, body_field::height_scale) * std::bit_cast<float>(0x3F733333u); // DAT_0032B2B8.
    position = Vec3{0.f, height, 0.f};
    const auto star = record(state, field::stars, size_t(index));
    const float orbit = get(star, star_field::orbit);
    const float side = float(retail_sin(orbit));
    const float forward = float(retail_cos(orbit));
    position.x = side * reach + position.x;
    position.z = forward * reach + position.z;
    const float bob = get(star, star_field::bob);
    const long double lift = retail_sin(bob);
    (void)retail_cos(bob);
    position.y = float((lift + lift) + position.y);
    euler = Vec3{0.f, orbit + std::bit_cast<float>(0x3FC90FDBu), 0.f}; // DAT_0032B140.
}

// Xbox 0x0008CEF0
void spawn_runtime_dizzy_stars(std::span<u8> state, RuntimeDizzyStateContext& context)
{
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.empty_name);
    put_half(parameters, 2, 0x3F);
    copy_name(parameters, 0x28, record(state, field::star_names, 0).data());
    copy_name(parameters, 0xA8, record(state, field::star_names, 1).data());
    put_float(parameters, 0x20, get(state, field::star_scale) * context.actor.radius_xz * 0.2f); // DAT_0032B1AC.
    for (i32 index = 0; index < 3; ++index)
    {
        const auto slot = record(state, field::stars, size_t(index));
        set(slot, star_field::orbit, float(index) * std::bit_cast<float>(0x40060A92u)); // DAT_0032B558.
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
        set(slot, star_field::bob, unit * std::bit_cast<float>(0x40C90FDBu)); // DAT_0032B298.
        Vec3 position;
        Vec3 euler;
        place_runtime_dizzy_star(state, context.body, context.actor, index, position, euler);
        std::memcpy(parameters.data() + 8, &position, 12);
        std::memcpy(parameters.data() + 0x14, &euler, 12);
        const u32 star = context.stars.create(parameters);
        set(slot, star_field::actor, star);
        if (star == 0)
        {
            continue;
        }
        Actor& actor = context.stars.actor(star);
        AnimationController* controller = context.stars.animation(star).controller.get();
        if (controller != nullptr)
        {
            MinaPlayer* player = controller->slots.at(0).player.get();
            if (player != nullptr)
            {
                player->duration = 0.2f; // DAT_0032B1AC.
                if (0.f > player->speed)
                {
                    player->speed = -animation_absolute(5.f);
                }
                else
                {
                    player->speed = animation_absolute(5.f);
                }
            }
            controller->dirty = true;
        }
        if (ensure_runtime_actor_bind(actor) != nullptr)
        {
            actor.bind->mode_parent = 1;
            actor.bind->parent = &context.actor;
            actor.bind->flags = 0;
        }
        ActorBind* bind = actor.bind;
        if (bind != nullptr)
        {
            bind->mode_local = bind->mode_parent;
            if (bind->parent == nullptr)
            {
                throw Error("dizzy star bind 0x0003BB20 has no parent");
            }
            bind_actor_under_parent(actor, *bind->parent, context.default_position, context.default_euler);
            bind->parent = nullptr;
        }
    }
}

// Xbox 0x0008D0E0
void orbit_runtime_dizzy_stars(std::span<u8> state, float step, RuntimeDizzyStateContext& context)
{
    const float orbit_step = step * std::bit_cast<float>(0x40FB53D2u); // DAT_0032B48C.
    const float bob_step = step * std::bit_cast<float>(0x405F66F3u); // DAT_0032B488.
    for (i32 index = 0; index < 3; ++index)
    {
        const auto slot = record(state, field::stars, size_t(index));
        const u32 star = get(slot, star_field::actor);
        if (star == 0)
        {
            continue;
        }
        set(slot, star_field::orbit, wrap_angle(orbit_step + get(slot, star_field::orbit)));
        set(slot, star_field::bob, wrap_angle(bob_step + get(slot, star_field::bob)));
        Vec3 position;
        Vec3 euler;
        place_runtime_dizzy_star(state, context.body, context.actor, index, position, euler);
        Actor& actor = context.stars.actor(star);
        actor.position = position;
        actor.xform_valid = 0;
        wrap_runtime_euler(euler, actor.euler);
        actor.xform_valid = 0;
    }
}

// Xbox 0x0008C9A0
bool initialize_runtime_dizzy_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeDizzyStateContext& context)
{
    const auto body = context.body;
    set(state, field::star_scale, get(parameters, parameter::star_scale));
    set(state, field::time_left, read_f32(body, 0x23C));
    set(state, field::waits_for_animation, get(parameters, parameter::waits_for_animation));
    set(state, field::settles, get(parameters, parameter::settles));
    set(state, field::body_bit, get(parameters, parameter::body_bit));
    copy_name(record(state, field::star_names, 0), 0, record(parameters, parameter::star_names, 0).data());
    copy_name(record(state, field::star_names, 1), 0, record(parameters, parameter::star_names, 1).data());
    body[0x6B] = u8(body[0x6B] + 1);
    adjust_body_counters(body, 1);
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + 1));
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[3] |= 2;
    // The two input trackers 0x00109810 runs over: cleared, their state 1.
    std::ranges::fill(record(state, field::primary_tracking), u8(0));
    set(state, field::primary_tracking_state, 1);
    std::ranges::fill(record(state, field::secondary_tracking), u8(0));
    set(state, field::secondary_tracking_state, 1);
    const auto actor = context.actor_bytes;
    set(state, field::player_input, read_u16(actor, 0xCDC) == 2 ? 1 : (read_u32(actor, 0x93C) != 0 ? 1 : 0));
    spawn_runtime_dizzy_stars(state, context);
    orbit_runtime_dizzy_stars(state, 0.f, context);
    // Actor +0x614 is sound slot 4 (slots of 0xB8 from +0x334); 0x00302740 is the cue name.
    queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
        "XACT_SOUNDBANK_GBAT_DIZZY", context.sounds);
    const u32 bit = get(state, field::body_bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] |= u8(1u << (bit & 7));
    }
    return true;
}

// Xbox 0x0008CB30
void destroy_runtime_dizzy_state(std::span<u8> state, RuntimeDizzyStateContext& context)
{
    const auto body = context.body;
    body[0x6B] = u8(body[0x6B] - 1);
    adjust_body_counters(body, -1);
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
    context.bits.bytes[3] &= 0xFD;
    for (i32 index = 0; index < 3; ++index)
    {
        const auto slot = record(state, field::stars, size_t(index));
        const u32 star = get(slot, star_field::actor);
        if (star == 0)
        {
            continue;
        }
        Actor& actor = context.stars.actor(star);
        activate_actor_bind_children(actor.link == nullptr ? nullptr : actor.link->child, actor);
        ActorRenderRegistration& render = context.stars.render(star);
        if (render.child.prev == nullptr)
        {
            // The message is a stack slot holding 0x80; the receiver reads only its id.
            std::array<u32, 0x42> message{};
            message[0] = 0x80;
            context.stars.message(star, message);
            if (render.child.prev == nullptr)
            {
                register_actor_child_list(render);
            }
        }
        set(slot, star_field::actor, 0);
    }
    const u32 bit = get(state, field::body_bit);
    if (bit != 0)
    {
        context.bits.bytes[bit >> 3] &= u8(~(1u << (bit & 7)));
    }
    release_actor_sound(context.actor.sound_slots[4], context.sounds);
}

// Xbox 0x0008CEA0
void message_runtime_dizzy_state(std::span<u8> state, std::span<const u8> message)
{
    if (read_u32(message, 0) != 0x7C)
    {
        return;
    }
    const u32 actor = read_u32(message, 4);
    for (size_t index = 0; index < field::stars.count; ++index)
    {
        const auto slot = record(state, field::stars, index);
        if (actor == get(slot, star_field::actor))
        {
            set(slot, star_field::actor, 0);
        }
    }
}

// Xbox 0x0008CC40
void tick_runtime_dizzy_state(std::span<u8> state, RuntimeDizzyStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, base::elapsed,
        runtime_actor_rate(actor) * runtime_frame_step(context.clock) + get(state, base::elapsed));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if (get(state, field::settles) != 0)
    {
        settle_runtime_body_turn(body, actor);
    }
    float step = runtime_frame_step(context.clock) * runtime_actor_rate(actor);
    if (get(state, field::player_input) != 0)
    {
        // Both reads take the tick's input argument.
        if (context.input == nullptr || context.input_state == nullptr)
        {
            throw Error("dizzy state player-input branch without the tick's input device");
        }
        std::array<float, 2> axes{};
        runtime_input_axes(*context.input, axes, *context.input_state);
        update_runtime_player_input_tracking(record(state, field::primary_tracking), axes, context.input_preset, 1,
                                             context.clock);
        runtime_input_secondary_axes(*context.input, axes, *context.input_state);
        update_runtime_player_input_tracking(record(state, field::secondary_tracking), axes, context.input_preset, 1,
                                             context.clock);
        if (get(state, field::primary_tracking_state) == 2 || get(state, field::secondary_tracking_state) == 2)
        {
            step = context.input_scale * step;
        }
    }
    orbit_runtime_dizzy_stars(state, step, context);
    const float remaining = get(state, field::time_left);
    if (remaining > 0.f)
    {
        const float next = remaining - step;
        set(state, field::time_left, next);
        if (!(0.f < next))
        {
            set(state, field::time_left, 0.f);
        }
    }
    const bool expired = get(state, field::time_left) == 0.f;
    bool crossed = false;
    const MinaPlayer* player = slot_player(context.animation);
    if (player != nullptr && player->position != player->previous)
    {
        float lower = player->previous;
        float upper = player->position;
        if (0.f > player->speed)
        {
            lower = player->position;
            upper = player->previous;
        }
        if (upper > lower)
        {
            crossed = 1.f >= lower && upper > 1.f; // DAT_0032B0F8.
        }
        else
        {
            crossed = 1.f >= lower || upper > 1.f;
        }
    }
    if (!expired || (get(state, field::waits_for_animation) != 0 && !crossed))
    {
        return;
    }
    auto& recovery = context.recovery;
    const auto settings = recovery.settings;
    const u32 brake = settings.data() == nullptr ? 1 : (settings[0x1A4] == 0 ? 1 : 0);
    reset_runtime_body_turn(brake, body, recovery.first_timer);
    put_word(body, 0x174, read_u32(body, 0x170));
    const std::array<u32, 2> notice{0x75, context.actor_address};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(notice.data()));
    select_runtime_body_goal_action(state, recovery);
}
}
