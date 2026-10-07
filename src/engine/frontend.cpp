#include "gbtg/bundle.hpp"
#include "gbtg/anim.hpp"
#include "gbtg/resource_error.hpp"
#include "gbtg/dialogue.hpp"
#include "gbtg/engine_internal.hpp"
#include "gbtg/font.hpp"
#include "gbtg/gfx.hpp"
#include "gbtg/hash.hpp"
#include "gbtg/loctext.hpp"
#include "gbtg/movie.hpp"
#include "gbtg/resource.hpp"
#include "gbtg/texture.hpp"

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
float scene_sim_step(float dt);

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
        fn_6d4f0(bat, ButtonBatPoint{-4.f, -42.f}, 0.4f, animation, registry, error, random);
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
        fn_6da00(bat, ButtonBatPoint{130.f + float(row) * 190.f, 180.f}, 2.f,
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
        fn_6da00(bat, ButtonBatPoint{exit_x, exit_y}, 1.f,
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
            if (fn_42360(bat.actor, bat.idle_timer, 0.f, dt, 1.f))
            {
                fn_6d8d0(bat, 3, animation, registry, error, random);
            }
            break;
        case 2:
        case 3:
        case 4:
            if (actor_animation_completed(animation, 0))
            {
                fn_6d8d0(bat, 1, animation, registry, error, random);
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
        fn_6d6c0(bat, mesh);
        gfx_set_bones(nullptr, 0);
    }
};

namespace
{

std::u16string gameselect_string(const Game& g, std::string_view key);
std::u16string default_new_game_name(const Game& g);
float text_width(const GloopyFont& font, std::u16string_view text, float scale);
int keyboard_glyph_frame(const Game& g, int page, int row, int col, const std::u16string& label, size_t index);
int keyboard_name_frame(const Game& g, const std::u16string& name, size_t index);
void draw_plate_background(float center_x, float center_y, float width, float height, u32 color);

RgbaImage image_from_entry(const BundleEntry& entry)
{
    TextureHeader hdr;
    if (entry.cpu.size() < 28 || !parse_texture_header(entry.cpu, hdr))
    {
        throw Error("texture header");
    }
    std::span<const u8> gpu = entry.gpu;
    if (!entry.gpu_chunks.empty())
    {
        const auto& chunk = entry.gpu_chunks[0];
        if (size_t(chunk.offset) + chunk.size <= entry.gpu.size())
        {
            gpu = entry.gpu.subspan(chunk.offset, chunk.size);
        }
    }
    return decode_xbox_texture(hdr, gpu);
}

const Game::UiImage* find_ui(const Game& g, std::string_view key)
{
    for (const auto& image : g.ui)
    {
        if (image.name.find(key) != std::string::npos)
        {
            return &image;
        }
    }
    return nullptr;
}

bool scene_control_is(const Game& g, std::string_view suffix)
{
    // The scene script names one objparams. The type table at 0x0030DB50
    // registers that object: scenecontrol_gameselect is entitySceneControlGameSelect.
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()) == suffix)
        {
            return true;
        }
    }
    return false;
}

bool scene_control_starts(const Game& g, std::string_view suffix)
{
    constexpr std::string_view kPrefix = "aid_objparams_ghoulies_scenecontrol_";
    for (const auto& aid : g.scene_script.aids)
    {
        if (aid.starts_with(kPrefix) && aid.substr(kPrefix.size()).starts_with(suffix))
        {
            return true;
        }
    }
    return false;
}

// Xbox 0x0005CA60
// Submits the quad 0x0005E380 pushes. x,y is the top-left in the 640x480 HUD.
void draw_sprite(const Game::UiImage* image, float x, float y, float w, float h)
{
    if (image != nullptr)
    {
        gfx_sprite_submit(image->image, x, y, w, h);
    }
}

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

constexpr int kKbPages = 3;
constexpr int kKbRows = 5;

constexpr KbRow kKb[kKbPages][kKbRows] = {
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

const KbRow& kb_row(const Game& g)
{
    return kKb[g.kb_page][g.kb_row];
}

void kb_clamp_col(Game& g)
{
    const int n = kb_row(g).count;
    if (g.kb_col >= n)
    {
        g.kb_col = n - 1;
    }
    if (g.kb_col < 0)
    {
        g.kb_col = 0;
    }
}

void kb_open(Game& g)
{
    begin_naming_background(g);
    // 0x000E02D0 empty slot: object +0x94 = 4. 0x000DCEC0 loads
    // supportsavedgame__defaultnewgamename into the naming widget. 0x000110D0
    // copies that into the keyboard, and 0x00011870 stores its length at +0xA8,
    // which is the caret.
    g.frontend_mode = 4;
    g.naming = true;
    g.kb_page = 0;
    g.kb_row = 0;
    g.kb_col = 0;
    g.kb_saved = 1;
    g.kb_shift = false;
    g.kb_caps = false;
    const std::u16string name = default_new_game_name(g);
    g.kb_len = int(name.size());
    if (g.kb_len > 64)
    {
        g.kb_len = 64;
    }
    g.kb_caret = g.kb_len;
    // 0x00015A10 and 0x00016EF0 build new text widgets here, so the morph restarts.
    ++g.keyboard_generation;
    for (int i = 0; i < 64; ++i)
    {
        g.kb_text[i] = i < g.kb_len ? name[size_t(i)] : 0;
    }
}

bool kb_blocked(const Game& g)
{
    // FUN_00017560. Symbols page cannot rest on Shift or Caps Lock.
    const KbRow& row = kb_row(g);
    if (g.kb_col < 0 || g.kb_col >= row.count)
    {
        return true;
    }
    const u32 code = row.keys[g.kb_col].code;
    return g.kb_page == 1 && (code == 0x10002 || code == 0x10003);
}

void kb_up_adjust(Game& g)
{
    // FUN_00016950, jump table 0x16B14. Adjust the column, then the caller steps the row.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 1)
        {
            if (col > 1 && col < 5)
            {
                g.kb_saved = col;
                g.kb_col = 1;
            }
            else if (col == 5)
            {
                g.kb_col = (g.kb_saved == 2 || g.kb_saved == 3) ? g.kb_saved : 1;
                g.kb_saved = 5;
            }
        }
        else if (col > 1 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col > 6 && col < 9)
        {
            g.kb_saved = col;
            g.kb_col = 2;
        }
        else if (col > 8)
        {
            g.kb_saved = col;
            g.kb_col = 3;
        }
        break;
    case 1:
        if (page == 2 && col == 11)
        {
            g.kb_saved = 11;
            g.kb_col = 10;
        }
        else if (page == 1 && col == 6)
        {
            g.kb_saved = 6;
            g.kb_col = 5;
        }
        break;
    case 3:
        if (col == 7)
        {
            int saved = g.kb_saved;
            if (page == 1)
            {
                if (saved > 6)
                {
                    saved = 6;
                }
            }
            else if (saved < 7)
            {
                saved = 7;
            }
            g.kb_col = saved;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            if (saved > 6)
            {
                saved = 6;
            }
            g.kb_col = saved;
        }
        else if (col > 1)
        {
            g.kb_col = 7;
        }
        break;
    default:
        break;
    }
}

void kb_down_adjust(Game& g)
{
    // FUN_00016B30, jump table 0x16D2C.
    const int page = g.kb_page;
    const int col = g.kb_col;
    switch (g.kb_row)
    {
    case 0:
        if (page == 2 && col == 10)
        {
            g.kb_col = g.kb_saved > 10 ? g.kb_saved : 10;
        }
        else if (page == 1 && col == 5)
        {
            int saved = g.kb_saved < 5 ? 5 : g.kb_saved;
            g.kb_col = saved > 6 ? 6 : saved;
        }
        break;
    case 2:
        if (col > 7)
        {
            g.kb_saved = col;
            g.kb_col = 7;
        }
        break;
    case 3:
        if (col > 0 && col < 7)
        {
            g.kb_saved = col;
            g.kb_col = 1;
        }
        else if (col >= 7)
        {
            g.kb_col = g.kb_saved > 8 ? 3 : 2;
        }
        break;
    case 4:
        if (col == 1)
        {
            int saved = g.kb_saved;
            const int cap = page == 1 ? 5 : 6;
            if (saved > cap)
            {
                saved = cap;
            }
            g.kb_col = saved;
        }
        else if (col == 2)
        {
            if (page == 1)
            {
                g.kb_saved = 2;
                g.kb_col = 5;
            }
            else
            {
                int saved = g.kb_saved;
                if (saved > 8)
                {
                    saved = 8;
                }
                else if (saved <= 7)
                {
                    saved = 7;
                }
                g.kb_col = saved;
            }
        }
        else if (col == 3)
        {
            if (page == 1)
            {
                g.kb_saved = 3;
                g.kb_col = 5;
            }
            else
            {
                g.kb_col = g.kb_saved < 10 ? 9 : 10;
            }
        }
        break;
    default:
        break;
    }
}

void kb_move(Game& g, int dcol, int drow)
{
    if (dcol != 0)
    {
        // 0x00016D40 / 0x00016DA0 wrap inside the row, then 0x00017560 may repeat the step.
        for (int guard = 0; guard < 16; ++guard)
        {
            const int n = kb_row(g).count;
            g.kb_col = (g.kb_col + dcol + n) % n;
            if (!kb_blocked(g))
            {
                return;
            }
        }
        return;
    }
    // +0xF0 is 5. A blocked landing (Shift / Caps on the symbols page) repeats the move.
    for (int guard = 0; guard < 16; ++guard)
    {
        if (drow < 0)
        {
            kb_up_adjust(g);
            g.kb_row = g.kb_row == 0 ? kKbRows - 1 : g.kb_row - 1;
        }
        else
        {
            kb_down_adjust(g);
            g.kb_row = g.kb_row == kKbRows - 1 ? 0 : g.kb_row + 1;
        }
        if (!kb_blocked(g))
        {
            return;
        }
    }
}

// 0x000357D0. EAX and EDX are the two names. 1 when both start with 'aid_' and
// the rest matches after AND 0xDF, or when neither starts with 'aid_'.
bool aid_name_match(std::string_view a, std::string_view b)
{
    auto aid_head = [](std::string_view s)
    {
        return s.size() >= 4 &&
            static_cast<unsigned char>(s[0]) == 'a' &&
            static_cast<unsigned char>(s[1]) == 'i' &&
            static_cast<unsigned char>(s[2]) == 'd' &&
            static_cast<unsigned char>(s[3]) == '_';
    };
    const bool a_aid = aid_head(a);
    const bool b_aid = aid_head(b);
    if (!a_aid)
    {
        return !b_aid;
    }
    if (!b_aid)
    {
        return false;
    }
    size_t i = 4;
    for (;;)
    {
        const unsigned char ca = (i < a.size() ? static_cast<unsigned char>(a[i]) : 0) & 0xDF;
        const unsigned char cb = (i < b.size() ? static_cast<unsigned char>(b[i]) : 0) & 0xDF;
        if (ca != cb)
        {
            return false;
        }
        if (ca == 0)
        {
            return true;
        }
        ++i;
    }
}

bool aid_slot(std::string_view name)
{
    return name.size() >= 4 &&
        static_cast<unsigned char>(name[0]) == 'a' &&
        static_cast<unsigned char>(name[1]) == 'i' &&
        static_cast<unsigned char>(name[2]) == 'd' &&
        static_cast<unsigned char>(name[3]) == '_';
}

// 0x00035960. The list is the packed aid names at *DAT_00511B64 (sceneorder).
// EBX is the key. The stack flag is 1 from 0x00010E9D0. A match whose next
// slot starts with 'aid_' returns that next name. The je at 0x000359BA lands
// on the epilogue, so the lea of the next slot is the return value.
const char* sceneorder_lookup(const Game& g, const char* key, int flag)
{
    if (g.scene_order.empty())
    {
        return nullptr;
    }
    if (key == nullptr)
    {
        return g.scene_order[0].c_str();
    }
    size_t i = 0;
    for (;;)
    {
        const bool next_aid = i + 1 < g.scene_order.size() && aid_slot(g.scene_order[i + 1]);
        if (aid_name_match(g.scene_order[i], key))
        {
            if (next_aid)
            {
                return g.scene_order[i + 1].c_str();
            }
            if (flag == 0)
            {
                return nullptr;
            }
            return g.scene_order[0].c_str();
        }
        if (!next_aid)
        {
            return nullptr;
        }
        ++i;
    }
}

// 0x00010E9D0. The new save's script id is the 0x00035960 result for
// aid_script_ghoulies_gameselect_gameselect (0x00322B8C).
void store_new_game_script(Game& g)
{
    const char* script = sceneorder_lookup(g, "aid_script_ghoulies_gameselect_gameselect", 1);
    char* dest = g.slot_script[g.slot];
    if (script == nullptr)
    {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (script[i] != '\0' && i + 1 < sizeof(g.slot_script[g.slot]))
    {
        dest[i] = script[i];
        ++i;
    }
    dest[i] = '\0';
}

bool wide_name_equal(const char16_t* a, const char16_t* b)
{
    // 0x0014A91C. lstrcmpW.
    for (;;)
    {
        if (*a != *b)
        {
            return false;
        }
        if (*a == 0)
        {
            return true;
        }
        ++a;
        ++b;
    }
}

// 0x0010DB10. Walk the save list for the wide name and copy blob+0x102.
// A miss leaves the caller's buffer unwritten.
const char* save_script_for_slot(const Game& g, int slot)
{
    if (g.slot_name[slot][0] == 0)
    {
        return nullptr;
    }
    for (int i = 0; i < 3; ++i)
    {
        if (g.slot_script[i][0] == '\0')
        {
            continue;
        }
        if (wide_name_equal(g.slot_name[i], g.slot_name[slot]))
        {
            return g.slot_script[i];
        }
    }
    return nullptr;
}

const char* save_script(const Game& g)
{
    return save_script_for_slot(g, g.slot);
}

char16_t kb_insert_char(const Game& g, u32 code)
{
    // Keys are stored as the uppercase / symbol code. Shift (cleared after one insert) or caps
    // keeps A-Z; otherwise letters go in lowercase. FUN_000176E0 does this case map.
    if (code >= 'A' && code <= 'Z' && !g.kb_shift && !g.kb_caps)
    {
        return char16_t(code + 32);
    }
    return char16_t(code);
}

void kb_activate(Game& g)
{
    const u32 code = kb_row(g).keys[g.kb_col].code;
    // 0x000152F0. Codes below 0x10000 (except the caret arrows) insert.
    if (code == 0x22B2)
    {
        if (g.kb_caret > 0)
        {
            --g.kb_caret;
        }
        return;
    }
    if (code == 0x22B3)
    {
        if (g.kb_caret < g.kb_len)
        {
            ++g.kb_caret;
        }
        return;
    }
    if (code < 0x10000)
    {
        if (g.kb_len >= 64)
        {
            return;
        }
        for (int i = g.kb_len; i > g.kb_caret; --i)
        {
            g.kb_text[i] = g.kb_text[i - 1];
        }
        g.kb_text[g.kb_caret] = kb_insert_char(g, code);
        ++g.kb_caret;
        ++g.kb_len;
        g.kb_text[g.kb_len] = 0;
        if (g.kb_shift)
        {
            g.kb_shift = false;
        }
        return;
    }
    if (code == 0x10000)
    {
        if (g.kb_caret <= 0)
        {
            return;
        }
        for (int i = g.kb_caret - 1; i < g.kb_len; ++i)
        {
            g.kb_text[i] = g.kb_text[i + 1];
        }
        --g.kb_caret;
        --g.kb_len;
        return;
    }
    if (code == 0x10002)
    {
        g.kb_shift = !g.kb_shift;
        return;
    }
    if (code == 0x10003)
    {
        g.kb_caps = !g.kb_caps;
        return;
    }
    // Page keys rewrite +0xBC. 0x10005 symbols, 0x10006 accents, 0x10004 alphabet.
    if (code == 0x10005)
    {
        g.kb_page = 1;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10006)
    {
        g.kb_page = 2;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10004)
    {
        g.kb_page = 0;
        kb_clamp_col(g);
        return;
    }
    if (code == 0x10007)
    {
        // Xbox 0x000DD270 returns to widget 1, then 0x000DD6C0 closes
        // the naming widget with mode 4. That is not a saved-slot status.
        g.frontend_mode = 1;
        g.naming = false;
        if (g.kb_len > 0)
        {
            g.slot_name_len[g.slot] = g.kb_len;
            for (int i = 0; i < 64; ++i)
            {
                g.slot_name[g.slot][i] = g.kb_text[i];
            }
            g.slot_status[g.slot] = 2;
            // 0x00010D430's success path calls 0x00010E9D0 before 0x000DD270.
            // 0x00010E9D0 stores 0x00035960's return at blob+0x102. The key is
            // aid_script_ghoulies_gameselect_gameselect (0x00322B8C). On the retail
            // sceneorder that return is the following aid, chapter1_scene0_1book.
            store_new_game_script(g);
            std::cout << "slot " << g.slot << " named (" << g.kb_len << " chars)\n";
        }
    }
}

// One row of the slot table at 0x0042902C, stride 0x124.
// Init copies the center into the widget at +0x428 / +0x42C.
// 0x000DF9A0 case 6 writes the sprite scale at +0x434 / +0x438:
//   screen * rest_norm / texture_size
// and the hit box at +0x510 / +0x514: rest_norm * {640,480} + DAT_00428FBC (12).
// 0x000DE540 lerps the selected slot from that rest scale to the 0.25 hot scale.
// 0x0005CA60 then centers a quad of texture_size * scale on +0x428. The box is
// only the argument to 0x00061360.
struct SlotPlate
{
    const char* texture;
    float center_x;
    float center_y;
    float rest_x;
    float rest_y;
    float hot_x;
    float hot_y;
};

constexpr float kFrameW = 640.f;
constexpr float kFrameH = 480.f;

constexpr SlotPlate kSlots[3] = {
    {"gameselect_oneframe", 130.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_twoframe", 320.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
    {"gameselect_threeframe", 510.f, 200.f, 0.16f, 0.16f, 0.25f, 0.25f},
};

// Xbox 0x0004B4D0. Suffixes at 0x004232F4 map through 0x002FE698.
int script_chapter_index(std::string_view script)
{
    constexpr std::string_view prefix = "aid_script_ghoulies_chapter";
    constexpr std::string_view suffixes[] = {"void", "1", "2a", "2b", "2c", "3a", "3b", "3c", "4", "5"};
    if (script.compare(0, 0x1b, prefix) == 0)
    {
        for (int i = 1; i < 10; ++i)
        {
            if (script.substr(0x1b).starts_with(suffixes[i]))
            {
                return i;
            }
        }
    }
    return 0;
}

int slot_chapter(const Game& g, int index)
{
    const char* script = save_script_for_slot(g, index);
    if (script == nullptr)
    {
        throw Error("game select: occupied slot has no translated save script");
    }
    constexpr int chapters[] = {-1, 1, 2, 2, 2, 3, 3, 3, 4, 5};
    return chapters[script_chapter_index(script)];
}

// Xbox 0x000DE9E0: the selected slot owns the movie playback reference.
const char* slot_movie(const Game& g, int index)
{
    // 0x000E01B0 fills DAT_00506598 with the empty-slot movies and DAT_005065A4 with chapters.
    const int status = g.slot_status[index];
    if (status == 2 || status == 4 || status == 3)
    {
        static const char* chapters[] = {
            "aid_movie_ghoulies_gameselect_chapter1",
            "aid_movie_ghoulies_gameselect_chapter2",
            "aid_movie_ghoulies_gameselect_chapter3",
            "aid_movie_ghoulies_gameselect_chapter4",
            "aid_movie_ghoulies_gameselect_chapter5",
        };
        const int chapter = slot_chapter(g, index);
        if (chapter < 1 || chapter > 5)
        {
            throw Error("game select: save script has no chapter movie");
        }
        return chapters[chapter - 1];
    }
    static const char* kEmpty[3] = {
        "aid_movie_ghoulies_gameselect_emptyslots_one",
        "aid_movie_ghoulies_gameselect_emptyslots_two",
        "aid_movie_ghoulies_gameselect_emptyslots_three",
    };
    return kEmpty[index];
}

// Xbox 0x00101730: interpolate each packed color byte, clamp, then truncate.
u32 menu_color(u32 selected, u32 rest, float fraction)
{
    u32 color = 0;
    for (int shift = 0; shift < 32; shift += 8)
    {
        float value = float((rest >> shift) & 0xff) * (1.f - fraction) +
                      float((selected >> shift) & 0xff) * fraction;
        if (value < 0.f)
        {
            value = 0.f;
        }
        else if (255.f < value)
        {
            value = 255.f;
        }
        color |= u32(u8(value)) << shift;
    }
    return color;
}

std::string slot_backdrop(const Game& g)
{
    const int status = g.slot_status[g.slot];
    return status == 2 || status == 4 || status == 3
               ? "gameselect_chapter" + std::to_string(slot_chapter(g, g.slot))
               : "gameselect_emptyslotbackground";
}

constexpr const char* kContentsTextures[3] = {
    "gameselect_house", "gameselect_zombieclapframe", "gameselect_skellyscratchheadframe",
};

// Xbox 0x000DF140 / 0x000DA1E0: retain the previous background and start
// the countdown from button-bat +0x93C. The menu constructors override it.
void begin_menu_zoom(Game::MenuVisual& visual, const std::string& backdrop)
{
    visual.old_backdrop = visual.backdrop;
    visual.backdrop = backdrop;
    visual.remaining = visual.duration;
    visual.fraction = 0.f;
    visual.alpha = 0;
    visual.old_alpha = 255;
    ++visual.generation;
}

// Xbox 0x000DF330 / 0x000DA300, followed by the background-alpha part of
// 0x000DE540 / 0x000D9D10. Plate values are read from fraction when drawn.
void tick_menu_zoom(Game::MenuVisual& visual, float dt)
{
    if (0.f < visual.remaining)
    {
        visual.remaining -= dt;
        if (visual.remaining <= 0.f)
        {
            visual.remaining = 0.f;
        }
    }
    float fraction = 1.f;
    if (visual.duration != 0.f)
    {
        fraction = (visual.remaining - visual.duration) / (0.f - visual.duration);
        if (1.f < fraction)
        {
            fraction = 1.f;
        }
        else if (fraction < 0.f)
        {
            fraction = 0.f;
        }
    }
    visual.fraction = fraction;
    if (visual.old_backdrop != visual.backdrop)
    {
        visual.alpha = u8(fraction * 255.f);
        visual.old_alpha = 255 - visual.alpha;
    }
}

// Xbox 0x000DDF20 / 0x000D9640: current backdrop precedes the old backdrop.
void draw_menu_backdrop(const Game& g, const Game::MenuVisual& visual)
{
    if (const Game::UiImage* image = find_ui(g, visual.backdrop))
    {
        gfx_sprite_submit(image->image, 0.f, 0.f, kFrameW, kFrameH, 0.f, 0.f, 1.f, 1.f,
                          {1.f, 1.f, 1.f, float(visual.alpha) / 255.f});
    }
    if (const Game::UiImage* image = find_ui(g, visual.old_backdrop))
    {
        gfx_sprite_submit(image->image, 0.f, 0.f, kFrameW, kFrameH, 0.f, 0.f, 1.f, 1.f,
                          {1.f, 1.f, 1.f, float(visual.old_alpha) / 255.f});
    }
}

// Xbox 0x000DDF20: plate pass, before the selected movie overlay.
void draw_plate(const Game& g, int index, bool selected)
{
    const SlotPlate& slot = kSlots[index];
    // 0x000DEB20. Empty uses the slot's oneframe/twoframe/threeframe.
    // A named slot (status 4) uses the chapter table at 0x0042939C. A new game is chapter 1.
    std::string texture = slot.texture;
    switch (g.slot_status[index])
    {
    case 2:
    case 4:
        texture = "gameselect_chapter" + std::to_string(slot_chapter(g, index));
        break;
    case 3:
        texture = "gameselect_chapter1";
        break;
    }
    const float fraction = selected ? g.slot_visual.fraction : 0.f;
    const float nx = (slot.hot_x - slot.rest_x) * fraction + slot.rest_x;
    const float ny = (slot.hot_y - slot.rest_y) * fraction + slot.rest_y;
    // 0x0005CA60. On-screen size is texture pixels * scale, and scale is
    // (norm * screen) / texture pixels, so the quad is norm * {640,480}
    // centered on the table center.
    const float w = nx * kFrameW;
    const float h = ny * kFrameH;
    const Game::UiImage* image = find_ui(g, texture);
    if (image != nullptr)
    {
        const float padding = (20.f - 12.f) * fraction + 12.f;
        draw_plate_background(slot.center_x, slot.center_y, w + padding, h + padding,
                              menu_color(0xC43EF7B6u, 0x80069764u, fraction));
        draw_sprite(image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f, w, h);
    }
}

// Xbox 0x000DDF20: +0xB84 is tested against DAT_00429000 (10), then
// faded over DAT_00429004 frames. Static plates remain underneath the movie.
void draw_slot_movie(const Game& g)
{
    if (g.slot_movie_index != g.slot)
    {
        if (g.slot_movie_index != -1)
        {
            movie_release_visible(g.slot_movie_name.c_str());
        }
        g.slot_movie_index = g.slot;
        g.slot_movie_name = slot_movie(g, g.slot);
        g.slot_movie_frames = 0;
        movie_note_visible(slot_movie(g, g.slot));
    }
    const char* movie = slot_movie(g, g.slot);
    if (10 <= g.slot_movie_frames)
    {
        movie_copy_frame(movie, g.slot_movie_image);
    }
    if (movie_is_open(movie))
    {
        if (10 < g.slot_movie_frames)
        {
            const SlotPlate& slot = kSlots[g.slot];
            const float w = ((slot.hot_x - slot.rest_x) * g.slot_visual.fraction + slot.rest_x) * kFrameW;
            const float h = ((slot.hot_y - slot.rest_y) * g.slot_visual.fraction + slot.rest_y) * kFrameH;
            const float alpha = float(u8(std::clamp(float(g.slot_movie_frames - 10) / 20.f * 255.f,
                                                   0.f, 255.f))) / 255.f;
            gfx_sprite_submit(g.slot_movie_image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f, w, h,
                              0.f, 0.f, 1.f, 1.f, {1.f, 1.f, 1.f, alpha});
        }
        ++g.slot_movie_frames;
    }
}

// Xbox 0x000DEB20 initializes the overlays. 0x000DE540 scales them with
// the selected plate, and 0x000DDF20 draws them after the movie overlay.
void draw_slot_status(const Game& g, int index)
{
    const char* texture = nullptr;
    if (g.slot_status[index] == 4)
    {
        texture = "gameselect_disc_nowrite";
    }
    if (g.slot_status[index] == 3)
    {
        texture = "gameselect_disc_broken";
    }
    if (texture != nullptr)
    {
        if (const Game::UiImage* image = find_ui(g, texture))
        {
            const SlotPlate& slot = kSlots[index];
            const float fraction = index == g.slot ? g.slot_visual.fraction : 0.f;
            const float scale_x = 0.7f * ((slot.hot_x / slot.rest_x - 1.f) * fraction + 1.f);
            const float scale_y = 0.7f * ((slot.hot_y / slot.rest_y - 1.f) * fraction + 1.f);
            const float w = float(image->image.width) * scale_x;
            const float h = float(image->image.height) * scale_y;
            gfx_sprite_submit(image->image, slot.center_x - w * 0.5f, slot.center_y - h * 0.5f,
                              w, h, 0.f, 0.f, 1.f, 1.f, {1.f, 1.f, 1.f, 127.f / 255.f});
        }
    }
}

std::u16string key_label(const Game& g, u32 code)
{
    // 0x00011150. The wide string is the loctext pointer at [0x00338DCC + offset].
    // 0x20 Space, 0x10000 Backspace, 0x10002 Shift, 0x10003 Capslock,
    // 0x10004 Alphabet, 0x10005 Symbols, 0x10006 Accents, 0x10007 Done.
    const char* key = nullptr;
    switch (code)
    {
    case 0x10007:
        key = "virtualkeyboard_Done";
        break;
    case 0x10006:
        key = "virtualkeyboard_Accents";
        break;
    case 0x10005:
        key = "virtualkeyboard_Symbols";
        break;
    case 0x10004:
        key = "virtualkeyboard_Alphabet";
        break;
    case 0x10003:
        key = "virtualkeyboard_Capslock";
        break;
    case 0x10002:
        key = "virtualkeyboard_Shift";
        break;
    case 0x10000:
        key = "virtualkeyboard_Backspace";
        break;
    case 0x20:
        key = "virtualkeyboard_Space";
        break;
    case 0x22B2:
    case 0x22B3:
        return {};
    default:
        return std::u16string(1, kb_insert_char(g, code));
    }
    return gameselect_string(g, key);
}

const GloopyFont& gloopy_font(const Game& g)
{
    static GloopyFont font;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "font" / "069563d4";
        if (!font.load(path))
        {
            std::cerr << "gloopy font failed " << path.string() << "\n";
        }
    }
    return font;
}

// Font table slot 1. 0x000605A0(1) selects aid_font_localisation_dialog_english_dialogfont.
const GloopyFont& dialog_font(const Game& g)
{
    static GloopyFont font;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "font" / "02d54854";
        if (!font.load(path))
        {
            std::cerr << "dialog font failed " << path.string() << "\n";
        }
    }
    return font;
}

// Xbox 0x0005E9F0
// 0x00051760 advances by x1-x0. 0x000608D0 shifts the cell left by x0 so texel x0
// sits on the pen. 0x0005CA60 centers the quad, and the half-cell bias here cancels
// that, leaving the top-left at (pen - x0*scale, pen_y). A zero scale returns.
void draw_glyphs(const GloopyFont& font, std::u16string_view text, float pen_x, float pen_y, float scale, int frame)
{
    if (font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    const float em = float(font.cell()) * scale;
    float x = pen_x;
    for (char16_t ch : text)
    {
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

void draw_centered_line(const GloopyFont& font, std::u16string_view text, float anchor_x, float pen_y, float scale,
                        int frame)
{
    if (text.empty() || font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    draw_glyphs(font, text, anchor_x - width * 0.5f, pen_y, scale, frame);
}

// Xbox 0x00061360. Host rasterization of its central quad and mirrored
// horizontal scanlines. Keyboard calls pass 0.25; menu plates pass 0.1.
const RgbaImage& rounded_fill(int w, int h, u32 color, float corner_fraction = 0.25f)
{
    const int radius = int(std::floor(float(h) * corner_fraction));
    struct Key
    {
        int w;
        int h;
        int radius;
        u32 color;
        bool operator<(const Key& other) const
        {
            return std::tie(w, h, radius, color) < std::tie(other.w, other.h, other.radius, other.color);
        }
    };
    static std::map<Key, RgbaImage> images;
    const Key key{w, h, radius, color};
    auto found = images.find(key);
    if (found == images.end())
    {
        RgbaImage image;
        image.width = w;
        image.height = h;
        image.bgra.assign(size_t(w * h * 4), 0);
        for (int y = 0; y < h; ++y)
        {
            const int dy = y < radius ? radius - y : y > h - radius ? y - (h - radius) : 0;
            const float inset = float(radius) - std::sqrt(float(radius * radius - dy * dy));
            for (int x = int(std::ceil(inset)); float(x) < float(w) - inset; ++x)
            {
                const size_t offset = size_t((y * w + x) * 4);
                image.bgra[offset] = u8(color >> 16);
                image.bgra[offset + 1] = u8(color >> 8);
                image.bgra[offset + 2] = u8(color);
                image.bgra[offset + 3] = u8(color >> 24);
            }
        }
        found = images.emplace(key, std::move(image)).first;
    }
    return found->second;
}

void draw_plate_background(float center_x, float center_y, float width, float height, u32 color)
{
    const int w = int(width);
    const int h = int(height);
    // 0x000DDF20 truncates each dimension before integer division by two.
    gfx_sprite_submit(rounded_fill(w, h, color, 0.1f), float(int(center_x) - w / 2),
                      float(int(center_y) - h / 2), float(w), float(h));
}
// 0x00016FC0. x,y are the text pen; the button sits 5px up and left (DAT_0032B184)
// and is textWidth + 10 by cell*0.65 + 10 (DAT_0032B10C). 0xFFFF0000 is not drawn.
void draw_key_button(float pen_x, float pen_y, float text_w, float em, u32 color)
{
    if (color == 0xFFFF0000u)
    {
        return;
    }
    const float bw = text_w + 10.f;
    const float bh = em + 10.f;
    const int iw = int(bw);
    const int ih = int(bh);
    gfx_sprite_submit(rounded_fill(iw, ih, color), pen_x - 5.f, pen_y - 5.f, float(iw), float(ih));
}

// 0x00017210 chooses the button colour before 0x00016FC0.
// Default DAT_0020AE6C, hot key DAT_0020AE68. Shift (+0x10) and Caps (+0xC)
// use the hot colour on pages 0 and 2. Page 1 draws both of those keys with
// DAT_0020AE78. The hot key wins after that test.
u32 key_fill(const Game& g, u32 code, bool hot)
{
    constexpr u32 kFill = 0x80069764u;
    constexpr u32 kHot = 0xC43EF7B6u;
    constexpr u32 kGray = 0x80808080u;
    u32 color = kFill;
    if (code == 0x10002 || code == 0x10003)
    {
        const bool latched = code == 0x10002 ? g.kb_shift : g.kb_caps;
        if (g.kb_page == 1)
        {
            color = kGray;
        }
        else if (latched && (g.kb_page == 0 || g.kb_page == 2))
        {
            color = kHot;
        }
    }
    if (hot)
    {
        color = kHot;
    }
    return color;
}

// Xbox 0x00142C20: prompt record +0x40 indexes the retail icon AID table.
static constexpr const char* kJoypadIconNames[] = {
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

const RgbaImage* joypad_icon(const Game& g, int code)
{
    if (code < 0 || code >= 9)
    {
        return nullptr;
    }
    for (const auto& image : g.ui)
    {
        if (image.name == kJoypadIconNames[code])
        {
            return &image.image;
        }
    }
    return nullptr;
}

void draw_keyboard(const Game& g)
{
    if (!g.naming)
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    if (font.cell() <= 0)
    {
        return;
    }
    const Game::UiImage* caret = find_ui(g, "virtkb_caret");
    const Game::UiImage* left = find_ui(g, "virtkb_arrowleft");
    const Game::UiImage* right = find_ui(g, "virtkb_arrowright");
    const int frame = font.rest_frame();
    constexpr float kLabel = 0.65f;
    constexpr float kName = 1.45f;
    const float em = float(font.cell()) * kLabel;
    const float name_em = float(font.cell()) * kName;

    // 0x00017210. Bar colour DAT_0020AE70. x=122, y=46. Width is
    // [keyboard+0x114] + 16, and +0x114 is screen width - 260. Height is
    // cell*1.45 + 16. Both extents are truncated before the fill.
    const int bar_w = int((kFrameW - 260.f) + 16.f);
    const int bar_h = int(name_em + 16.f);
    gfx_sprite_submit(rounded_fill(bar_w, bar_h, 0x40069764u), 122.f, 46.f, float(bar_w), float(bar_h));

    // 0x00015A10. Pen (130, 54), scale 1.45, eax=1. 0x00060070 morphs, then holds
    // the last frame. Insert calls 0x00015A10 again, which starts a new widget.
    // 0x000174D0 / 0x00015010 centres virtkb_caret on that string at the same scale.
    const std::u16string name(g.kb_text, g.kb_text + g.kb_len);
    float name_x = 130.f;
    float caret_x = 130.f;
    for (int i = 0; i < g.kb_len; ++i)
    {
        if (i == g.kb_caret)
        {
            caret_x = name_x;
        }
        const int name_frame = keyboard_name_frame(g, name, size_t(i));
        if (const RgbaImage* glyph = font.image(g.kb_text[i], name_frame))
        {
            const float bx = float(font.bearing(g.kb_text[i])) * kName;
            gfx_sprite_submit(*glyph, name_x - bx, 54.f, name_em, name_em);
        }
        name_x += float(font.advance(g.kb_text[i])) * kName;
    }
    if (g.kb_caret >= g.kb_len)
    {
        caret_x = name_x;
    }
    if (caret != nullptr)
    {
        const float cw = float(caret->image.width) * kName;
        const float ch = float(caret->image.height) * kName;
        const float cy = 54.f + name_em * 0.5f;
        draw_sprite(caret, caret_x - cw * 0.5f, cy - ch * 0.5f, cw, ch);
    }

    for (int r = 0; r < kKbRows; ++r)
    {
        const KbRow& row = kKb[g.kb_page][r];
        float x = 0.f;
        for (int c = 0; c < row.count; ++c)
        {
            const float slot = float(row.keys[c].width);
            const float pen_x = 64.f + x;
            const float pen_y = 135.f + float(r) * 42.f;
            const u32 code = row.keys[c].code;
            const bool hot = r == g.kb_row && c == g.kb_col;
            const std::u16string label = key_label(g, code);
            // 0x00016360 falls through to the key code. A missing glyph, which
            // is what the arrow codes are, advances by the cell, so the button
            // is square. Letters and words use the measured string.
            const float measured = label.empty() ? float(font.advance(code)) * kLabel : text_width(font, label, kLabel);
            draw_key_button(pen_x, pen_y, measured, em, key_fill(g, code, hot));
            if (!label.empty())
            {
                // 0x00016EF0 submits eax=1, so 0x00060070 plays the morph and
                // then holds the last frame. The highlight does not put it back
                // on the texture clock.
                float gx = pen_x;
                for (size_t i = 0; i < label.size(); ++i)
                {
                    const char16_t ch = label[i];
                    const int glyph_frame = keyboard_glyph_frame(g, g.kb_page, r, c, label, i);
                    if (const RgbaImage* glyph = font.image(ch, glyph_frame))
                    {
                        const float bx = float(font.bearing(ch)) * kLabel;
                        gfx_sprite_submit(*glyph, gx - bx, pen_y, em, em);
                    }
                    gx += float(font.advance(ch)) * kLabel;
                }
            }
            // 0x00015A60 / 0x00015B50. Centre is half the measured width across
            // and half an em down. 0x0005CA60 then centres the texture, scale 1.
            if (code == 0x22B2 || code == 0x22B3)
            {
                const Game::UiImage* arrow = code == 0x22B2 ? left : right;
                if (arrow != nullptr)
                {
                    const float aw = float(arrow->image.width);
                    const float ah = float(arrow->image.height);
                    const float cx = pen_x + measured * 0.5f;
                    const float cy = pen_y + em * 0.5f;
                    draw_sprite(arrow, cx - aw * 0.5f, cy - ah * 0.5f, aw, ah);
                }
            }
            x += slot;
            if (c == 0)
            {
                x += 4.f;
            }
        }
    }

    // VirtualKeyboardMenu, built by 0x00014AB0. On 640x480 the anchor is
    // (DAT_00509740 + 100, DAT_0050974C - 16) = (160.5, 415.5). Rows step by
    // DAT_0032B620 (-28.8). 0x0001428F0's width is the widest string plus
    // cell*0.65 + 20, and that opens the second column.
    // Codes: 0 A select, 1 B exit, 2 X change mode, 7 d-pad move, 8 triggers caret.
    // Face buttons stay in the left column, d-pad and triggers in the right,
    // lowest code on the anchor and the rest stacked upward.
    struct Prompt
    {
        int code;
        const char* key;
    };
    constexpr Prompt kPrompts[] = {
        {0, "virtualkeyboard_select"},
        {7, "virtualkeyboard_move"},
        {1, "virtualkeyboard_exit"},
        {8, "virtualkeyboard_movecaret"},
        {2, "virtualkeyboard_changemode"},
    };
    constexpr float kPrompt = 0.65f;
    constexpr float kAnchorX = 160.5f;
    constexpr float kAnchorY = 415.5f;
    constexpr float kRowStep = -28.8f;
    float widest = 0.f;
    for (const Prompt& prompt : kPrompts)
    {
        widest = std::max(widest, text_width(font, gameselect_string(g, prompt.key), kPrompt));
    }
    const float column = widest + float(font.cell()) * kPrompt + 20.f;
    const float prompt_em = float(font.cell()) * kPrompt;
    int left_row = 0;
    int right_row = 0;
    // Stable order is by icon code, not the file order of the five records.
    int order[5] = {0, 1, 2, 3, 4};
    std::sort(order, order + 5, [&](int a, int b) { return kPrompts[a].code < kPrompts[b].code; });
    for (int index : order)
    {
        const Prompt& prompt = kPrompts[index];
        const std::u16string part = gameselect_string(g, prompt.key);
        if (part.empty())
        {
            continue;
        }
        const bool right = prompt.code >= 7;
        const int row = right ? right_row++ : left_row++;
        const float text_x = kAnchorX + (right ? column : 0.f);
        const float mid_y = kAnchorY + float(row) * kRowStep;
        const float text_y = mid_y - prompt_em * 0.5f;
        if (const RgbaImage* icon = joypad_icon(g, prompt.code))
        {
            // DAT_0032B264 (0.7) is stored on the prompt child at +0x18.
            constexpr float kIcon = 0.7f;
            const float iw = float(icon->width) * kIcon;
            const float ih = float(icon->height) * kIcon;
            gfx_sprite_submit(*icon, text_x - iw - 4.f, mid_y - ih * 0.5f, iw, ih);
        }
        draw_glyphs(font, part, text_x, text_y, kPrompt, frame);
    }
}

void draw_contents(const Game& g);

constexpr const char* kContentsMovies[] = {
    "aid_movie_ghoulies_gameselect_house",
    "aid_movie_ghoulies_gameselect_zombieclap",
    "aid_movie_ghoulies_gameselect_skellyscratchhead",
};

std::u16string utf8_to_u16(std::string_view utf8)
{
    std::u16string out;
    for (size_t i = 0; i < utf8.size();)
    {
        const unsigned char c = static_cast<unsigned char>(utf8[i]);
        char32_t cp = c;
        size_t n = 1;
        if (c >= 0xF0 && i + 3 < utf8.size())
        {
            cp = (char32_t(c & 0x07) << 18) | (char32_t(utf8[i + 1] & 0x3F) << 12) |
                 (char32_t(utf8[i + 2] & 0x3F) << 6) | char32_t(utf8[i + 3] & 0x3F);
            n = 4;
        }
        else if (c >= 0xE0 && i + 2 < utf8.size())
        {
            cp = (char32_t(c & 0x0F) << 12) | (char32_t(utf8[i + 1] & 0x3F) << 6) | char32_t(utf8[i + 2] & 0x3F);
            n = 3;
        }
        else if (c >= 0xC0 && i + 1 < utf8.size())
        {
            cp = (char32_t(c & 0x1F) << 6) | char32_t(utf8[i + 1] & 0x3F);
            n = 2;
        }
        if (cp > 0xFFFF)
        {
            cp -= 0x10000;
            out.push_back(char16_t(0xD800 + (cp >> 10)));
            out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
        }
        else
        {
            out.push_back(char16_t(cp));
        }
        i += n;
    }
    return out;
}

// aid_loctext_ghoulies_gameselect_gameselecttext. 0x000FF120 joins "gameselect__" + key.
std::u16string gameselect_string(const Game& g, std::string_view key)
{
    static std::map<std::string, std::u16string> table;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "loctext" / "english" / "04d74e34";
        if (std::filesystem::exists(path))
        {
            try
            {
                const LocTable parsed = parse_loctext(read_file(path), path.filename().string());
                for (const auto& [k, v] : parsed.keys_to_utf8)
                {
                    table.emplace(k, utf8_to_u16(v));
                }
            }
            catch (const std::exception& ex)
            {
                std::cerr << "gameselect loctext " << ex.what() << "\n";
            }
        }
    }
    std::string full = "gameselect__";
    full.append(key);
    const auto it = table.find(full);
    if (it == table.end())
    {
        return {};
    }
    return it->second;
}

// Xbox 0x000FF0A0, saved-game callers use the supportsavedgame namespace.
std::u16string savedgame_string(const Game& g, std::string_view key)
{
    static std::map<std::string, std::u16string> strings;
    static bool loaded = false;
    if (!loaded)
    {
        loaded = true;
        const auto path = g.aids.game_dir / "loctext" / "english" / "070611e5";
        if (std::filesystem::exists(path))
        {
            const LocTable parsed = parse_loctext(read_file(path), "supportsavedgame");
            for (const auto& [name, value] : parsed.keys_to_utf8)
            {
                strings.emplace(name, utf8_to_u16(value));
            }
        }
    }
    const auto found = strings.find("supportsavedgame__" + std::string(key));
    return found == strings.end() ? u"ERROR" : found->second;
}

// Xbox 0x000DCFC0.
std::u16string default_new_game_name(const Game& g)
{
    return savedgame_string(g, "defaultnewgamename");
}

float text_width(const GloopyFont& font, std::u16string_view text, float scale)
{
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    return width;
}

float banner_delay(unsigned& seed);

// Xbox 0x00060070. 0x0005FC30 calls it once the glyph delay at +0x204 has
// counted down. remaining is the life copy at +0x400, which starts at the
// life 0x0005F5C0 stored and then counts down. sticky is widget+0x121C.
// Gameselect passes EAX = 1 into 0x0005F5C0, so that flag is set and the
// life is 60 (0x42700000) instead of the caller's 1.0. The pool step at
// +0x20 is DAT_0032B0F4 (0.5). Glyph frame count is texture byte +0x11.
int goo_glyph_frame(float remaining, float life, float step, int count, int rate, bool sticky)
{
    if (!sticky && remaining < 0.001f)
    {
        return 0;
    }
    if (life != 0.f)
    {
        if (step <= 0.f)
        {
            step = float(count) / float(rate);
        }
        if (life - step <= remaining)
        {
            const float span = (1.f / step) * float(count);
            if (span == 0.f)
            {
                return 0;
            }
            return (int((life - remaining) * span) % count) & 0xff;
        }
        if (remaining <= step && !sticky)
        {
            // Xbox 0x000602E0.
            const float span = (1.f / step) * float(count);
            if (span != 0.f)
            {
                return (int(span * remaining) % count) & 0xff;
            }
            return 0;
        }
    }
    return count - 1;
}

// Host storage for the glyph arrays in one 0x0005F5C0 widget.
using GooLine = Game::MenuText;

unsigned& goo_seed()
{
    static unsigned seed = 0;
    return seed;
}

// Xbox 0x0005F5C0, sticky menu-string initialization.
void arm_goo(GooLine& line, const std::u16string& text, float, int slot)
{
    line.text = text;
    line.slot = slot;
    line.glyphs.assign(text.size(), GooLine::Glyph{});
    line.draw_frame = ~u32(0);
    line.draw_age = 0.f;
    line.sticky = true;
    line.removed = false;
    unsigned& seed = goo_seed();
    for (GooLine::Glyph& glyph : line.glyphs)
    {
        glyph.delay = banner_delay(seed);
    }
}

// Xbox 0x0005F970 / 0x0005F7E0: end a widget using the pool's +0x20
// step and its glyph ages at +0xA04, then clear the sticky flag.
void end_goo(GooLine& line)
{
    for (GooLine::Glyph& glyph : line.glyphs)
    {
        const float duration = banner_delay(goo_seed()) + 0.5f;
        if (duration <= glyph.age)
        {
            if (duration <= glyph.remaining)
            {
                glyph.remaining = duration;
            }
        }
        else
        {
            glyph.remaining = glyph.age;
        }
    }
    line.sticky = false;
}

// Xbox 0x0005FC30. Delay and life are separate countdowns: crossing a delay
// boundary does not spend the excess time on life in the same draw.
void tick_goo(const GooLine& line, const GloopyFont& font, float dt, u32 frame)
{
    if (line.draw_frame == frame)
    {
        return;
    }
    line.draw_frame = frame;
    size_t expired = 0;
    for (size_t i = 0; i < line.glyphs.size(); ++i)
    {
        GooLine::Glyph& glyph = line.glyphs[i];
        if (glyph.delay == 0.f)
        {
            const int count = font.glyph_frame_count(line.text[i]);
            glyph.frame = count == 0 ? -1 :
                goo_glyph_frame(glyph.remaining, 60.f, 0.5f, count,
                                font.glyph_frame_rate(line.text[i]), line.sticky);
            if (0.f < glyph.remaining)
            {
                glyph.remaining -= dt;
                if (glyph.remaining <= 0.f)
                {
                    glyph.remaining = 0.f;
                }
            }
            glyph.age += dt;
        }
        else if (0.f < glyph.delay)
        {
            glyph.delay -= dt;
            if (glyph.delay <= 0.f)
            {
                glyph.delay = 0.f;
            }
        }
        if (glyph.remaining < 0.001f && !line.sticky)
        {
            ++expired;
            glyph.frame = 0;
        }
    }
    if (expired == line.glyphs.size())
    {
        line.removed = true;
        return;
    }
    line.draw_age += dt;
}

int goo_frame(const GooLine& line, size_t index, const Game& g)
{
    tick_goo(line, gloopy_font(g), scene_sim_step(g.clock.dt), g.frame);
    return line.glyphs[index].frame;
}

// One widget per key, born when the keyboard opens (0x00016EF0) or when
// 0x00015D60 rebuilds a letter after shift or caps changes the label.
int keyboard_glyph_frame(const Game& g, int page, int row, int col, const std::u16string& label, size_t index)
{
    auto& slots = g.keyboard_text;
    if (g.keyboard_text_generation != g.keyboard_generation)
    {
        g.keyboard_text_generation = g.keyboard_generation;
        for (int p = 0; p < kKbPages; ++p)
        {
            for (int r = 0; r < kKbRows; ++r)
            {
                for (int c = 0; c < kKb[p][r].count && c < 12; ++c)
                {
                    arm_goo(slots[p][r][c], key_label(g, kKb[p][r].keys[c].code), g.time, -1);
                }
            }
        }
    }
    if (page < 0 || page >= kKbPages || row < 0 || row >= kKbRows || col < 0 || col >= 12)
    {
        return 0;
    }
    GooLine& slot = slots[page][row][col];
    if (slot.text != label)
    {
        arm_goo(slot, label, g.time, -1);
    }
    return goo_frame(slot, index, g);
}

// 0x00015A10 replaces the name widget. A caret move does not.
int keyboard_name_frame(const Game& g, const std::u16string& name, size_t index)
{
    GooLine& line = g.keyboard_name;
    if (g.keyboard_name_generation != g.keyboard_generation || line.text != name)
    {
        arm_goo(line, name, g.time, -1);
        g.keyboard_name_generation = g.keyboard_generation;
    }
    return goo_frame(line, index, g);
}

void draw_goo_line(const GloopyFont& font, const GooLine& line, float pen_x, float pen_y, float scale, const Game& g,
                   bool center)
{
    tick_goo(line, font, scene_sim_step(g.clock.dt), g.frame);
    if (line.removed || font.cell() <= 0 || scale == 0.f)
    {
        return;
    }
    float x = pen_x;
    if (center)
    {
        x -= text_width(font, line.text, scale) * 0.5f;
    }
    const float em = float(font.cell()) * scale;
    for (size_t i = 0; i < line.text.size(); ++i)
    {
        const char16_t ch = line.text[i];
        const int frame = goo_frame(line, i, g);
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

// Contents widget, mode-table index 3. 0x000E02D0 sets the scene object's +0x94
// to 3 when the chosen slot's status is 2, 3, or 4, and that index's draw is
// 0x000D9640. 0x000D8B10 walks three records of 0x104 from 0x00428D04 and the
// layout table at 0x00306994 (stride 0x1C): centers (130, 200), (320, 200),
// (510, 200), rest norm 0.16, hot norm 0.25. The background at +0xC88 is the
// highlighted row's texture, centered, scaled up to the frame. 0x000D9D10
// lerps only the highlighted plate toward the hot norm. The save name sits at
// screen center + (0, -185) = (320, 55), scale 1.8. The highlighted row's
// loctext is the wide string at slot+4, placed at (320, 278), scale 1.4.
// The chapter line is the buffer at +0xA78, anchor (320, 330), scale 0.75.
// 0x000D8CD0 copies "Progress not set" from 0x00324CB4 into that buffer.
// 0x000DA040 overwrites it only after 0x00010DB10 copies a script id out of the
// save list. 0x0004B4D0 matches aid_script_ghoulies_chapter, the chapter number
// is the dword at [index*4 + 0x002FE698], and 0x00011EC70 supplies the scene
// number (a 0 is replaced with 1). Those two counts are formatted with the
// wide "%s %d" at 0x00324C50 onto gameslotcontents_chapter and
// gameslotcontents_scene, then joined with the wide "%s%s : %s" at 0x00324C3C
// and the scene name from 0x00011ED80. No save list means no id is copied, so
// no chapter number is written and the buffer stays the init literal.
#include "frontend_progress.inl"

// Xbox 0x0011EBC0. The chapter suffix identifies the length of the prefix
// ending in "_scene"; atol reads the scene number immediately after it.
int script_scene_number(std::string_view script)
{
    const int chapter = script_chapter_index(script);
    if (chapter == 0)
    {
        return 0;
    }
    constexpr std::string_view suffixes[] = {"void", "1", "2a", "2b", "2c", "3a", "3b", "3c", "4", "5"};
    const std::string prefix = "aid_script_ghoulies_chapter" + std::string(suffixes[chapter]) + "_scene";
    return int(std::atol(std::string(script.substr(prefix.size())).c_str()));
}

// Xbox 0x0011EC70.
int script_chapter_scene(std::string_view script)
{
    const int chapter = script_chapter_index(script);
    int scene = script_scene_number(script);
    switch (chapter)
    {
    case 2:
    case 3:
    case 4:
        for (int i = 2; i < chapter; ++i)
        {
            scene += kChapterSceneCounts[i];
        }
        break;
    case 5:
    case 6:
    case 7:
        for (int i = 5; i < chapter; ++i)
        {
            scene += kChapterSceneCounts[i];
        }
        break;
    }
    return scene;
}

// Xbox 0x0004B530.
int script_chapter_first_scene(int chapter)
{
    int scene = 0;
    for (int i = 1; i < chapter; ++i)
    {
        scene += kChapterSceneCounts[i];
    }
    return scene;
}

// Xbox 0x0011ED30.
int script_scene_index(std::string_view script)
{
    const int scene = script_scene_number(script);
    const int chapter = script_chapter_index(script);
    if (chapter == 0)
    {
        return -1;
    }
    if (scene != 0)
    {
        return script_chapter_first_scene(chapter) - 1 + scene;
    }
    return script_chapter_first_scene(chapter);
}

// Xbox 0x0011ED80: the ordinal maps through two separate tables before
// 0x000FF120 resolves the scenenames text key.
std::u16string script_scene_name(const Game& g, int scene)
{
    static std::map<std::string, std::u16string> strings;
    static bool loaded = false;
    if (!loaded)
    {
        const auto path = g.aids.game_dir / "loctext" / "english" /
                          aid_hash_hex(aid_hash("aid_loctext_ghoulies_script_scenenames"));
        const LocTable parsed = parse_loctext(read_file(path), "scenenames");
        for (const auto& [key, text] : parsed.keys_to_utf8)
        {
            strings.emplace(key, utf8_to_u16(text));
        }
        loaded = true;
    }
    const auto found = strings.find("scenenames__" + std::string(kSceneNameKeys[kSceneNameIndices[scene]]));
    return found == strings.end() ? std::u16string{} : found->second;
}

// Xbox 0x000DA040: retain the initialized buffer when either label lookup
// fails. The host save-list lookup exposes the selected blob's +0x102.
std::u16string contents_progress_line(const Game& g)
{
    constexpr char16_t kLiteral[] = u"Progress not set";
    const std::u16string chapter_label = gameselect_string(g, "gameslotcontents_chapter");
    if (chapter_label.empty())
    {
        return kLiteral;
    }
    const std::u16string scene_label = gameselect_string(g, "gameslotcontents_scene");
    if (scene_label.empty())
    {
        return kLiteral;
    }
    const char* script = save_script(g);
    if (script == nullptr)
    {
        return kLiteral;
    }
    int scene = script_chapter_scene(script);
    if (scene == 0)
    {
        scene = 1;
    }
    const int chapter = slot_chapter(g, g.slot);
    return chapter_label + u" " + utf8_to_u16(std::to_string(chapter)) +
           scene_label + u" " + utf8_to_u16(std::to_string(scene)) + u" : " +
           script_scene_name(g, script_scene_index(script));
}

// Xbox 0x001421C0 / 0x00142720 / 0x00142AF0: mode 1 alternates
// columns, with icon centres and a separate offset for the text widgets.
void draw_menu_prompts(const Game& g, const GloopyFont& font,
                       const GooLine* prompts, const int* codes, int count,
                       float anchor_x, float anchor_y, float icon_scale,
                       float text_scale, float text_offset_x, float text_offset_y,
                       float column)
{
    for (int i = 0; i < count; ++i)
    {
        const float x = anchor_x + float(i % 2) * column;
        const float y = anchor_y - float(i / 2) * 32.f * icon_scale;
        if (const RgbaImage* icon = joypad_icon(g, codes[i]))
        {
            const float width = float(icon->width) * icon_scale;
            const float height = float(icon->height) * icon_scale;
            gfx_sprite_submit(*icon, x - width * 0.5f, y - height * 0.5f, width, height);
        }
        draw_goo_line(font, prompts[i], x + text_offset_x, y + text_offset_y,
                      text_scale, g, false);
    }
}

void draw_contents(const Game& g)
{
    struct Plate
    {
        const char* texture;
        const char* label;
        float center_x;
    };
    constexpr Plate kPlates[3] = {
        {"gameselect_house", "gameslotcontents_playgame", 130.f},
        {"gameselect_zombieclapframe", "gameslotcontents_features", 320.f},
        {"gameselect_skellyscratchheadframe", "gameslotcontents_challenges", 510.f},
    };
    const int hot = g.contents_row >= 0 && g.contents_row < 3 ? g.contents_row : 0;
    const Plate& shown = kPlates[hot];
    // Xbox 0x000D9640 / 0x000DA1E0: the backdrop is a texture, not a movie.
    draw_menu_backdrop(g, g.contents_visual);
    for (int i = 0; i < 3; ++i)
    {
        if (i == hot)
        {
            continue;
        }
        const float w = 0.16f * kFrameW;
        const float h = 0.16f * kFrameH;
        if (const Game::UiImage* image = find_ui(g, kPlates[i].texture))
        {
            draw_plate_background(kPlates[i].center_x, 200.f, w + 12.f, h + 12.f, 0x80069764u);
            draw_sprite(image, kPlates[i].center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
        }
    }
    {
        const float fraction = g.contents_visual.fraction;
        const float w = ((0.25f - 0.16f) * fraction + 0.16f) * kFrameW;
        const float h = ((0.25f - 0.16f) * fraction + 0.16f) * kFrameH;
        if (const Game::UiImage* image = find_ui(g, shown.texture))
        {
            const float padding = (20.f - 12.f) * fraction + 12.f;
            draw_plate_background(shown.center_x, 200.f, w + padding, h + padding,
                                  menu_color(0xC43EF7B6u, 0x80069764u, fraction));
            draw_sprite(image, shown.center_x - w * 0.5f, 200.f - h * 0.5f, w, h);
        }
        // Xbox 0x000D9640: +0x2B0 >= 7 copies the selected movie; +0x2B4
        // latches the first successful copy. Other rows draw their static textures.
        movie_note_visible(kContentsMovies[hot]);
        if (7 <= g.contents_movie_frames[hot])
        {
            const bool copied = movie_copy_frame(kContentsMovies[hot], g.contents_movie_images[hot]);
            if (!g.contents_movie_copied[hot] && copied)
            {
                g.contents_movie_copied[hot] = true;
            }
        }
        if (movie_is_open(kContentsMovies[hot]))
        {
            if (g.contents_movie_copied[hot])
            {
                // DAT_0032B284 = 7, DAT_0032B250 = 0.05. The Xbox
                // conversion truncates the clamped byte before submission.
                const float alpha = float(u8(std::clamp(float(g.contents_movie_frames[hot] - 7) *
                                                       0.05f * 255.f, 0.f, 255.f))) / 255.f;
                gfx_sprite_submit(g.contents_movie_images[hot], shown.center_x - w * 0.5f,
                                  200.f - h * 0.5f, w, h, 0.f, 0.f, 1.f, 1.f,
                                  {1.f, 1.f, 1.f, alpha});
            }
            if (g.contents_movie_frames[hot] < 100)
            {
                ++g.contents_movie_frames[hot];
            }
        }
    }

    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    const std::u16string label = gameselect_string(g, shown.label);
    const std::u16string chapter = contents_progress_line(g);
    auto& text_pool = g.contents_text;
    int& last_row = text_pool.selection;
    GooLine& name_line = text_pool.name;
    GooLine& label_line = text_pool.title;
    GooLine& chapter_line = text_pool.chapter;
    auto& departing_labels = text_pool.departing_names;
    auto& departing_chapters = text_pool.departing_chapters;
    u32& label_generation = text_pool.generation;
    if (label_generation != g.contents_visual.generation)
    {
        if (!label_line.text.empty())
        {
            end_goo(label_line);
            departing_labels.push_back(label_line);
        }
        if (!chapter_line.text.empty())
        {
            end_goo(chapter_line);
            departing_chapters.push_back(chapter_line);
        }
        label_line = GooLine{};
        chapter_line = GooLine{};
        label_generation = g.contents_visual.generation;
    }
    if (name_line.slot != g.slot || name_line.text != name)
    {
        arm_goo(name_line, name, g.time, g.slot);
    }
    if (g.contents_visual.fraction >= 1.f && (last_row != hot || label_line.text != label))
    {
        arm_goo(label_line, label, g.time, hot);
    }
    if (g.contents_visual.fraction >= 1.f && hot == 0 && chapter_line.text != chapter)
    {
        arm_goo(chapter_line, chapter, g.time, -1);
    }
    last_row = hot;
    draw_goo_line(font, name_line, 320.f, 55.f, 1.8f, g, true);
    for (const GooLine& line : departing_labels)
    {
        draw_goo_line(font, line, 320.f, 278.f, 1.4f, g, true);
    }
    for (const GooLine& line : departing_chapters)
    {
        draw_goo_line(font, line, 320.f, 330.f, 0.75f, g, true);
    }
    std::erase_if(departing_labels, [](const GooLine& line)
    {
        return line.removed;
    });
    std::erase_if(departing_chapters, [](const GooLine& line)
    {
        return line.removed;
    });
    draw_goo_line(font, label_line, 320.f, 278.f, 1.4f, g, true);
    draw_goo_line(font, chapter_line, 320.f, 330.f, 0.75f, g, true);

    // 0x000DAA37 calls 0x000142180 with the prompt set for the highlighted row.
    // 0x0001428F0 walks that set in record order, stride 0x48.
    // Play (0x003069F0): select, move, back, info.
    // +0xF24 == 3 would select 0x00306D00 instead, which has no select entry.
    // 0x000DA040 leaves +0xF24 at 2 when 0x00010DC60 finds no save record.
    // Features (0x00307010) and Challenges (0x00307320): select, move, back.
    const char* prompt_keys[4] = {
        "gameslotcontents_selectprompt",
        "gameslotcontents_moveprompt",
        "gameslotcontents_backprompt",
        nullptr,
    };
    int prompt_count = 3;
    if (hot == 0)
    {
        prompt_keys[3] = "gameslotcontents_infoprompt";
        prompt_count = 4;
    }
    auto& prompts = text_pool.prompts;
    float column = 0.f;
    const char* all_keys[] = {
        "gameslotcontents_selectprompt", "gameslotcontents_moveprompt",
        "gameslotcontents_backprompt", "gameslotcontents_infoprompt",
    };
    for (const char* key : all_keys)
    {
        column = std::max(column, text_width(font, gameselect_string(g, key), 0.76f));
    }
    // 0x001428F0 returns maximum text width + 32 * icon scale + 20.
    column += 32.f * 0.85f + 20.f;
    for (int i = 0; i < prompt_count; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
    }
    const int codes[] = {0, 7, 1, 3};
    // 0x000DA580 uses the outer safe rectangle, scale .85 / .76.
    draw_menu_prompts(g, font, prompts, codes, prompt_count,
                      64.f, 428.f, 0.85f, 0.76f, 17.6f, -13.6f, column);
}

// 0x000DF9A0 while the widget mode is 6 places the selected slot's wide name
// through 0x0005F5C0. The anchor is the per-slot point init wrote at +0x418:
// screen center plus DAT_00428FA8 (0) and DAT_00428FAC (-185). Scale is
// DAT_00428FA4 (1.8). param_8 = 1, so the line is centered on that point.
// The title is the same call with DAT_00428FB0 (1) at center + (0, 50).
// The prompt bar's y is DAT_0050974C - 16. On 640x480, 0x000FC190 stores
// that edge at 431.5, and the three prompt strings use DAT_00429024 (0.75).
void draw_slot_select_copy(const Game& g)
{
    const GloopyFont& font = gloopy_font(g);
    std::u16string name;
    if (g.slot_name_len[g.slot] > 0)
    {
        name.assign(g.slot_name[g.slot], g.slot_name[g.slot] + g.slot_name_len[g.slot]);
    }
    else
    {
        name = savedgame_string(g, "emptyslotname");
    }

    if (g.slot_status[g.slot] == 3)
    {
        name = savedgame_string(g, "damagedgamename");
    }

    // 0x000DF9A0 builds these widgets again on the way into mode 6, including
    // each time the slot changes. The title and the prompt bar stay put.
    auto& text_pool = g.slot_text;
    const bool entered = text_pool.generation == ~u32(0);
    GooLine& slot_name = text_pool.name;
    GooLine& title = text_pool.title;
    auto& prompts = text_pool.prompts;
    u32& name_generation = text_pool.generation;
    auto& departing_names = text_pool.departing_names;
    if (name_generation != g.slot_visual.generation)
    {
        if (!slot_name.text.empty())
        {
            end_goo(slot_name);
            departing_names.push_back(slot_name);
        }
        slot_name = GooLine{};
        name_generation = g.slot_visual.generation;
    }
    if (g.slot_visual.fraction >= 1.f && (entered || slot_name.slot != g.slot || slot_name.text != name))
    {
        arm_goo(slot_name, name, g.time, g.slot);
    }
    const std::u16string title_text = gameselect_string(g, "gameslotselection_instructions");
    if (entered || title.text != title_text)
    {
        arm_goo(title, title_text, g.time, -1);
    }
    const char* prompt_keys[3] = {
        "gameslotselection_selectprompt",
        "gameslotselection_moveprompt",
        "gameslotselection_backtoattract",
    };
    for (int i = 0; i < 3; ++i)
    {
        const std::u16string part = gameselect_string(g, prompt_keys[i]);
        if (entered || prompts[i].text != part)
        {
            arm_goo(prompts[i], part, g.time, -1);
        }
    }

    for (const GooLine& line : departing_names)
    {
        draw_goo_line(font, line, 320.f, 55.f, 1.8f, g, true);
    }
    std::erase_if(departing_names, [](const GooLine& line)
    {
        return line.removed;
    });
    draw_goo_line(font, slot_name, 320.f, 55.f, 1.8f, g, true);
    draw_goo_line(font, title, 320.f, 290.f, 1.f, g, true);

    float column = 0.f;
    for (const char* key : prompt_keys)
    {
        column = std::max(column, text_width(font, gameselect_string(g, key), 0.75f));
    }
    column = std::max(column, text_width(font,
        gameselect_string(g, "gameslotselection_deleteprompt"), 0.75f));
    column += 52.f;
    const int codes[] = {0, 7, 1};
    draw_menu_prompts(g, font, prompts, codes, 3,
                      76.5f, 415.5f, 1.f, 0.75f, 20.f, -16.f, column);
}

// Xbox 0x000FF120 builds "%s__%s" from the two cstrings at 0x00323EE4 / 0x00323EEC.
std::u16string attract_start_banner(const Game& g)
{
    static std::u16string text;
    static bool loaded = false;
    if (loaded)
    {
        return text;
    }
    loaded = true;
    // aid_loctext_ghoulies_attract_attracttext
    const auto path = g.aids.game_dir / "loctext" / "english" / "0bbbe8e4";
    if (!std::filesystem::exists(path))
    {
        return text;
    }
    try
    {
        const LocTable table = parse_loctext(read_file(path), path.filename().string());
        const auto it = table.keys_to_utf8.find("attract__StartBanner");
        if (it != table.keys_to_utf8.end())
        {
            text = utf8_to_u16(it->second);
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << "start banner loctext " << ex.what() << "\n";
    }
    return text;
}

// 0x0005F6E2. Delay is (rng_float in [1, 2) - 1) * DAT_0032B104.
float banner_delay(unsigned& seed)
{
    seed = seed * 0x19660Du + 0x3C6EF35Fu;
    const float unit = std::bit_cast<float>((seed & 0x7fffffu) | 0x3f800000u);
    return (unit - 1.f) * 0.25f;
}

// Xbox 0x00060070. The text pool's step at +0x20 is 0.5 (0x0005F40A, DAT_0032B0F4).
// Life is the 4.0 passed by 0x000809C0. edi (widget+0x121C) is 0, so a spent life
// returns frame 0. Otherwise one forward pass, the last frame, then one pass back.
int banner_glyph_frame(float age, float delay, int count)
{
    constexpr float kLife = 4.f;
    constexpr float kStep = 0.5f;
    constexpr float kEps = 0.001f;
    if (count <= 1)
    {
        return 0;
    }
    const float t = age - delay;
    if (t < 0.f)
    {
        return 0;
    }
    const float remaining = kLife - t;
    if (remaining <= kEps)
    {
        return 0;
    }
    if (remaining >= kLife - kStep)
    {
        return int(t * float(count) / kStep) % count;
    }
    if (kStep < remaining)
    {
        return count - 1;
    }
    return int(remaining * float(count) / kStep) % count;
}

// 0x0004CE26, type 0x15. The name at +0x10 is joined as "dialogs__<name>"
// (0x000FF0A0) and looked up in aid_loctext_ghoulies_dialog_dialogs.
// {page} starts another screen. {nobutton} hides the A prompt. {settime}
// is the attract hold; a line without it waits on A (0x000584D0).
struct DialogPage
{
    std::u16string text;
    std::string speaker;
    std::string cue;
    bool button = true;
};

struct MoveCaption
{
    float start = 0.f;
    float seconds = 0.f;
    // 0x000589F0 stores DAT_0032B248 (0.6). {scale} replaces it (0x000570C2).
    float scale = 0.6f;
    bool head = true;
    bool pause_audio = true;
    // Type 0x15 record +0xD0. 0x0004C4F0 plays it when the hold ends.
    std::string next_cue;
    std::string motif;
    u8 target = 0;
    std::vector<DialogPage> pages;
};

int g_caption_hold = -1;
int g_caption_page = 0;
float g_caption_left = 0.f;
bool g_caption_wait = false;
DialogueScreen g_dialogue_screen;
int g_dialogue_mode = 0;
float g_dialogue_timer = 0.f;
float g_dialogue_fade = 0.f;
std::string g_dialogue_motif;

void reset_caption_hold()
{
    g_dialogue_screen.close();
    g_dialogue_mode = 0;
    pcm_dialog_hold(false);
    if (!g_dialogue_motif.empty())
    {
        pcm_stop_cue(g_dialogue_motif.c_str());
        g_dialogue_motif.clear();
    }
    g_caption_hold = -1;
    g_caption_page = 0;
    g_caption_left = 0.f;
    g_caption_wait = false;
}

float tag_number(std::string_view tag)
{
    const size_t sp = tag.find(' ');
    if (sp == std::string_view::npos || sp + 1 >= tag.size())
    {
        return 0.f;
    }
    return std::strtof(std::string(tag.substr(sp + 1)).c_str(), nullptr);
}

bool speaker_token(std::string_view tag)
{
    // Tokens below SpeakerMAX (index 0x12). 0x00056F80 sends these to 0x000576D0.
    static const char* kNames[] = {
        "boy",           "girl",          "butler",        "cook",
        "groundskeeper", "housekeeper",   "krackpot",      "baroness",
        "baron",         "crone",         "piratecaptain", "skeletonbad",
        "mrribs",        "pirate",        "littleguy",     "zombiestandard",
        "hunchback",     "binklogo",
    };
    for (const char* name : kNames)
    {
        if (tag == name)
        {
            return true;
        }
    }
    return false;
}

void parse_dialog_line(std::string_view raw, MoveCaption& cap)
{
    std::string visible;
    std::string speaker;
    std::string cue;
    bool show_button = true;
    auto flush = [&]()
    {
        if (visible.empty())
        {
            return;
        }
        DialogPage page;
        page.text = utf8_to_u16(visible);
        page.speaker = speaker;
        page.cue = cue;
        page.button = show_button;
        cap.pages.push_back(std::move(page));
        visible.clear();
    };
    size_t i = 0;
    while (i < raw.size())
    {
        if (raw[i] != '{')
        {
            visible.push_back(raw[i]);
            ++i;
            continue;
        }
        const size_t end = raw.find('}', i + 1);
        if (end == std::string_view::npos)
        {
            break;
        }
        const std::string_view tag = raw.substr(i + 1, end - (i + 1));
        if (tag.rfind("settime", 0) == 0)
        {
            const float seconds = tag_number(tag);
            if (seconds > 0.f)
            {
                cap.seconds = seconds;
            }
        }
        else if (tag.rfind("scale ", 0) == 0 || tag == "scale")
        {
            const float scale = tag_number(tag);
            if (scale > 0.f)
            {
                cap.scale = scale;
            }
        }
        else if (tag == "page")
        {
            flush();
        }
        else if (tag.rfind("sfx ", 0) == 0)
        {
            // Xbox 0x00057B00 builds the XACT cue name; 0x00057D10 plays it
            // on entry to mode 2, rather than while measuring the text.
            cue = "XACT_SOUNDBANK_" + std::string(tag.substr(4));
        }
        else if (tag == "linek" || tag == "linel" || tag == "linec" || tag == "liner")
        {
            visible.push_back('\n');
        }
        else if (tag == "nobutton")
        {
            // 0x0005709D. DAT_004EE290 = 0, so the accept widget is not drawn.
            show_button = false;
        }
        else if (tag == "nohead")
        {
            // 0x000570A9. DAT_004EE294 = 0, so 0x000575F0 does not draw the head.
            cap.head = false;
        }
        else if (speaker_token(tag))
        {
            speaker = std::string(tag);
        }
        i = end + 1;
    }
    flush();
}

const LocTable& dialog_loctext(const Game& g)
{
    static LocTable table;
    static bool loaded = false;
    if (loaded)
    {
        return table;
    }
    loaded = true;
    // aid_loctext_ghoulies_dialog_dialogs
    const auto path = g.aids.game_dir / "loctext" / "english" / "0c50e153";
    if (!std::filesystem::exists(path))
    {
        return table;
    }
    try
    {
        table = parse_loctext(read_file(path), path.filename().string());
    }
    catch (const std::exception& ex)
    {
        std::cerr << "dialog loctext " << ex.what() << "\n";
    }
    return table;
}

std::string event_cstring(std::span<const u8> rec, size_t at)
{
    if (at >= rec.size())
    {
        return {};
    }
    size_t n = at;
    while (n < rec.size() && rec[n] != 0)
    {
        ++n;
    }
    return std::string(reinterpret_cast<const char*>(rec.data() + at), n - at);
}

const std::vector<MoveCaption>& move_captions(const Game& g)
{
    static std::string clip;
    static std::vector<MoveCaption> lines;
    if (!g.cutscenes.playing || g.cutscenes.index >= g.cutscenes.items.size() || g.scene_bundle == nullptr)
    {
        clip.clear();
        lines.clear();
        reset_caption_hold();
        return lines;
    }
    const std::string& name = g.cutscenes.items[g.cutscenes.index].name;
    if (name == clip)
    {
        return lines;
    }
    clip = name;
    lines.clear();
    reset_caption_hold();
    std::string prefix = name;
    const std::string from = "aid_cutscene_";
    if (prefix.rfind(from, 0) == 0)
    {
        prefix.replace(0, from.size(), "aid_cutsceneevents_");
    }
    const LocTable& table = dialog_loctext(g);
    for (const auto& e : g.scene_bundle->entries)
    {
        if (e.type != ResourceType::CutsceneEvents || e.name.rfind(prefix, 0) != 0)
        {
            continue;
        }
        if (e.name.size() > prefix.size() && e.name[prefix.size()] != '_')
        {
            continue;
        }
        size_t pos = 0;
        while (pos + 16 <= e.cpu.size())
        {
            const u32 size = read_u32(e.cpu, pos);
            if (size < 16 || pos + size > e.cpu.size())
            {
                break;
            }
            const auto rec = e.cpu.subspan(pos, size);
            const u32 type = read_u32(rec, 0x0c) & 0xffff;
            if (type == 0x15 && rec.size() > 0x10)
            {
                const std::string key = "dialogs__" + event_cstring(rec, 0x10);
                const auto it = table.keys_to_utf8.find(key);
                if (it != table.keys_to_utf8.end())
                {
                    MoveCaption cap;
                    // Attract bypasses the global pause in 0x000584D0.
                    // The event's own cutscene wait (0x0004E640) still applies.
                    cap.pause_audio = !scene_control_starts(g, "attract");
                    cap.start = read_f32(rec, 4);
                    // Xbox 0x0004CE26 / 0x0004D510: motif at +0x50,
                    // background byte at +0x150. 0x0004C4F0 releases it.
                    cap.motif = event_cstring(rec, 0x50);
                    if (cap.motif == "XACT_SOUNDBANK_GMISC_NOSOUND")
                    {
                        cap.motif.clear();
                    }
                    cap.target = rec.size() > 0x150 ? rec[0x150] : 0;
                    if (rec.size() > 0xd0)
                    {
                        const std::string cue = event_cstring(rec, 0xd0);
                        if (cue.rfind("XACT_SOUNDBANK_", 0) == 0 && cue != "XACT_SOUNDBANK_GMISC_NOSOUND")
                        {
                            cap.next_cue = cue;
                        }
                    }
                    parse_dialog_line(it->second, cap);
                    if (!cap.pages.empty())
                    {
                        lines.push_back(std::move(cap));
                    }
                }
            }
            pos += size;
        }
    }
    return lines;
}

// Xbox 0x00058C30. 0x00059880 measures the line count at +0x0C. The quad's
// -1/+1 corners cancel the half-size matrix in 0x0005CA60. 0x00060980 adds
// two pixels of spacing, and 0x0005E9F0 draws the five-pixel shadow first.
void draw_dialog_caption(const GloopyFont& font, std::u16string_view text, float scale)
{
    if (text.empty() || font.cell() <= 0 || scale == 0.f || g_dialogue_fade == 0.f)
    {
        return;
    }
    std::vector<std::u16string_view> rows;
    size_t at = 0;
    while (at <= text.size())
    {
        const size_t br = text.find(u'\n', at);
        const size_t end = br == std::u16string_view::npos ? text.size() : br;
        if (end > at)
        {
            rows.push_back(text.substr(at, end - at));
        }
        if (br == std::u16string_view::npos)
        {
            break;
        }
        at = br + 1;
    }
    if (rows.empty())
    {
        return;
    }
    constexpr float kFirst = 2.f;
    constexpr float kLift = 37.5f;
    // 0x58C30 scales both the font state and the centred draw matrix.
    const float draw = scale * g_dialogue_fade * g_dialogue_fade;
    const float spacing = 2.f * g_dialogue_fade;
    const float line_h = float(font.drop()) * draw * 1.1f;
    const float pen_y = (kFrameH - float(rows.size()) * line_h) * 0.5f + line_h * 0.5f;
    const int frame = font.rest_frame();
    for (size_t row = 0; row < rows.size(); ++row)
    {
        const std::u16string_view line = rows[row];
        const float y = pen_y + line_h * float(row);
        // 0x000596DB consumes the first-letter flag. 0x00057043 ({linek})
        // sets the line-break/alignment flags without re-arming that flag.
        const float big = row == 0 ? draw * kFirst : draw;
        float width = float(font.advance(line.front())) * big + spacing;
        for (size_t i = 1; i < line.size(); ++i)
        {
            width += float(font.advance(line[i])) * draw + spacing;
        }
        float x = (kFrameW - width) * 0.5f;
        for (size_t i = 0; i < line.size(); ++i)
        {
            const float size = i == 0 ? big : draw;
            const float gy = row == 0 && i == 0 ? y - kLift * draw : y;
            if (const RgbaImage* glyph = font.image(line[i], frame))
            {
                const float em = float(font.cell()) * size;
                const float gx = x - float(font.bearing(line[i])) * size;
                const float shadow = 5.f * g_dialogue_fade * g_dialogue_fade;
                gfx_sprite_submit(*glyph, gx + shadow, gy + shadow, em, em, 0.f, 0.f, 1.f, 1.f,
                                  {0.f, 0.f, 0.f, 128.f / 255.f});
                gfx_sprite_submit(*glyph, gx, gy, em, em, 0.f, 0.f, 1.f, 1.f,
                                  {1.f, 1.f, 1.f, 1.f});
            }
            x += float(font.advance(line[i])) * size + spacing;
        }
    }
}

// 0x000576D0 builds aid_texture_ghoulies_scoreboard_dialogheads_<token>
// (format at 0x00323588) and 0x000575F0 draws it. The position constants are
// DAT_0032B564 (320) and DAT_0032B2F0 (80). 0x0005CA60 centres the quad.
struct DialogHeadFrames
{
    std::vector<RgbaImage> frames;
};

const DialogHeadFrames& dialog_head_frames(const Game& g, const std::string& speaker)
{
    static std::map<std::string, DialogHeadFrames> cache;
    const auto found = cache.find(speaker);
    if (found != cache.end())
    {
        return found->second;
    }
    DialogHeadFrames result;
    const std::string name = "aid_texture_ghoulies_scoreboard_dialogheads_" + speaker;
    const auto path = g.aids.game_dir / "demand" / aid_hash_hex(aid_hash(name));
    if (std::filesystem::exists(path))
    {
        try
        {
            LooseResource file = parse_loose_resource(read_file(path));
            TextureHeader hdr;
            if (parse_texture_header(file.cpu, hdr))
            {
                std::span<const u8> gpu = file.gpu;
                if (!file.gpu_chunks.empty())
                {
                    const auto& chunk = file.gpu_chunks[0];
                    if (size_t(chunk.offset) + chunk.size <= file.gpu.size())
                    {
                        gpu = file.gpu.subspan(chunk.offset, chunk.size);
                    }
                }
                // 0x00057792 reads the frame count at texture+0x11. Each
                // frame occupies data_size bytes, as in the font textures.
                const size_t count = hdr.tile_count;
                for (size_t frame = 0; frame < count; ++frame)
                {
                    const size_t offset = size_t(hdr.texture_offset) + frame * hdr.data_size;
                    if (offset > gpu.size() || hdr.data_size > gpu.size() - offset)
                    {
                        throw Error("dialog head frame out of range");
                    }
                    TextureHeader tile = hdr;
                    tile.texture_offset = 0;
                    result.frames.push_back(decode_xbox_texture(tile, gpu.subspan(offset, hdr.data_size)));
                }
            }
        }
        catch (const std::exception& ex)
        {
            result.frames.clear();
            std::cerr << name << " " << ex.what() << "\n";
        }
    }
    return cache.emplace(speaker, std::move(result)).first->second;
}

struct DialogHeadAnimation
{
    std::string speaker;
    unsigned seed = 0;
    int state = 0;
    int frame = 0;
    float remaining = 0.f;
    u32 tick = static_cast<u32>(-1);
};
DialogHeadAnimation g_dialog_head_animation;

const RgbaImage* dialog_head(const Game& g, const std::string& speaker)
{
    const auto& frames = dialog_head_frames(g, speaker).frames;
    if (frames.empty())
    {
        return nullptr;
    }
    auto& anim = g_dialog_head_animation;
    if (anim.speaker != speaker)
    {
        anim.speaker = speaker;
        anim.state = 0;
        anim.frame = 0;
        anim.remaining = 0.f;
    }
    if (anim.tick != g.frame)
    {
        anim.tick = g.frame;
        // 0x00057450: probabilistic blink, then a 0.1-second forward
        // pass and a 0.1-second reverse pass. This uses the executable's
        // LCG formula with a host-local seed; shared RNG state is not ported.
        const int last = int(frames.size()) - 1;
        if (anim.state == 0)
        {
            anim.seed = anim.seed * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((anim.seed & 0x7fffffu) | 0x3f800000u) - 1.f;
            if (random <= 0.01f)
            {
                anim.state = 1;
                anim.remaining = 0.1f;
            }
        }
        else
        {
            const float fraction = anim.remaining * 10.f;
            anim.frame = int((anim.state == 1 ? 1.f - fraction : fraction) * float(last));
            anim.remaining -= g.clock.dt;
            if (anim.remaining <= 0.f)
            {
                if (anim.state == 1)
                {
                    anim.state = 2;
                    anim.remaining = 0.1f;
                    anim.frame = last;
                }
                else
                {
                    anim.state = 0;
                    anim.remaining = 0.f;
                    anim.frame = 0;
                }
            }
        }
    }
    return &frames[size_t(std::clamp(anim.frame, 0, int(frames.size()) - 1))];
}

void draw_move_captions(const Game& g, bool texture_pass = false)
{
    if (g_caption_hold < 0 || g_dialogue_mode == 0)
    {
        return;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (size_t(g_caption_hold) >= lines.size())
    {
        return;
    }
    const MoveCaption& cap = lines[size_t(g_caption_hold)];
    if (cap.pages.empty())
    {
        return;
    }
    const int page = std::clamp(g_caption_page, 0, int(cap.pages.size()) - 1);
    const DialogPage& shown = cap.pages[size_t(page)];
    const float widget_scale = 1.5f * (g_dialogue_mode == 3 ? 0.f : g_dialogue_fade);
    const bool content = cap.target == 0 || texture_pass;
    if (content)
    {
        g_dialogue_screen.draw(cap.target != 0, !texture_pass && shown.button, widget_scale);
    }
    if (content && cap.head && !shown.speaker.empty())
    {
        if (const RgbaImage* head = dialog_head(g, shown.speaker))
        {
            const float w = float(head->width) * g_dialogue_fade;
            const float h = float(head->height) * g_dialogue_fade;
            gfx_sprite_submit(*head, 320.f - w * 0.5f, 80.f - h * 0.5f, w, h,
                              0.f, 0.f, 1.f, 1.f);
        }
    }
    const RgbaImage* button = nullptr;
    if (!texture_pass && shown.button && widget_scale >= 0.001f)
    {
        if (cap.target != 0)
        {
            g_dialogue_screen.draw_button(widget_scale);
        }
        button = joypad_icon(g, 0);
    }
    if (content)
    {
        draw_dialog_caption(dialog_font(g), shown.text, cap.scale);
    }
    if (button != nullptr)
    {
        const float w = float(button->width) * widget_scale;
        const float h = float(button->height) * widget_scale;
        gfx_sprite_submit(*button, 320.f - w * 0.5f, dialogue_button_y - h * 0.5f, w, h);
    }
}

// Xbox 0x00080A30 creates the attract banner. 0x000809C0 places the widget:
// x = DAT_00509730 (0x000FC190, width/2), y = DAT_0050972C - font[+0x10] * DAT_0032B5B4.
// param_8 = 1, so 0x0005EF40 draws from anchor_x - measured_width/2.
// 0x00080C10 counts DAT_0032B284 (7s) and builds the widget again.
void draw_start_banner(const Game& g)
{
    if (g.scene_name.find("attract") == std::string::npos)
    {
        return;
    }
    const std::u16string text = attract_start_banner(g);
    if (text.empty())
    {
        return;
    }
    const GloopyFont& font = gloopy_font(g);
    const float scale = 1.125f;
    const float inset = std::min(kFrameH * 0.85f, kFrameH * 0.90f);
    const float anchor_y = kFrameH - (kFrameH - inset) * 0.5f;
    const float pen_y = anchor_y - float(font.drop()) * scale;
    float width = 0.f;
    for (char16_t ch : text)
    {
        width += float(font.advance(ch)) * scale;
    }
    const float pen_x = kFrameW * 0.5f - width * 0.5f;

    constexpr float kPeriod = 7.f;
    struct BannerAnim
    {
        float born = -1.f;
        unsigned seed = 1;
        std::vector<float> delay;
    };
    static BannerAnim anim;
    if (anim.born < 0.f || g.time - anim.born >= kPeriod || anim.delay.size() != text.size())
    {
        anim.born = g.time;
        anim.delay.resize(text.size());
        for (float& delay : anim.delay)
        {
            delay = banner_delay(anim.seed);
        }
    }
    const float age = g.time - anim.born;
    const int count = font.rest_frame() + 1;
    const float em = float(font.cell()) * scale;
    float x = pen_x;
    for (size_t i = 0; i < text.size(); ++i)
    {
        const char16_t ch = text[i];
        const int frame = banner_glyph_frame(age, anim.delay[i], count);
        if (const RgbaImage* glyph = font.image(ch, frame))
        {
            const float bx = float(font.bearing(ch)) * scale;
            gfx_sprite_submit(*glyph, x - bx, pen_y, em, em);
        }
        x += float(font.advance(ch)) * scale;
    }
}

} // namespace

// Xbox 0x00058150. Background dialogues render to the texture selected by
// 0x001417A0 before the book samples it; target 0 uses the normal HUD pass.
void prepare_dialogue_texture(const Game& g)
{
    if (g_dialogue_mode == 0 || g_caption_hold < 0)
    {
        return;
    }
    const auto& lines = move_captions(g);
    const u8 slot = lines.at(size_t(g_caption_hold)).target;
    if (slot == 0)
    {
        return;
    }
    const RgbaImage* target = nullptr;
    for (const auto& prop : g.cutscene_props)
    {
        for (const auto& texture : prop.mesh.textures)
        {
            if (texture.movie_slot == slot)
            {
                target = &texture;
            }
        }
    }
    for (const auto& mesh : g.models)
    {
        for (const auto& texture : mesh.textures)
        {
            if (texture.movie_slot == slot)
            {
                target = &texture;
            }
        }
    }
    if (target == nullptr)
    {
        throw Error("dialogue dynamic texture slot missing: " + std::to_string(slot));
    }
    gfx_begin_dialog_texture(slot, target->width, target->height);
    draw_move_captions(g, true);
    gfx_end_dialog_texture();
}

void arm_caption(const Game& g, const MoveCaption& cap)
{
    g_caption_page = 0;
    g_caption_left = 0.f;
    g_caption_wait = false;
    g_dialogue_screen.open(g);
    // Xbox 0x000584D0, mode 1. Mode 2 begins after the 0.3-second fade.
    g_dialogue_mode = 1;
    g_dialogue_timer = 0.3f;
    g_dialogue_fade = 0.f;
    const bool button = !cap.pages.empty() && cap.pages[0].button;
    g_caption_wait = button;
    if (cap.pause_audio)
    {
        pcm_dialog_hold(true);
    }
    if (!cap.motif.empty())
    {
        g_dialogue_motif = cap.motif;
        pcm_play_cue(g, g_dialogue_motif.c_str(), true);
    }
}

void caption_accept(const Game& g)
{
    if (!g_caption_wait || g_dialogue_mode != 2)
    {
        return;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (g_caption_hold < 0 || size_t(g_caption_hold) >= lines.size())
    {
        g_caption_wait = false;
        return;
    }
    // Xbox 0x00058B80 / 0x00057E60: A requests mode 3, not immediate close.
    g_dialogue_mode = 3;
    g_dialogue_timer = 0.3f;
}

float attract_caption_dt(const Game& g, float dt)
{
    // Xbox 0x0004C850 -> 0x0004E640 pauses the event's cutscene at +0x158.
    // 0x0004DA50 releases it only after dialogue mode returns to zero.
    // This wait also applies to title captions; their audio keeps playing.
    // Book scenes leave the menu widget down (mode -1) and still show type 0x15.
    if (g.frontend_mode > 0)
    {
        return dt;
    }
    const std::vector<MoveCaption>& lines = move_captions(g);
    if (g_dialogue_mode != 0)
    {
        // Xbox 0x00057E60. The detached page advances during every hold.
        g_dialogue_screen.tick(dt);
        const MoveCaption& cap = lines.at(size_t(g_caption_hold));
        switch (g_dialogue_mode)
        {
        case 1:
            g_dialogue_fade = std::clamp((g_dialogue_timer - 0.3f) * -3.333333f, 0.f, 1.f);
            g_dialogue_timer -= dt;
            if (g_dialogue_timer <= 0.f)
            {
                g_dialogue_mode = 2;
                g_dialogue_fade = 1.f;
                g_caption_left = cap.seconds > 0.f ? cap.seconds : 0.1f;
                const auto& shown = cap.pages.at(size_t(g_caption_page));
                g_caption_wait = shown.button;
                if (cap.head && !shown.cue.empty())
                {
                    pcm_play_cue(g, shown.cue.c_str(), false);
                }
            }
            break;
        case 2:
            if (!g_caption_wait)
            {
                g_caption_left -= dt;
                if (g_caption_left <= 0.f)
                {
                    g_dialogue_mode = 3;
                    g_dialogue_timer = 0.3f;
                }
            }
            break;
        case 3:
            g_dialogue_fade = std::clamp(1.f - (g_dialogue_timer - 0.3f) * -3.333333f, 0.f, 1.f);
            g_dialogue_timer -= dt;
            if (g_dialogue_timer <= 0.f)
            {
                // Xbox 0x00058AE0: parse the next page or enter mode 4.
                if (g_caption_page + 1 < int(cap.pages.size()))
                {
                    ++g_caption_page;
                    g_dialogue_mode = 1;
                    g_dialogue_timer = 0.3f;
                    g_dialogue_fade = 0.f;
                }
                else
                {
                    g_dialogue_mode = 4;
                }
            }
            break;
        case 4:
            g_dialogue_mode = 0;
            g_caption_wait = false;
            g_caption_left = 0.f;
            g_dialogue_screen.close();
            if (!g_dialogue_motif.empty())
            {
                pcm_stop_cue(g_dialogue_motif.c_str());
                g_dialogue_motif.clear();
            }
            pcm_dialog_advance(g, cap.next_cue.c_str());
            break;
        }
        return 0.f;
    }
    const float time = g.cutscenes.time;
    for (int i = g_caption_hold + 1; i < int(lines.size()); ++i)
    {
        const float start = lines[size_t(i)].start;
        if (time < start && time + dt >= start)
        {
            g_caption_hold = i;
            arm_caption(g, lines[size_t(i)]);
            return start - time;
        }
        if (time >= start)
        {
            g_caption_hold = i;
            arm_caption(g, lines[size_t(i)]);
            return 0.f;
        }
    }
    return dt;
}

void load_ui(Game& g)
{
    g.ui.clear();
    // 0x000DD740 resolves these aids with 0x00035E90. They are files in the
    // scene bundle the gameselect scene control just opened, not a name test.
    if (!g.scene_bundle || !scene_control_is(g, "gameselect"))
    {
        return;
    }
    for (const auto* entry : entries_of_type(*g.scene_bundle, ResourceType::Texture))
    {
        try
        {
            Game::UiImage image;
            image.name = entry->name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
        catch (const std::exception& ex)
        {
            std::cout << "  ui fail " << entry->name << ": " << ex.what() << "\n";
        }
    }
    for (const char* name : kJoypadIconNames)
    {
        if (const BundleEntry* entry = g.aids.load_aid(name))
        {
            Game::UiImage image;
            image.name = name;
            image.image = image_from_entry(*entry);
            g.ui.push_back(std::move(image));
        }
    }
    std::cout << "  ui textures " << g.ui.size() << "\n";
}

// 0x000E5B40, from 0x000DF9A0 case 1 when the slot widget opens. EAX is this
// cue and the id on the stack is 1. 0x00038910 acquires it; 0x000382C0 then
// binds a channel and 0x00038DA0 sets that voice playing. The cue-list row is
// Gstreamsditties / pause: device 3 (Gdvd8), wave 0.
constexpr const char* kSlotOpen = "XACT_SOUNDBANK_GSTREAMSDITTIES_PAUSE";

void begin_frontend(Game& g)
{
    movie_reset();
    // entitySceneControlAttract (registrar 0x00080970). The banner is that object,
    // built because the script names aid_objparams_ghoulies_scenecontrol_attract*.
    if (scene_control_starts(g, "attract"))
    {
        pcm_stop_cue(kSlotOpen);
        g.frontend_mode = 0;
        g.naming = false;
        // A new attract script builds a new banner, so +0x294 starts clear.
        g.attract_banner_latched = false;
        std::cout << "title attract\n";
        return;
    }
    // entitySceneControlGameSelect init 0x00083660. It calls 0x000E5AE0, which
    // installs modes[DAT_00429BC0] when stream id 2 is idle (0x00039D50 returns 0).
    // Boot 0x000E5A80 stored 1, and 0x000DD740 stores 1 again: slot select.
    // A scene with no frontend scene-control leaves the widget down.
    if (!scene_control_is(g, "gameselect"))
    {
        pcm_stop_cue(kSlotOpen);
        g.frontend_mode = -1;
        g.naming = false;
        return;
    }
    g.frontend_pending = 1;
    g.frontend_mode = g.frontend_pending;
    g.naming = false;
    g.slot_movie_index = -1;
    g.slot_movie_frames = 0;
    g.slot_visual = Game::MenuVisual{};
    g.contents_visual = Game::MenuVisual{};
    g.slot_text = Game::MenuTextPool{};
    g.contents_text = Game::MenuTextPool{};
    g.slot_cursor.reset();
    g.contents_cursor.reset();
    g.menu_book_target = 0;
    begin_menu_zoom(g.slot_visual, slot_backdrop(g));
    // 0x000DD774 calls 0x000E01B0 while the slot widget is installed.
    movie_open_gameselect();
    pcm_play_cue(g, kSlotOpen, true);
    std::cout << "frontend mode 1 slot select\n";
}

void gameselect_tick(Game& g, const InputState& in)
{
    const int previous_mode = g.frontend_mode;
    const int previous_row = g.contents_row;
    const int previous_slot = g.slot;
    bool cursor_created = false;
    if (tick_menu_book(g) && g.menu_book_target != 0)
    {
        g.frontend_mode = g.menu_book_target;
        g.menu_book_target = 0;
        g.naming = false;
        if (g.frontend_mode == 3)
        {
            g.contents_row = 0;
            g.contents_cursor.reset();
            movie_open_contents();
            for (int i = 0; i < 3; ++i)
            {
                g.contents_movie_frames[i] = 0;
                g.contents_movie_copied[i] = false;
            }
        }
    }
    if (g.frontend_mode == 1 || g.frontend_mode == 3)
    {
        auto& cursor = g.frontend_mode == 1 ? g.slot_cursor : g.contents_cursor;
        if (cursor == nullptr)
        {
            cursor = std::make_shared<MenuCursor>(g);
            cursor->move(g.frontend_mode == 1 ? g.slot : g.contents_row);
            cursor_created = true;
        }
        cursor->tick(scene_sim_step(g.clock.dt));
    }
    if (g.frontend_mode == 1)
    {
        tick_menu_zoom(g.slot_visual, scene_sim_step(g.clock.dt));
    }
    if (g.frontend_mode == 3)
    {
        tick_menu_zoom(g.contents_visual, scene_sim_step(g.clock.dt));
    }
    // Dialog A. 0x000584D0's widget takes the accept edge and steps {page}.
    if (in.accept && !g.slot_accept)
    {
        caption_accept(g);
    }
    if (g.frontend_mode == 0)
    {
        g.naming = false;
        // 0x00080C10 calls 0x00080CB0 while the banner dword at +0x294 is 0.
        // 0x001431B0 accepts a new A or Start, not both. 0x0011EB30 then requests
        // the gameselect script copied from 0x00322B8C.
        if (!g.attract_banner_latched && attract_confirm_pressed())
        {
            g.attract_banner_latched = true;
            // 0x00322B8C. 0x0011EB30 queues it. The next 0x0011E4C0 that enters state 4 loads it.
            script_request(g, "aid_script_ghoulies_gameselect_gameselect");
        }
    }
    else if (g.frontend_mode == 4)
    {
        // 0x00014C10 then 0x00015090. A and Start (codes 1 and 2) activate the
        // key. B (codes 3 and 4) calls 0x000DD550, which leaves for slot select.
        // X (code 5) cycles the page. Triggers (codes 9 and 10) move the caret.
        // D-pad codes 11-14 move the highlight.
        if (in.back && !g.slot_back)
        {
            g.frontend_mode = 1;
            g.naming = false;
        }
        else
        {
            if (in.left && !g.slot_left)
            {
                kb_move(g, -1, 0);
            }
            if (in.right && !g.slot_right)
            {
                kb_move(g, 1, 0);
            }
            if (in.up && !g.slot_up)
            {
                kb_move(g, 0, -1);
            }
            if (in.down && !g.slot_down)
            {
                kb_move(g, 0, 1);
            }
            if (in.caret_left && !g.slot_caret_left && g.kb_caret > 0)
            {
                --g.kb_caret;
            }
            if (in.caret_right && !g.slot_caret_right && g.kb_caret < g.kb_len)
            {
                ++g.kb_caret;
            }
            if (in.page && !g.slot_page)
            {
                if (g.kb_page == 0)
                {
                    g.kb_page = 1;
                }
                else if (g.kb_page == 1)
                {
                    g.kb_page = 2;
                }
                else
                {
                    g.kb_page = 0;
                }
                kb_clamp_col(g);
            }
            if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start))
            {
                kb_activate(g);
            }
        }
    }
    else if (g.frontend_mode == 1 && !menu_book_busy())
    {
        // 0x000E0210: direction 8 (right) increments, direction 4 (left) decrements. Both end in state 6.
        if (in.right && !g.slot_right && (g.slot_cursor->bat.state == 1 || g.slot_cursor->bat.state == 3))
        {
            g.slot = (g.slot + 1) % 3;
        }
        if (in.left && !g.slot_left && (g.slot_cursor->bat.state == 1 || g.slot_cursor->bat.state == 3))
        {
            g.slot = (g.slot + 2) % 3;
        }
        // Xbox 0x00142460 / 0x00141E20: prompt face-button acceptance sound.
        if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start) ||
            (in.back && !g.slot_back))
        {
            pcm_play_owned_cue(g, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON");
        }
        // 0x000E02D0: empty slot sets mode 4. Status 2, 3, or 4 sets mode 3 (slot contents).
        if (in.accept && !g.slot_accept)
        {
            if (g.slot_status[g.slot] == 0)
            {
                kb_open(g);
            }
            else
            {
                // Xbox 0x000E02D0: queue widget 3 and book clip 0.
                // 0x00083E20 installs it only after the first page threshold.
                g.menu_book_target = 3;
                g.slot_cursor->depart(g.slot);
                begin_menu_book(g, 0);
            }
        }
        // 0x000E05D0 copies aid_script_ghoulies_gameselect_backtoattract from 0x00324F30
        // into 0x0050E458, then 0x0011EAD0 requests that name.
        if (in.back && !g.slot_back)
        {
            g.naming = false;
            script_request(g, "aid_script_ghoulies_gameselect_backtoattract");
        }
    }
    else if (g.frontend_mode == 3 && !menu_book_busy())
    {
        // 0x000D9450 ticks the prompt widget. Move is 0x000DB450: direction 8
        // increments the row, direction 4 decrements it, then 0x000DA580 mode 6
        // swaps the prompt set through 0x000142180. 0x000D9D10, once the plate
        // zoom reaches 1, pushes mode 2 (Play), 5 (Features), or 4 (Challenges).
        // Those bodies place the row label and, for mode 2, the chapter buffer.
        // None of them request a script.
        if (in.right && !g.slot_right && (g.contents_cursor->bat.state == 1 || g.contents_cursor->bat.state == 3))
        {
            g.contents_row = (g.contents_row + 1) % 3;
        }
        if (in.left && !g.slot_left && (g.contents_cursor->bat.state == 1 || g.contents_cursor->bat.state == 3))
        {
            g.contents_row = (g.contents_row + 2) % 3;
        }
        // Xbox 0x00142460 / 0x00141E20: prompt face-button acceptance sound.
        if ((in.accept && !g.slot_accept) || (in.start && !g.slot_start) ||
            (in.back && !g.slot_back))
        {
            pcm_play_owned_cue(g, "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON");
        }
        // Play is 0x000DB570: it copies the slot name, calls 0x00083B10 with 1,
        // then 0x000DA580 with mode 7. Stream 2 is idle, so 0x00083B10 arms the
        // play state and the scene-control tick runs 0x00083C70, which
        // 0x0011EB30-requests the script 0x0010DB10 copied from the save.
        // Features 0x000DB6D0 and Challenges 0x000DB670 are separate callbacks.
        // Back is 0x000DB510, which returns to slot select.
        // Select record code 0 uses button word 0x110: A (0x0100) or Start (0x0010).
        // 0x001083F0 accepts either edge.
        if (((in.accept && !g.slot_accept) || (in.start && !g.slot_start)) && g.contents_row == 0)
        {
            const char* script = save_script(g);
            if (script != nullptr)
            {
                g.frontend_mode = 7;
                std::cout << "play " << script << "\n";
                script_request(g, script);
            }
        }
        if (in.back && !g.slot_back)
        {
            // Xbox 0x000DB510: queue widget 1 and book clip 1 (3 to 2).
            g.menu_book_target = 1;
            begin_menu_book(g, 1);
        }
    }
    if (g.frontend_mode == 1 && (previous_mode != 1 || previous_slot != g.slot))
    {
        if (previous_mode != 1)
        {
            g.slot_text = Game::MenuTextPool{};
        }
        begin_menu_zoom(g.slot_visual, slot_backdrop(g));
        if (g.slot_cursor != nullptr && !cursor_created)
        {
            g.slot_cursor->move(g.slot);
        }
    }
    if (g.frontend_mode == 3 && (previous_mode != 3 || previous_row != g.contents_row))
    {
        if (previous_mode != 3)
        {
            g.contents_text = Game::MenuTextPool{};
        }
        begin_menu_zoom(g.contents_visual, kContentsTextures[g.contents_row]);
        if (g.contents_cursor == nullptr)
        {
            g.contents_cursor = std::make_shared<MenuCursor>(g);
        }
        if (!cursor_created)
        {
            g.contents_cursor->move(g.contents_row);
        }
    }
    // Xbox 0x000DB450 / 0x000DACE0: release and reset the departing row
    // before 0x000D9F60 starts the newly selected movie.
    if (previous_mode == 3 && (g.frontend_mode != 3 || previous_row != g.contents_row))
    {
        movie_release_visible(kContentsMovies[previous_row]);
        g.contents_movie_frames[previous_row] = 0;
        g.contents_movie_copied[previous_row] = false;
    }
    // Xbox 0x000DF9A0: leaving the slot widget releases its +0xB80 movie.
    if (previous_mode == 1 && g.frontend_mode != 1 && g.slot_movie_index != -1)
    {
        movie_release_visible(g.slot_movie_name.c_str());
        g.slot_movie_index = -1;
    }
    g.slot_down = in.down;
    g.slot_up = in.up;
    g.slot_left = in.left;
    g.slot_right = in.right;
    g.slot_accept = in.accept;
    g.slot_back = in.back;
    g.slot_start = in.start;
    g.slot_page = in.page;
    g.slot_caret_left = in.caret_left;
    g.slot_caret_right = in.caret_right;
    tick_naming_background(g);
}

void draw_ui(const Game& g)
{
    gfx_sprite_clear();
    if (g.frontend_mode < 0)
    {
        draw_move_captions(g);
        return;
    }
    if (g.frontend_mode == 0)
    {
        draw_start_banner(g);
        draw_move_captions(g);
        return;
    }
    if (g.frontend_mode == 3 || g.frontend_mode == 7)
    {
        // Mode 7 is the widget 0x000DA580 stores from Play. Its draw is 0x000D5300.
        // Until that body is translated, the contents plates stay up for the
        // frame before 0x0011EB30's load replaces the scene.
        draw_contents(g);
        if (g.contents_cursor != nullptr)
        {
            g.contents_cursor->draw();
        }
        return;
    }
    // Naming is mode-table index 4. Its draw is 0x000DD170, which draws the
    // keyboard object and not the three slot plates.
    if (g.frontend_mode == 4)
    {
        draw_naming_background(g);
        draw_keyboard(g);
        return;
    }
    // Xbox 0x000DF140: occupied statuses use the chapter texture as backdrop.
    draw_menu_backdrop(g, g.slot_visual);
    for (int i = 0; i < 3; ++i)
    {
        if (i != g.slot)
        {
            draw_plate(g, i, false);
        }
    }
    draw_plate(g, g.slot, true);
    draw_slot_movie(g);
    for (int i = 0; i < 3; ++i)
    {
        draw_slot_status(g, i);
    }
    if (g.frontend_mode == 1)
    {
        draw_slot_select_copy(g);
    }
    draw_keyboard(g);
    if (g.slot_cursor != nullptr)
    {
        g.slot_cursor->draw();
    }
}

// Xbox 0x00046760: resolve mapname text and place it beside its map spot.
// 0x00060710 measures at scale 0.6; 0x0005F5C0 submits the string.
void draw_map_label(const Game& g, std::string_view key, float x, float y, float spot_height)
{
    static std::map<std::string, std::u16string> labels;
    if (labels.empty())
    {
        const auto path = g.aids.game_dir / "loctext" / "english" /
                          aid_hash_hex(aid_hash("aid_loctext_ghoulies_mapnames"));
        const LocTable table = parse_loctext(read_file(path), "mapnames");
        for (const auto& [name, text] : table.keys_to_utf8)
        {
            labels.emplace(name, utf8_to_u16(text));
        }
    }
    const auto found = labels.find("mapname__" + std::string(key));
    const std::u16string text = found == labels.end() ? u"ERROR" : found->second;
    const GloopyFont& font = gloopy_font(g);
    const float width = text_width(font, text, 0.6f);
    const float pen_x = x + width <= 640.f * 0.85f ? x + spot_height : x - width - spot_height;
    const float pen_y = y - float(font.drop()) * 0.6f * 0.5f;
    draw_glyphs(font, text, pen_x, pen_y, 0.6f, font.rest_frame());
}

} // namespace gbtg
