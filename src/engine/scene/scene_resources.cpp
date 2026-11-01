#include "gbtg/entity.h"
#include "gbtg/anim.h"
#include "gbtg/model_runtime.h"
#include "gbtg/actor_runtime.h"
#include "gbtg/gfx.h"
#include "actor_loading.h"
#include "../entities/aid_spawn_resources.h"

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <malloc.h>
#include <span>
#include <vector>

namespace gbtg
{
ActorLoadList g_side_list{};

namespace
{


} // namespace

// Xbox 0x0011B630
void entity_release_aid_records()
{
    // Xbox 0x0011B630
    // The next pointer is taken before 0x000F4EE0, which unlinks and frees
    // the record. The side list at DAT_00510568 is cleared without a walk.
    AidSpawnQueueRecord* link = g_aid_spawn_queue.next;
    while (link != nullptr)
    {
        AidSpawnQueueRecord* next = link->next;
        release_aid_spawn_queue(link);
        link = next;
    }
    g_aid_spawn_queue.previous = nullptr;
    g_aid_spawn_queue.next = nullptr;
    g_side_list.prev = nullptr;
    g_side_list.next = nullptr;
}

namespace
{

// DAT_00512140. Twenty records, stride 0x18, ending at DAT_00512320.
// +0 is the live flag, +4 the float the tick decays, +0xc the tag
// 0x00124320 posts, and +0x14 the voice key 0x001316F0 stored.
struct EffectSlot
{
    i32 live = 0;
    float time = 0.f;
    i32 value = 0;
    i32 tag = 0;
    i32 param = 0;
    i32 voice = 0;
};

constexpr i32 kEffectSlotCount = 20;
EffectSlot g_effect_slots[kEffectSlotCount]{};

// DAT_00512320. Pointers occupy DAT_00512324 and stop before DAT_00512344.
// The times at DAT_00512334 are a second array and this reset leaves them.
constexpr i32 kEffectSpawnCap = 4;
void* g_effect_spawn[kEffectSpawnCap]{};
i32 g_effect_spawn_count = 0;

// Voice record inside the span at [DAT_00545abc]+0. The key is the dword
// at +0. The aid 0x00035C00 releases sits at +0x88. Stride is 0x94.
struct EffectVoice
{
    i32 key = 0;
    void* aid = nullptr;
};

// Span allocated by 0x00130BB0. +0 is the stride word, +4/+8 the range.
struct EffectVoiceSpan
{
    u16 stride = 0x94;
    std::vector<EffectVoice> voices;
};

// DAT_00545abc. Null until that allocator runs. +0 is the span.
struct EffectBank
{
    EffectVoiceSpan* span = nullptr;
};

EffectBank* g_effect_bank = nullptr;

// 0x001319C0. EAX is the bank, ECX the slot key. The first matching record
// releases its aid, then the tail slides down by one stride and the end
// shrinks. A miss leaves the span alone. The caller ignores the return.
void effect_drop_voice(EffectBank& bank, i32 key)
{
    // Xbox 0x00123B30
    EffectVoiceSpan* span = bank.span;
    if (span == nullptr)
    {
        return;
    }
    std::vector<EffectVoice>& voices = span->voices;
    for (size_t i = 0; i < voices.size(); ++i)
    {
        if (voices[i].key != key)
        {
            continue;
        }
        // 0x00035C00. The aid directory is not in this file, so the hold
        // is cleared. 0x00165AF0 then memmoves the following records down.
        voices[i].aid = nullptr;
        voices.erase(voices.begin() + static_cast<std::ptrdiff_t>(i));
        return;
    }
}

} // namespace

// Xbox 0x00123B30
void effects_clear_voice_slots()
{
    // Xbox 0x00123B30
    // The bank is re-read after each drop. A null bank skips 0x001319C0
    // and the slot is still cleared. The float at +4 is a movss of 0.
    for (EffectSlot& slot : g_effect_slots)
    {
        if (g_effect_bank != nullptr)
        {
            effect_drop_voice(*g_effect_bank, slot.voice);
        }
        slot = {};
    }

    i32 count = g_effect_spawn_count;
    if (count > kEffectSpawnCap)
    {
        count = kEffectSpawnCap;
    }
    for (i32 i = 0; i < count; ++i)
    {
        g_effect_spawn[i] = nullptr;
    }
    g_effect_spawn_count = 0;
}

namespace
{

// DAT_00512520. Two bins, stride 0x14, ending at DAT_00512548.
// +0 is the cap 0x0012A7D0 subtracts from one list length before it
// walks the list at +0x10. The four list heads are left in place.
struct IdBin
{
    i32 cap = 0;
    u32 list_4 = 0;
    u32 list_8 = 0;
    u32 list_c = 0;
    u32 list_10 = 0;
};

static_assert(sizeof(IdBin) == 0x14);
static_assert(offsetof(IdBin, list_10) == 0x10);

constexpr i32 kIdBinCount = 2;
IdBin g_id_bins[kIdBinCount]{};

// DAT_0051254c. Ten records, stride 0x90, so the id words run from
// DAT_005125d8 and stop before DAT_00512b78. +0x8c is the id 0x0012A1E0
// stores from 0x00039830. 0xFFFF means that slot has none. The name at
// +0 and the live count at DAT_00512548 are not part of this reset.
struct IdRecord
{
    u8 head[0x8C]{};
    u16 id = 0;
    u8 tail[2]{};
};

static_assert(offsetof(IdRecord, id) == 0x8C);
static_assert(sizeof(IdRecord) == 0x90);

constexpr i32 kIdRecordCount = 10;
IdRecord g_id_records[kIdRecordCount]{};

} // namespace

std::span<u8> scene_named_sound_records()
{
    return {reinterpret_cast<u8*>(g_id_records), sizeof(g_id_records)};
}

// Xbox 0x00129F30
void scene_initialize_handle_slots()
{
    // Xbox 0x00129F30
    for (IdRecord& record : g_id_records)
    {
        record.id = 0xFFFF;
    }
    g_id_bins[0].cap = 0x14;
    g_id_bins[1].cap = 6;
}


} // namespace gbtg
