#pragma once
#include "gbtg/retail_field.h"

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
namespace avatar_motion_blur_field
{
constexpr RetailField<float, 0x934> height_scale{}; // the copy's +0x34 at the start
constexpr RetailField<float, 0x938> timer{}; // the phase's time left
constexpr RetailField<float, 0x93C> fade{};
constexpr RetailField<i32, 0x940> light{}; // the scene point light's slot, or -1
constexpr RetailField<u32, 0x944> phase{};
constexpr RetailField<u32, 0x948> pixel_shader{}; // the glow's: 7, or 6 when the source's +0x196 is set
constexpr RetailField<u32, 0x94C> resource{}; // parameter +0x338's resource, by address
constexpr RetailField<std::array<char, 0x80>, 0x950> effect{};
constexpr RetailField<std::array<char, 0x80>, 0x9D0> ignite_effect{}; // started when phase 0 ends
constexpr u32 kSize = 0xA50;
static_assert(retail_fits(kSize, ignite_effect));
} // namespace avatar_motion_blur_field

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
namespace avatar_afterimage_field
{
constexpr RetailField<float, 0x934> time_left{};
constexpr RetailField<float, 0x938> life{};
constexpr RetailField<u32, 0x93C> alpha{}; // its low byte is the model's alpha
constexpr RetailField<i32, 0x940> start_alpha{};
constexpr RetailField<Vec3, 0x944> start_scale{}; // the copy's +0x30 at the start
constexpr RetailField<Vec3, 0x950> end_scale{};
constexpr RetailField<u32, 0x95C> has_texture_parameter{};
constexpr RetailField<u32, 0x960> texture{}; // by address
constexpr RetailField<u32, 0x964> glow{}; // drawn through the glow capture
constexpr u32 kSize = 0x968;
static_assert(retail_fits(kSize, glow));
} // namespace avatar_afterimage_field
} // namespace gbtg
