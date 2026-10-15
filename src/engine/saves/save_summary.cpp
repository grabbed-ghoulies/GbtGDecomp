#include "save_summary.h"
#include "save_games.h"
#include "../frontend/gameselect_rows.h"
#include "../scene/scene_types.h"

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
