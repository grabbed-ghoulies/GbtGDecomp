#include "trophy_heads_control.h"
#include "scene_controls.h"
#include "gbtg/sound_runtime.h"
#include "../camera/camera_body_slots.h"
#include "../cutscenes/actor_tracks.h"
#include "../cutscenes/camera_sampling.h"
#include "../entities/actor_shape_bounds.h"
#include "../frontend/dialogue_resources.h"

#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float real(std::span<const u8> bytes, size_t offset)
{
    return std::bit_cast<float>(read_u32(bytes, offset));
}

void put_real(std::span<u8> bytes, size_t offset, float value)
{
    put_word(bytes, offset, std::bit_cast<u32>(value));
}

u16 half(std::span<const u8> bytes, size_t offset)
{
    return read_u16(bytes, offset);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put_vector(std::span<u8> bytes, size_t offset, const Vec3& value)
{
    put_real(bytes, offset, value.x);
    put_real(bytes, offset + 4, value.y);
    put_real(bytes, offset + 8, value.z);
}

void copy_string(std::span<u8> bytes, size_t offset, const char* text)
{
    size_t at = 0;
    char copied;
    do
    {
        copied = text[at];
        bytes[offset + at] = u8(copied);
        ++at;
    }
    while (copied != 0);
}

// The step the tick adds twice: nothing while frozen (DAT_004312DC > 0), else DAT_005096C0 * DAT_004312C4.
float frame_step(const TrophyHeadsControlServices& services)
{
    float step = 0.f;
    if (services.freeze <= 0.f)
    {
        step = services.dt * services.scene_scale;
    }
    return step;
}

// DAT_004E1C38[DAT_004E1C34].
RuntimeCutsceneInstance& current_cutscene(TrophyHeadsControlServices& services)
{
    auto& cutscenes = services.cutscenes;
    if (cutscenes.index < 0 || cutscenes.instances[size_t(cutscenes.index)] == nullptr)
    {
        throw Error("trophy-heads control reads the cutscene stack with no cutscene (DAT_004E1C34 < 0)");
    }
    return *cutscenes.instances[size_t(cutscenes.index)];
}

const CutsceneResourceView& tracks_of(const RuntimeCutsceneInstance& instance, TrophyHeadsControlServices& services)
{
    return services.cutscenes.bindings.runtime.views.at(instance.resource);
}

CutsceneActorRowView& row_of(RuntimeCutsceneInstance& instance, i32 index, TrophyHeadsControlServices& services)
{
    auto& rows = services.cutscenes.bindings.runtime.instances.at(&instance).actors;
    if (index < 0 || size_t(index) >= rows.size())
    {
        throw Error("trophy-heads control reads a cutscene actor row outside the clip (+0x44)");
    }
    return rows[size_t(index)];
}

// Resource +3: the clip's track count.
std::int8_t track_count(const RuntimeCutsceneInstance& instance)
{
    return std::int8_t(instance.resource[3]);
}

ActorAnimationState& animation_of(Actor& actor, TrophyHeadsControlServices& services)
{
    return *services.cutscenes.bindings.runtime.animations.at(&actor);
}

const TrophyHeadRow& head_row(i32 id, const TrophyHeadsControlServices& services)
{
    if (id < 0 || size_t(id) >= services.heads.size())
    {
        throw Error("trophy head id lies past the DAT_00301920 table");
    }
    return services.heads[size_t(id)];
}

// A unit random in [1, 2): the 0x0054DD7C step with the mantissa forced under 1.0.
float random_unit(u32& random)
{
    random = random * 0x19660D + 0x3C6EF35F;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u);
}
}

// Xbox 0x000816A0
void trophy_heads_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x188;
    output.parameters = 0x108;
}

// Xbox 0x00081710
bool construct_runtime_trophy_heads_control(std::span<u8> state, std::span<const u8> parameters,
    TrophyHeadsControlState& control, TrophyHeadsControlServices& services)
{
    copy_string(state, 0x104, reinterpret_cast<const char*>(parameters.data() + 0x88));
    put_word(state, 0x100, 0);
    put_word(state, 0x30, 0);
    if ((services.scene_flags & 8) == 0 && (services.runtime_flags & 0x200000) == 0)
    {
        put_word(state, 0x34, 0x32);
    }
    else
    {
        put_word(state, 0x34, 10);
    }
    for (size_t slot = 0; slot < 0x15; ++slot)
    {
        put_word(state, 0x4C + slot * 4, 0);
        put_word(state, 0xA0 + slot * 4, 0);
    }
    choose_runtime_trophy_heads(state, services);
    control.cutscene = start_scene_cutscene(reinterpret_cast<const char*>(parameters.data() + 8), 0, 1,
        services.cutscenes);
    --services.pause_count;
    --services.transition_pause_count;
    put_word(state, 0x100, 1);
    set_runtime_trophy_heads_state(state, 1, control, services);
    services.story_byte |= 8;
    put_word(state, 0x184, (services.challenge_bits[0] >> 1) & 1);
    if (((services.scene_flags & 8) != 0 || (services.runtime_flags & 0x200000) != 0)
        && services.chapter_mode == 1)
    {
        services.chapter_mode = 0;
        if (std::int8_t(services.runtime_flags >> 8) >= 0)
        {
            auto& pulse = services.pulse;
            pulse.remaining = 1.5f; // DAT_0032B210
            pulse.duration = 1.5f;
            pulse.first = 10.f; // DAT_0032B10C
            pulse.second = 0.f;
            pulse.third = 0.f;
            pulse.mode = 0;
            pulse.fourth = 1.f; // DAT_0032B0F8
            pulse.active = 1;
            return true;
        }
    }
    return true;
}

// Xbox 0x00081AD0
void destroy_runtime_trophy_heads_control(std::span<u8> state, TrophyHeadsControlServices& services)
{
    if (word(state, 0x100) != 0)
    {
        ++services.pause_count;
        ++services.transition_pause_count;
        put_word(state, 0x100, 0);
    }
}

// Xbox 0x00081B00
void message_runtime_trophy_heads_control(std::span<u8> state, std::span<const u32> message,
    const void* reference, TrophyHeadsControlState& control, SceneControlInstance& instance,
    SceneControlLists& lists, TrophyHeadsControlServices& services)
{
    if (message[0] == 0x7F)
    {
        if (reference == control.cutscene && word(state, 0x100) != 0)
        {
            ++services.pause_count;
            ++services.transition_pause_count;
            put_word(state, 0x100, 0);
        }
    }
    else if (message[0] == 0x8E)
    {
        const std::int8_t id = std::int8_t(state[0xF4]);
        switch (message[1])
        {
        case 0:
        case 2:
        {
            char animation[0x80];
            char events[0x80];
            auto& runtime = services.cutscenes.bindings.runtime;
            set_runtime_cutscene_actor_animation(current_cutscene(services), id, nullptr, nullptr,
                services.default_euler, runtime);
            const auto& row = head_row(i32(word(state, 0xF4)), services);
            format_dialogue_resource_name(animation, "aid_anim_ghoulies_actor_trophyheads_%s", row.animation);
            format_dialogue_resource_name(events, "aid_animevents_ghoulies_actor_trophyheads_%s", row.events);
            set_runtime_cutscene_actor_animation(current_cutscene(services), id, animation, events,
                services.default_euler, runtime);
            break;
        }
        case 8:
            if (i32(word(state, 0x34)) - 10 <= 0)
            {
                set_runtime_trophy_heads_state(state, 5, control, services);
                break;
            }
            [[fallthrough]];
        case 7:
        {
            services.retire_actor(control.player);
            control.player = 0;
            put_word(state, 0x2C, 0);
            auto& runtime = services.cutscenes.bindings.runtime;
            set_runtime_cutscene_actor_animation(current_cutscene(services), id, nullptr, nullptr,
                services.default_euler, runtime);
            {
                auto& instance_now = current_cutscene(services);
                CutsceneEventPauseContext pause{runtime.instances.at(&instance_now).actors.data(), runtime.animations,
                    services.voices, runtime.resume_callbacks};
                pause_cutscene_events(instance_now, 0, pause); // 0x0004E640, EAX 0
            }
            set_runtime_trophy_heads_state(state, 1, control, services);
            const std::int8_t index = find_runtime_cutscene_track(services.cutscenes, std::int8_t(state[0xF4]));
            auto& instance_now = current_cutscene(services);
            Actor* actor = row_of(instance_now, index, services).actor;
            if (actor == nullptr)
            {
                throw Error("trophy head row has no actor; 0x0003B310 reads its +0x50");
            }
            hold_runtime_actor_animation_slot(animation_of(*actor, services), 1);
            if (message[1] == 8)
            {
                put_word(state, 0x34, word(state, 0x34) - 10);
            }
            break;
        }
        default:
            break;
        }
    }
    if (message[0] == 0x80)
    {
        retire_runtime_scene_control(instance, lists); // 0x00115E70, ESI the control
    }
}

// Xbox 0x00081890
void tick_runtime_trophy_heads_control(std::span<u8> state, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services)
{
    if ((services.runtime_flags & 0x400) == 0 && services.gameplay_tick != services.gameplay_mark)
    {
        services.gameplay_time += frame_step(services);
        services.gameplay_mark = services.gameplay_tick;
    }
    const u16 current = half(state, 0x44);
    if (current == 1)
    {
        if (0.f < real(state, 0x40))
        {
            auto& instance = current_cutscene(services);
            seek_runtime_cutscene(instance, real(state, 0x40) / instance.duration, 0.f, 0,
                services.cutscenes.bindings.runtime);
            put_real(state, 0x40, 0.f);
        }
        const auto& cutscenes = services.cutscenes;
        if (-1 < cutscenes.index && !(current_cutscene(services).time < current_cutscene(services).duration))
        {
            set_runtime_trophy_heads_state(state, 3, control, services);
        }
        else
        {
            std::int8_t index = 0;
            auto* instance = &current_cutscene(services);
            if (0 < track_count(*instance))
            {
                do
                {
                    const u8 id = u8(tracks_of(*instance, services).tracks[size_t(index)].id);
                    if (id != 0)
                    {
                        if (row_of(*instance, index, services).previous == 0)
                        {
                            if (word(state, 0x9C + size_t(id) * 4) != 0)
                            {
                                put_word(state, 0x9C + size_t(id) * 4, 0);
                            }
                        }
                        else if (runtime_trophy_head_chosen(state, std::int8_t(id)) != 0
                            && word(state, 0x9C + size_t(id) * 4) == 0 && word(state, 0x48 + size_t(id) * 4) == 0)
                        {
                            put_word(state, 0xF4, id);
                            put_word(state, 0x9C + size_t(id) * 4, 1);
                            put_word(state, 0x48 + size_t(id) * 4, 1);
                            set_runtime_trophy_heads_state(state, 2, control, services);
                        }
                    }
                    instance = &current_cutscene(services);
                    index = std::int8_t(u8(index) + 1);
                }
                while (index < track_count(*instance));
            }
        }
    }
    else if (current == 3)
    {
        const u32 event = 0xAF;
        services.queue_event({reinterpret_cast<const u8*>(&event), sizeof(event)}); // 0x00116810, EDI the event
    }
    else if (current == 4 && services.prompt_owner == 0)
    {
        set_runtime_trophy_heads_state(state, 1, control, services);
    }
    if (word(state, 0x184) == 0 && half(state, 0x44) == 1 && (services.runtime_flags & 0x4400) == 0)
    {
        const auto& instance = current_cutscene(services);
        if (0.985f < instance.time / instance.duration) // DAT_0032B6DC
        {
            set_runtime_trophy_heads_state(state, 4, control, services);
        }
    }
    put_real(state, 0x20, real(state, 0x20) + frame_step(services));
}

// Xbox 0x00081CE0
void set_runtime_trophy_heads_state(std::span<u8> state, u8 next, TrophyHeadsControlState& control,
    TrophyHeadsControlServices& services)
{
    auto& runtime = services.cutscenes.bindings.runtime;
    if (half(state, 0x44) == 4)
    {
        auto& instance = current_cutscene(services);
        CutsceneEventPauseContext pause{runtime.instances.at(&instance).actors.data(), runtime.animations,
            services.voices, runtime.resume_callbacks};
        pause_cutscene_events(instance, 0, pause); // 0x0004E640, EDI the clip, EAX 0
    }
    if (next == 1)
    {
        camera_bind_named_view(0, "cutscene"); // 0x00048CF0, EBX 0x00322DEC
        auto* instance = &current_cutscene(services);
        std::int8_t index = 0;
        if (0 < track_count(*instance))
        {
            do
            {
                const u8 id = u8(tracks_of(*instance, services).tracks[size_t(index)].id);
                if (id != 0)
                {
                    Actor* actor = row_of(*instance, index, services).actor;
                    char animation[0x80];
                    format_dialogue_resource_name(animation, "aid_anim_ghoulies_actor_trophyheads_%s",
                        head_row(id, services).animation);
                    set_runtime_cutscene_actor_animation(current_cutscene(services), std::int8_t(id), animation,
                        nullptr, services.default_euler, runtime);
                    if (actor == nullptr)
                    {
                        throw Error("trophy head row has no actor; 0x00081CE0 reads its +0x30");
                    }
                    Vec3 scale;
                    auto& sampled = current_cutscene(services);
                    sample_cutscene_actor_scale(&tracks_of(sampled, services).tracks[size_t(index)].scale, 0, 0.f,
                        scale); // 0x00051430, EAX resource +0x3C, EBX the output
                    if (scale.x != actor->scale_x || scale.y != actor->radius_y || scale.z != actor->radius_xz)
                    {
                        actor->scale_x = scale.x;
                        actor->radius_y = scale.y;
                        actor->radius_xz = scale.z;
                        actor->xform_valid = 0;
                    }
                    auto& clip = current_cutscene(services);
                    clip.euler = services.default_euler;
                    update_runtime_cutscene_actors(clip, -1.f, clip.time / clip.duration, runtime); // 0x0004E1E0
                    auto& viewed = current_cutscene(services);
                    update_cutscene_camera_view(viewed, tracks_of(viewed, services).cameras.data(),
                        services.cutscenes.camera); // 0x0004D7D0, DAT_004E1BC4
                    auto& animation_state = animation_of(*actor, services);
                    if (animation_state.controller != nullptr)
                    {
                        auto& controller = *animation_state.controller;
                        if (controller.slots[1].player != nullptr)
                        {
                            ++controller.slots[1].player->paused;
                        }
                        controller.dirty = true;
                    }
                    measure_runtime_actor_bound_size(*actor, 0x200, services.shapes(*actor));
                }
                instance = &current_cutscene(services);
                index = std::int8_t(u8(index) + 1);
            }
            while (index < track_count(*instance));
        }
    }
    else if (next == 2)
    {
        control.player = services.spawn_player(); // 0x00134920, EAX 0, the position and euler DAT_0050DD80
        put_word(state, 0x2C, control.player);
        std::span<u8> body = services.player_body(control.player); // +0x95C
        put_half(body, 0x94, half(state, 0x34));
        const std::int8_t index = find_runtime_cutscene_track(services.cutscenes, std::int8_t(state[0xF4]));
        Actor* head = row_of(current_cutscene(services), index, services).actor;
        const i32 count = i32(word(state, 0x30));
        put_word(state, 0x30, u32(count + 1));
        control.head = head;
        start_runtime_trophy_head_attack(count, i32(word(state, 0xF4)), head, services);
        put_half(state, 0x44, 2);
        return;
    }
    else if (next == 4)
    {
        auto& instance = current_cutscene(services);
        ++instance.pending[2]; // +0x158
        std::int8_t index = 0;
        if (0 < instance.actor_count)
        {
            do
            {
                auto& row = row_of(instance, index, services);
                if (Actor* actor = row.actor; actor != nullptr)
                {
                    auto& animation_state = animation_of(*actor, services);
                    if (row.flags != 0 && animation_state.controller != nullptr)
                    {
                        auto& controller = *animation_state.controller;
                        if (controller.slots[1].player != nullptr)
                        {
                            ++controller.slots[1].player->paused;
                        }
                        controller.dirty = true;
                    }
                    if (animation_state.controller != nullptr)
                    {
                        auto& controller = *animation_state.controller;
                        if (controller.slots[0].player != nullptr)
                        {
                            ++controller.slots[0].player->paused;
                        }
                        controller.dirty = true;
                    }
                }
                index = std::int8_t(u8(index) + 1);
            }
            while (i32(index) < instance.actor_count);
        }
        if ((instance.flags & 0x10) == 0)
        {
            for (size_t slot = 0; slot < 3; ++slot)
            {
                const i16 voice_index = i16(read_u16(instance.event_state, slot * 2)); // +0x48
                if (voice_index == -1)
                {
                    continue;
                }
                auto& voice = services.voices.voices.at(size_t(voice_index));
                if (voice.cue != nullptr)
                {
                    switch (voice.state)
                    {
                    case 2:
                        voice.state = 3;
                        break;
                    case 5:
                    case 6:
                    case 7:
                        voice.state = 7;
                        pause_runtime_sound_cue(voice.cue->runtime, 1, services.voices.core.sound); // 0x0015CDD0
                        break;
                    default:
                        break;
                    }
                }
            }
        }
        if ((services.challenge_bits[0] & 2) == 0)
        {
            award_runtime_powerup(1, 0, 1, 0.f, 0, services.powerups); // 0x00123E70, EDX 1
        }
        put_word(state, 0x184, 1);
        put_half(state, 0x44, 4);
        return;
    }
    put_half(state, 0x44, next);
}

// Xbox 0x000821A0
void choose_runtime_trophy_heads(std::span<u8> state, TrophyHeadsControlServices& services)
{
    put_word(state, 0x46, 0xFFFFFFFFu);
    std::int8_t id;
    do
    {
        id = std::int8_t(i32((random_unit(services.random) - 1.f) * 2.f + 1.f)); // DAT_0032B110
    }
    while (accept_runtime_trophy_head(state, id, services) == 0);
    state[0x46] = u8(id);
    do
    {
        id = std::int8_t(i32((random_unit(services.random) - 1.f) * 8.f + 4.f)); // DAT_0032B26C, DAT_0032B1B0
    }
    while (accept_runtime_trophy_head(state, id, services) == 0);
    state[0x47] = u8(id);
    do
    {
        id = std::int8_t(i32((random_unit(services.random) - 1.f) * 8.f + 14.f)); // DAT_0032B5B0
    }
    while (accept_runtime_trophy_head(state, id, services) == 0);
    state[0x48] = u8(id);
    if (random_unit(services.random) - 1.f < 0.5f) // DAT_0032B0F4
    {
        do
        {
            id = std::int8_t(i32((random_unit(services.random) - 1.f) * 21.f + 1.f)); // DAT_0032B1BC
        }
        while (accept_runtime_trophy_head(state, id, services) == 0);
        state[0x49] = u8(id);
    }
}

// Xbox 0x000820D0
u32 accept_runtime_trophy_head(std::span<const u8> state, std::int8_t id, TrophyHeadsControlServices& services)
{
    u32 slot = 0;
    do
    {
        const std::int8_t chosen = std::int8_t(state[0x46 + slot]);
        if (chosen == -1)
        {
            break;
        }
        const char* name = head_row(id, services).events;
        if (name == nullptr || chosen == id)
        {
            return 0;
        }
        const char* other = head_row(chosen, services).events;
        if (std::strcmp(other, name) == 0)
        {
            return 0;
        }
        ++slot;
    }
    while (slot < 4);
    return runtime_trophy_head_has_event(state, id, services);
}

// Xbox 0x00082440
u32 runtime_trophy_head_has_event(std::span<const u8> state, std::int8_t id, TrophyHeadsControlServices& services)
{
    std::span<const u8> events;
    if (word(state, 0x104) == kAid)
    {
        events = acquire_animation_resource(services.resources, reinterpret_cast<const char*>(state.data() + 0x104));
        if (events.data() == nullptr)
        {
            resource_error(services.error, 2);
        }
    }
    if (events.data() == nullptr)
    {
        throw Error("trophy-heads events resource at +0x104 is absent; 0x00082440 reads its +0xC");
    }
    const auto sizes = services.cutscenes.bindings.runtime.event_sizes; // DAT_00423630
    size_t at = 0;
    u16 kind = half(events, at + 0xC);
    while (true)
    {
        if (kind == 0)
        {
            release_animation_resource(services.resources, events); // 0x00035C00
            return 0;
        }
        if (kind == 0xD && half(events, at + 0x10) == u16(i32(id)))
        {
            break;
        }
        at += sizes[kind];
        kind = half(events, at + 0xC);
    }
    release_animation_resource(services.resources, events);
    return 1;
}

// Xbox 0x00082170
u32 runtime_trophy_head_chosen(std::span<const u8> state, std::int8_t id)
{
    size_t at = 0x46;
    while (true)
    {
        const std::int8_t chosen = std::int8_t(state[at]);
        if (chosen == -1)
        {
            return 0;
        }
        if (chosen == id)
        {
            break;
        }
        ++at;
        if (3 < at - 0x46)
        {
            return 0;
        }
    }
    return 1;
}

// Xbox 0x00082370
void start_runtime_trophy_head_attack(i32 count, i32 id, Actor* head, TrophyHeadsControlServices& services)
{
    const auto& row = head_row(id, services);
    char first[0x80];
    char second[0x80];
    format_dialogue_resource_name(first, "aid_cutscene_ghoulies_chapter1_trophyroom_%sattack1", row.cutscene);
    format_dialogue_resource_name(second, "aid_cutscene_ghoulies_chapter1_trophyroom_%sattack2", row.cutscene);
    const Vec3 offset{0.f, row.turn, 0.f};
    std::array<u8, 0x1D8> parameters{};
    u32 address = 0;
    if (head != nullptr)
    {
        address = services.actor_address(*head);
    }
    build_runtime_scene_challenge_parameters(parameters, first, address, 0.55f, 0.55f, nullptr, nullptr, 1,
        &offset, 1, 0, 0, second, services.runtime_flags);
    if (count < 3)
    {
        put_word(parameters, 0x148, u32(count + 1));
    }
    else
    {
        put_word(parameters, 0x148, 4);
    }
    services.create_object(parameters); // 0x00107520
}

// Xbox 0x000873A0
void build_runtime_scene_challenge_parameters(std::span<u8> parameters, const char* first, u32 actor,
    float scale_x, float scale_y, const Vec3* position, const Vec3* euler, u32 word_134, const Vec3* offset,
    u32 word_14c, u32 word_150, u32 word_1d4, const char* second, u32& runtime_flags)
{
    put_half(parameters, 0, 0x1D8);
    put_half(parameters, 2, 0x6D);
    put_half(parameters, 4, 0x98);
    put_word(parameters, 8, actor);
    copy_string(parameters, 0xC, first);
    copy_string(parameters, 0x8C, second);
    if (position == nullptr)
    {
        put_word(parameters, 0x120, 0);
    }
    else
    {
        put_vector(parameters, 0x114, *position);
        put_word(parameters, 0x120, 1);
    }
    if (euler == nullptr)
    {
        put_word(parameters, 0x130, 1);
    }
    else
    {
        put_vector(parameters, 0x124, *euler);
        put_word(parameters, 0x130, 0);
    }
    if (offset == nullptr)
    {
        put_word(parameters, 0x144, 0);
    }
    else
    {
        put_vector(parameters, 0x138, *offset);
        put_word(parameters, 0x144, 1);
    }
    put_word(parameters, 0x14C, word_14c);
    put_word(parameters, 0x134, word_134);
    put_real(parameters, 0x10C, scale_x);
    put_word(parameters, 0x1D4, word_1d4);
    put_real(parameters, 0x110, scale_y);
    put_word(parameters, 0x148, 0);
    put_word(parameters, 0x150, word_150);
    copy_string(parameters, 0x154, "aid_aidlist_ghoulies_scenecontrol_superscary"); // 0x00302058
    runtime_flags |= 0x400;
}

// Xbox 0x0004F010
std::int8_t find_runtime_cutscene_track(const SceneCutsceneRuntime& cutscenes, std::int8_t id)
{
    if (cutscenes.index < 0 || cutscenes.instances[size_t(cutscenes.index)] == nullptr)
    {
        throw Error("0x0004F010 reads the cutscene stack with no cutscene (DAT_004E1C34 < 0)");
    }
    const auto& instance = *cutscenes.instances[size_t(cutscenes.index)];
    const auto& tracks = cutscenes.bindings.runtime.views.at(instance.resource).tracks;
    std::int8_t index = 0;
    if (0 < instance.actor_count)
    {
        do
        {
            if (tracks[size_t(index)].id == id)
            {
                return index;
            }
            index = std::int8_t(u8(index) + 1);
        }
        while (i32(index) < instance.actor_count);
    }
    return -1;
}

// Xbox 0x0004E7D0
void set_runtime_cutscene_actor_animation(RuntimeCutsceneInstance& instance, std::int8_t id, const char* animation,
    const char* events, const Vec3& default_euler, CutsceneRuntimeContext& context)
{
    const auto& tracks = context.views.at(instance.resource).tracks;
    std::int8_t index = 0;
    if (!(0 < instance.actor_count))
    {
        return;
    }
    while (tracks[size_t(index)].id != id)
    {
        index = std::int8_t(u8(index) + 1);
        if (instance.actor_count <= i32(index))
        {
            return;
        }
    }
    if (index == -1)
    {
        return;
    }
    auto& row = context.instances.at(&instance).actors.at(size_t(index));
    Actor* actor = row.actor;
    if (actor == nullptr)
    {
        return;
    }
    auto& state = *context.animations.at(actor);
    if (animation != nullptr)
    {
        AnimationMix mix; // 0x000306E0
        mix.old_pose_duration = 0.f;
        mix.fade_duration = 0.f;
        mix.blend_mode = 0x23;
        mix.mode = 1;
        std::string copied;
        if (events != nullptr)
        {
            copied = events;
            mix.events = copied;
        }
        start_actor_animation_mix(state, 1, mix, animation, context.resources, context.error,
            context.random); // 0x0003ADA0, EBX the actor, EDI the mix
        if (state.controller != nullptr)
        {
            if (state.controller->slots[0].player != nullptr)
            {
                ++state.controller->slots[0].player->paused;
            }
            state.controller->dirty = true;
        }
        row.flags = 1;
        return;
    }
    if (state.controller != nullptr)
    {
        if (state.controller->slots[1].player != nullptr)
        {
            clear_animation_controller_slot(*state.controller, 1, true, context.resources); // 0x00031240
        }
        state.controller->dirty = true;
    }
    if (state.controller != nullptr)
    {
        if (state.controller->slots[0].player != nullptr)
        {
            --state.controller->slots[0].player->paused;
        }
        state.controller->dirty = true;
    }
    row.flags = 0;
    wrap_runtime_euler(default_euler, actor->euler); // 0x00105AA0, ESI DAT_0050DD74, EDI actor +0x14
    actor->xform_valid = 0;
}

// Xbox 0x0003B310
void hold_runtime_actor_animation_slot(ActorAnimationState& animation, i32 slot)
{
    if (animation.controller != nullptr)
    {
        auto& controller = *animation.controller;
        if (controller.slots[size_t(slot)].player != nullptr)
        {
            ++controller.slots[size_t(slot)].player->paused;
        }
        controller.dirty = true;
    }
}

// Xbox 0x0003D670
void measure_runtime_actor_bound_size(Actor& actor, u32 mask, GhoulySpawnPool*& shapes)
{
    const Vec3 position = actor_world_position(actor); // 0x000412A0, EDI the output
    std::array<u8, 0x20> bound{};
    build_runtime_actor_bound(actor, bound.data(), mask, false, shapes); // 0x000426F0
    const float bottom = real(bound, 8);
    const float inverse = 1.f / actor.radius_y; // DAT_0032B0F8
    const float low = (bottom - position.y) * inverse;
    const float high = ((real(bound, 0x14) + bottom) - position.y) * inverse;
    const float width = real(bound, 0x1C) / actor.radius_xz;
    actor.model_size_y = high; // +0x1C8
    actor.model_size_z = low; // +0x1CC
    actor.model_size_x = width; // +0x1C4
    float largest = high;
    if (high < low)
    {
        largest = low;
    }
    actor.model_extent = float(retail_square_root(largest * largest + width * width)); // 0x00152B30, +0x1D0
}
}
