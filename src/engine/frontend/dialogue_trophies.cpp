#include "dialogue_trophies.h"

namespace gbtg
{
// Xbox 0x0004A400
i32 dialogue_score_tier(i32 score, const std::array<i32, 4>& thresholds)
{
    if (score == -1)
    {
        return 1;
    }
    if (thresholds[0] < thresholds[1])
    {
        if (thresholds[3] <= score)
        {
            return 6;
        }
        if (thresholds[2] <= score)
        {
            return 5;
        }
        if (thresholds[1] <= score)
        {
            return 4;
        }
        if (thresholds[0] <= score)
        {
            return 3;
        }
    }
    else
    {
        if (score <= thresholds[3])
        {
            return 6;
        }
        if (score <= thresholds[2])
        {
            return 5;
        }
        if (score <= thresholds[1])
        {
            return 4;
        }
        if (score <= thresholds[0])
        {
            return 3;
        }
    }
    return 2;
}

// Xbox 0x000572D0
void mark_dialogue_pause_icon(i32 token, u32& flags)
{
    switch (token)
    {
    case 0x4B:
        flags |= 0x210;
        break;
    case 0x4C:
        flags |= 0x90;
        return;
    case 0x4D:
        flags |= 0x110;
        return;
    case 0x4E:
        flags |= 0x50;
        return;
    case 0x4F:
        flags |= 0x30;
        return;
    }
}

// Xbox 0x00057330
void select_final_dialogue_trophy(std::span<const i32> scores, i32 selected, DialogueTrophyContext& context)
{
    i32 token;
    switch (dialogue_score_tier(scores[selected], context.thresholds))
    {
    case 3:
        token = 0x52;
        break;
    case 4:
        token = 0x53;
        break;
    case 5:
        token = 0x54;
        break;
    case 6:
        token = 0x55;
        break;
    default:
        return;
    }
    select_dialogue_icon(context.resources, token, 2, context.name_buffer,
        context.names, context.registry, context.error);
    context.flags |= 8;
}

// Xbox 0x000573A0
void select_high_dialogue_trophy(i32 score, DialogueTrophyContext& context)
{
    i32 token;
    switch (dialogue_score_tier(score, context.thresholds))
    {
    case 3:
        token = 0x52;
        break;
    case 4:
        token = 0x53;
        break;
    case 5:
        token = 0x54;
        break;
    case 6:
        token = 0x55;
        break;
    default:
        return;
    }
    select_dialogue_icon(context.resources, token, 2, context.name_buffer,
        context.names, context.registry, context.error);
    context.flags |= 8;
}

}
