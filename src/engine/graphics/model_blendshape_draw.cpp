#include "model_blendshape_draw.h"

namespace gbtg
{
// Xbox 0x00157880
void select_runtime_model_blend_targets(u32 node, const RuntimeBlendshapeNodeInstance& instance,
    RuntimeModelBlendshapeDraw& context)
{
    const auto& image = context.tree.image;
    runtime_render_state(context.tree.renderer);
    auto& slots = context.weights;
    slots = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    std::array<i32, 8> targets{};
    i32 slot = 0;
    i32 index = 0;
    if (i32(image.half(node, 0x24)) - 1 > 0)
    {
        do
        {
            float weight = instance.weights[index];
            if (weight > slots[size_t(slot)])
            {
                slots[size_t(slot)] = weight;
                targets[size_t(slot)] = index + 1;
                // The weakest slot receives the next stronger weight.
                for (i32 candidate = 0; candidate < 7; ++candidate)
                {
                    if (weight > slots[size_t(candidate)])
                    {
                        weight = slots[size_t(candidate)];
                        slot = candidate;
                    }
                }
                if (weight > slots[7])
                {
                    slot = 7;
                }
            }
            ++index;
        } while (index < i32(image.half(node, 0x24)) - 1);
    }
    u32 list = image.word(node, 0x20);
    i32 attribute = 0;
    if (0 < i32(image.word(image.word(list), 0x24)))
    {
        do
        {
            if (size_t(attribute) >= context.streams.size())
            {
                throw Error("blend shape attribute lies outside the render state streams");
            }
            const u32 record = image.word(image.word(list), 0x20) + u32(attribute) * 0x18;
            const u8 kind = image.byte(record, 1);
            if (kind < 0x1B || 0x22 < kind)
            {
                context.streams[size_t(attribute)] = record;
            }
            else
            {
                const u32 records = image.word(image.word(list, u32(targets[kind - 0x1B]) * 4), 0x20);
                u32 found = 0;
                while (image.byte(records, found * 0x18 + 1) != 9)
                {
                    ++found;
                }
                context.streams[size_t(attribute)] = records + found * 0x18;
            }
            list = image.word(node, 0x20);
            ++attribute;
        } while (attribute < i32(image.word(image.word(list), 0x24)));
    }
}

// Xbox 0x00157A50
void dispatch_runtime_model_blendshape(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelBlendshapeDraw& context)
{
    const auto& image = context.tree.image;
    const auto found = storage.blendshape_nodes.find(image.word(node, 0xC));
    if (found == storage.blendshape_nodes.end())
    {
        throw Error("blend shape draw has no instance copy");
    }
    if (image.byte(node, 0x26) != 0)
    {
        throw Error("untranslated blend shape vertex update 0x00157650");
    }
    select_runtime_model_blend_targets(node, found->second, context);
    queue_runtime_model_group(node, context.tree);
}
}
