#pragma once

#include "gbtg/anim.h"
#include "gbtg/entity.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"

#include <functional>
#include <span>

namespace gbtg
{
// Factory type 0x14, entityAvatarMiscAnimDelete (getter 0x0006C9E0, vtable 0x00504BBC; the factory table's row 0x13
// carries the name) over the avatar (type 2): plays its animation once from the parameters' start position at their
// speed (its sign the direction, its size the duration), then leaves an after-image fading over the parameters' fade
// time and deletes itself. +8 is the empty 0x001628B0, +0xC is 0x00070370 and +0x10 runs after the actor update
// 0x000420D0; +0x14, +0x18 and +0x38 are the avatar's (0x0003C080).
namespace anim_delete_parameter
{
constexpr RetailField<float, 0x334> start{};
constexpr RetailField<float, 0x338> speed{};
constexpr RetailField<float, 0x33C> fade{};
constexpr u32 kSize = 0x340;
static_assert(retail_fits(kSize, fade));
} // namespace anim_delete_parameter
namespace anim_delete_field
{
constexpr RetailField<float, 0x934> fade{};
constexpr u32 kSize = 0x938;
static_assert(retail_fits(kSize, fade));
} // namespace anim_delete_field
struct RuntimeAnimDelete
{
    std::span<u8> bytes;
    Actor& actor;
    ActorAnimationState& animation; // +0x50.
    u32 address;
    ObjectFactoryRegistry& factory;
    std::function<FactoryObject*(FactoryObject)> keep; // Scene ownership of created objects.
    const char* default_name; // DAT_00546208, for 0x000411A0.
};
void anim_delete_object_sizes(ObjectSizes& output); // Xbox 0x0006C9C0
bool initialize_runtime_anim_delete(std::span<const u8> parameters, RuntimeAnimDelete& context); // Xbox 0x0006CA20
// The part after the actor update 0x000420D0.
void update_runtime_anim_delete(RuntimeAnimDelete& context); // Xbox 0x0006CAF0
}
