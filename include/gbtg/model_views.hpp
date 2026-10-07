#pragma once

#include "gbtg/model_runtime.hpp"

namespace gbtg
{

// Host pointer/ownership views of already relocated model records. This adapter
// does not relocate, initialize, or acquire resources itself.
class RuntimeModelAssetView
{
public:
    struct Storage;
    std::unique_ptr<Storage> storage;
    RuntimeModelAsset asset{};

    RuntimeModelAssetView();
    ~RuntimeModelAssetView();
    RuntimeModelAssetView(const RuntimeModelAssetView&) = delete;
    RuntimeModelAssetView& operator=(const RuntimeModelAssetView&) = delete;
};

std::unique_ptr<RuntimeModelAssetView> view_relocated_runtime_model(
    const RuntimeRelocationImage& image, u32 directory,
    const RuntimeModelAllocation& runtime, std::span<const u8> resource_identity,
    std::span<const RuntimeModelNodeType> copy_types, u32 descriptor_base,
    const std::function<const u8*(u32)>& gpu_pointer);

struct RuntimePreparedModelResource
{
    RuntimeRelocationImage image;
    const RuntimeGpuSegments* gpu = nullptr;
    RuntimeModelAllocation runtime{};
    std::unique_ptr<RuntimeModelAssetView> view;
};
struct RuntimeModelResourceLoader
{
    // The bundle/executable provider owns these CPU/GPU allocations. Missing
    // preparation is an unresolved provider dependency, never an empty model.
    std::unordered_map<const u8*, RuntimePreparedModelResource> resources;
    RuntimeModelResourceViews& views;
    RuntimeNodeRelocationTypes relocation_types;
    std::span<const RuntimeModelNodeType> copy_types;
    RuntimeSpecialTextures& textures;
    const RuntimeModelHeapBoundaries& heap;
    const RuntimeNodeRelocationBoundaries& d3d;
    const std::function<const u8*(u32)>& gpu_pointer;
};
void register_runtime_model_resource_hook(AnimationResourceRegistry& registry,
    RuntimeModelResourceLoader& loader); // Xbox 0x00052F00

} // namespace gbtg
