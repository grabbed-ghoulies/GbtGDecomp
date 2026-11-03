#include "body_bandaged_state.h"
#include "actor_render_registration.h"
#include "actor_scare_requests.h"
#include "actor_trail_requests.h"
#include "player_mind_control.h"
#include "player_mind_requests.h"
#include "gbtg/anim.h"
#include "gbtg/object_factory.h"
#include "body_fields.h"

#include <array>
#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace
{
namespace field = bandaged_state_field;
namespace parameter = bandaged_state_parameter;

constexpr float kScaleShare = 0.05f; // DAT_0032B250.
constexpr float kBindTime = 8.f; // DAT_0032B26C.
constexpr float kWrapDone = std::bit_cast<float>(0x3F666666u); // 0.9, pushed for 0x0003B1F0.
constexpr float kStruggleEnd = 1.f; // Pushed for 0x0003B1F0.
constexpr float kStruggleFull = 20.f; // DAT_0032B234.
constexpr float kHalf = 0.5f; // DAT_0032B0F4.
constexpr float kFadeTime = 0.2f; // Pushed for 0x0013AF80.
constexpr float kBitsLift = std::bit_cast<float>(0x3F19999Au); // DAT_0032B248, 0.6.
constexpr float kWrapBindTime = kBitsLift; // DAT_0032B248 again, the bind animation's 0.6 s.
constexpr float kUnwrapSpeed = -0.5f; // Pushed for 0x0006CBB0.
constexpr float kUnwrapStart = 1.f;
constexpr float kUnwrapFade = 0.2f;
constexpr const char* kWrapSound = "XACT_SOUNDBANK_GMUMMY_BANDAGEWRAP"; // 0x00302620.
constexpr const char* kRipSound = "XACT_SOUNDBANK_GMUMMY_BANDAGERIP"; // 0x003026A0.

std::function<RuntimeBandagedStateContext&(u32)> bandaged_context_of;

// The three animation records the state found (0x0003AAA0), held natively beside the state's +0x2BC .. +0x2C4 words.
std::unordered_map<const u8*, std::array<const AnimationRecord*, 3>> bandaged_records;

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

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void copy_name(std::span<u8> destination, size_t offset, const char* source)
{
    std::strcpy(reinterpret_cast<char*>(destination.data() + offset), source);
}

const char* text(std::span<const u8> state, size_t offset)
{
    return reinterpret_cast<const char*>(state.data() + offset);
}

// The inlined 0x0003AAA0 pair (the attached actor's +0xDB0 variant first, then the plain name).
const AnimationRecord* find_animation(u32 name, RuntimeBandagedStateContext& context)
{
    return find_runtime_body_animation(context.image_string(name), context.actions);
}

const AnimationRecord& record_of(std::span<const u8> state, size_t index, const char* where)
{
    const auto found = bandaged_records.find(state.data());
    const AnimationRecord* record = found == bandaged_records.end() ? nullptr : found->second[index];
    if (record == nullptr)
    {
        throw Error(std::string(where) + " starts an animation record (0x0003A9E0, ECX) the body's table lacks");
    }
    return *record;
}

// A scene actor by address: the body's own actor, or a spawned avatar.
Actor& actor_at(u32 address, RuntimeBandagedStateContext& context)
{
    if (address == context.actor_address)
    {
        return context.actor;
    }
    return context.actor_view(address).bat.actor;
}

// The body's held actor (+0x48) render bins.
ActorRenderRegistration& held_render(RuntimeBandagedStateContext& context)
{
    return context.actor_view(get(context.body, body_field::attached)).render;
}

// The +0x309..+0x30C counts move by `step`.
void adjust_body_counts(std::span<u8> body, i32 step)
{
    for (size_t offset = 0x309; offset <= 0x30C; ++offset)
    {
        body[offset] = u8(body[offset] + step);
    }
}

// The avatar follows the body's world position and euler (0x00041730, 0x00105AA0).
void place_avatar(u32 address, const Vec3& position, const Vec3& euler, RuntimeBandagedStateContext& context)
{
    Actor& avatar = context.actor_view(address).bat.actor;
    actor_set_world_position(avatar, position); // 0x00041730, EDI avatar, ESI position
    normalize_euler_angles(avatar.euler, euler); // 0x00105AA0, ESI euler, EDI avatar +0x14
    avatar.xform_valid = 0; // +0x2A8.
}

// The tail shared by 0x0008AF60 and 0x0008B050: the stream's last effect (DAT_0042A434) is flagged 8.
void flag_last_effect()
{
    const i16 index = i16(g_last_actor_effect);
    g_last_actor_effect = -1;
    if (index != -1)
    {
        g_actor_effect_nodes[index]->bytes[0x18] |= 8;
    }
}

bool construct_bandaged_state_view(FactoryObject& object, std::span<const u8> parameters)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    return initialize_runtime_bandaged_state(state, parameters, bandaged_context_of(body_address_of(state)));
}

void destroy_bandaged_state_view(FactoryObject& object)
{
    const std::span<u8> state{object.bytes.get(), object.size};
    destroy_runtime_bandaged_state(state, bandaged_context_of(body_address_of(state)));
}

const ObjectFactoryVtable* bandaged_state_lifecycle()
{
    static const ObjectFactoryVtable table{0x005056E0, bandaged_state_object_sizes, construct_bandaged_state_view,
        destroy_bandaged_state_view};
    return &table;
}
}

// Xbox 0x0008A2E0
void bandaged_state_object_sizes(ObjectSizes& output)
{
    output.instance = field::kSize;
    output.parameters = parameter::kSize;
}

// Xbox 0x0008A360
bool initialize_runtime_bandaged_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimeBandagedStateContext& context)
{
    const auto body = context.body;
    set(state, field::scale, context.actor.radius_y * get(body, body_field::height_scale) * kScaleShare);
    body[0x38] = 1;
    body[0x39] = 1;
    release_runtime_player_attachment(0, 0, context.attachment); // 0x0001B810, EAX body
    auto& records = bandaged_records[state.data()];
    records[0] = find_animation(get(parameters, parameter::still_animation), context);
    set(state, field::still_animation, records[0] != nullptr ? 1u : 0u);
    records[1] = find_animation(get(parameters, parameter::struggle_animation), context);
    set(state, field::struggle_animation, records[1] != nullptr ? 1u : 0u);
    records[2] = find_animation(get(parameters, parameter::escape_animation), context);
    set(state, field::escape_animation, records[2] != nullptr ? 1u : 0u);
    set(state, field::unk_3C8, kBindTime);
    set(state, field::timer, kBindTime);
    set(state, field::still_sound, get(parameters, parameter::still_sound));
    set(state, field::struggle_sound, get(parameters, parameter::struggle_sound));
    set(state, field::mummified_model, get(parameters, parameter::mummified_model));
    set(state, field::wrap_model, get(parameters, parameter::wrap_model));
    set(state, field::bind_animation, get(parameters, parameter::bind_animation));
    set(state, field::dust_effect, get(parameters, parameter::dust_effect));
    set(state, field::bits_effect, get(parameters, parameter::bits_effect));
    put_float(state, 0x2A8, 0.f);
    put_float(state, 0x2A4, 0.f);
    put_float(state, 0x2B0, 0.f);
    put_float(state, 0x2AC, 0.f);
    set(state, field::tracking_state, 1u);
    put_float(state, 0x2B4, 0.f);
    set_runtime_bandaged_phase(1, state, context); // 0x0008A950, EAX state
    return true;
}

// Xbox 0x0008A5A0
void destroy_runtime_bandaged_state(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    release_runtime_bandaged_state(state, context); // 0x0008AB60, EDI state
    const u32 phase = get(state, field::phase);
    if (phase == 1)
    {
        release_actor_sound(context.actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
    }
    else if (phase == 2 || phase == 3)
    {
        release_actor_sound(context.actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    set(state, field::phase, 0u);
    bandaged_records.erase(state.data());
}

// Xbox 0x0008A5F0
void tick_runtime_bandaged_state(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const auto body = context.body;
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    set(state, body_state_field::elapsed, rate * step + get(state, body_state_field::elapsed));
    level_body_euler(body);
    std::memcpy(body.data() + 0x318, &context.default_position, sizeof(Vec3));
    if (context.input == nullptr)
    {
        throw Error("bandaged state 0x0008A5F0 reads the pad (0x001085F0, EAX) of a tick without its input device");
    }
    std::array<float, 2> axes{};
    runtime_input_axes(*context.input, axes, context.input_state); // 0x001085F0, EAX the tick's input
    update_runtime_player_input_tracking(state.subspan(field::kTracking, input_tracking_field::kSize), axes,
        context.struggle_preset, 1, context.clock); // 0x00109810, ESI state +0x2A4, EDI 0x0030260C
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820
    if (const u32 mummified = get(state, field::mummified); mummified != 0)
    {
        place_avatar(mummified, position, euler, context);
    }
    if (const u32 wrap = get(state, field::wrap); wrap != 0)
    {
        place_avatar(wrap, position, euler, context);
        NativeActorFactoryInstance& view = context.actor_view(wrap);
        if (actor_animation_completed(view.animation, 0)) // The inlined 0x0003B290, slot 0.
        {
            const Actor& avatar = view.bat.actor;
            const std::array<float, 3> tint{avatar.scale_x * kHalf, avatar.radius_y, avatar.radius_xz * kHalf};
            context.actor_fade(wrap, kFadeTime, tint.data()); // 0x0013AF80, EAX wrap, EDI the tint
            activate_actor_bind_tree(context.actor_view(wrap).bat.actor); // 0x00112CC0, ESI wrap
            set(state, field::wrap, 0u);
        }
    }
    switch (get(state, field::phase))
    {
    case 1:
    {
        const u32 wrap = get(state, field::wrap);
        if (wrap == 0 || runtime_actor_animation_crossed_position(context.actor_view(wrap).animation, 0,
                             kWrapDone)) // 0x0003B1F0, EAX wrap, ECX 0
        {
            hide_runtime_bandaged_body(state, context); // 0x0008AAD0, EAX state
            set_runtime_bandaged_phase(2, state, context); // 0x0008A950, EAX state
        }
        return;
    }
    case 2:
        if (get(state, field::tracking_state) == 2)
        {
            set_runtime_bandaged_phase(3, state, context); // 0x0008A950, EAX state
        }
        break;
    case 3:
    {
        const u32 mummified = get(state, field::mummified);
        if (mummified == 0)
        {
            throw Error("bandaged state 0x0008A5F0 reads the animation (+0x50) of a mummified avatar it has none of");
        }
        if (runtime_actor_animation_crossed_position(context.actor_view(mummified).animation, 0,
                kStruggleEnd) // 0x0003B1F0, EAX mummified, ECX 0
            && get(state, field::tracking_state) != 2)
        {
            set_runtime_bandaged_phase(2, state, context); // 0x0008A950, EAX state
        }
        break;
    }
    case 4:
        if (runtime_actor_animation_slot_complete(context.animation, 0)) // 0x0003B290, EAX actor, ECX 0
        {
            if (get(body, body_field::energy) == 0)
            {
                select_runtime_body_recovery_action(state, 0, context.recovery); // 0x00133F50, EAX body
                return;
            }
            set(state, body_state_field::next_key, context.body_slot_key(0x20)); // Body vtable +0x20.
        }
        return;
    default:
        return;
    }
    if (runtime_bandaged_escape_due(state, context)) // 0x0008B140, EAX state, ECX body
    {
        set_runtime_bandaged_phase(4, state, context); // 0x0008A950, EAX state
    }
}

// Xbox 0x0008A8E0
void message_runtime_bandaged_state(std::span<u8> state, std::span<const u32> message,
    RuntimeBandagedStateContext& context)
{
    if (message[0] == 0x61)
    {
        if (get(state, field::mummified) != 0)
        {
            start_runtime_bandage_dust(context.actor_address, state, context); // 0x0008AF60, EAX actor
            start_runtime_bandage_bits(context.actor_address, state, context); // 0x0008B050, EAX actor
            spawn_runtime_bandage_unwrap(state, context); // 0x0008ADC0
        }
        return;
    }
    if (message[0] == 0x7C)
    {
        if (message[1] == get(state, field::mummified))
        {
            set(state, field::mummified, 0u);
        }
        if (message[1] == get(state, field::wrap))
        {
            set(state, field::wrap, 0u);
        }
    }
}

// Xbox 0x0008A950, EAX state
void set_runtime_bandaged_phase(u32 phase, std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const auto body = context.body;
    Actor& actor = context.actor;
    const u32 previous = get(state, field::phase);
    if (previous == 1)
    {
        release_actor_sound(actor.sound_slots[1], context.sounds); // 0x0012A3D0, ESI actor +0x3EC
    }
    else if (previous == 2 || previous == 3)
    {
        release_actor_sound(actor.sound_slots[4], context.sounds); // 0x0012A3D0, ESI actor +0x614
    }
    set(state, field::phase, phase);
    switch (phase)
    {
    case 1:
    {
        spawn_runtime_bandage_wrap(state, context); // 0x0008AC30
        set(body, body_field::move_holds, u16(get(body, body_field::move_holds) + 1));
        set(state, field::held, 1u);
        request_actor_sound(actor, 5, kWrapSound, context.sounds); // 0x00041D00, EDX actor, ECX 5
        const std::array<u32, 3> words{0x68, context.actor_address, get(body, body_field::attacker)};
        broadcast_actor_world_message(words); // 0x00122CE0, EDI the message
        return;
    }
    case 2:
    case 3:
    {
        const u32 mummified = get(state, field::mummified);
        if (mummified == 0)
        {
            throw Error("bandaged state 0x0008A950 starts an animation (0x0003A9E0, EAX) on a mummified avatar it "
                "has none of");
        }
        const size_t index = phase == 2 ? 0 : 1;
        auto& actions = context.actions;
        start_actor_animation(context.actor_view(mummified).animation,
            record_of(state, index, "bandaged state 0x0008A950"), actions.registry, actions.error,
            actions.random); // 0x0003A9E0, EAX mummified, ECX the record
        const size_t sound = phase == 2 ? field::still_sound.offset : field::struggle_sound.offset;
        request_actor_sound(actor, 4, text(state, sound), context.sounds); // The tail of 0x0008A950: 0x00041D00
        return;
    }
    case 4:
    {
        auto& actions = context.actions;
        start_actor_animation(context.animation, record_of(state, 2, "bandaged state 0x0008A950"),
            actions.registry, actions.error, actions.random); // 0x0003A9E0, EAX actor, ECX the record
        start_runtime_bandage_dust(context.actor_address, state, context); // 0x0008AF60, EAX actor
        start_runtime_bandage_bits(context.actor_address, state, context); // 0x0008B050, EAX actor
        spawn_runtime_bandage_unwrap(state, context); // 0x0008ADC0
        release_runtime_bandaged_state(state, context); // 0x0008AB60, EDI state
        request_actor_sound(actor, 5, kRipSound, context.sounds); // 0x00041D00, EDX actor, ECX 5
        // Retail leaves the third word of the 0x69 message unwritten; its receivers read the first two.
        const std::array<u32, 2> words{0x69, context.actor_address};
        broadcast_actor_world_message(words); // 0x00122CE0, EDI the message
        return;
    }
    default:
        return;
    }
}

// Xbox 0x0008AAD0, EAX state
void hide_runtime_bandaged_body(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const auto body = context.body;
    context.bits.bytes[2] |= 1;
    adjust_body_counts(body, 1);
    put_word(body, 0x310, 1);
    spawn_runtime_mummified_avatar(state, context); // 0x0008AE30, EBX state
    unregister_actor_render_bins(context.render); // 0x0003CBB0, EDI actor
    if (get(body, body_field::attached) != 0)
    {
        unregister_actor_render_bins(held_render(context)); // 0x0003CBB0, EDI body +0x48
    }
    set(state, field::hidden, 1u);
}

// Xbox 0x0008AB60, EDI state
void release_runtime_bandaged_state(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const auto body = context.body;
    if (get(state, field::hidden) != 0)
    {
        adjust_body_counts(body, -1);
        put_word(body, 0x310, 0);
        context.bits.bytes[2] &= 0xFE;
        if (context.render.child.prev == nullptr) // Actor +0x908: the actor is not being deleted.
        {
            register_actor_render_bins(context.render); // 0x0003CAE0
        }
        if (get(body, body_field::attached) != 0)
        {
            register_actor_render_bins(held_render(context)); // 0x0003CAE0
        }
        set(state, field::hidden, 0u);
    }
    if (get(state, field::held) != 0)
    {
        set(body, body_field::move_holds, u16(get(body, body_field::move_holds) - 1));
        set(state, field::held, 0u);
    }
    if (const u32 mummified = get(state, field::mummified); mummified != 0)
    {
        activate_actor_bind_tree(context.actor_view(mummified).bat.actor); // 0x00112CC0, ESI mummified
        set(state, field::mummified, 0u);
    }
    if (const u32 wrap = get(state, field::wrap); wrap != 0)
    {
        activate_actor_bind_tree(context.actor_view(wrap).bat.actor); // 0x00112CC0, ESI wrap
        set(state, field::wrap, 0u);
    }
}

// Xbox 0x0008AC30
void spawn_runtime_bandage_wrap(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.empty_name); // 0x000411A0
    std::memcpy(parameters.data() + 8, &position, sizeof(position));
    std::memcpy(parameters.data() + 0x14, &euler, sizeof(euler));
    put_float(parameters, 0x20, get(state, field::scale));
    copy_name(parameters, 0x28, text(state, field::wrap_model.offset));
    const u32 wrap = context.create_object(parameters); // 0x00107520
    set(state, field::wrap, wrap);
    if (wrap == 0)
    {
        throw Error("bandaged state 0x0008AC30 reads the animation (+0x50) of a bandage wrap it failed to create");
    }
    // 'PxiM': 0.6 s (DAT_0032B248), DAT_0032B1FC, 0x21, DAT_0032B1AC, mode 1, 0x31, DAT_0032B1AC, no events.
    AnimationMix mix;
    mix.duration = kWrapBindTime;
    mix.weight = -10.f; // DAT_0032B1FC.
    mix.blend_mode = 0x21;
    mix.fade_duration = 0.2f; // DAT_0032B1AC.
    mix.position = 0.f;
    mix.mode = 1;
    mix.priority = 0x31;
    mix.old_pose_duration = 0.2f; // DAT_0032B1AC.
    mix.events = std::string_view(""); // +0x24's first byte is 0.
    ActorAnimationState& animation = context.actor_view(wrap).animation;
    if (animation.controller == nullptr)
    {
        animation.controller = make_animation_controller(4); // 0x00030740, ESI 4
        animation.controller->event_callback = [](std::span<const u8> event, u32 index, float position,
            void* owner, int phase)
        {
            animation_actor_event(event, index, position, *static_cast<ActorAnimationState*>(owner), phase); // 0x0003B3C0
        };
        animation.controller->event_owner = &animation;
    }
    auto& actions = context.actions;
    start_controller_animation(*animation.controller, 0, {}, text(state, field::bind_animation.offset), mix,
        actions.registry, actions.error, actions.random); // 0x00030880, its result unused
}

// Xbox 0x0008ADC0
void spawn_runtime_bandage_unwrap(std::span<const u8> state, RuntimeBandagedStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820
    std::array<u8, 0x340> parameters{};
    build_runtime_anim_delete_parameters(parameters, position, euler, get(state, field::scale),
        text(state, field::wrap_model.offset), text(state, field::bind_animation.offset), kUnwrapSpeed,
        kUnwrapStart, kUnwrapFade, context.empty_name); // 0x0006CBB0
    context.create_object(parameters); // 0x00107520
}

// Xbox 0x0008AE30, EBX state
void spawn_runtime_mummified_avatar(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    const Vec3 position = actor_world_position(context.actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(context.actor); // 0x00041820
    std::array<u8, 0x334> parameters{};
    initialize_actor_effect_parameters(parameters.data(), context.empty_name); // 0x000411A0
    std::memcpy(parameters.data() + 8, &position, sizeof(position));
    std::memcpy(parameters.data() + 0x14, &euler, sizeof(euler));
    const char* model = text(state, field::mummified_model.offset);
    copy_name(parameters, 0x28, model);
    put_word(parameters, 0x2B0, read_u32(context.actor_bytes, 0x30C));
    copy_name(parameters, 0x2B4, model);
    const u32 mummified = context.create_object(parameters); // 0x00107520
    set(state, field::mummified, mummified);
    if (mummified == 0)
    {
        throw Error("bandaged state 0x0008AE30 scales (+0x30) a mummified avatar it failed to create");
    }
    Actor& avatar = context.actor_view(mummified).bat.actor;
    const float scale = get(state, field::scale);
    if (avatar.scale_x != scale || avatar.radius_y != scale || avatar.radius_xz != scale) // UCOMISS; JP sets them.
    {
        avatar.scale_x = scale;
        avatar.radius_y = scale;
        avatar.radius_xz = scale;
        avatar.xform_valid = 0;
    }
    actor_refresh_transform(avatar); // 0x00040DE0
    start_runtime_bandage_dust(mummified, state, context); // 0x0008AF60, EAX mummified
}

// Xbox 0x0008AF60, EAX actor
void start_runtime_bandage_dust(u32 actor_address, std::span<const u8> state, RuntimeBandagedStateContext& context)
{
    Actor& actor = actor_at(actor_address, context);
    g_actor_effect_binding.mode = 0; // DAT_0050690C.
    g_actor_effect_binding.actor = &actor;
    g_actor_effect_binding.index = -1;
    g_actor_creation_options.user = &g_actor_effect_binding; // DAT_004E1C8C.
    g_actor_creation_options.flags = 8; // DAT_004E1C5C.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(actor); // 0x00041820
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags |= 1;
    g_actor_creation_options.flags |= 4;
    g_actor_creation_options.euler = euler;
    if (load_actor_effect_stream(text(state, field::dust_effect.offset), &g_actor_creation_options,
            context.effects) != 0) // 0x00053170
    {
        flag_last_effect();
    }
}

// Xbox 0x0008B050, EAX actor
void start_runtime_bandage_bits(u32 actor_address, std::span<const u8> state, RuntimeBandagedStateContext& context)
{
    const Actor& actor = actor_at(actor_address, context);
    g_actor_creation_options.flags = 0; // DAT_004E1C5C.
    Vec3 position = actor_world_position(actor); // 0x000412A0
    const Vec3 euler = actor_world_euler(actor); // 0x00041820
    position.y = (actor.model_size_y - actor.model_size_z) * actor.radius_y * kBitsLift + position.y; // +0x1C8, +0x1CC
    g_actor_creation_options.flags |= 1;
    g_actor_creation_options.position = position;
    g_actor_creation_options.flags |= 4;
    g_actor_creation_options.euler = euler;
    if (load_actor_effect_stream(text(state, field::bits_effect.offset), &g_actor_creation_options,
            context.effects) != 0) // 0x00053170
    {
        flag_last_effect();
    }
}

// Xbox 0x0008B140, EAX state, ECX body
bool runtime_bandaged_escape_due(std::span<u8> state, RuntimeBandagedStateContext& context)
{
    if (get(context.body, body_field::energy) == 0)
    {
        return true;
    }
    const float step = runtime_frame_step(context.clock);
    const float rate = runtime_actor_rate(context.actor);
    const float timer = get(state, field::timer);
    if (timer > 0.f) // COMISS; JBE checks the struggle.
    {
        const float left = timer - rate * step;
        set(state, field::timer, left);
        if (0.f >= left) // COMISS; JC (below or unordered) checks the struggle.
        {
            set(state, field::timer, 0.f);
            return true;
        }
    }
    return get(state, field::struggle) == kStruggleFull; // UCOMISS; JP keeps the bandages on.
}

// Xbox 0x0006CBB0, EAX parameters, EDI position, ECX euler, EBX model
void build_runtime_anim_delete_parameters(std::span<u8, 0x340> parameters, const Vec3& position, const Vec3& euler,
    float scale, const char* model, const char* animation, float speed, float start, float fade,
    const char* default_name)
{
    initialize_actor_effect_parameters(parameters.data(), default_name); // 0x000411A0
    put_half(parameters, 0, 0x340);
    put_half(parameters, 2, 0x5C);
    put_half(parameters, 4, 0x14);
    std::memcpy(parameters.data() + 8, &position, sizeof(position));
    std::memcpy(parameters.data() + 0x14, &euler, sizeof(euler));
    put_float(parameters, 0x20, scale);
    copy_name(parameters, 0x28, model);
    copy_name(parameters, 0xA8, animation);
    put_float(parameters, 0x338, speed);
    put_float(parameters, 0x334, start);
    put_float(parameters, 0x33C, fade);
}

void install_runtime_bandaged_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeBandagedStateContext&(u32 body_address)> context_of)
{
    bandaged_context_of = std::move(context_of);
    types[0x60] = ObjectFactoryType{9, bandaged_state_lifecycle}; // Getter 0x0008A300.
}
}
