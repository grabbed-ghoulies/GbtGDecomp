#include "resource_requests.h"

#include <cstring>

namespace gbtg
{
SoundResourceRequests g_sound_resource_requests;

// Xbox 0x00038720
void request_sound_resource(const char* name, u32 reason)
{
    size_t index = 0;
    auto* slot = g_sound_resource_requests.names.data();
    do
    {
        if ((*slot)[0] == '\0')
        {
            std::strcpy(slot->data(), name);
            g_sound_resource_requests.states[index] = 1;
            g_sound_resource_requests.reasons[index] = reason;
            return;
        }
        ++slot;
        ++index;
    }
    while (slot < g_sound_resource_requests.names.data() + 6);
}
}
