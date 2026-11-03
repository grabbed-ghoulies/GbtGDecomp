#include "model_blendshape_draw.h"

#include <cstring>

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
    if (i32(image.get(node, blendshape_node::target_count)) - 1 > 0)
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
        } while (index < i32(image.get(node, blendshape_node::target_count)) - 1);
    }
    u32 list = image.get(node, blendshape_node::targets);
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
            const u8 kind = image.get(record, model_vertex_buffer_field::kind);
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
            list = image.get(node, blendshape_node::targets);
            ++attribute;
        } while (attribute < i32(image.word(image.word(list), 0x24)));
    }
}

// Xbox 0x00157650
void blend_runtime_model_blendshape(u32 node, RuntimeBlendshapeNodeInstance& instance,
    RuntimeModelBlendshapeDraw& context)
{
    const auto& image = context.tree.image;
    runtime_render_state(context.tree.renderer); // 0x00156DD0
    const auto targets = [&](u32 target)
    {
        return image.word(image.get(node, blendshape_node::targets), target * 4); // Node +0x20's target list.
    };
    const u32 attributes = image.word(targets(0), 0x24);
    for (u32 attribute = 0; attribute < attributes; ++attribute)
    {
        if (attribute >= context.streams.size() || attribute >= instance.buffers.size())
        {
            throw Error("blend shape attribute lies outside the render state streams");
        }
        RuntimeBlendshapeVertexBuffer& buffer = instance.buffers[attribute]; // Instance +4, stride 0x18.
        const u32 record = image.word(targets(0), 0x20) + attribute * 0x18;
        const u8 kind = image.get(record, model_vertex_buffer_field::kind);
        if ((context.tree.mask & 1) != 0 && (kind == 9 || kind == 10))
        {
            const u16 count = image.get(node, blendshape_node::target_count);
            float sum = 0.f;
            for (u32 index = 0; index + 1 < count; ++index)
            {
                sum = sum + instance.weights[index];
            }
            const u8* source = image.pointer(image.get(record, model_vertex_buffer_field::data));
            const u32 size = buffer.words[5]; // +0x14.
            u8* output;
            if (sum == 0.f)
            {
                output = const_cast<u8*>(buffer.data); // +0x10.
            }
            else
            {
                u8* scratch = instance.attribute_targets[attribute]; // +0x184.
                std::memcpy(scratch, source, size);
                for (u32 target = 1; target < count; ++target)
                {
                    const u32 target_record = image.word(targets(target), 0x20) + attribute * 0x18;
                    const float weight = instance.weights[target - 1];
                    const u8* from = image.pointer(image.word(target_record, 0x10));
                    const u8* end = from + image.word(target_record, 0x14);
                    if (weight == 0.f)
                    {
                        continue;
                    }
                    u8* to = scratch;
                    while (from < end)
                    {
                        float vertex[3];
                        float blended[3];
                        std::memcpy(vertex, from, 12);
                        std::memcpy(blended, to, 12);
                        blended[0] = vertex[0] * weight + blended[0];
                        blended[1] = vertex[1] * weight + blended[1];
                        blended[2] = vertex[2] * weight + blended[2];
                        std::memcpy(to, blended, 12);
                        const u8 stride = image.byte(target_record, 0);
                        from += stride;
                        to += stride;
                    }
                }
                output = const_cast<u8*>(buffer.data); // D3DVertexBuffer_Lock2 on instance +8.
                source = scratch;
            }
            std::memcpy(output, source, size);
        }
        context.streams[attribute] = context.buffer_resource(buffer); // Render state +0x78.
    }
}

// Xbox 0x00157A50
void dispatch_runtime_model_blendshape(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelBlendshapeDraw& context)
{
    const auto& image = context.tree.image;
    const auto found = storage.blendshape_nodes.find(image.get(node, model_node_field::instance_offset));
    if (found == storage.blendshape_nodes.end())
    {
        throw Error("blend shape draw has no instance copy");
    }
    if (image.get(node, blendshape_node::blends) != 0)
    {
        blend_runtime_model_blendshape(node, found->second, context); // 0x00157650
    }
    else
    {
        select_runtime_model_blend_targets(node, found->second, context); // 0x00157880
    }
    queue_runtime_model_group(node, context.tree); // 0x00158960
}
}
