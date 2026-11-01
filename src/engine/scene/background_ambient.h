#pragma once
#include "../camera/cutscene_impulse.h"

#include "gbtg/entity.h"
#include <span>

namespace gbtg
{
struct RuntimeBackgroundAmbient
{
    std::span<Vec3> emitters; // DAT_0050E834, 21 positions.
    i32& count; // DAT_0050E930.
    Vec3& vary_point; // DAT_0050E938.
    i32& vary_enabled; // DAT_0050E944.
    float& vary_scale; // DAT_0050E948.
    std::span<float, 4> light_color; // DAT_00509B3C.
    i32& phase_enabled; // DAT_0050E950.
    Vec3& phase_point; // DAT_0050E958.
    std::span<float, 12> matrix_scratch; // DAT_004DD798.
};
RuntimeBackgroundAmbient runtime_background_ambient();
// The lightning flash words (DAT_0050E7BC..DAT_0050E7D0) a cutscene event starts through 0x00118840.
CutsceneCameraImpulse ambient_flash_state();
void initialize_background_ambient_nodes(const Actor& actor, u32& random,
    RuntimeBackgroundAmbient& state); // Xbox 0x000432B0
}
