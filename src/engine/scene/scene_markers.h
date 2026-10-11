#pragma once
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <array>
#include <functional>
#include <span>

namespace gbtg
{
// Scene marker records (DAT_0051108C): a heap block with the record count at +0, the used size at +4
// and the rows from +8. Each row starts with its size dword, kind word (+4) and id word (+6); +8 is the
// next id of a chain.
struct RuntimeSceneMarkerRandomizer
{
    u32& random; // DAT_0054DD7C.
    const i32& swap_count; // DAT_00545CC8.
    std::span<const u16> swap_ids; // DAT_00545C48, id pairs.
    const i32& group_count; // DAT_00545D1C.
    std::span<const u16> group_ids; // DAT_00545CCC, ten ids per group.
};
// Handlers indexed by marker kind (DAT_0042A720). An empty entry is a retail null slot; kinds whose
// retail handler is not translated must be marked so dispatch refuses them.
struct RuntimeSceneMarkerHandlers
{
    std::array<std::function<void(u8* record)>, 0x1C> handlers;
    std::array<bool, 0x1C> untranslated{};
};
u8* load_runtime_marker_resource(const char* name, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x000FFE20, EAX name
u8* load_runtime_scene_markers(const char* scene_record, AnimationResourceRegistry& registry,
    ResourceErrorState& error); // Xbox 0x0011CB80, ECX scene record
void swap_runtime_marker_pairs(u8* records, const RuntimeSceneMarkerRandomizer& context); // Xbox 0x001401B0
void shuffle_runtime_marker_rows(std::span<u8*> rows, u32& random); // Xbox 0x00140450, EDI row pool
void shuffle_runtime_marker_groups(u8* records, const RuntimeSceneMarkerRandomizer& context); // Xbox 0x001402B0
void dispatch_runtime_marker_records(u8* records, const RuntimeSceneMarkerHandlers& handlers); // Xbox 0x0011EEC0, EDI records
void start_runtime_scene_markers(const u8* records, const RuntimeSceneMarkerRandomizer& randomizer,
    const RuntimeSceneMarkerHandlers& handlers); // Xbox 0x00140020
u16 next_runtime_marker_id(u16 id, i16 kind, i32 wrap, const u8* records); // Xbox 0x0011F1C0, AX id
void restart_runtime_marker_player(const u8* records,
    const RuntimeSceneMarkerHandlers& handlers); // Xbox 0x00127FF0
u8* find_runtime_marker(const u8* records, i16 id); // Inlined id walk over DAT_0051108C.
}
