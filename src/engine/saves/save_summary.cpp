#include "save_summary.h"
#include "../frontend/app_error_screen.h"
#include "save_games.h"
#include "../frontend/gameselect_rows.h"
#include "../scene/scene_types.h"
#include "../frontend/gameselect_challenges.h"
#include "../frontend/gameselect_progress.h"
#include "../scene/gameselect_control.h"
#include "../scene/executable_resources.h"
#include "../runtime/aid_list_lookup.h"
#include "../runtime/name_lookup.h"

#include <cstring>

namespace gbtg
{
void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

namespace
{
// Xbox 0x000F2AC0, ESI the save's unlock block: DAT_00506C70 keeps only its bits 2, 4 and 8.
void copy_save_unlocks(const RuntimeProfileBlock& unlocks, RuntimeProfileBlock& profile)
{
    clear_runtime_profile_block(profile.bytes);
    u8 bits = profile.bytes[0];
    bits = (unlocks.bytes[0] & 2) == 0 ? u8(bits & 0xFD) : u8(bits | 2);
    profile.bytes[0] = bits;
    bits = profile.bytes[0];
    bits = (unlocks.bytes[0] & 4) == 0 ? u8(bits & 0xFB) : u8(bits | 4);
    profile.bytes[0] = bits;
    if ((unlocks.bytes[0] & 8) != 0)
    {
        profile.bytes[0] |= 8;
        return;
    }
    profile.bytes[0] &= 0xF7;
}
}

// Xbox 0x000F2F00
void refresh_runtime_save_summary(GameSelectControlServices& services)
{
    auto& profile = services.profile;
    if (profile.name[0] == 0)
    {
        return;
    }
    const char16_t* name = profile.name.data(); // EBX DAT_00506C90
    if ((profile.scene_flags->bytes[0] & 2) == 0)
    {
        profile.times = {};
    }
    else
    {
        const i32 fifth = save_game_word(name, 0x19C); // 0x0010DDA0
        const i32 fourth = save_game_word(name, 0x198); // 0x0010DD60
        const i32 third = save_game_word(name, 0x194); // 0x0010DD20
        const i32 second = save_game_word(name, 0x190); // 0x0010DCE0
        profile.times[0] = save_game_word(name, 0x18C); // 0x0010DCA0
        profile.times[1] = second;
        profile.times[2] = third;
        profile.times[3] = fourth;
        profile.times[4] = fifth;
        profile.times[5] = 0;
    }
    RuntimeProfileBlock* progress = save_game_progress(name); // 0x0010DDE0
    engine_copy_bytes(profile.progress.bytes.data(), progress->bytes.data(), u32(progress->size)); // 0x000F71F0
    RuntimeProfileBlock* unlocks = save_game_unlocks(name); // 0x0010DE80
    copy_save_unlocks(*unlocks, profile.unlocks); // 0x000F2AC0, ESI the block
    RuntimeProfileBlock* challenges = save_game_challenges(name); // 0x0010DE30
    engine_copy_bytes(profile.challenges.bytes.data(), challenges->bytes.data(), u32(challenges->size));
    load_save_game_words_1b4(name, g_game_progress.words_18f8); // 0x0010DED0
    save_game_word(name, 0x1A0); // 0x0010E050, its result unused here.
}

// Xbox 0x000F3230
void store_runtime_save_challenges(RuntimeProfile& profile)
{
    // 0x000F72D0(0x19): kind 100, size 13 and the bytes inline after the header, zeroed.
    std::array<u8, 13> bytes{};
    RuntimeProfileBlock copy{100, 13, bytes};
    engine_copy_bytes(copy.bytes.data(), profile.challenges.bytes.data(),
        u32(profile.challenges.size)); // 0x000F71F0, EAX the copy's +8
    store_save_game_challenges(profile.name.data(), copy); // 0x0010E5B0, EBX DAT_00506C90
    // 0x000663E0 frees the copy.
}

// Xbox 0x000F2A50
void copy_runtime_progress_bits(RuntimeProfileBlock& block, const RuntimeProfile& profile)
{
    std::fill(block.bytes.begin(), block.bytes.end(), u8(0));
    for (u32 bit = 0; bit < 0x3E; ++bit)
    {
        const u8 mask = u8(1u << (bit & 7));
        if ((profile.progress.bytes[bit >> 3] & mask) != 0)
        {
            block.bytes[bit >> 3] = u8(block.bytes[bit >> 3] | mask);
        }
    }
}

// Xbox 0x000F3290
void store_runtime_save_progress(RuntimeProfile& profile)
{
    // 0x000F72D0(0x14): kind 0x3E, size 8 and the bytes inline after the header, zeroed.
    std::array<u8, 8> bytes{};
    RuntimeProfileBlock block{0x3E, 8, bytes};
    copy_runtime_progress_bits(block, profile); // 0x000F2A50, EBX the block
    store_save_game_progress(profile.name.data(), block); // 0x0010E4F0, EAX DAT_00506C90
    // 0x000663E0 frees the block.
}

// Xbox 0x000F2D70
void finish_runtime_game_save(RuntimeProfile& profile, SaveGameContext& saves)
{
    const char16_t* name = profile.name.data(); // DAT_00506C90
    if (name[0] == 0)
    {
        return;
    }
    if (profile.scene_flags == nullptr)
    {
        throw Error("0x000F2D70 reads the scene flags (DAT_00506C74) before they exist");
    }
    if ((profile.scene_flags->bytes[0] & 2) != 0)
    {
        set_save_game_word_18c(name, u32(profile.times[0])); // 0x0010E360, DAT_00506C78
        set_save_game_word_190(name, u32(profile.times[1])); // 0x0010E3B0, DAT_00506C7C
        set_save_game_word_198(name, u32(profile.times[3])); // 0x0010E450, DAT_00506C84
        set_save_game_word_194(name, u32(profile.times[2])); // 0x0010E400, DAT_00506C80
        set_save_game_word_19c(name, u32(profile.times[4])); // 0x0010E4A0, DAT_00506C88
        set_save_game_finished(name); // 0x0010E310
    }
    store_runtime_save_challenges(profile); // 0x000F3230
    store_runtime_save_progress(profile); // 0x000F3290
    if (save_game_word(name, save_record_field::unk_184.offset) != 0) // 0x0010DC20, EBX the name
    {
        clear_save_game_word_184(name); // 0x0010E2C0
    }
    // 0x000F32F0 on a failed write: apperror gamesystem_corruptgamemessage for the name.
    queue_save_game(name, [](const void* argument)
    {
        show_runtime_corrupt_game(static_cast<const char16_t*>(argument)); // 0x000F32F0
    }, saves); // 0x0010DA80, EAX the name
}

namespace
{
// The script's index in the held scene order (DAT_00511B64), -1 when absent.
i32 held_scene_index(const RuntimeSceneSave& c, const char* script)
{
    return find_runtime_aid_list_entry(c.saves.scene_order, script); // 0x00035A40, EBX the script
}

// 0x000F3410 and 0x000F3590: 1 when the script's index lies before `first`'s or after
// aid_script_ghoulies_chapter5_scene13_2playcam's.
i32 outside_held_scenes(const RuntimeSceneSave& c, const char* script, const char* first)
{
    const i32 index = held_scene_index(c, script);
    if (held_scene_index(c, first) <= index)
    {
        if (index <= held_scene_index(c, "aid_script_ghoulies_chapter5_scene13_2playcam"))
        {
            return 0;
        }
    }
    return 1;
}

// Xbox 0x000F3410, EDX the script: outside chapter 5's scenes 2..13 (the countdown's).
i32 outside_chapter5_countdown(const RuntimeSceneSave& c, const char* script)
{
    return outside_held_scenes(c, script, "aid_script_ghoulies_chapter5_scene2_1playcam");
}

// Xbox 0x000F3590, EDX the script: outside chapter 5's scenes 1..13.
i32 outside_chapter5_scenes(const RuntimeSceneSave& c, const char* script)
{
    return outside_held_scenes(c, script, "aid_script_ghoulies_chapter5_scene1_1playcam");
}

// The scene's chapter (0x0004B4D0 on a copy of DAT_00510E58), 1..9 for a story scene.
bool story_chapter(const RuntimeSceneSave& c, const char* script)
{
    const i32 chapter = gameselect_script_chapter(script, c.gameselect); // 0x0004B4D0, EAX the copy
    return chapter > 0 && chapter < 10;
}

// Xbox 0x000F3080: in a story scene, its script into the save outside the countdown, and the five times outside
// chapter 5.
void store_runtime_scene_times(RuntimeSceneSave& c)
{
    char script[0x80];
    std::strcpy(script, c.scene); // DAT_00510E58
    if (!story_chapter(c, script))
    {
        return;
    }
    const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
    if (outside_chapter5_countdown(c, script) != 0) // 0x000F3410, EDX the copy
    {
        set_save_game_script(name, script); // 0x0010E220
    }
    if (outside_chapter5_scenes(c, script) != 0) // 0x000F3590, EDX the copy
    {
        set_save_game_word_18c(name, u32(c.profile.times[0])); // 0x0010E360, DAT_00506C78
        set_save_game_word_190(name, u32(c.profile.times[1])); // 0x0010E3B0, DAT_00506C7C
        set_save_game_word_198(name, u32(c.profile.times[3])); // 0x0010E450, DAT_00506C84
        set_save_game_word_194(name, u32(c.profile.times[2])); // 0x0010E400, DAT_00506C80
        set_save_game_word_19c(name, u32(c.profile.times[4])); // 0x0010E4A0, DAT_00506C88
    }
}

// Xbox 0x000F3130: 0x000F3080 for a scene quit from the pause, with three of the times.
void store_runtime_quit_scene_times(RuntimeSceneSave& c)
{
    char script[0x80];
    std::strcpy(script, c.scene); // DAT_00510E58
    if (!story_chapter(c, script))
    {
        return;
    }
    const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
    if (outside_chapter5_countdown(c, script) != 0) // 0x000F3410, EDX the copy
    {
        set_save_game_script(name, script); // 0x0010E220
    }
    if (outside_chapter5_scenes(c, script) != 0) // 0x000F3590, EDX the copy
    {
        set_save_game_word_18c(name, u32(c.profile.times[0])); // 0x0010E360, DAT_00506C78
        set_save_game_word_194(name, u32(c.profile.times[2])); // 0x0010E400, DAT_00506C80
        set_save_game_word_19c(name, u32(c.profile.times[4])); // 0x0010E4A0, DAT_00506C88
    }
}

// Xbox 0x000F31C0: a restarted scene keeps the save's script; three of the times outside chapter 5.
void store_runtime_restart_scene_times(RuntimeSceneSave& c)
{
    char script[0x80];
    std::strcpy(script, c.scene); // DAT_00510E58
    if (!story_chapter(c, script))
    {
        return;
    }
    if (outside_chapter5_scenes(c, script) != 0) // 0x000F3590, EDX the copy
    {
        const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
        set_save_game_word_18c(name, u32(c.profile.times[0])); // 0x0010E360, DAT_00506C78
        set_save_game_word_194(name, u32(c.profile.times[2])); // 0x0010E400, DAT_00506C80
        set_save_game_word_19c(name, u32(c.profile.times[4])); // 0x0010E4A0, DAT_00506C88
    }
}

// Xbox 0x000F34C0: in chapter 5's book (DAT_002FE698 kind 5), the furthest scene reached into the save (+0x25D);
// from scene 13 on, its +0x264 flag at the final book scene and +0x260 with the countdown's gold (DAT_00506C5C).
void store_runtime_book_scene(RuntimeSceneSave& c)
{
    const char* scene = c.scene; // DAT_00510E58
    const i32 chapter = gameselect_script_chapter(scene, c.gameselect); // 0x0004B4D0
    if (chapter <= 0 || chapter >= 10)
    {
        return;
    }
    const i32 book = i32(read_u32(c.gameselect.image.memory(0x002FE698 + u32(gameselect_script_chapter(scene,
        c.gameselect)) * 4, 4), 0)); // 0x0004B4D0 again
    if (book != 5)
    {
        return;
    }
    const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
    const i32 reached = gameselect_book_scene(scene, c.gameselect); // 0x0011EC70
    if (reached < 0xD)
    {
        if (i32(save_game_byte(name, 0x25D)) < reached) // 0x0010E0E0, AL
        {
            set_save_game_byte_25d(name, u8(reached)); // 0x0010E8A0
        }
        return;
    }
    if (transition_aid_equal("aid_script_ghoulies_chapter5_scene13_1book", scene)) // 0x000357D0, EDX the scene
    {
        set_save_game_word_264(name); // 0x0010E940
    }
    if (c.gold != 0) // DAT_00506C5C
    {
        set_save_game_word_260(name); // 0x0010E8F0
    }
}

// The +0x184 word cleared when set, then the save queued with 0x000F32F0 for a failed write.
void queue_runtime_scene_save(RuntimeSceneSave& c)
{
    const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
    if (save_game_word(name, save_record_field::unk_184.offset) != 0) // 0x0010DC20
    {
        clear_save_game_word_184(name); // 0x0010E2C0
    }
    queue_save_game(name, [](const void* argument)
    {
        show_runtime_corrupt_game(static_cast<const char16_t*>(argument)); // 0x000F32F0
    }, c.saves); // 0x0010DA80, EAX the name
}
}

// Xbox 0x00049D40
void clamp_runtime_challenge_steps(RuntimeSceneSave& c)
{
    if ((c.profile.scene_flags->bytes[0] & 8) == 0 && (c.runtime_flags & 0x200000u) == 0)
    {
        return;
    }
    if (transition_aid_equal(c.scene, "aid_script_ghoulies_gameselect_gameselect")) // 0x000357D0, DAT_002FDE60
    {
        return;
    }
    const i32 index = held_scene_index(c, c.scene); // 0x00035A40, EBX DAT_00510E58
    g_game_progress.word_1740 = held_scene_index(c,
        "aid_script_ghoulies_chapter5_scene1_1playcam"); // DAT_002FDEE0, DAT_004E1740
    if (index <= g_game_progress.word_1740)
    {
        g_game_progress.word_1740 = index;
    }
}

// Xbox 0x00049C30
void end_runtime_challenge_game(RuntimeSceneSave& c)
{
    const u32 flags = c.runtime_flags; // DAT_0050EA9C
    if ((c.profile.scene_flags->bytes[0] & 8) == 0 && (flags & 0x200000u) == 0)
    {
        return;
    }
    clamp_runtime_challenge_steps(c); // 0x00049D40
    u8& bits = c.profile.scene_flags->bytes[0];
    if ((bits & 8) == 0 && (flags & 0x200000u) == 0)
    {
        return;
    }
    bits &= 0xF7;
    g_challenge_selection.order[0] = 0; // DAT_004E1748
    // 0x001234B0's body inlined on PTR_s_aid_aidlist_ghoulies_sceneorder (DAT_00430CF8).
    hold_runtime_scene_order(c.gameselect.scene_order, c.gameselect);
}

// Xbox 0x000F2C40
void save_runtime_scene_start(RuntimeSceneSave& c)
{
    if (c.profile.name[0] == 0) // DAT_00506C90
    {
        return;
    }
    if ((c.profile.scene_flags->bytes[0] & 2) != 0)
    {
        store_runtime_scene_times(c); // 0x000F3080
    }
    store_runtime_save_challenges(c.profile); // 0x000F3230
    store_runtime_book_scene(c); // 0x000F34C0
    if (outside_chapter5_scenes(c, c.scene) != 0) // 0x000F3590, EDX DAT_00510E58
    {
        store_runtime_save_progress(c.profile); // 0x000F3290
    }
    queue_runtime_scene_save(c);
}

// Xbox 0x000F2CB0
void save_runtime_quit_scene(RuntimeSceneSave& c)
{
    if (c.profile.name[0] == 0)
    {
        return;
    }
    if ((c.profile.scene_flags->bytes[0] & 2) != 0)
    {
        store_runtime_quit_scene_times(c); // 0x000F3130
    }
    store_runtime_save_challenges(c.profile); // 0x000F3230
    if (outside_chapter5_scenes(c, c.scene) != 0) // 0x000F3590, EDX DAT_00510E58
    {
        store_runtime_save_progress(c.profile); // 0x000F3290
    }
    queue_runtime_scene_save(c);
}

// Xbox 0x000F2D10
void save_runtime_restart_scene(RuntimeSceneSave& c)
{
    if (c.profile.name[0] == 0)
    {
        return;
    }
    if ((c.profile.scene_flags->bytes[0] & 2) != 0)
    {
        store_runtime_restart_scene_times(c); // 0x000F31C0
    }
    store_runtime_save_challenges(c.profile); // 0x000F3230
    if (outside_chapter5_scenes(c, c.scene) != 0) // 0x000F3590, EDX DAT_00510E58
    {
        store_runtime_save_progress(c.profile); // 0x000F3290
    }
    queue_runtime_scene_save(c);
}

// Xbox 0x000F2E80
void save_runtime_challenge_scene(RuntimeSceneSave& c)
{
    if (c.profile.name[0] == 0 || (c.profile.scene_flags->bytes[0] & 8) == 0)
    {
        return;
    }
    clamp_runtime_challenge_steps(c); // 0x00049D40
    const char16_t* name = c.profile.name.data(); // EBX DAT_00506C90
    set_save_game_word_1a0(name, u32(g_game_progress.word_1740)); // 0x0010E800, DAT_004E1740
    set_save_game_real_1a4(name, c.scene_time + g_game_progress.real_1744); // 0x0010E850, DAT_004312D0
    store_runtime_save_progress(c.profile); // 0x000F3290
    store_runtime_book_scene(c); // 0x000F34C0
    queue_runtime_scene_save(c);
}

// Xbox 0x00117000's save: a challenge game (scene flag 8) 0x000F2E80, a restarted scene (DAT_00430D0C 1)
// 0x000F2D10, otherwise 0x000F2C40.
void save_runtime_scene(RuntimeSceneSave& c, i32 chapter_mode)
{
    if ((c.profile.scene_flags->bytes[0] & 8) == 0)
    {
        if (chapter_mode != 1) // DAT_00430D0C
        {
            save_runtime_scene_start(c); // 0x000F2C40
            return;
        }
        save_runtime_restart_scene(c); // 0x000F2D10
        return;
    }
    save_runtime_challenge_scene(c); // 0x000F2E80
}

// Xbox 0x000F3020, EAX the name: the save's challenges and progress words become the current game's.
void load_runtime_save_progress(const char16_t* name, GameSelectControlServices& services)
{
    auto& profile = services.profile;
    RuntimeProfileBlock* challenges = save_game_challenges(name); // 0x0010DE30
    engine_copy_bytes(profile.challenges.bytes.data(), challenges->bytes.data(), u32(challenges->size));
    load_save_game_words_1b4(name, g_game_progress.words_18f8); // 0x0010DED0
    load_save_game_words_208(name, g_game_progress.words_194c); // 0x0010DF90
    g_game_progress.word_1740 = save_game_word(name, 0x1A0); // 0x0010E050
    const i32 bits = save_game_word(name, 0x1A4); // 0x0010E090, XMM0
    float value;
    std::memcpy(&value, &bits, 4);
    g_game_progress.real_1744 = value;
}
}
