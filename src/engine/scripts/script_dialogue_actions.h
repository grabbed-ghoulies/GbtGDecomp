#pragma once
#include "../frontend/dialogue_activation.h"

namespace gbtg
{
struct RuntimeScriptActorQueryContext;
struct RuntimeScriptDialogueContext
{
    RuntimeDialogueActivationContext& dialogue;
    RuntimeScriptActorQueryContext& actors;
    u32& active; // DAT_00512474.
    u32& scene_flags; // DAT_0050EA9C.
    u32& random; // DAT_0054DD7C.
    char* variant_name; // DAT_00508EB8.
};
// Complete type 0x14 branch of the retail action handler.
int execute_runtime_script_dialogue(const u8* command, i32 gate,
    RuntimeScriptDialogueContext& context); // Xbox 0x001274B0, type 0x14
}
