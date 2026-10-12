#include "camera_mind_requests.h"
#include "../scripts/script_table_lookup.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00078170
i32 update_camera_mind_request(RuntimePlayerMindRequest& request, RuntimeCameraMindRequests& context)
{
    switch (read_u16(request.bytes, 8))
    {
    case 1:
        return i32(update_camera_target_request(context.view, request.bytes,
            std::bit_cast<float>(context.request_address(request)), context.rig));
    case 3:
        throw Error("camera mind request kind 3 (0x00079B80) is not translated");
    case 4:
        throw Error("camera mind request kind 4 (0x0007A080) is not translated");
    case 0x12:
        throw Error("camera mind request kind 0x12 (0x0007A910) is not translated");
    default:
        return context.parent_update(request);
    }
}

// Xbox 0x0007AEF0
i32 initialize_camera_mind_request(RuntimePlayerMindRequest& request, RuntimeCameraMindRequests& context)
{
    std::span<u8> bytes = request.bytes;
    const u16 kind = read_u16(bytes, 8);
    if (kind == 0x29)
    {
        context.random = context.random * 0x19660D + 0x3C6EF35F;
        const float unit = std::bit_cast<float>((context.random & 0x7FFFFF) | 0x3F800000);
        const float low = read_f32(bytes, 0x10);
        const float high = read_f32(bytes, 0x14);
        const float value = (high - low) * (unit - 1.f) + low; // DAT_0032B0F8.
        std::memcpy(bytes.data() + 0x18, &value, 4);
    }
    u16 key;
    if (kind == 1 || kind == 0x12)
    {
        key = 0x19;
        if (read_u16(context.view.body, 0x748) == key)
        {
            return 1;
        }
    }
    else
    {
        key = 0x1B;
    }
    if (context.actions.table != nullptr)
    {
        u32 parameters = 0;
        const u8* row = find_runtime_script_table_row(context.actions.table, key, parameters);
        if (row != nullptr)
        {
            if (parameters != 0)
            {
                throw Error("camera mind action row carries a template token the selection has no view of");
            }
            select_runtime_body_action_row(context.view.body, row, nullptr, context.actions);
        }
    }
    return 1;
}
}
