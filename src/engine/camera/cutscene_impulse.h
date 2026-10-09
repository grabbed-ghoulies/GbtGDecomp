#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct CutsceneCameraImpulse
{
    float progress; // DAT_0050E7BC
    float retained; // DAT_0050E7C0
    float amount; // DAT_0050E7C8
    float scale; // DAT_0050E7CC
    u32 active; // DAT_0050E7D0
};
void begin_cutscene_camera_impulse(CutsceneCameraImpulse& impulse,
    u32& random); // Xbox 0x00118840
}
