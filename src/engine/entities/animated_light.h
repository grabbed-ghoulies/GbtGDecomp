#pragma once
#include "../graphics/billboard_queue.h"
#include "../scene/scene_point_lights.h"
#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/model_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"

#include <span>

namespace gbtg
{
// entityAvatarMiscAnimatedLight (factory type 0x13, getter 0x0006C260, vtable 0x00504B80) over the actor (type 2): an
// animated prop with a point light (0x00123720) and a glow billboard at its model binding 0 (0x000408D0, through the
// binding's joint and the actor transform). The animation starts at a random point. The message is the actor's
// 0x0003EF70 (0x0006C9B0 jumps to it); the other slots are the actor's (getter 0x0003C080).
namespace animated_light_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x334> glow{}; // the glow sprite's AID
constexpr u32 kSize = 0x3B4;
static_assert(retail_fits(kSize, glow));
} // namespace animated_light_parameter
namespace animated_light_field
{
// +0x934 the binding (host: RuntimeAnimatedLight::binding), +0x93C the glow sprite (host: ::glow).
constexpr RetailField<i32, 0x938> light{}; // its DAT_00511B70 record, -1 for none
constexpr u32 kSize = 0x940;
static_assert(retail_fits(kSize, light));
} // namespace animated_light_field
struct RuntimeAnimatedLight
{
    std::span<u8> bytes;
    Actor& actor;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeScenePointLights& lights;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    u32& random; // DAT_0054DD7C.
    BillboardQueue& billboards;
    const u8* binding = nullptr; // +0x934.
    std::span<const u8> glow; // +0x93C.
};
void animated_light_object_sizes(ObjectSizes& output); // Xbox 0x0006C240
bool initialize_runtime_animated_light(RuntimeAnimatedLight& context,
    std::span<const u8> parameters); // Xbox 0x0006C2B0
void destroy_runtime_animated_light(RuntimeAnimatedLight& context); // Xbox 0x0006C540
// After the actor update 0x000420D0 (which the caller runs first): the light record follows the binding.
void update_runtime_animated_light(RuntimeAnimatedLight& context); // Xbox 0x0006C5A0
// The glow billboard (0x0005C1A0); the caller then draws the actor (0x0003C9E0).
void queue_runtime_animated_light_glow(RuntimeAnimatedLight& context); // Xbox 0x0006C7C0
}
