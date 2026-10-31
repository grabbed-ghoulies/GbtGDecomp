#pragma once

#include "scene_types.h"

namespace gbtg
{
// DAT_00510E58. +0x80 is DAT_00510ED8. +0x100 is DAT_00510F58.
// +0x108 is DAT_00510F60. +0x124 is DAT_00510F7C. +0x128 is DAT_00510F80.
// The Xbox dword at +0x230 is the object 0x00109470 takes in EAX.
// DAT_0051108C (g_51108c) is the next symbol, kept separate. The host
// pointer at +0x230 is wider than that Xbox dword.
extern u8 g_510e58[0x230 + sizeof(void*)];

// DAT_00510ED8. 0x80-byte aid. entity.cpp keeps a private copy of 0x0011EB30.
extern char (&g_510ed8)[0x80];

// DAT_00430D0C. Cleared with DAT_00430D08 and DAT_00430D10.
extern i32 g_script_30d0c;
extern i32 g_script_30d94; // DAT_00430D94: a restart keeps DAT_00506C8C.
extern i32 g_506c8c; // DAT_00506C8C.

extern i32 g_4312e0; // DAT_004312E0, motion integration steps from 0x0011E7F0.
extern u8 g_4312c8[0x14];
extern i32 g_4e02e8; // DAT_004E02E8, the stream slot gate 0x0011E4C0 sets and 0x0011E7F0 clears. // DAT_004312C8..DAT_004312D8, the scene times 0x0012B1F0 advances.

// 0x0011EB30. EAX is the source aid. The stack dword is the mode. A first
// dword of 'aid_' clears the lead byte, then the source is copied through
// its NUL. DAT_00430D08, DAT_00430D0C, DAT_00430D10, and the byte at
// DAT_00430D14 become 0. A zero DAT_00430D94 also clears DAT_00506C8C.
// DAT_00430D94 and DAT_00430D98 become 0. The mode lands at DAT_00510F58.
// entity.cpp keeps a private copy of these addresses.
void request_scene_aid(const char* src, i32 mode);

// The player's exit, notice and camera-tune volumes, at the head of 0x00117100.
void scene_update_actor_marker(Game& g); // Xbox 0x00140E30

} // namespace gbtg
