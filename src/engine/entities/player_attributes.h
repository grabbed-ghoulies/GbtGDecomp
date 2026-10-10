#pragma once
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"

namespace gbtg
{
struct RuntimePlayerAttributes
{
    std::span<const u8>& resource; // actor +0x964, retained registry reference.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random;
    const char* const* actor_tags; // 0x0042A854, 0xE9 entries.
    std::span<const u8> address_zero; // Required retail zero-address view for name reads.
};
void initialize_runtime_player_attributes(std::span<u8> actor, std::span<const u8> parameters,
    RuntimePlayerAttributes& context); // Xbox 0x00019240
u16 find_runtime_player_tag_index(u16 tag, const u8* rows,
    const char* const* names); // Xbox 0x00133A50, low AX result consumed by caller.
}
