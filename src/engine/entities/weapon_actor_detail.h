#pragma once

#include "weapon_actor.h"
#include "gbtg/anim.h"

#include <bit>
#include <cstdio>
#include <cstring>
#include <string>

// Shared helpers of the weapon modules (weapon_actor.cpp, weapon_motion.cpp, weapon_breakage.cpp).
namespace gbtg::weapon_detail
{
inline constexpr u32 kAidPrefix = 0x5F646961; // "aid_".

[[noreturn]] inline void untranslated(u32 address, const char* from)
{
    char text[16];
    std::snprintf(text, sizeof(text), "0x%08X", address);
    throw Error(std::string("untranslated Xbox ") + text + " reached from " + from);
}

inline void store8(std::span<u8> bytes, size_t offset, u8 value)
{
    bytes[offset] = value;
}

inline void store16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

inline void store32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

inline float next_unit(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
}

// The base update's frame step: zero while DAT_004312DC holds, else DAT_005096C0 * DAT_004312C4.
inline float frame_step(const ActorFrameContext& frame)
{
    if (frame.freeze <= 0.f)
    {
        return frame.dt * frame.scene_scale;
    }
    return 0.f;
}

// Actor +0x92C unless +0x930 holds the animation.
inline float frame_rate(const Actor& actor)
{
    if (actor.animation_pause <= 0.f)
    {
        return actor.animation_rate;
    }
    return 0.f;
}

// Actor +0x918 names the holder's link when the weapon is carried; the holder is that link minus 0x918.
inline Actor* weapon_holder(const Actor& actor)
{
    if (actor.link != nullptr && actor.link->head != nullptr &&
        static_cast<const ActorLink*>(actor.link->head)->head != nullptr)
    {
        return actor.link->owner;
    }
    return nullptr;
}

// The model's pose for shape records: +0x168, else +0x164.
inline const ModelPoseBuffer* weapon_shape_pose(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return nullptr;
    }
    const auto* pose = actor.model->runtime->alternate;
    if (pose == nullptr)
    {
        pose = actor.model->runtime->primary;
    }
    return pose;
}

// The 0x34-byte locator record as 0x00051CE0 reads it.
inline ModelBoneBinding locator_binding(const u8* record)
{
    ModelBoneBinding binding;
    std::memcpy(&binding.kind, record, 2);
    std::memcpy(&binding.joint, record + 2, 2);
    std::memcpy(binding.matrix.data(), record + 4, 48);
    return binding;
}

// The model's animation header (model CPU +0x10 -> +0x10) has a nonzero group count at +0.
inline bool weapon_has_pieces(const Actor& actor)
{
    if (actor.model == nullptr || actor.model->cpu.data() == nullptr)
    {
        return false;
    }
    const auto cpu = actor.model->cpu;
    const u32 header = read_u32(cpu, read_u32(cpu, 0x10) + 0x10);
    return header != 0 && i32(cpu[header]) > 0;
}

// The class's vtable slots, by factory type.
void update_state(RuntimeWeaponActor& weapon, u16 state); // Vtable +0x44.
void late_update_state(RuntimeWeaponActor& weapon, u16 state); // Vtable +0x48.
void leave_state(RuntimeWeaponActor& weapon, u16 state, u16 next); // Vtable +0x40.
void enter_state(RuntimeWeaponActor& weapon, u16 state, u16 previous); // Vtable +0x3C.
RuntimeBodyContactCallback contact_test(RuntimeWeaponActor& weapon); // Actor +0x99C.
}
