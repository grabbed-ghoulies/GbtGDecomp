#pragma once

#include "../scene/scene_types.h"

namespace gbtg
{
// DAT_00546328. Scene-order slot. Its address is the handler argument.
extern i32 g_546328;

// DAT_0054632C. 1 runs the countdown and the handler. 3 runs the handler.
// A nonzero value is the clear gate.
extern i32 g_54632c;

// DAT_0050484C. Incremented when it is below 1 and DAT_00546344 is above 0.
extern i32 g_50484c;

// DAT_0050E7A0. Counted when the scene state leaves or enters 1 or 2.
// entity.cpp keeps a private copy as g_amb_counter.
extern i32 g_50e7a0;

// DAT_00510CEC. Counted up when DAT_0054632C is 0 on entry to 0x0012C5D0.
extern i32 g_510cec;

// DAT_004DEA24. Counted with that gate, and once more on the way out.
// gfx keeps the same address as present_insert.hold.
extern i32 g_4dea24;

// DAT_00510F80. Scene record +0x128. Cleared when the phase is 1.
extern i32& g_510f80;

// DAT_00510F7C. Scene record +0x124. Cleared when the phase is 3.
extern i32& g_510f7c;

// DAT_00546330 and DAT_00546334. Both receive the dword at slot+0x104.
extern u32 g_546330;

extern u32 g_546334;

// DAT_00546340. Address of slot+4 when that dword is 'aid_', otherwise null.
extern const char* g_546340;

// DAT_00507DF4. 0x0011CE40 stores 0.
extern i32 g_507df4;

// DAT_00510CE8. Nonzero clears bit 0x20 of DAT_0050EA9C, then stores 0.
extern i32 g_510ce8;

// DAT_00510E40. Nonzero is EBX for 0x00035C00, then stores 0.
extern void* g_510e40;

// DAT_00510E34. Nonzero decrements DAT_004DEA24, then stores 0.
extern i32 g_510e34;

// Xbox 0x0012C340
void transition_tick_active(); // Xbox 0x0012C340

// Xbox 0x0012C4E0
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +8 (DAT_003129B4) with &DAT_00546328. The image
// slots are 0x001628B0, 0x0012B5A0, 0x0012C1A0, and 0x001628B0. Those handlers
// are not translated. EAX is then DAT_0054937C for 0x00048C70.
// Phase 0 increments DAT_00510CEC and DAT_004DEA24. Phase 1 clears
// DAT_00510F80. Phase 3 clears DAT_00510F7C. Any other phase falls through.
// DAT_00510CEC and DAT_004DEA24 decrement. Bit 0x100 of DAT_0050EA9C clears.
// DAT_0054632C becomes 0. A nonzero DAT_0054634C would call 0x0005F460. That
// call is not translated. Bit 0x2000 then clears and the pointer becomes 0.
// DAT_004DEA24 decrements again.
// Xbox 0x0012C4E0
void transition_dispatch_handler(); // Xbox 0x0012C4E0

// Xbox 0x0012C5D0
// A nonzero DAT_0054632C calls 0x0012C4E0 before the phase test. Phase 0
// increments DAT_00510CEC and DAT_004DEA24 and ORs 0x100 into DAT_0050EA9C.
// Phase 1 clears DAT_00510F80. Phase 3 clears DAT_00510F7C. Any other phase
// falls through. DAT_0054632C becomes 1.
//
// 0x0012C2D0 is not translated. ESI would be DAT_0054633C and EDI DAT_00546338.
// EAX would replace DAT_00546328. That return is not stored. The slot already
// in DAT_00546328 is scaled by 0x110. The dword at +0x104 is copied to
// DAT_00546334 and DAT_00546330. DAT_00546344 becomes 0. DAT_00546348 becomes
// 0xf. DAT_00546340 is the address of slot+4 when that dword is 'aid_'
// (0x5f646961), and null when it is not.
//
// 0x000496C0 is not translated. EDX would be this node, EAX the string
// "transition", and CL 0. That call would copy the name to +0x34. The node
// is inserted after the slot-0 record whose prev is null. A null DAT_004E16A0
// would then call camera_bind_named_view with slot 0 and that name. The name is
// written by 0x000496C0, and entity.cpp keeps the live slot, so that call is
// not made. 0x00048F10 is not translated. 0x0005F390 is not translated: the
// stack is 0, 1, 0x80 and ESI is DAT_0054634C. 0x000FF0A0 is not translated:
// the stack is aid_loctext_ghoulies_gameselect_gameselecttext, "gameselect",
// "loading", and DAT_00546350.
//
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +0xC (DAT_003129B8) with &DAT_00546328. Those
// handlers are not translated. DAT_004DEA24 increments on the way out.
// Xbox 0x0012C5D0
void transition_tick_queue(); // Xbox 0x0012C5D0

// Xbox 0x0012C770
// Phase 0 increments DAT_00510CEC and DAT_004DEA24 and ORs 0x100 into
// DAT_0050EA9C. Phase 1 clears DAT_00510F80. Phase 3 clears DAT_00510F7C.
// Any other phase falls through. The slot in DAT_00546328 is scaled by 0x110.
// The dword at +0x104 is copied to DAT_00546330, then DAT_0054632C becomes 3.
// DAT_0050484C decrements when it is above 0. DAT_00546340 is the address of
// slot+0x84 when that dword is 'aid_' (0x5f646961), and null when it is not.
// DAT_00546334 then receives DAT_00546330.
//
// The handler dword at slot+0 selects the 0x18-byte record at DAT_003129AC.
// The call is the pointer at +0x10 (DAT_003129BC) with &DAT_00546328. The
// image slots are 0x0012BD50, 0x0012B790, 0x0012C270, and 0x001628B0. Those
// handlers are not translated.
// Xbox 0x0012C770
void transition_finish_pending(); // Xbox 0x0012C770

} // namespace gbtg
