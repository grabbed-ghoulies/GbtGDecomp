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
int script_handle_pair_updates(Game& g, const ScriptNode& node, int param_2); // Xbox 0x00124FC0

// Xbox 0x001251E0. Handler 5. The dword at node+4 is the type. Type 0x7A
// calls 0x000FBFB0 with the cstring at node+8 and passes that index to
// 0x0004A100. Type 0x7B calls 0x0004A240. The other listed types build the
// record 0x0004ADD0 reads through EAX: opcode at +0, a word or dword at +8,
// and for type 0x80 the dword at node+0x48 at +0xC. Type 0x7C stores opcode
// 0x10 and float 0. Type 0x7D stores opcode 0x11 and the float at node+8.
// Type 0x7F returns 2 when 0x001077B0 of the cstring at node+8 is 0; otherwise
// opcode 9 and a second call, and the word is AX. Types 0x7E (opcode 5) and
// 0x82 (opcode 4) share that word store. Type 0x80 stores opcode 0xA. Type
// 0x81 stores opcode 0x12. Type 0x83 stores opcode 0xB and the dword at
// node+8. Type 0x84 stores opcode 0xC. Type 0x85 stores opcode 0. Type 0x86
// stores opcode 3. 0x0004A100, 0x0004A240, and 0x0004ADD0 are not translated,
// so those calls are not made. Any other type returns 3. A handled type
// returns 2.
// Xbox 0x001251E0
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

// Xbox 0x001259B0. Handler 7. The dword at node+4 is the type. Type 0x4F
// with a zero second argument stores the word at node+8 and the dword at
// node+0xC at DAT_005123B0 + DAT_00512410*8, then increments DAT_00512410.
// A nonzero second argument calls 0x001117E0. EAX is the word at node+8 and
// EBX is the dword at node+0xC. Type 0x56 builds a message whose first dword
// is 0x67 and whose second dword is the word at node+8, then calls
// 0x00122F70 with that message in EDI. Type 0x29 with a nonzero second
// argument and a zero third argument reads DAT_00512378. Zero calls
// 0x000532E0 on the node and passes its return, the word at node+8 in EAX,
// the word at node+0xC, and the dword at node+0x14 to 0x0005B540. The dword
// at node+0x10 then selects the return: zero returns 2, nonzero calls
// 0x00117400(0xffff, 0), sets DAT_00512378, and returns 0. Nonzero calls
// 0x0005B780 with the word at node+8 in SI. A miss compares 8 with the dword
// at node+0xC. A hit compares the written word. A match calls
// 0x00117400(0xffff, 1), clears DAT_00512378, and returns 2. The other path
// stores those two words in the 4-byte record at DAT_0051237C and grows
// DAT_005123AC when the index has reached the count. 0x001117E0 (type 0x4F
// with a nonzero second argument) is not translated, so that call is not made.
// Any other type returns 3.
// A handled store returns 2.
// Xbox 0x001259B0
int script_handle_action_links(Game& g, const ScriptNode& node, int param_2, int param_3); // Xbox 0x001259B0
bool script_node_is_last_action(const SceneScript& script, const ScriptNode& node); // Xbox 0x000532E0
struct AudioBankStreams;
AudioBankStreams& scene_music_streams(Game& g); // The native scene's bank streams (DAT_004DF370..).
struct ScriptDoorStartContext
{
    RuntimeDoorContext& doors; // DAT_004EE2B8.
    std::function<void(u32 id)> notify_body_list; // Actor vtable +0xC with {id} to each DAT_00511B20 node's actor.
};
void script_apply_scene_door_states(ScriptDoorStartContext& context); // Xbox 0x00125B80

// Xbox 0x00126140. Handler 8. The dword at node+4 is the type. The table
// covers types 0x17 through 0x88. Any other type, and the holes in that
// range, return 3.
//
// Type 0x17 returns 0 when the third argument is nonzero. Zero tests the
// dword at node+0x10 (nonzero calls 0x000D0530), then passes the word at
// node+8, the dword at node+0xC and ESI = node+0x14 to 0x000D0550 and
// returns 2.
//
// Type 0x18 tests DAT_00512414. Zero calls 0x000D0680, stores 1, and
// returns 0. Nonzero compares DAT_005064D8 with 3. Equal stores 0 and
// returns 2. Otherwise it returns 0.
//
// Type 0x54 copies five fields into DAT_00506540 and returns 2.
//
// The step types build a stack step and pass it to 0x000D05D0 in EDI:
// 0x19 (0x00125C80, step 0), 0x1B (0x00125CC0, step 1), 0x1C (0x00125D00,
// step 2) and 0x1D (0x00125D40, step 3) do. The builders 0x00125D80,
// 0x00125DC0, 0x00125E00, 0x00125E40, 0x00125E80, 0x00125EC0, 0x00125F00,
// 0x00125F40, 0x00125F80, 0x00125FC0, 0x00126000, 0x00126040, 0x00126080,
// 0x001260C0 and 0x00126100 are not translated. 0x001077B0 runs where
// the dump calls it: EAX is the cstring at the listed node offset.
// 0x0001BB60, 0x00126940, and 0x00116C50 are not translated, so no index is
// invented and those calls are not made. A handled type returns 2. Type
// 0x1A returns 2 before 0x000D05D0 when bit 0x10 of DAT_0050EA9C is set.
// The second argument is unused.
// Xbox 0x00126140
int script_handle_scene_actions(Game& g, const ScriptNode& node, int param_2, int param_3); // Xbox 0x00126140

// Xbox 0x001269D0. Handler 9. The dword at node+4 is the type. Type 0x49 ORs
// the dword at node+8 into DAT_0050EA9C and returns 2. Type 0x4A clears those
// bits and returns 2. Any other type returns 3.
// Xbox 0x001269D0
int script_handle_flag_bits(Game& g, const ScriptNode& node); // Xbox 0x001269D0

// Xbox 0x00126A10. Handler 10. The dword at node+4 is the type. The byte map
// at 0x00126D94 covers types 0x2A through 0x97. Any other type, and the holes
// in that range, return 3.
//
// Type 0x2A tests DAT_00512418. Zero calls 0x0013E8D0 with EBX = node+8,
// EDX = the dword at node+0x88, and node+0x8c on the stack.
// A zero dword at node+0x10c returns 2. Any other value stores 1
// and returns 0. Nonzero calls 0x001412F0; a ready load clears the flag and
// returns 2, while an unfinished load returns 0.
//
// Type 0x2B would call 0x000F4E50 with EAX = node+8. That call is not
// translated. The case returns 2.
//
// Type 0x51 calls 0x001077B0 with EAX = node+8. It walks DAT_00511B38. A
// matching word at object+6, and a zero dword at node+0x48 or that dword
// equal to the word at object+0x1FC, calls vtable +0xC with message id 1.
// An empty list returns 2. A finished walk returns 2.
//
// Type 0x58 builds the message {0x65, word}, the word from 0x0002B710 (the
// actorStrategyId named at node+0x48), calls 0x001077B0 with EAX = node+8,
// and passes that tag in EDI with the message in EBX to 0x00122D40, which
// sends it to each DAT_00511B38 actor with that tag. The case returns 2.
//
// Type 0x5C stores message id 0x36, calls 0x001077B0 with EAX = node+8, and
// passes that tag in EDI with the message in EBX to 0x00122D40, which sends
// it to each DAT_00511B38 actor with that tag. The case returns 2.
//
// Type 0x5E calls 0x001077B0 with EAX = node+8 and passes the tag, the
// dword at node+0x4c and XMM0 (the float at node+0x48) to 0x000F4930, the
// ghoul intro's caption. The case returns 2.
//
// Type 0x6B calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, stores the byte at
// node+0x48 into object+0x969. A live pointer at object+0x958 whose pointer
// at +0x10 is live would call 0x00029C80 with EAX = that pointer, ECX = the
// slot, and EDX = 1. That call is not translated, so a zero return is not
// taken and the vtable call that would clear +0x10 is not made. An empty
// list returns 2. A finished walk returns 2.
//
// Type 0x70 calls 0x001077B0 with EAX = node+8. ESI would be that tag, XMM1
// the float at node+0x48, and XMM2 the float at node+0x4c, for 0x0010F6C0.
// That call is not translated. The case returns 2.
//
// Type 0x78 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, would call 0x00019660
// with EAX = the object and the dword at node+0x48 on the stack. That call
// is not translated. An empty list returns 2. A finished walk returns 2.
//
// Type 0x97 calls 0x001077B0 with EAX = node+8 and walks DAT_00511B48. A tag
// below 2, or a word at object+6 equal to the tag, would call 0x00028210.
// That call is not translated. The word at object+0x990 is cleared. The
// message id is 0x4D and the dword at node+0x48 follows it. 0x000412A0
// writes the world position and 0x00041820 writes the world euler. Vtable
// +0xC receives that message. An empty list returns 2. A finished walk
// returns 2.
// Xbox 0x00126A10
int script_handle_actor_actions(Game& g, const ScriptNode& node); // Xbox 0x00126A10

// Xbox 0x00126E10. Handler 11. The dword at node+4 is the type. The byte map
// at 0x001271F0 covers types 0x4D through 0x9E. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second argument
// is the flag 0x00128A00 passes: nonzero when a DAT_0050E4DC node has 0x7D
// at +8.
//
// Type 0x4D loads the word at node+8 into DX for 0x00129ED0. A hit would call
// 0x00041820 and 0x000412A0 with ESI set to that object, add the float at
// node+0x8C times 0.017453292 (0x0032B0FC) to the second euler float, then
// call 0x00112C50 and 0x00112CC0. 0x00112C50 is not translated. The port's
// pulse object does not carry +0x08, +0x14, +0x38, or +0x88, so the two
// camera writes are not applied to it and 0x00112CC0 is not called.
//
// Type 0x4E multiplies the float at node+0x10 by 0.01 (0x0032B124). 0x00019200
// would take that product in XMM1 and ECX = 0x0054DD7C. That call is not
// translated, so its nonzero return is not taken and 0x001400A0 is not called.
//
// Type 0x60 loads the word at node+8 into DX for 0x00129ED0. A hit would call
// 0x00112CC0 with ESI set to that object. That call is not made.
//
// Type 0x62 calls 0x0011F1C0(0x13, 1) with AX 0. A nonzero AX walks the
// records at DAT_0051108C+8, comparing the word at record+6 with AX and the
// dword at record+0x28 with the dword at node+8. A hit calls 0x0013D250 with
// record+0xC and (0 - record+0x18, record+0x1C + pi, record+0x20). A miss
// calls 0x0011F1C0(0x13, 0) with AX the current id and stops when AX matches
// the first return or AX is 0.
//
// Type 0x67 with a zero second argument stores the byte at node+8 and the
// word at node+0xC into DAT_00512420 and increments DAT_00512440. Nonzero
// with a zero dword at node+0xC would call 0x0012C8A0 with the byte at node+8
// and 0x0012C950. Any other dword would call 0x0012C8A0 with 0x0012C930, then
// 0x00122CE0 with EDI pointing at message id 0x4A. Those calls are not
// translated.
//
// Type 0x6E would call 0x001249B0 with EDI = the dword at node+8 and EAX = 8.
// That call is not translated.
//
// Type 0x73 would call 0x001400D0. CX is the word at node+8. The nine stack
// words are node+0xC through node+0x2C. That call is not translated.
//
// Type 0x74 calls 0x00115F40 with EDI pointing at message id 1: each active
// scene control (DAT_0050E438) gets it through vtable +0xC.
//
// Type 0x8E would call 0x001189D0. XMM0 is the float at node+0xC, ECX is the
// dword at node+8, EAX points at node+0x1C, and the stack dwords are node+0x10,
// node+0x14, and node+0x18. That call is not translated.
//
// Type 0x9D would call 0x00049D10. A zero return loads the dword at node+8
// into ECX for 0x000F2A20. A nonzero return from that call increments
// DAT_00506C84 and calls 0x0012D7E0 with EDI = 1 and that count on the stack.
// 0x00049D10 and 0x000F2A20 are not translated, so the increment is not made.
//
// Type 0x9E with a nonzero dword at node+0x10 stores the dword at node+8 at
// DAT_00512448 + DAT_00512444*4. 0x000F29A0 would take ECX = that dword and
// its EAX would be stored at DAT_0051245C. That call is not translated, so
// the parallel dword is not stored. DAT_00512444 is then incremented. A
// nonzero dword at node+0xC would call 0x000F29D0. Zero would call 0x000F29F0.
// Both take ECX = the dword at node+8. Neither call is translated.
// Xbox 0x00126E10
int script_handle_resource_actions(Game& g, const ScriptNode& node, int param_2); // Xbox 0x00126E10

// DAT_00512470. Type 0x13 sets this when the dword at node+0x88 is nonzero.
// Type 0x16 sets it when the dword at node+0x8C is nonzero. Type 0x53 sets it
// on the first visit.
extern i32 g_flag_2470;
extern u32 g_flag_10b4; // DAT_005110B4, the mirror pass gate.

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

// Xbox 0x00127340. Handler 12. The dword at node+4 is the type. The byte map
// at 0x00127458 covers types 0x3A through 0x8D. Any other type, and the holes
// in that range, return 3. Every handled type returns 2. The second and third
// arguments from 0x00128A00 are unused.
//
// Type 0x3A would call 0x00116D20. ESI is node+8 and the stack dword is 3.
// That call is not translated.
//
// Type 0x3B tests the byte at DAT_0050E4E0. Zero falls through and returns 2.
// Nonzero would call 0x00038460 with that address. The dword at bank+0x88 is
// decremented. A result less than 1, with a bank id other than 0x12 and a
// nonzero return from 0x00038590, would call 0x00038360. 0x00038260 then
// takes the bank in EAX and the id is stored as 0. 0x00038590, 0x00038360,
// and 0x00038260 are not translated, and the bank object from 0x00038460 is
// not visible here, so those stores are not made. The byte at DAT_0050E4E0
// is then cleared.
//
// Type 0x45 tests DAT_0050E560. Nonzero falls through and returns 2. Zero
// would call 0x00116C90, stores 1, and returns 2. That call is not translated.
//
// Type 0x55 calls 0x001077B0 with EAX = the cstring at node+8. ECX would be
// that return for 0x00116C50. That call is not translated.
//
// Type 0x57 would call 0x00116D20. ESI is node+8 and the stack dword is 2.
// That call is not translated.
//
// Type 0x8D would call 0x00038720. EAX is node+8 and the stack dword is 6.
// That call is not translated. The case falls through and returns 2.
// Xbox 0x00127340
void create_scene_ambient_tune(Game& g); // Xbox 0x00116C90
extern char g_name_e4e0[0x80]; // DAT_0050E4E0, the scene's named music stream.
extern i32 g_flag_e560; // DAT_0050E560: node 0x45 has made this scene's ambient tune.
// The music release at the top of 0x00116E00 (from 0x0011CAC0, before each scene starts).
void release_scene_music_at_start(); // Xbox 0x00116E00 (part)
int script_handle_event_actions(Game& g, const ScriptNode& node); // Xbox 0x00127340

// Xbox 0x001274B0. Handler 13. The dword at node+4 is the type. The byte map
// at 0x00127A2C covers types 0x13 through 0x9C. Slots are 0x13, 0x14, 0x15,
// 0x16, 0x53, 0x9B, and 0x9C. Any other type returns 3. The second argument
// from 0x00128A00 is unused. The third argument is the value that walk passes.
//
// Type 0x13 starts the named cutscene and waits for its instance to finish.
// A terminal cutscene keeps the instance alive through the scene transition.
// The optional skip widget is still owned by the untranslated UI path.
//
// Type 0x14 with a nonzero third argument returns 2. Nonzero DAT_00512474
// returns 0 while DAT_004EE1EC is nonzero. Otherwise it calls
// 0x00117400(0xFFFF, 1), clears DAT_00512474, and returns 2. Zero calls
// 0x001077B0 with EAX = node+8. A tag above 1 would call 0x001291A0(tag, 0).
// A zero return would return 2. That call is not translated, so the return
// is not taken. A nonzero dword at node+0x8C would call 0x00049D10. A
// nonzero return would return 2. That call is not translated, so the return
// is not taken. A nonzero dword at node+0x90 tests bit 0x8000000 of
// DAT_0050EA9C when node+0x94 is 0, and bit 0x10000000 when node+0x94 is 1.
// A set bit returns 2. Any other node+0x94 skips the test. 0x000FBF90 would
// take EAX = node+0x48. A nonzero return would build "%s%d" at DAT_00508EB8
// through 0x001661F9 when node+0x88 is nonzero, calling 0x0001BB60 with
// ECX = that dword + 1, EDX = 1, and ESI = 0x0054DD7C. 0x000582A0 would
// then take ECX = that buffer, or node+0x48 when node+0x88 is zero, and a
// stack 0. A nonzero return calls 0x00117400(0xFFFF, 0), stores 1, and ORs
// the same DAT_0050EA9C bit. node+0x94 equal to 1 returns 0 after the OR.
// The other path returns 0 as well. 0x000FBF90, 0x0001BB60, and 0x000582A0
// are not translated, so that block is not entered and the case returns 2.
//
// Type 0x15 would call 0x00106D50 with the byte at node+0x88, the dword at
// node+0x8C, and 1. EDI is node+8. 0x00106DD0 then takes DL = that byte.
// Neither call is translated. The case returns 2.
//
// Type 0x16 with a nonzero third argument returns 2. Nonzero DAT_00512470
// would call 0x0004EFE0. A zero return returns 0. A nonzero return clears
// the flag, calls 0x0004ECE0, and returns 2. That call is not translated,
// so the nonzero return is not taken and this path returns 0. A zero flag
// with a nonzero dword at node+0x8C stores 1 and returns 0. Otherwise the
// case returns 2.
//
// Type 0x53 plays the player's walk-in cutscene at the first kind 0xC marker with the relic candle's scene control;
// when it ends, the player marker runs again (0x00127FF0). See play_script_walk_in_cutscene.
//
// Type 0x9B calls 0x000FB120 with 1.0, EAX = node+8, and ECX = a stack
// vector. Type 0x9C calls 0x000493D0 with EDX = node+8 and EAX = that
// vector. DAT_00511B40 null, or the sentinel 0x93C, zeros the vector.
// Any other pointer calls 0x000412A0 with ESI = that pointer minus 0x93C
// and EDI = the vector. 0x000FB120 and 0x000493D0 are not translated, and
// the port's 0x000412A0 does not take this ESI/EDI pair, so those calls
// are not made. Both cases return 2.
// Xbox 0x001274B0
int script_handle_cutscene_actions(Game& g, const ScriptNode& node, int param_3); // Xbox 0x001274B0

// DAT_00430d08. Types 0x46 and 0xA6 store 0 when the second argument is zero.
// The same path stores a zero byte at DAT_00430d14. 0x0011EB30 clears both
// from entity.cpp; that copy is private to that file.
extern i32 g_flag_30d08;

extern char g_byte_30d14[0x8C];
// DAT_00430D08 takes `value` and DAT_00430D14 the name (empty for none): the weapon the player spawns holding.
void remember_script_player_weapon(const char* name, i32 value); // Xbox 0x00123AD0, EAX name, ECX value
// The teardown's: the story bits script type 0x9E changed go back when `restore` (DAT_00430D0C 1); the list empties.
void restore_script_story_bits(bool restore); // Xbox 0x001272B0, EAX restore

// Xbox 0x00127BC0. Handler 14. The dword at node+4 is the type. The byte map
// at 0x00127F6C covers types 0x2C through 0xA6. Slot 14 of the table at
// 0x00127F30 returns 3, and the holes in that range use it. A type outside
// the range returns 3 as well. The second argument is the flag 0x00128A00
// passes. The third argument is the value that walk passes.
//
// Type 0x2C with a nonzero third argument returns 2. Zero calls 0x001292D0.
// A null return returns 2. A live object loads the dword at object+0x95C
// into EAX, calls 0x0001B810 with two stack zeros, calls 0x00112CC0, and
// returns 2. 0x001292D0 is not translated, so the live path is not entered.
//
// Type 0x2D with a nonzero third argument returns 2. Zero would call
// 0x00127FF0 and return 2. That call is not translated.
//
// Type 0x2E calls 0x001292D0. A null return returns 2. A live object tests
// the dword at node+8. Zero calls 0x00042090. Nonzero calls 0x000420B0.
// EAX is the object and ECX is that dword. Both return 2. 0x001292D0 is not
// translated, so neither call is made.
//
// Type 0x2F calls 0x001292D0. A null return returns 2. A live object passes
// the stack slot at frame+0x1C to 0x00049DB0 in EAX, stores 7 in the next
// dword, and calls vtable +0xC with that slot (message 0x4D, goal 7).
//
// Type 0x30 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x32 and a second dword of 0.
//
// Type 0x31 with a zero second argument calls 0x00123AD0. EAX is node+8 and
// ECX is 0 (the weapon the player spawns with). A nonzero second argument calls
// 0x001292D0. A null return returns 2. A live object calls 0x0013A720 with
// EAX = node+8, ECX = the object, and a stack 1, then returns 2.
//
// Type 0x46 with a zero second argument stores 0 at DAT_00430d08 and a zero
// byte at DAT_00430d14, then returns 2. Nonzero calls 0x001292D0. A null
// return returns 2. A live object is pushed for 0x0013A7C0, then 0x0012ECC0
// runs, and the case returns 2. Those calls are not translated.
//
// Type 0x6F calls 0x001292D0. A null return returns 0. A live object calls
// 0x00028360 with ESI set to that object. AX 0 returns 2. A nonzero AX
// returns 2 when the dword at object+0x984 is nonzero, and 0 when that
// dword is 0. Before a native scene start there is no player, so it holds.
//
// Type 0x76 calls 0x001292D0. A null return returns 2. A live object calls
// vtable +0xC with message id 0x42 and a second dword of 1.
//
// Type 0x77 is that same call with a second dword of 0.
//
// Type 0x91 calls 0x001292D0. A null return returns 2. A live object calls
// 0x00128DF0 with the object, two zeros, the address of the node pointer,
// 0x4240, and 0. vtable +0xC then takes message id 0x37 and three zero
// dwords. A nonzero return from 0x00128DF0 would call 0x000412A0 with ESI
// set to that return and EDI aimed at the first zero. Those calls are not
// translated.
//
// Type 0x96 calls 0x001292D0. A null return returns 2. A live object calls
// 0x0011F1C0 with EAX = 0 and stack words 2, 1. A zero AX returns 2. Nonzero
// compares the word at record+0x28 from 0x0011EF20 with the dword at node+8.
// A miss calls 0x0011F1C0 with EAX = the previous AX and stack words 2, 0,
// and returns 2 when that AX is 0. A hit calls 0x0002B430 with EAX = the
// object and ECX = record+0xC, then 0x0002B560 with ECX = record+0x18.
// The case returns 2.
//
// Type 0xA0 calls 0x001292D0. A null return returns 2. A live object stores
// 0x2C over the first argument and calls vtable +0xC with that address.
//
// Type 0xA6 with a zero second argument takes the same DAT_00430d08 store as
// type 0x46. Nonzero calls 0x001292D0. A null return returns 2. A live object
// is pushed for 0x0013A860. A zero return returns 2. Nonzero calls 0x0012ECC0
// and returns 2. Those calls are not translated.
// Xbox 0x00127BC0
int script_handle_actor_effects(Game& g, const ScriptNode& node, int param_2, int param_3); // Xbox 0x00127BC0

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

// Xbox 0x00128050. Handler 15. The dword at node+4 is the type. The byte map
// at 0x001286BC covers types 0x1 through 0xA5. Any other type, and the holes
// in that range, return 3. The second argument is the flag 0x00128A00 passes.
// The third argument is the value that walk passes. A handled type returns 2.
//
// Type 0x1 calls 0x0011C890 with EAX = node+8 to queue the background name.
//
// Type 0x2 with a zero second argument calls 0x0011C950 to queue an object name.
// A nonzero second argument would call 0x00107720 with EAX = node+8. A zero
// return falls through and returns 2. A nonzero return would call 0x00107520
// with that block, then 0x000663E0 on it. This branch still needs a
// scene-owned factory and resource context before those calls can be wired.
//
// Type 0x4 stores the dword at node+8 into DAT_0050EA98.
// Types 0x5 and 0xC return 2.
// Type 0x6 stores the dword at node+8 into DAT_00427958.
// Type 0x7 stores the dword at node+8 into DAT_00430D10.
//
// Type 0x8 calls 0x001077B0 with EAX = node+8. 0x0010F690 would take that tag
// in ECX and the dword at node+0x48 on the stack. That call is not translated.
// The walk of DAT_00511B48 still runs. A word at object+6 equal to the tag,
// and a live pointer at object+0x958, stores the dword at node+0x48 into
// that pointer +0x6C. An empty list returns 2. A finished walk returns 2.
//
// Type 0x9 calls 0x001077B0 with EAX = node+8. 0x0010F660 would take that tag
// in ECX and the float at node+0x48 in XMM0. The message is id 0x4E and that
// same float. 0x00122D40 would take the tag in EDI and the message in EBX.
// Those two calls are not translated.
//
// Type 0xA clears the word at DAT_00507E78, then would call 0x000FF0A0 with
// "aid_loctext_ghoulies_script_scenenames", "scenenames", node+8, and that
// word. A zero return returns 2. A nonzero return would call 0x000FBE10 on
// the word. A nonzero return from that call returns 2. Zero, with a nonzero
// dword at node+0x48 and a zero third argument, draws when DAT_00512484 is
// live. A null DAT_00512484 would call 0x0005F390(0, 0, 0x5E4), step
// DAT_0054DD7C, and store 2.5, 0.5, and the new seed at +0x14, +0x18, and
// +0x1C. 0x0005F550, 0x000606D0, and 0x0005F580 would then run, and
// 0x0011DCF0 runs on the way out. 0x000FF0A0 is not translated, so its
// nonzero return is not taken and that block is not entered.
//
// Type 0xB would call 0x00123AD0. ECX is the byte at node+8 and EAX is node+9.
// Type 0x4B would call 0x000FD780 with ESI = node+8.
// Type 0x4C would call 0x0003A1F0 with ESI = the dword at node+8.
// Type 0x52 would call 0x0012AA50 with node+8 on the stack.
// Those calls are not translated.
//
// Type 0x61 stores four indexes and the words they select from 0x00314DE8,
// 0x00314DFC, 0x00314E10, 0x00314E28, and 0x00314E38. The second float uses
// the index at node+8, the same index as the first.
//
// Type 0x64 packs the bytes at node+0x28, node+0x2C, and node+0x30 into
// DAT_00512134. DAT_00512138 is 1 when the dword at node+8 is nonzero.
// DAT_00512104 copies that dword. 0x001236D0 would take the packed bytes at
// node+0xC, node+0x10, node+0x14, and node+0x18 twice, then the dwords at
// node+0x1C, node+0x20, and node+0x24. XMM0 is the float at node+0x1C.
// That call is not translated.
//
// Type 0x6D calls 0x001077B0 with EAX = node+8 and stores the word at
// DAT_0050E1B4 + DAT_0050E1B0*2, then increments the count.
//
// Type 0x79 would call 0x00114050. ECX is the dword at node+8. BL is 2 when
// that dword is 0 and 1 otherwise. That call is not translated.
//
// Type 0x89 stores the float at node+8 times 60 plus 0.99 into DAT_00506C60.
// 0x000F27C0 would then run with EAX = 1. That call is not translated.
//
// Type 0x8A with a nonzero dword at node+8 stores 0 at DAT_00509B74 and 1 at
// DAT_00509B70. Zero stores 1 and 0.
//
// Type 0x8F builds the vector (0, 1, 0) and the float at node+8 times
// 0.0055555557 times pi. 0x001047E0 would take that vector and that angle.
// The cross product that follows reads the matrix that call writes, and
// 0x000FE160 would take the result. Neither call is translated, so the
// product is not formed.
//
// Type 0x9A would call 0x000F2740. Type 0xA1 would call 0x000FEA90 with EAX =
// the dword at node+8. Those calls are not translated.
//
// Type 0xA2 stores 1 at DAT_005110B4 when the dword at node+8 is 0.
// Type 0xA4 stores 1 at DAT_0050EF1C when that dword is nonzero.
// Type 0xA5 stores 1 at DAT_00510650 when that dword is nonzero.
// Xbox 0x00128050
int script_handle_world_actions(Game& g, const ScriptNode& node, int param_2, int param_3); // Xbox 0x00128050

// Xbox 0x00128770. Handler 16. The dword at node+4 is the type. Type 0x39
// loads the dword at node+8 into EDI and calls 0x0012D050. That call walks
// DAT_00511B28, the first node of DAT_00511B24. A zero EDI calls 0x00147860
// on each object. A nonzero EDI decrements the dword at object+0xAD0 and
// calls 0x0003EEB0 when that counter hits 0. 0x0012D050 is not translated.
// The case returns 2.
//
// Type 0x59 stores 1 in the incoming argument slot, then calls 0x000FBFB0
// with count 0xE9, the names at 0x0042A854, and the cstring at node+8. A miss
// zeros EDI. A hit increments the index and zero-extends it into EDI. Both
// paths point EBX at that slot and call 0x00122FA0. That call walks
// DAT_00511B28, compares DI with the word at object+6, and calls vtable +0xC
// with EBX on a match. 0x00122FA0 is not translated. Both paths return 2.
// Any other type returns 3.
// Xbox 0x00128770
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
