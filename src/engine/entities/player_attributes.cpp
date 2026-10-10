#include "player_attributes.h"
#include "../runtime/name_lookup.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x00019240
void initialize_runtime_player_attributes(std::span<u8> actor, std::span<const u8> parameters,
    RuntimePlayerAttributes& context)
{
    if (read_u32(parameters, 0x5BA) == 0x5F646961)
    {
        const auto resource = acquire_animation_resource(context.registry,
            reinterpret_cast<const char*>(parameters.data() + 0x5BA));
        if (resource.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        context.resource = resource;
    }
    if (read_u16(actor, 0xCDC) == 1)
    {
        const auto chance = [&](size_t offset)
        {
            if (context.resource.data() == nullptr)
            {
                return 0.f;
            }
            return read_f32(context.resource, offset);
        };
        const auto select = [&](size_t field, size_t offset)
        {
            const float threshold = chance(offset);
            context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
            const float value = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            actor[field] = value <= threshold;
        };
        select(0x96E, 0x14);
        select(0x96D, 0x10);
        const u32 timer = context.resource.data() == nullptr ? 0 : read_u32(context.resource, 0x1C);
        std::memcpy(actor.data() + 0x974, &timer, 4);
        select(0x96F, 0x20);
        select(0x970, 0x24);
        select(0x96C, 0x30);
        select(0x969, 0x34);
        const auto names = context.resource.data() == nullptr ? context.address_zero : context.resource;
        const i32 first = binary_search_names(0xE9, context.actor_tags,
            reinterpret_cast<const char*>(names.data() + 0x38));
        const u16 first_tag = first == -1 ? 0 : static_cast<u16>(static_cast<u32>(first) + 1);
        std::memcpy(actor.data() + 0x97C, &first_tag, 2);
        const i32 second = binary_search_names(0xE9, context.actor_tags,
            reinterpret_cast<const char*>(names.data() + 0x78));
        if (second == -1)
        {
            const u16 zero = 0;
            std::memcpy(actor.data() + 0x97E, &zero, 2);
            return;
        }
        const u16 second_tag = static_cast<u16>(static_cast<u32>(second) + 1);
        std::memcpy(actor.data() + 0x97E, &second_tag, 2);
    }
}

// Xbox 0x00133A50
u16 find_runtime_player_tag_index(u16 tag, const u8* rows, const char* const* names)
{
    u16 index = 1;
    do
    {
        const char* row = reinterpret_cast<const char*>(rows);
        if (std::strcmp(row, names[0]) == 0)
        {
            return 0;
        }
        if (std::strcmp(row, names[tag]) == 0)
        {
            return index;
        }
        rows += 0x87;
        ++index;
    }
    while (true);
}
}
