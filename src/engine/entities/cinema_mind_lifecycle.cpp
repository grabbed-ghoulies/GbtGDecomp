#include "cinema_mind_lifecycle.h"
#include "mind_fields.h"
#include <bit>
#include <cstring>

namespace gbtg
{
// Xbox 0x0007C500
void runtime_cinema_mind_storage_sizes(ObjectSizes& sizes)
{
    sizes = {sizeof(CinemaMindRecord), cinema_mind_parameter::kSize};
}

// Xbox 0x0007CC00
void load_runtime_cinema_mind_resource(RuntimeCinemaMindLifecycle& context, std::span<const u8> parameters)
{
    if (std::memcmp(field_data(parameters, cinema_mind_parameter::sound_resource), "aid_", 4) == 0)
    {
        context.resource =
            acquire_animation_resource(context.registry, field_data(parameters, cinema_mind_parameter::sound_resource));
        if (context.resource.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
        cinema_mind_record(context.bytes).sound_resource = context.address_of(context.resource.data());
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
                cinema_mind_record(context.bytes).sound_count = cinema_mind_record(context.bytes).sound_count + 1;
            }
            while (index < 3);
        }
    }
    if (cinema_mind_record(context.bytes).sound_count != 0)
    {
        const float minimum = read_f32(context.resource, 0);
        const float maximum = read_f32(context.resource, 4);
        context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
        const float random = std::bit_cast<float>((context.random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        const float delay = (maximum - minimum) * random + minimum;
        cinema_mind_record(context.bytes).sound_timer = delay;
    }
}

// Xbox 0x0007C7D0
bool initialize_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context, std::span<const u8> parameters)
{
    cinema_mind_record(context.bytes).roam_radius = get(parameters, cinema_mind_parameter::roam_radius);
    cinema_mind_record(context.bytes).target_radius = get(parameters, cinema_mind_parameter::target_radius);
    if (read_u16(context.actor, 0x1FC) == 0)
    {
        mind_record(context.bytes).target_flags = mind_record(context.bytes).target_flags & 0xFFFFFFDFu;
        mind_record(context.bytes).target_range = 0.f;
    }
    cinema_mind_record(context.bytes).unk_A0 = 0;
    cinema_mind_record(context.bytes).unk_9C = 0;
    cinema_mind_record(context.bytes).unk_A8 = 0;
    cinema_mind_record(context.bytes).unk_A4 = 0;
    cinema_mind_record(context.bytes).unk_B0 = 1;
    cinema_mind_record(context.bytes).unk_AC = 0;
    cinema_mind_record(context.bytes).unk_B8 = 0;
    cinema_mind_record(context.bytes).unk_B4 = 0;
    cinema_mind_record(context.bytes).unk_C0 = 0;
    cinema_mind_record(context.bytes).unk_BC = 0;
    cinema_mind_record(context.bytes).unk_C8 = 1;
    cinema_mind_record(context.bytes).unk_C4 = 0;
    load_runtime_cinema_mind_resource(context, parameters);
    return true;
}

// Xbox 0x0007C880
void destroy_runtime_cinema_mind(RuntimeCinemaMindLifecycle& context)
{
    if (cinema_mind_record(context.bytes).sound_resource != 0)
    {
        release_animation_resource(context.registry, context.resource);
        cinema_mind_record(context.bytes).sound_resource = 0;
        context.resource = {}; // Native pointer view of the cleared field.
    }
}
}
