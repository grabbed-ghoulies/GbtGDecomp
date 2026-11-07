#pragma once
#include "body_state_fields.h"

#include "actor_clock.h"
#include "actor_live_attachments.h"
#include "body_action_selection.h"
#include "body_timer_requests.h"
#include "ghouly_spawn_record.h"
#include "ghouly_spawn_scheduler.h"
#include "../graphics/billboard_queue.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/model_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"
#include "gbtg/sound_runtime.h"

#include <functional>

namespace gbtg
{
struct RuntimeDisplaySetupContext;
// Factory type 0x6F, entityStateGenerateGhouly (getter 0x00093B60, vtable 0x005058C0) over the body state (type 9):
// the haunted TV's summoning. Phase 1 plays the intro animation and sound while two bolts crackle from the TV's two
// kind-2 shape records; when it ends a beam actor (the parameters' +0x118 model) and a light stand at the spawn point
// in front of the TV. Phase 2 sends the bolts down to the beam for half a second while the ghouly the actor's box
// (+0xBA0) holds grows out of it; phase 3 lets the beam fade and plays the end animation. The +0x14 slot is the empty
// 0x001628B0, +0x18 (0x00094130) draws the bolts in the kind-2 pass and +0x1C is 0x0002B620. State +8 is the body.
namespace generate_ghouly_parameter
{
constexpr RetailField<u32, 0x0C> intro_animation{}; // XBE name addresses: phase 1, 2 and 3
constexpr RetailField<u32, 0x10> release_animation{};
constexpr RetailField<u32, 0x14> end_animation{};
constexpr RetailField<std::array<char, 0x80>, 0x18> release_sound{}; // actor sound slot 2 at phase 2
constexpr RetailField<std::array<char, 0x80>, 0x98> intro_sound{}; // actor sound slot 4 at phase 1
constexpr RetailField<std::array<char, 0x80>, 0x118> beam_model{};
constexpr RetailField<std::array<char, 0x80>, 0x198> release_effect{}; // 0x0003DF40 on the ghouly and its attachment
constexpr RetailField<std::array<char, 0x80>, 0x218> bolt_texture{};
constexpr RetailField<std::array<char, 0x80>, 0x298> spark_texture{};
constexpr u32 kSize = 0x318;
static_assert(retail_fits(kSize, spark_texture));
} // namespace generate_ghouly_parameter
// A bolt row. The pool and the shape record live on the host, so their words are 1 while set.
struct GhoulyBoltRow
{
    u32 pool = 0; // +0
    u32 shape = 0; // +4
    float angle = 0.f; // +8, off the actor's heading (UNK_003028C8)
};
static_assert(sizeof(GhoulyBoltRow) == 0xC);
struct GenerateGhoulyState
{
    BodyStateBase base{}; // +0
    u32 phase = 0; // +0x18, 1 intro, 2 release, 3 end
    float grow = 0.f; // +0x1C, phase 2's seconds left (DAT_0032B0F4)
    u32 intro_animation = 0; // +0x20, set when 0x0003AAA0 found the record
    u32 release_animation = 0; // +0x24
    u32 end_animation = 0; // +0x28
    std::array<char, 0x80> release_sound{}; // +0x2C
    std::array<char, 0x80> intro_sound{}; // +0xAC, cleared when slot 4 already plays a voice
    std::array<char, 0x80> beam_model{}; // +0x12C
    std::array<char, 0x80> release_effect{}; // +0x1AC
    std::array<char, 0x80> bolt_texture_name{}; // +0x22C
    std::array<char, 0x80> spark_texture_name{}; // +0x2AC
    std::array<GhoulyBoltRow, 2> bolt_rows{}; // +0x32C
    u32 bolts = 0; // +0x344, set while the bolts have points to draw
    u32 bolt_texture = 0; // +0x348, host texture views, 1 while set
    u32 spark_texture = 0; // +0x34C
    std::array<char, 0x80> ghouly_name{}; // +0x350, 0x000286B0 from actor +0xBA0
    std::array<char, 0x80> ghouly_variant{}; // +0x3D0, and +0xC20
    i32 ghouly_kind = 0; // +0x450, actor +0xB9C: 0 a template clone, 1 or 2 0x00112C50
    u32 ghouly = 0; // +0x454
    u32 attached = 0; // +0x458, the ghouly's body +0x48 actor
    u32 released = 0; // +0x45C
    u32 goals_drained = 0; // +0x460
    Vec3 ghouly_scale{}; // +0x464
    Vec3 attached_scale{}; // +0x470, retail stores the ghouly's scale here too
    u32 beam = 0; // +0x47C
    Vec3 spawn_point{}; // +0x480
    float beam_radius = 0.f; // +0x48C
    float beam_height = 0.f; // +0x490
    u8 light = 0; // +0x494, DAT_00509B7C slot, 0xFF for none
    std::array<u8, 3> unk_495{}; // +0x495, padding
    u32 beam_on = 0; // +0x498
    u32 spawn_found = 0; // +0x49C
};
static_assert(sizeof(GenerateGhoulyState) == 0x4A0);
// Scene views every generator shares.
struct RuntimeGenerateGhoulyScene
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    RuntimeShaderRegistrationState& lights; // DAT_00509B7C / DAT_00509B78.
    RuntimeActorLink& live_actors; // DAT_00511B08.
    std::function<GhoulySpawnContext&()> spawns; // 0x000F40A0's views.
    std::function<RuntimeActorLink&(u32 actor)> live_link; // A scene actor's +0x900.
    std::function<u32(u32 actor)> attached_of; // A player actor's body (+0x95C) +0x48, or 0.
    std::function<void(u32 actor)> drain_goals; // 0x00028210 over a player actor's +0x980 ring.
    std::function<u32(FactoryObject)> keep; // The scene keeps a created object; its address.
    std::function<void(u32 object)> release; // 0x001075C0.
    std::function<u32(const char* name, u32 actor)> create_effect; // 0x0003DF40.
    std::function<void(u32 actor, float duration, const float* tint)> fade; // 0x0013AF80, EAX actor, EDI tint.
    std::function<void(u32 actor)> retire; // 0x00112CC0, ESI actor.
    std::function<RuntimeDisplaySetupContext&()> display;
    std::function<BillboardFlushContext()> billboards;
};
struct RuntimeGenerateGhoulyContext
{
    RuntimeGenerateGhoulyScene& scene;
    std::span<u8> body;
    Actor& actor; // Body +8.
    std::span<u8> actor_bytes;
    ActorAnimationState& animation; // Actor +0x50.
    RuntimeBodyActionSelection& actions;
    const RuntimeActorClock& clock;
    entity_detail::ListHead& live_attachments; // Actor +0x8F4.
    RuntimeBodyHoldTimerContext& hold; // 0x00027BE0.
    RuntimeActorSoundQueues& sounds;
    u32& random; // DAT_0054DD7C.
    float occupancy_initial;
    std::span<ExecutableResourcePool> resources;
    ObjectFactoryRegistry& factory;
    const Vec3& default_position; // DAT_0050DD80.
    const Vec3& default_euler; // DAT_0050DD74.
    GhoulySpawnRecord* spawn_record; // Actor +0xB98.
    const char* default_name; // DAT_00546208, 0x000411A0's names.
    std::function<const char*(u32 address)> image_string; // The parameters' XBE name addresses; null for 0.
    std::function<Actor*(u32 address)> actor_at;
    std::function<void(u32 actor, std::span<const u32>)> message; // Actor vtable +0xC.
    std::function<u16(u32 slot)> body_slot_key; // Body vtable +0x20.
};
void generate_ghouly_state_object_sizes(ObjectSizes& output); // Xbox 0x00093B40
bool initialize_runtime_generate_ghouly_state(GenerateGhoulyState& state, std::span<const u8> parameters,
    RuntimeGenerateGhoulyContext& context); // Xbox 0x00093BC0
void destroy_runtime_generate_ghouly_state(GenerateGhoulyState& state,
    RuntimeGenerateGhoulyContext& context); // Xbox 0x00093DF0
void tick_runtime_generate_ghouly_state(GenerateGhoulyState& state,
    RuntimeGenerateGhoulyContext& context); // Xbox 0x00093EC0
void message_runtime_generate_ghouly_state(GenerateGhoulyState& state, std::span<const u32> message); // Xbox 0x00094150
void draw_runtime_generate_ghouly_state(GenerateGhoulyState& state, i32 kind,
    RuntimeGenerateGhoulyContext& context); // Xbox 0x00094130
// Type 0x6F (base 9) with its lifecycle; `context_of` gives a state's views by its body address (state +8).
void install_runtime_generate_ghouly_state_class(std::span<ObjectFactoryType> types,
    std::function<RuntimeGenerateGhoulyContext&(u32 body_address)> context_of);
}
