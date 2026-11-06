#include "light_cones.h"
#include "boot_shader_tables.h"
#include "vector_normalize.h"
#include "../audio/listener_settings.h"
#include "../entities/entity_collision.h"
#include "../entities/player_overlay.h"
#include "../frontend/dialogue_draw_context.h"
#include "../scene/scene_light_grid.h"
#include "gbtg/anim.h"
#include "gbtg/sound_runtime.h"
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace gbtg
{
namespace
{
using Matrix = std::array<float, 16>;

// x87: the length of the difference, summed x, z, then y.
long double distance_xzy(const Vec3& first, const Vec3& second)
{
    const long double x = (long double)first.x - second.x;
    const long double y = (long double)first.y - second.y;
    const long double z = (long double)first.z - second.z;
    return std::sqrt((x * x + z * z) + y * y);
}

// The rim point moved by the 3x4 matrix, summed x, y, z (0x0011A43D on).
Vec3 rotate_xyz(const float m[12], const Vec3& p)
{
    return Vec3{((m[0] * p.x + p.y * m[1]) + p.z * m[2]) + m[3], ((m[4] * p.x + p.y * m[5]) + p.z * m[6]) + m[7],
        ((m[8] * p.x + p.y * m[9]) + p.z * m[10]) + m[11]};
}

// The same, summed in the loop's orders (0x0011AC11 on).
Vec3 rotate_looped(const float m[12], const Vec3& p)
{
    return Vec3{((m[2] * p.z + m[0] * p.x) + m[1] * p.y) + m[3], ((m[4] * p.x + m[6] * p.z) + m[5] * p.y) + m[7],
        ((m[8] * p.x + m[10] * p.z) + m[9] * p.y) + m[11]};
}

// The order's comparison (0x00119B70): undrawn cones first, then the drawn ones farthest first.
const RuntimeLightCones* g_sorted = nullptr;

int compare_light_cones(const void* first, const void* second)
{
    const RuntimeLightCone& a = g_sorted->records.at(size_t(*static_cast<const i32*>(first)));
    const RuntimeLightCone& b = g_sorted->records.at(size_t(*static_cast<const i32*>(second)));
    if (a.visible == 0)
    {
        return -1;
    }
    if (a.used != 0 && b.visible != 0 && b.used != 0)
    {
        return b.distance > a.distance ? 1 : -1;
    }
    return a.used != 0 ? 1 : -1;
}

DialogueD3DBoundaries& d3d(RuntimeLightConeDraw& context)
{
    return context.projector.pass.outer.draw.d3d;
}

u32& deferred(RuntimeLightConeDraw& context, u32 address)
{
    const size_t index = (address - 0x00181968u) / 4;
    auto block = context.projector.pass.outer.draw.deferred;
    if (index >= block.size())
    {
        throw Error("light cone draw writes past the DAT_00181968 deferred block");
    }
    return block[index];
}
}

// Xbox 0x0011AEE0
i32 add_runtime_light_cone(RuntimeLightCones& cones, Actor& actor, u32 actor_address, u32 marker, float pitch,
    float yaw, float spread, float length, float fade, float spot_offset)
{
    i32 index = 0;
    for (; index < i32(cones.records.size()); ++index)
    {
        if (cones.records[size_t(index)].used == 0)
        {
            break;
        }
    }
    if (index == i32(cones.records.size()))
    {
        return -1;
    }
    RuntimeLightCone& cone = cones.records[size_t(index)];
    cone.pitch = pitch;
    cone.yaw = yaw;
    cone.spread = spread;
    cone.length = length;
    cone.actor_address = actor_address;
    cone.actor = &actor;
    cone.fade = fade;
    cone.used = 1u;
    cone.visible = 0u;
    cone.marker = marker;
    cone.spot_offset = spot_offset;
    cones.count = cones.count + 1;
    return index;
}

// Xbox 0x0011AEA0
void clear_runtime_light_cones(RuntimeLightCones& cones, const std::function<void(std::span<const u8>)>& release)
{
    cones.count = 0;
    for (RuntimeLightCone& cone : cones.records)
    {
        cone.used = 0u;
        cone.visible = 0u;
        cone.actor = nullptr;
    }
    if (cones.texture.data() != nullptr)
    {
        release(cones.texture); // 0x00035C00
        cones.texture = {};
    }
}

// Xbox 0x001047E0, ESI output
void build_runtime_axis_rotation(float output[12], const Vec3& axis, float angle)
{
    const float sine = float(retail_sin(angle)); // 0x00153320
    const float cosine = float(retail_cos(angle)); // 0x00153230
    const float t = 1.f - cosine; // DAT_0032B0F8
    Vec3 n{};
    normalize_runtime_graphics_vector(n, axis); // 0x00184F99
    output[0] = n.x * n.x * t + cosine;
    output[4] = n.z * sine + n.y * n.x * t;
    output[1] = n.y * n.x * t - n.z * sine;
    output[5] = n.y * n.y * t + cosine;
    output[2] = n.y * sine + n.z * n.x * t;
    output[8] = n.z * n.x * t - n.y * sine;
    output[10] = n.z * n.z * t + cosine;
    output[6] = n.z * n.y * t - n.x * sine;
    output[9] = n.x * sine + n.z * n.y * t;
    output[3] = 0.f;
    output[7] = 0.f;
    output[11] = 0.f;
}

// Xbox 0x00119F40, ECX index, EAX at
void build_runtime_light_cone(RuntimeLightCones& cones, i32 index, const Vec3& at, const Vec3& from, float spread)
{
    RuntimeLightCone& cone = cones.records.at(size_t(index));
    cone.from = from;
    cone.spot = at;
    cone.at = at;
    // The rim's radius: the spread, unless the cone opens past 15 degrees.
    const float dx = at.x - from.x;
    const long double dy = (long double)at.y - from.y;
    const long double dz = (long double)at.z - from.z;
    const long double reach = std::sqrt((dz * dz + dy * dy) + (long double)dx * dx);
    float angle = float(std::atan2((long double)spread, reach));
    if (0.f > angle) // DAT_0032B100
    {
        angle = angle + 6.2831855f; // DAT_0032B298
    }
    float radius = spread;
    if (!(0.2617994f >= angle)) // DAT_0032B340
    {
        const long double limit = std::tan((long double)0.2617993950843811); // DAT_0032B338
        radius = float(limit * std::sqrt(((long double)dx * dx + dz * dz) + dy * dy));
    }
    cone.radius = radius;
    // The axis, unit unless it has no length.
    Vec3 axis{at.x - from.x, at.y - from.y, at.z - from.z};
    const float axis_squared = (axis.x * axis.x + axis.y * axis.y) + axis.z * axis.z;
    if (axis_squared != 0.f)
    {
        const long double scale = retail_reciprocal_sqrt(axis_squared); // 0x001534C0
        axis = Vec3{float(scale * axis.x), float(scale * axis.y), float(scale * axis.z)};
    }
    cone.axis = axis;
    // The spot moves on along the axis.
    const float shift = cone.spot_offset * radius;
    Vec3 spot = cone.spot;
    spot = Vec3{spot.x + shift * axis.x, spot.y + axis.y * shift, spot.z + axis.z * shift};
    cone.spot = spot;
    // The first rim point: across the axis (in the horizontal plane), scaled so the rim keeps the cone's slope.
    const long double scaled = distance_xzy(from, spot) * radius;
    const Vec3 back{from.x - at.x, from.y - at.y, from.z - at.z};
    const long double back_length =
        std::sqrt(((long double)back.x * back.x + (long double)back.z * back.z) + (long double)back.y * back.y);
    const float scale = float(scaled / back_length);
    Vec3 across{axis.y * 0.f - axis.z, axis.z * 0.f - axis.x * 0.f, axis.x - axis.y * 0.f};
    const float across_squared = (across.x * across.x + across.y * across.y) + across.z * across.z;
    if (across_squared != 0.f)
    {
        const long double inverse = retail_reciprocal_sqrt(across_squared); // 0x001534C0
        across = Vec3{float(inverse * across.x), float(inverse * across.y), float(inverse * across.z)};
    }
    across = Vec3{scale * across.x, scale * across.y, scale * across.z};
    cone.rim[0].position = Vec3{spot.x + across.x, spot.y + across.y, spot.z + across.z};
    // Each further rim point turns the one before 30 degrees about the axis through the spot.
    std::array<float, 12> turn{};
    build_runtime_axis_rotation(turn.data(), axis, 0.5235988f); // 0x001047E0
    const std::array<float, 12> to_origin{1.f, 0.f, 0.f, 0.f - spot.x, 0.f, 1.f, 0.f, 0.f - spot.y, 0.f, 0.f, 1.f,
        0.f - spot.z};
    const std::array<float, 12> back_to_spot{1.f, 0.f, 0.f, spot.x, 0.f, 1.f, 0.f, spot.y, 0.f, 0.f, 1.f, spot.z};
    std::array<float, 12> step{};
    multiply_affine_matrices(step.data(), turn.data(), to_origin.data()); // 0x001041B0
    multiply_affine_matrices(step.data(), back_to_spot.data(), step.data()); // 0x001041B0
    for (size_t point = 1; point < 12; ++point)
    {
        const Vec3 previous = cone.rim[point - 1].position;
        cone.rim[point].position = point < 9 ? rotate_xyz(step.data(), previous) : rotate_looped(step.data(), previous);
    }
    // Each rim point's colour: its height on the cone (blue) and how far the spot lies (alpha).
    for (size_t point = 0; point < 12; ++point)
    {
        auto& row = cone.rim[point];
        float alpha = float(cone.fade / distance_xzy(from, spot));
        if (alpha > 1.f) // DAT_0032B0F8
        {
            alpha = 1.f;
        }
        const Vec3 position = row.position;
        const float vx = position.x - from.x;
        const float vy = position.y - from.y;
        const float vz = position.z - from.z;
        const float squared = (vx * vx + vz * vz) + vy * vy;
        float nx = vx;
        float ny = vy;
        long double nz = vz;
        if (squared != 0.f)
        {
            const long double inverse = retail_reciprocal_sqrt(squared); // 0x001534C0
            nx = float(vy * inverse); // The x slot takes y.
            ny = float(vy * inverse);
            nz = inverse * vz;
        }
        const long double height = ((((nz + nx) * 0.0L + ny) + 1.0L) * 0.5L) * 210.0L; // DAT_0032B0F4, DAT_0032B334
        const u32 blue = u8(truncate_runtime_float(height)); // 0x001673D8
        const u32 opacity = u8(truncate_runtime_float((long double)alpha * 200.0L)); // DAT_0032B330
        row.color = (blue | opacity << 16 | 0x2800u) << 8;
        row.word = 0xC828D200u;
    }
    // The plane through `from` across the axis.
    cone.plane = {axis.x, axis.y, axis.z, 0.f - ((axis.z * from.z + axis.y * from.y) + axis.x * from.x)};
}

// Xbox 0x00119BE0
void sort_runtime_light_cones(RuntimeLightCones& cones, const Vec3& view)
{
    for (i32 index = 0; index < 16; ++index)
    {
        cones.order[size_t(index)] = index;
        RuntimeLightCone& cone = cones.records.at(size_t(index));
        if (cone.used == 0 || cone.visible == 0)
        {
            continue;
        }
        const Vec3 spot = cone.spot;
        cone.distance = ((spot.z - view.z) * (spot.z - view.z) + (spot.y - view.y) * (spot.y - view.y))
                + (spot.x - view.x) * (spot.x - view.x);
    }
    g_sorted = &cones;
    std::qsort(cones.order.data(), 16, 4, compare_light_cones); // _qsort, 0x00119B70
    g_sorted = nullptr;
}

// Xbox 0x0011AF90
void draw_runtime_light_cones(RuntimeLightConeDraw& context)
{
    RuntimeLightCones& cones = context.cones;
    RuntimeScenePointLightPass& pass = context.projector.pass;
    if (cones.texture.data() == nullptr)
    {
        cones.texture = context.acquire("aid_texture_particles_bathtubgas"); // 0x00035B60, 0x0030F798
    }
    // Each cone is built from its actor's marker; a beam blocked near its start reaches 10 out instead.
    for (i32 index = 0, seen = 0; seen < cones.count && index < 16; ++index)
    {
        RuntimeLightCone& cone = cones.records.at(size_t(index));
        if (cone.used == 0)
        {
            continue;
        }
        if (cone.visible != 0)
        {
            Actor* actor = cones.records[size_t(index)].actor;
            if (actor == nullptr)
            {
                throw Error("0x0011AF90 reads a light cone's actor (+8) through address zero");
            }
            const u32 marker = cone.marker;
            Vec3 eye{};
            actor_marker_world_position(*actor, i32(marker), eye); // 0x0003AE60, EDX the marker
            Vec3 euler{};
            actor_marker_world_euler(*actor, i16(marker), euler); // 0x0003B160, AX the marker
            euler.x = float((long double)euler.x + cone.pitch);
            euler.y = float((long double)euler.y + cone.yaw);
            Vec3 end = eye;
            add_sound_direction(end, euler.x, euler.y, cone.length); // 0x00106220, ESI the end
            Vec3 far = end;
            Vec3 near = eye;
            entity_record_sweep_probe(far, eye, 4, 0); // 0x00122480, EBX the far copy
            entity_record_sweep_probe(near, end, 4, 0); // 0x00122480, EBX the near copy
            const float nx = eye.x - near.x;
            const float ny = eye.y - near.y;
            const float nz = eye.z - near.z;
            const float near_squared = (nx * nx + nz * nz) + ny * ny;
            Vec3 at = far;
            if (near_squared > 1.f) // DAT_0032B0F8
            {
                const float fx = eye.x - far.x;
                const float fy = eye.y - far.y;
                const float fz = eye.z - far.z;
                if (!(near_squared > (fx * fx + fz * fz) + fy * fy))
                {
                    at = near;
                }
            }
            const float ax = eye.x - at.x;
            const float ay = eye.y - at.y;
            const float az = eye.z - at.z;
            if (100.f > (ax * ax + az * az) + ay * ay) // DAT_0032B12C
            {
                std::array<float, 3> out{at.x - eye.x, at.y - eye.y, at.z - eye.z};
                normalize_vector_in_place(out.data()); // 0x0002AC70, ESI the vector
                at = Vec3{out[0] * 10.f + eye.x, out[1] * 10.f + eye.y, out[2] * 10.f + eye.z}; // DAT_0032B10C
            }
            build_runtime_light_cone(cones, index, at, eye, cone.spread); // 0x00119F40
        }
        ++seen;
    }
    if (pass.view == nullptr)
    {
        throw Error("0x00119BE0 reads the DAT_005077A8 display view through address zero");
    }
    sort_runtime_light_cones(cones, pass.view->position); // 0x00119BE0
    // Each drawn cone's spot is projected on the background grid.
    context.projector.projector.alternate = 1; // DAT_00504940
    begin_runtime_shadow_projector(context.projector); // 0x000644F0
    const u32 grid = runtime_light_grid_offset(pass.grid.model); // **(model +0x10)
    const auto& texture = cones.texture;
    const u8* texture_record = texture.data() + read_u32(texture, 4) + u32(texture[0x12]) * 0x14;
    for (i32 index = 0, seen = 0; seen < cones.count && index < 16; ++index)
    {
        RuntimeLightCone& cone = cones.records.at(size_t(index));
        if (cone.used == 0)
        {
            continue;
        }
        if (cone.visible != 0)
        {
            context.projector.projector.alternate_color = cone.plane; // DAT_00504944..DAT_00504950
            project_runtime_texture_on_grid(grid, cone.at, cone.from,
                pass.address_of(texture_record), cone.radius, 0xC800D228u, false, 2,
                context.projector); // 0x00064820, ECX grid, EDX at, EAX 2
        }
        ++seen;
    }
    pass.z_bias(0); // 0x00171660
    d3d(context).cull(0); // 0x00171420
    pass.multisample_antialias(0); // 0x00172600
    d3d(context).multisample_mode(0); // 0x001725A0
    pass.multisample_target_mode(0); // 0x001725D0
    context.projector.projector.alternate = 0; // DAT_00504940
    reset_dialogue_sprite_state(pass.outer.draw); // 0x0005E110
    deferred(context, 0x00181968) |= 0x800;
    deferred(context, 0x001819A0) = 2;
    deferred(context, 0x001819B0) = 2;
    deferred(context, 0x001819A8) = 0;
    deferred(context, 0x001819B8) = 0;
    d3d(context).vertex_shader(0x42); // 0x001739F0
    d3d(context).cull(0x900); // 0x00171420
    d3d(context).vertex_shader(pass.word(kLightConeVertexShader.handle)); // 0x001739F0
    const u32 constants = pass.word(kLightConeVertexShader.constants);
    if (constants != 0)
    {
        d3d(context).constants(0x60, pass.floats(constants), 0xC); // 0x001735D0, ECX 0x60
    }
    d3d(context).pixel_shader(pass.word(kDiffusePixelShader)); // 0x00175370
    Matrix world{};
    Matrix view{};
    Matrix projection{};
    d3d(context).get_transform(6, world.data()); // 0x00173EC0
    d3d(context).get_transform(0, view.data());
    d3d(context).get_transform(1, projection.data());
    Matrix product{};
    d3d(context).multiply(product.data(), world.data(), view.data()); // 0x001850CA
    d3d(context).multiply(product.data(), product.data(), projection.data()); // 0x001850CA
    d3d(context).transpose(product.data(), product.data()); // 0x00184725
    d3d(context).constants(0x60, product.data(), 4); // 0x00173440
    // The rim of each drawn cone, farthest first, as a strip of {rim point, colour, from, word}.
    for (i32 slot = 0, seen = 0; seen < cones.count && slot < 16; ++slot)
    {
        RuntimeLightCone& cone = cones.records.at(size_t(cones.order[size_t(slot)]));
        if (cone.used == 0)
        {
            continue;
        }
        if (cone.visible != 0)
        {
            const Vec3 from = cone.from;
            u32* push = d3d(context).begin_push(0x6D); // 0x00177250
            *push++ = 0x417FC;
            *push++ = 6;
            *push++ = 0x41A01818;
            const auto vertex = [&](size_t point)
            {
                auto& row = cone.rim[point];
                const Vec3 position = row.position;
                const std::array<u32, 8> words{std::bit_cast<u32>(position.x), std::bit_cast<u32>(position.y),
                    std::bit_cast<u32>(position.z), row.color, std::bit_cast<u32>(from.x),
                    std::bit_cast<u32>(from.y), std::bit_cast<u32>(from.z), row.word};
                std::memcpy(push, words.data(), sizeof(words));
                push += words.size();
            };
            for (size_t point = 0; point < 12; ++point)
            {
                vertex(point);
            }
            vertex(0);
            *push++ = 0x417FC;
            *push++ = 0;
            d3d(context).end_push(push); // 0x00177270
        }
        ++seen;
    }
}
}
