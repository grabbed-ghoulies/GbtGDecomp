#include "cinema_mind_lifecycle.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
void word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x0007C500
void runtime_cinema_mind_storage_sizes(ObjectSizes& sizes)
{
    sizes = {0xD0, 0x128};
}

// Xbox 0x0007CC00
void load_runtime_cinema_mind_resource(RuntimeCinemaMindLifecycle& context, std::span<const u8> parameters)
{
    if (read_u32(parameters, 0xA8) == 0x5F646961)
    {
        context.resource = acquire_animation_resource(context.registry,
            reinterpret_cast<const char*>(parameters.data() + 0xA8));
        if (context.resource.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        word(context.bytes, 0x84, context.address_of(context.resource.data()));
        if (context.resource.data() != nullptr)
        {
            i32 index = 0;
            size_t name = 0x10;
            do
            {
                if (context.resource[name] == 0)
                {
                    break;
                }
                ++index;
                name += 0x80;
                word(context.bytes, 0x88, read_u32(context.bytes, 0x88) + 1);
            }
            while (index < 3);
        }
    }
    if (read_u32(context.bytes, 0x88) != 0)
    {
        const float minimum = read_f32(context.resource, 0);
        const float maximum = read_f32(context.resource, 4);
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float random = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        const float delay = (maximum - minimum) * random + minimum;
        word(context.bytes, 0x8C, std::bit_cast<u32>(delay));
    }
}

// Xbox 0x0007C7D0
bool initialize_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context, std::span<const u8> parameters)
{
    word(context.bytes, 0x7C, read_u32(parameters, 0xA0));
    word(context.bytes, 0x80, read_u32(parameters, 0xA4));
    if (read_u16(context.actor, 0x1FC) == 0)
    {
        word(context.bytes, 0x64, read_u32(context.bytes, 0x64) & 0xFFFFFFDFu);
        word(context.bytes, 0x6C, 0);
    }
    word(context.bytes, 0xA0, 0);
    word(context.bytes, 0x9C, 0);
    word(context.bytes, 0xA8, 0);
    word(context.bytes, 0xA4, 0);
    word(context.bytes, 0xB0, 1);
    word(context.bytes, 0xAC, 0);
    word(context.bytes, 0xB8, 0);
    word(context.bytes, 0xB4, 0);
    word(context.bytes, 0xC0, 0);
    word(context.bytes, 0xBC, 0);
    word(context.bytes, 0xC8, 1);
    word(context.bytes, 0xC4, 0);
    load_runtime_cinema_mind_resource(context, parameters);
    return true;
}

// Xbox 0x0007C880
void destroy_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context)
{
    if (read_u32(context.bytes, 0x84) != 0)
    {
        release_animation_resource(context.registry, context.resource);
        word(context.bytes, 0x84, 0);
        context.resource = {}; // Native pointer view of the cleared field.
    }
}
}
