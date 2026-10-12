#include "bank_streams.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00038880
void restart_audio_stream(const char* name, i32 id, AudioBankStreams& context)
{
    for (auto& slot : context.streams)
    {
        if (std::strcmp(name, slot.name.data()) == 0)
        {
            if (slot.state == 1)
            {
                return;
            }
            if (slot.state == 2)
            {
                if (0.24f > slot.time)
                {
                    return;
                }
                release_audio_stream(name, context);
            }
        }
    }
    request_audio_stream(name, id, context);
}
}
