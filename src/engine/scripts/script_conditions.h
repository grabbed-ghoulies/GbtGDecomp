#pragma once

#include "../scene/scene_types.h"
#include <functional>
#include <span>

namespace gbtg
{
extern std::vector<ScriptCond> g_script_conds;

extern std::vector<u32> g_script_fired;

extern LiveScript*& g_live_scripts;
// The body (node +0x18, actor +0x95C) of a DAT_00511B48 node (actor +0x944), bound by the scene's actor services.
extern std::function<std::span<const u8>(const void* node)> g_alt_node_body;
// DAT_00511B48 nodes whose body +0x94 (health) is nonzero.
i32 count_runtime_live_actors(); // Xbox 0x00129210
const u32* find_runtime_script_event_type(u32 type); // Xbox 0x00116890
void release_runtime_script_events(u32 type); // Xbox 0x001168C0, EDI type: drops every queued event of the type

// Xbox 0x00116810. Heap allocation is the 0x000F72D0 boundary; the linked
// insertion and message copy retain the original ownership and ordering.
// Xbox 0x00116810
void script_queue_event(Game& g, std::span<const u8> message); // Xbox 0x00116810

u32& scene_word(u32 addr);

u32 cond_u32(const ScriptCond& rec, u32 at);

void cond_put_u32(ScriptCond& rec, u32 at, u32 v);

float cond_f32(const ScriptCond& rec, u32 at);

void cond_put_f32(ScriptCond& rec, u32 at, float v);

u32 node_u32(const SceneScript& script, const ScriptNode& node, u32 rel);

const ScriptNode* script_node_at(const SceneScript& script, u32 offset);

void log_script_nodes(const SceneScript& script);

void script_vm_reset();

// Xbox 0x00124C80. Grows DAT_0051234C by the 0x20 stride and copies `size` bytes.
void script_append_condition(const ScriptCond& src, u32 size, u32 opcode); // Xbox 0x00124C80

// Xbox 0x000E9210. EAX is the DAT_0050E4DC node. A non-null prev stores next
// at prev+4. A non-null next stores prev at next+0. Both links are then
// cleared. The tail matches 0x000663E0: 0x000F7100 selects the heap and
// 0x0014D360 frees the node.
// Xbox 0x000E9210
void script_release_event(LiveScript* node); // Xbox 0x000E9210

extern SceneFlagBlock* g_scene_flags;

// Xbox 0x00124BE0. A zero keeps the record. Any other value queues the link
// at +4. A return of exactly 1 also removes the record and the slot that
// slides down is evaluated next. Opcode 0 is 0x00132650. Opcode 1 is
// 0x00132680. Opcode 2 is 0x001326D0. Opcode 3 is 0x00132720. Opcode 4 is
// 0x001327A0. Opcode 5 is 0x00132800. Opcode 6 is 0x00132840. Opcode 7 is
// 0x001328D0. Opcode 8 is 0x00132940. Opcode 9 is 0x001329A0. Opcode 10 is
// 0x00132A30. Opcode 11 is 0x00132A00. Opcode 12 is 0x00132A60. Opcode 13 is
// 0x00132BC0. Opcode 14 is 0x00132BF0. Opcode 15 is 0x00132D00. Opcode 16 is
// 0x00132C70. Opcode 17 is 0x00132AB0. Opcode 18 is 0x00132DB0. Opcode 19 is
// 0x00132E00. Opcode 20 is 0x00132E90. Opcode 21 is 0x00132ED0. Opcode 22 is
// 0x00132F00. Opcode 23 is 0x00132F30. Opcode 24 is 0x00132FA0. Opcode 25 is
// 0x00132FF0. Opcode 26 is 0x00132B50.
// Xbox 0x00124BE0
void script_evaluate_conditions(Game& g); // Xbox 0x00124BE0

u8 node_u8(const SceneScript& script, const ScriptNode& node, u32 rel);

u16 node_u16(const SceneScript& script, const ScriptNode& node, u32 rel);

} // namespace gbtg
