#include "intro_notices.h"

namespace gbtg
{
// Xbox 0x0013FF70
u32 runtime_intro_notice_ready(const RuntimeIntroNotices& context)
{
    const u32 flags = context.runtime_flags;
    if ((context.scene_flags & 8) != 0 || (flags & 0x200000) != 0 || (flags & 0x4000) != 0
        || context.intro_pending != 0)
    {
        return 0;
    }
    const auto* node = static_cast<const entity_detail::ListHead*>(context.cases.next);
    if (node == nullptr)
    {
        return 1;
    }
    const EntityBitBlock* bits = context.case_body_bits(node);
    if (bits == nullptr)
    {
        return 1;
    }
    return (bits->bytes[3] & 8) != 0 ? 0 : 1;
}

// Xbox 0x0013FF00
u32 queue_runtime_intro_notice(const char* name, RuntimeIntroNotices& context)
{
    if (runtime_intro_notice_ready(context) == 0)
    {
        return 0;
    }
    size_t index = 0;
    char value;
    do
    {
        value = name[index];
        context.intro_name[index] = value;
        ++index;
    } while (value != '\0');
    context.intro_pending = 1;
    return 1;
}
}
