// Native side of powerup_hud_oracle.py. Reads one hex-word case per line and prints the state and
// call log the retail side prints.
#include "engine/entities/powerup_awards.h"
#include "engine/entities/actor_live_attachments.h"
#include "engine/audio/bank_streams.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace gbtg
{
namespace
{
std::vector<std::string> g_log;
std::array<std::array<u8, 0x10>, 0x80> g_textures{};
std::vector<u8> g_icons(0x50 * 0x80);
u32 g_text_result = 0;
RuntimeDialogueTextPool g_text_pool{};
const RuntimePowerupRow* g_rows = nullptr;

std::string hex(u32 value)
{
    char text[16];
    std::snprintf(text, sizeof(text), "%x", value);
    return text;
}

std::string resource_name(const u8* pointer)
{
    if (pointer == nullptr)
    {
        return "0";
    }
    if (pointer == g_icons.data())
    {
        return "icons";
    }
    char text[8];
    std::snprintf(text, sizeof(text), "T%02x", u32((pointer - g_textures[0].data()) / 0x10));
    return text;
}
}

std::span<const u8> acquire_animation_resource(AnimationResourceRegistry&, const char* name)
{
    g_log.push_back(std::string("acquire ") + name);
    if (std::strcmp(name, "aid_aidlist_ghoulies_scoreboard_icons") == 0)
    {
        return g_icons;
    }
    const auto index = std::stoul(std::string(name + std::strlen(name) - 2), nullptr, 16);
    return g_textures[index];
}

void release_animation_resource(AnimationResourceRegistry&, std::span<const u8> resource)
{
    g_log.push_back("release " + resource_name(resource.data()));
}

void resource_error(ResourceErrorState&, int code)
{
    g_log.push_back("error " + std::to_string(code));
}

u32 request_namespaced_localtext(const char* resource, const char* group, const char* key, char16_t* output,
    LocalTextResourceContext&)
{
    g_log.push_back(std::string("text ") + resource + " " + group + " " + key + " "
        + (output != nullptr ? "scratch" : "?"));
    return g_text_result;
}

void allocate_dialogue_text_pool(RuntimeDialogueTextPool*& output, i32 font, u32 variant, u32 flags,
    GhoulySpawnPool*&)
{
    g_log.push_back("textpool " + hex(u32(font)) + " " + hex(variant) + " " + hex(flags) + " cell");
    output = &g_text_pool;
}

u32 create_dialogue_text_record(RuntimeDialogueTextPool& pool, const char16_t*, float x, float y, float scale_x,
    float scale_y, float lifetime, i32 centered, i32 panel, i32 sticky, const RuntimeTextMetrics&, float, i32&, u32&)
{
    char text[160];
    std::snprintf(text, sizeof(text), "record %s scratch %08x %08x %08x %08x %08x %x %x %x",
        &pool == &g_text_pool ? "pool" : "?", std::bit_cast<u32>(x), std::bit_cast<u32>(y),
        std::bit_cast<u32>(scale_x), std::bit_cast<u32>(scale_y), std::bit_cast<u32>(lifetime), u32(centered),
        u32(panel), u32(sticky));
    g_log.push_back(text);
    return 0;
}

void broadcast_actor_pulse_message(const i32* message)
{
    g_log.push_back("pulse " + hex(u32(message[0])) + " " + hex(u32(message[1])) + " " + hex(u32(message[2]))
        + " active " + hex(g_rows[u32(message[2]) % 20].active));
}

void release_audio_stream(const char* name, AudioBankStreams&)
{
    g_log.push_back(std::string("streamrelease ") + name);
}

void request_audio_stream(const char* name, i32 id, AudioBankStreams&)
{
    g_log.push_back(std::string("streamrequest ") + name + " " + hex(u32(id)));
}
}

using namespace gbtg;

namespace
{
const char* const kStreamNames[]{"XACT_A", "XACT_B", "XACT_C"};
const char* const kPowerupNames[]{"powerupId_Null", "powerupId_Book", "powerupId_DizzyReaper",
    "powerupId_EnergyBoost", "powerupId_EnergyRobber", "powerupId_EventComplete", "powerupId_ExtraTime",
    "powerupId_GhoulyFreeze", "powerupId_InstantDeath", "powerupId_Invisibility", "powerupId_Invulnerability",
    "powerupId_KnockDownMania", "powerupId_MiniCooper", "powerupId_PermanentWeaponBoost",
    "powerupId_PlayerSlowDown", "powerupId_PlayerSpeedUp", "powerupId_RandomNasty", "powerupId_ReverseControls",
    "powerupId_ShockerBlocker", "powerupId_TraitorFever"};

struct Reader
{
    std::istringstream in;
    u32 next()
    {
        std::string token;
        in >> token;
        return u32(std::stoul(token, nullptr, 16));
    }
    float real()
    {
        return std::bit_cast<float>(next());
    }
};

std::span<const u8> texture_at(u32 index)
{
    return index == 0 ? std::span<const u8>{} : std::span<const u8>{g_textures[index]};
}

RuntimeScoreboardMessage read_message(Reader& in)
{
    std::array<u32, 37> w{};
    for (auto& word : w)
    {
        word = in.next();
    }
    RuntimeScoreboardMessage m{};
    const auto f = [&](size_t i)
    {
        return std::bit_cast<float>(w[i]);
    };
    m.id = w[0];
    m.kind = w[1];
    m.word_8 = w[2];
    m.float_c = f(3);
    m.float_10 = f(4);
    m.word_14 = w[5];
    m.word_18 = w[6];
    m.word_1c = w[7];
    m.scale_x = f(8);
    m.scale_y = f(9);
    m.float_28 = f(10);
    m.float_2c = f(11);
    m.float_30 = f(12);
    m.float_34 = f(13);
    m.float_38 = f(14);
    m.counter.value = i32(w[15]);
    m.counter.target = i32(w[16]);
    for (size_t i = 0; i < 8; ++i)
    {
        m.counter.digits[i] = i32(w[17 + i]);
    }
    m.counter.word_28 = w[25];
    m.counter.word_2c = w[26];
    m.counter.timer = w[27];
    m.word_70 = w[28];
    m.word_74 = w[29];
    m.word_78 = w[30];
    m.pulse = w[31];
    m.pulse_scale = f(32);
    m.icon = w[33];
    m.texture = texture_at(w[34]);
    m.width = w[35];
    m.scaled_width = f(36);
    return m;
}

std::string dump_board(const RuntimeScoreboard& board)
{
    std::string out = "n" + std::to_string(board.messages.size());
    for (const auto& m : board.messages)
    {
        const auto b = [](float v)
        {
            return std::bit_cast<u32>(v);
        };
        const u32 words[]{m.id, m.kind, m.word_8, b(m.float_c), b(m.float_10), m.word_14, m.word_18, m.word_1c,
            b(m.scale_x), b(m.scale_y), b(m.float_28), b(m.float_2c), b(m.float_30), b(m.float_34), b(m.float_38),
            u32(m.counter.value), u32(m.counter.target), u32(m.counter.digits[0]), u32(m.counter.digits[1]),
            u32(m.counter.digits[2]), u32(m.counter.digits[3]), u32(m.counter.digits[4]), u32(m.counter.digits[5]),
            u32(m.counter.digits[6]), u32(m.counter.digits[7]), m.counter.word_28, m.counter.word_2c,
            m.counter.timer, m.word_70, m.word_74, m.word_78, m.pulse, b(m.pulse_scale), m.icon};
        for (u32 word : words)
        {
            out += " " + hex(word);
        }
        out += " " + resource_name(m.texture.data()) + " " + hex(m.width) + " " + hex(b(m.scaled_width));
    }
    out += " " + hex(board.next_id) + " " + resource_name(board.first_icon.data()) + " "
        + resource_name(board.second_icon.data());
    return out;
}
}

int main()
{
    for (size_t i = 0; i < g_textures.size(); ++i)
    {
        const u32 width = 0x10 + u32(i) * 3;
        std::memcpy(g_textures[i].data() + 8, &width, 4);
    }
    // 0x00314170, the counter preset 0x0012F990 copies its +8 word from.
    const std::array<u32, 3> preset{std::bit_cast<u32>(0.4f), std::bit_cast<u32>(0.5f), std::bit_cast<u32>(0.1f)};
    // The registry is only passed through to the logging resource stubs.
    alignas(64) static u8 registry_storage[sizeof(AnimationResourceRegistry)]{};
    auto& registry = *reinterpret_cast<AnimationResourceRegistry*>(registry_storage);
    ResourceErrorState error{};
    std::string text;
    while (std::getline(std::cin, text))
    {
        Reader in{std::istringstream(text)};
        g_log.clear();
        const u32 entry = in.next();
        const u32 aid_seed = in.next();
        g_text_result = in.next();
        std::fill(g_icons.begin(), g_icons.end(), u8(0));
        for (u32 i = 0; i < 0x50; ++i)
        {
            char name[16];
            const bool aid = i < 0x10 || (0x3A <= i && i <= 0x4B) || (i * 7 + aid_seed) % 5 != 0;
            std::snprintf(name, sizeof(name), aid ? "aid_icon_%02x" : "tex_icon_%02x", i);
            std::strcpy(reinterpret_cast<char*>(g_icons.data() + i * 0x80), name);
        }
        RuntimeScoreboard board{};
        const u32 count = in.next();
        for (u32 i = 0; i < count; ++i)
        {
            board.messages.push_back(read_message(in));
        }
        board.next_id = in.next();
        const u32 first = in.next();
        const u32 second = in.next();
        board.first_icon = first == 0 ? std::span<const u8>{} : std::span<const u8>{g_textures[first - 1]};
        board.second_icon = second == 0 ? std::span<const u8>{} : std::span<const u8>{g_textures[second - 1]};
        board.icon_scale = in.real();
        RuntimeScoreboardResources resources{registry, error,
            {reinterpret_cast<const u8*>(preset.data()), sizeof(preset)}};
        std::string out;
        switch (entry)
        {
        case 0:
        {
            std::span<const u8> output{reinterpret_cast<const u8*>(&preset), 1};
            acquire_runtime_scoreboard_icon(in.next(), output, resources);
            out = resource_name(output.data());
            break;
        }
        case 1:
        {
            u32 a[7];
            for (u32& v : a)
            {
                v = in.next();
            }
            const u32 id = add_runtime_scoreboard_message(a[0], a[1], a[2], i32(a[3]), a[4], a[5], a[6], board,
                resources);
            out = hex(id) + " " + dump_board(board);
            break;
        }
        case 2:
        {
            const bool removed = remove_runtime_scoreboard_message(in.next(), board, registry);
            out = hex(removed ? 1 : 0) + " " + dump_board(board);
            break;
        }
        case 3:
        {
            const u32 id = add_runtime_powerup_message(in.next(), board, resources);
            out = hex(id) + " " + dump_board(board);
            break;
        }
        case 4:
        {
            const u32 kind = in.next();
            const u32 exists = in.next();
            const u16 width = u16(in.next());
            const float bottom = in.real();
            for (float& origin : g_text_pool.origin)
            {
                origin = in.real();
            }
            static const std::unordered_map<const u8*, LocalTextResourceView> views;
            LocalTextResourceContext localtext{registry, error, views, {}};
            std::array<char16_t, 0x40> scratch{};
            scratch[0] = 0x4141;
            RuntimeDialogueTextPool* pool = exists != 0 ? &g_text_pool : nullptr;
            GhoulySpawnPool* pools = nullptr;
            static const RuntimeTextMetrics* metrics = nullptr;
            i32 next_id = 0;
            u32 random = 0;
            RuntimePowerupCaption caption{localtext, scratch.data(), kPowerupNames, pool, pools, width, bottom,
                *metrics, 640.f, next_id, random};
            show_runtime_powerup_caption(kind, caption);
            out = hex(scratch[0]) + " " + (pool == &g_text_pool ? "pool" : "0");
            for (float origin : g_text_pool.origin)
            {
                out += " " + hex(std::bit_cast<u32>(origin));
            }
            break;
        }
        case 5:
        {
            const u32 slot = in.next();
            const i32 amount = i32(in.next());
            std::array<u32, 2> ids{in.next(), in.next()};
            std::array<u32, 2> active{in.next(), in.next()};
            std::array<i32, 2> counts{i32(in.next()), i32(in.next())};
            RuntimeScoreboard* pointer = &board;
            RuntimeBonusCounters counters{pointer, ids, active, counts};
            show_runtime_bonus_counter(slot, amount, counters, resources);
            out = hex(ids[0]) + " " + hex(ids[1]) + " " + hex(active[0]) + " " + hex(active[1]) + " "
                + hex(u32(counts[0])) + " " + hex(u32(counts[1])) + " " + dump_board(board);
            break;
        }
        case 6:
        {
            const u32 kind = in.next();
            const u32 present = in.next();
            std::array<RuntimePowerupRow, 20> rows{};
            for (auto& row : rows)
            {
                row.active = in.next();
                row.amount = in.real();
                row.extra = in.next();
                row.player = in.next();
                row.id = i32(in.next());
                row.message = in.next();
            }
            RuntimeScoreboard* pointer = present != 0 ? &board : nullptr;
            RuntimePowerupTable table{rows, pointer, registry};
            g_rows = rows.data();
            cancel_runtime_powerup(kind, table);
            for (const auto& row : rows)
            {
                out += hex(row.active) + " " + hex(std::bit_cast<u32>(row.amount)) + " " + hex(row.extra) + " "
                    + hex(row.player) + " " + hex(u32(row.id)) + " " + hex(row.message) + " ";
            }
            out += dump_board(board);
            break;
        }
        case 7:
        {
            const u32 present = in.next();
            const u32 n = in.next();
            std::vector<u8> storage(0x14 * n + 1);
            GhoulySpawnPool pool{0x14, 0, storage.data(), storage.data() + 0x14 * n, storage.data() + 0x14 * n};
            for (u32 i = 0; i < n; ++i)
            {
                const u32 kind = in.next();
                std::memcpy(storage.data() + 0x14 * i, &kind, 4);
            }
            for (u32 i = 0; i < n; ++i)
            {
                const u32 time = in.next();
                std::memcpy(storage.data() + 0x14 * i + 8, &time, 4);
            }
            extend_runtime_scene_timers(in.real(), present != 0 ? &pool : nullptr);
            for (u32 i = 0; i < n; ++i)
            {
                u32 time;
                std::memcpy(&time, storage.data() + 0x14 * i + 8, 4);
                out += hex(time) + " ";
            }
            break;
        }
        case 8:
        {
            const u32 present = in.next();
            const u32 n = in.next();
            std::vector<u8> storage(0x88 * n + 1);
            GhoulySpawnPool pool{0x88, 0, storage.data(), storage.data() + 0x88 * n, storage.data() + 0x88 * n};
            for (u32 i = 0; i < n; ++i)
            {
                const u32 kind = in.next();
                std::memcpy(storage.data() + 0x88 * i, &kind, 4);
            }
            const u8* found = find_runtime_challenge_record(in.next(), present != 0 ? &pool : nullptr);
            out = found != nullptr ? std::to_string((found - storage.data()) / 0x88) : "-1";
            break;
        }
        case 9:
        {
            alignas(8) static u8 voices[0x4000]{};
            static AudioBankStreams streams{{}, {}, {}, *reinterpret_cast<AudioEventVoices*>(voices)};
            for (auto& slot : streams.streams)
            {
                const u32 name = in.next();
                slot.name.fill(0);
                if (name != 0)
                {
                    std::strcpy(slot.name.data(), kStreamNames[name - 1]);
                }
                slot.state = i32(in.next());
                slot.time = in.real();
            }
            const u32 name = in.next();
            const u32 id = in.next();
            restart_audio_stream(kStreamNames[name], i32(id), streams);
            break;
        }
        }
        std::cout << out << " |";
        for (size_t i = 0; i < g_log.size(); ++i)
        {
            std::cout << (i == 0 ? " " : " ; ") << g_log[i];
        }
        std::cout << "\n";
    }
}
