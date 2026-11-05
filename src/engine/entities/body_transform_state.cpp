#include "body_transform_state.h"
#include "actor_fields.h"
#include "actor_transform.h"
#include "gbtg/model_runtime.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
namespace parameter = transform_state_parameter;

std::function<RuntimeTransformStateContext&(u32)> transform_context_of;

// The host halves of +0x1C and +0xA4 .. +0xAC: the records 0x0003AAA0 found.
struct TransformRecords
{
    const AnimationRecord* body = nullptr;
    const AnimationRecord* morph = nullptr;
    const AnimationRecord* second = nullptr;
    const AnimationRecord* third = nullptr;
};
std::unordered_map<const u8*, TransformRecords> transform_records;

void copy_name(char* target, const char* source)
{
    size_t index = 0;
    do
    {
        target[index] = source[index];
    }
    while (source[index++] != 0);
}

float roll(RuntimeTransformStateContext& context)
{
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu; // DAT_0054DD7C
    return std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
}

// A countdown by the actor's step that stops at zero; true when it ran out this tick.
bool count_down(float& timer, RuntimeTransformStateContext& context)
{
    if (!(timer > 0.f))
    {
        return false;
    }
    timer = timer - runtime_actor_rate(context.actor) * runtime_frame_step(context.clock);
    if (0.f < timer) // COMISS; JB keeps a positive time.
    {
        return false;
    }
    timer = 0.f;
    return true;
}

const MinaPlayer* first_player(const ActorAnimationState& animation)
{
    const AnimationController* controller = animation.controller.get();
    return controller == nullptr ? nullptr : controller->slots.at(0).player.get();
}

Actor& actor_of(u32 address, RuntimeTransformStateContext& context)
{
    return context.actor_view(address).bat.actor;
}

// 0x000F96F0 with AL the value on the actor's +0x204 and +0x310 models.
void set_actor_node(u32 address, u8 node, u8 value, RuntimeTransformStateContext& context)
{
    NativeActorFactoryInstance& view = context.actor_view(address);
    if (view.bat.actor.model != nullptr && view.bat.actor.model->runtime != nullptr)
    {
        set_runtime_model_node_state(*view.bat.actor.model->runtime, node, value, context.node_states);
    }
    if (auto* secondary = view.startup.models.secondary.get())
    {
        set_runtime_model_node_state(*secondary, node, value, context.node_states);
    }
}

// The inlined 0x0003AAA0 on another actor, with no variant.
const AnimationRecord* find_actor_record(u32 address, const char* name, RuntimeTransformStateContext& context)
{
    if (address == 0)
    {
        throw Error("0x000A4400 looks up a morph animation with no morph actor");
    }
    return find_actor_animation(context.actor_view(address).animation.table, name, nullptr, context.random);
}

// 0x0003A9E0, EAX the actor, ECX the record.
void start_actor_record(u32 address, const AnimationRecord& record, RuntimeTransformStateContext& context)
{
    start_actor_animation(context.actor_view(address).animation, record, context.actions.registry,
        context.actions.error, context.random);
}

// Xbox 0x000A4B00, EAX the template's name, ECX the actor: made at the actor's pose with all 32 nodes hidden.
u32 create_morph(const char* name, RuntimeTransformStateContext& context)
{
    auto clone = clone_executable_resource(context.resources, name); // 0x00107720
    if (clone.bytes == nullptr)
    {
        throw Error("0x000A4B00 writes the pose into a morph template that was not found");
    }
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0, EDI clone +8
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820, EDI clone +0x14
    std::memcpy(clone.bytes.get() + 8, &position, 12);
    std::memcpy(clone.bytes.get() + 0x14, &euler, 12);
    const u32 morph = context.create_object({clone.bytes.get(), clone.size}); // 0x00107520
    clone.bytes.reset(); // 0x000F7100's heap, 0x0014D360
    if (morph == 0)
    {
        throw Error("0x000A4B00 hides the nodes of a morph actor that was not made");
    }
    for (u8 node = 0; node < 0x20; ++node)
    {
        set_actor_node(morph, node, 1, context); // 0x000F96F0, AL 1
    }
    return morph;
}

// Xbox 0x000A4BB0: the final actor from the template at +0xBC, at the body's pose and scale and with its marker key.
u32 create_result(TransformState& state, RuntimeTransformStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820
    const char* name = state.result.data();
    if (std::memcmp(name, "aid_", 4) != 0)
    {
        return 0;
    }
    auto clone = clone_executable_resource(context.resources, name); // 0x00107720
    if (clone.bytes == nullptr)
    {
        return 0;
    }
    const float scale = context.actor.radius_xz; // Actor +0x38.
    const i16 key = i16(actor_marker_id(context.actor_bytes));
    std::memcpy(clone.bytes.get() + 8, &position, 12);
    std::memcpy(clone.bytes.get() + 0x14, &euler, 12);
    std::memcpy(clone.bytes.get() + 0x20, &scale, 4);
    const u16 type = key != 0 ? context.spawn_type(key) : u16(0); // 0x0011F2E0, ESI the key
    std::memcpy(clone.bytes.get() + 0x2AA, &key, 2);
    std::memcpy(clone.bytes.get() + 0x2AC, &type, 2);
    const u32 created = context.create_object({clone.bytes.get(), clone.size}); // 0x00107520
    clone.bytes.reset(); // 0x000663E0
    return created;
}

// 0x0013B1F0 on an actor of the scene, by its address.
void burst_into_bits(u32 address, float lifetime, const char* name, RuntimeTransformStateContext& context)
{
    burst_runtime_actor_into_bits(actor_of(address, context), address, lifetime, name, context.death.default_name,
        [&context](std::span<const u8> parameters)
        {
            context.create_object(parameters); // 0x00107520
        }); // 0x0013B1F0
}

// The morph actor bursts and its bind tree goes (message 0x7C's release without the burst).
void release_morph(TransformState& state, bool burst, RuntimeTransformStateContext& context)
{
    const u32 morph = state.morph;
    if (morph == 0)
    {
        return;
    }
    if (burst)
    {
        burst_into_bits(morph, 5.f, state.bits_effect.data(), context); // 0x0013B1F0, ESI state +0x24
    }
    activate_actor_bind_tree(actor_of(morph, context)); // 0x00112CC0, ESI the morph
    state.morph = 0u;
}

bool construct_transform_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    TransformState& state = body_state_view<TransformState>({object.bytes.get(), object.size});
    return initialize_runtime_transform_state(state, parameters, transform_context_of(state.base.body));
}

void destroy_transform_state_view(FactoryObject& object)
{
    TransformState& state = body_state_view<TransformState>({object.bytes.get(), object.size});
    destroy_runtime_transform_state(state, transform_context_of(state.base.body));
}

const ObjectFactoryVtable* transform_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505C60, transform_state_object_sizes, construct_transform_state_view,
        destroy_transform_state_view};
    return &table;
}
}

// Xbox 0x000A4380
void transform_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(TransformState);
    output.parameters = parameter::kSize;
}

// Xbox 0x000A4400
bool initialize_runtime_transform_state(TransformState& state, std::span<const u8> parameters,
    RuntimeTransformStateContext& context)
{
    context.bits.bytes[6] |= 0x10;
    RuntimeBodyStateContext shape = context.state();
    set_runtime_body_shape_mode(4, shape); // 0x0001FEB0, BL 4, EDI body
    const u32 morph = create_morph(field_data(parameters, parameter::morph), context); // 0x000A4B00
    state.morph = morph;
    copy_name(state.bits_effect.data(), field_data(parameters, parameter::bits_effect));
    auto& records = transform_records[body_state_key(state)];
    records = {};
    if (get(parameters, parameter::body_animation) != 0)
    {
        // The inlined 0x0003AAA0 pair: the held actor's +0xDB0 variant first, then the plain name.
        records.body = find_runtime_body_animation(context.image_string(get(parameters, parameter::body_animation)),
            context.actions);
        state.body_animation = records.body != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::morph_animation) != 0)
    {
        records.morph = find_actor_record(morph, context.image_string(get(parameters, parameter::morph_animation)),
            context);
        state.morph_animation = records.morph != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::third_animation) != 0)
    {
        records.third = find_actor_record(morph, context.image_string(get(parameters, parameter::third_animation)),
            context);
        state.third_animation = records.third != nullptr ? 1u : 0u;
    }
    if (get(parameters, parameter::second_animation) != 0)
    {
        records.second = find_actor_record(morph, context.image_string(get(parameters, parameter::second_animation)),
            context);
        state.second_animation = records.second != nullptr ? 1u : 0u;
    }
    copy_name(state.first_sound.data(), field_data(parameters, parameter::first_sound));
    copy_name(state.second_sound.data(), field_data(parameters, parameter::second_sound));
    copy_name(state.third_sound.data(), field_data(parameters, parameter::third_sound));
    copy_name(state.result_sound.data(), field_data(parameters, parameter::result_sound));
    copy_name(state.hit_sound.data(), field_data(parameters, parameter::hit_sound));
    state.second_hold = roll(context) * 0.f + 1.25f; // DAT_0032B114.
    state.third_hold = roll(context) * 0.f + 1.25f;
    copy_name(state.result.data(), field_data(parameters, parameter::result));
    state.shown = 0u;
    state.phase = 1u;
    request_actor_sound(context.actor, 1, state.first_sound.data(), context.sounds); // 0x00041D00
    if (records.body != nullptr && records.morph != nullptr && state.morph != 0)
    {
        start_runtime_body_animation_record(*records.body, context.actions); // 0x0001AFA0, ECX +0x1C
        start_actor_record(state.morph, *records.morph, context); // 0x0003A9E0, ECX +0xA4
    }
    return true;
}

// Xbox 0x000A4690
void destroy_runtime_transform_state(TransformState& state, RuntimeTransformStateContext& context)
{
    context.bits.bytes[6] |= 0x10;
    release_morph(state, false, context);
    transform_records.erase(body_state_key(state));
}

// Xbox 0x000A46D0
void tick_runtime_transform_state(TransformState& state, RuntimeTransformStateContext& context)
{
    const auto& records = transform_records.at(body_state_key(state));
    switch (state.phase)
    {
    case 1:
    {
        const MinaPlayer* player = first_player(context.animation); // Actor +0x50 -> +0x58 -> [0].
        if (state.shown == 0 && state.morph != 0)
        {
            float shown = player != nullptr ? player->position : 0.f; // Player +0x14.
            shown = shown * 1.3333333730697632f * 32.f; // DAT_0032B3C0, DAT_0032B458.
            if (shown > 32.f)
            {
                shown = 32.f;
            }
            else if (0.f > shown)
            {
                shown = 0.f;
            }
            const i32 nodes = _mm_cvttss_si32(_mm_set_ss(shown));
            for (i32 node = 0; node < nodes; ++node)
            {
                set_actor_node(state.morph, u8(node), 0, context); // 0x000F96F0, AL 0
            }
            if (nodes == 0x20)
            {
                state.shown = 1u;
                unregister_actor_render_bins(context.render); // 0x0003CBB0, EDI the actor
            }
        }
        if (player != nullptr)
        {
            bool finished = false;
            if (player->mode == 1) // Player +0x2A.
            {
                bool running;
                if (0.f > player->speed) // Player +0x1C.
                {
                    running = 0.f < player->position;
                }
                else
                {
                    running = player->position < 1.f; // DAT_0032B0F8.
                }
                finished = !running;
            }
            if (!finished)
            {
                return;
            }
        }
        state.phase = 2u;
        request_actor_sound(context.actor, 4, state.second_sound.data(), context.sounds); // 0x00041D00
        if (records.second != nullptr && state.morph != 0)
        {
            start_actor_record(state.morph, *records.second, context); // The tail jump to 0x0003A9E0.
            return;
        }
        state.second_hold = 0.f;
        return;
    }
    case 2:
    {
        float hold = state.second_hold;
        const bool done = count_down(hold, context);
        state.second_hold = hold;
        if (!done)
        {
            return;
        }
        state.phase = 3u;
        request_actor_sound(context.actor, 4, state.third_sound.data(), context.sounds); // 0x00041D00
        if (records.third != nullptr && state.morph != 0)
        {
            start_actor_record(state.morph, *records.third, context); // The jump to 0x0003A9E0.
            return;
        }
        state.third_hold = 0.f;
        return;
    }
    case 3:
    {
        float hold = state.third_hold;
        const bool done = count_down(hold, context);
        state.third_hold = hold;
        if (!done)
        {
            return;
        }
        state.phase = 4u;
        const u32 created = create_result(state, context); // 0x000A4BB0
        if (created == 0)
        {
            throw Error("0x000A4CA0 sends the emerge message to a transform result that was not made");
        }
        const std::array<u32, 2> emerge{0x65, 0x3FD};
        context.message(created, emerge); // 0x000A4CA0: the new actor's vtable +0xC.
        release_morph(state, true, context); // 0x0013B1F0 and 0x00112CC0 on the morph actor
        activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI the body's actor
        request_actor_sound(actor_of(created, context), 1, state.result_sound.data(),
            context.sounds); // The tail jump to 0x00041D00, EDX the new actor
        return;
    }
    default:
        return;
    }
}

// Xbox 0x000A49D0
void message_runtime_transform_state(TransformState& state, std::span<const u32> message,
    RuntimeTransformStateContext& context)
{
    switch (message[0])
    {
    case 0x15:
    case 0x18:
    case 0x19:
    case 0x1B:
    case 0x1D:
    {
        const char* sound = state.hit_sound.data();
        if (sound[0] != 0)
        {
            queue_actor_sound(context.actor.sound_slots[1], 0, &context.actor, 1, nullptr, nullptr, 1.f, sound,
                context.sounds); // 0x0012A260, EBX actor +0x3EC
        }
        register_actor_render_bins(context.render); // 0x0003CAE0
        finish_runtime_body_death(1, context.scene_flags, context.restart_requested, context.death); // 0x00134AE0
        release_morph(state, true, context); // 0x0013B1F0 and 0x00112CC0 on the morph actor
        return;
    }
    case 0x7C:
        if (message[1] == state.morph)
        {
            release_morph(state, false, context); // 0x00112CC0 on the morph actor
        }
        return;
    default:
        return;
    }
}

void install_runtime_transform_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeTransformStateContext&(u32 body_address)> context_of)
{
    transform_context_of = std::move(context_of);
    types[0x8C] = ObjectFactoryType{9, transform_state_lifecycle}; // Getter 0x000A43A0.
}
}
