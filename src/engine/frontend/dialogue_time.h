#pragma once
#include "../../gbtg/text/localtext_resources.h"
namespace gbtg
{
void append_dialogue_minutes(char16_t* output, float seconds, i32 omit_days,
    LocalTextResourceContext& context); // Xbox 0x000FBB20
void append_dialogue_seconds(char16_t* output, float seconds, i32 omit_days,
    LocalTextResourceContext& context); // Xbox 0x000FB860
}
