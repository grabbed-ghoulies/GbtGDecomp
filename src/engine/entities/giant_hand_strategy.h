#pragma once
#include <array>
#include "actor_factory_owner.h"
#include "body_state_actions.h"
#include "player_strategy_goals.h"
#include "strategy_fields.h"

#include <functional>

namespace gbtg
{
// entityStrategyGiantHand (factory type 0x3E, getter 0x000AC8E0, vtable 0x00505DC4; the factory table's row 0x3D
// carries the name) over the base strategy (type 8): a hand that waits out of sight and out of the world (0x401)
// while it tells the players how far its target is from the target's room, then grabs (0x3EF, "grab") once the
// target stands in it: it shows itself, takes its collision back and, on the grab's body bit 0x2D, starts the scene
// challenge its parameters name (0x000873A0). Every hand hears another's grab (message 0x6C) and its end (0x6D).
// Strategy bytes: +0x1C pause count (states 0x3FC, 0x3FF and 0x415 leave it one lower).
struct GiantHandStrategyState
{
    StrategyBase base{}; // +0
    u32 parameters = 0; // +0x38, 0x00302B78
    u32 other_grabbing = 0; // +0x3C, another hand grabs (message 0x6C, cleared by 0x6D)
    std::array<char, 0x80> challenge_first{}; // +0x40, parameters +0x18
    std::array<char, 0x80> challenge_second{}; // +0xC0, parameters +0x98
    i32 held = 0; // +0x140, messages 0x86 / 0x87 whose +8 is 7
    u32 shown = 0; // +0x144, drawn (0x000ACCA0), hidden by 0x001364B0
    u32 solid = 0; // +0x148, in the world's collision (0x001365C0), out by 0x00136540
    u32 model_flags = 0; // +0x14C, actor +0x1F0 kept while out of the world
    i32 model_mode = 0; // +0x150, actor +0x118 kept while out of the world
    u32 shape_mode = 0; // +0x154, body +0x244 at construction
    std::array<u8, 4> unk_158{}; // +0x158, never read or written by translated code
    u32 challenged = 0; // +0x15C, the grab started its challenge
    u32 latched = 0; // +0x160, body bit 0x23 has been seen
};
static_assert(sizeof(GiantHandStrategyState) == 0x164);
struct RuntimeGiantHandStrategy
{
    RuntimeBaseStrategyMessages& base; // Base goals, entry and strategy link (+0x20).
    NativeActorFactoryInstance& actor_view; // Render bins, probes, model anchors and live attachments.
    std::span<const u8>& attributes; // Actor +0x964.
    std::function<RuntimeBodyStateContext()> body_state; // 0x0001FEB0's views of the body.
    std::function<void(std::span<const u32>)> tell_players; // 0x00122D10, EDI the message, over DAT_00511B40.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520.
    u32& runtime_flags; // DAT_0050EA9C.
    const i32& chapter_mode; // DAT_00430D0C.
    const char* goal_grab; // "grab", 0x0031D588.
};
void giant_hand_strategy_object_sizes(ObjectSizes& output); // Xbox 0x000AC8C0
bool initialize_runtime_giant_hand_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    RuntimeGiantHandStrategy& context); // Xbox 0x000AC930
void update_runtime_giant_hand_strategy(RuntimeGiantHandStrategy& context); // Xbox 0x000ACA30, vtable +0x10
void dispatch_runtime_giant_hand_message(std::span<const u32> message,
    RuntimeGiantHandStrategy& context); // Xbox 0x000ACAF0, vtable +0xC
void enter_runtime_giant_hand_state(u16 state, u16 previous,
    RuntimeGiantHandStrategy& context); // Xbox 0x000ACBB0, vtable +0x14
void leave_runtime_giant_hand_state(u16 state, RuntimeGiantHandStrategy& context); // Xbox 0x000ACBF0, vtable +0x18
// Takes an actor out of the world's collision: its segment probes, model mode (+0x118) and shape probe, body shape
// mode 4, actor +0x1F0 bit 1 and body bit 0x3E set.
void remove_runtime_actor_collision(NativeActorFactoryInstance& view,
    RuntimeBodyStateContext& body); // Xbox 0x00136540, ESI actor
// Puts it back: +0x1F0 and +0x118 restored, the probes registered again, the body's shape mode and bit 0x3E cleared.
void restore_runtime_actor_collision(u32 flags, i32 mode, u8 shape_mode, NativeActorFactoryInstance& view,
    RuntimeBodyStateContext& body); // Xbox 0x001365C0, ESI actor, EAX flags
// Hides an actor: its render bins, its first live attachment suspended, its model anchors released and body bit
// 0x3C set.
void hide_runtime_actor(NativeActorFactoryInstance& view, EntityBitBlock& bits); // Xbox 0x001364B0, EBX actor
// The DAT_00512344 record of `mask` that holds the point, else the nearest one, or null.
const OccupancyRecord* find_nearest_occupancy_record(const Vec3& point, u32 mask); // Xbox 0x00124760, EAX point
// How far outside the record the point lies (0 inside it).
float occupancy_record_distance(const OccupancyRecord& record, const Vec3& point); // Xbox 0x00124B90, ECX record
}
