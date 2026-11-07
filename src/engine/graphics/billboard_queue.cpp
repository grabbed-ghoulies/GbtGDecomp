#include "billboard_queue.h"
#include "boot_shader_tables.h"
#include "d3dx_view.h"
#include "../frontend/dialogue_draw_outer.h"
#include "gbtg/anim.h"
#include "gbtg/model_relocation.h"
#include "../camera/display_view.h"

#include <bit>
#include <cstdlib>
#include <cstring>

namespace gbtg
{
BillboardQueue g_billboard_queue;

namespace
{
u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}

// 0x0005C360: farther entries first; equal distances compare as -1.
int compare_billboards(const void* first, const void* second)
{
    const auto* a = *static_cast<BillboardQueueEntry* const*>(first);
    const auto* b = *static_cast<BillboardQueueEntry* const*>(second);
    return a->distance < b->distance ? 1 : -1;
}
}

// Xbox 0x0005C1A0
void queue_billboard(BillboardQueue& queue, const u8* sprite, u32 color, u32 frame, const Vec3& position,
    const float* size, float rotation)
{
    if (queue.count < 0x800)
    {
        auto& entry = queue.entries[size_t(queue.count)];
        entry.sprite = sprite;
        entry.position = position;
        entry.size = {size[0], size[1]};
        entry.rotation = rotation;
        entry.color = color;
        entry.frame = frame;
        ++queue.count;
    }
}

// Xbox 0x0005C390
void sort_billboards(BillboardQueue& queue, const Vec3& view)
{
    for (i32 index = 0; index < queue.count; ++index)
    {
        auto& entry = queue.entries[size_t(index)];
        const float x = view.x - entry.position.x;
        const float y = view.y - entry.position.y;
        const float z = view.z - entry.position.z;
        entry.distance = x * x + y * y + z * z;
        queue.order[size_t(index)] = &entry;
    }
    std::qsort(queue.order.data(), size_t(queue.count), sizeof(BillboardQueueEntry*), compare_billboards);
}

// Xbox 0x0005C410
void begin_billboard_draw(BillboardFlushContext& c)
{
    auto& draw = c.draw;
    auto& d3d = draw.d3d;
    ++draw.override_count;
    draw.fog_disabled = 1;
    set_runtime_render_fog_enabled(draw.renderer, 0);
    d3d.cull(0);
    d3d.simple_state(0x40350, 0x8006);
    deferred(draw, 0x181C98) = 0x8006;
    d3d.simple_state(0x40344, 0x302);
    deferred(draw, 0x181C68) = 0x302;
    d3d.simple_state(0x40348, 0x303);
    deferred(draw, 0x181C6C) = 0x303;
    d3d.simple_state(0x40304, 1);
    deferred(draw, 0x181C5C) = 1;
    d3d.simple_state(0x40340, 5);
    deferred(draw, 0x181C64) = 5;
    d3d.simple_state(0x40354, 0x203);
    deferred(draw, 0x181C54) = 0x203;
    d3d.simple_state(0x4035C, 1);
    deferred(draw, 0x181C70) = 1;
    d3d.depth(1);
    deferred(draw, 0x181970) = 3;
    deferred(draw, 0x181974) = 3;
    deferred(draw, 0x181980) = 2;
    deferred(draw, 0x18197C) = 2;
    deferred(draw, 0x181984) = 2;
    deferred(draw, 0x181968) |= 1;
    d3d.vertex_shader(c.image.word(kBillboardVertexShader.handle));
    const u32 constants = c.image.word(kBillboardVertexShader.constants);
    if (constants != 0)
    {
        d3d.constants(0x60, reinterpret_cast<const float*>(c.image.pointer(constants)), 0xC);
    }
    d3d.pixel_shader(c.image.word(kBillboardPixelShader));
    std::array<float, 16> projection;
    d3d.get_transform(1, projection.data());
    transpose_runtime_graphics_matrix(projection.data(), projection.data());
    d3d.constants(0x64, projection.data(), 4);
    d3d.get_transform(0, c.queue.view.data());
}

// Xbox 0x0005C580
void set_billboard_transform(const Vec3& position, float rotation, BillboardFlushContext& c)
{
    const float sine = float(retail_sin(rotation));
    const float cosine = float(retail_cos(rotation));
    const auto& v = c.queue.view;
    const float x = position.y * v[4] + position.z * v[8] + position.x * v[0] + v[12];
    const float y = position.y * v[5] + position.z * v[9] + position.x * v[1] + v[13];
    const float z = position.y * v[6] + position.z * v[10] + position.x * v[2] + v[14];
    const std::array<float, 16> transform{cosine, -sine, 0.f, x, sine, cosine, 0.f, y, 0.f, 0.f, 1.f, z, 0.f, 0.f,
        0.f, 1.f};
    c.draw.d3d.constants(0x60, transform.data(), 4);
}

// Xbox 0x0005C7F0
void bind_billboard_sprite(const u8* sprite, i32 frame, u32 color, float& width, float& height,
    BillboardFlushContext& c)
{
    u8* texture = const_cast<u8*>(sprite);
    if (frame != -1 && frame < i32(texture[0x11]))
    {
        texture[0x13] = u8(frame);
    }
    bind_dialogue_texture(texture, 0, c.draw);
    const float scale = std::bit_cast<float>(0x3B808081u); // DAT_0032B154, 1/255.
    const std::array<float, 4> constant{float(color & 0xFF) * scale, float((color >> 8) & 0xFF) * scale,
        float((color >> 16) & 0xFF) * scale, float(color >> 24) * scale};
    c.outer.pixel_constants(0, constant.data(), 1);
    u32 type;
    std::memcpy(&type, texture, 4);
    if (type == 0x12 || type == 0x1E || type == 0x3F)
    {
        u16 value;
        std::memcpy(&value, texture + 8, 2);
        width = float(value);
        std::memcpy(&value, texture + 0xA, 2);
        height = float(value);
        return;
    }
    width = 1.f;
    height = 1.f;
}

// Xbox 0x0005C8C0
void push_billboard_quad(const float* size, float u_extent, float v_extent, BillboardFlushContext& c)
{
    const float left = 0.f - size[0];
    const float bottom = 0.f - size[1];
    const float right = size[0];
    const float top = size[1];
    const float depth = 0.f; // DAT_005462AC; nothing writes it.
    u32* push = c.draw.d3d.begin_push(0x19);
    const auto word = [](float value)
    {
        return std::bit_cast<u32>(value);
    };
    push[0] = 0x417FC;
    push[1] = 8;
    push[2] = 0x40501818;
    const std::array<float, 20> vertices{left, bottom, depth, depth, v_extent, right, bottom, depth, u_extent,
        v_extent, right, top, depth, u_extent, depth, left, top, depth, depth, depth};
    for (size_t index = 0; index < vertices.size(); ++index)
    {
        push[3 + index] = word(vertices[index]);
    }
    push[0x17] = 0x417FC;
    push[0x18] = 0;
    c.draw.d3d.end_push(push + 0x19);
}

// Xbox 0x0005C250
void flush_billboards(BillboardFlushContext& c)
{
    sort_billboards(c.queue, entity_detail::g_display_view->position);
    begin_billboard_draw(c);
    c.draw.d3d.simple_state(0x40354, 0x203);
    deferred(c.draw, 0x181C54) = 0x203;
    c.draw.d3d.simple_state(0x4035C, 0);
    deferred(c.draw, 0x181C70) = 0;
    c.draw.d3d.depth(1);
    for (i32 index = 0; index < c.queue.count; ++index)
    {
        const BillboardQueueEntry& entry = *c.queue.order[size_t(index)];
        set_billboard_transform(entry.position, entry.rotation, c);
        float width;
        float height;
        bind_billboard_sprite(entry.sprite, i32(entry.frame), entry.color, width, height, c);
        push_billboard_quad(entry.size.data(), width, height, c);
    }
    if (--c.draw.override_count == 0)
    {
        c.queue.count = 0;
        c.draw.fog_disabled = 0;
        return;
    }
    c.queue.count = 0;
}
}
