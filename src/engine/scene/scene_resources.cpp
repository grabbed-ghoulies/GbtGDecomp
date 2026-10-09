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

// DAT_00510578. 0x00028830 increments this when the word at body+0xcdc is 1.
i32 g_aid_body_count = 0;

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
    g_aid_body_count = 0;
}

namespace
{

// DAT_00506c74. 0x000F2B60 allocates 0xe bytes: kind 9, size 2, and the
// byte pointer aimed at the inline payload. The two bytes are scene flags.
struct SceneFlagBlock
{
    i32 kind = 0;      // +0
    i32 size = 0;      // +4
    u8* bytes = nullptr; // +8
};

SceneFlagBlock* g_scene_flags = nullptr; // DAT_00506c74

i32 g_scene_time = 0;   // DAT_00506c78. Sum of the truncated float at DAT_004312d0.
i32 g_scene_acc_7c = 0; // DAT_00506c7c
i32 g_scene_acc_80 = 0; // DAT_00506c80
i32 g_scene_acc_84 = 0; // DAT_00506c84
i32 g_scene_acc_88 = 0; // DAT_00506c88
i32 g_scene_acc_8c = 0; // DAT_00506c8c
u16 g_scene_gate = 0;   // DAT_00506c90. Nonzero runs the update at 0x000F2C40.

// DAT_00511b64. 0x000E270F stores a heap scene name here. 0x001234B0 stores
// the C string 0x00035E90 writes from the loaded aid (record +8).
const char* g_scene_name_src = nullptr;

// DAT_00511b68. Called with &DAT_00511b64 before that pointer is replaced.
using SceneNameRelease = void (*)(const char** slot);
SceneNameRelease g_scene_name_release = nullptr;

// DAT_004dd8fc. 0x00032700 stores the code 0x001234B0 passes (2).
i32 g_asset_fault_code = 0;

// DAT_00510ed8. 0x80 bytes, then the mode dword at DAT_00510f58.
// 0x0012C2D0 passes this address to the script lookup.
constexpr i32 kSceneNameBytes = 0x80;
char g_scene_name[kSceneNameBytes]{};
i32 g_scene_name_mode = 0; // DAT_00510f58

// DAT_00430d08. Three dwords, then an 0x80-byte path at DAT_00430d14.
// DAT_00430d94 sits immediately after that path.
i32 g_script_id = 0;        // DAT_00430d08
i32 g_script_state = 0;     // DAT_00430d0c
i32 g_script_count = 0;     // DAT_00430d10
char g_script_path[0x80]{}; // DAT_00430d14
i32 g_script_busy = 0;      // DAT_00430d94. 0x0008F675 sets this to 1.
i32 g_script_busy_next = 0; // DAT_00430d98

// 'aid_' as a little-endian dword. 0x0011EB30 clears the name when it matches.
constexpr u32 kAidPrefix = 0x5f646961u;
static_assert(kAidPrefix == (static_cast<u32>('a') | (static_cast<u32>('i') << 8) |
                             (static_cast<u32>('d') << 16) | (static_cast<u32>('_') << 24)));

// 0x0011EB30. EAX is the source string. The stack dword is stored at
// DAT_00510f58. A null source is left uncopied; Xbox would fault on it.
void scene_copy_name(const char* src, i32 mode)
{
    // Xbox 0x0011EAD0
    u32 head = 0;
    std::memcpy(&head, g_scene_name, sizeof(head));
    if (head == kAidPrefix)
    {
        g_scene_name[0] = '\0';
    }
    if (src != nullptr)
    {
        i32 i = 0;
        while (i < kSceneNameBytes - 1 && src[i] != '\0')
        {
            g_scene_name[i] = src[i];
            i += 1;
        }
        g_scene_name[i] = '\0';
    }
    g_script_id = 0;
    g_script_state = 0;
    g_script_count = 0;
    g_script_path[0] = '\0';
    if (g_script_busy == 0)
    {
        g_scene_acc_8c = 0;
    }
    g_script_busy = 0;
    g_script_busy_next = 0;
    g_scene_name_mode = mode;
}

// 0x00035940. EBX is the held pointer. 0x00035C00 drops one ref on the aid
// whose record+8 matches it. The aid directory is not in this file, so the
// ref walk has nothing to find and the slot is cleared.
void scene_release_aid(const char** slot)
{
    // Xbox 0x001234B0
    if (slot == nullptr)
    {
        return;
    }
    *slot = nullptr;
}

// 0x00032700. Stores the code at DAT_004dd8fc. DAT_004dd8f8 is BSS, so retail
// then spins and does not reach the stores after the call. The port records
// the code and returns so the hold is still published.
void scene_report_missing(i32 code)
{
    // Xbox 0x001234B0
    g_asset_fault_code = code;
}

// 0x00035E90. The name is the first stack argument. The out slot is the
// saved ECX, and a hit writes record+8 (a C string). DAT_004df250,
// DAT_004df244, and DAT_004df248 are not open here, so the slot stays null.
const char* scene_lookup_aid(const char* name)
{
    // Xbox 0x001234B0
    (void)name;
    return nullptr;
}

} // namespace

// Xbox 0x001234B0
void scene_store_aid_name(const char* name)
{
    // Xbox 0x001234B0
    if (g_scene_name_release != nullptr && g_scene_name_src != nullptr)
    {
        g_scene_name_release(&g_scene_name_src);
    }

    // Xbox reads the first dword with no null test. A null name misses the
    // 'aid_' check instead of faulting.
    u32 head = 0;
    if (name != nullptr)
    {
        std::memcpy(&head, name, sizeof(head));
    }
    if (head != kAidPrefix)
    {
        g_scene_name_src = nullptr;
        g_scene_name_release = &scene_release_aid;
        return;
    }

    const char* resolved = scene_lookup_aid(name);
    if (resolved == nullptr)
    {
        scene_report_missing(2);
    }
    g_scene_name_src = resolved;
    g_scene_name_release = &scene_release_aid;
}

// Xbox 0x0011EAD0
void scene_clear_flag_block()
{
    // Xbox 0x0011EAD0
    // rep stos of the byte count at flag+4 into the pointer at flag+8.
    // 0x000F2B60 publishes that block; a missing one is left alone.
    SceneFlagBlock* block = g_scene_flags;
    if (block != nullptr && block->bytes != nullptr && block->size > 0)
    {
        std::memset(block->bytes, 0, static_cast<size_t>(block->size));
    }
    g_scene_gate = 0;
    g_scene_time = 0;
    g_scene_acc_7c = 0;
    g_scene_acc_80 = 0;
    g_scene_acc_84 = 0;
    g_scene_acc_88 = 0;
    g_scene_acc_8c = 0;
    scene_copy_name(g_scene_name_src, 1);
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
