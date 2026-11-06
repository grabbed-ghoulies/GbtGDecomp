#pragma once
#include "gameselect_rows.h"
#include "../scene/executable_resources.h"

namespace gbtg
{
// One game select menu page's settings in the executable's data, the same words on each: the features page
// (DAT_00428C28), options (DAT_00429958), chapter replay (DAT_004299B8), scene replay (DAT_00429A10), bonus books
// (DAT_00429A78) and story books (DAT_00429AE0). Nothing in the dumps writes them.
struct GameSelectPageLayout
{
    float title_text_scale; // +0
    float banner_text_scale; // +0x4
    float banner_position[2]; // +0x8
    float title_position[2]; // +0x10
    float prompts_left; // +0x18
    float prompts_bottom; // +0x1C
    float prompts_spacing; // +0x20
    float prompts_origin_x; // +0x24
    float prompts_icon_scale; // +0x28
    float prompts_text_scale; // +0x2C
    float pool_origin[2]; // +0x30
    float menu_offset[2]; // +0x38
    i32 menu_visible_rows; // +0x40
    float menu_row_spacing; // +0x44
    float menu_text_scale; // +0x48
    u32 menu_wrap; // +0x4C
    float menu_origin[2]; // +0x50
};
static_assert(sizeof(GameSelectPageLayout) == 0x58);

inline const GameSelectPageLayout& gameselect_page_layout(const GameSelectControlServices& s, u32 address)
{
    return *reinterpret_cast<const GameSelectPageLayout*>(s.image.memory(address, sizeof(GameSelectPageLayout)).data());
}
} // namespace gbtg
