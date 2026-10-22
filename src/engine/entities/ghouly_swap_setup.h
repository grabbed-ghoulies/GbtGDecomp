#pragma once
#include "actor_base_messages.h"
#include "ghouly_requests.h"
#include "ghouly_spawn_record.h"
#include "../scene/scene_marker_handlers.h"
#include "gbtg/retail_field.h"

#include <functional>

namespace gbtg
{
// entityAvatarGhoulySetupSwap, factory type 0x26 (getter 0x0006BB30, vtable 0x00504B44) over the actor (type 2): a
// ghoulybox that swaps a scene actor for its ghouly (scene 8's haunted chairs). Message 0x7D finds the actor it swaps
// (+0x93C: the DAT_00511AA4 object whose +0x1FE is this setup's +0x1FC, else the nearest spawn actor) and an anchor
// by +0x1FE. The update asks for the box (0xA8) once the player is close; the box's 0xA7 request makes the ghouly
// at the swapped actor's pose and retires that actor. Message 0xA6 answers a pose request with the swapped actor's
// pose. The +0x10 slot is 0x0006C0A0 itself (no actor frame update); the draw and visibility slots are the actor's.
namespace ghouly_swap_parameter
{
constexpr RetailField<u32, 0x334> controller_key{}; // set: the setup's ghouly controller is registered under it
constexpr RetailField<std::array<char, 0x80>, 0x338> box{}; // the ghoulybox
constexpr RetailField<std::array<char, 0x80>, 0x3B8> attributes{}; // the spawns' attributes
constexpr RetailField<float, 0x438> box_chance{}; // the chance the setup keeps its box
constexpr RetailField<float, 0x43C> radius{}; // the player radius that asks for the box
constexpr RetailField<float, 0x440> chance{};
constexpr RetailField<u32, 0x444> unk_444{};
constexpr u32 kSize = 0x4C8;
static_assert(retail_fits(kSize, unk_444));
} // namespace ghouly_swap_parameter
namespace ghouly_swap_field
{
constexpr RetailField<u32, 0x934> anchor{}; // the actor message 0x7D looks up by +0x1FE; 0xA7 asks it for the pose
constexpr RetailField<u32, 0x938> load{}; // the ghoulybox load (0x000F39A0); its native view is the context's
constexpr RetailField<u32, 0x93C> source{}; // the actor the ghouly replaces
constexpr RetailField<u32, 0x940> swapped{}; // set once 0xA7 made the ghouly
constexpr RetailField<std::array<char, 0x80>, 0x944> box{}; // empty unless the constructor kept it
constexpr RetailField<std::array<char, 0x80>, 0x9C4> attributes{};
constexpr RetailField<float, 0xA48> radius{};
constexpr RetailField<float, 0xA4C> chance{};
constexpr RetailField<u32, 0xA50> unk_A50{};
constexpr u32 kSize = 0xA54;
static_assert(retail_fits(kSize, unk_A50));
} // namespace ghouly_swap_field
struct RuntimeGhoulySwapSetup
{
    std::span<u8> bytes;
    Actor& actor;
    GhoulyController& controller;
    u32& random; // DAT_0054DD7C.
    GhoulySpawnRecord*& load; // +0x938, native pointer view.
    std::function<u32(const void*)> address_of;
    std::function<void(u32, std::span<u32>)> message; // An actor's actual vtable +0xC by address.
    std::function<void(std::span<u32>)> self_message; // This actor's actual vtable +0xC.
    std::function<Actor*(u32)> actor_at; // A scene actor by its retail address.
};
struct RuntimeGhoulySwapSetupMessages
{
    RuntimeGhoulySwapSetup& setup;
    ActorBaseContext& base;
    RuntimeMarkerObjects& objects;
    std::function<std::span<u8>(u32)> bytes_at;
    std::function<FactoryObject*(FactoryObject)> keep; // Native scene ownership, not a creation callee.
    std::function<void(ActorBaseContext&, std::span<u32>)> base_message; // 0x0003EF70.
    std::function<u32(const entity_detail::LatchNode&)> latch_owner; // The object a DAT_00511AA4 node sits in.
    std::function<u32(const Actor&)> actor_address; // A scene actor's retail address.
    u32& runtime_flags; // DAT_0050EA9C.
    std::function<void(u32)> retire; // 0x00112CC0, ESI the actor.
};
void ghouly_swap_setup_object_sizes(ObjectSizes& sizes); // Xbox 0x0006BB10
bool initialize_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context,
    std::span<const u8> parameters); // Xbox 0x0006BB70
void destroy_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context); // Xbox 0x0006BC70
// The swapped actor's pose (the source), then the anchor's answer to 0xA6 for it, or the source itself without an
// anchor; false when the anchor refuses.
bool query_runtime_ghouly_swap_pose(RuntimeGhoulySwapSetup& context, Vec3& source_position, Vec3& source_euler,
    Vec3& position, Vec3& euler); // Xbox 0x0006BCA0, EBX source euler
void dispatch_runtime_ghouly_swap_setup_message(RuntimeGhoulySwapSetupMessages& context,
    std::span<u32> message); // Xbox 0x0006BDF0
void update_runtime_ghouly_swap_setup(RuntimeGhoulySwapSetup& context); // Xbox 0x0006C0A0
// The first DAT_00511AA4 object whose +0x1FE word is the key (EAX actor's +0x1FC), as its retail address; 0 when
// none matches.
u32 find_runtime_actor_by_link_key(u16 key, const std::function<u32(const entity_detail::LatchNode&)>& owner,
    const std::function<std::span<u8>(u32)>& bytes_at); // Xbox 0x00129F00, EAX actor
}
