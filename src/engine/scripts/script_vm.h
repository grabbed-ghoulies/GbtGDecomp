#pragma once

#include "../scene/scene_types.h"

#include <string_view>

namespace gbtg
{
struct Game;
// DAT_0051251C. Types 0xF and 0x10 decrement it. Type 0x11 increments it.
extern i32 g_gate_251c;
// DAT_00512518. Type 0xF stores 1 while it holds the section's end.
extern i32 g_gate_2518;
extern i32 g_script_e794; // DAT_0050E794.
extern i32 g_script_e788; // DAT_0050E788: the scene is done; 0x00117140 requests the next one.
// DAT_00545978, the broken background parts, and DAT_00545980, the running score: 0x0012AF10 and a section title
// clear them.
extern u32 g_545978;
extern u32 g_545980;
// DAT_0050E76C..DAT_0050E780: the next scene's entry position and euler; DAT_0050E784 is 1 once they are set.
extern Vec3 g_scene_entry_position;
extern Vec3 g_scene_entry_euler;
extern i32 g_scene_entry_set;
void store_scene_entry_pose(const Vec3& position, const Vec3& euler); // Xbox 0x00117580, ECX position, EAX euler

extern bool g_scene_script_started;

// The scene title and mode (DAT_00512494, DAT_0051248C, DAT_00512490) with the condition reset and decay latches.
void script_activate_title(Game& g, std::string_view title, i32 mode, i32 id); // Xbox 0x00128AB0, EAX title, ECX mode, EDX id
// The teardown's script globals: no title, the title pool, the skip input, the pause name and the fired pool.
void release_scene_script_title(Game& g); // Xbox 0x001289A0

// Seven counters, each followed by 60 counters and a 60-by-61 matrix (DAT_0052C274, DAT_00512B78).
void script_initialize_statistics(std::span<u32> statistics); // Xbox 0x0012AF10, EAX statistics

// DAT_00509B20..DAT_00509B28, the scene ambient tint 0x0011CAC0 resets to 1.
extern float g_509b20;
extern float g_509b24;
extern float g_509b28;
extern float g_509b2c;
extern float g_509b4c; // DAT_00509B4C, the directional light intensity.

// Xbox 0x00049D10.
bool scene_end_flag_set(); // Xbox 0x00049D10
extern i32 g_545cc8; // DAT_00545CC8, the DAT_00545C48 pairs.
extern i32 g_545d1c; // DAT_00545D1C, the DAT_00545CCC groups.
extern std::array<u16, 0x40> g_545c48; // DAT_00545C48.
extern std::array<u16, 0x28> g_545ccc; // DAT_00545CCC.
bool script_end_gate_set(); // Xbox 0x00123B00
bool test_and_set_runtime_story_bit(std::span<u8> bits, u32 index); // Xbox 0x000F2A20
// Bit `index` of the story block (*(DAT_00506C6C + 8)); ECX index.
bool test_runtime_story_bit(std::span<const u8> bits, u32 index); // Xbox 0x000F29A0
void set_runtime_story_bit(std::span<u8> bits, u32 index); // Xbox 0x000F29D0
void clear_runtime_story_bit(std::span<u8> bits, u32 index); // Xbox 0x000F29F0
// Bit `index` of the unlock block (*(DAT_00506C70 + 8)); ECX index.
bool test_runtime_unlock_bit(std::span<const u8> bits, u32 index); // Xbox 0x000F2B30

void script_vm_bind(Game& g);
int script_execute_fired_action(Game& g, u32& offset); // Xbox 0x00128A00

// Xbox 0x00117140. Drain the scene script, then request the next scene AID.
void script_tick_fired_vm(Game& g); // Xbox 0x00117140

// DAT_00545CC8

// Xbox 0x0011CAC0. Writes 1.0 (0x3f800000) to DAT_00509b2c, DAT_00509b20,
// DAT_00509b24, DAT_00509b28, and DAT_00509b4c. DAT_00509b50 becomes
// 0x3eb2b8c3. DAT_005462FC becomes 1. DAT_00509b54 becomes 0x3f32b8c3.
// DAT_00509b58 becomes 1. DAT_00509b70 becomes 0. DAT_00509b74 becomes 1.
// It then makes a fresh camera shake pool (0x000FAEE0), resets the camera view
// tunables (0x0013CBD0) and releases the music (part of 0x00116E00); 0x00124420
// is not translated. DAT_00545D1C and DAT_00545CC8 are cleared after the
// 0x000FAEE0 call.
void scene_reset_runtime_parameters(); // Xbox 0x0011CAC0

// Xbox 0x0011CB60. Start the loaded scene and run its script latch.
void script_queue_scene_start(Game& g); // Xbox 0x0011CB60

// Xbox 0x00117100, then the eval at 0x00117140. The update walk calls the
// table at 0x00314334. Opcode 0 is 0x00133070. The other updates are not
// translated, so those records are not stepped.
void script_vm_tick(Game& g, float dt); // Xbox 0x00117100

} // namespace gbtg
