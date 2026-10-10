#pragma once
#include "body_attachment_cleanup.h"

namespace gbtg
{
struct RuntimeParentBodyConstruction
{
    RuntimeParentBodyCleanup& owner;
    RuntimeBodyActionSelection& selection;
    RuntimeBodyAction& action;
    std::span<u8> actor_bytes;
    const RuntimeBodyModelNodes* nodes;
    const u8* model_channels; // Native byte view of actor +0x204 model channel fields.
    std::span<const u16> scale_exclusions; // DAT_0050E1B4.
    i32 scale_exclusion_count;
    std::span<const u8> animation_classes; // DAT_0050E170, stride 8.
    i32 animation_class_count;
    std::span<const u8> friction_curve; // DAT_00433D68.
    std::span<const u8> tags; // DAT_00545AC4.
    std::span<u32, 10> resource_counts;
    std::span<const RuntimeBodyLoadedResourceSize> loaded_sizes;
    const std::unordered_map<const u8*, const RuntimeScriptLookupTable*>& action_tables;
};
bool initialize_runtime_parent_body(std::span<u8> body, std::span<const u8> parameters,
    RuntimeParentBodyConstruction& context); // Xbox 0x00024C50
}
