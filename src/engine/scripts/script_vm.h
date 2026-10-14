#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
// DAT_0051251C. Types 0xF and 0x10 decrement it. Type 0x11 increments it.
extern i32 g_gate_251c;

extern bool g_scene_script_started;

// DAT_00509B20..DAT_00509B28, the scene ambient tint 0x0011CAC0 resets to 1.
extern float g_509b20;
extern float g_509b24;
extern float g_509b28;

// Xbox 0x00049D10.
bool scene_end_flag_set(); // Xbox 0x00049D10
bool test_and_set_runtime_story_bit(std::span<u8> bits, u32 index); // Xbox 0x000F2A20

void script_vm_bind(Game& g);
int script_execute_fired_action(Game& g, u32& offset); // Xbox 0x00128A00

// Xbox 0x00117140. Drain the scene script, then request the next scene AID.
void script_tick_fired_vm(Game& g); // Xbox 0x00117140

// DAT_00545CC8

// Xbox 0x0011CAC0. Writes 1.0 (0x3f800000) to DAT_00509b2c, DAT_00509b20,
// DAT_00509b24, DAT_00509b28, and DAT_00509b4c. DAT_00509b50 becomes
// 0x3eb2b8c3. DAT_005462FC becomes 1. DAT_00509b54 becomes 0x3f32b8c3.
// DAT_00509b58 becomes 1. DAT_00509b70 becomes 0. DAT_00509b74 becomes 1.
// 0x000FAEE0, 0x0013CBD0, 0x00116E00, and 0x00124420 are not translated, so
// those calls are not made. DAT_00545D1C and DAT_00545CC8 are cleared after
// the 0x000FAEE0 call.
// Xbox 0x0011CAC0
void scene_reset_runtime_parameters(); // Xbox 0x0011CAC0

// Xbox 0x0011CB60. Start the loaded scene and run its script latch.
void script_queue_scene_start(Game& g); // Xbox 0x0011CB60

// Xbox 0x00117100, then the eval at 0x00117140. The update walk calls the
// table at 0x00314334. Opcode 0 is 0x00133070. The other updates are not
// translated, so those records are not stepped.
void script_vm_tick(Game& g, float dt); // Xbox 0x00117100

} // namespace gbtg
