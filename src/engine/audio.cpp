#include "gbtg/engine_internal.hpp"

namespace gbtg
{

void audio_tick(Game& g, float dt)
{
    // Xbox 0x0015B780 audio(dt).
    // Xbox 0x0003A040 listener.
    // Xbox 0x000396F0 / 0x00038640 / 0x00038C20 XACT.
    (void)g;
    (void)dt;
}

} // namespace gbtg
