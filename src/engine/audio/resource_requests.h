#pragma once

#include "gbtg/types.h"
#include <array>

namespace gbtg
{
struct SoundResourceRequests
{
    std::array<std::array<char, 128>, 6> names{}; // DAT_004DFFA0
    std::array<u32, 6> states{}; // DAT_004E02A0
    std::array<u32, 6> reasons{}; // DAT_004E02B8
};
extern SoundResourceRequests g_sound_resource_requests;
void request_sound_resource(const char* name, u32 reason); // Xbox 0x00038720
}
