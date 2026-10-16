#include "body_dizzy_state.h"
#include "actor_bind_tree.h"
#include "actor_live_attachments.h"
#include "actor_trail_requests.h"
#include "player_attachment_helpers.h"
#include "../camera/camera_body_slots.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstring>

namespace gbtg
{
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

size_t slot_offset(i32 index)
{
    return 0x15C + size_t(index) * 0xC;
}
}

// Xbox 0x0008C920
void dizzy_state_object_sizes(ObjectSizes& output)
{
    output.instance = 0x184;
    output.parameters = 0x11C;
}

// Xbox 0x0008D230
void place_runtime_dizzy_star(std::span<const u8> state, std::span<const u8> body, const Actor& actor, i32 index,
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

// Xbox 0x0008CEF0
void spawn_runtime_dizzy_stars(std::span<u8> state, RuntimeDizzyStateContext& context)
{
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.empty_name);
    put_half(parameters, 2, 0x3F);
    copy_name(parameters, 0x28, state.data() + 0x5C);
    copy_name(parameters, 0xA8, state.data() + 0xDC);
    put_float(parameters, 0x20, read_f32(state, 0x54) * context.actor.radius_xz * 0.2f); // DAT_0032B1AC.
    for (i32 index = 0; index < 3; ++index)
    {
        const size_t slot = slot_offset(index);
        put_float(state, slot, float(index) * std::bit_cast<float>(0x40060A92u)); // DAT_0032B558.
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000u) - 1.f;
        put_float(state, slot + 4, unit * std::bit_cast<float>(0x40C90FDBu)); // DAT_0032B298.
        Vec3 position;
        Vec3 euler;
        place_runtime_dizzy_star(state, context.body, context.actor, index, position, euler);
        std::memcpy(parameters.data() + 8, &position, 12);
        std::memcpy(parameters.data() + 0x14, &euler, 12);
        const u32 star = context.stars.create(parameters);
        put_word(state, slot + 8, star);
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
        const size_t slot = slot_offset(index);
        const u32 star = read_u32(state, slot + 8);
        if (star == 0)
        {
            continue;
        }
        put_float(state, slot, wrap_angle(orbit_step + read_f32(state, slot)));
        put_float(state, slot + 4, wrap_angle(bob_step + read_f32(state, slot + 4)));
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
    put_word(state, 0x54, read_u32(parameters, 0x110));
    put_word(state, 0x58, read_u32(body, 0x23C));
    put_word(state, 0x4C, read_u32(parameters, 0x114));
    put_word(state, 0x50, read_u32(parameters, 0xC));
    put_word(state, 0x180, read_u32(parameters, 0x118));
    copy_name(state, 0x5C, parameters.data() + 0x10);
    copy_name(state, 0xDC, parameters.data() + 0x90);
    body[0x6B] = u8(body[0x6B] + 1);
    adjust_body_counters(body, 1);
    put_half(body, 0x30, u16(read_u16(body, 0x30) + 1));
    body[0x38] = 1;
    body[0x39] = 1;
    context.bits.bytes[3] |= 2;
    // The two input filters 0x00109810 runs over (+0x18 and +0x30).
    put_float(state, 0x1C, 0.f);
    put_float(state, 0x18, 0.f);
    put_float(state, 0x24, 0.f);
    put_float(state, 0x20, 0.f);
    put_word(state, 0x2C, 1);
    put_float(state, 0x28, 0.f);
    put_float(state, 0x34, 0.f);
    put_float(state, 0x30, 0.f);
    put_float(state, 0x3C, 0.f);
    put_float(state, 0x38, 0.f);
    put_word(state, 0x44, 1);
    put_float(state, 0x40, 0.f);
    const auto actor = context.actor_bytes;
    put_word(state, 0x48, read_u16(actor, 0xCDC) == 2 ? 1 : (read_u32(actor, 0x93C) != 0 ? 1 : 0));
    spawn_runtime_dizzy_stars(state, context);
    orbit_runtime_dizzy_stars(state, 0.f, context);
    // Actor +0x614 is sound slot 4 (slots of 0xB8 from +0x334); 0x00302740 is the cue name.
    queue_actor_sound(context.actor.sound_slots[4], 0, &context.actor, 4, nullptr, nullptr, 1.f,
        "XACT_SOUNDBANK_GBAT_DIZZY", context.sounds);
    const u32 bit = read_u32(state, 0x180);
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
    put_half(body, 0x30, u16(read_u16(body, 0x30) - 1));
    context.bits.bytes[3] &= 0xFD;
    for (i32 index = 0; index < 3; ++index)
    {
        const size_t slot = slot_offset(index) + 8;
        const u32 star = read_u32(state, slot);
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
        put_word(state, slot, 0);
    }
    const u32 bit = read_u32(state, 0x180);
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
    for (const size_t slot : {size_t(0x164), size_t(0x170), size_t(0x17C)})
    {
        if (actor == read_u32(state, slot))
        {
            put_word(state, slot, 0);
        }
    }
}

// Xbox 0x0008CC40
void tick_runtime_dizzy_state(std::span<u8> state, RuntimeDizzyStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    put_float(state, 0x10, runtime_actor_rate(actor) * runtime_frame_step(context.clock) + read_f32(state, 0x10));
    put_float(body, 0x734, 0.f);
    put_float(body, 0x72C, 0.f);
    if (read_u32(state, 0x50) != 0)
    {
        throw Error("untranslated Xbox 0x00134720 reached from the dizzy state tick 0x0008CC40");
    }
    const float step = runtime_frame_step(context.clock) * runtime_actor_rate(actor);
    if (read_u32(state, 0x48) != 0)
    {
        // 0x001085F0 / 0x001088D0 into the +0x18 / +0x30 filters (0x00109810), which can speed the stars.
        throw Error("the dizzy state's player input branch (0x00109810 over state +0x18 / +0x30) is not bound");
    }
    orbit_runtime_dizzy_stars(state, step, context);
    const float remaining = read_f32(state, 0x58);
    if (remaining > 0.f)
    {
        const float next = remaining - step;
        put_float(state, 0x58, next);
        if (!(0.f < next))
        {
            put_float(state, 0x58, 0.f);
        }
    }
    const bool expired = read_f32(state, 0x58) == 0.f;
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
    if (!expired || (read_u32(state, 0x4C) != 0 && !crossed))
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
