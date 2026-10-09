#include "ghouly_spawn_scheduler.h"
#include "ghouly_requests.h"
#include "actor_lookup.h"

#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x000F3E90
i32 request_spawn_actor(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition,
    i32 force, GhoulySpawnContext& context)
{
    GhoulySpawnMessage message;
    std::strcpy(message.name, definition.name);
    context.random_state = context.random_state * 0x19660Du + 0x3C6EF35Fu;
    const float random = std::bit_cast<float>((context.random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    if (random <= definition.weight)
    {
        std::strcpy(message.variant, definition.variant);
    }
    if (std::memcmp(record.attributes_30, "aid_", 4) == 0)
    {
        std::strcpy(message.attributes, record.attributes_30);
    }
    else
    {
        std::strcpy(message.attributes, definition.first_override);
    }
    std::strcpy(message.second_override, definition.second_override);
    message.result = 0;
    if (force != 0 || spawn_resource_under_class_limit(context.resources, context.class_limits, message.name))
    {
        auto& controller = *static_cast<GhoulyController*>(record.controller_24);
        controller.message(controller, &message);
    }
    return message.result;
}

// Xbox 0x000F3FC0
i32 request_spawn_variant(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition)
{
    GhoulyVariantMessage message;
    std::strcpy(message.name, definition.variant);
    message.result = 0;
    auto& controller = *static_cast<GhoulyController*>(record.controller_24);
    controller.message(controller, &message);
    return message.result;
}

// Xbox 0x000F4030
i32 request_spawn_alternate(GhoulySpawnRecord& record, const GhoulySpawnDefinition& definition)
{
    GhoulyVariantMessage message;
    message.id = 0xAA;
    std::strcpy(message.name, definition.alternate);
    message.result = 0;
    auto& controller = *static_cast<GhoulyController*>(record.controller_24);
    controller.message(controller, &message);
    return message.result;
}

// Xbox 0x000F40A0
i32 dispatch_next_ghouly_spawn(GhoulySpawnRecord& record, i32 force, GhoulySpawnContext& context)
{
    if ((context.flags & 0x01000000u) != 0)
    {
        return -2;
    }
    const i32 count = i32(record.pool_8->cursor - record.pool_8->begin) / record.pool_8->stride;
    i32 index;
    if (record.value_14 == 1)
    {
        context.random_state = context.random_state * 0x19660Du + 0x3C6EF35Fu;
        const float random = std::bit_cast<float>((context.random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        index = _mm_cvtt_ss2si(_mm_set_ss(random * float(count)));
    }
    else if (record.value_14 == 4)
    {
        i32 attempts = 10;
        do
        {
            context.random_state = context.random_state * 0x19660Du + 0x3C6EF35Fu;
            const float random = std::bit_cast<float>((context.random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            index = _mm_cvtt_ss2si(_mm_set_ss(random * float(count)));
            if (index != record.index_28)
            {
                break;
            }
            --attempts;
        }
        while (attempts != 0);
    }
    else
    {
        index = std::bit_cast<i32>(record.value_10) % count;
    }
    auto* definition = reinterpret_cast<GhoulySpawnDefinition*>(record.pool_8->begin + record.pool_8->stride * index);
    i32 result;
    if (std::memcmp(definition->alternate, "aid_", 4) == 0)
    {
        result = request_spawn_alternate(record, *definition);
    }
    else if (std::memcmp(definition->name, "aid_", 4) == 0)
    {
        result = request_spawn_actor(record, *definition, force, context);
    }
    else
    {
        result = request_spawn_variant(record, *definition);
    }
    if (result == 1)
    {
        if (definition->flags != 0)
        {
            const u8* last = record.pool_8->cursor - record.pool_8->stride;
            auto* destination = reinterpret_cast<u8*>(definition);
            if (destination < last)
            {
                std::memmove(destination, destination + record.pool_8->stride, size_t(last - destination));
            }
            record.pool_8->cursor -= record.pool_8->stride;
            record.index_28 = -1;
            return 1;
        }
        record.index_28 = index;
        ++record.value_10;
    }
    return result;
}

// Xbox 0x000F3CF0
bool tick_ghouly_spawn_record(GhoulySpawnRecord& record, float delta, GhoulySpawnContext& context)
{
    i32 cap;
    if (record.value_b0 == 0)
    {
        cap = -1;
    }
    else if (context.maximum == 0)
    {
        cap = 99;
    }
    else
    {
        cap = std::bit_cast<i32>(u32(context.maximum) - u32(context.body_count));
        cap &= cap < 1 ? 0 : -1;
    }
    if (record.remaining_c != -1 && (record.remaining_c < cap || cap == -1))
    {
        cap = record.remaining_c;
    }
    i32 attempts = 0;
    if (delta > 0.f)
    {
        while (attempts < 10)
        {
            float consumed = 0.f;
            const i32 deficit = std::bit_cast<i32>(u32(context.minimum) - u32(context.body_count));
            if (record.timer_20 == 0.f || (record.value_b0 != 0 && (deficit & (deficit < 1 ? 0 : -1)) != 0))
            {
                if (cap < 1)
                {
                    consumed = delta;
                }
            }
            else if (!(record.timer_20 > delta))
            {
                consumed = record.timer_20;
                context.random_state = context.random_state * 0x19660Du + 0x3C6EF35Fu;
                const float random = std::bit_cast<float>((context.random_state & 0x7FFFFFu) | 0x3F800000u) - 1.f;
                const float low = std::bit_cast<float>(record.value_18);
                record.timer_20 = (std::bit_cast<float>(record.value_1c) - low) * random + low;
            }
            else
            {
                record.timer_20 -= delta;
                cap = 0;
                consumed = delta;
            }
            delta -= consumed;
            if (cap > 0)
            {
                const i32 result = dispatch_next_ghouly_spawn(record, 0, context);
                switch (result)
                {
                case 0:
                    ++attempts;
                    break;
                case 1:
                    if (record.remaining_c != -1)
                    {
                        record.remaining_c = std::bit_cast<i32>(u32(record.remaining_c) - 1u);
                    }
                    cap = std::bit_cast<i32>(u32(cap) - 1u);
                    break;
                case -2:
                    record.remaining_c = 0;
                    return true;
                case -1:
                    return record.remaining_c == 0;
                }
            }
            if (!(delta > 0.f))
            {
                return record.remaining_c == 0;
            }
        }
    }
    return record.remaining_c == 0;
}
}
