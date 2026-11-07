#pragma once

#include "actor_frame_render.h"
#include "actor_frame_update.h"
#include "../graphics/billboard_queue.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/model_runtime.h"
#include "gbtg/object_factory.h"
#include "gbtg/resource_error.h"
#include "gbtg/retail_field.h"

#include <array>
#include <cstdint>
#include <span>
#include <type_traits>
#include <functional>
#include <unordered_map>

namespace gbtg
{
// fxCustom (factory type 0xC, getter 0x000EC650, vtable 0x005068F4) over the base object (type 1): an effect that
// rides an actor. Its constructor (0x000EC6A0) appends it to the actor's +0x1D8 frame list (its node at +8), so the
// actor's frame update (0x000420D0) runs its vtable +0x10 and the actor's draw (0x0003DE00) its +0x14. +8 / +0xC are
// the node's previous and next, kept on the host.
namespace fx_custom_parameter
{
constexpr RetailField<u32, 0x08> actor{};
constexpr u32 kSize = 0xC;
static_assert(retail_fits(kSize, actor));
} // namespace fx_custom_parameter
struct FxCustomBase
{
    u32 vtable = 0; // +0, the type's vtable address
    u16 type = 0; // +4, the factory type
    u16 unk_6 = 0; // +6
    std::array<u32, 2> node{}; // +8, the frame list node's previous and next, kept on the host
    float elapsed = 0.f; // +0x10, seconds alive
    u32 actor = 0; // +0x14, the actor the effect rides
};
static_assert(sizeof(FxCustomBase) == 0x18);

// An effect's bytes viewed as its struct.
template <typename State>
State& fx_custom_view(std::span<u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<State*>(bytes.data());
}

template <typename State>
const State& fx_custom_view(std::span<const u8> bytes)
{
    static_assert(std::is_trivially_copyable_v<State>);
    detail::check_retail_range(bytes.size(), 0, sizeof(State));
    return *reinterpret_cast<const State*>(bytes.data());
}

// entityFxCustomElectric (factory type 0x2E, getter 0x00076590, vtable 0x005053E0) over fxCustom: crackling sparks
// on the actor's contact shapes (model block 4, mask 0x20002, or 0x80 when a shape carries it), each a billboard of
// a random frame of the parameters' texture that flickers, a kind-2 light (DAT_00509B7C) that follows the actor, and
// a sound in actor sound slot 4 (the haunted TV's attack). The texture, the {shape, frame} rows and the light record
// live on the host; the fields say whether each exists.
namespace fx_electric_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> sound{};
constexpr RetailField<std::array<char, 0x80>, 0x8C> texture{}; // loaded when it starts "aid_"
constexpr u32 kSize = 0x10C;
static_assert(retail_fits(kSize, texture));
} // namespace fx_electric_parameter
struct FxElectricState
{
    FxCustomBase base{}; // +0
    std::array<char, 0x80> sound{}; // +0x18, empty plays none
    u32 texture_loaded = 0; // +0x98
    u32 spark_count = 0; // +0x9C
    u32 sparks = 0; // +0xA0, nonzero while the rows exist
    std::int8_t light_slot = 0; // +0xA4, -1 for none
    std::array<u8, 3> unk_A5{}; // +0xA5, padding
};
static_assert(sizeof(FxElectricState) == 0xA8);

// entityFxCustomSolidFadeIn (factory type 0x32, getter 0x00077DB0, vtable 0x00505440) over fxCustom: the actor's
// tint (+0x198 colour and the +0x19C..+0x19E levels) fades in, holds (or until message 5), fades out, then either
// waits and starts again or releases itself (0x0003DE30). Message 3 restarts the hold. Its +0x14 slot is the empty
// 0x001628B0 (the generate ghouly state's release effect).
namespace fx_solid_fade_in_parameter
{
constexpr RetailField<u32, 0x0C> colour{};
constexpr RetailField<float, 0x10> strength{};
constexpr RetailField<float, 0x14> fade_in_time{};
constexpr RetailField<float, 0x18> hold_time{};
constexpr RetailField<float, 0x1C> fade_out_time{};
constexpr RetailField<float, 0x20> wait_time{};
constexpr u32 kSize = 0x24;
static_assert(retail_fits(kSize, wait_time));
} // namespace fx_solid_fade_in_parameter
struct FxSolidFadeInState
{
    FxCustomBase base{}; // +0
    u32 phase = 0; // +0x18, 0 in, 1 hold, 2 out, 3 wait
    float time_left = 0.f; // +0x1C, in the phase
    float level = 0.f; // +0x20
    float fade_in_time = 0.f; // +0x24
    float hold_time = 0.f; // +0x28
    float fade_out_time = 0.f; // +0x2C
    float wait_time = 0.f; // +0x30, zero or less: the effect releases itself after the fade-out
    u32 colour = 0; // +0x34
    float strength = 0.f; // +0x38
    std::array<u8, 0x14> unk_3C{}; // +0x3C, never read or written by translated code
    u32 hold_ended = 0; // +0x50, message 5 ends the hold
};
static_assert(sizeof(FxSolidFadeInState) == 0x54);

// entityFxCustomFadeToGrey (type 0x30) and entityFxCustomFadeOut (type 0x33) share a level that moves toward its
// target at a speed (a second's worth); message 0xAC {level, seconds} sets a new target.
struct FxFadeState
{
    FxCustomBase base{}; // +0
    float level = 0.f; // +0x18
    float target = 0.f; // +0x1C
    float speed = 0.f; // +0x20
    union // +0x24
    {
        u32 fade_to_grey_texture; // the fade-to-grey's texture address
        u32 fade_out_activates_binds = 0; // the fade-out's switch to activate the actor's bind tree at full level
    };
};
static_assert(sizeof(FxFadeState) == 0x28);
namespace fx_fade_parameter
{
constexpr RetailField<float, 0x0C> seconds{}; // zero or less: the level jumps to 1
constexpr RetailField<std::array<char, 0x80>, 0x10> fade_to_grey_texture{}; // loaded when it starts "aid_"
constexpr RetailField<u32, 0x10> fade_out_activates_binds{};
constexpr u32 kFadeToGreySize = 0x90;
constexpr u32 kFadeOutSize = 0x14;
static_assert(retail_fits(kFadeToGreySize, fade_to_grey_texture) &&
              retail_fits(kFadeOutSize, fade_out_activates_binds));
} // namespace fx_fade_parameter

// entityFxCustomFadeOut (type 0x33, getter 0x00076BE0, vtable 0x005053F8) over fxCustom: the actor's alpha (+0x19B)
// falls as the level rises toward its target. At full level it releases itself, first activating the actor's bind
// tree when its switch is set.
struct RuntimeFxCustomContext
{
    std::function<Actor*(u32)> actor_at; // The parameters' +8 actor.
    std::function<ActorFrameState&(Actor&)> frame_of; // That actor's frame list (+0x1D8).
    std::function<std::span<const u8>(const Actor&)> actor_bytes; // 0x0003D860's view of the actor.
    std::unordered_map<ActorFrameAttachment*, const u8*>& frame_objects; // The list nodes' objects.
    std::unordered_map<ActorFrameAttachment*, ActorFrameDrawCallback>& draws; // The list nodes' vtable +0x14.
    AnimationResourceRegistry& registry;
    ResourceErrorState& error; // 0x00032700.
    RuntimeActorSoundQueues& sounds;
    RuntimeShaderRegistrationState& lights; // DAT_00509B7C / DAT_00509B78.
    u32& random; // DAT_0054DD7C.
    const float& pause; // DAT_004312DC.
    const float& frame_delta; // DAT_005096C0.
    const float& time_scale; // DAT_004312C4.
    std::function<BillboardFlushContext()> billboards; // The sprite pass 0x0005C410 .. 0x0005C8C0 draws through.
    std::function<RuntimeActorStartupState&(Actor&)> startup_of; // The actor's +0x198 colour and +0x19C levels.
    std::function<std::span<u8>(Actor&)> object_bytes; // The actor's own bytes (+0x1A0..+0x1A8).
    std::function<u32(const u8*)> address_of; // An effect's retail address.
    std::function<u32(const u8*)> resource_address; // A loaded resource's retail address.
    std::function<bool(Actor&, u32)> release_frame_object; // 0x0003DE30, ECX actor, ESI object.
};
void fx_custom_object_sizes(ObjectSizes& output); // Xbox 0x00095530
void fx_custom_electric_object_sizes(ObjectSizes& output); // Xbox 0x00076570
void fx_custom_solid_fade_in_object_sizes(ObjectSizes& output); // Xbox 0x00077D90
// entityFxCustomFadeToGrey (type 0x30, getter 0x00076E80, vtable 0x00505410) over fxCustom: the actor fades to grey
// (its draw hooks +0x1A0 / +0x1A4 are 0x000770F0 / 0x000771F0 with the effect at +0x1A8), `FxFadeState`.
void fx_custom_fade_to_grey_object_sizes(ObjectSizes& output); // Xbox 0x00076E60
void message_fx_custom_fade_to_grey(std::span<u8> effect, std::span<const u32> message); // Xbox 0x00076DF0
// entityFxCustomOutline (type 0x31, getter 0x00077300, vtable 0x00505428): a candle hit's burning outline, a
// billboard on each chosen shape that fades in, holds and fades out. The texture, rows and light record live on the
// host; the fields say whether each exists.
namespace fx_outline_parameter
{
constexpr RetailField<std::array<char, 0x80>, 0x0C> sound{};
constexpr RetailField<std::array<char, 0x80>, 0x8C> texture{}; // loaded when it starts "aid_"
constexpr RetailField<float, 0x10C> fade_in_time{};
constexpr RetailField<float, 0x110> hold_time{};
constexpr RetailField<float, 0x114> fade_out_time{};
constexpr RetailField<float, 0x118> scale{};
constexpr RetailField<u32, 0x11C> colour{};
constexpr RetailField<i16, 0x120> shape_group{};
constexpr RetailField<u32, 0x124> has_light{};
constexpr RetailField<u16, 0x128> light_flags{};
// Copied into the light record's +0x78, +0x80 and +0x84 words.
constexpr RetailField<u32, 0x12A> light_78{};
constexpr RetailField<u32, 0x12E> light_80{};
constexpr RetailField<u32, 0x132> light_84{};
constexpr u32 kSize = 0x138;
static_assert(retail_fits(kSize, light_84));
} // namespace fx_outline_parameter
struct FxOutlineState
{
    FxCustomBase base{}; // +0
    u32 phase = 0; // +0x18, 1 fading in, 2 holding, 3 fading out
    u32 hold_ended = 0; // +0x1C, message 5 ends the hold
    float time_left = 0.f; // +0x20, in the phase
    float hold_time = 0.f; // +0x24
    float fade_in_time = 0.f; // +0x28
    float fade_out_time = 0.f; // +0x2C
    std::array<char, 0x80> sound{}; // +0x30, empty plays none
    u32 texture_loaded = 0; // +0xB0
    float scale = 0.f; // +0xB4, of each shape's radius
    u32 colour = 0; // +0xB8
    float level = 0.f; // +0xBC, 0.01 .. 1 as it fades
    i16 shape_group = 0; // +0xC0, only shapes of this group, or -1 for any
    std::array<u8, 2> unk_C2{}; // +0xC2, padding
    u32 row_count = 0; // +0xC4
    u32 rows = 0; // +0xC8, nonzero while the rows exist
    std::int8_t light_slot = 0; // +0xCC, -1 for none
    std::array<u8, 3> unk_CD{}; // +0xCD, padding
};
static_assert(sizeof(FxOutlineState) == 0xD0);
void fx_custom_outline_object_sizes(ObjectSizes& output); // Xbox 0x000772E0
// Message 3 holds the outline at full level again, 5 lets its hold end.
void message_fx_custom_outline(std::span<u8> effect, std::span<const u32> message); // Xbox 0x00077A80
// Message 3 restarts the hold, 5 ends it.
void message_fx_custom_solid_fade_in(std::span<u8> effect, std::span<const u32> message); // Xbox 0x000780C0
// A light record (0x98 bytes, 0x00051BC0) of the kind at the position in the first free DAT_00509B7C slot.
std::int8_t allocate_runtime_light(u8 kind, const Vec3& position, u8* record,
    RuntimeShaderRegistrationState& lights); // Xbox 0x000FE080, ESI position
void move_runtime_light(std::int8_t slot, const Vec3& position,
    RuntimeShaderRegistrationState& lights); // Xbox 0x000FE0F0, AL slot, ECX position
// Every mask a shape list's records carry, ORed.
u32 runtime_shape_list_mask(const u8* list); // Xbox 0x00056C80, EDX the list
// Types 0xC (base 1), 0x2E, 0x30, 0x31, 0x32 and 0x33 (base 0xC) with their lifecycles.
void install_runtime_fx_custom_classes(std::span<ObjectFactoryType> types, RuntimeFxCustomContext& context);
}
