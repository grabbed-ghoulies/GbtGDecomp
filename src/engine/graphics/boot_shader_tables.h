#pragma once

#include "gbtg/types.h"

namespace gbtg
{
// The boot shader tables in .data. Every row holds assembly source that 0x00065EE0 and 0x00063FB0 turn into a handle,
// written back into the row, so these are addresses to read through the image, after boot, rather than values.
//
// Vertex rows, 0x18 bytes from DAT_004271E0: +0 source, +4 source size, +8 constants pointer (0 when none),
// +0xC unread (1 or 3 where constants are set, else 0), +0x10 declaration, +0x14 handle.
// Pixel rows, 0x10 bytes from DAT_00425DE0: +0 source, +4 source size, +8 factory (0 assembles the source), +0xC handle.
//
// Names come from the source's own comment where it has one (ZSprite, Pass Colour), otherwise from the pass that binds
// the row. Rows nothing in the dumps binds stay unnamed; see docs/RETAIL_LAYOUTS.md.
struct BootVertexShader
{
    u32 handle;
    u32 constants;
};

constexpr BootVertexShader boot_vertex_shader(u32 index)
{
    return {0x004271F4 + index * 0x18, 0x004271E8 + index * 0x18};
}

constexpr u32 boot_pixel_shader(u32 index)
{
    return 0x00425DEC + index * 0x10;
}

inline constexpr BootVertexShader kScreenLineVertexShader = boot_vertex_shader(0); // Position only: add oPos, v0, c0.
inline constexpr BootVertexShader kZSpriteVertexShader = boot_vertex_shader(1);
inline constexpr BootVertexShader kScreenQuadVertexShader = boot_vertex_shader(2); // One texture, screen coordinates.
inline constexpr BootVertexShader kShadowProjectorVertexShader = boot_vertex_shader(7);
inline constexpr BootVertexShader kShadowProjectorClippedVertexShader = boot_vertex_shader(8); // Adds oT1 for texkill.
inline constexpr BootVertexShader kPointLightLitVertexShader = boot_vertex_shader(9); // Reads the normal, v2.
inline constexpr BootVertexShader kPointLightFlatVertexShader = boot_vertex_shader(10);
inline constexpr BootVertexShader kBlurQuadVertexShader = boot_vertex_shader(12); // Four offset, clamped textures.
inline constexpr BootVertexShader kClothColouredVertexShader = boot_vertex_shader(13); // Vertex colour and texture.
inline constexpr BootVertexShader kClothRampVertexShader = boot_vertex_shader(14); // Lit through the ramp texture.
inline constexpr BootVertexShader kShadowVolumeVertexShader = boot_vertex_shader(15);
inline constexpr BootVertexShader kBillboardVertexShader = boot_vertex_shader(16);
inline constexpr BootVertexShader kDialogueVertexShader = boot_vertex_shader(20); // Constant colour and texture.
inline constexpr BootVertexShader kLightConeVertexShader = boot_vertex_shader(22);

inline constexpr u32 kZSpritePixelShader = boot_pixel_shader(0); // The factory row, 0x00063F40.
inline constexpr u32 kPassColourPixelShader = boot_pixel_shader(2);
inline constexpr u32 kTexturedDiffusePixelShader = boot_pixel_shader(8); // mul r0, t0, v0.
inline constexpr u32 kTexturedDiffuseClippedPixelShader = boot_pixel_shader(9); // Adds texkill t1.
inline constexpr u32 kPointLightPixelShader = boot_pixel_shader(10);
inline constexpr u32 kBlurPixelShader = boot_pixel_shader(11); // Averages four textures.
inline constexpr u32 kRefractionPixelShader = boot_pixel_shader(12);
inline constexpr u32 kClothColouredPixelShader = boot_pixel_shader(13);
inline constexpr u32 kClothRampPixelShader = boot_pixel_shader(14);
inline constexpr u32 kShadowVolumePixelShader = boot_pixel_shader(15);
inline constexpr u32 kBillboardPixelShader = boot_pixel_shader(16); // mul r0, c0, t0.
inline constexpr u32 kPointLightModulatePixelShader = boot_pixel_shader(17);
inline constexpr u32 kDiffusePixelShader = boot_pixel_shader(19); // mov r0, v0: light cones and door shadows.
}
