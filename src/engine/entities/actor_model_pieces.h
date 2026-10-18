#pragma once

#include "actor_matrix_sampling.h"
#include "actor_matrix_setup.h"
#include "gbtg/entity.h"

#include <vector>

namespace gbtg
{
// Actor +0x2A4: one 0xB0-byte row per piece of the model's animation header (model CPU +0x10 -> +0x10, count at
// its +1, point lists at its +8). Each row keeps the piece's spin (+0xC), its turn (+0x18..+0x20), its radius (+0x48),
// its half extent (+0xAC) and its centre (+0x90). The heap block is the 0x000F72D0 boundary.
void build_actor_model_pieces(const Actor& actor, std::vector<u8>& pieces, u32& random); // Xbox 0x0003F170
// Sets each piece of one header group flying: its matrix copied from the matrix array (+0x4C), its sampled
// position (+0x30) and euler (+0x3C), its extent (+0xAC) shrunk by its offset from the model's root bone, and a
// random spin (+0, +4, +8).
void launch_actor_model_piece_group(Actor& actor, i32 group, std::vector<u8>& pieces, const ActorMatrixSetup& matrices,
    RuntimeActorMatrixSampling& sampling, const std::function<std::span<const u8>(u32)>& resolve,
    u32& random); // Xbox 0x0003F820, EAX group
}
