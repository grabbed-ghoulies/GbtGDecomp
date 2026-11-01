#include "shadow_volume_pass.h"
#include "d3dx_matrix.h"
#include "d3dx_view.h"
#include "../camera/display_view.h"
#include "../camera/runtime_display_setup.h"
#include "gbtg/texture.h"

#include <array>
#include <bit>
#include <cstring>
#include <vector>

namespace gbtg
{
namespace
{
using Matrix = std::array<float, 16>;

DialogueD3DBoundaries& d3d(RuntimeBackgroundClothDraw& context)
{
    return context.pass.outer.draw.d3d;
}

u32& deferred(RuntimeBackgroundClothDraw& context, u32 address)
{
    const size_t index = (address - 0x00181968u) / 4;
    auto block = context.pass.outer.draw.deferred;
    if (index >= block.size())
    {
        throw Error("shadow volume pass writes past the DAT_00181968 deferred block");
    }
    return block[index];
}

// D3DDevice_SetRenderState_Simple (0x00171050) and its deferred mirror.
void simple(RuntimeBackgroundClothDraw& context, u32 method, u32 value, u32 shadow)
{
    d3d(context).simple_state(method, value);
    deferred(context, shadow) = value;
}

// One corner's depth through the view's z row; behind the plane it widens the near / far range.
void measure_corner(float depth, float& near_depth, float& far_depth, u32& behind)
{
    if (depth <= 0.f)
    {
        depth = 0.f - depth;
        behind = 1;
        if (depth < near_depth)
        {
            near_depth = depth;
        }
        if (far_depth <= depth && depth != far_depth)
        {
            far_depth = depth;
        }
    }
    else
    {
        near_depth = 0.f;
    }
}

// The render target to a book page's level 0, with the pass's depth surface.
void target_page(u32 page, RuntimeShadowVolumeTargets& targets, RuntimeBackgroundClothDraw& context)
{
    auto& outer = context.pass.outer.d3d;
    const DialogueSurfaceHandle surface = outer.surface(DialogueSurfaceHandle(page), 0); // D3DTexture_GetSurfaceLevel2
    outer.render_target(surface, targets.depth); // D3DDevice_SetRenderTarget, DAT_0050EC0C
    outer.release(surface); // D3DResource_Release
}
} // namespace

// Xbox 0x00119160
void create_runtime_shadow_ramp(RuntimeShadowVolumeTargets& targets, std::span<BackgroundRegion> regions, i32& count,
    const std::function<u32(u32, u32, u32, u32, u32, u32)>& create_texture,
    const std::function<std::span<u8>(u32)>& texels)
{
    targets.ramp = create_texture(0x1000, 1, 1, 1, 6, 3); // D3DDevice_CreateTexture2, A8R8G8B8
    // D3DTexture_LockRect, the 4096 words in place, then XGSwizzleRect over them.
    std::vector<u8> linear(0x1000 * 4);
    for (u32 index = 0; index < 0x1000; ++index)
    {
        const u32 word = ((index >> 4 & 0xFu) | (index & 0xFu) << 8) << 8 | (index >> 8 & 0xFu);
        std::memcpy(linear.data() + index * 4, &word, 4);
    }
    swizzle_nv2a_into(texels(targets.ramp), linear, 0x1000, 1, 4);
    for (BackgroundRegion& region : regions)
    {
        region.source = nullptr; // DAT_0050EAA0 + n * 0x2C
    }
    count = 0; // DAT_0050EC00
}

// Xbox 0x0013C760
u32 measure_runtime_shadow_region(BackgroundRegion& region, const entity_detail::DisplayView& view)
{
    const Vec3& first = region.first; // EDX, +4
    const Vec3& second = region.second; // ECX, +0x10
    region.near_depth = std::bit_cast<float>(0x47C34FF3u); // DAT_0032B2E8, 99999.9
    region.far_depth = std::bit_cast<float>(0xC7C34FF3u); // DAT_0032B2E4
    const float a = view.transform[8]; // +0x88
    const float b = view.transform[9]; // +0x8C
    const float c = view.transform[10]; // +0x90
    const float d = view.transform[11]; // +0x94
    u32 behind = 0;
    measure_corner(c * first.z + b * first.y + a * first.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * first.z + b * second.y + a * first.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * second.z + b * first.y + a * first.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * second.z + b * second.y + a * first.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * first.z + b * first.y + a * second.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * first.z + b * second.y + a * second.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(c * second.z + b * first.y + a * second.x + d, region.near_depth, region.far_depth, behind);
    measure_corner(a * second.x + c * second.z + b * second.y + d, region.near_depth, region.far_depth, behind);
    return behind;
}

// Xbox 0x00119110
u32 measure_runtime_shadow_regions(u32 any, RuntimeShadowVolumes& volumes, const entity_detail::DisplayView& view)
{
    u32 result = 0;
    for (i32 index = 0; index < volumes.count; ++index) // DAT_0050EC00
    {
        BackgroundRegion& region = volumes.regions[size_t(index)];
        region.behind = measure_runtime_shadow_region(region, view); // 0x0013C760, +0x28
        if (result != 0 || any != 0)
        {
            result = 1;
        }
    }
    return result;
}

// Xbox 0x00118FD0
void draw_runtime_shadow_region(const BackgroundRegion& region, RuntimeBackgroundClothDraw& context)
{
    // c0x66: the region's near depth and the reciprocal of its corners' separation. Retail's last two floats are the
    // stack words above its locals (its return address and the region argument).
    const std::array<float, 4> depth{region.near_depth, 1.f / region.distance, 0.f, 0.f}; // DAT_0032B0F8
    d3d(context).constants(0x66, depth.data(), 1); // D3DDevice_SetVertexShaderConstant1
    const u8* source = region.source;
    u32 count;
    std::memcpy(&count, source + 0x14, 4);
    i32 remaining = i32(count) * 6;
    u32 pair = 0;
    while (remaining != 0)
    {
        const i32 run = remaining < 0x7F3 ? remaining / 6 : 0x153;
        u32* push = d3d(context).begin_push(u32(run * 6 + 5)); // D3DDevice_BeginPush_4
        push[0] = 0x417FC; // NV097_SET_BEGIN_END
        push[1] = 5;
        push[2] = u32(run) * 0x180000u + 0x40001818u; // NV097_INLINE_ARRAY, non-incrementing
        u32* cursor = push + 3;
        for (i32 step = 0; step < run; ++step)
        {
            // Each pair: the vertex at +0x30 + 0xC n and its partner `count` vertices on.
            std::memcpy(cursor, source + (pair * 3 + 0xC) * 4, 12);
            std::memcpy(cursor + 3, source + (count + 4 + pair) * 0xC, 12);
            cursor += 6;
            ++pair;
        }
        cursor[0] = 0x417FC;
        cursor[1] = 0;
        d3d(context).end_push(cursor + 2); // D3DDevice_EndPush
        remaining -= run * 6;
    }
}

// Xbox 0x00119230
void draw_runtime_shadow_volumes(RuntimeShadowVolumes& volumes, RuntimeBackgroundClothDraw& context)
{
    if (volumes.count == 0 || volumes.transition_guard >= 1) // DAT_0050EC00, DAT_0050484C
    {
        return;
    }
    auto& targets = volumes.targets;
    auto& outer = context.pass.outer.d3d;
    measure_runtime_shadow_regions(0, volumes, *context.pass.view); // 0x00119110, its result unused
    bind_display_projection(context.display); // 0x000F6170
    const u32 page = context.pass.address_of(context.pass.pages.page_texture); // DAT_005047DC
    const u32 second_page = context.pass.address_of(context.pass.pages.depth_texture); // DAT_005047E8
    targets.target = outer.back_buffer(0); // D3DDevice_GetRenderTarget2
    targets.depth = outer.depth_buffer(); // D3DDevice_GetDepthStencilSurface2
    context.pass.texture(0, 0); // D3DDevice_SetTexture
    deferred(context, 0x00181D3C) = 0x44444444;
    deferred(context, 0x00181968) |= 0x1200;
    deferred(context, 0x00181D08) = 1;
    d3d(context).depth(1); // D3DDevice_SetRenderState_ZEnable
    simple(context, 0x40310, 1, 0x00181C74);
    deferred(context, 0x00181968) |= 0x3000;
    deferred(context, 0x00181D0C) = 0;
    simple(context, 0x40304, 0, 0x00181C5C);
    simple(context, 0x40300, 0, 0x00181C60);
    simple(context, 0x40350, 0x8006, 0x00181C98);
    deferred(context, 0x001819A8) = 2;
    deferred(context, 0x001819AC) = 0;
    deferred(context, 0x001819A0) = 4;
    deferred(context, 0x001819B8) = 2;
    deferred(context, 0x001819BC) = 0;
    deferred(context, 0x001819B0) = 4;
    deferred(context, 0x00181984) = 2;
    deferred(context, 0x00181980) = 2;
    deferred(context, 0x0018197C) = 2;
    deferred(context, 0x00181970) = 1;
    deferred(context, 0x00181974) = 1;
    deferred(context, 0x00181A20) = 1;
    deferred(context, 0x00181968) |= 0x801;
    deferred(context, 0x00181A30) = 1;
    d3d(context).pixel_shader(0); // D3DDevice_SetPixelShader
    d3d(context).vertex_shader(context.pass.word(0x0042735C)); // D3DDevice_SetVertexShader
    const u32 table = context.pass.word(0x00427350);
    if (table != 0)
    {
        d3d(context).constants(0x60, context.pass.floats(table), 0xC); // 0x001735D0, ECX 0x60
    }
    // c0x60: (view x world) x projection-viewport, transposed; c0x65 the eye, (view x world)^-1 of the origin.
    Matrix view{};
    Matrix world{};
    Matrix screen{};
    d3d(context).get_transform(0, view.data()); // D3DDevice_GetTransform
    d3d(context).get_transform(6, world.data());
    d3d(context).projection_viewport(screen.data()); // D3DDevice_GetProjectionViewportMatrix
    Matrix product{};
    multiply_runtime_graphics_matrices(product.data(), view.data(), world.data()); // 0x001850CA
    Matrix inverse{};
    inverse_runtime_graphics_matrix(inverse.data(), nullptr, product.data()); // 0x00184792
    multiply_runtime_graphics_matrices(product.data(), product.data(), screen.data());
    transpose_runtime_graphics_matrix(product.data(), product.data()); // 0x00184725
    Vec3 eye{};
    transform_runtime_graphics_coord(eye, eye, inverse.data()); // 0x00185015
    d3d(context).constants(0x60, product.data(), 4); // D3DDevice_SetVertexShaderConstant4
    d3d(context).constants(0x64, context.pass.floats(0x00430C00), 1); // D3DDevice_SetVertexShaderConstant1
    // The eye's fourth float is the next stack word, the transposed product's first.
    const std::array<float, 4> eye_constant{eye.x, eye.y, eye.z, product[0]};
    d3d(context).constants(0x65, eye_constant.data(), 1);
    outer.clear(0, nullptr, 2, 0, 1.f, 0); // D3DDevice_Clear, the stencil
    context.pass.texture(0, targets.ramp); // DAT_0050EC04
    deferred(context, 0x00181970) = 3;
    deferred(context, 0x00181974) = 3;
    deferred(context, 0x00181980) = 1;
    deferred(context, 0x0018197C) = 1;
    deferred(context, 0x001819A0) = 2;
    deferred(context, 0x001819A8) = 2;
    deferred(context, 0x00181968) |= 0x801;
    deferred(context, 0x001819AC) = 0;
    for (i32 index = 0; index < volumes.count; ++index)
    {
        const BackgroundRegion& region = volumes.regions[size_t(index)]; // DAT_0050EAA0 + index * 0x2C
        const u32 reference = u32(index + 1);
        // The volume marks the stencil with its reference where it passes the depth test, writing no colour.
        simple(context, 0x40354, 0x203, 0x00181C54);
        simple(context, 0x4035C, 0, 0x00181C70);
        d3d(context).depth(1);
        d3d(context).stencil_enable(1); // D3DDevice_SetRenderState_StencilEnable
        simple(context, 0x40378, 0x1E01, 0x00181C84);
        d3d(context).simple_state(0x40370, 0x1E00); // D3DDevice_SetRenderState_StencilFail (0x001723C0)
        simple(context, 0x40374, 0x1E00, 0x00181C80);
        simple(context, 0x40364, 0x207, 0x00181C88);
        simple(context, 0x40368, reference, 0x00181C8C);
        simple(context, 0x40300, 0, 0x00181C60);
        simple(context, 0x40304, 0, 0x00181C5C);
        simple(context, 0x40358, 0, 0x00181C7C);
        d3d(context).cull(0); // D3DDevice_SetRenderState_CullMode
        draw_runtime_shadow_region(region, context); // 0x00118FD0
        // Then its depth ramp, added where the stencil holds the reference: back faces into page 0, front faces
        // into page 1, each page cleared before the first region.
        simple(context, 0x40358, 0x01010101, 0x00181C7C);
        d3d(context).depth(0);
        simple(context, 0x40354, 0x207, 0x00181C54);
        simple(context, 0x4035C, 0, 0x00181C70);
        simple(context, 0x40304, 1, 0x00181C5C);
        simple(context, 0x40344, 1, 0x00181C68);
        simple(context, 0x40348, 1, 0x00181C6C);
        d3d(context).stencil_enable(1);
        simple(context, 0x40364, 0x202, 0x00181C88);
        simple(context, 0x40368, reference, 0x00181C8C);
        simple(context, 0x40378, 0x1E00, 0x00181C84);
        target_page(page, targets, context);
        if (index == 0)
        {
            outer.clear(0, nullptr, 0xF0, 0, 1.f, 0); // D3DDevice_Clear, the colour
        }
        d3d(context).cull(0x901);
        draw_runtime_shadow_region(region, context);
        target_page(second_page, targets, context);
        if (index == 0)
        {
            outer.clear(0, nullptr, 0xF0, 0, 1.f, 0);
        }
        d3d(context).cull(0x900);
        draw_runtime_shadow_region(region, context);
        outer.render_target(targets.target, targets.depth); // DAT_0050EC08, DAT_0050EC0C
    }
    // The pages compared over the screen wherever a region marked the stencil, blended in the first region's colour.
    d3d(context).cull(0);
    d3d(context).depth(0);
    simple(context, 0x40350, 0x8006, 0x00181C98);
    simple(context, 0x40344, 0x302, 0x00181C68);
    simple(context, 0x40348, 0x303, 0x00181C6C);
    d3d(context).stencil_enable(1);
    simple(context, 0x40364, 0x205, 0x00181C88);
    simple(context, 0x40368, 0, 0x00181C8C);
    d3d(context).vertex_shader(0xA00404); // The quad's FVF: XYZRHW and its texture sets (DAT_00430B00, stride 0x40).
    d3d(context).pixel_shader(context.pass.word(0x00425EDC));
    outer.pixel_constants(0, reinterpret_cast<const float*>(volumes.regions[0].source + 4),
        1); // D3DDevice_SetPixelShaderConstant, *DAT_0050EAA0 + 4
    context.pass.texture(0, page);
    context.pass.texture(1, second_page);
    deferred(context, 0x001819F0) = 3;
    deferred(context, 0x001819F4) = 3;
    deferred(context, 0x00181A00) = 1;
    deferred(context, 0x00181968) |= 2;
    deferred(context, 0x001819FC) = 1;
    d3d(context).draw_vertices_up(8, 4, context.pass.floats(0x00430B00), 0x40); // D3DDevice_DrawVerticesUP, a quad
    simple(context, 0x40364, 0x207, 0x00181C88);
    d3d(context).stencil_enable(0);
    outer.release(targets.target); // D3DResource_Release, DAT_0050EC08
    outer.release(targets.depth); // DAT_0050EC0C
}
}
