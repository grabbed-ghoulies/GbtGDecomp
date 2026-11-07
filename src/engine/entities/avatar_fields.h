#pragma once
#include "gbtg/retail_field.h"
#include "actor_fields.h"

#include <array>

namespace gbtg
{
// entityAvatarFxMotionBlur's parameters (type 15, 0x4B8 bytes) after the actor's 0x334. See docs/RETAIL_LAYOUTS.md.
namespace avatar_motion_blur_parameter
{
constexpr RetailField<u32, 0x334> source{}; // the actor it copies
constexpr RetailField<std::array<char, 0x80>, 0x338> resource{}; // an AID: the glow's texture
constexpr RetailField<std::array<char, 0x80>, 0x3B8> effect{};
constexpr RetailField<std::array<char, 0x80>, 0x438> ignite_effect{};
constexpr u32 kSize = 0x4B8;
static_assert(retail_fits(kSize, ignite_effect));
} // namespace avatar_motion_blur_parameter

// entityAvatarFxMotionBlur's own part (0x934..0xA50): the fading copy of a dying actor, in phases.
struct AvatarMotionBlurState
{
    ActorBase base{}; // +0
    float height_scale = 0.f; // +0x934, the copy's +0x34 at the start
    float timer = 0.f; // +0x938, the phase's time left
    float fade = 0.f; // +0x93C
    i32 light = 0; // +0x940, the scene point light's slot, or -1
    u32 phase = 0; // +0x944
    u32 pixel_shader = 0; // +0x948, the glow's: 7, or 6 when the source's +0x196 is set
    u32 resource = 0; // +0x94C, parameter +0x338's resource, by address
    std::array<char, 0x80> effect{}; // +0x950
    std::array<char, 0x80> ignite_effect{}; // +0x9D0, started when phase 0 ends
};
static_assert(sizeof(AvatarMotionBlurState) == 0xA50);

// The after-image's parameters (type 16, 0x354 bytes) after the actor's 0x334.
namespace avatar_afterimage_parameter
{
constexpr RetailField<u32, 0x334> source{}; // the actor it copies
constexpr RetailField<float, 0x338> life{};
constexpr RetailField<u8, 0x33C> alpha{};
constexpr RetailField<Vec3, 0x340> end_scale{}; // the copy's +0x30 at the end
constexpr RetailField<u32, 0x34C> glow{};
constexpr RetailField<u32, 0x350> texture_record{}; // a shader texture record: shader, parameter, texture AID
constexpr u32 kSize = 0x354;
static_assert(retail_fits(kSize, texture_record));
} // namespace avatar_afterimage_parameter

// The after-image's own part (0x934..0x968): a copy of an actor's look that fades and rescales over its life.
struct AvatarAfterimageState
{
    ActorBase base{}; // +0
    float time_left = 0.f; // +0x934
    float life = 0.f; // +0x938
    u32 alpha = 0; // +0x93C, its low byte is the model's alpha
    i32 start_alpha = 0; // +0x940
    Vec3 start_scale{}; // +0x944, the copy's +0x30 at the start
    Vec3 end_scale{}; // +0x950
    u32 has_texture_parameter = 0; // +0x95C
    u32 texture = 0; // +0x960, by address
    u32 glow = 0; // +0x964, drawn through the glow capture
};
static_assert(sizeof(AvatarAfterimageState) == 0x968);
} // namespace gbtg
