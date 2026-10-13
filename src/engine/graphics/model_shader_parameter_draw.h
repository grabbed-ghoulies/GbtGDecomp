#pragma once
#include "model_tree_dispatch.h"

namespace gbtg
{
// The material globals the shader-parameter draw publishes for later binds.
struct RuntimeModelMaterialGlobals
{
    u32 texture_table = 0; // DAT_004D976C, render state +0x70 when +0x74 is set.
    std::array<u32, 16> camera{}; // DAT_0054D9A0.
    std::array<float, 16> normalized_combined{}; // DAT_0054D960.
    std::array<float, 3> material_vector{}; // DAT_00336130, .data initial value.
    u32 texture_key = 0; // DAT_004D9770.
    u32 shadow_texture = 0; // DAT_004D9764; nothing writes it.
};
// DAT_004D9644: a list sentinel whose +4 link is the first record. Records are relocated addresses with
// their own +4 link, +0x68 use flag, +0xC4 key and +0xC8 texture.
struct RuntimeModelTextureRecords
{
    u32 first = 0; // DAT_004D9648; nothing links a record.
    std::function<u32(u32, u32&)> lookup; // DAT_0054D950; nothing binds it.
};
struct RuntimeModelMaterialBind
{
    ModelTreeDispatchContext& tree;
    RuntimeModelMaterialGlobals& globals;
    RuntimeModelTextureRecords& records;
    std::span<u32> deferred; // DAT_00181968: dirty mask, then 0x20-word texture stages from DAT_00181970.
    u32& alpha_test; // DAT_00181C60.
    u32& alpha_function; // DAT_00181C58.
    u32& alpha_reference; // DAT_00181C64.
    const std::array<u32, 16>& environment_primary; // DAT_004D9710.
    const std::array<u32, 16>& environment_secondary; // DAT_004D9680.
    std::function<void(u32, u32)> stage_texture; // DAT_004D9754 (texture, stage).
    std::function<void(u32)> material; // DAT_004D9750 (material).
    std::function<void(u32, u32)> set_texture; // D3D 0x001749E0 (stage, texture).
    std::function<void(u32, u32, u32)> bump_environment; // D3D 0x001719D0 (stage, type, value).
    std::function<void(u32, const float*, u32)> vertex_constants; // D3D 0x001733E0 / 0x00173440, vector count.
    std::function<void(u32, const float*, u32)> pixel_constants; // D3D 0x00175570.
    std::function<void(u32, u32)> simple; // D3D 0x00171050.
};
void set_runtime_model_texture_table(u32 table, RuntimeModelMaterialGlobals& globals); // Xbox 0x0015B6B0
void set_runtime_model_material_camera(const u32* camera, RuntimeModelMaterialGlobals& globals); // Xbox 0x0015B6C0
void set_runtime_model_normalized_combined(const float* combined,
    RuntimeModelMaterialGlobals& globals); // Xbox 0x0015B6E0
void set_runtime_model_material_vector(const float* vector, RuntimeModelMaterialGlobals& globals); // Xbox 0x0015B690
void set_runtime_model_texture_key(u32 key, RuntimeModelMaterialGlobals& globals); // Xbox 0x0015B720
void hash_runtime_model_name(const u8* text, i32 count, u32& hash); // Xbox 0x00158BB0
u32 runtime_model_material_texture_key(u32 node, RuntimeModelMaterialBind& context); // Xbox 0x00157030
void resolve_runtime_model_texture_record(u32 key, u32& texture, RuntimeModelMaterialBind& context); // Xbox 0x0015ABB0
void bind_runtime_model_material(u32 material, const RuntimeShaderInstance* instance,
    RuntimeModelMaterialBind& context); // Xbox 0x0015AD20
void dispatch_runtime_model_shader_parameters(u32 node, RuntimeModelInstanceStorage& storage,
    RuntimeModelMaterialBind& context); // Xbox 0x00157090
}
