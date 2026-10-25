#include "score_awards.h"
#include "../audio/listener_settings.h"
#include "../../gbtg/text/localtext_resources.h"

namespace gbtg
{
// Xbox 0x0012DE90
void award_runtime_score(i32 points, u32 category, const char16_t* text, i32 quiet,
    RuntimeScoreAwards& context)
{
    long double value = points;
    if (points < 0)
    {
        value = value + 4294967296.0L; // DAT_0032B144.
    }
    if (category >= context.multipliers.size())
    {
        throw Error("score award category lies outside DAT_003130B4");
    }
    value = value * context.multipliers[category];
    const u32 award = u32(truncate_runtime_float(value));
    context.total = context.total + award;
    context.running_total = context.running_total + award;
    if (quiet == 0 && (context.runtime_flags & 0x10000u) != 0)
    {
        i32 slot = 0;
        do
        {
            if (context.points[size_t(slot)] == 0)
            {
                copy_xapi_wide_text(context.texts.data() + size_t(slot) * 0x100, text);
                context.categories[size_t(slot)] = category;
                context.points[size_t(slot)] = award;
                break;
            }
            ++slot;
        }
        while (slot < 10);
        if (context.refresh != 0)
        {
            context.refresh = 0;
        }
    }
}

// Xbox 0x0012DF90
void clear_runtime_score_awards(RuntimeScoreAwards& context)
{
    for (size_t slot = 0; slot < 10; ++slot)
    {
        context.categories[slot] = 0; // DAT_004326FC
        context.texts[slot * 0x100] = 0; // DAT_00432724
        context.points[slot] = 0; // DAT_00433B24
    }
    if (context.refresh != 2)
    {
        context.refresh = 2; // DAT_004326E8
    }
}
}
