#include "body_scrabble_state.h"
#include <cstddef>
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_gravity_motion.h"
#include "player_mind_events.h"
#include "body_fields.h"

#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{

std::function<RuntimeScrabbleStateContext&(u32)> scrabble_context_of;

// Xbox 0x0009F370, EAX actor: the body turns to face the mind's goal point (+0x70), or the actor's heading when the
// point gives no angle.
void face_goal_point(RuntimeScrabbleStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 goal = mind_record(context.mind).goal_point;
    const Vec3 direction{goal.x - position.x, goal.y - position.y,
        goal.z - position.z};
    float angle = 0.f;
    if (!runtime_direction_angle(direction, angle)) // 0x00105F90
    {
        angle = context.actor.euler.y; // Actor +0x18.
    }
    level_body_euler(context.body);
   body_record(context.body).euler.y = angle;
}

// Xbox 0x0009F2D0, EAX body: the body moves along the actor's heading at its motion speed (0x000259C0), scaled by its
// surface's DAT_00217EC0 row when the body's +0x878 bit for that surface is set.
void run_along_heading(RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    const u8 mode = body_record(body).surface_override_frames == 0 ? body_record(body).surface
                                                                        : body_record(body).surface_override;
    float scale = 1.f; // DAT_0032B0F8.
    if ((body_record(body).scaled_surfaces & (1u << (mode & 0x1F))) != 0)
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
   body_record(body).velocity.z = float(retail_cos(heading) * speed); // 0x00153230
   body_record(body).velocity.x = across;
   body_record(body).velocity.y = 0.f;
}

bool construct_scrabble_state_view(FactoryObject& object, std::span<const u8>)
{
    ScrabbleState& state = body_state_view<ScrabbleState>({object.bytes.get(), object.size});
    return initialize_runtime_scrabble_state(state, scrabble_context_of(state.base.body));
}

void destroy_scrabble_state_view(FactoryObject& object)
{
    ScrabbleState& state = body_state_view<ScrabbleState>({object.bytes.get(), object.size});
    destroy_runtime_scrabble_state(scrabble_context_of(state.base.body));
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
    output.instance = sizeof(ScrabbleState);
    output.parameters = 0xC;
}

// Xbox 0x0009F0C0
bool initialize_runtime_scrabble_state(ScrabbleState& state, RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    body_record(body).saved_ease_rate = body_record(body).ease_rate;
    body_record(body).ease_rate = std::bit_cast<float>(0x3E4CCCCDu); // DAT_0032B1AC.
    body_record(body).ease_rate_swaps = body_record(body).ease_rate_swaps + 1;
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    const float roll = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const float duration = roll * 0.f + 2.f; // DAT_0032B100, DAT_0032B110.
    state.duration = duration;
    state.time_left = duration;
    return true;
}

// Xbox 0x0009F150
void destroy_runtime_scrabble_state(RuntimeScrabbleStateContext& context)
{
    const auto body = context.body;
    body_record(body).ease_rate = body_record(body).saved_ease_rate;
    body_record(body).ease_rate_swaps = body_record(body).ease_rate_swaps - 1;
}

// Xbox 0x0009F170
void tick_runtime_scrabble_state(ScrabbleState& state, RuntimeScrabbleStateContext& context)
{
    Actor& actor = context.actor;
    state.base.elapsed = runtime_actor_rate(actor) * runtime_frame_step(context.clock) + state.base.elapsed;
    face_goal_point(context); // 0x0009F370, EAX actor
    run_along_heading(context); // 0x0009F2D0, EAX body
    const float duration = state.duration;
    const float quarter = duration * 0.25f; // DAT_0032B104.
    float rate;
    if (duration == quarter) // UCOMISS; an ordered equality takes the 0.7.
    {
        rate = std::bit_cast<float>(0x3F333333u); // DAT_0032B264.
    }
    else
    {
        rate = (state.time_left - quarter) / (duration - quarter) * std::bit_cast<float>(0x3EE66666u)
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
    const float left = state.time_left;
    if (0.f < left)
    {
        const float next = left - runtime_actor_rate(actor) * runtime_frame_step(context.clock);
        state.time_left = next;
        if (!(0.f < next)) // COMISS; JC keeps a positive time.
        {
            state.time_left = 0.f;
            state.base.next_key = context.body_slot_key(0x20); // Body vtable +0x20.
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
