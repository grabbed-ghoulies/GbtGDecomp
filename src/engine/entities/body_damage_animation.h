#pragma once
#include "body_action_selection.h"

namespace gbtg
{
struct RuntimeBodyActionMessages;
struct RuntimeDamageAngleSelection
{
    float reference;
    u32 count;
    u32 index;
    float angle;
};
void select_runtime_damage_angle(RuntimeDamageAngleSelection& selection,
    float angle); // Xbox 0x00105C10
struct RuntimeDamageAnimationRow
{
    float angle;
    const char* name;
};
struct RuntimeDamageAnimationRows
{
    i32 count;
    std::span<const RuntimeDamageAnimationRow> rows;
};
const char* runtime_damage_animation_direction(const RuntimeDamageAnimationRows& rows,
    float angle); // Xbox 0x00020B50
const char* runtime_damage_animation_joint(i16 joint); // Xbox 0x00020C10
const AnimationRecord* find_runtime_body_damage_animation(std::span<const u8> body,
    const RuntimeDamageAnimationRows& rows, float angle, i16 joint,
    std::span<char> buffer, RuntimeBodyActionSelection& context); // Xbox 0x00020C80
void select_runtime_body_damage_reaction(std::span<u8> body, u16 key,
    float duration, float pause, float angle, i16 joint, u32 animate,
    std::span<const u8> attributes, const RuntimeDamageAnimationRows& rows,
    std::span<char> buffer, RuntimeBodyActionMessages& context); // Xbox 0x00024970
}
