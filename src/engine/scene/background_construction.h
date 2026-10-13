#pragma once
#include "background_animation.h"
#include "background_ambient.h"
#include "background_doors.h"
#include "background_lifetime.h"
#include "background_model_nodes.h"
#include "background_sectors.h"
#include "background_volumes.h"
#include "../entities/actor_matrix_setup.h"

namespace gbtg
{
struct RuntimeBackgroundConstruction
{
    std::span<u8> fields;
    Actor& actor;
    ActorMatrixSetup& matrices;
    RuntimeBackgroundLifetime& lifetime;
    RuntimeBackgroundAnimation& animation;
    std::vector<BackgroundAnimationVolume> volumes;
    std::vector<RuntimePointGraph> graphs;
};
struct RuntimeBackgroundConstructionServices
{
    std::span<char> room; // DAT_00545D40.
    entity_detail::ListHead& backgrounds; // DAT_00511B1C.
    std::span<BackgroundRegion> regions; // Scene region rows.
    i32& region_count; // Scene region count.
    std::span<const Vec3> vertices;
    const std::unordered_map<const u8*, PhysicalShapeEventList>& shape_lists;
    const ActorMatrixBindings& matrix_bindings;
    std::span<const RuntimeModelNodeType> model_types;
    RuntimeBackgroundSectors& sectors;
    entity_detail::ListHead& attachment_owner;
    entity_detail::ListHead& attachment_global;
    entity_detail::ListHead& anchor_owner;
    const Vec3& origin;
    RuntimeBackgroundAnimation*& first_animation; // First linked background, not current actor.
    const std::function<std::span<const u8>(u32)>& animation_source;
    const std::function<u32(u8*, u32)>& bind_allocation;
    const std::function<u32(const void*)>& address_of;
    RuntimeDoorContext& doors;
    RuntimeBackgroundAmbient& ambient;
    u32& random;
    AnimationResourceRegistry& resources;
    ResourceErrorState& error;
    i32& disable_framebuffer_clear; // DAT_004DEA50.
    const BackgroundNodeViews& nodes;
    const RuntimeShaderMatrixBoundaries& matrix_boundaries;
};
void runtime_background_storage_sizes(ObjectSizes& output); // Xbox 0x00042BA0
bool initialize_runtime_background(RuntimeBackgroundConstruction& state,
    std::span<const u8> parameters, RuntimeBackgroundConstructionServices& services); // Xbox 0x00042DA0
}
