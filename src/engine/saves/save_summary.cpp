#include "save_summary.h"
#include "../frontend/app_error_screen.h"
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
