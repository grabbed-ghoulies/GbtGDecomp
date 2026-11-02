#include "body_hit_stream_state.h"
#include "actor_live_attachments.h"
#include "body_animation_commands.h"
#include "body_damage_motion.h"
#include "body_orientation.h"
#include "player_mind_events.h"
#include "../graphics/actor_effect_nodes.h"
#include "body_fields.h"
#include "player_fields.h"

#include <array>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = hit_stream_state_field;
namespace parameter = hit_stream_state_parameter;

std::function<RuntimeHitStreamStateContext&(u32)> hit_stream_context_of;

u32 body_address_of(std::span<const u8> state)
{
    return read_u32(state, 8);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
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

std::span<u8> effect_tokens(std::span<u8> state)
{
    return state.subspan(field::kEffects, 36);
}

// Body +0x30, +0x6B, +0x309..+0x30C and +0x338 step together.
void step_counters(std::span<u8> body, i32 step)
{
    set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + step));
    body[0x6B] = u8(body[0x6B] + step);
    for (size_t index = 0; index < 4; ++index)
    {
        body[0x309 + index] = u8(body[0x309 + index] + step);
    }
    set(body, body_field::ease_rate_swaps, get(body, body_field::ease_rate_swaps) + u32(step));
}

// {0x6E, actor, attacker, taken, `knocked`, 0, 1, 0} to every actor (0x00122C70).
void announce_emptied(RuntimeHitStreamStateContext& context, u32 attacker, u32 taken, u32 knocked)
{
    std::array<i32, 0x42> message{};
    message[0] = 0x6E;
    message[1] = i32(context.actor_address);
    message[2] = i32(attacker);
    message[3] = i32(taken);
    message[4] = i32(knocked);
    message[6] = 1;
    broadcast_actor_pulse_message(message.data()); // 0x00122C70, EDI the message
}

bool construct_hit_stream_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_hit_stream_state(state, parameters, hit_stream_context_of(body_address_of(state)));
}

void destroy_hit_stream_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_hit_stream_state(state, hit_stream_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* hit_stream_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005059A0, hit_stream_state_object_sizes, construct_hit_stream_state_view,
        destroy_hit_stream_state_view};
    return &table;
}
}

// Xbox 0x000971E0
void hit_stream_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x00097260
bool initialize_runtime_hit_stream_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeHitStreamStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    set(state, field::unk_18, get(parameters, parameter::unk_10));
    set(state, field::stream_left, 0.5f); // DAT_0032B0F4.
    set(state, field::hit, 1u);
    set(state, field::push_speed, get(parameters, parameter::push_speed));
    set(state, field::unk_130, get(parameters, parameter::unk_194));
    copy_name(field_data(state, field::sound), field_data(parameters, parameter::sound));
    char* start = field_data(state, field::start_sound);
    copy_name(start, field_data(parameters, parameter::start_sound));
    if (start[0] != 0 && start[0] != 0)
    {
        queue_actor_sound(actor.sound_slots[4], 0, &actor, 4, nullptr, nullptr, 1.f, start,
            context.sounds); // 0x0012A260, EBX actor +0x614
    }
    if (context.actor_address != 0xFFFFFF74u) // Actor +0x8C is not address zero.
    {
        // 0x0003BE20 over callout 0x36 with 0x0013B8B0 recording the tokens at +0x13C.
        context.events.command_attachments = effect_tokens(state);
        dispatch_actor_attachment_records(context.callout_records, 0x36, record_runtime_body_attachment_event,
            nullptr, context.events, context.address_of(state.data() + field::kEffects));
    }
    if (read_u32(parameters, parameter::effect.offset) == 0x5F646961) // "aid_"
    {
        u32 effect = 0;
        auto clone = clone_executable_resource(context.resources, field_data(parameters, parameter::effect)); // 0x00107720
        if (clone.bytes != nullptr)
        {
            std::memcpy(clone.bytes.get() + 8, &context.actor_address, 4);
            effect = context.create_object({clone.bytes.get(), clone.size}); // 0x00107520
            clone.bytes.reset(); // 0x000663E0
        }
        set(state, field::effect, effect);
    }
    step_counters(body, 1);
    body[0x38] = 1;
    body[0x39] = 1;
    put_word(body, 0x330, read_u32(body, 0x334));
    set(body, body_field::ease_rate, 7.f); // DAT_0032B284.
    context.bits.bytes[2] |= 2;
    return true;
}

// Xbox 0x00097400
void destroy_runtime_hit_stream_state(std::span<u8> state, RuntimeHitStreamStateContext& context)
{
    const auto body = context.body;
    put_word(body, 0x334, read_u32(body, 0x330));
    step_counters(body, -1);
    context.bits.bytes[2] &= 0xFD;
    if (get(state, field::sounding) != 0)
    {
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
        set(state, field::sounding, 0u);
    }
    if (field_data(state, field::start_sound)[0] != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    // The inlined 0x0013B810: each recorded token's attachment goes (0x0003CFD0).
    clear_runtime_player_attachment_events(effect_tokens(state), context.lists);
    const u32 effect = get(state, field::effect);
    if (effect != 0)
    {
        context.release_frame_object(context.actor, effect); // 0x0003DE30, ECX actor, ESI effect
        set(state, field::effect, 0u);
    }
}

// Xbox 0x00097500
void tick_runtime_hit_stream_state(std::span<u8> state, RuntimeHitStreamStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const u32 attacker = get(body, body_field::attacker);
    set(state, body_state_field::elapsed, runtime_actor_step(actor, context.clock)
        + get(state, body_state_field::elapsed));
    if (body[0xA6] != 0)
    {
        put_float(body, 0xB4, 2.f); // DAT_0032B110.
    }
    if (actor.animation_pause > 0.f && get(state, field::hit) == 0)
    {
        if (get(state, field::sounding) != 0)
        {
            release_actor_sound(actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
            set(state, field::sounding, 0u);
        }
    }
    else if (get(state, field::sounding) == 0)
    {
        const char* sound = field_data(state, field::sound);
        if (sound[0] != 0)
        {
            queue_actor_sound(actor.sound_slots[1], 0, &actor, 1, nullptr, nullptr, 1.f, sound,
                context.sounds); // 0x0012A260, EBX actor +0x3EC
        }
        set(state, field::sounding, 1u);
    }
    bool facing = true;
    if (get(state, field::hit) != 0)
    {
        // The out word starts as the tick's own argument; 0x000979F0 writes it only when it drains.
        u32 taken = context.address_of(state.data());
        if (drain_runtime_hit_stream(state, taken, context) == 0) // 0x000979F0
        {
            if (get(state, field::unk_130) == 0)
            {
                set(state, body_state_field::next_key, u16(0x82));
                announce_emptied(context, attacker, taken, 0);
                return;
            }
            if (context.first_timer != nullptr)
            {
                const std::array<u32, 2> expire{5, 0};
                context.first_timer->message(expire); // Body +0x760 vtable +0xC.
            }
            set(state, body_state_field::next_key, context.body_slot_key(0x4C)); // Body vtable +0x4C.
            announce_emptied(context, attacker, taken, 1);
            return;
        }
        if (attacker == 0)
        {
            facing = false;
        }
        else
        {
            const Actor* source = context.actor_at(attacker);
            if (source == nullptr)
            {
                throw Error("0x00097500 pushes the body away from an attacker (body +0x270) that is not an actor");
            }
            Vec3 push;
            runtime_horizontal_direction(push, runtime_actor_angle_toward(actor, *source), // 0x00041900
                get(state, field::push_speed)); // 0x001060A0, ESI the push
            std::memcpy(body.data() + 0x318, &push, sizeof(push));
        }
    }
    if (facing && attacker != 0 && actor.animation_pause == 0.f)
    {
        if (get(body, body_field::move_holds) == 0 && get(body, body_field::move_hold_time) == 0.f)
        {
            const Actor* source = context.actor_at(attacker);
            if (source == nullptr)
            {
                throw Error("0x00097500 faces an attacker (body +0x270) that is not an actor");
            }
            face_runtime_body_point(body, actor, actor_world_position(*source)); // 0x000412A0, 0x00134510
        }
    }
    else
    {
        level_body_euler(body);
    }
    const float left = get(state, field::stream_left);
    if (0.f < left)
    {
        const float next = left - runtime_actor_step(actor, context.clock);
        set(state, field::stream_left, next);
        if (!(0.f < next))
        {
            set(state, field::stream_left, 0.f);
            set(state, body_state_field::next_key, context.body_slot_key(0x20)); // Body vtable +0x20.
        }
    }
    set(state, field::hit, 0u);
}

// Xbox 0x00097890
void message_runtime_hit_stream_state(std::span<u8> state, std::span<const u32> message,
    RuntimeHitStreamStateContext& context)
{
    if (message[0] != 0x18)
    {
        return;
    }
    set(state, field::stream_left, 0.5f); // DAT_0032B0F4.
    set(state, field::hit, 1u);
    const auto tokens = effect_tokens(state);
    const i32 count = std::bit_cast<i32>(read_u32(tokens, 0));
    for (i32 index = 0; index < count; ++index)
    {
        const u16 token = read_u16(tokens, size_t(index + 1) * 4);
        // The token's attachment on actor +0x1B0, and its DAT_00506930 effect.
        const ActorAttachmentNode* node = context.lists.second;
        while (node != nullptr && read_u16(node->bytes, 0x18) != token)
        {
            node = node->next;
        }
        if (node == nullptr)
        {
            continue;
        }
        const u32 slot = read_u32(node->bytes, 8);
        if (slot >= context.lists.timed.size())
        {
            throw Error("0x00097890 reads DAT_00506930 slot " + std::to_string(i32(slot)) + " of a callout effect");
        }
        ActorEffectNode* effect = context.lists.timed[slot];
        if (effect == nullptr)
        {
            continue;
        }
        const ActorAttachmentNode* again = context.lists.second;
        while (again != nullptr && read_u16(again->bytes, 0x18) != token)
        {
            again = again->next;
        }
        if (again != nullptr && read_u32(again->bytes, 0x10) != 0)
        {
            continue;
        }
        // At the stream's point 2.5 lower (DAT_0032B20C), turned half round from its velocity (DAT_0032B148).
        const Vec3 point{std::bit_cast<float>(message[0xA]), std::bit_cast<float>(message[0xB]) - 2.5f,
            std::bit_cast<float>(message[0xC])};
        const Vec3 velocity{std::bit_cast<float>(message[6]), std::bit_cast<float>(message[7]),
            std::bit_cast<float>(message[8])};
        float angle = 0.f;
        runtime_direction_angle(velocity, angle); // 0x00105F90, EAX message +0x18
        std::memcpy(effect->bytes.data() + 0x1C, &point, sizeof(point));
        const Vec3 euler{0.f, angle + 3.1415927410125732f, 0.f};
        std::memcpy(effect->bytes.data() + 0x50, &euler, sizeof(euler));
    }
}

// Xbox 0x000979F0, EAX state, EDI body
u16 drain_runtime_hit_stream(std::span<u8> state, u32& taken, RuntimeHitStreamStateContext& context)
{
    const auto body = context.body;
    const auto counts = [&context]()
    {
        // Not a player (+0xCDC 2, or +0x93C), or one the scene lets be hurt (0x00028D50).
        return (get(context.actor_bytes, player_field::mode) != 2 &&
                get(context.actor_bytes, player_field::case_link) == 0) ||
               context.damage_enabled();
    };
    if ((context.bits.bytes[0] & 0x80) == 0 && counts())
    {
        float step = 0.f;
        if (!(context.clock.freeze > 0.f))
        {
            step = context.clock.dt * context.clock.scale;
        }
        float drained = step * float(std::bit_cast<i32>(read_u32(body, 0x280))) + get(state, field::drained);
        set(state, field::drained, drained);
        u32 whole = 0;
        if (!(drained < 1.f)) // DAT_0032B0F8.
        {
            do
            {
                drained -= 1.f;
                ++whole;
            } while (!(drained < 1.f));
            set(state, field::drained, drained);
        }
        taken = whole;
        if (counts())
        {
            if ((context.bits.bytes[5] & 0x10) != 0)
            {
                whole *= 2;
            }
            if (whole != 0)
            {
                const i32 left = i32(get(body, body_field::energy)) - i32(whole);
                set(body, body_field::energy, left < 1 ? 0 : u16(left));
            }
        }
    }
    return get(body, body_field::energy);
}

void install_runtime_hit_stream_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeHitStreamStateContext&(u32 body_address)> context_of)
{
    hit_stream_context_of = std::move(context_of);
    types[0x76] = ObjectFactoryType{9, hit_stream_state_lifecycle}; // Getter 0x00097200.
}
}
