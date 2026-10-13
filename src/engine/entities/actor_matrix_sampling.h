#pragma once
#include "gbtg/entity.h"
#include <functional>

namespace gbtg
{
struct RuntimeActorMatrixSampling
{
    std::span<u8> records; // Actor +0x288, stride 0x48.
    std::span<const u8> node; // Actor +0x290.
    std::span<float, 12> scratch; // DAT_0050D7A0.
    float*& active; // DAT_0050DD70.
    const Vec3& origin; // DAT_0050DD80.
    const std::function<std::span<const u8>(u32)>& resolve;
};
void sample_actor_model_matrix(Actor& actor, i32 index, Vec3& position, Vec3& euler,
    i32 clamp_height, RuntimeActorMatrixSampling& context); // Xbox 0x00040640
}
