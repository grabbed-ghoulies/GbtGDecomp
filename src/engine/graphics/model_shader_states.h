#pragma once
#include "gbtg/model_render.h"
#include "model_library_hooks.h"
#include "model_tree_dispatch.h"

namespace gbtg
{
struct RuntimeModelShaderStates
{
    u32& depth_function; // DAT_00181C54.
    u32& depth_write; // DAT_00181C70.
    u32& blend; // DAT_00181C5C.
    u32& alpha_test; // DAT_00181C60.
    u32& blend_operation; // DAT_00181C98.
    u32& source_blend; // DAT_00181C68.
    u32& destination_blend; // DAT_00181C6C.
    u32& alpha_function; // DAT_00181C64.
    std::function<void(u32, u32)> simple;
    std::function<void(u32)> z_enable;
};
void set_runtime_model_opaque_states(RuntimeModelShaderStates& states); // Xbox 0x0015B380
void set_runtime_model_alpha_depth_states(RuntimeModelShaderStates& states); // Xbox 0x0015B3F0
void set_runtime_model_alpha_states(RuntimeModelShaderStates& states); // Xbox 0x0015B4B0
void set_runtime_model_additive_states(RuntimeModelShaderStates& states); // Xbox 0x0015B580
struct RuntimeModelShaderDraw
{
    ModelTreeDispatchContext& tree;
    RuntimeModelShaderStates& states;
    std::function<void(u32)> pixel_shader;
    RuntimeModelLibraryHooks& hooks; // DAT_004D9768, the post-shader hook.
};
void bind_runtime_model_shader_callback(std::function<void(u32)> callback,
    RuntimeModelLibraryHooks& hooks); // Xbox 0x0015B660
void bind_runtime_model_shader(u32 shader, RuntimeModelShaderDraw& context); // Xbox 0x0015B5E0
u32 runtime_model_shader_texture_stage(u32 shader,
    const RuntimeRelocationImage& image); // Xbox 0x0015B670
void dispatch_runtime_model_shader(u32 node, RuntimeModelShaderDraw& context); // Xbox 0x00157CE0
struct RuntimeModelShaderCombiner
{
    RuntimeModelShaderDraw& draw;
    std::span<u32> shadows; // D3D_g_RenderState, DAT_00181B70.
    u32& dirty; // DAT_00181968.
    u32& control; // DAT_00181C44.
    u32& texture_stage; // DAT_005078D0.
    std::function<void(u32, u32)> specialized_state; // Actual D3D setter address, value.
};
void configure_runtime_model_texture_combiner(u32 flags, i32 stage,
    RuntimeModelShaderCombiner& context); // Xbox 0x000F7420
void set_runtime_model_alpha_test_states(RuntimeModelShaderStates& states); // Xbox 0x0005E0C0
void apply_runtime_model_shader_overrides(RuntimeModelInstance& model, u32 shader,
    RuntimeModelShaderCombiner& context); // Xbox 0x000F84A0
void post_runtime_model_shader(u32 shader, RuntimeModelShaderCombiner& context); // Xbox 0x000F9B40
void set_runtime_model_stage_colors(u32 first, u32 second,
    RuntimeModelShaderCombiner& context); // Xbox 0x000F7FA0
// DAT_004D9750 as 0x000F8740 binds it; the material argument is unused.
void apply_runtime_model_material_colors(RuntimeModelShaderCombiner& context); // Xbox 0x000F9B90
}
