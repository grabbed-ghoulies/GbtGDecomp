#include "script_dialogue_actions.h"
#include "script_actor_query.h"
#include "script_actions.h"
#include "script_vm.h"
#include "../cutscenes/event_assets.h"
#include "../runtime/crt_format.h"
#include "gbtg/runtime_localtext.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
u32 dialogue_command_word(const u8* command, u32 offset)
{
    u32 value;
    std::memcpy(&value, command + offset, sizeof(value));
    return value;
}
}
// Xbox 0x001274B0, type 0x14
int execute_runtime_script_dialogue(const u8* command, i32 gate,
    RuntimeScriptDialogueContext& context)
{
    if (gate != 0)
    {
        return 2;
    }
    if (context.active != 0)
    {
        if (context.dialogue.state.mode != 0)
        {
            return 0;
        }
        script_update_decay_latches(0xFFFF, 1);
        context.active = 0;
        return 2;
    }
    const u16 tag = static_cast<u16>(script_find_actor_tag(
        reinterpret_cast<const char*>(command + 8)));
    if (tag > 1 && count_runtime_script_actor_tags(0,
        std::bit_cast<i16>(tag), nullptr, context.actors) == 0)
    {
        return 2;
    }
    if (dialogue_command_word(command, 0x8C) != 0 && scene_end_flag_set())
    {
        return 2;
    }
    if (dialogue_command_word(command, 0x90) != 0)
    {
        const u32 kind = dialogue_command_word(command, 0x94);
        if (kind == 0)
        {
            if ((context.scene_flags & 0x08000000) != 0)
            {
                return 2;
            }
        }
        else if (kind == 1)
        {
            if ((context.scene_flags & 0x10000000) != 0)
            {
                return 2;
            }
        }
    }
    const char* key = reinterpret_cast<const char*>(command + 0x48);
    if (text_key_present(key))
    {
        const u32 variants = dialogue_command_word(command, 0x88);
        if (variants != 0)
        {
            const i32 variant = select_cutscene_random_index(context.random,
                std::bit_cast<i32>(variants + 1), 1);
            scene_detail::crt_format_string(context.variant_name, "%s%d", key, variant);
            key = context.variant_name;
        }
        if (enqueue_runtime_dialogue_key(key, 0, context.dialogue) != 0)
        {
            script_update_decay_latches(0xFFFF, 0);
            context.active = 1;
            if (dialogue_command_word(command, 0x90) != 0)
            {
                const u32 kind = dialogue_command_word(command, 0x94);
                if (kind == 0)
                {
                    context.scene_flags |= 0x08000000;
                }
                else if (kind == 1)
                {
                    context.scene_flags |= 0x10000000;
                    return 0;
                }
            }
        }
        return 0;
    }
    return 2;
}
}
