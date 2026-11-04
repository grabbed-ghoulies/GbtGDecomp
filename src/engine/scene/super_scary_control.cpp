#include "super_scary_control.h"
#include "scene_controls.h"
#include "scene_control_fields.h"
#include "gbtg/sound_runtime.h"
#include "../camera/actor_camera_effects.h"
#include "../camera/display_view.h"
#include "../camera/entity_camera.h"
#include "../camera/view_initialization.h"
#include "../entities/actor_bind_tree.h"
#include "../entities/actor_live_attachments.h"
#include "../entities/actor_model_anchors.h"
#include "../entities/actor_spawn_eligibility.h"
#include "../entities/actor_transform.h"
#include "../entities/body_damage_motion.h"
#include "../entities/body_timers.h"
#include "../entities/entity_collision.h"
#include "../entities/player_mind_events.h"
#include "../entities/player_mind_requests.h"
#include "../entities/player_state.h"
#include "../frontend/dialogue_activation.h"
#include "../frontend/dialogue_draw_outer.h"
#include "../frontend/player_health_display.h"
#include "../frontend/scoreboard_draw.h"
#include "../scripts/script_vm.h"
#include "../runtime/name_lookup.h"
#include "../entities/body_fields.h"

#include <bit>
#include <cstring>
#include <string>

namespace gbtg
{
i32 g_super_scary_presses = 0;
u32 g_super_scary_reshuffle = 0;

namespace
{
constexpr u32 kAid = 0x5F646961u; // "aid_"

namespace parameter = super_scary_parameter;

// A byte-by-byte copy through the terminator, as 0x00084340 copies the clip names.
void copy_string(char* output, const char* text)
{
    size_t at = 0;
    char copied;
    do
    {
        copied = text[at];
        output[at] = copied;
        ++at;
    }
    while (copied != 0);
}

// Button `index` of the row (+0x214 + index * 4).
u32 button_at(const SuperScaryControlState& control, i32 index)
{
    return control.buttons[size_t(index)];
}

// The shiver voice is cleared to -1 by a two-byte 0xFF fill.
void clear_shiver_voice(SuperScaryControlState& control)
{
    control.shiver_voice = -1;
}

// Player body +0x728, cleared while Cooper is bound to the head and set again after. The body's own layout is
// not named yet.
void set_body_word_728(std::span<u8> body, u32 value)
{
    std::memcpy(body.data() + 0x728, &value, 4);
}

// Modes 2 and 3 bind Cooper to the head.
bool bound_mode(const SuperScaryControlState& control)
{
    return 1 < control.scare_mode && control.scare_mode < 4;
}

float frame_step(const SuperScaryControlServices& services)
{
    return runtime_frame_step(services.clock);
}

float random_unit(u32& random)
{
    random = random * 0x19660D + 0x3C6EF35F;
    return std::bit_cast<float>((random & 0x7FFFFF) | 0x3F800000u);
}

// An aidlist row: the 0x80-byte name at `offset` from +0x294.
const char* clip_name(const SuperScaryControlState& control, size_t offset)
{
    if (control.clips.data() == nullptr || offset + 4 > control.clips.size())
    {
        throw Error("Super Scary aidlist row lies past its resource (+0x294)");
    }
    return reinterpret_cast<const char*>(control.clips.data() + offset);
}

RuntimeCutsceneInstance& current_cutscene(SuperScaryControlServices& services)
{
    auto& cutscenes = services.cutscenes;
    if (cutscenes.index < 0 || cutscenes.instances[size_t(cutscenes.index)] == nullptr)
    {
        throw Error("Super Scary challenge reads the cutscene stack with no cutscene (DAT_004E1C34 < 0)");
    }
    return *cutscenes.instances[size_t(cutscenes.index)];
}

// 0x0004EAD0 when a clip lies under the current one, else 0x0004ECE0.
void leave_clip(SuperScaryControlServices& services)
{
    if (services.cutscenes.index < 1)
    {
        release_scene_cutscenes(services.cutscenes);
    }
    else
    {
        pop_scene_cutscene(services.cutscenes);
    }
}

// The release of +0x38 by its kind: 0x00107DD0, 0x00107960 or 0x00107B60.
void release_input(SuperScaryControlState& control, SuperScaryControlServices& services)
{
    pop_runtime_input_entry(services.input); // 0x001093C0
    switch (read_u32(control.input, 0))
    {
    case 1:
    case 2:
    case 3:
        release_runtime_simple_input(control.input);
        break;
    case 4:
        release_runtime_composite_input(control.input, services.input_release);
        break;
    case 5:
        release_runtime_actor_input(control.input, services.input_release);
        break;
    default:
        break;
    }
}

RuntimeDialogueInputDevice input_device(const SuperScaryControlState& control)
{
    return {control.input, {nullptr, nullptr, nullptr, nullptr}};
}

// The joint the camera and the shiver read: model joint `index` (0x00052FC0), or -1 without a model.
u32 player_joint(Actor& actor, i16 index)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return 0xFFFFFFFFu;
    }
    const u32 joint = actor_lookup_joint_index(actor.model->cpu, index) & 0xFF;
    if (joint == 0xFF)
    {
        return 0xFFFFFFFFu;
    }
    return joint;
}

// The animation table of 0x00086860's calls: the aidlist row and its events row (+0x1000.. or none).
struct ClipAnimation
{
    size_t animation;
    size_t events; // 0: none.
};

// Cases 1 and 3: modes 2 and 3 have their own rows with events; the rest have none.
std::optional<ClipAnimation> approach_animation(i32 mode, i32 character)
{
    if (mode == 2)
    {
        if (character == 1)
        {
            return ClipAnimation{0x180, 0x1000};
        }
        if (character == 2)
        {
            return ClipAnimation{0x900, 0x1200};
        }
        return std::nullopt;
    }
    if (mode == 3)
    {
        if (character == 1)
        {
            return ClipAnimation{0x200, 0x1000};
        }
        if (character == 2)
        {
            return ClipAnimation{0x980, 0x1200};
        }
        return std::nullopt;
    }
    if (character == 1)
    {
        return ClipAnimation{0x80, 0};
    }
    if (character == 2)
    {
        return ClipAnimation{0x800, 0};
    }
    return std::nullopt;
}

// Cases 2 and 4: mode 1 has its own rows; every row has events.
std::optional<ClipAnimation> recoil_animation(i32 mode, i32 character)
{
    if (mode == 1)
    {
        if (character == 1)
        {
            return ClipAnimation{0x100, 0x1000};
        }
        if (character == 2)
        {
            return ClipAnimation{0x880, 0x1200};
        }
        return std::nullopt;
    }
    if (mode == 2 || mode == 3)
    {
        return approach_animation(mode, character);
    }
    if (character == 1)
    {
        return ClipAnimation{0x80, 0x1000};
    }
    if (character == 2)
    {
        return ClipAnimation{0x800, 0x1200};
    }
    return std::nullopt;
}

void play_clip_animation(const SuperScaryControlState& control, u32 actor, const ClipAnimation& row, i16 mode,
    float duration, float fade, float position, SuperScaryControlServices& services)
{
    play_runtime_super_scary_animation(actor, clip_name(control, row.animation),
        row.events == 0 ? nullptr : clip_name(control, row.events), mode, duration, fade, position, services);
}

// The +0x2BC render toggles after cases 2 to 5: the head (+0x2C) leaves the bins, the player (+0x28) returns.
bool hide_head_show_player(SuperScaryControlState& control, i32 next,
    SuperScaryControlServices& services)
{
    if (control.swap_render == 0)
    {
        return false;
    }
    if (control.head_hidden == 0)
    {
        services.unregister_render(control.head);
        control.head_hidden = 1;
    }
    if (control.player_hidden != 0)
    {
        services.register_render(control.player);
        control.player_hidden = 0;
        control.phase = next;
        return true;
    }
    return false;
}
}

// Xbox 0x000842D0
void super_scary_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x2C8;
    output.parameters = 0x1D8;
}

// Xbox 0x00084340
bool construct_runtime_super_scary_control(std::span<const u8> parameters,
    SuperScaryControlState& control, SuperScaryControlServices& services)
{
    ++services.pause_count;
    std::span<const u8> clips;
    if (read_u32(parameters, parameter::clip_list.offset) == kAid)
    {
        clips = acquire_animation_resource(services.resources, field_data(parameters, parameter::clip_list));
        if (clips.data() == nullptr)
        {
            resource_error(services.error, 2);
        }
    }
    control.clips = clips;
    control.player = 0;
    g_super_scary_presses = 0;
    g_super_scary_reshuffle = 0;
    control.paused_cutscene = services.cutscenes.index > -1 ? 1 : 0;
    control.focal_jitter_timer = 0.25f; // DAT_0032B104
    control.shock_timer = 0.45f; // DAT_0032B4F8
    control.heartbeat_timer = 0.01f; // DAT_0032B124
    control.head = get(parameters, parameter::head);
    control.input_pushed = 0;
    control.phase = 0;
    control.button_count = services.button_count;
    control.presses = 0;
    control.bat = 0;
    control.heartbeats_left = 8;
    control.wrong_press_timer = 0.f;
    control.unk_B4 = 0.f;
    control.move_player = get(parameters, parameter::move_player);
    control.first_clip_timer = get(parameters, parameter::first_clip_time);
    control.second_clip_timer = get(parameters, parameter::second_clip_time);
    control.scare_mode = get(parameters, parameter::scare_mode);
    control.player_released = 0;
    control.player_hidden = 0;
    control.head_hidden = 0;
    control.swap_render = get(parameters, parameter::swap_render);
    control.shock_camera_position = Vec3{0.f, 0.f, 0.f};
    control.shock_camera_roll = 0.f;
    control.shock_camera_yaw = 0.f;
    control.shock_camera_pitch = 0.f;
    const u32 mode = control.scare_mode;
    if (mode == 2 || mode == 3)
    {
        if (control.head == 0)
        {
            control.scare_mode = mode == 2 ? 0 : 1;
        }
        else
        {
            const auto head = services.actor_view(control.head);
            control.head_binding = find_actor_model_binding(head.actor, 2); // 0x000408D0, EDI 2
        }
    }
    copy_string(control.first_clip.data(), field_data(parameters, parameter::first_clip));
    copy_string(control.second_clip.data(), field_data(parameters, parameter::second_clip));
    if (get(parameters, parameter::has_clip_position) == 0)
    {
        control.has_clip_position = 0;
    }
    else
    {
        control.clip_position = get(parameters, parameter::clip_position);
        control.has_clip_position = 1;
    }
    if (get(parameters, parameter::spawn_under_view) != 0)
    {
        control.spawn_under_view = 1;
    }
    else
    {
        control.player_spawn = get(parameters, parameter::player_spawn);
        control.spawn_under_view = 0;
    }
    if (get(parameters, parameter::has_clip_euler) == 0)
    {
        control.has_clip_euler = 0;
    }
    else
    {
        control.clip_euler = get(parameters, parameter::clip_euler);
        control.has_clip_euler = 1;
    }
    choose_runtime_super_scary_buttons(get(parameters, parameter::button_pattern), control, services);
    initialize_camera_view(control.camera, "superscaryshock", 1); // 0x000496C0, EAX 0x00324238, CL 1
    link_camera_view(0, control.camera); // The 0x00048C20 link, inlined.
    control.camera_linked = 1;
    for (size_t slot = 0; slot < 5; ++slot)
    {
        const size_t row = 0x1880 + slot * 0x80;
        std::span<const u8> texture;
        if (read_u32(control.clips, row) == kAid)
        {
            texture = acquire_animation_resource(services.resources, clip_name(control, row));
            if (texture.data() == nullptr)
            {
                resource_error(services.error, 2);
            }
        }
        control.button_textures[slot] = texture;
    }
    control.music = 0;
    control.unk_290 = get(parameters, parameter::unk_14C);
    clear_shiver_voice(control);
    if (services.scene_name_text != nullptr)
    {
        // DAT_00512484: its first word's +8 goes back to its +4.
        throw Error("Super Scary construction resets the DAT_00512484 text pool; its native view is not bound");
    }
    return true;
}

// Xbox 0x000846E0
void destroy_runtime_super_scary_control(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    --services.pause_count;
    if (control.swap_render != 0)
    {
        if (control.player_hidden != 0)
        {
            services.register_render(control.player);
            control.player_hidden = 0;
        }
        if (control.head_hidden != 0)
        {
            services.register_render(control.head);
            control.head_hidden = 0;
        }
    }
    const u32 music = control.music;
    if (music != 0)
    {
        if (music == 1)
        {
            release_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONPRESS", services.streams);
        }
        else if (music == 2)
        {
            release_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_SUCCEED", services.streams);
        }
        else if (music == 3)
        {
            release_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONNODIE", services.streams);
        }
        control.music = 0;
    }
    for (auto& texture : control.button_textures)
    {
        release_animation_resource(services.resources, texture); // 0x00035C00, EBX the texture
    }
    if (control.player_released == 0)
    {
        std::array<u32, 1> message{10};
        services.actor_message(control.player, message);
        control.player_released = 1;
    }
    if (control.camera_linked != 0)
    {
        camera_unlink_view(&control.camera); // 0x00048C70
        control.camera_linked = 0;
    }
    if (control.input_pushed != 0)
    {
        release_input(control, services);
        control.input_pushed = 0;
    }
    const i16 voice = control.shiver_voice;
    if (voice != -1)
    {
        auto& record = services.voices.voices.at(size_t(voice));
        if (record.cue == nullptr)
        {
            record.state = 10;
        }
        else
        {
            switch (record.state)
            {
            case 2:
            case 3:
                record.state = 4;
                break;
            case 5:
            case 6:
            case 7:
                record.state = 9;
                stop_runtime_sound_cue(record.cue->runtime, 0, services.voices.core.stopped_count,
                    services.voices.core.sound); // 0x0015CE30
                break;
            default:
                break;
            }
        }
        clear_shiver_voice(control);
    }
    if (control.clips.data() != nullptr)
    {
        release_animation_resource(services.resources, control.clips);
        control.clips = {};
    }
    if (control.bat != 0)
    {
        services.retire_actor(control.bat); // 0x00112D10, message 0x80 and 0x0003ED70, inlined 0x00112CC0
        control.bat = 0;
    }
    services.runtime_flags &= 0xFFFFFBFFu;
}

// Xbox 0x00084930
void message_runtime_super_scary_control(std::span<const u32> message,
    SuperScaryControlState& control, SceneControlInstance& instance, SceneControlLists& lists)
{
    if (message[0] == 0x7C && control.player == message[1])
    {
        control.player = 0;
    }
    if (message[0] == 0x80)
    {
        retire_runtime_scene_control(instance, lists); // 0x00115E70
    }
}

// Xbox 0x00084960
void tick_runtime_super_scary_control(SceneControlBase& base, SuperScaryControlState& control,
    SceneControlInstance& instance, SceneControlLists& lists, SuperScaryControlServices& services)
{
    base.elapsed = base.elapsed + frame_step(services);
    if (g_super_scary_reshuffle != 0)
    {
        const i32 row = i32((random_unit(services.random) - 1.f) * 4.f); // DAT_0032B1B0
        std::memcpy(control.buttons.data(), services.button_rows.data() + size_t(row) * 0x50, 0x50);
        g_super_scary_reshuffle = 0;
    }
    switch (control.phase)
    {
    case 0:
        set_runtime_super_scary_state(1, control, services);
        break;
    case 1:
    {
        float timer = control.first_clip_timer;
        const bool expired = tick_runtime_body_frame_timer(timer, services.clock); // 0x0012B2A0, EAX +0x1B8
        control.first_clip_timer = timer;
        if (expired)
        {
            set_runtime_super_scary_state(2, control, services);
        }
        tick_runtime_super_scary_shocks(control, services);
        break;
    }
    case 2:
        if (runtime_actor_animation_slot_complete(services.actor_view(control.player).animation, 0)) // 0x0003B290
        {
            set_runtime_super_scary_state(3, control, services);
        }
        tick_runtime_super_scary_shocks(control, services);
        break;
    case 3:
    {
        float timer = control.second_clip_timer;
        const bool expired = tick_runtime_body_frame_timer(timer, services.clock);
        control.second_clip_timer = timer;
        if (expired)
        {
            set_runtime_super_scary_state(4, control, services);
        }
        tick_runtime_super_scary_shocks(control, services);
        break;
    }
    case 4:
        if (runtime_actor_animation_slot_complete(services.actor_view(control.player).animation, 0))
        {
            set_runtime_super_scary_state(5, control, services);
        }
        tick_runtime_super_scary_shocks(control, services);
        break;
    case 5:
    case 6:
    {
        refresh_runtime_input_device(control.input_address, services.frame, services.input,
            services.input_release); // 0x00109470, EAX +0x38
        if (0.f < control.wrong_press_timer)
        {
            control.wrong_press_timer = control.wrong_press_timer - frame_step(services); // 0x0012B270
        }
        if (!(0.f < control.wrong_press_timer))
        {
            if (runtime_super_scary_right_button(control, services) != 0)
            {
                const i32 presses = control.presses + 1;
                control.presses = presses;
                if ((services.runtime_flags & 0x1000000) == 0)
                {
                    g_super_scary_presses = presses;
                }
                request_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON", 1, services.voices); // 0x000395E0
                if (control.presses == control.button_count)
                {
                    set_runtime_super_scary_state(8, control, services);
                    return;
                }
                std::array<float, 2> position{};
                runtime_super_scary_button_position(position, control, control.presses, services);
                auto bat = services.bat_view(control.bat);
                fly_runtime_button_bat(bat.bat, bat.animation, ButtonBatPoint{position[0], position[1]}, 1.5f,
                    services.resources, services.error, services.random); // 0x0006DA00, ESI +0x30, EBX the point
            }
            else if (runtime_super_scary_wrong_button(control, services) != 0)
            {
                request_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_WRONGBUTTON", 1, services.voices);
                control.wrong_press_timer = 1.f; // DAT_0032B0F8
                auto bat = services.bat_view(control.bat);
                wobble_runtime_button_bat(bat.bat, bat.animation, services); // 0x0006DAD0, ESI +0x30
            }
        }
        float heartbeat = control.heartbeat_timer;
        const bool beat = tick_runtime_body_frame_timer(heartbeat, services.clock); // 0x0012B2A0, EAX +0xA8
        control.heartbeat_timer = heartbeat;
        if (!beat)
        {
            const float calm =
                1.f - (float(control.heartbeats_left) - 8.f) * -0.125f; // DAT_0032B26C, DAT_0032B6EC
            float shake;
            if (0.f > calm)
            {
                shake = 0.f;
            }
            else
            {
                shake = 1.f;
                if (!(calm > 1.f))
                {
                    shake = calm;
                }
            }
            place_runtime_super_scary_camera(shake, control, services);
            break;
        }
        const i32 beats = control.heartbeats_left - 1;
        control.heartbeats_left = beats;
        if (beats < 0)
        {
            if (std::int8_t(services.runtime_flags >> 8) < 0)
            {
                set_runtime_super_scary_state(8, control, services);
                return;
            }
            set_runtime_super_scary_state(10, control, services);
            return;
        }
        float calm = 1.f - (float(beats) - 8.f) * -0.125f;
        if (0.f > calm)
        {
            calm = 0.f;
        }
        else if (calm > 1.f)
        {
            calm = 1.f;
        }
        const float eased = float(ease_runtime_hud(float(ease_runtime_hud(float(ease_runtime_hud(
            float(ease_runtime_hud(calm)))))))); // 0x00103850 four times, XMM0
        control.focal_jitter_timer = 0.25f; // DAT_0032B104
        control.heartbeat_timer = eased * 0.7f + 0.3f; // DAT_0032B264, DAT_0032B268
        if (control.heartbeats_left < 4)
        {
            request_spatial_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_HEARTBEATFAST", 1, 0.f,
                services.voices); // 0x00039510
        }
        else
        {
            request_spatial_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_HEARTBEAT", 1, 0.f, services.voices);
        }
        shake_runtime_super_scary_camera(control, services);
        place_runtime_super_scary_camera(calm, control, services);
        break;
    }
    case 7:
        if (services.prompt_owner == 0)
        {
            finish_runtime_super_scary(7, control, services);
        }
        break;
    case 8:
        if (!scene_cutscene_finished(services.cutscenes)) // 0x0004EFE0
        {
            settle_runtime_super_scary_player(control, services);
            break;
        }
        if (test_runtime_story_bit(services.story_bits, 5)) // 0x000F29A0, ECX 5
        {
            finish_runtime_super_scary(7, control, services);
            break;
        }
        set_runtime_super_scary_state(7, control, services);
        break;
    case 9:
        if (services.prompt_owner == 0)
        {
            finish_runtime_super_scary(8, control, services);
        }
        break;
    case 10:
        if (services.scene_end_flag()
            && transition_aid_equal("aid_script_ghoulies_chapter1_scene2_1viewcam", services.scene_name)) // 0x000357D0
        {
            break;
        }
        if (!scene_cutscene_finished(services.cutscenes))
        {
            settle_runtime_super_scary_player(control, services);
            break;
        }
        if (test_runtime_story_bit(services.story_bits, 5))
        {
            finish_runtime_super_scary(8, control, services);
            break;
        }
        set_runtime_super_scary_state(9, control, services);
        break;
    case 11:
    {
        float timer = control.shock_timer;
        const bool expired = tick_runtime_body_frame_timer(timer, services.clock);
        control.shock_timer = timer;
        if (expired)
        {
            dispose_runtime_scene_control(instance, lists); // 0x00116140, EAX the control
        }
        break;
    }
    default:
        break;
    }
    if (control.player != 0)
    {
        const std::span<u8> body = services.player_body(control.player);
        set_runtime_player_health_display(services.health, i32(get(body, body_field::energy)), 0); // 0x00130710, EDI
        services.health_intensity = 2.f; // DAT_0032B110
        acquire_runtime_player_health_textures(services.health); // 0x0012FE10
    }
}

// Xbox 0x000856E0
void set_runtime_super_scary_state(i32 next, SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    const i32 mode = control.scare_mode;
    switch (next)
    {
    case 1:
    {
        services.pause_actors(0xFF, 0); // 0x00117400(0xFFFF, 0)
        if (control.paused_cutscene != 0)
        {
            auto& runtime = services.cutscenes.bindings.runtime;
            auto& instance = current_cutscene(services);
            ++instance.pending[2]; // +0x158
            auto& rows = runtime.instances.at(&instance).actors;
            std::int8_t index = 0;
            if (0 < instance.actor_count)
            {
                do
                {
                    auto& row = rows.at(size_t(index));
                    if (Actor* actor = row.actor; actor != nullptr)
                    {
                        auto& animation = *runtime.animations.at(actor);
                        if (row.flags != 0 && animation.controller != nullptr)
                        {
                            if (animation.controller->slots[1].player != nullptr)
                            {
                                ++animation.controller->slots[1].player->paused;
                            }
                            animation.controller->dirty = true;
                        }
                        if (animation.controller != nullptr)
                        {
                            if (animation.controller->slots[0].player != nullptr)
                            {
                                ++animation.controller->slots[0].player->paused;
                            }
                            animation.controller->dirty = true;
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
                    const i16 voice_index = i16(read_u16(instance.event_state, slot * 2));
                    if (voice_index == -1)
                    {
                        continue;
                    }
                    auto& voice = services.voices.voices.at(size_t(voice_index));
                    if (voice.cue == nullptr)
                    {
                        continue;
                    }
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
        announce_runtime_super_scary_step(0, services);
        std::array<float, 2> position{};
        runtime_super_scary_button_position(position, control, control.presses, services);
        control.bat = services.create_bat(position, 1.5f, -1.f); // 0x0013B580, EAX the position
        if (control.spawn_under_view != 0)
        {
            Vec3 euler;
            aim_runtime_super_scary_camera(control, euler, services); // ESI +0x1C8, ECX a local
        }
        control.player = spawn_runtime_super_scary_player(control.player_spawn, control, services);
        control.input = create_runtime_selected_input(services.input_release); // 0x00107D60, ESI +0x38
        control.input_address = u32(reinterpret_cast<std::uintptr_t>(control.input.data()));
        services.input_addresses[control.input_address] = control.input;
        services.input_views[control.input_address] = input_device(control);
        push_runtime_input_entry(control.input_address, "superscaryshock", services.input); // 0x00109340
        control.input_pushed = 1;
        runtime_actor_angle_toward(services.actor_view(control.player).actor,
            services.actor_view(control.head).actor); // 0x00041900; the angle is not used
        if (const Actor* attached = services.attached_actor(control.player);
            attached != nullptr && actor_spawn_owner_unavailable(*attached)) // 0x001460C0, ECX body +0x48
        {
            if (control.scare_mode == 0)
            {
                control.scare_mode = 1;
            }
            else if (control.scare_mode == 2)
            {
                control.scare_mode = 3;
            }
        }
        const Vec3 offset = control.clip_euler;
        const Vec3 place = control.clip_position;
        const float duration = start_runtime_super_scary_clip(
            control.has_clip_position != 0 ? &place : nullptr,
            control.has_clip_euler != 0 ? &offset : nullptr, control.head,
            control.first_clip.data(), services);
        if (const auto row = approach_animation(control.scare_mode, services.player_character()); row)
        {
            play_clip_animation(control, control.player, *row, 1, 0.55f, 0.f, -1.f, services);
        }
        if (!(0.f < control.first_clip_timer))
        {
            control.first_clip_timer = duration;
        }
        control.shock_timer = 0.45f; // DAT_0032B4F8
        control.shocks_left = 3;
        if (bound_mode(control))
        {
            const auto view = services.actor_view(control.player);
            control.bound_position = actor_world_position(view.actor); // 0x000412A0, EDI +0x27C
            services.bind_to_head(control.player, control.head, control.head_binding); // 0x0003B500, ECX player
            set_body_word_728(services.player_body(control.player), 0);
            if (view.secondary_model)
            {
                view.secondary_mode = 0;
            }
        }
        if (control.swap_render != 0 && control.player_hidden == 0)
        {
            services.unregister_render(control.player);
            control.player_hidden = 1;
            control.phase = next;
            return;
        }
        break;
    }
    case 2:
    case 4:
    {
        leave_clip(services);
        announce_runtime_super_scary_step(next == 2 ? 1 : 3, services);
        if (const auto row = recoil_animation(mode, services.player_character()); row)
        {
            play_clip_animation(control, control.player, *row, 1, 0.55f, 0.f, -1.f, services);
        }
        const bool bound = bound_mode(control);
        const size_t clip = next == 2 ? (bound ? 0x1500 : 0x1480) : (bound ? 0x1600 : 0x1580);
        start_runtime_super_scary_clip(nullptr, nullptr, control.player, clip_name(control, clip), services);
        if (hide_head_show_player(control, next, services))
        {
            return;
        }
        break;
    }
    case 3:
    {
        leave_clip(services);
        announce_runtime_super_scary_step(2, services);
        if (const auto row = approach_animation(mode, services.player_character()); row)
        {
            play_clip_animation(control, control.player, *row, 1, 0.55f, 0.f, -1.f, services);
        }
        const Vec3 offset = control.clip_euler;
        const Vec3 place = control.clip_position;
        const float duration = start_runtime_super_scary_clip(
            control.has_clip_position != 0 ? &place : nullptr,
            control.has_clip_euler != 0 ? &offset : nullptr, control.head,
            control.second_clip.data(), services);
        if (!(0.f < control.second_clip_timer))
        {
            control.second_clip_timer = duration;
        }
        if (control.swap_render != 0)
        {
            if (control.player_hidden == 0)
            {
                services.unregister_render(control.player);
                control.player_hidden = 1;
            }
            if (control.head_hidden != 0)
            {
                services.register_render(control.head);
                control.head_hidden = 0;
                control.phase = next;
                return;
            }
        }
        break;
    }
    case 5:
    {
        const auto& view = *services.display_view;
        control.shock_camera_position = view.position;
        auto player = services.actor_view(control.player);
        actor_refresh_transform(player.actor); // 0x00040DE0
        const u32 joint = player_joint(player.actor, 0); // 0x00052FC0(0)
        Vec3 point;
        actor_transform_joint_point(player.actor, u16(joint), actor_transform_origin, point); // 0x0003AEB0, EBX DAT_0050DD80
        float yaw = 0.f;
        float pitch = 0.f;
        const Vec3 camera = control.shock_camera_position;
        direction_to_yaw_pitch(point.x - camera.x, point.y - camera.y, point.z - camera.z, yaw, pitch); // 0x00105E20
        control.shock_camera_pitch = 0.f - pitch;
        control.shock_camera_yaw = yaw + 3.14159274f; // DAT_0032B148
        control.shock_camera_roll = 0.f;
        leave_clip(services);
        camera_bind_named_view(0, "superscaryshock"); // 0x00048CF0, EBX 0x00324238
        place_runtime_super_scary_camera(1.f, control, services);
        announce_runtime_super_scary_step(4, services);
        control.auto_press_timer = (random_unit(services.random) - 1.f) * 0.2f + 0.2f; // DAT_0032B1AC
        const i32 character = services.player_character();
        std::optional<ClipAnimation> row;
        const auto pick = [character](ClipAnimation first, ClipAnimation second) -> std::optional<ClipAnimation>
        {
            if (character == 1)
            {
                return first;
            }
            if (character == 2)
            {
                return second;
            }
            return std::nullopt;
        };
        switch (mode)
        {
        case 1:
            row = pick({0x300, 0x1080}, {0xA80, 0x1280});
            break;
        case 2:
            row = pick({0x380, 0x1080}, {0xB00, 0x1280});
            break;
        case 3:
            row = pick({0x400, 0x1080}, {0xB80, 0x1280});
            break;
        default:
            row = pick({0x280, 0x1080}, {0xA00, 0x1280});
            break;
        }
        if (row)
        {
            play_clip_animation(control, control.player, *row, 0, -1.f, 0.2f, -1.f, services);
        }
        const i16 voice = request_spatial_audio_event_voice("XACT_SOUNDBANK_GBOY_SHIVER", 0, 0.f,
            services.voices); // 0x00039510
        control.shiver_voice = voice;
        set_runtime_super_scary_music(control, 1, services);
        if (hide_head_show_player(control, next, services))
        {
            return;
        }
        break;
    }
    case 6:
        break;
    case 7:
    case 9:
        services.story_bits[0] |= 0x20;
        set_runtime_super_scary_music(control, 0, services);
        if ((services.runtime_flags & 0x4000) == 0)
        {
            enqueue_runtime_dialogue_key(next == 7 ? "chapter1_scene2_shock1_success"
                : "chapter1_scene2_shock1_failure", 0, services.dialogue); // 0x000582A0, ECX 0x00302158 / 0x00302178
            control.phase = next;
            return;
        }
        break;
    case 8:
    {
        end_runtime_super_scary_challenge(1, control, services);
        const i32 character = services.player_character();
        const auto pick = [character](ClipAnimation first, ClipAnimation second) -> std::optional<ClipAnimation>
        {
            if (character == 1)
            {
                return first;
            }
            if (character == 2)
            {
                return second;
            }
            return std::nullopt;
        };
        bool head = false;
        std::optional<ClipAnimation> row;
        switch (control.scare_mode)
        {
        case 1:
            row = pick({0x680, 0x1180}, {0xE00, 0x1380});
            break;
        case 2:
            row = pick({0x700, 0x1180}, {0xE80, 0x1380});
            head = true;
            break;
        case 3:
            row = pick({0x780, 0x1180}, {0xF00, 0x1380});
            head = true;
            break;
        default:
            row = pick({0x600, 0x1180}, {0xD80, 0x1380});
            break;
        }
        if (row)
        {
            play_clip_animation(control, control.player, *row, 1, -1.f, 0.f, -1.f, services);
        }
        if (head)
        {
            play_clip_animation(control, control.head, ClipAnimation{0xF80, 0x1400}, 1, -1.f, 0.f, -1.f, services);
        }
        const bool bound = bound_mode(control);
        start_runtime_super_scary_clip(nullptr, nullptr, control.player, clip_name(control, bound ? 0x1800 : 0x1780),
            services);
        set_runtime_super_scary_music(control, 2, services);
        control.phase = next;
        return;
    }
    case 10:
    {
        end_runtime_super_scary_challenge(0, control, services);
        if (bound_mode(control))
        {
            start_runtime_super_scary_clip(nullptr, nullptr, control.player, clip_name(control, 0x1700), services);
            play_clip_animation(control, control.head, ClipAnimation{0xF80, 0x1400}, 1, -1.f, 0.f, -1.f, services);
        }
        else
        {
            start_runtime_super_scary_clip(nullptr, nullptr, control.player, clip_name(control, 0x1680), services);
        }
        const u32 music = control.music;
        if (music != 3)
        {
            if (music == 1)
            {
                release_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONPRESS", services.streams); // 0x00038970
            }
            else if (music == 2)
            {
                release_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_SUCCEED", services.streams);
            }
            acquire_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONNODIE", 7, services.streams); // 0x00038910
            control.music = 3;
            control.phase = next;
            return;
        }
        break;
    }
    case 11:
        control.shock_timer = 1.f; // DAT_0032B0F8
        if (control.swap_render != 0)
        {
            if (control.head_hidden != 0)
            {
                services.register_render(control.head);
                control.head_hidden = 0;
            }
            if (control.player_hidden != 0)
            {
                services.register_render(control.player);
                control.player_hidden = 0;
            }
        }
        break;
    default:
        break;
    }
    control.phase = next;
}

// Xbox 0x00086BC0
void finish_runtime_super_scary(i32 outcome, SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    leave_clip(services);
    services.runtime_flags &= 0xFFFFFBFFu;
    services.pause_actors(0xFF, 1); // 0x00117400(0xFFFF, 1)
    services.pause_actors(0xFF, 1);
    if (control.camera_linked != 0)
    {
        camera_unlink_view(&control.camera); // 0x00048C70, EAX +0x40
        control.camera_linked = 0;
    }
    if (control.input_pushed != 0)
    {
        release_input(control, services);
        control.input_pushed = 0;
    }
    set_runtime_super_scary_state(11, control, services);
    announce_runtime_super_scary_step(u32(outcome), services); // The tail jump, EAX the outcome.
}

// Xbox 0x000874F0
void end_runtime_super_scary_challenge(i32 succeeded, SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    if (control.player_released == 0)
    {
        std::array<u32, 1> message{10};
        services.actor_message(control.player, message);
        control.player_released = 1;
    }
    u32 step = 6;
    if (control.input_pushed != 0)
    {
        release_input(control, services);
        control.input_pushed = 0;
        step = 5;
    }
    announce_runtime_super_scary_step(step, services);
    const i16 voice = control.shiver_voice;
    if (voice != -1)
    {
        stop_audio_event_voice(voice, 0, services.voices); // 0x00039690, EAX the voice
        clear_shiver_voice(control);
    }
    if (control.bat != 0)
    {
        services.retire_actor(control.bat); // 0x00112CC0, ESI +0x30
        control.bat = 0;
    }
    if (bound_mode(control))
    {
        auto player = services.actor_view(control.player);
        detach_actor_bind(player.actor); // 0x0003B560, EBX the player
        set_body_word_728(services.player_body(control.player), 1);
        if (player.secondary_model)
        {
            player.secondary_mode = 1;
        }
        const float height = services.floor_height(control.bound_position); // 0x00110BF0, 0x00110EF0
        control.settle_floor_y = height;
        control.settle_start_y = height + 20.f; // DAT_0032B234
        control.settle_timer = 0.5f; // DAT_0032B0F4
        control.bound_position.y = height + 20.f;
        actor_set_world_position(player.actor, control.bound_position); // 0x00041730
        services.refresh_body_position(control.player); // Body +0x18 from 0x000412A0 on the body's actor (+8).
    }
    std::array<u32, 5> message{0x1C, 0, 0, u32(control.scare_mode), u32(succeeded)};
    if (succeeded == 0 && services.override_health == 0)
    {
        message[2] = 10;
    }
    services.message_players(message); // 0x00122D10, EDI the message
}

// Xbox 0x00087170
void choose_runtime_super_scary_buttons(i32 count, SuperScaryControlState& control, SuperScaryControlServices& services)
{
    if (services.frontend_state == 3 && (services.scene_flags & 8) == 0 && (services.runtime_flags & 0x200000) == 0)
    {
        count = 5;
    }
    else if (u32(count) > 5)
    {
        return;
    }
    const std::span<u32> buttons = control.buttons;
    u32* chosen = control.buttons.data();
    switch (count)
    {
    case 0:
        for (i32 index = 0; index < control.button_count; ++index)
        {
            chosen[size_t(index)] = u32(i32((random_unit(services.random) - 1.f) * 4.f)); // DAT_0032B1B0
        }
        return;
    case 1:
        chosen[0] = 0;
        chosen[1] = 0;
        chosen[2] = 0;
        chosen[3] = 0;
        return;
    case 2:
        chosen[0] = 0;
        chosen[1] = 0;
        chosen[2] = 1;
        chosen[3] = 1;
        shuffle_runtime_super_scary_buttons(buttons, 4, services.random);
        return;
    case 3:
        chosen[0] = 0;
        chosen[1] = 1;
        chosen[2] = 2;
        chosen[3] = u32(i32((random_unit(services.random) - 1.f) * 3.f)); // DAT_0032B260
        shuffle_runtime_super_scary_buttons(buttons, 4, services.random);
        return;
    case 4:
        chosen[0] = 0;
        chosen[1] = 1;
        chosen[2] = 2;
        chosen[3] = 3;
        shuffle_runtime_super_scary_buttons(buttons, 4, services.random);
        return;
    default:
    {
        const i32 row = i32((random_unit(services.random) - 1.f) * 4.f);
        std::memcpy(buttons.data(), services.button_rows.data() + size_t(row) * 0x50, 0x50);
        return;
    }
    }
}

// Xbox 0x00087080
void shuffle_runtime_super_scary_buttons(std::span<u32> buttons, i32 count, u32& random)
{
    i32 swaps = i32((random_unit(random) - 1.f) * 901.f + 100.f); // DAT_0032B2C0, DAT_0032B12C
    if (swaps < 0)
    {
        return;
    }
    ++swaps;
    const float scale = float(count);
    do
    {
        const i32 first = i32((random_unit(random) - 1.f) * scale);
        const i32 second = i32((random_unit(random) - 1.f) * scale);
        --swaps;
        const u32 kept = buttons[size_t(first)];
        buttons[size_t(first)] = buttons[size_t(second)];
        buttons[size_t(second)] = kept;
    }
    while (swaps != 0);
}

// Xbox 0x00086C90
void announce_runtime_super_scary_step(u32 step, SuperScaryControlServices& services)
{
    const std::array<u32, 2> message{0x8E, step};
    if (g_scene_control_owner != nullptr)
    {
        auto& owner = *g_scene_control_owner;
        RuntimeActorLink* node = owner.context.lists.active.next; // DAT_0050E438
        while (node != nullptr)
        {
            RuntimeActorLink* next = node->next;
            owner.message(owner.instance_of(*node), message);
            node = next;
        }
    }
    services.broadcast_pulse(message); // 0x00122C70, EDI the message
}

// Xbox 0x000854E0
float start_runtime_super_scary_clip(const Vec3* position, const Vec3* euler, u32 actor, const char* name,
    SuperScaryControlServices& services)
{
    auto view = services.actor_view(actor);
    const Vec3 place = position != nullptr ? *position : actor_world_position(view.actor); // 0x000412A0, ESI EDX
    const Vec3 turn = euler != nullptr ? *euler : actor_world_euler(view.actor); // 0x00041820
    const Vec3 scale{view.actor.scale_x, view.actor.radius_y, view.actor.radius_xz}; // +0x30..+0x38
    if (services.cutscenes.index > -1)
    {
        push_scene_cutscene(name, 1, services.cutscenes); // 0x0004EB20
    }
    else
    {
        start_scene_cutscene(name, 0, 1, services.cutscenes); // 0x0004EDA0
    }
    auto& runtime = services.cutscenes.bindings.runtime;
    const auto update = [&]()
    {
        auto& clip = current_cutscene(services);
        update_runtime_cutscene_actors(clip, -1.f, clip.time / clip.duration, runtime); // 0x0004E1E0
        auto& viewed = current_cutscene(services);
        update_cutscene_camera_view(viewed, runtime.views.at(viewed.resource).cameras.data(),
            services.cutscenes.camera); // 0x0004D7D0, DAT_004E1BC4
    };
    current_cutscene(services).position = place;
    update();
    current_cutscene(services).euler = turn;
    update();
    current_cutscene(services).scale = scale;
    update();
    camera_bind_named_view(0, "cutscene"); // 0x00048CF0, EBX 0x00322DEC
    return current_cutscene(services).duration;
}

// Xbox 0x00086860
void play_runtime_super_scary_animation(u32 actor, const char* animation, const char* events, i16 mode,
    float duration, float fade, float position, SuperScaryControlServices& services)
{
    AnimationMix mix; // The 0x000306E0 defaults, inlined.
    mix.duration = 0.f;
    mix.position = 0.f;
    mix.fade_duration = 0.f;
    mix.blend_mode = 0x23;
    mix.mode = mode;
    if (!(fade < 0.f))
    {
        mix.old_pose_duration = fade;
    }
    if (0.f < duration)
    {
        mix.duration = duration;
    }
    if (0.f < position)
    {
        mix.position = position;
    }
    std::string copied;
    if (events != nullptr)
    {
        copied = events;
        mix.events = copied;
    }
    auto view = services.actor_view(actor);
    auto& state = view.animation;
    if (state.controller == nullptr)
    {
        state.controller = make_animation_controller(4); // 0x00030740, ESI 4
        state.controller->event_callback = [](std::span<const u8> event, u32 index, float at, void* owner,
                                               int phase)
        {
            animation_actor_event(event, index, at, *static_cast<ActorAnimationState*>(owner), phase); // 0x0003B3C0
        };
        state.controller->event_owner = &state;
    }
    start_controller_animation(*state.controller, 0, {}, animation, mix, services.resources, services.error,
        services.random); // 0x00030880
}

// Xbox 0x00084E50
void place_runtime_super_scary_camera(float shake, SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    const float distance = shake * 8.f + 4.5f; // DAT_0032B26C, DAT_0032B3B0
    const float height = shake * 3.f + 2.f; // DAT_0032B260, DAT_0032B110
    Vec3 eye{};
    Vec3 turn{};
    float focal = shake;
    switch (control.scare_mode)
    {
    case 0:
    case 1:
    {
        auto player = services.actor_view(control.player);
        actor_refresh_transform(player.actor); // 0x00040DE0
        const u32 joint = player_joint(player.actor, 4); // 0x00052FC0(4)
        Vec3 point;
        actor_transform_joint_point(player.actor, u16(joint), actor_transform_origin, point); // 0x0003AEB0
        const float facing = player.actor.euler.y; // Actor +0x18
        eye = Vec3{point.x, point.y + height, point.z};
        add_sound_direction(eye, 0.f, facing, distance); // 0x00106220, ESI the point
        float yaw = 0.f;
        float pitch = 0.f;
        direction_to_yaw_pitch(point.x - eye.x, point.y - eye.y, point.z - eye.z, yaw, pitch); // 0x00105E20
        focal = shake * 8.70466614f + 14.3500004f; // DAT_0032B560, DAT_0032B55C
        turn = Vec3{0.f - pitch, yaw + 3.14159274f, 0.f}; // DAT_0032B148
        break;
    }
    case 2:
    case 3:
        eye = control.shock_camera_position;
        turn = Vec3{control.shock_camera_pitch, control.shock_camera_yaw,
            control.shock_camera_roll};
        focal = 10.f - shake * 4.f; // DAT_0032B10C, DAT_0032B1B0
        break;
    default:
        break;
    }
    if (0.f < control.focal_jitter_timer)
    {
        control.focal_jitter_timer = control.focal_jitter_timer - frame_step(services);
        const float noise = random_unit(services.random);
        const float fall = 1.f - (control.focal_jitter_timer - 0.25f) * -4.f; // DAT_0032B104, DAT_0032B2A4
        float weight = 0.f;
        if (!(0.f > fall))
        {
            weight = 1.f;
            if (!(fall > 1.f))
            {
                weight = fall;
            }
        }
        focal = ((noise - 1.f) * 2.f - 1.f) * weight + focal; // DAT_0032B110
    }
    control.camera.position = eye; // +0x48
    control.camera.rotation.x = turn.x; // +0x54
    control.camera.rotation.y = turn.y; // +0x58
    control.camera.rotation.z = turn.z; // +0x5C
    control.camera.focal = focal; // +0x64
    i32 fit_width;
    i32 fit_x;
    i32 fit_height;
    i32 fit_y;
    float fov;
    camera_fit_display_view(focal, control.camera.aspect_slot, fit_width, fit_x, fit_height, fit_y, fov,
        control.camera.pixel_aspect, control.camera.focal_pixels); // 0x000F57F0, EDI +0x70, +0x98, +0x9C
}

// Xbox 0x00086CE0
void aim_runtime_super_scary_camera(SuperScaryControlState& control, Vec3& euler, SuperScaryControlServices& services)
{
    const auto& view = *services.display_view;
    Vec3 place = view.position;
    const float x = view.rotation.x;
    const float y = view.rotation.y;
    const float z = view.rotation.z;
    const float w = view.rotation.w;
    const float a = (w * 0.f - x) + z * 0.f;
    const float b = (w * 0.f - z * 0.f) + y;
    const float c = (w - y * 0.f) + x * 0.f; // DAT_0032B100
    const float d = (z + x * 0.f) + y * 0.f;
    const Vec3 forward{((w * b + d * x) + c * y) - a * z, ((w * a + d * y) + z * b) - c * x,
        ((w * c + d * z) + a * x) - b * y};
    euler.x = 0.f;
    euler.z = 0.f;
    runtime_direction_angle(forward, euler.y); // 0x00105F90, EAX the direction, EBX ECX +4
    euler.y += 3.14159274f; // DAT_0032B148
    Vec3 start = place;
    place.y -= 50.f; // DAT_0032B1E0
    entity_record_sweep_probe(place, start, 8, 0); // 0x00122480, EBX the point
    control.player_spawn = place;
}

// Xbox 0x00086E80
u32 spawn_runtime_super_scary_player(const Vec3& position, SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    const u32 player = services.scene_player(); // DAT_00511B40 - 0x93C
    if (const Actor* attached = services.attached_actor(player);
        attached != nullptr && !actor_spawn_owner_unavailable(*attached)) // Body +0x48: the 0x001460C0 test, inlined
    {
        services.release_attachment(player); // 0x0001B810(0, 0)
    }
    services.pause_actors(0xFF, 0); // 0x00117400(0xFFFF, 0)
    auto view = services.actor_view(player);
    if (control.move_player != 0)
    {
        actor_set_world_position(view.actor, position); // 0x00041730, ESI the position, EDI the player
        services.refresh_body_position(player); // Body +0x18 from 0x000412A0 on the body's actor (+8).
    }
    const Vec3 head = actor_world_position(services.actor_view(control.head).actor); // 0x000412A0
    const Vec3 self = actor_world_position(view.actor);
    const Vec3 direction{head.x - self.x, head.y - self.y, head.z - self.z};
    float angle = 0.f;
    runtime_direction_angle(direction, angle); // 0x00105F90
    normalize_euler_angles(view.actor.euler, Vec3{0.f, angle, 0.f}); // 0x00105AA0, EDI the player +0x14
    view.actor.xform_valid = 0; // +0x2A8
    std::array<u32, 1> message{9};
    services.actor_message(player, message);
    return player;
}

// Xbox 0x00086FD0
void set_runtime_super_scary_music(SuperScaryControlState& control, i32 next, SuperScaryControlServices& services)
{
    const i32 current = control.music;
    if (next == current)
    {
        return;
    }
    const auto stream = [](i32 index) -> const char*
    {
        switch (index)
        {
        case 1:
            return "XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONPRESS"; // 0x00301ED8
        case 2:
            return "XACT_SOUNDBANK_GSTREAMSDITTIES_SUCCEED"; // 0x00301F58
        case 3:
            return "XACT_SOUNDBANK_GSTREAMSDITTIES_BUTTONNODIE"; // 0x00301FD8
        default:
            return nullptr;
        }
    };
    if (const char* name = stream(current); name != nullptr)
    {
        release_tracked_audio_bank(name, services.streams); // 0x00038970
    }
    if (const char* name = stream(next); name != nullptr)
    {
        acquire_tracked_audio_bank(name, 7, services.streams); // 0x00038910
    }
    control.music = next;
}

// Xbox 0x00086990
void tick_runtime_super_scary_shocks(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    if (control.first_clip_timer <= 1.f && 0 < control.shocks_left
        && 0.f < control.shock_timer)
    {
        const float left = control.shock_timer - frame_step(services);
        control.shock_timer = left;
        if (left <= 0.f)
        {
            control.shock_timer = 0.45f; // DAT_0032B4F8
            request_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_SHOCKSTRINGS_LEFT", 1, services.voices);
            request_audio_event_voice("XACT_SOUNDBANK_GSUPERSCARYSTUF_SHOCKSTRINGS_RIGHT", 1, services.voices);
            control.shocks_left = control.shocks_left - 1;
            shake_runtime_super_scary_camera(control, services);
        }
    }
}

// Xbox 0x00086A40
u32 runtime_super_scary_right_button(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    if (services.auto_presses != 0)
    {
        const float step = frame_step(services);
        if (0.f < control.auto_press_timer)
        {
            const float left = control.auto_press_timer - step;
            control.auto_press_timer = left;
            if (!(0.f < left))
            {
                control.auto_press_timer = 0.f;
                control.auto_press_timer = (random_unit(services.random) - 1.f) * 0.2f + 0.2f; // DAT_0032B1AC
                return 1;
            }
        }
    }
    const u32 mask = 0x100u << (button_at(control, control.presses) & 0x1F);
    const auto device = input_device(control);
    if (runtime_input_button_edge(device, u16(~mask & 0xF00), services.input_state) != 0) // 0x001083F0, EAX EDI
    {
        return 0;
    }
    return runtime_input_button_edge(device, u16(mask), services.input_state);
}

// Xbox 0x00086B30
u32 runtime_super_scary_wrong_button(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    if (services.auto_presses != 0)
    {
        return 0;
    }
    const u32 mask = 0x100u << (button_at(control, control.presses) & 0x1F);
    return runtime_input_button_edge(input_device(control), u16(~mask & 0xF00), services.input_state);
}

// Xbox 0x00086B70
void shake_runtime_super_scary_camera(SuperScaryControlState& control, SuperScaryControlServices& services)
{
    services.camera_shake(clip_name(control, 0), services.display_view->position, 1.f); // 0x000FB120, EAX +0x294
}

// Xbox 0x00087720
void settle_runtime_super_scary_player(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    const i32 mode = control.scare_mode;
    if (!(1 < mode && mode < 4) || !(0.f < control.settle_timer))
    {
        return;
    }
    const float step = frame_step(services);
    if (0.f < control.settle_timer)
    {
        const float left = control.settle_timer - step;
        control.settle_timer = left;
        if (!(0.f < left))
        {
            control.settle_timer = 0.f;
        }
    }
    auto player = services.actor_view(control.player);
    Vec3 place = actor_world_position(player.actor); // 0x000412A0
    const float top = control.settle_floor_y;
    const float bottom = control.settle_start_y;
    float y = (control.settle_timer - 0.5f) * -2.f * (top - bottom) + bottom; // DAT_0032B0F4, DAT_0032B14C
    if (top > bottom)
    {
        if (y > top)
        {
            y = top;
        }
        else if (bottom > y)
        {
            y = bottom;
        }
    }
    else if (top > y)
    {
        y = top;
    }
    else if (y > bottom)
    {
        y = bottom;
    }
    place.y = y;
    actor_set_world_position(player.actor, place); // 0x00041730
    services.refresh_body_position(control.player);
}

// Xbox 0x00085190
void runtime_super_scary_button_position(std::array<float, 2>& output, const SuperScaryControlState& control, i32 index,
    SuperScaryControlServices& services)
{
    const float width = float(i32(services.screen[0]));
    float centre = width * 0.5f; // DAT_0032B0F4
    const i32 count = control.button_count;
    const float room = width - 96.f; // DAT_0032B1B4
    const float wanted = float(count + 1) * 96.f;
    const auto spacing = [&]()
    {
        if (room > wanted)
        {
            return 96.f;
        }
        return room / wanted * 96.f;
    };
    if (count % 2 != 0)
    {
        centre -= float(count - 1) * spacing() * 0.5f;
    }
    else
    {
        centre -= float(count) * spacing() * 0.5f - spacing() * 0.5f;
    }
    output[0] = float(index) * spacing() + centre;
    output[1] = float(services.display_view->fitted_height) * 0.8f; // +0x64, DAT_0032B244
}

// Xbox 0x00085280
void draw_runtime_super_scary_panel(const SuperScaryControlState& control, SuperScaryControlServices& services)
{
    std::array<float, 2> first{};
    runtime_super_scary_button_position(first, control, 0, services);
    const i32 count = control.button_count;
    std::array<float, 2> last{};
    runtime_super_scary_button_position(last, control, count - 1, services);
    const float room = float(i32(services.screen[0])) - 96.f; // DAT_0032B1B4
    const float wanted = float(count + 1) * 96.f;
    const float left_spacing = room > wanted ? 96.f : room / wanted * 96.f;
    const float left = first[0] - left_spacing;
    const float top = first[1] - 128.f; // DAT_0032B368
    const float right_spacing = room > wanted ? 96.f : room / wanted * 96.f;
    const float right = right_spacing + last[0];
    const float bottom = last[1] + 48.f; // DAT_0032B520
    draw_runtime_dialogue_panel(float(i32(left)), float(i32(top)), float(i32(right - left)),
        float(i32(bottom - top)), 0.25f, services.panel_color, services.panels); // 0x00061360, DAT_0042795C
}

// Xbox 0x00085380
void draw_runtime_super_scary_control(SuperScaryControlState& control,
    SuperScaryControlServices& services)
{
    const i32 phase = control.phase;
    if (phase < 5 || phase > 6 || std::int8_t(services.runtime_flags) < 0)
    {
        return;
    }
    const std::array<float, 2> scale{1.5f, 1.5f}; // DAT_0032B210
    for (i32 index = 0; index < control.button_count; ++index)
    {
        std::array<float, 2> position{};
        runtime_super_scary_button_position(position, control, index, services);
        if (control.presses == index)
        {
            draw_runtime_super_scary_panel(control, services);
        }
        u32 color = services.button_color; // DAT_00314F48
        if (index < control.presses)
        {
            color = (color & 0x00FFFFFFu) | 0x60000000u;
        }
        const u32 button = button_at(control, index);
        queue_runtime_dialogue_sprite(scale.data(), position.data(), control.button_textures.at(button).data(), color,
            0, 0.f, services.sprites); // 0x001221B0, +0x298 + the button * 4
        if (control.presses == index && 0.1f < control.wrong_press_timer) // DAT_0032B254
        {
            const u32 highlight = (services.highlight_color & 0x00FFFFFFu) | 0x80000000u; // DAT_00314F30
            queue_runtime_dialogue_sprite(scale.data(), position.data(), control.button_textures[4].data(), highlight,
                0, 0.f, services.sprites); // +0x2A8
        }
    }
    if (control.bat != 0)
    {
        services.draw_bat(control.bat, i32(services.display_view - entity_detail::g_display_records), 3);
    }
}

// Xbox 0x0006DAD0
void wobble_runtime_button_bat(ButtonBatScreen& bat, ActorAnimationState& animation,
    SuperScaryControlServices& services)
{
    if (bat.state == 2)
    {
        return;
    }
    if (animation.table != nullptr)
    {
        const AnimationRecord* record = find_actor_animation(animation.table, "doh", nullptr,
            services.random); // 0x0003AAA0, ECX 0x0031C3D4
        if (record != nullptr)
        {
            start_actor_animation(animation, *record, services.resources, services.error,
                services.random); // 0x0003A9E0
        }
    }
    bat.state = 4;
}
}
