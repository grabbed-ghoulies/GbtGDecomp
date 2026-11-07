#pragma once
#include "gbtg/entity.h"

#include <array>

namespace gbtg
{
// An event step (0x88 bytes, a DAT_005064E0 pool row): what the event asks of the player. Script nodes 0x19..0x88
// build them and 0x000D05D0 queues them; 0x000D0B40 runs each by its type. What follows +0x18 depends on the type.

// The step's delay block (+4): waited out once the step is otherwise done (0x000D0700).
struct EventStepDelay
{
    float left = 0.f; // +0
    u32 started = 0; // +4
    u32 waits_for_cutscene = 0; // +8, a cutscene ran when it started: wait for it instead
};
static_assert(sizeof(EventStepDelay) == 0xC);

// Type 2 (0x000CFDA0): hold while an actor of the tag lives. Types 7 and 0xD also keep a tag at +0x18.
struct TagEventStep
{
    u16 tag; // +0x18
    u16 unk_1A; // +0x1A
    u32 seen; // +0x1C, one has lived (or none needs to)
};
// Type 3 (0x000CFE20): kill `required` of the tag.
struct KillCountEventStep
{
    i32 required; // +0x18
    u16 tag; // +0x1C
};
// Types 4 (0x000CFE50, a time limit) and 5 (0x000CFEE0, survive): the seconds left.
struct TimedEventStep
{
    float seconds; // +0x18
};
// Type 6 (0x000CFFD0): done at once; its builder keeps a word from the script node.
struct DoneEventStep
{
    u32 unk_18; // +0x18
};
// Type 7 (0x000CFF60): fail once the player's kills of the tag grow past the total kept on the first run.
struct SpareTagEventStep
{
    u16 tag; // +0x18
    u16 unk_1A; // +0x1A
    i32 kept_total; // +0x1C, -1 until the first run
};
// Type 8 (0x000CFFE0): "next one different": a kill of the current tag fails it.
struct NextDifferentEventStep
{
    u16 current; // +0x18, 0 until a tag is killed
    std::array<u16, 9> tags; // +0x1A
    std::array<i32, 8> seen; // +0x2C, each tag's last kill total
    u8 count; // +0x4C
};
// Type 10 (0x000D00C0): fail once the player takes a kind-3 hit past the total kept on the first run.
struct UnhitSinceEventStep
{
    i32 kept_total; // +0x18, -1 until the first run
};
// Type 0xD (script node 0x50): one of the named tags, drawn at random.
struct OneTypeEventStep
{
    u16 tag; // +0x18
};
// Type 0xE (0x000D0160): the number still to go against `target`.
struct NumberEventStep
{
    i32 target; // +0x18
};
// Type 0xF (0x000CFAE0): the key carrier, then the key and its camera.
struct KeyCarrierEventStep
{
    u32 carrier_found; // +0x18
    u32 key_shown; // +0x1C
    u32 key; // +0x20, the key object, by address
    std::array<u8, 0x60> camera_words; // +0x24, the camera view; the host keeps g_event_sequence.key_camera
    u32 camera_linked; // +0x84
};
// Type 0x10 (0x000D01D0): fail once the attacks pass `limit`.
struct AttackLimitEventStep
{
    i32 limit; // +0x18
};
// Type 0x12 (0x000CFDF0, script node 0x88): kill `required` of the tag drawn at random.
struct TagKillCountEventStep
{
    u16 tag; // +0x18
    u16 unk_1A; // +0x1A
    i32 required; // +0x1C
};

struct EventStep
{
    u32 type = 0; // +0
    EventStepDelay delay{}; // +4, 0x000D0700's ECX
    u32 icon = 0; // +0x10, the scoreboard message (0x0012FB80), 0 for none
    u32 icon_shown = 0; // +0x14
    union
    {
        std::array<u8, 0x70> body{}; // +0x18, zero until a builder fills it
        TagEventStep tag;
        KillCountEventStep kill_count;
        TimedEventStep timed;
        DoneEventStep done;
        SpareTagEventStep spare_tag;
        NextDifferentEventStep next_different;
        UnhitSinceEventStep unhit_since;
        OneTypeEventStep one_type;
        NumberEventStep number;
        KeyCarrierEventStep key_carrier;
        AttackLimitEventStep attack_limit;
        TagKillCountEventStep tag_kill_count;
    };
};
static_assert(sizeof(EventStep) == 0x88);
static_assert(offsetof(EventStep, key_carrier) == 0x18);
static_assert(offsetof(EventStep, key_carrier) + offsetof(KeyCarrierEventStep, camera_linked) == 0x84);
static_assert(offsetof(EventStep, next_different) + offsetof(NextDifferentEventStep, count) == 0x4C);
} // namespace gbtg
