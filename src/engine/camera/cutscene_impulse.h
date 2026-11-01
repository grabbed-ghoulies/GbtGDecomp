#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// The ambient lightning flash's words (0x00117930, ticked by 0x00117A50), which a cutscene event starts through
// 0x00118840: views of the scene ambient's own state.
struct CutsceneCameraImpulse
{
    float& progress; // DAT_0050E7BC, the flash's blend
    float& retained; // DAT_0050E7C0, its fade
    float& amount; // DAT_0050E7C8, the level it goes to
    float& scale; // DAT_0050E7CC, the rate the level is drawn from
    i32& active; // DAT_0050E7D0, the gate
};
void begin_cutscene_camera_impulse(CutsceneCameraImpulse& impulse,
    u32& random); // Xbox 0x00118840
}
