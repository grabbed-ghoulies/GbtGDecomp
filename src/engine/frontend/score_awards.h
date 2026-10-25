#pragma once

#include "gbtg/types.h"
#include <span>

namespace gbtg
{
// Score totals and the ten pending award captions.
struct RuntimeScoreAwards
{
    u32& total; // DAT_0054597C.
    u32& running_total; // DAT_00545980.
    const u32& runtime_flags; // DAT_0050EA9C; bit 16 queues captions.
    std::span<const float> multipliers; // DAT_003130B4, by award category.
    std::span<char16_t> texts; // DAT_00432724, ten rows of 0x100 units.
    std::span<u32, 10> categories; // DAT_004326FC.
    std::span<u32, 10> points; // DAT_00433B24; zero marks a free caption.
    u32& refresh; // DAT_004326E8.
};
// Scales the points by the category multiplier, adds them to both totals and, unless quiet, queues the caption.
void award_runtime_score(i32 points, u32 category, const char16_t* text, i32 quiet,
    RuntimeScoreAwards& context); // Xbox 0x0012DE90, EAX points, EBX category
// The scene teardown's: the ten captions are cleared (category, first text unit and points) and the scoreboard
// refreshes them (DAT_004326E8 2).
void clear_runtime_score_awards(RuntimeScoreAwards& context); // Xbox 0x0012DF90
}
