#include "body_slide_state.h"
#include "body_gravity_motion.h"
#include "player_goal_cleanup.h"
#include "player_mind_requests.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = slide_state_field;
namespace parameter = slide_state_parameter;

std::function<RuntimeSlideStateContext&(u32)> slide_context_of;

// Host views of the two 0x0003AAA0 records the state's +0x1C / +0x20 words point at.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 2>> slide_records;

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

// The inlined phase change: a phase in 1..3 gives body +0x334 back from +0x330 and lowers +0x338, then the new phase
// keeps +0x334 at +0x330, sets it to `speed` and raises +0x338.
void enter_phase(std::span<u8> state, u32 phase, float speed, RuntimeSlideStateContext& context)
{
    const auto body = context.body;
    const i32 current = i32(get(state, field::phase));
    if (0 < current && current < 4)
    {
        put_word(body, 0x334, read_u32(body, 0x330));
        put_word(body, 0x338, read_u32(body, 0x338) - 1);
    }
    set(state, field::phase, phase);
    put_word(body, 0x330, read_u32(body, 0x334));
    put_float(body, 0x334, speed);
    put_word(body, 0x338, read_u32(body, 0x338) + 1);
}

// 0x0003A9E0, ECX the record at +0x1C (index 0) or +0x20 (index 1).
void start_animation(std::span<const u8> state, size_t index, RuntimeSlideStateContext& context)
{
    const auto found = slide_records.find(state.data());
    const AnimationRecord* record = found == slide_records.end() ? nullptr : found->second[index];
    if (record == nullptr)
    {
        throw Error("0x0003A9E0 reads the slide state's animation record, which 0x0003AAA0 did not find");
    }
    auto& actions = context.actions;
    start_actor_animation(actions.animation, *record, actions.registry, actions.error, actions.random); // 0x0003A9E0
}

// Xbox 0x000A10D0, EAX body: the actor's +0x3C velocity along its heading at twice the body's motion speed
// (0x000259C0), scaled by its surface's DAT_00217EC0 row when the body's +0x878 bit for that surface is set.
void push_along_heading(RuntimeSlideStateContext& context)
{
    const auto body = context.body;
    const u8 mode = read_u16(body, 0x880) == 0 ? body[0x874] : body[0x87E];
    float scale = 1.f; // DAT_0032B0F8.
    if ((read_u32(body, 0x878) & (1u << (mode & 0x1F))) != 0)
    {
        if (mode >= context.mode_scales.size())
        {
            throw Error("0x000A10D0 reads a surface mode outside the DAT_00217EC0 view");
        }
        scale = context.mode_scales[mode];
    }
    const float heading = context.actor.euler.y; // Actor +0x18.
    const float speed = runtime_body_motion_speed(body, context.actor_bytes, context.damage_tables) * scale
        * 2.f; // 0x000259C0, DAT_0032B110.
    const float across = float(retail_sin(heading) * speed); // 0x00153320
    context.actor.anchor = Vec3{across, 0.f, float(retail_cos(heading) * speed)}; // Actor +0x3C, 0x00153230.
}

// Xbox 0x000A1180, ESI state, EDI body: the slide's event bits start its callout and its contact shapes; when slot 0's
// animation is done (the inlined 0x0003B290) the body holds for 0.2 s.
void slide(std::span<u8> state, RuntimeSlideStateContext& context)
{
    const auto& bits = context.bits.bytes; // Body +0x15C -> +8.
    if ((bits[0] & 4) != 0)
    {
        set(state, field::sounding, 1);
        if (context.actor_address != 0xFFFFFF74u)
        {
            context.callouts(context.actor_address, 0x4D, nullptr); // 0x0003BE20, EAX actor +0x8C
        }
    }
    if ((bits[0] & 8) != 0)
    {
        set(state, field::shapes_active, 1);
        if (context.collision != nullptr)
        {
            activate_runtime_body_contact_shapes(*context.collision, context.actor, get(state, field::shape_tag), 7,
                1.f); // 0x0013F420, XMM4 DAT_0032B0F8
            put_word(context.body, 0x2CC, 0);
        }
    }
    if (!runtime_actor_animation_slot_complete(context.actions.animation, 0)) // The inlined 0x0003B290.
    {
        return;
    }
    enter_phase(state, 2, 0.f, context);
    set(state, field::hold_timer, std::bit_cast<float>(0x3E4CCCCDu)); // DAT_0032B1AC.
}

// Xbox 0x000A12B0, EDX state: the hold counts down, then the recovery starts with body +0x334 at 2.5.
void hold(std::span<u8> state, RuntimeSlideStateContext& context)
{
    const float left = get(state, field::hold_timer);
    if (!(0.f < left))
    {
        return;
    }
    const float next = left - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    set(state, field::hold_timer, next);
    if (0.f < next) // COMISS; JC keeps a positive time.
    {
        return;
    }
    set(state, field::hold_timer, 0.f);
    enter_phase(state, 3, 2.5f, context); // DAT_0032B20C.
    start_animation(state, 1, context); // 0x0003A9E0, ECX state +0x20
}

// Xbox 0x000A1370, EDI state, EBX body: the recovery's event bits release the slide sound and the contact shapes;
// when slot 0's animation is done the body hands over its next action.
void recover(std::span<u8> state, RuntimeSlideStateContext& context)
{
    const auto& bits = context.bits.bytes; // Body +0x15C -> +8.
    if ((bits[0] & 4) != 0 && get(state, field::sounding) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
        set(state, field::sounding, 0);
    }
    if ((bits[0] & 8) != 0)
    {
        set(state, field::shapes_active, 0);
        if (context.collision != nullptr)
        {
            deactivate_runtime_body_contact_shapes(*context.collision, get(state, field::shape_tag)); // 0x0013F4D0
        }
    }
    if (!runtime_actor_animation_slot_complete(context.actions.animation, 0)) // The inlined 0x0003B290.
    {
        return;
    }
    set(state, body_state_field::next_key, context.body_slot_key(0x20)); // Body vtable +0x20.
}

bool construct_slide_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_slide_state(state, parameters, slide_context_of(body_address_of(state)));
}

void destroy_slide_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_slide_state(state, slide_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* slide_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505B40, slide_state_object_sizes, construct_slide_state_view,
        destroy_slide_state_view};
    return &table;
}
}

// Xbox 0x000A0C10
void slide_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x000A0C90
bool initialize_runtime_slide_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeSlideStateContext& context)
{
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    auto& records = slide_records[state.data()];
    records[0] = find_runtime_body_animation(context.image_string(get(parameters, parameter::slide)),
        context.actions);
    set(state, field::slide_animation, records[0] != nullptr ? 1u : 0u);
    records[1] = find_runtime_body_animation(context.image_string(get(parameters, parameter::recover)),
        context.actions);
    set(state, field::recover_animation, records[1] != nullptr ? 1u : 0u);
    set(state, field::shape_tag, get(parameters, parameter::shape_tag));
    set(state, field::event, get(parameters, parameter::event));
    set(state, field::sounding, 0);
    push_along_heading(context); // 0x000A10D0, EAX body
    increment_runtime_body_latch_mask(context.body, 1, context.actor, context.latches); // The inlined 0x0001F3F0.
    enter_phase(state, 1, 0.f, context);
    start_animation(state, 0, context); // 0x0003A9E0, ECX state +0x1C
    return true;
}

// Xbox 0x000A0E80
void destroy_runtime_slide_state(std::span<u8> state, RuntimeSlideStateContext& context)
{
    const auto body = context.body;
    decrement_runtime_body_latch_mask(body, 1, context.actor, context.latches); // The inlined 0x0001F2F0.
    set(state, field::shapes_active, 0);
    if (context.collision != nullptr)
    {
        deactivate_runtime_body_contact_shapes(*context.collision, get(state, field::shape_tag)); // 0x0013F4D0
    }
    const i32 phase = i32(get(state, field::phase));
    if (0 < phase && phase < 4)
    {
        put_word(body, 0x334, read_u32(body, 0x330));
        put_word(body, 0x338, read_u32(body, 0x338) - 1);
    }
    set(state, field::phase, 0);
    if (get(state, field::sounding) != 0)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
        set(state, field::sounding, 0);
    }
    slide_records.erase(state.data());
}

// Xbox 0x000A0F90
void tick_runtime_slide_state(std::span<u8> state, RuntimeSlideStateContext& context)
{
    Actor& actor = context.actor;
    set(state, body_state_field::elapsed, runtime_actor_rate(actor) * runtime_frame_step(context.clock)
        + get(state, body_state_field::elapsed));
    put_float(context.body, 0x734, 0.f);
    put_float(context.body, 0x72C, 0.f);
    if (get(state, field::shapes_active) != 0)
    {
        float timer = get(state, field::trail_timer);
        request_actor_video_rate_trail(actor, std::bit_cast<float>(0x3E19999Au), timer,
            context.trails); // The inlined 0x0013B1B0: 0x0013B030 at half the video rate.
        set(state, field::trail_timer, timer);
    }
    switch (get(state, field::phase))
    {
    case 1:
        slide(state, context); // 0x000A1180
        break;
    case 2:
        hold(state, context); // 0x000A12B0
        break;
    case 3:
        recover(state, context); // 0x000A1370
        break;
    default:
        break;
    }
}

// Xbox 0x000A1070
void message_runtime_slide_state(std::span<const u8> state, std::span<const u8> message,
    RuntimeSlideStateContext& context)
{
    const u32 id = read_u32(message, 0);
    if (id == 0x41)
    {
        // 0x0013AD60, EAX message +0x1C, ECX the actor at message +0x14.
        g_actor_creation_options.flags = 0;
        g_actor_creation_options.position = Vec3{read_f32(message, 0x1C), read_f32(message, 0x20),
            read_f32(message, 0x24)};
        g_actor_creation_options.flags = 1;
        const u32 actor = read_u32(message, 0x14);
        if (actor != 0xFFFFFF74u)
        {
            context.callouts(actor, get(state, field::event), &g_actor_creation_options);
        }
    }
    else if (id == 0x70)
    {
        const u32 other = read_u32(message, 4);
        if (read_u32(message, 8) == context.actor_address && other != 0xFFFFFF74u)
        {
            context.callouts(other, get(state, field::event), nullptr); // 0x0003BE20, EAX other +0x8C
        }
    }
}

void install_runtime_slide_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeSlideStateContext&(u32 body_address)> context_of)
{
    slide_context_of = std::move(context_of);
    types[0x83] = ObjectFactoryType{9, slide_state_lifecycle}; // Getter 0x000A0C30.
}
}
