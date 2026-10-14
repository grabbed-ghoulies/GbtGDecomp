#pragma once
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include "../camera/runtime_display_setup.h"
#include "../entities/entity_types.h"
#include "../graphics/scene_ambient_sampling.h"
#include <functional>

namespace gbtg
{
// The actor fields the shadow pass reads and writes.
struct RuntimeSceneShadowActor
{
    Actor& actor;
    RuntimeModelInstance* model; // +0x204
    RuntimeModelInstance* shadow; // +0x310
    i32 mode; // +0x30C
    std::array<u8, 2>& flags; // +0x330 (direction set), +0x331 (volume flags).
    Vec3& eased; // +0x318
    Vec3& target; // +0x324
    Actor* held; // +0x314
};
struct RuntimeSceneShadowCasters
{
    RuntimeDisplaySetupContext& display;
    u32 view; // (DAT_005077A8 - DAT_00507018) / 0x1E4 on entry.
    std::span<entity_detail::ListHead, 8> bins; // DAT_00511AA8.
    std::function<Actor&(entity_detail::ListHead&)> bin_actor; // Owner of a pass-0 bin record (+0x124 + 0xC * view).
    std::function<RuntimeSceneShadowActor(Actor&)> shadow;
    const Actor* player; // DAT_00511B40 minus 0x93C.
    std::span<const u8> background; // The DAT_00511B20 background's +0x204 resource, empty without one.
    std::span<const SceneAmbientPoint, 32> points; // DAT_00511B70.
    u32 points_enabled; // DAT_005120F0.
    const Vec3& light_offset; // DAT_0050D764.
    i32 paused; // DAT_00509B70.
    i32 ease; // DAT_00509B74.
    u32& random; // DAT_0054DD7C.
    i32& light_count; // DAT_0050D71C.
};
// Normalizes `vector` into `output`; a zero vector gives the unit axis named by `axis` ('x'/'X'..'z'/'Z').
void runtime_direction_or_axis(const Vec3& vector, Vec3& output, u8 axis); // Xbox 0x00106B90
// World positions (record +0x40) and, for kind 2, directions (+0x4C) of the model's positional shader lights.
void place_runtime_model_shader_lights(RuntimeModelInstance& model, const Actor& actor); // Xbox 0x00120FE0
float runtime_actor_view_distance(const Actor& actor, const entity_detail::DisplayView& view); // Xbox 0x00121160
ModelPoseBuffer* runtime_actor_pose(const RuntimeModelInstance* model); // Xbox 0x00040B20
bool runtime_background_light_in_view(const Vec3& position, std::span<const u8> background,
    entity_detail::DisplayView& view); // Xbox 0x000FE4D0
// The nearest light probe whose radius reaches `position` in a background probe table (count, stride 0x28).
std::span<const u8> nearest_runtime_light_probe(const Vec3& position, std::span<const u8> table); // Xbox 0x00052270
i32 nearest_runtime_ambient_point(const Vec3& position, float& distance,
    const RuntimeSceneShadowCasters& context); // Xbox 0x001238F0
void add_runtime_actor_shadow_caster(const RuntimeSceneShadowActor& actor, i32 volume); // Xbox 0x00121E30
void register_runtime_actor_shadow(const RuntimeSceneShadowActor& actor, u32 player,
    RuntimeSceneShadowCasters& context); // Xbox 0x001211E0
// Sets up view 0's display, then each pass-0 bin actor with a model places its shader lights and registers its
// shadow volume.
void update_runtime_scene_shadow_casters(RuntimeSceneShadowCasters& context); // Xbox 0x00120EF0
}
