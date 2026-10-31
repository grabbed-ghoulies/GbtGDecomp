#pragma once
#include "../entities/actor_clock.h"
#include "../graphics/actor_effect_nodes.h"
#include "../graphics/scene_model_draw.h"
#include "gbtg/model_runtime.h"

#include <array>
#include <memory>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
namespace entity_detail
{
struct DisplayView;
}
// One of the scene's three post models (DAT_00512AF0, stride 0x2C): models drawn after the scene at the camera's eye
// (or the origin), such as a sky. Script world action 0x52 loads the first (0x0012AA50).
struct RuntimePostModel
{
    std::unique_ptr<RuntimeModelInstance> model; // +0, 0x000F9840.
    float scale = 0.f; // +4
    Vec3 euler{}; // +8
    Vec3 spin{}; // +0x14, radians a second.
    float spinning = 0.f; // +0x20, nonzero turns the euler by the spin.
    u32 at_origin = 0; // +0x24, drawn at DAT_0050DD80 instead of the eye.
    u32 full_light = 0; // +0x28, drawn with the ambient tint (DAT_00509B20..2C) at one.
};
using RuntimePostModels = std::array<RuntimePostModel, 3>;
struct RuntimePostModelContext
{
    RuntimePostModels& models;
    ActorEffectModelContext& create; // 0x00035E90 / 0x000F9840 / 0x000F8BF0 / 0x00035C00.
    std::function<u32(const void*)> address_of; // A model's cpu address.
    const RuntimeActorClock& clock;
    entity_detail::DisplayView*& view; // DAT_005077A8.
    RuntimeDisplaySetupContext* display; // The draw's views, once the scene has them.
    SceneModelDrawContext* draw;
    std::array<float*, 4> tint; // DAT_00509B20, DAT_00509B24, DAT_00509B28, DAT_00509B2C.
    const Vec3& origin; // DAT_0050DD80.
    float far_clip; // DAT_0032B5C8.
};
// Each post model's model is released.
void clear_runtime_post_models(RuntimePostModelContext& context); // Xbox 0x0012A9B0
// Post model `index` takes the model `name` (an AID, else nothing) at `scale`, not spinning; a missing AID raises
// resource error 2.
void load_runtime_post_model(const char* name, u32 index, float scale,
    RuntimePostModelContext& context); // Xbox 0x0012A9E0, EAX name
// Script world action 0x52: the post models are cleared and the first takes `name` at scale one.
void set_runtime_post_model(const char* name, RuntimePostModelContext& context); // Xbox 0x0012AA50
// While the first post model is set, each spinning one turns its euler by its spin (0x001059B0 wraps it).
void tick_runtime_post_models(RuntimePostModelContext& context); // Xbox 0x0012AA70
// While the first post model is set, each is drawn (0x000F8CF0, kind 3) with the view's far clip at DAT_0032B5C8.
void draw_runtime_post_models(RuntimePostModelContext& context); // Xbox 0x0012AB10
}
