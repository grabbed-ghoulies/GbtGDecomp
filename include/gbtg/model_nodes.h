#pragma once
#include "gbtg/retail_field.h"

#include <array>

namespace gbtg
{
// A loaded model image's records, read by address through RuntimeRelocationImage. See docs/RETAIL_LAYOUTS.md.

// The model's breakable-piece header (CPU +0x10 -> +0x10; 0 for none): its group count at +0, piece count at +1, and
// from +0x14 a 0xC-byte row per group, with the group's piece list at +0 (from the header) and piece count at +2.
inline u32 runtime_model_piece_header(std::span<const u8> cpu)
{
    return read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
}
inline u8 runtime_model_piece_group_size(std::span<const u8> cpu, u32 header, u32 group)
{
    return cpu[header + 0x16 + group * 0xC];
}
inline u32 runtime_model_piece_group_list(std::span<const u8> cpu, u32 header, u32 group)
{
    return header + read_u16(cpu, header + 0x14 + group * 0xC);
}

// The model directory (0x00052910): its entries and their count.
namespace model_directory_field
{
constexpr RetailField<u32, 0x0> entries{};
constexpr RetailField<u32, 0x4> count{};
} // namespace model_directory_field

// A model directory entry (8 bytes): a block's kind (0 the node tree, 6 physics, 7 textures, 0x12 geometry, ...) and
// its address (0x00052910).
namespace model_directory_entry_field
{
constexpr RetailField<u32, 0x0> kind{};
constexpr RetailField<u32, 0x4> block{};
constexpr u32 kSize = 0x8;
} // namespace model_directory_entry_field

// Directory kind 0's block: the node tree and the GPU base its vertex data relocates against.
namespace model_tree_block_field
{
constexpr RetailField<u32, 0x0> tree{};
constexpr RetailField<u32, 0x4> gpu_base{};
} // namespace model_tree_block_field

// The node tree (0x00156790): root chains, the name table and two pointer-list tables.
namespace model_tree_field
{
constexpr RetailField<u32, 0x08> roots{}; // an array of root nodes
constexpr RetailField<u32, 0x0C> root_count{};
constexpr RetailField<u32, 0x10> names{};
constexpr RetailField<u32, 0x14> lists{};
constexpr RetailField<u32, 0x28> second_lists{};
} // namespace model_tree_field

// Every node's header (0x20 bytes): its type, its registry entry and its links. A chain is a node and its `next`
// siblings; `child` starts the chain below it.
namespace model_node_field
{
constexpr RetailField<u32, 0x00> type_name{}; // "ndPushBuffer" and the rest, by address
constexpr RetailField<u16, 0x04> type_index{}; // in the node type registry (0x00158770)
constexpr RetailField<u16, 0x06> mask{}; // the dispatch skips the node unless its mask shares a bit (0x001589F0)
constexpr RetailField<u32, 0x0C> instance_offset{}; // the node's data in a model instance's storage
constexpr RetailField<u32, 0x10> descriptor{}; // its registry entry (stride 0x54)
constexpr RetailField<u32, 0x14> child{};
constexpr RetailField<u32, 0x18> next{};
constexpr RetailField<u32, 0x1C> unk_1C{}; // a third link, relocated with the other two
constexpr u32 kSize = 0x20;
} // namespace model_node_field

// A node type registry entry (0x54 bytes, 0x00158770): its name inline, then the draw callback (0x00158900).
namespace model_node_type_field
{
constexpr RetailField<u32, 0x50> callback{}; // the callback's address
constexpr u32 kSize = 0x54;
} // namespace model_node_type_field

// ndVertexBuffer (0x001583C0): its vertex buffers. Each blend shape target is laid out the same way.
namespace vertex_buffer_node
{
constexpr RetailField<u32, 0x20> buffers{}; // records of 0x18 bytes (model_vertex_buffer_field)
constexpr RetailField<u32, 0x24> buffer_count{};
} // namespace vertex_buffer_node

// A vertex buffer record (0x18 bytes).
namespace model_vertex_buffer_field
{
constexpr RetailField<u8, 0x01> kind{}; // 9 for the buffer a blend shape target draws from
constexpr u32 header_offset = 0x4; // the D3D vertex buffer header
constexpr RetailField<u32, 0x10> data{}; // its GPU data
constexpr u32 kSize = 0x18;
} // namespace model_vertex_buffer_field

// ndBlendShape (0x001573A0, 0x00157A50): its targets, each a vertex buffer node.
namespace blendshape_node
{
constexpr RetailField<u32, 0x20> targets{}; // an array of target addresses
constexpr RetailField<u16, 0x24> target_count{};
constexpr RetailField<u8, 0x26> blends{}; // set: the targets are blended (0x00157650), else one is chosen (0x00157880)
} // namespace blendshape_node

// ndVertexShader and ndVtxDesc (0x001584A0, 0x00158510): the streams, the shader and its constant registers. A
// register of 0x60 or more is unused.
namespace vertex_shader_node
{
constexpr RetailField<u32, 0x20> streams{}; // an array of stream indices
constexpr RetailField<u32, 0x24> stream_count{};
constexpr RetailField<u32, 0x28> projection_register{};
constexpr RetailField<u32, 0x2C> world_register{};
constexpr RetailField<u32, 0x30> camera_register{};
constexpr RetailField<u32, 0x34> parameter_register{};
constexpr RetailField<u32, 0x3C> first_channel{}; // 0xFFFFFFFF for none (no lighting flags but bit 31)
constexpr RetailField<u32, 0x40> second_channel{}; // 0xFFFFFFFF for none
constexpr RetailField<i32, 0x44> variant{};
constexpr RetailField<u32, 0x4C> auxiliary_register{};
constexpr RetailField<u32, 0x50> declaration{};
constexpr RetailField<u32, 0x54> program{};
constexpr RetailField<u32, 0x58> strides{}; // one byte per stream
constexpr RetailField<u32, 0x64> shader{}; // its index in the vertex shader cache
} // namespace vertex_shader_node

// ndPushBuffer and ndBGPushBuffer (0x00158160, 0x001580A0, 0x001582A0): one recorded push buffer, or indexed draws.
namespace push_buffer_node
{
constexpr RetailField<u32, 0x20> draw_count{};
constexpr RetailField<u32, 0x24> header{}; // the D3D push buffer header; set: one recorded buffer
constexpr RetailField<u32, 0x28> data{};
constexpr RetailField<u32, 0x2C> size{};
// The indexed draws (D3D 0x001729C0): three arrays with one word per draw.
constexpr RetailField<u32, 0x30> draw_indices{};
constexpr RetailField<u32, 0x34> draw_primitives{};
constexpr RetailField<u32, 0x38> draw_counts{};
constexpr RetailField<u8, 0x3C> cull_disable{}; // nonzero draws with cull mode 0
constexpr RetailField<u8, 0x3D> state_slot{}; // signed: the instance state the node tests, -1 for none
constexpr RetailField<u8, 0x3E> state_value{}; // the slot's two bits it draws with
constexpr RetailField<u8, 0x3F> culled{}; // ndBGPushBuffer: outside the frustum this frame (0x00157140)
constexpr RetailField<u32, 0x40> unk_40{}; // ndBGPushBuffer
constexpr RetailField<u32, 0x44> unk_44{}; // ndBGPushBuffer
constexpr RetailField<Vec3, 0x48> bounds_min{}; // ndBGPushBuffer
constexpr RetailField<Vec3, 0x54> bounds_max{}; // ndBGPushBuffer
} // namespace push_buffer_node

// ndSkeleton (0x001569A0, 0x001569D0).
namespace skeleton_node
{
constexpr RetailField<u32, 0x20> joint_count{};
constexpr RetailField<u32, 0x24> joints{}; // 0x20-byte joint records
} // namespace skeleton_node

// A skeleton joint (0x20 bytes).
namespace skeleton_joint_field
{
constexpr RetailField<u16, 0x00> parent{}; // 0xFFFF for the root
constexpr RetailField<Vec3, 0x04> local{};
constexpr RetailField<Vec3, 0x10> world{};
constexpr u32 kSize = 0x20;
} // namespace skeleton_joint_field

// ndTransform and nd2D (0x00157EA0, 0x00157F10): a joint's matrix or the node's own.
namespace transform_node
{
constexpr RetailField<u8, 0x20> mode{}; // low nibble 2: the joint's matrix
constexpr RetailField<u16, 0x22> joint{};
constexpr u32 matrix_offset = 0x24; // the node's own matrix (0x40 bytes), transposed in place when relocated
} // namespace transform_node

// ndShader2 and ndShaderParam2 (0x00157C60, 0x001572D0): a primary and an optional secondary shader record.
namespace shader_node
{
constexpr RetailField<u32, 0x20> primary{};
constexpr RetailField<u32, 0x24> secondary{};
} // namespace shader_node

// ndMtxArray (0x00155D50, 0x00155DA0): its rows and the matrices they index.
namespace matrix_array_node
{
constexpr RetailField<u32, 0x20> rows{}; // 8-byte rows; byte 6 is the static matrix index
constexpr RetailField<i32, 0x24> row_count{};
constexpr RetailField<u8, 0x29> owns_matrices{}; // the matrices were copied to the heap at postload
constexpr RetailField<u16, 0x2A> matrices_offset{}; // the image matrices' offset from the node
constexpr RetailField<u32, 0x2C> matrices{}; // 0x40 bytes each
constexpr RetailField<u32, 0x30> matrix_count{};
} // namespace matrix_array_node

// ndRigidSkinIdx (0x00157C40, 0x00157B90): one matrix index byte per skin slot.
namespace rigid_skin_node
{
constexpr RetailField<u32, 0x20> indices{};
constexpr RetailField<u32, 0x24> index_count{};
} // namespace rigid_skin_node

// A shader parameter record, a node's material (0x0015ACB0 relocates it, 0x0015AD20 binds it).
namespace model_material_field
{
constexpr RetailField<u32, 0x00> colours{}; // RGBA bytes, one pixel constant each
constexpr RetailField<u32, 0x04> constants{}; // model_material_constant_field records
constexpr RetailField<u32, 0x08> stages{}; // model_material_stage_field records
constexpr RetailField<u32, 0x0C> stage_count{};
constexpr RetailField<u32, 0x10> constant_count{};
constexpr RetailField<u32, 0x14> colour_count{};
constexpr RetailField<u8, 0x18> alpha_reference{}; // 0: no alpha test
constexpr RetailField<u8, 0x1B> takes_shadow{};
constexpr RetailField<u32, 0x20> name{}; // hashed into the texture key
constexpr RetailField<u32, 0x24> entries{}; // model_material_entry_field records
constexpr RetailField<u32, 0x28> entry_count{};
constexpr RetailField<u32, 0x34> shadow_register{};
constexpr RetailField<u32, 0x40> shadow_stage{}; // the stage the shadow texture replaces
constexpr RetailField<u32, 0x44> instance_size{};
} // namespace model_material_field

// A material's texture stage (0x1C bytes).
namespace model_material_stage_field
{
constexpr RetailField<u32, 0x00> texture{}; // an index in the texture table, 0xFFFFFFFF for none
constexpr RetailField<u8, 0x04> address_u{};
constexpr RetailField<u8, 0x05> address_v{};
constexpr RetailField<u8, 0x06> address_w{};
constexpr RetailField<u8, 0x07> keyed{}; // the texture comes from the texture key's record
constexpr RetailField<u16, 0x08> bump_stage{}; // signed; negative for no bump mapping
constexpr RetailField<u16, 0x0A> unk_0A{}; // nonzero sets the stage's colour sign (0xF0000000)
constexpr RetailField<u32, 0x0C> bump_matrix_00{};
constexpr RetailField<u32, 0x10> bump_matrix_01{};
constexpr RetailField<u32, 0x14> bump_matrix_11{};
constexpr RetailField<u32, 0x18> bump_matrix_10{};
constexpr u32 kSize = 0x1C;
} // namespace model_material_stage_field

// A material's vertex constant (0x48 bytes): four vectors unless an environment replaces them.
namespace model_material_constant_field
{
constexpr u32 values_offset = 0x00;
constexpr RetailField<u32, 0x40> constant_register{};
constexpr RetailField<u32, 0x44> source{}; // 1 the primary environment, 2 the secondary, else the values
constexpr u32 kSize = 0x48;
} // namespace model_material_constant_field

// A material's named entry (0x10 bytes).
namespace model_material_entry_field
{
constexpr RetailField<u32, 0x0> name{};
constexpr RetailField<i32, 0x8> value{};
constexpr u32 kSize = 0x10;
} // namespace model_material_entry_field

// A pixel shader record (0x0015B340, 0x0015B360).
namespace model_pixel_shader_field
{
constexpr RetailField<u32, 0x4> program{};
constexpr RetailField<u32, 0xC> handle{};
} // namespace model_pixel_shader_field

// A texture record (0x00052710 relocates it, 0x00052770 posts it).
namespace model_texture_field
{
constexpr RetailField<u32, 0x00> format{};
constexpr RetailField<u32, 0x04> frames{}; // the frame headers (0x14 bytes each), from the record
constexpr RetailField<u16, 0x08> width{};
constexpr RetailField<u16, 0x0A> height{};
constexpr RetailField<u8, 0x0E> code{};
constexpr RetailField<u8, 0x0F> levels{};
constexpr RetailField<u8, 0x11> frame_count{};
constexpr RetailField<u8, 0x12> frame{};
constexpr RetailField<u8, 0x13> animated{}; // 0xFF with more than one frame
constexpr RetailField<u32, 0x14> data{}; // GPU data, 0 with none
constexpr RetailField<u32, 0x18> frame_size{};
constexpr u32 kFrameHeaderSize = 0x14;
} // namespace model_texture_field

// A texture table (0x00052710): its count and an array of texture records.
namespace model_texture_table_field
{
constexpr RetailField<u32, 0x0> count{};
constexpr RetailField<u32, 0x4> textures{};
} // namespace model_texture_table_field

// Directory kind 6, the physics block (0x00055620): a count, then records each starting with its kind and size.
namespace model_physics_block_field
{
constexpr RetailField<i32, 0x00> count{};
constexpr RetailField<u32, 0x0C> unk_0C{}; // relocated
constexpr u32 records_offset = 0x10;
} // namespace model_physics_block_field

namespace model_physics_record_field
{
constexpr RetailField<u8, 0x00> kind{}; // 1 a grid mesh, 0 and 2 a posed mesh
constexpr RetailField<u32, 0x08> size{}; // the next record is this far on
} // namespace model_physics_record_field

// Physics kinds 0 and 2: triangles posed by a joint (0x00054520).
namespace model_posed_mesh_field
{
constexpr RetailField<u32, 0x10> triangle_count{};
constexpr RetailField<u32, 0x14> triangles{}; // model_mesh_triangle_field records
constexpr RetailField<i32, 0x18> joint{};
} // namespace model_posed_mesh_field

// Physics kind 1: triangles bucketed in a grid of cells (0x00053720, 0x00054340). Coordinates are signed.
namespace model_grid_mesh_field
{
constexpr RetailField<std::array<u16, 3>, 0x10> cell_min{};
constexpr RetailField<std::array<u16, 3>, 0x16> cell_max{};
constexpr RetailField<u16, 0x1C> row_stride{};
constexpr RetailField<u16, 0x1E> plane_stride{};
constexpr RetailField<u16, 0x20> cell_count{};
constexpr RetailField<u16, 0x22> cell_size{};
constexpr RetailField<std::array<u16, 3>, 0x24> origin{};
constexpr RetailRecord<0x2C, 8> cells{}; // one per cell: model_grid_cell_field
} // namespace model_grid_mesh_field

namespace model_grid_cell_field
{
constexpr RetailField<u32, 0x0> triangle_count{};
constexpr RetailField<u32, 0x4> triangles{};
} // namespace model_grid_cell_field

// A mesh triangle (0x14 bytes).
namespace model_mesh_triangle_field
{
constexpr RetailField<std::array<u32, 3>, 0x00> vertices{}; // indices into the mesh's vertices
constexpr RetailField<u32, 0x0C> flags{};
constexpr u32 kSize = 0x14;
} // namespace model_mesh_triangle_field

// Directory kind 0x12, the geometry block (0x0004F500): its vertex streams.
namespace model_geometry_block_field
{
constexpr RetailField<u32, 0x08> unk_08{};
constexpr RetailField<u32, 0x0C> unk_0C{};
constexpr RetailField<u32, 0x1C> stream_count{};
constexpr RetailField<u32, 0x3C> streams{}; // records of 0x1C bytes (model_geometry_stream_field)
constexpr RetailField<u32, 0x40> unk_40{};
} // namespace model_geometry_block_field

namespace model_geometry_stream_field
{
constexpr RetailField<u32, 0x08> length{}; // the vertex buffer header's length
constexpr RetailField<u32, 0x0C> data{}; // a GPU offset, then its address
constexpr u32 header_offset = 0x10; // the D3D vertex buffer header
constexpr u32 kSize = 0x1C;
} // namespace model_geometry_stream_field
} // namespace gbtg
