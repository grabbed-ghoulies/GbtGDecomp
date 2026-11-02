#include "mind_target_policy.h"
#include "player_mind_targets.h"
#include "mind_fields.h"

namespace gbtg
{
// Xbox 0x00141580
bool runtime_mind_target_is_movable(const RuntimeMindTarget& target)
{
    if (get(target.bytes, mind_target_field::kind) == 0)
    {
        // DAT_0031509C, three retail actorBodyFlagId words.
        constexpr std::array<u32, 3> disallowed{31, 36, 37};
        for (u32 bit : disallowed)
        {
            if (script_mind_test_bit(target.body, bit))
            {
                return false;
            }
        }
    }
    return true;
}

// Xbox 0x00029C80
bool runtime_mind_target_is_eligible(std::span<const u8> mind,
    std::span<const u8> actor_bytes, const Actor& actor, u32 actor_address,
    const RuntimeMindTarget& target, bool adjust_for_actor_state)
{
    if (get(target.bytes, mind_target_field::unk_18) != 0)
    {
        return false;
    }
    u32 flags = get(mind, mind_field::target_flags);
    const u32 categories = get(mind, mind_field::target_categories);
    if (adjust_for_actor_state)
    {
        if (read_f32(actor_bytes, 0x978) > 0.f || actor_bytes[0x969] != 0)
        {
            flags = (flags & 0xFFFFFFFBu) | 0x10;
        }
    }
    flags = (flags & 0xFFFFFF5Fu) | 8;
    if (get(target.bytes, mind_target_field::actor) == actor_address
        || (categories & (1u << (get(target.bytes, mind_target_field::kind) & 31))) == 0
        || !runtime_mind_target_allowed(target, flags))
    {
        return false;
    }
    return runtime_mind_target_reachable(actor, nullptr, target, flags);
}
}
