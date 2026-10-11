#include "player_mind_request_update.h"
#include "body_animation_commands.h"

namespace gbtg
{
// Xbox 0x0007D360
i32 update_runtime_player_mind_request(RuntimePlayerMindRequestUpdate& context, RuntimePlayerMindRequest& request)
{
    switch (request.kind)
    {
    case 8:
    {
        const auto* name = reinterpret_cast<const char*>(request.bytes.data() + 0x10);
        const u32 index = runtime_body_flag_name_index(name, context.flag_names);
        if ((context.flags.bytes[index >> 3] & u8(1u << (index & 7))) == 0)
        {
            return 1;
        }
        [[fallthrough]];
    }
    case 7:
        select_runtime_body_action(context.body, context.effect_action_key(), context.actions);
        return 1;
    case 0x16:
        update_runtime_player_control(context.control);
        return 0;
    case 0x2E:
        return i32(handle_runtime_player_navigation_request(context.navigation, request.bytes));
    case 0x31:
        select_runtime_body_action(context.body, context.contact_action_key(), context.actions);
        return 1;
    default:
        return update_runtime_parent_mind_request(context.parent, request.bytes);
    }
}
}
