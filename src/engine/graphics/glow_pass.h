#pragma once

#include "../frontend/dialogue_draw_outer.h"
#include "../camera/display_view.h"

namespace gbtg
{
// The glow targets boot 0x000620C0 makes and 0x00062220 fills.
struct RuntimeGlowTargets
{
    DialogueSurfaceHandle page_surface = 0; // DAT_005047E0, book page 0's surface.
    DialogueSurfaceHandle depth_surface = 0; // DAT_005047FC.
    u8* page_texture = nullptr; // DAT_005047DC, book page 0's frame record.
    u8* depth_texture = nullptr; // DAT_005047E8, the depth surface's memory as LIN_A8R8G8B8.
    std::array<float, 8> corners{}; // DAT_00504804..DAT_00504820: four x, y corners.
    std::array<i32, 4> rectangle{}; // DAT_00504824..DAT_00504830, the D3DRECT 0x00062220 clears.
};
struct RuntimeGlowPass
{
    RuntimeDialogueOuterDrawContext& outer; // Targets DAT_005047D0 / DAT_005047D4 / DAT_00504800, D3D.
    RuntimeGlowTargets& targets;
    const i32& transition_guard; // DAT_0050484C.
    std::function<u32(u32 address)> word_at; // Shader handles and tables in the image.
    std::function<void(u32)> stencil_fail; // D3DDevice_SetRenderState_StencilFail.
    std::function<void(u32)> texture_modes; // D3DDevice_SetRenderState_PSTextureModes.
    std::function<u8*(u32 address)> texture_at; // A texture resource's record by native address.
};

// The screen rectangle of a cylinder (EAX centre, XMM4 radius, the stack height) through the display view: EDI
// receives the minimum, EBX the maximum. False when it misses the viewport or collapses.
bool runtime_screen_cylinder_rect(const Vec3& center, float radius, float height, float* minimum, float* maximum,
    const entity_detail::DisplayView& view); // Xbox 0x0013BCC0
u32 scale_runtime_color(u32 color, float scale); // Xbox 0x00101640
// Draws go to book page 0 and its depth surface; the rectangle (or the viewport when either corner is null) is
// cleared.
void begin_runtime_glow_capture(const float* minimum, const float* maximum,
    RuntimeGlowPass& context); // Xbox 0x00062220, EDI minimum, EAX maximum
void set_runtime_glow_depth_states(RuntimeGlowPass& context); // Xbox 0x00061D00
void set_runtime_glow_color_states(u32 vertex_shader, u32 pixel_shader, u32 color,
    RuntimeGlowPass& context); // Xbox 0x00061EE0, ECX vertex shader, EAX pixel shader
// Back to the frame: the captured depth is stamped into the stencil over the rectangle, then the colour states.
u32 begin_runtime_glow_composite(u32 color, u32 vertex_shader, u32 pixel_shader,
    RuntimeGlowPass& context); // Xbox 0x000623C0
void composite_runtime_glow(u32 first_texture, u32 pixel_shader, u32 second_texture, u32 first, u32 second,
    RuntimeGlowPass& context); // Xbox 0x00062750, ECX first texture, EAX pixel shader, EBX second texture
}
