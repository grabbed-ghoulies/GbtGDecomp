#include "cutscene_impulse.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00118840
void begin_cutscene_camera_impulse(CutsceneCameraImpulse& impulse, u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    impulse.amount = (std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f) * impulse.scale;
    impulse.retained = 1.f;
    impulse.progress = 1.f;
    if (1.f < impulse.amount)
    {
        impulse.amount = 1.f;
    }
    else if (impulse.amount < std::bit_cast<float>(0x3D888889u))
    {
        impulse.amount = std::bit_cast<float>(0x3D888889u); // DAT_0032B128
    }
    impulse.active = 1;
}
}
