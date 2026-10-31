#pragma once
#include "gbtg/types.h"

#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// The scene's refraction: the first background's type-0x13 mesh, a 128 x 128 noise texture, and the full-screen quad
// (four XYZRHW vertices with a texture set and two 3D sets, stride 0x30) that draws the back buffer through the noise
// over the mesh's stencil.
struct RuntimeSceneRefraction
{
    std::span<const u8> model; // The background model's CPU header the mesh's offsets resolve in.
    const u8* mesh = nullptr; // DAT_00510580: +0 the vertices (stride 0xC), +4 the u16 indices, +8 the triangles.
    u32 noise = 0; // DAT_00510584, a texture's native address.
    std::array<float, 48> quad{}; // DAT_00510588..DAT_00510647.
    float drift_u = 0.f; // DAT_00510648
    float drift_v = 0.f; // DAT_0051064C
};
extern RuntimeSceneRefraction g_scene_refraction;
extern u32 g_crt_random_seed; // The main thread's _tiddata +0x14 (holdrand); nothing in the XBE seeds it.

// CRT rand: holdrand = holdrand * 214013 + 2531011, its bits 16..30.
i32 runtime_crt_rand(u32& seed); // Xbox 0x00166C29
// The model's type-0x13 block, or null without one.
const u8* find_runtime_refraction_mesh(std::span<const u8> model); // Xbox 0x0011B7A0
// The quad over the back buffer (`width` x `height`): its first coordinates the drift plus the corner, the two 3D
// sets the corner and a tenth of the size.
void build_runtime_refraction_quad(RuntimeSceneRefraction& refraction, u16 width,
    u16 height); // Xbox 0x0011B7F0
// The 128 x 128 noise texture: each row's green and red offset from 0x7F by -1.5 sin and -5 cos of the row's angle
// (ten turns over the 128 rows) and a random -1..1, blue 0xFF and alpha 0x7F, swizzled into the texture.
void create_runtime_refraction_noise(RuntimeSceneRefraction& refraction,
    const std::function<u32(u32 width, u32 height, u32 levels, u32 usage, u32 format, u32 pool)>& create_texture,
    const std::function<std::span<u8>(u32 texture)>& texels, u32& seed); // Xbox 0x0011B8F0
}
