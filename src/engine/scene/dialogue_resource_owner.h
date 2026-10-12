#pragma once
#include "gbtg/aid.h"
#include "../frontend/dialogue_text_metrics.h"
#include "../../gbtg/text/localtext_resources.h"
#include "scene_resource_memory.h"
#include "gbtg/model_views.h"
#include "gbtg/entity.h"
#include "../entities/body_resources.h"
#include <map>

namespace gbtg
{
// Native views and backing storage for the translated dialogue calls. The
// registry retains actual retail resources; no localized strings are invented.
class SceneDialogueResourceOwner
{
public:
    // With executable pools, their provider (0x00034E90/0x00034EC0, category 4) is registered first,
    // as 0x00034C30 does at boot, ahead of the bundle and demand provider.
    explicit SceneDialogueResourceOwner(AidStore& aids, std::span<ExecutableResourcePool> executable = {});
    SceneDialogueResourceOwner(const SceneDialogueResourceOwner&) = delete;
    SceneDialogueResourceOwner& operator=(const SceneDialogueResourceOwner&) = delete;
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    std::unordered_map<const u8*, LocalTextResourceView> text_views;
    LocalTextResourceContext text{registry, error, text_views, {}};
    std::array<RuntimeTextFont, 2> fonts{};
    std::array<const RuntimeTextFont*, 2> font_views{&fonts[0], &fonts[1]};
    std::array<RuntimeTextKerning, 2> kerning{};
    RuntimeTextMetrics metrics{font_views, kerning, 1.f, 1.f, 2.f};
    void load_fonts();
    // Model resources (hook 0x00052F00): relocated copies, their GPU segments and the host D3D state.
    SceneResourceMemory memory;
    SceneD3DResources d3d;
    RuntimeNodeRelocationBoundaries d3d_boundaries = scene_d3d_boundaries(d3d, memory);
    RuntimeModelHeapBoundaries model_heap = scene_model_heap_boundaries(memory);
    RuntimeBlendshapeBoundaries blendshapes = scene_blendshape_boundaries(memory);
    std::unique_ptr<RuntimeVertexShaderCache> shader_cache = std::make_unique<RuntimeVertexShaderCache>();
    u32 default_pixel_shader = 0;
    std::array<RuntimeNodeRelocationType, 17> relocation_types =
        runtime_model_relocation_types(*shader_cache, default_pixel_shader, d3d_boundaries);
    std::array<RuntimeModelNodeType, 17> copy_types = runtime_model_copy_types(blendshapes);
    RuntimeSpecialTextures special_textures{};
    RuntimeModelResourceViews model_views;
    std::function<const u8*(u32)> gpu_pointer = [this](u32 address)
    {
        return static_cast<const u8*>(memory.pointer(address));
    };
    // The node type registry (*DAT_004370A0) is a runtime allocation of 0x54-byte descriptors.
    RuntimeModelAllocation node_types = memory.allocate(u32(relocation_types.size()) * 0x54, 0x10);
    RuntimeModelResourceLoader models{{}, model_views, {node_types.address, relocation_types}, copy_types,
        special_textures, model_heap, d3d_boundaries, gpu_pointer};
    u32 loading = 0; // DAT_004DEDA0; the native provider relocates outside an asynchronous load.
    // Actor +0x204 views, keyed by the relocated CPU block. Actor code reads the model as one address
    // space of offsets: the original bundle bytes, with the runtime header that 0x00052B70 allocates
    // (directory +0x10) appended and its block pointers written as offsets.
    std::unordered_map<const u8*, ActorModel> actor_models;
    std::string last_missing; // Host diagnostic: the last name no provider record matched.
    // Retail words of each loaded row (DAT_004DF09C +8..+0x14), parallel to registry.loaded, as
    // 0x00035D90 reads them: CPU pointer, CPU size, resource-chunk pointer (or null) and its size.
    std::vector<RuntimeBodyLoadedResourceSize> loaded_sizes = std::vector<RuntimeBodyLoadedResourceSize>(0x1000);

private:
    struct Resource
    {
        AnimationBundleRecord record;
        LooseResource loose;
        std::vector<u8> text_file;
        std::vector<RuntimeLocalTextKey> keys;
        std::vector<RuntimeGpuSegment> segments;
        RuntimeGpuSegments gpu_segments;
        std::span<const u8> serialized; // The bundle's own model bytes, before relocation.
        u32 address = 0; // The relocated CPU block's address.
        std::vector<u8> actor_cpu;
        const BundleEntry* entry = nullptr;
    };
    AidStore& aids_;
    std::map<std::string, Resource> resources_;
    std::array<std::vector<RuntimeTextGlyph>, 2> glyphs_;
    std::array<LooseResource, 2> font_files_;
    Resource* find_resource(const char* name);
    void prepare_text(Resource& resource);
    void prepare_model(Resource& resource, const BundleEntry& entry);
    void publish_actor_model(const AnimationLoadedResource& loaded);
    RuntimeBodyLoadedResourceSize& loaded_row(const AnimationLoadedResource& output);
    void record_bundle_size(const AnimationBundleRecord& record, const AnimationLoadedResource& output);
};
}
