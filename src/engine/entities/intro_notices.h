#pragma once
#include "gbtg/entity.h"
#include "entity_types.h"
#include <functional>
#include <span>

namespace gbtg
{
// Gate and storage for the queued intro notice (DAT_00545B38/DAT_00545B44).
struct RuntimeIntroNotices
{
    const u32& runtime_flags; // DAT_0050EA9C.
    const u8& scene_flags; // **(DAT_00506C74+8).
    u32& intro_pending; // DAT_00545B38.
    std::span<char> intro_name; // DAT_00545B44.
    const entity_detail::ListHead& cases; // DAT_00511B3C; nodes at actor +0x93C.
    const std::function<const EntityBitBlock*(const entity_detail::ListHead*)>& case_body_bits;
};
u32 runtime_intro_notice_ready(const RuntimeIntroNotices& context); // Xbox 0x0013FF70
u32 queue_runtime_intro_notice(const char* name, RuntimeIntroNotices& context); // Xbox 0x0013FF00, ESI name
}
