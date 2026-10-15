#pragma once

#include "gbtg/bundle.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/engine_internal.h"
#include "gbtg/font.h"
#include "gbtg/gfx.h"
#include "gbtg/hash.h"
#include "gbtg/loctext.h"
#include "gbtg/movie.h"
#include "gbtg/resource.h"
#include "gbtg/texture.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <tuple>
#include <vector>


namespace gbtg
{
namespace frontend_detail
{
struct KbKey;
struct KbRow;
}
struct MenuCursor;
using namespace frontend_detail;

// Host resource ownership for the slot +0xB38 / contents +0x864 cursor.
// The cursor state, playback, pose and screen draw use their translated entries.
struct MenuCursor
{
    Mesh mesh;
    ActorModel model;
    ButtonBatScreen bat;
    std::vector<AnimationBundleRecord> resources;
    std::array<AnimationBundlePool, 1> pools;
    AnimationResourceRegistry registry;
    ActorAnimationState animation;
    ResourceErrorState error{};
    ModelPoseBuffer pose;
    ModelAnimationState model_animation;
    ModelNodeFlagCache flags{};
    RuntimeActorSoundQueues sounds;
    std::array<RuntimeSoundVoice, 48> sound_voices{};
    std::array<RuntimeSoundCue, 48> sound_cues{};
    std::array<RuntimeSoundBank, 48> sound_banks{};
    std::array<u64, 48> sound_handles{};
    Game& game;
    u32 random = 0;
    std::vector<float> palette;

    explicit MenuCursor(Game& owner) : game(owner)
    {
        // The object template is embedded at 0x00377CC8, not a demand file.
        const auto* entry = game.aids.load_aid("aid_model_ghoulies_actor_buttonbat");
        if (entry == nullptr)
        {
            throw Error("game select: missing retail button-bat model");
        }
        mesh = parse_model(*entry);
        model.cpu = entry->cpu;
        bat.actor.model = &model;
        bat.actor.radius_xz = 1.f; // template +0x20, DAT_00377CE8.
        bat.actor.scale_200 = 2.f; // DAT_00428F90 / contents DAT_0032B110.
        bat.actor.animation_rate = 1.f;
        for (const auto& record : button_bat_animation_table().records)
        {
            for (const auto name : {record.animation, record.events})
            {
                const auto* resource = game.aids.load_aid(name);
                if (resource == nullptr)
                {
                    throw Error("game select: missing retail cursor animation");
                }
                resources.push_back(AnimationBundleRecord{std::string(name), 0,
                    resource->cpu, resource->gpu, {}, {}});
            }
        }
        pools[0] = AnimationBundlePool{resources, true};
        register_animation_resource_provider(registry,
            [this](u32, const AnimationResourceRecord& record, const void*& resolved)
            {
                return find_bundle_resource_provider(pools, record.name.c_str(), resolved);
            },
            [](u32, const void* resolved, AnimationLoadedResource& output)
            {
                return load_bundle_resource_provider(resolved, output);
            },
            [this](u32, const char* name, const AnimationLoadedResource&)
            {
                unload_bundle_resource_provider(pools, name);
            }, 3, nullptr);
        animation.table = &button_bat_animation_table();
        reset_actor_sound_queues(sounds);
        sounds.voices = sound_voices;
        // Xbox 0x00129F30: capacities of the ordinary and alternate groups.
        sounds.groups[0].capacity = 20;
        sounds.groups[1].capacity = 6;
        sounds.boundaries.stop_cue = [this](void*, u32, u32, void* cue)
        {
            auto* runtime = static_cast<RuntimeSoundCue*>(cue);
            const size_t index = size_t(runtime - sound_cues.data());
            pcm_stop_owned_cue(sound_handles[index]);
            sound_handles[index] = 0;
        };
        animation.message_handler = [this](const ActorAnimationMessage& message)
        {
            // Retail cursor resources contain sound-start / sound-stop events.
            // 0x0006D8A0 -> 0x0003EF70 message 7 -> 0x0003A380.
            switch (read_u16(message.event, 0xC))
            {
            case 0x1D:
                request_actor_event_sound(message.event, bat.actor, random, sounds);
                break;
            case 0x1F:
                stop_actor_sound(bat.actor, i32(read_u32(message.event, 0x10)), sounds);
                break;
            default:
                throw Error("game select: untranslated cursor animation event");
            }
        };
        // 0x0013B580 offsets the constructor's (0,0) anchor by scale * (2,21).
        button_bat_initialize(bat, ButtonBatPoint{-4.f, -42.f}, 0.4f, animation, registry, error, random);
        pose.count = int(mesh.joints.size());
        pose.matrices.resize(mesh.joints.size());
        initialize_model_pose(pose);
        model_animation.primary = &pose;
        model_animation.joint_count = pose.count;
        model_animation.skeleton = mesh.joints;
        palette.resize(mesh.joints.size() * 16);
    }

    ~MenuCursor()
    {
        reset_actor_sound_queues(sounds);
        gfx_release_mesh(mesh);
    }

    void move(int row)
    {
        // Xbox 0x000DE9E0 / 0x000D9F60: table center plus its -20 Y anchor.
        button_bat_tick(bat, ButtonBatPoint{130.f + float(row) * 190.f, 180.f}, 2.f,
            animation, registry, error, random);
        bat.animation = animation.controller->slots[0].player.get();
    }

    void depart(int row)
    {
        // Xbox 0x000E02D0: choose the nearer offscreen edge from the
        // unadjusted +0xB20/+0xB24 anchor, then fly with offset scale 1.
        const float x = 130.f + float(row) * 190.f;
        const float exit_x = 320.f < 640.f - x ? 690.f : -50.f;
        const float exit_y = 240.f < 480.f - 180.f ? 530.f : -50.f;
        button_bat_tick(bat, ButtonBatPoint{exit_x, exit_y}, 1.f,
            animation, registry, error, random);
        bat.animation = animation.controller->slots[0].player.get();
    }

    // Host ownership of the menu's real voices. The pending-list dispatch is
    // 0x0012A7D0; its 0x00039830 / 0x00039A20 calls allocate and play below.
    void dispatch_sounds()
    {
        // Host slots whose retail stop transition has completed can be reused.
        for (auto& voice : sound_voices)
        {
            if (voice.state == 9 || voice.state == 10)
            {
                voice.state = 0;
                voice.cue = nullptr;
            }
        }
        for (auto& sound : bat.actor.sound_slots)
        {
            if (actor_sound_is_active(sound, sounds))
            {
                const size_t index = sound.previous_bone;
                if (!pcm_owned_cue_playing(sound_handles[index]))
                {
                    // 0x0012A510: ended voices leave the active list.
                    release_actor_sound(sound, sounds);
                    sound_voices[index].cue = nullptr;
                    sound_voices[index].state = 0;
                }
            }
        }
        for (auto& group : sounds.groups)
        {
            dispatch_pending_actor_sounds(group, [this](RuntimeActorSound& sound) -> i16
            {
                for (size_t i = 0; i < sound_voices.size(); ++i)
                {
                    if (sound_voices[i].state == 0)
                    {
                        const u64 handle = pcm_play_owned_cue(game, sound.name.data());
                        if (handle == 0)
                        {
                            return -1;
                        }
                        sound_handles[i] = handle;
                        sound_cues[i] = RuntimeSoundCue{};
                        sound_cues[i].bank = &sound_banks[i];
                        sound_cues[i].handle = &sound_cues[i];
                        sound_voices[i].cue = &sound_cues[i];
                        sound_voices[i].state = 5;
                        return static_cast<i16>(i);
                    }
                }
                return -1;
            }, [](Actor&, RuntimeActorSound&)
            {
                // 0x0003EF70 message 0x89 has no actor-specific action.
            });
        }
    }

    void tick(float dt)
    {
        tick_animation_controller(*animation.controller, dt, false, registry);
        dispatch_sounds();
        // Cursor-specific tail of Xbox 0x0006D5C0, after controller advancement.
        switch (bat.state)
        {
        case 1:
            if (actor_tick_timer(bat.actor, bat.idle_timer, 0.f, dt, 1.f))
            {
                button_bat_set_target(bat, 3, animation, registry, error, random);
            }
            break;
        case 2:
        case 3:
        case 4:
            if (actor_animation_completed(animation, 0))
            {
                button_bat_set_target(bat, 1, animation, registry, error, random);
            }
            break;
        }
        bat.animation = animation.controller->slots[0].player.get();
        const AnimationMatrix identity{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
        apply_controller_to_model(*animation.controller, model_animation, false, nullptr, flags, identity);
    }

    void draw()
    {
        for (size_t joint = 0; joint < pose.matrices.size(); ++joint)
        {
            float* output = palette.data() + joint * 16;
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    output[column * 4 + row] = pose.matrices[joint][size_t(row * 4 + column)];
                }
            }
            output[3] = output[7] = output[11] = 0.f;
            output[15] = 1.f;
        }
        gfx_set_bones(palette.data(), pose.count);
        button_bat_draw(bat, mesh);
        gfx_set_bones(nullptr, 0);
    }
};

namespace frontend_detail
{
// Keys built by FUN_00011870. Width is the dword stored at key+4.
// Pages are the three groups split by FUN_00017A40. Layout (FUN_00015C40):
// x starts at 0, y at 135, row pitch is +0xEC (42). The first key of a row then adds 4.
// Draw adds the 64px margin at DAT_0032B450.
struct KbKey
{
    u32 code;
    int width;
};

struct KbRow
{
    KbKey keys[12];
    int count;
};

inline constexpr int kKbPages = 3;

inline constexpr int kKbRows = 5;

inline constexpr KbRow kKb[kKbPages][kKbRows] = {
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {'A', 34}, {'B', 34}, {'C', 34}, {'D', 34}, {'E', 34}, {'F', 34}, {'G', 34}, {'H', 34}, {'I', 34}, {'J', 34}}, 11},
        {{{0x10003, 166}, {'K', 34}, {'L', 34}, {'M', 34}, {'N', 34}, {'O', 34}, {'P', 34}, {'Q', 34}, {'R', 34}, {'S', 34}, {'T', 34}}, 11},
        {{{0x10005, 166}, {'U', 34}, {'V', 34}, {'W', 34}, {'X', 34}, {'Y', 34}, {'Z', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'(', 34}, {')', 34}, {'%', 34}, {'\\', 34}, {'/', 34}}, 6},
        {{{0x10002, 166}, {'$', 34}, {0x00A3, 34}, {0x00A5, 34}, {':', 34}, {'\'', 34}, {'"', 34}}, 7},
        {{{0x10003, 166}, {'?', 34}, {'!', 34}, {0x00BF, 34}, {0x00A1, 34}, {'-', 34}, {'&', 34}}, 7},
        {{{0x10004, 166}, {'{', 34}, {'}', 34}, {0x00AB, 34}, {0x00BB, 34}, {',', 34}, {'.', 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
    {
        {{{0x10007, 166}, {'1', 34}, {'2', 34}, {'3', 34}, {'4', 34}, {'5', 34}, {'6', 34}, {'7', 34}, {'8', 34}, {'9', 34}, {'0', 34}}, 11},
        {{{0x10002, 166}, {0x00C0, 34}, {0x00C1, 34}, {0x00C2, 34}, {0x00C4, 34}, {0x00C3, 34}, {0x00C5, 34}, {0x00C6, 34}, {0x00C7, 34}, {0x00C8, 34}, {0x00C9, 34}, {0x00CA, 34}}, 12},
        {{{0x10003, 166}, {0x00CB, 34}, {0x00CC, 34}, {0x00CD, 34}, {0x00CE, 34}, {0x00CF, 34}, {0x00D1, 34}, {0x00D2, 34}, {0x00D3, 34}, {0x00D4, 34}, {0x00D5, 34}, {0x00D8, 34}}, 12},
        {{{0x10004, 166}, {0x00D6, 34}, {0x00DF, 34}, {0x00D9, 34}, {0x00DA, 34}, {0x00DB, 34}, {0x00DC, 34}, {0x10000, 136}}, 8},
        {{{0x10006, 166}, {0x20, 204}, {0x22B2, 68}, {0x22B3, 68}}, 4},
    },
};

inline constexpr float kFrameW = 640.f;

inline constexpr float kFrameH = 480.f;

// Xbox 0x00142C20: prompt record +0x40 indexes the retail icon AID table.
inline constexpr const char* kJoypadIconNames[] = {
    "aid_texture_joypad_buttons_a",
    "aid_texture_joypad_buttons_b",
    "aid_texture_joypad_buttons_x",
    "aid_texture_joypad_buttons_y",
    "aid_texture_joypad_buttons_black",
    "aid_texture_joypad_buttons_white",
    "aid_texture_joypad_sticks_xboxsticksmaller",
    "aid_texture_joypad_buttons_dpad",
    "aid_texture_joypad_triggers_both",
};

inline constexpr const char* kContentsMovies[] = {
    "aid_movie_ghoulies_gameselect_house",
    "aid_movie_ghoulies_gameselect_zombieclap",
    "aid_movie_ghoulies_gameselect_skellyscratchhead",
};

// Host storage for the glyph arrays in one 0x0005F5C0 widget.
using GooLine = Game::MenuText;
}

} // namespace gbtg
