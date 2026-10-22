#pragma once

#include "gbtg/types.h"
#include <functional>
#include <memory>
#include <unordered_map>
#include "gbtg/anim.h"
#include "gbtg/model_relocation.h"

namespace gbtg
{

struct RuntimeModelNode;
struct RuntimeShaderCpu;
struct RuntimeShaderInstance;
struct RuntimeBlendshapeCpu;
struct RuntimeModelInstanceStorage;
struct RuntimeModelNodeType
{
    const char* name; // type record +0, stride 0x54
    std::function<void(const RuntimeModelNode&, RuntimeModelInstanceStorage&)> copy; // +0x50 -> +4
};
struct RuntimeModelNode
{
    u32 instance_offset; // +0
    u16 type; // +4
    const RuntimeModelNodeType* descriptor; // +0x10
    RuntimeModelNode* child; // +0x14
    RuntimeModelNode* sibling; // +0x18
    const RuntimeShaderCpu* shader_cpu = nullptr; // shader node +0x20, tree-relative
    RuntimeShaderInstance* shader_instance = nullptr; // node instance descriptor +4
    u32 copy_offset = 0; // node +0xC; distinct from the auxiliary value offset at +0
    std::span<const u8> bytes; // original node bytes for type-specific data
    const RuntimeShaderCpu* secondary_shader_cpu = nullptr; // shader node +0x24
    const RuntimeBlendshapeCpu* blendshape_cpu = nullptr; // blendshape node +0x20 -> +0
    std::span<const Joint> joints; // skeleton +0x24, count +0x20
    const char* type_name = nullptr; // resolved original node +0, before descriptor lookup
    std::span<const u8> matrix_records; // ndMtxArray +0x20, 8-byte records, count +0x30
};
struct RuntimeModelNamedEntry
{
    const char* name; // +0, stride 0xC
    RuntimeModelNode* node; // +4
};
struct RuntimeModelNameTable
{
    std::span<const RuntimeModelNamedEntry> entries; // +4, count +0
};
struct RuntimeModelTree
{
    u32 instance_size; // +0
    std::span<RuntimeModelNode* const> roots; // +8, count +0xC
    const RuntimeModelNameTable* names; // +0x10
};

u32 find_runtime_model_type(std::span<const RuntimeModelNodeType> types, const char* name,
    const RuntimeModelNodeType*& result); // Xbox 0x00158820
u32 runtime_model_type_index(std::span<const RuntimeModelNodeType> types,
    const char* name); // Xbox 0x001588A0
RuntimeModelNode* find_runtime_model_node(RuntimeModelNode* first, u16 type); // Xbox 0x00156280
RuntimeModelNode* find_runtime_model_node_by_name(const RuntimeModelTree& tree, u16 root,
    const char* type, std::span<const RuntimeModelNodeType> types); // Xbox 0x001562C0
u32 runtime_model_instance_size(const RuntimeModelTree& tree); // Xbox 0x001561D0
void copy_runtime_model_node(const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance); // Xbox 0x001588E0
void copy_runtime_model_nodes(const RuntimeModelNode* first, RuntimeModelInstanceStorage& instance); // Xbox 0x00156120
void copy_runtime_model_tree(const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance); // Xbox 0x001561E0
bool find_runtime_model_named_node(const RuntimeModelTree& tree, const char* name,
    RuntimeModelNode*& result); // Xbox 0x001568E0
u32 runtime_model_named_count(const RuntimeModelTree& tree); // Xbox 0x00156960
void runtime_model_named_entry(const RuntimeModelTree& tree, int index,
    const char*& name, RuntimeModelNode*& node); // Xbox 0x00156970
void bind_model_auxiliary_targets(std::array<float*, 48>& targets, const RuntimeModelTree& tree,
    std::span<u8> instance, std::span<const char* const, 48> names); // Xbox 0x000F9AA0
extern const std::array<const char*, 48> runtime_model_auxiliary_names; // 0x00339A08
void bind_model_auxiliary_targets(std::array<float*, 48>& targets, const RuntimeModelTree& tree,
    std::span<u8> instance); // Xbox 0x000F9AA0, retail names

struct RuntimeShaderCpuEntry
{
    const char* name; // +0, stride 0x10
    i32 value; // +8: parameter index or signed animation rate
};
struct RuntimeShaderCpu
{
    std::span<const RuntimeShaderCpuEntry> entries; // +0x24, count +0x28
    std::span<const std::array<u8, 0x48>> initial_parameters; // +4, count +0x10
    u32 zero_word_count = 0; // +0xC
    u32 instance_size = 0; // +0x44
};
struct RuntimeShaderParameter
{
    std::array<u8, 0x44> bytes;
};
struct RuntimeShaderInstance
{
    std::span<RuntimeShaderParameter> parameters; // +0
    std::span<u32> zero_words; // +4
};
struct RuntimeShaderNodeInstance
{
    RuntimeShaderInstance* primary = nullptr; // node runtime +4
    RuntimeShaderInstance* secondary = nullptr; // +8
    RuntimeShaderInstance primary_instance;
    RuntimeShaderInstance secondary_instance;
    u8* primary_storage = nullptr;
};
struct RuntimeBlendshapeVertexBuffer
{
    std::array<u32, 6> words{}; // original 0x18-byte GPU resource record
    const u8* data = nullptr; // resolved word +0x10
};
struct RuntimeBlendshapeCpu
{
    std::span<const RuntimeBlendshapeVertexBuffer> buffers; // GPU table +0x20, count +0x24
};
using RuntimeModelGpuAllocator = std::function<u8*(u32, u32, const char*, u32)>;
struct RuntimeBlendshapeBoundaries
{
    RuntimeModelGpuAllocator allocator; // DAT_00434960, getter 0x00155D00
    std::function<void(RuntimeBlendshapeVertexBuffer&, const u8*)> register_resource; // D3DResource_Register
    std::function<int(RuntimeBlendshapeVertexBuffer&)> is_busy; // D3DResource_IsBusy
    std::function<void(const u8*, const char*, u32)> free_physical; // DAT_00434964, getter 0x00155D10
};
struct RuntimeBlendshapeNodeInstance
{
    std::array<RuntimeBlendshapeVertexBuffer, 16> buffers; // runtime +4, stride 0x18
    std::array<u8*, 16> attribute_targets{}; // +0x184
    float* weights = nullptr; // +0x1C4 points to +0x1C8
};
void release_runtime_blendshape_node(const RuntimeModelNode& node,
    RuntimeModelInstanceStorage& instance,
    const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x001574B0
void release_runtime_model_nodes(const RuntimeModelNode* first,
    RuntimeModelInstanceStorage& instance,
    const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x00156160
void release_runtime_model_tree_nodes(const RuntimeModelTree& tree,
    RuntimeModelInstanceStorage& instance,
    const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x00156210
// Host pointer fields remain per model instance, keyed by the original node
// instance offset. This avoids putting a 64-bit pointer into a 32-bit payload.
struct RuntimeModelInstanceStorage
{
    std::span<u8> bytes;
    std::unordered_map<u32, RuntimeShaderNodeInstance> shader_nodes;
    std::unordered_map<u32, RuntimeBlendshapeNodeInstance> blendshape_nodes;
    std::unordered_map<u32, ModelPoseBuffer*> skeleton_poses; // Native skeleton instance +4 pointer words.
};
std::array<RuntimeModelNodeType, 17> runtime_model_copy_types(
    const RuntimeBlendshapeBoundaries& boundaries); // bindings from Xbox 0x00155A10
const RuntimeModelGpuAllocator& runtime_model_gpu_allocator(
    const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x00155D00
void copy_runtime_blendshape_buffers(const RuntimeModelNode& node,
    RuntimeBlendshapeNodeInstance& output, const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x00157550
void copy_runtime_node_blendshape(const RuntimeModelNode& node, RuntimeBlendshapeNodeInstance& output,
    std::span<u8> instance, const RuntimeBlendshapeBoundaries& boundaries); // Xbox 0x00157AA0
struct RuntimeShaderAnimation
{
    RuntimeShaderParameter* parameter; // +0
    std::array<float, 3> position; // +4..+0xC
    std::array<float, 5> rates; // +0x10..+0x20
    std::array<u32, 2> retained; // +0x24..+0x28, not written by setup
};
struct RuntimeShaderAnimationList
{
    u32 count; // +0
    std::unique_ptr<RuntimeShaderAnimation[]> entries; // +4
};
struct RuntimeModelShaderAnimations
{
    u32 count; // +0
    std::unique_ptr<std::unique_ptr<RuntimeShaderAnimationList>[]> entries; // +4
};
struct RuntimeShaderAnimationScratch
{
    std::array<RuntimeShaderAnimation, 100> animations{}; // 0x004DBF88
    std::array<RuntimeShaderAnimationList*, 100> lists{}; // 0x004DBDF8, borrowed scratch pointers
};
struct RuntimeModelResourceBlock
{
    u32 type; // CPU directory record +0, stride 8
    const u8* data; // +4
    u32 word = 0; // +4 as stored after relocation; type 1 is not relocated and holds the node mask 0x000320F0 reads.
};
struct RuntimeShaderRegistrationState
{
    std::array<u8*, 40> records{}; // 0x00509B7C
    u32 count = 0; // 0x00509B78
};
using RuntimeShaderMatrix = std::array<float, 16>;
RuntimeShaderMatrix& multiply_runtime_matrix(RuntimeShaderMatrix& output,
    const RuntimeShaderMatrix& left, const RuntimeShaderMatrix& right); // Xbox 0x00151E64
void write_runtime_model_matrix(std::span<const u8> node, std::span<u8> instance,
    i32 index, const RuntimeShaderMatrix& matrix); // Xbox 0x00156010
struct RuntimeShaderMatrixBoundaries
{
    std::function<void(RuntimeShaderMatrix&, float, float, float)> translation;
    std::function<void(RuntimeShaderMatrix&, float, float, float)> scaling;
    std::function<void(RuntimeShaderMatrix&, float)> rotation_x;
    std::function<void(RuntimeShaderMatrix&, float)> rotation_y;
    std::function<void(RuntimeShaderMatrix&, float)> rotation_z;
    std::function<void(RuntimeShaderMatrix&, const RuntimeShaderMatrix&, const RuntimeShaderMatrix&)> multiply;
    std::function<void(RuntimeShaderMatrix&, const RuntimeShaderMatrix&)> transpose;
};
void runtime_shader_rotation(RuntimeShaderMatrix& output, char axis, float radians,
    const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x00165260
void tick_runtime_shader_animation(RuntimeShaderAnimation& animation, float step,
    const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x001658B0
void tick_runtime_model_shader_animations(RuntimeModelShaderAnimations& animations, float step,
    const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x00165A00

u32 runtime_model_shader_storage_size(std::span<const RuntimeModelResourceBlock> blocks); // Xbox 0x00052F40
void register_runtime_model_shaders(u8* shader_storage,
    RuntimeShaderRegistrationState& state); // Xbox 0x000FDF60
void copy_runtime_node_empty(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x00158940
void copy_runtime_node_empty_thunk(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x00158670
void copy_runtime_node_bg_push_buffer(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x001580C0
void copy_runtime_node_visibility(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x00158070
void copy_runtime_node_transform(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x00158040
void copy_runtime_node_matrix_array(const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x00155E20
u32 runtime_shader_instance_size(const RuntimeShaderCpu& shader); // Xbox 0x0015B260
void initialize_runtime_shader_instance(const RuntimeShaderCpu& shader,
    RuntimeShaderInstance& instance, std::span<u8> storage); // Xbox 0x0015B040
void copy_runtime_node_shader_parameters(const RuntimeModelNode& node,
    RuntimeShaderNodeInstance& output, std::span<u8> instance); // Xbox 0x00157320

const RuntimeShaderCpuEntry* runtime_shader_entry(const RuntimeShaderCpu& shader,
    u32 index); // Xbox 0x0015B0C0
bool find_runtime_shader_entry(const RuntimeShaderCpu& shader, const char* name,
    const RuntimeShaderCpuEntry*& result); // Xbox 0x0015B0E0
bool find_runtime_shader_parameter(const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    const char* name, RuntimeShaderParameter*& result); // Xbox 0x0015B1F0
void set_runtime_shader_parameter_flag(const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    const char* name, u8 flag); // Xbox 0x0015B230
void runtime_shader_animation_name(const char* prefix, const char* parameter,
    char* output); // Xbox 0x00165300
std::array<float, 5> runtime_shader_animation_rates(const RuntimeShaderCpu& shader,
    const char* parameter); // Xbox 0x00165360
std::unique_ptr<RuntimeShaderAnimationList> make_runtime_shader_animation_list(
    const char* prefix, const RuntimeShaderCpu& shader, RuntimeShaderInstance& instance,
    RuntimeShaderAnimationScratch& scratch); // Xbox 0x00165570
void collect_runtime_model_shader_animations(const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance,
    RuntimeShaderAnimationScratch& scratch, u32& count); // Xbox 0x001657A0
std::unique_ptr<RuntimeModelShaderAnimations> make_runtime_model_shader_animations(
    const RuntimeModelTree& tree, RuntimeModelInstanceStorage& instance,
    RuntimeShaderAnimationScratch& scratch); // Xbox 0x00165830

struct RuntimeModelAsset
{
    std::span<const RuntimeModelResourceBlock> blocks; // CPU +0, count +4
    const RuntimeModelTree* tree; // CPU +0x10 -> +0x24
    std::span<const u8> cpu; // original CPU resource identity, for registry ownership
};
struct RuntimeModelInstance
{
    const RuntimeModelAsset* source; // +0
    std::unique_ptr<u8[]> allocation;
    size_t allocation_size;
    RuntimeModelInstanceStorage tree_storage; // +0x2A0
    u8* shader_storage; // +0x28C
    RuntimeModelNode* skeleton_node; // +0x1BC
    ModelPoseBuffer* render_pose = nullptr; // skeleton runtime record +4, host view of pose +0x10
    std::array<u32, 20> shader_indices; // +0x16C..+0x1B8
    ModelPoseBuffer primary_pose;
    ModelPoseBuffer* primary; // +0x164
    ModelPoseBuffer* alternate; // +0x168
    std::array<float*, 48> auxiliary_targets; // +0x1CC
    std::array<u8, 8> node_flags; // +0x294 / +0x298
    u32 state_290;
    u32 state_1c4;
    u32 state_1c0;
    u32 state_1c8; // Model +0x1C8, the draw's second material word.
    u32 mirror_pass = 0; // Model +0x15C, the mirror flag 0x00121900 stores before each world pass.
    std::unique_ptr<RuntimeModelShaderAnimations> shader_animations; // +0x29C
};
void set_runtime_model_node_state(RuntimeModelInstance& model, u8 slot, u8 value,
    std::span<u32> shared_states); // Xbox 0x000F96F0
void bind_runtime_model_pose(RuntimeModelInstance& model, ModelPoseBuffer* pose,
    const RuntimeShaderMatrixBoundaries& boundaries); // Xbox 0x000F9550
std::unique_ptr<RuntimeModelInstance> make_runtime_model_instance(const RuntimeModelAsset& asset,
    int create_pose, int copy_shaders, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch); // Xbox 0x000F9840
// The matrices a source ndMtxArray instance flagged (by record byte +6), with their flags set in the copy.
void copy_runtime_matrix_array_flags(const RuntimeModelNode& source_node, std::span<const u8> source,
    const RuntimeModelNode& node, std::span<u8> instance); // Xbox 0x001560A0
void copy_runtime_model_node_flags(RuntimeModelInstance& model,
    const RuntimeModelInstance& source); // Xbox 0x000F8B80, EAX model, ECX source
// A copy of a live instance: its header and shader storage, a fresh tree copy with the flagged matrices, its
// primary pose and node flags.
std::unique_ptr<RuntimeModelInstance> clone_runtime_model_instance(const RuntimeModelInstance& source,
    std::span<const RuntimeModelNodeType> types, AnimationResourceRegistry& registry,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch); // Xbox 0x000F87D0
struct RuntimeModelDeferredRelease
{
    RuntimeModelInstance* instance; // +0, stride 8
    u32 fence; // +4
    std::unique_ptr<RuntimeModelInstance> owner; // host ownership of the queued allocation
};
struct RuntimeModelDeferredPool
{
    std::vector<RuntimeModelDeferredRelease> records; // pool +2/+4, original stride 8
    u16 grow; // pool +2 word
    u32 limit; // (pool limit - begin) / stride
};
void grow_runtime_model_deferred_pool(RuntimeModelDeferredPool& pool); // Xbox 0x000662A0
void release_runtime_model_shader_animations(
    std::unique_ptr<RuntimeModelShaderAnimations>& animations); // Xbox 0x00165A60
void unregister_runtime_model_shaders(u8* shader_storage, RuntimeShaderRegistrationState& state,
    const std::function<void(u8*)>& heap_free); // Xbox 0x000FDFC0
const RuntimeModelAsset* defer_runtime_model_release(RuntimeModelInstance& instance,
    RuntimeModelDeferredPool& pool, u32 fence, AnimationResourceRegistry& registry,
    RuntimeShaderRegistrationState& registration,
    const std::function<void(u8*)>& heap_free); // Xbox 0x000F8BF0
ModelPoseBuffer* release_runtime_model_pose(ModelPoseBuffer* pose); // Xbox 0x00031470
const RuntimeModelAsset* release_runtime_model_instance(RuntimeModelInstance& instance,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000F8C50
void collect_runtime_model_releases(RuntimeModelDeferredPool& pool, u32& fence,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000F9780
void flush_runtime_model_releases(RuntimeModelDeferredPool& pool, u32& fence,
    AnimationResourceRegistry& registry, const RuntimeBlendshapeBoundaries& blendshape,
    const RuntimeModelHeapBoundaries& heap,
    const RuntimeNodeRelocationBoundaries& d3d); // Xbox 0x000F97F0
struct RuntimeActorModelState
{
    std::unique_ptr<RuntimeModelInstance> model; // actor +0x204
    std::unique_ptr<RuntimeModelInstance> secondary; // actor +0x310
    std::array<char, 128> name{}; // +0x208
};
struct RuntimeActorResourceReference
{
    std::span<const u8> resource; // actor +0x8C
    std::array<char, 0x100> name{}; // +0x90
};
void set_runtime_actor_resource(RuntimeActorResourceReference& reference, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x0003BFD0
struct RuntimeModelResourceViews
{
    // Typed views of already-relocated CPU resources. An absent view is an
    // untranslated resource-loader dependency, never a substitute model.
    std::unordered_map<const u8*, const RuntimeModelAsset*> assets;
};
// A fresh instance of the model an AID names, with each of the source skeleton's joints that the new model names
// too taking the source's pose matrix; null (after error 2) when the AID has no resource.
std::unique_ptr<RuntimeModelInstance> make_runtime_model_copy_by_name(const char* name,
    const RuntimeModelInstance& source, AnimationResourceRegistry& registry, ResourceErrorState& error,
    const RuntimeModelResourceViews& views, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch); // Xbox 0x000F8A10
bool set_runtime_actor_model(RuntimeActorModelState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error, const RuntimeModelResourceViews& views,
    RuntimeModelDeferredPool& deferred, u32 fence, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch,
    const std::function<void(u8*)>& heap_free); // Xbox 0x00040C00
void set_runtime_actor_secondary_model(RuntimeActorModelState& actor, const char* name,
    AnimationResourceRegistry& registry, ResourceErrorState& error, const RuntimeModelResourceViews& views,
    RuntimeModelDeferredPool& deferred, u32 fence, std::span<const RuntimeModelNodeType> types,
    RuntimeShaderRegistrationState& registration, RuntimeShaderAnimationScratch& scratch,
    const std::function<void(u8*)>& heap_free); // Xbox 0x00041F90

} // namespace gbtg
