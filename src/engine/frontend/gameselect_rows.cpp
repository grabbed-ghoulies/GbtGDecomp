#include "gameselect_rows.h"
#include "gameselect_challenges.h"
#include "gameselect_contents.h"
#include "gameselect_delete.h"
#include "gameselect_features.h"
#include "gameselect_naming.h"
#include "gameselect_options.h"
#include "gameselect_stream_wait.h"
#include "final_score.h"
#include "gameselect_scene_replay.h"
#include "gameselect_slots.h"

#include "../scene/executable_resources.h"

#include <bit>
#include <cmath>
#include <cstring>
#include <sstream>

namespace gbtg
{
namespace
{
constexpr u32 kHeapFreeWidget = 0x000D8AC0;

[[noreturn]] void untranslated(const GameSelectWidgetRow& row, u32 slot)
{
    std::ostringstream message;
    message << "game-select widget row " << gameselect_widget_row_index(row) << " +0x" << std::hex << slot * 4
            << " (0x" << row.address[slot] << ") is not translated";
    throw Error(message.str());
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_half(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

// The mode table at 0x0030A000 (20 rows). Row 0 is empty.
const std::array<GameSelectWidgetRow, 20> kRows{{
    {{0, 0, 0, 0, 0, 0}, nullptr, nullptr, nullptr, nullptr, nullptr},
    {{0x000DD730, kHeapFreeWidget, 0x000DD740, 0x000DDC90, 0x000DDD80, 0x000DDF20}, create_runtime_slot_select,
        open_runtime_slot_select, close_runtime_slot_select, tick_runtime_slot_select, draw_runtime_slot_select},
    {{0x000DB720, kHeapFreeWidget, 0x000DB730, 0x000DB950, 0x000DB9E0, 0x000DBB90}, create_runtime_game_deletion,
        open_runtime_game_deletion, close_runtime_game_deletion, tick_runtime_game_deletion,
        draw_runtime_game_deletion},
    {{0x000D8AB0, kHeapFreeWidget, 0x000D8B10, 0x000D9300, 0x000D9450, 0x000D9640}, create_runtime_game_contents,
        open_runtime_game_contents, close_runtime_game_contents, tick_runtime_game_contents,
        draw_runtime_game_contents},
    {{0x000DCEB0, kHeapFreeWidget, 0x000DCEC0, 0x000DD030, 0x000DD0A0, 0x000DD170}, create_runtime_game_naming,
        open_runtime_game_naming, close_runtime_game_naming, tick_runtime_game_naming, draw_runtime_game_naming},
    {{0x000DC0B0, kHeapFreeWidget, 0x000DC0C0, 0x000DC640, 0x000DC6F0, 0x000DC9A0}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000D5850, kHeapFreeWidget, 0x000D5880, 0x000D62A0, 0x000D6410, 0x000D67F0}, create_runtime_game_challenges,
        open_runtime_game_challenges_menu, close_runtime_game_challenges, tick_runtime_game_challenges,
        draw_runtime_game_challenges},
    {{0x000D4EA0, kHeapFreeWidget, 0x000D4EB0, 0x000D5160, 0x000D51E0, 0x000D5300}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000D4EA0, kHeapFreeWidget, 0x000D7A60, 0x000D7DB0, 0x000D7E40, 0x000D7F80}, create_runtime_game_features,
        open_runtime_game_features, close_runtime_game_features, tick_runtime_game_features,
        draw_runtime_game_features},
    {{0x000E1E00, kHeapFreeWidget, 0x000E1E10, 0x000E21A0, 0x000E2230, 0x000E4DB0},
        create_runtime_game_scene_replay, open_runtime_game_chapter_replay, close_runtime_game_chapter_replay,
        tick_runtime_game_chapter_replay, draw_runtime_game_scene_replay},
    {{0x000E1E00, kHeapFreeWidget, 0x000E2B90, 0x000E2F20, 0x000E2FB0, 0x000E4DB0},
        create_runtime_game_scene_replay, open_runtime_game_scene_replay, close_runtime_game_scene_replay,
        tick_runtime_game_scene_replay, draw_runtime_game_scene_replay},
    {{0x000E1E00, kHeapFreeWidget, 0x000E39F0, 0x000E3D80, 0x000E3E10, 0x000E4DB0},
        create_runtime_game_scene_replay, open_runtime_game_bonus_book_replay, close_runtime_game_bonus_book_replay,
        tick_runtime_game_bonus_book_replay, draw_runtime_game_scene_replay},
    {{0x000E5BD0, kHeapFreeWidget, 0x000E5BE0, 0x000E6120, 0x000E6210, 0x000E62F0}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000E7E00, kHeapFreeWidget, 0x000E7E10, 0x000E81F0, 0x000E8270, 0x000E83A0}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000E7A00, kHeapFreeWidget, 0x000E7A10, 0x000E7C10, 0x000E7C70, 0x000E7CA0}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000E89A0, kHeapFreeWidget, 0x000E89B0, 0x000E8C30, 0x000E8CB0, 0x000E8CF0}, nullptr, nullptr, nullptr,
        nullptr, nullptr},
    {{0x000E1E00, kHeapFreeWidget, 0x000E48A0, 0x000E4BF0, 0x000E4C80, 0x000E4DB0},
        create_runtime_game_scene_replay, open_runtime_game_story_books, close_runtime_game_story_books,
        tick_runtime_game_story_books, draw_runtime_game_scene_replay},
    {{0x000E0B60, kHeapFreeWidget, 0x000E0B70, 0x000E0F50, 0x000E1000, 0x000E1130}, create_runtime_game_options,
        open_runtime_game_options, close_runtime_game_options, tick_runtime_game_options, draw_runtime_game_options},
    {{0x000E0650, kHeapFreeWidget, 0x000E0660, 0x000E0830, 0x000E08C0, 0x000E0A30},
        create_runtime_game_stream_wait, open_runtime_game_stream_wait, close_runtime_game_stream_wait,
        tick_runtime_game_stream_wait, draw_runtime_game_stream_wait},
    {{0x000D79A0, kHeapFreeWidget, 0x000D79B0, 0x000D79F0, 0x000D7A00, 0x00082F70}, create_runtime_game_final_score,
        open_runtime_game_final_score, close_runtime_game_final_score, tick_runtime_game_final_score,
        draw_runtime_game_final_score},
}};
}

const GameSelectWidgetRow& gameselect_widget_row(u32 index)
{
    if (index >= kRows.size())
    {
        throw Error("game-select widget row lies past the 0x0030A000 table");
    }
    return kRows[index];
}

u32 gameselect_widget_row_index(const GameSelectWidgetRow& row)
{
    return u32(&row - kRows.data());
}

GameSelectWidgetHandle create_gameselect_widget(const GameSelectWidgetRow& row, GameSelectControlServices& services)
{
    if (row.create == nullptr)
    {
        untranslated(row, 0);
    }
    return row.create(services);
}

// Xbox 0x000D8AC0
void destroy_runtime_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidgetHandle& widget)
{
    if (row.address[1] != kHeapFreeWidget)
    {
        untranslated(row, 1);
    }
    widget.reset(); // HeapFree on the block's heap (0x000F7100 picks it): the host heap boundary.
}

void open_gameselect_widget(const GameSelectWidgetRow& row, std::span<u8> owner, GameSelectControlState& control,
    GameSelectWidget& widget, GameSelectControlServices& services)
{
    if (row.open == nullptr)
    {
        untranslated(row, 2);
    }
    row.open(owner, control, widget, services);
}

void close_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    if (row.close == nullptr)
    {
        untranslated(row, 3);
    }
    row.close(widget, services);
}

void tick_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    if (row.tick == nullptr)
    {
        untranslated(row, 4);
    }
    row.tick(widget, services);
}

void draw_gameselect_widget(const GameSelectWidgetRow& row, GameSelectWidget& widget,
    GameSelectControlServices& services)
{
    if (row.draw == nullptr)
    {
        untranslated(row, 5);
    }
    row.draw(widget, services);
}

// Xbox 0x000533A0
std::span<u8> create_runtime_screen_texture(u16 width, u16 height, u8 levels, u32 format, u32 data,
    const RuntimeScreenTextureBoundaries& boundaries)
{
    const RuntimeModelAllocation object = boundaries.allocate(0x30);
    const std::span<u8> texture = object.bytes;
    std::fill(texture.begin(), texture.end(), u8(0));
    texture[0xF] = levels;
    put_half(texture, 0xA, height);
    put_half(texture, 8, width);
    put_word(texture, 4, 0x1C);
    put_word(texture, 0, format);
    texture[0xC] = 0;
    put_word(texture, 0x14, 0);
    put_word(texture, 0x18, 0);
    texture[0x12] = 0;
    texture[0x13] = 0;
    const u32 size = boundaries.header(width, height, levels, 0, format, 0, object.address + 0x1C);
    put_word(texture, 0x18, size);
    if (data == 0)
    {
        put_word(texture, 0x14, boundaries.allocate_physical(size, 0x80).address);
    }
    else
    {
        put_word(texture, 0x14, data);
    }
    u32 resource;
    std::memcpy(&resource, texture.data() + 4, 4);
    u32 address;
    std::memcpy(&address, texture.data() + 0x14, 4);
    boundaries.register_resource(object.address + resource, address);
    return texture;
}

// Xbox 0x00053450
void release_runtime_screen_texture(std::span<u8> texture, const RuntimeScreenTextureBoundaries& boundaries)
{
    u32 data;
    std::memcpy(&data, texture.data() + 0x14, 4);
    boundaries.free_physical(data);
    boundaries.free(boundaries.address_of(texture.data())); // HeapFree on the block's heap (0x000F7100).
}

// Xbox 0x00053680
bool runtime_texture_format_drawable(std::span<const u8> texture)
{
    u32 format;
    std::memcpy(&format, texture.data(), 4);
    switch (format)
    {
    case 0:
    case 4:
    case 5:
    case 0xC:
    case 0xE:
    case 0xF:
    case 0x12:
    case 0x1A:
    case 0x1E:
    case 0x28:
    case 0x3A:
    case 0x3C:
    case 0x3F:
        return true;
    default:
        return false;
    }
}

namespace
{
}

// Xbox 0x001080B0, ESI the direction: an axis inside (DAT_0032B274, DAT_0032B1AC) is dropped; the larger axis
// gives up (1), down (2), left (4) or right (8).
u16 gameselect_direction_code(std::array<float, 2> direction, const GameSelectControlServices& s)
{
    const float dead = 0.2f; // DAT_0032B1AC.
    const float low = -0.2f; // DAT_0032B274.
    if (dead > direction[0] && direction[0] > low)
    {
        direction[0] = 0.f;
    }
    if (dead > direction[1] && direction[1] > low)
    {
        direction[1] = 0.f;
    }
    if (direction[0] == 0.f && direction[1] == 0.f)
    {
        return 0;
    }
    if (std::fabs(direction[0]) < std::fabs(direction[1])) // 0x00152BB0
    {
        return direction[1] > 0.f ? 1 : 2; // DAT_0032B100.
    }
    return direction[0] > 0.f ? 8 : 4; // DAT_0032B100.
}
}
