#pragma once
#include "gbtg/entity.h"

namespace gbtg
{
struct RuntimeMindEscapePosition;
u32 select_runtime_cinema_mind_escape_request(std::span<u8> mind,
    std::span<const u8> request, RuntimeMindEscapePosition& context); // Xbox 0x0007B110
u32 face_runtime_cinema_mind_target(std::span<const u8> mind, std::span<u8> body,
    const Actor& actor, std::span<const u8> target); // Xbox 0x0007B800
u32 initialize_runtime_cinema_mind_request(Actor& actor, std::span<const u8> target,
    std::span<u8> request, u32& random); // Xbox 0x0007C580
}
