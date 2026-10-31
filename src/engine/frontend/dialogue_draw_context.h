#pragma once
#include "dialogue_activation.h"
#include "gbtg/model_render.h"
#include <functional>

namespace gbtg
{
// These callbacks are the D3D/D3DX calls made by the retail drawing leaves.
struct DialogueD3DBoundaries
{
    std::function<void(u32, u32)> simple_state;
    std::function<void(u32)> noise, cull, depth, vertex_shader, pixel_shader;
    std::function<void(u32, const u8*)> texture;
    std::function<void(u32, u32)> texture_coordinates;
    std::function<void(u32, const void*, u32)> stream;
    std::function<void(u32, u32, const void*)> shader_input;
    std::function<void(u32, const float*)> set_transform;
    std::function<void(u32, float*)> get_transform;
    std::function<void(std::span<const u32, 6>)> viewport;
    // D3D 0x001735D0 (register, data, count in dwords): the dwords fill the constants from the register on.
    std::function<void(u32, const u32*, u32)> constants_not_inline;
    std::function<void(u32, const float*, u32)> constants;
    std::function<void(float*, const float*, const float*)> multiply;
    std::function<void(float*, const float*)> transpose;
    std::function<u32*(u32)> begin_push;
    std::function<void(u32*)> end_push;
    // D3DDevice_DrawVerticesUP (D3D 0x001727B0): primitive, vertex count, the vertices and their stride in bytes.
    std::function<void(u32, u32, const void*, u32)> draw_vertices_up;
    std::function<void(u32)> multisample_mode; // D3D 0x001725A0.
    std::function<void(u32)> stencil_enable; // D3D 0x00172330.
    std::function<void(u32, u32)> vertex_data_color; // D3D 0x00172CC0 (register, D3DCOLOR).
    std::function<void(u32)> begin; // D3DDevice_Begin, D3D 0x00172D10 (primitive).
    std::function<void(i32, float, float, float, float)> vertex_data4f; // D3D 0x00172C60 (register, x, y, z, w).
    std::function<void(i32, float, float)> vertex_data2f; // D3D 0x00172C20 (register, a, b).
    std::function<void()> end; // D3DDevice_End, D3D 0x00172D50.
};
struct DialogueRenderFrame
{
    std::array<float, 16> projection; // native frame+0x84
    const RuntimeShaderMatrix*& view; // Native frame +0x40, the frame's camera matrix pointer slot.
};
struct RuntimeDialogueDrawState
{
    i32 font; // DAT_005043D8
    u32 active; // DAT_005043DC
    u32 color; // DAT_005043E0
    float text_scale_x; // DAT_005043E4
    float text_scale_y; // DAT_005043E8
    u32 retained; // DAT_005043EC
    float angle; // DAT_005043F0
    u32 retained_f4; // DAT_005043F4
    float half_height; // DAT_005043F8
    float half_width; // DAT_005043FC
    float wrap; // DAT_00504400
    u32 shadow_color; // DAT_00504404
    float shadow_x; // DAT_00504408
    float shadow_y; // DAT_0050440C
    u32 random; // DAT_00504410
    float fade; // DAT_0042496C
    float width; // DAT_00424950
    i32 lines; // DAT_00424954
    float line_step; // DAT_00424948
    float scale_x; // DAT_0042493C
    float scale_y; // DAT_00424940
    float space; // DAT_0042494C
    float paragraph_scale; // DAT_00424910
    float paragraph_offset; // DAT_00424914
    float wave_speed; // DAT_00424918
    float wave_height; // DAT_0042491C
    float origin_x; // DAT_00424900
    float origin_y; // DAT_00424904
    std::array<float, 2> speaker_position; // DAT_004E6090
    std::array<float, 2> speaker_scale; // DAT_004E6098
};
struct RuntimeDialogueOuterDrawContext;
struct RuntimeDialogueDrawContext
{
    RuntimeDialogueActivationContext& dialogue;
    RuntimeDialogueDrawState& state;
    RuntimeTextMetrics& metrics;
    DialogueD3DBoundaries& d3d;
    RuntimeModelRenderContext& renderer;
    std::unordered_map<RuntimeModelRenderFrame*, DialogueRenderFrame>& frames;
    std::span<u32> deferred; // Native view of DAT_00181968..00181D08.
    u32& override_count; // DAT_00509B5C
    i32& fog_disabled; // DAT_00509B00
    std::array<float, 24>& affine; // DAT_0050D7A0, matrix followed by scratch matrix.
    float*& current_affine; // DAT_0050DD70
    std::array<float, 16>& projection; // DAT_00506F58
    std::array<float, 16>& view; // DAT_00506F18
    std::array<float, 16>& world; // DAT_00506ED8
    std::array<float, 16>& sprite_world; // DAT_00504378
    std::span<const u32, 4> fitted_viewport; // Current display+0x58.
    i32 logical_width; // DAT_0054DD80
    i32 logical_height; // DAT_0054DD84
    u16 width;
    u16 height;
    u32 white; // DAT_00314F48
    u32 vertex_shader; // DAT_004273D4
    u32 pixel_shader; // DAT_00425E6C
    const u32* shader_constants; // PTR_DAT_004273C8's data, null when it is 0.
    std::array<u32, 3> quad_zero; // DAT_005462B4/B8/BC
    u32 quad_one; // DAT_0042515C
    std::array<float, 2> quad_top; // DAT_0042514C
    std::array<float, 2> quad_bottom; // DAT_00425154
    const u8* fallback_texture; // DAT_004E1C58
    float animation_time; // DAT_004312C8
    u32 counter_baseline; // DAT_0050E0D8
    std::function<std::int64_t()> counter; // Existing QueryPerformanceCounter boundary.
    float ordinary_line_scale; // DAT_00424908
    float tab_line_scale; // DAT_0042490C
    // A pooled text record's panel (0x0005FC30): its colour, the alpha 0x00060160 fades it to and the draw.
    u32 panel_color = 0; // DAT_002FF2E0
    float panel_alpha = 0.f; // DAT_0032B49C
    RuntimeDialogueOuterDrawContext* outer = nullptr; // 0x00061360's context.
};
void make_dialogue_projection(float* output, float top, float bottom, float left,
    float right, float near_plane, float far_plane); // Xbox 0x001538A0
void set_dialogue_render_projection(const float* matrix, RuntimeDialogueDrawContext& context); // Xbox 0x00156400
void set_dialogue_render_view(const float* matrix, RuntimeDialogueDrawContext& context); // Xbox 0x00156330
void make_dialogue_axis_rotation(float* output, u8 axis, float sine, float cosine); // Xbox 0x001046A0
void rotate_dialogue_affine(u8 axis, float angle, RuntimeDialogueDrawContext& context); // Xbox 0x00104160
void initialize_dialogue_orthographic(RuntimeDialogueDrawContext& context); // Xbox 0x000F6260
void reset_dialogue_sprite_state(RuntimeDialogueDrawContext& context); // Xbox 0x0005E110
void set_dialogue_alpha_state(RuntimeDialogueDrawContext& context); // Xbox 0x0005E0C0
void set_dialogue_texture_state(RuntimeDialogueDrawContext& context); // Xbox 0x0005E2A0
void begin_dialogue_sprite_draw(RuntimeDialogueDrawContext& context); // Xbox 0x0005CDC0
void reset_dialogue_font(i32 font, RuntimeDialogueDrawContext& context); // Xbox 0x000605A0
void set_dialogue_font_scale(float x, float y, RuntimeDialogueDrawContext& context); // Xbox 0x00060640
void bind_dialogue_texture(u8* texture, u32 stage, RuntimeDialogueDrawContext& context); // Xbox 0x00053550
void draw_dialogue_quad(u32 color, RuntimeDialogueDrawContext& context); // Xbox 0x0005E380
void draw_dialogue_sized_quad(const float* dimensions, u32 color, RuntimeDialogueDrawContext& context); // Xbox 0x0005E570
void draw_dialogue_uv_quad(const float* top, const float* bottom, u32 color,
    const float* uv_top, const float* uv_bottom, RuntimeDialogueDrawContext& context); // Xbox 0x0005E770
void draw_dialogue_texture(i32 frame, const float* uv_top, u8* texture, const float* scale,
    const float* uv_bottom, float angle, u32 color, const float* position,
    RuntimeDialogueDrawContext& context); // Xbox 0x0005CA60
void draw_dialogue_icon(float& x, float y, float scale_x, float scale_y, u8 frame,
    RuntimeDialogueDrawContext& context); // Xbox 0x000577E0
void draw_dialogue_speaker(RuntimeDialogueDrawContext& context); // Xbox 0x000575F0
u8 dialogue_icon_frame(float time, RuntimeDialogueDrawContext& context); // Xbox 0x00057A80
float dialogue_glyph_wave(u16 code, float time, float speed, float height,
    RuntimeDialogueDrawContext& context); // Xbox 0x0005F190
float dialogue_glyph_bearing(i32 font, u16 code, float scale,
    RuntimeDialogueDrawContext& context); // Xbox 0x000608D0
void draw_runtime_dialogue_glyph(u32 code, float& x, float& y, float speed,
    float height, float time, u32 frame, u16 previous, i32 shadow,
    RuntimeDialogueDrawContext& context); // Xbox 0x0005E9F0
void draw_runtime_dialogue_text(const char16_t* text, float& x, float& y,
    u32 frame, const u32* frames, float speed, float height, float time, i32 shadow,
    RuntimeDialogueDrawContext& context); // Xbox 0x0005ED80
// DAT_00504410 from a pooled text's timer mode: a nonzero mode itself, else the counter since DAT_0050E0D8.
void set_dialogue_glyph_seed(u32 mode, RuntimeDialogueDrawContext& context); // Xbox 0x000606A0, EAX mode
// `count` characters of a wrapped line from (x, y): glyphs through 0x0005E9F0, spaces by the space advance, a
// newline back to the original x one cell lower.
void draw_runtime_dialogue_text_run(const char16_t* text, float& x, float& y, i32 count, const u32* frames,
    float speed, float height, float time, i32 shadow, RuntimeDialogueDrawContext& context); // Xbox 0x0005EC60
// Text centred on x. Narrower than the wrap width it is one line; otherwise each line breaks at the space that
// would pass the wrap width and is centred on its own width.
void draw_runtime_centered_dialogue_text(const char16_t* text, float& x, float& y, u32 frame,
    const u32* frames, float speed, float height, float time, u32 seed_mode, i32 shadow,
    RuntimeDialogueDrawContext& context); // Xbox 0x0005EF40
float dialogue_line_position(i32 alignment, i32 line, float origin,
    const DialoguePageLayout& page, RuntimeDialogueDrawContext& context); // Xbox 0x0005A630
void draw_runtime_dialogue_page(DialoguePageLayout& page, float fade,
    RuntimeDialogueDrawContext& context); // Xbox 0x00058C30
}
