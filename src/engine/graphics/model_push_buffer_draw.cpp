#include "model_push_buffer_draw.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00158240
bool runtime_push_buffer_visible(u32 node, ModelPushBufferDrawContext& c)
{
    const auto& image = c.tree.image;
    const i32 index = std::bit_cast<std::int8_t>(image.byte(node, 0x3D));
    if (index == -1)
    {
        return true;
    }
    RuntimeModelRenderState& state = runtime_render_state(c.tree.renderer);
    const u32 doubled = static_cast<u32>(index * 2);
    u32 remainder = doubled & 0x80000007;
    if (std::bit_cast<i32>(remainder) < 0)
    {
        remainder = ((remainder - 1) | 0xFFFFFFF8) + 1;
    }
    const i32 signed_doubled = std::bit_cast<i32>(doubled);
    const i32 offset = (signed_doubled + ((signed_doubled >> 31) & 7)) >> 3;
    const u32 flag = c.flag_origins.at(state.instance)[offset];
    return ((flag >> (static_cast<u8>(remainder) & 0x1F)) & 3) == image.byte(node, 0x3E);
}
// Xbox 0x001582A0
void draw_runtime_push_buffer(u32 node, ModelPushBufferDrawContext& c)
{
    const auto& image = c.tree.image;
    if (runtime_push_buffer_visible(node, c))
    {
        if (image.byte(node, 0x3C) != 0)
        {
            c.tree.set_cull_mode(0);
        }
        u32 index = 0;
        if (image.word(node, 0x20) != 0)
        {
            do
            {
                c.draw_indexed(image.word(image.word(node, 0x34), index * 4),
                    image.word(image.word(node, 0x38), index * 4),
                    image.word(image.word(node, 0x30), index * 4));
                ++index;
            } while (index < image.word(node, 0x20));
        }
        if (image.byte(node, 0x3C) != 0)
        {
            c.tree.set_cull_mode(c.tree.cull_mode);
        }
    }
    queue_runtime_model_group(node, c.tree);
}
// Xbox 0x001580E0
void draw_runtime_background_push_buffer(u32 node, std::span<const u8> instance, ModelPushBufferDrawContext& c)
{
    const auto& image = c.tree.image;
    ++c.background_tested;
    const u32 key = read_u32(instance, image.word(node, 0xC) + 4);
    const RuntimeModelRenderState& state = runtime_render_state(c.tree.renderer);
    if (key != 0xFFFFFFFFu && key != state.background_key)
    {
        ++c.background_skipped;
        queue_runtime_model_group(node, c.tree);
        return;
    }
    if (image.byte(node, 0x3F) == 0)
    {
        draw_runtime_push_buffer(node, c);
        return;
    }
    ++c.background_skipped;
    queue_runtime_model_sibling(node, c.tree);
}
void install_runtime_push_buffer_dispatch(ModelPushBufferDrawContext& c,
    std::unordered_map<u32, std::function<void(u32, void*)>>& callbacks)
{
    callbacks[0x001582A0] = [&c](u32 node, void*)
    {
        draw_runtime_push_buffer(node, c);
    };
    callbacks[0x001580E0] = [&c](u32 node, void* storage)
    {
        draw_runtime_background_push_buffer(node, static_cast<RuntimeModelInstanceStorage*>(storage)->bytes, c);
    };
}
}
