#pragma once

#include "dialogue_draw_context.h"
#include "../entities/ghouly_spawn_record.h"

namespace gbtg
{
// Boot-time text globals 0x00060320 fills: the null-character texture, both language fonts, the draw
// defaults and the dialogue text-pool registry, then the two counted tables 0x0005F210 publishes.
struct RuntimeDialogueTextTable
{
    i32 count; // DAT_005043BC / DAT_005043C4.
    const u8* rows; // DAT_005043C0 / DAT_005043C8, stride 0xC, ends at a zero word.
};
struct RuntimeDialogueTextSystem
{
    std::span<const u8>& fallback_texture; // DAT_004E1C58.
    i32& paragraph_disabled; // DAT_0050444C.
    std::array<std::span<const u8>, 2>& fonts; // DAT_00504444.
    RuntimeDialogueDrawState& draw; // DAT_005043D8..
    GhoulySpawnPool*& registry; // DAT_005043CC.
    i32& registry_ready; // DAT_005043D0.
    std::array<RuntimeDialogueTextTable, 2>& tables; // DAT_005043BC.
    std::array<const u8*, 2> table_sources; // PTR_DAT_00425160.
};
void count_runtime_dialogue_tables(RuntimeDialogueTextSystem& system); // Xbox 0x0005F210
void initialize_runtime_dialogue_text(u32 language, RuntimeDialogueTextSystem& system,
    AnimationResourceRegistry& registry, ResourceErrorState& error); // Xbox 0x00060320
}
