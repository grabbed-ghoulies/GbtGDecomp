#pragma once
#include "player_attachment.h"
#include "player_attributes.h"
#include "gbtg/actor_runtime.h"

namespace gbtg
{
void runtime_player_storage_sizes(std::span<u32, 2> sizes); // Xbox 0x00027F40
struct RuntimePlayerConstruction
{
    Actor& actor;
    RuntimePlayerAttributes& attributes;
    RuntimePlayerSpawnController& spawn;
    FactoryObject& body; // Actor +0x95C.
    FactoryObject& mind; // Actor +0x958.
    FactoryObject& strategy; // Actor +0x954.
    RuntimePlayerStrategySetup& strategy_setup;
    std::span<u8>& input; // Actor +0x960 native allocation retained until teardown.
    const RuntimeInputReleaseContext& input_release;
    const u8* tag_rows; // DAT_00545AC4, stride 0x87.
    const char* const* tag_names; // DAT_0042A854.
    RuntimePlayerAttachmentCreation& attachments;
    FactoryObject& created_attachment; // Retained factory ownership of this constructor's result.
    u32 initial_channel_word; // Actual DAT_00338EE0 word.
    u32& player_count; // DAT_00510578.
    const u32& story_flags; // DAT_0050EA98.
    RuntimeActorMatrixState& matrices;
    const RuntimeShaderMatrixBoundaries& matrix_boundaries;
};
bool initialize_runtime_player(std::span<u8> actor, std::span<const u8> parameters,
    RuntimePlayerConstruction& context); // Xbox 0x00028830
}
