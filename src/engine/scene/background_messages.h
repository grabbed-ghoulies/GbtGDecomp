#pragma once

#include "background_part_destruction.h"
#include "background_doors.h"
#include "../entities/actor_base_messages.h"
#include "../frontend/score_awards.h"
#include "../../gbtg/text/localtext_resources.h"

namespace gbtg
{
// The reply a background sends to whoever broke a part (id 0x50, rumble name at +4).
struct BackgroundRumbleMessage
{
    u32 id;
    const char* name;
};
struct BackgroundMessageContext
{
    ActorBaseContext& base;
    const std::function<void(std::span<u32>)>& base_message; // 0x0003EF70 over the retail message words.
    BackgroundPartDestruction& parts;
    std::span<const u8> zero; // Retail reads of the part header through address zero.
    RuntimeScoreAwards& score;
    LocalTextResourceContext& text;
    std::span<const u8> text_resource; // DAT_0054629C.
    char16_t* scratch; // DAT_00507E78.
    u32& broken_parts; // DAT_00545978.
    const char* rumble_fallback; // DAT_00421E58.
    entity_detail::ListHead& model_list; // DAT_00511AE8.
    RuntimeDoorContext& doors;
    const std::function<Actor*(u32)>& actor_at; // Retail actor address to actor; raises on unknown or zero.
    const std::function<std::span<const u8>(u32, size_t)>& resolve; // Hit lists, records and actor words.
    const std::function<void(Actor&, const void*)>& message; // Actor vtable +0xC.
};
// One hit record (stride 0x54): position +0, part +0x18. Breaks the part when no row holds it, and awards its
// score caption when asked.
u32 break_background_part_hit(Actor& actor, std::span<const u8> hit, u32 award, u32 category, u32 value,
    BackgroundMessageContext& context); // Xbox 0x000441F0, EAX actor
// Hit list: begin +4, end +8. Returns the parts broken and stores the last one.
i32 break_background_part_hits(Actor& actor, u32 hits, u32 award, u32 category, u32 value, i32& part,
    BackgroundMessageContext& context); // Xbox 0x000441A0, EAX hit list
void handle_background_message(Actor& actor, std::span<u32> message,
    BackgroundMessageContext& context); // Xbox 0x00043E50
}
