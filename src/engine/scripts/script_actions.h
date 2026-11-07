#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
struct RuntimeDoorContext;
std::string node_cstr(const SceneScript& script, const ScriptNode& node, u32 rel);
u32 script_find_actor_tag(const char* key); // Xbox 0x001077B0
class SceneActorServices;
// The natively started scene's actor services, or null before the scene has started natively (no actor lists).
SceneActorServices* script_scene_actors(Game& g);
// DI is the tag, EBX the message: each DAT_00511B38 actor whose word at +6 is the tag takes it through vtable +0xC.
void message_runtime_marked_actors(SceneActorServices& actors, u16 tag, std::span<const u32> message); // Xbox 0x00122D40

// Xbox 0x000F96F0. AL is the value. The stack byte selects the pair at +0x294.
void entity_store_pair(std::span<u8> pairs, u8 index, u8 value); // Xbox 0x000F96F0

// Xbox 0x00117400. Bit 0 of the mask drives DAT_0050E764. Bit 1 drives
// DAT_0050E768. A rising edge (counter becomes 1) and a falling edge
// (counter becomes 0) walk DAT_00511B38. Note 0x26 when the flag is set,
// 0x27 when it is clear. Vtable +0xC runs when the word at link+0x3a8
// equals 2, or the dword at object+0x93c is nonzero, only for the bit-1
// edge. The other objects get the bit-0 edge.
// Xbox 0x00117400
void script_update_decay_latches(u32 mask, i32 flag); // Xbox 0x00117400

// Xbox 0x00124CC0. Handler 1. Type 0x12 calls 0x00117400. Type 0x90 retires
// the actor whose short at +6 matches the tag. Type 0x9F packs the byte at
// +0x4C into the pair selected by the byte at +0x48. Any other type returns 3.
// Xbox 0x00124CC0
int script_handle_actor_state(Game& g, const ScriptNode& node); // Xbox 0x00124CC0

// Xbox 0x00124E50. Handler 2. Types 0x32 through 0x38 store the dword at
// node+8 and return 2. Types 0x34 and 0x36 scale by 0x001077B0. That call
// passes the cstring at node+0xC in EAX. Type 0x37 stores the float at
// node+8 times 0.01 (0x0032B124). Any other type returns 3.
// Xbox 0x00124E50
int script_handle_parameters(Game& g, const ScriptNode& node); // Xbox 0x00124E50

// Xbox 0x00124F30. Handler 3. Type 0x5D searches the cstring at node+8 in the
// actor tags. A miss calls 0x00122CA0 with tag 0. A hit calls it with the
// 1-based index, zero-extended from 16 bits. Both pass a message id of 1.
// Type 0x71 loads the word at node+8 into DX for 0x00129ED0. A hit calls
// vtable +0xC with a message id of 0x13. Any other type returns 3.
// Xbox 0x00124F30
int script_handle_actor_messages(Game& g, const ScriptNode& node); // Xbox 0x00124F30

// Xbox 0x00124FC0. Handler 4. The dword at node+4 must be 0x98. Any other
// type returns 3. The second argument is the flag 0x00128A00 passes: nonzero
// when a DAT_0050E4DC node has 0x7D at +8. Zero stores the dword at node+8
// at DAT_00512350 + DAT_00512370*8, the dword at node+0xC in the next dword,
// then increments DAT_00512370. Nonzero walks DAT_00511B20. The byte at
// node+8 selects the pair. The byte at node+0xC is the value in AL for
// 0x000F96F0. The object is the dword at link-0x730, then the dword at
// link-0x624. A null object skips that call. Both paths return 2.
// Xbox 0x00124FC0
int script_handle_pair_updates(Game& g, const ScriptNode& node, int live_7d); // Xbox 0x00124FC0

// Its cases are described above its definition in script_actions.cpp.
int script_handle_condition_setup(Game& g, const ScriptNode& node); // Xbox 0x001251E0

// Xbox 0x00125450. One node of the action walk. Type 0x3C stops the walk.
// The listed types copy a condition, seek the cursor to the block's closing
// 0x3C (0x000532B0) and tell the runner to advance past it. Every other type
// returns 3. The word filled by 0x001077B0 is left 0.
// Xbox 0x00125450
int script_handle_condition_node(Game& g, const ScriptNode& node, u32& cursor); // Xbox 0x00125450

// Xbox 0x000532B0
bool script_seek_node_type(const SceneScript& script, u32& cursor, u32 type); // Xbox 0x000532B0

// DAT_0050EA9C. Type 0x1A tests bit 0x10 of the low byte.
extern u32 g_flag_ea98; // DAT_0050EA98
extern u32 g_flags_ea9c;
extern float g_script_6c60; // DAT_00506C60, the chapter 5 countdown seconds (type 0x89).
// DAT_00506540 / DAT_00506544: script type 0x54's first two floats, the Grim Reaper's (0x000CF7B0).
extern float g_script_6540;
extern float g_script_6544;

// DAT_004DEA58, the input refresh frame.

// Its cases are described above its definition in script_actions.cpp.
int script_handle_action_links(Game& g, const ScriptNode& node, int live_7d, int ending); // Xbox 0x001259B0
bool script_node_is_last_action(const SceneScript& script, const ScriptNode& node); // Xbox 0x000532E0
struct AudioBankStreams;
AudioBankStreams& scene_music_streams(Game& g); // The native scene's bank streams (DAT_004DF370..).
struct ScriptDoorStartContext
{
    RuntimeDoorContext& doors; // DAT_004EE2B8.
    std::function<void(u32 id)> notify_body_list; // Actor vtable +0xC with {id} to each DAT_00511B20 node's actor.
};
void script_apply_scene_door_states(ScriptDoorStartContext& context); // Xbox 0x00125B80
// Scene start: the type 0x67 categories the script's head queued open or shut (0x0012C8A0 with mask 4).
void script_apply_queued_shape_events(
    const std::function<void(u8 category, bool set)>& change_shape_events); // Xbox 0x00127250
// Scene start: the type 0x98 model node states the script's head queued (0x000F96F0 on each body list node).
void script_apply_queued_model_node_states(
    const std::function<void(u8 slot, u8 value)>& set_model_nodes); // Xbox 0x00125070

// Its cases are described above its definition in script_actions.cpp.
int script_handle_scene_actions(Game& g, const ScriptNode& node, int live_7d, int ending); // Xbox 0x00126140

// Xbox 0x001269D0. Handler 9. The dword at node+4 is the type. Type 0x49 ORs
// the dword at node+8 into DAT_0050EA9C and returns 2. Type 0x4A clears those
// bits and returns 2. Any other type returns 3.
// Xbox 0x001269D0
int script_handle_flag_bits(Game& g, const ScriptNode& node); // Xbox 0x001269D0

// Its cases are described above its definition in script_actions.cpp.
int script_handle_actor_actions(Game& g, const ScriptNode& node); // Xbox 0x00126A10

// Its cases are described above its definition in script_actions.cpp.
int script_handle_resource_actions(Game& g, const ScriptNode& node, int live_7d); // Xbox 0x00126E10

// DAT_00512470. Type 0x13 sets this when the dword at node+0x88 is nonzero.
// Type 0x16 sets it when the dword at node+0x8C is nonzero. Type 0x53 sets it
// on the first visit.
extern i32 g_flag_2470;
extern u32 g_flag_10b4; // DAT_005110B4, the mirror pass gate.
extern u32 g_flag_ef1c; // DAT_0050EF1C: nonzero holds back the blob shadows (script type 0xA4, 0x00119990 clears).

// DAT_00512474. Shared with the scene-bound translated type-0x14 context;
// it remains set until native dialogue mode returns to zero.
extern i32 g_flag_2474;
extern i32 g_flag_2378; // DAT_00512378, type 0x29's held door.
extern i32 g_flag_2414; // DAT_00512414, type 0x18's started event.
extern i32 g_flag_2418; // DAT_00512418, type 0x2A's started ghoulybox.

// DAT_0051248C. Type 0x13 compares this dword with 3 and would pass that
// flag twice to 0x0004EDA0. The byte at [DAT_0051248C]+0x30F144 is g_decay_arm.
extern i32 g_state_248c;

extern i32 g_state_2490;

void create_scene_ambient_tune(Game& g); // Xbox 0x00116C90, node 0x45's ambient tune
extern char g_name_e4e0[0x80]; // DAT_0050E4E0, the scene's named music stream.
extern i32 g_flag_e560; // DAT_0050E560: node 0x45 has made this scene's ambient tune.
// The scene-start reset of the script's runtime (from 0x0011CAC0, before each scene loads).
void reset_scene_script_runtime(Game& g); // Xbox 0x00116E00
// Its cases are described above its definition in script_actions.cpp.
int script_handle_event_actions(Game& g, const ScriptNode& node); // Xbox 0x00127340

// Its cases are described above its definition in script_actions.cpp.
int script_handle_cutscene_actions(Game& g, const ScriptNode& node, int ending); // Xbox 0x001274B0

// DAT_00430d08. Types 0x46 and 0xA6 store 0 when the second argument is zero.
// The same path stores a zero byte at DAT_00430d14. 0x0011EB30 clears both
// from entity.cpp; that copy is private to that file.
extern i32 g_flag_30d08;

extern char g_byte_30d14[0x8C];
// DAT_00430D08 takes `value` and DAT_00430D14 the name (empty for none): the weapon the player spawns holding.
void remember_script_player_weapon(const char* name, i32 value); // Xbox 0x00123AD0, EAX name, ECX value
// The teardown's: the story bits script type 0x9E changed go back when `restore` (DAT_00430D0C 1); the list empties.
void restore_script_story_bits(bool restore); // Xbox 0x001272B0, EAX restore

// Its cases are described above its definition in script_actions.cpp.
int script_handle_actor_effects(Game& g, const ScriptNode& node, int live_7d, int ending); // Xbox 0x00127BC0

// DAT_00430D10. Type 0x7 stores the dword at node+8. entity.cpp keeps a
// private copy of this address.
extern i32 g_script_30d10;

u32 script_tune(u32 base, u32 index);

// DAT_00545AEC, DAT_00545AF0, DAT_00545AF4, DAT_00545AF8. Type 0x61 stores
// the four indexes. The looked-up words land at DAT_00434008, DAT_00434014,
// DAT_00434010, DAT_0043402C, and DAT_00434030.
extern u32& g_tune_5aec; // entity_detail::g_look_profile.

extern u32 g_tune_5af0;

extern u32 g_tune_5af4;

extern u32 g_tune_5af8;

// Scene load's camera reset: DAT_00434008 20, DAT_00434014 15, DAT_00434010 45, DAT_0043402C 0,
// DAT_00434030 -1, look profile DAT_00545AEC 1 and the other tune indexes 0. DAT_00545AD0..DAT_00545AE8 (the
// marker camera start) live in the scene services, which are created cleared after this reset.
void reset_runtime_camera_view_tunables(); // Xbox 0x0013CBD0
// Words of g_camera_view_tunables.
extern u32& g_tune_34008;

extern u32& g_tune_34014;

extern u32& g_tune_34010;

extern u32& g_tune_3402c;

extern u32& g_tune_34030;

// DAT_00545D20 and DAT_00545D24. 0x00140E30 clears both when the word at
// node+0x20 + 0x94 is 0. A live 0x001248D0(1, 0, 0) would set the first to 1, as the scene start
// (0x0011C72F) does.
// A live 0x001248D0(8, 0, 0) would set the second to 1.
extern i32 g_flag_5d20;

extern i32 g_flag_5d24;

// DAT_00545D28. Nonzero holds the previous tune indexes in the four words
// below. 0x00140E30 clears it after restoring those indexes.
extern i32 g_flag_5d28;

// DAT_00545D2C, DAT_00545D30, DAT_00545D34, DAT_00545D38. Saved copies of
// DAT_00545AEC, DAT_00545AF0, DAT_00545AF4, and DAT_00545AF8.
extern u32 g_tune_5d2c;

extern u32 g_tune_5d30;

extern u32 g_tune_5d34;

extern u32 g_tune_5d38;

// DAT_00510650. Type 0xA5 stores 1 when node+8 is nonzero.
extern u32 g_flag_0650;

// DAT_00512104. Type 0x64 copies node+8; 0x001237E0 tests it.
extern u32 g_flag_2104;
// DAT_00512138. Type 0x64 sets it while node+8 is nonzero; the candle (type 0xA1) adds its point light only then.
extern u32 g_flag_2138;
// The camera light 0x001236D0 stores for script type 0x64: DAT_00512114 / DAT_00512118 its colours and
// DAT_0051211C..DAT_00512128 its four floats.
extern u32 g_color_2134; // DAT_00512134, the point-light texture's clear colour (bytes R, G, B, 0).
extern u32 g_camera_light_color;
extern u32 g_camera_light_second_color;
extern float g_camera_light_1c;
extern float g_camera_light_20;
extern float g_camera_light_24;
extern float g_camera_light_28;

// Its cases are described above its definition in script_actions.cpp.
int script_handle_world_actions(Game& g, const ScriptNode& node, int live_7d, int ending); // Xbox 0x00128050

// Its cases are described above its definition in script_actions.cpp.
int script_handle_broadcast_actions(Game& g, const ScriptNode& node); // Xbox 0x00128770
// An actor's vtable +0xC with the retail message words; the scene binds it while it runs.
extern std::function<void(Actor& actor, std::span<u32> message)> g_script_actor_message;
// 0x00028210, ESI the actor: a scene player's +0x980 goal ring drained.
extern std::function<void(Actor& actor)> g_script_drain_actor_goals;
// 0x00122F70, EDI the message: vtable +0xC of each DAT_00511B20 node's actor (node - 0x934), the next node read first.
extern std::function<void(std::span<const u32> message)> g_script_body_list_message;
// 0x000F4930: the ghoul intro's caption for a tag after a delay (XMM0), shown for its lifetime.
extern std::function<void(u16 tag, float delay, float lifetime)> g_script_ghoul_intro_caption;
i32 draw_runtime_random_count(i32 high, i32 low, u32& random); // Xbox 0x0001BB60, ECX high, EDX low
// The DAT_00511B48 actors of the tag are marked for a one-type event; with `keep` 0 the tag joins DAT_0050E22C.
void mark_runtime_one_type_actors(u16 tag, i32 keep); // Xbox 0x00126940, DI tag
// The DAT_00511B24 objects (their +0x934 nodes from DAT_00511B28) whose +6 class key is `tag` hear `message`.
void message_runtime_tagged_list_actors(u16 tag, std::span<u32> message); // Xbox 0x00122FA0, DI tag, EBX message
// The scene's player character: its 0x00430DA0 override, else 2 with the alternate flags, else 1.
i32 scene_player_character(const Game& g); // Xbox 0x00087830

void end_cutscene_skip_input(Game& g); // Xbox 0x00127B50
} // namespace gbtg
