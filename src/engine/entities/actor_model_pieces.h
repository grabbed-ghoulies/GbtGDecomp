#pragma once

#include "actor_matrix_sampling.h"
#include "actor_matrix_setup.h"
#include "gbtg/entity.h"

#include <vector>

namespace gbtg
{
// One piece of a breakable model (0xB0 bytes, a row at actor +0x2A4).
struct ActorModelPiece
{
    Vec3 velocity{}; // +0
    Vec3 spin{}; // +0xC, added to the turn each tick, decaying
    Vec3 turn{}; // +0x18
    std::array<u8, 0xC> unk_24{}; // +0x24
    Vec3 position{}; // +0x30
    Vec3 euler{}; // +0x3C, the turn plus the spin
    float radius = 0.f; // +0x48
    std::array<float, 16> matrix{}; // +0x4C, copied from the matrix array when it launches
    u32 active = 0; // +0x8C, flying; 6 seconds, or resting on the floor after 3, end it
    Vec3 center{}; // +0x90
    std::array<u8, 0xC> unk_9C{}; // +0x9C
    float age = 0.f; // +0xA8
    float extent = 0.f; // +0xAC, half its size
};
static_assert(sizeof(ActorModelPiece) == 0xB0);
inline ActorModelPiece& actor_model_piece(std::vector<u8>& pieces, u32 row)
{
    detail::check_retail_range(pieces.size(), size_t(row) * sizeof(ActorModelPiece), sizeof(ActorModelPiece));
    return *reinterpret_cast<ActorModelPiece*>(pieces.data() + size_t(row) * sizeof(ActorModelPiece));
}
inline const ActorModelPiece& actor_model_piece(const std::vector<u8>& pieces, u32 row)
{
    detail::check_retail_range(pieces.size(), size_t(row) * sizeof(ActorModelPiece), sizeof(ActorModelPiece));
    return *reinterpret_cast<const ActorModelPiece*>(pieces.data() + size_t(row) * sizeof(ActorModelPiece));
}

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
