#pragma once

#include "../navigation/cell_registration.h"
#include "../frontend/scoreboard_messages.h"
#include "../audio/event_voices.h"
#include "gbtg/entity.h"

#include <array>
#include <functional>

namespace gbtg
{
struct Game;
struct RuntimeDoorContext;
struct RuntimeDialogueActivationContext;

// The scripted event sequence (0x000D0xxx): a script node (type 0x17) names an event and its door, later nodes
// queue steps (0x88-byte records, 0x000D05D0), and type 0x18 plays them: the event door moves to state 5, the
// event's page shows, each step gets a scoreboard icon, and the steps run until all are done.
struct ScriptEventSequence
{
    u16 door = 0; // DAT_005064DC.
    entity_detail::NavigationCellRegistration* steps = nullptr; // DAT_005064E0, stride 0x88, growth 2.
    u32 hold = 0; // DAT_005064E4.
    std::array<char, 0x40> name{}; // DAT_005064E8, the event's page key.
    u32 page_shown = 0; // DAT_00506528.
    u32 unk_52c = 0; // DAT_0050652C, cleared before a step's reward Grim Reaper (0x000D0B40 result 4).
    float door_delay = 0.f; // DAT_00506534.
    float icon_delay = 0.f; // DAT_00506538.
    u32 waiting = 0; // DAT_0050653C.
    // Step type 0xF's camera view (its +0x24..+0x83). The step rows move as steps end, so the native view (linked
    // into the camera list) stays here; one step 0xF links it at a time.
    EntityCamNode key_camera{};
};
extern i32 g_event_sequence_state; // DAT_005064D8.
extern ScriptEventSequence g_event_sequence;

// The scene services the sequence reaches; the scene binds it while it runs (null between scenes).
struct ScriptEventSequenceContext
{
    RuntimeDoorContext& doors; // DAT_004EE2B8.
    RuntimeScoreboard*& board; // DAT_00545AB0.
    RuntimeScoreboardResources& board_resources;
    AudioEventVoices& voices;
    const u32& prompt_owner; // DAT_004EE1EC.
    const i32& restart_requested; // DAT_00430D0C.
    std::span<const u8> step_sizes; // DAT_0042890C, stride 0x14 by step type.
    float icon_delay; // DAT_0032B24C.
    float icon_pulse; // DAT_0032B110.
    float icon_fade; // DAT_0032B210.
    std::function<bool()> steps_held; // DAT_00511B40 null, or its player's body +0x15C byte 3 bit 8.
    std::function<i32()> cutscene_index; // DAT_004E1C34.
    // Step type 1's start (0x000D0200): the DAT_00511AA4 actors' retail addresses in list order, each with its +6
    // class key and its +0x908 word; and the drop of a key candidate it does not keep.
    std::function<std::vector<std::array<u32, 3>>()> latch_actors;
    std::function<void(u32)> drop_key_candidate; // 0x00112D10 on +0x920, then message 0x80 and 0x0003ED70.
    u32* random = nullptr; // DAT_0054DD7C.
    // The Grim Reaper's summons (0x000CF7B0).
    std::function<const Actor*()> first_player; // DAT_00511B40 minus 0x93C, null without one.
    std::function<const u8*()> markers; // DAT_0051108C.
    const float* occupancy_initial = nullptr; // 0x00111420's occupancy start.
    // 0x00107720 clones the named template, the patch is applied, 0x00107520 creates it (the scene keeps it) and
    // 0x000663E0 frees the clone: the new object's address, or 0 with none.
    std::function<u32(const char*, const std::function<void(std::span<u8>)>&)> create_from_template;
    std::function<void(u32, std::span<u32>)> message; // Vtable +0xC of the object at an address.
    std::function<std::span<u8>(u32, size_t)> bytes_at; // Scene bytes at an address.
    std::function<std::span<u8>(u32)> strategy_at; // A player actor's strategy (+0x954) bytes by its address.
    // Step type 0xF (0x000CFAE0): the actor a DAT_00511B48 node sits in (node - 0x944), by retail address, and
    // 0x00112CC0 on the object at an address.
    std::function<u32(const void*)> active_actor_of;
    std::function<void(u32)> activate_object;
};
// Message 0x81 from 0x000D09A0: {id, door word, step count, steps}; the steps travel by address.
struct ScriptEventStartMessage
{
    u32 id;
    u16 door;
    u16 pad;
    u32 count;
    const u8* steps;
};
extern ScriptEventSequenceContext* g_event_sequence_context;

void release_script_event_steps(); // Xbox 0x000D0490
void reset_script_event_sequence(); // Xbox 0x000D0530
// AX door, the stack hold word, ESI name.
void begin_script_event_sequence(u16 door, u32 hold, const char* name); // Xbox 0x000D0550
// EDI step: copies the step type's size into the next pool record.
void queue_script_event_step(std::span<const u8> step); // Xbox 0x000D05D0
// Script type 0x18 on its first pass: start the steps and the door.
void play_script_event_sequence(Game& g); // Xbox 0x000D0680
void set_script_event_sequence_state(Game& g, i32 state); // Xbox 0x000D09A0
void prepare_script_event_steps(); // Xbox 0x000D0B10
void tick_script_event_door(Game& g); // Xbox 0x000D12E0
void tick_script_event_sequence(Game& g); // Xbox 0x000D0300
void pulse_script_event_icons(); // Xbox 0x000D1200
bool script_event_icons_ready(); // Xbox 0x000D1260
void show_script_event_icons(); // Xbox 0x000D0F20
// EAX the point, EBX the euler, the stack position: the kind 0x12 marker farthest from the point gives the euler,
// and a free spot within 15 of it on its floor the position.
void place_runtime_grim_reaper(const Vec3& point, Vec3& position, Vec3& euler); // Xbox 0x000CF950
// A failed or timed-out step's Grim Reaper beside the player; nonzero floats replace its +0x954 record's +0x40 and
// +0x3C.
u32 summon_runtime_grim_reaper(float first, float second); // Xbox 0x000CF7B0
bool show_script_event_page(Game& g); // Xbox 0x000D1190
bool run_script_event_steps(Game& g); // Xbox 0x000D0B40
// ECX the step's +4 timer block: the step waits on a running cutscene, or counts its +4 delay down.
bool script_event_step_delay_done(std::span<u8> timer); // Xbox 0x000D0700
i32 run_script_event_kill_step(std::span<u8> step); // Xbox 0x000CFA10, step type 0
i32 run_script_event_key_step(std::span<u8> step); // Xbox 0x000CFA90, step type 1
// +0x18 the tag, +0x1C set once an actor of it has been seen (or none needs to be).
i32 run_script_event_tag_step(std::span<u8> step); // Xbox 0x000CFDA0, step type 2
i32 run_script_event_kill_count_step(std::span<u8> step); // Xbox 0x000CFE20, step type 3
i32 run_script_event_time_limit_step(std::span<u8> step); // Xbox 0x000CFE50, step type 4
i32 run_script_event_survive_step(std::span<u8> step); // Xbox 0x000CFEE0, step type 5
// The key carrier: the first DAT_00511B48 actor whose +0x96D is set carries the key (+0xCE4) and its effect; once its
// death queues event 0xB5, the key object appears at the event's point and "ghoulykeycam" looks at it until the +4
// delay runs out.
i32 run_script_event_key_carrier_step(std::span<u8> step); // Xbox 0x000CFAE0, step type 0xF
// EDI kind: adds the step's scoreboard icon and returns its message id.
u32 add_script_event_icon(u32 kind, u16 tag); // Xbox 0x0012FB80, EDI kind, ESI tag
// EDX id: the scoreboard message fades out.
bool fade_script_event_icon(u32 id); // Xbox 0x0012FDC0
}
