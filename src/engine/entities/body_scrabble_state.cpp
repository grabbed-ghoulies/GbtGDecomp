#include "body_scrabble_state.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_gravity_motion.h"
#include "player_mind_events.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
namespace field = scrabble_state_field;

std::function<RuntimeScrabbleStateContext&(u32)> scrabble_context_of;

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

// Xbox 0x0009F370, EAX actor: the body turns to face the mind's goal point (+0x70), or the actor's heading when the
// point gives no angle.
void face_goal_point(RuntimeScrabbleStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 goal = get(context.mind, mind_field::goal_point);
    const Vec3 direction{goal.x - position.x, goal.y - position.y,
        goal.z - position.z};
    float angle = 0.f;
    if (!runtime_direction_angle(direction, angle)) // 0x00105F90
    {
        angle = context.actor.euler.y; // Actor +0x18.
    }
    put_float(context.body, 0x734, 0.f);
    put_float(context.body, 0x72C, 0.f);
    put_float(context.body, 0x730, angle);
}

// Xbox 0x0009F2D0, EAX body: the body moves along the actor's heading at its motion speed (0x000259C0), scaled by its
// surface's DAT_00217EC0 row when the body's +0x878 bit for that surface is set.
void run_along_heading(RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    const u8 mode = read_u16(body, 0x880) == 0 ? body[0x874] : body[0x87E];
    float scale = 1.f; // DAT_0032B0F8.
    if ((read_u32(body, 0x878) & (1u << (mode & 0x1F))) != 0)
    {
        if (mode >= context.mode_scales.size())
        {
            throw Error("0x0009F2D0 reads a surface mode outside the DAT_00217EC0 view");
        }
        scale = context.mode_scales[mode];
    }
    const float speed = runtime_body_motion_speed(body, context.actor_bytes, context.damage_tables) * scale; // 0x000259C0
    const float heading = context.actor.euler.y; // Actor +0x18.
    const float across = float(retail_sin(heading) * speed); // 0x00153320
    put_float(body, 0x320, float(retail_cos(heading) * speed)); // 0x00153230
    put_float(body, 0x318, across);
    put_float(body, 0x31C, 0.f);
}

bool construct_scrabble_state_view(FactoryObject& object, std::span<const u8>)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_scrabble_state(state, scrabble_context_of(body_address_of(state)));
}

void destroy_scrabble_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_scrabble_state(scrabble_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* scrabble_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505AC0, scrabble_state_object_sizes, construct_scrabble_state_view,
        destroy_scrabble_state_view};
    return &table;
}
}

// Xbox 0x0008E740
void scrabble_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = 0xC;
}

// Xbox 0x0009F0C0
bool initialize_runtime_scrabble_state(std::span<u8> state, RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    put_word(body, 0x330, read_u32(body, 0x334));
    put_float(body, 0x334, std::bit_cast<float>(0x3E4CCCCDu)); // DAT_0032B1AC.
    put_word(body, 0x338, read_u32(body, 0x338) + 1);
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const float duration = roll * 0.f + 2.f; // DAT_0032B100, DAT_0032B110.
    set(state, field::duration, duration);
    set(state, field::time_left, duration);
    return true;
}

// Xbox 0x0009F150
void destroy_runtime_scrabble_state(RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    put_word(body, 0x334, read_u32(body, 0x330));
    put_word(body, 0x338, read_u32(body, 0x338) - 1);
}

// Xbox 0x0009F170
void tick_runtime_scrabble_state(std::span<u8> state, RuntimeScrabbleStateContext& context)
{
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed, runtime_actor_rate(actor) * runtime_frame_step(context.clock)
        + get(state, body_state_field::elapsed));
    face_goal_point(context); // 0x0009F370, EAX actor
    run_along_heading(context); // 0x0009F2D0, EAX body
    const float duration = get(state, field::duration);
    const float quarter = duration * 0.25f; // DAT_0032B104.
    float rate;
    if (duration == quarter) // UCOMISS; an ordered equality takes the 0.7.
    {
        rate = std::bit_cast<float>(0x3F333333u); // DAT_0032B264.
    }
    else
    {
        rate = (get(state, field::time_left) - quarter) / (duration - quarter) * std::bit_cast<float>(0x3EE66666u)
            + 0.25f; // DAT_0032B4F8.
        if (rate > std::bit_cast<float>(0x3F333333u))
        {
            rate = std::bit_cast<float>(0x3F333333u);
        }
        else if (0.25f > rate)
        {
            rate = 0.25f;
        }
    }
    AnimationController* controller = context.animation.controller.get(); // Actor +0x50.
    if (controller != nullptr)
    {
        retime_mina_playback(controller->slots.at(0).player.get(), rate, controller->dirty); // 0x00030DB0, EAX 0
    }
    const float left = get(state, field::time_left);
    if (0.f < left)
    {
        const float next = left - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
        set(state, field::time_left, next);
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            set(state, field::time_left, 0.f);
            set(state, body_state_field::next_key, context.body_slot_key(0x20)); // Body vtable +0x20.
        }
    }
}

void install_runtime_scrabble_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeScrabbleStateContext&(u32 body_address)> context_of)
{
    scrabble_context_of = std::move(context_of);
    types[0x7F] = ObjectFactoryType{9, scrabble_state_lifecycle}; // Getter 0x0009F060.
}
}
