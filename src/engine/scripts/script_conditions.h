#pragma once
#include <optional>

#include "../scene/scene_types.h"
#include <functional>
#include <span>

namespace gbtg
{
extern std::vector<ScriptCond> g_script_conds;

extern std::vector<u32> g_script_fired;

void reset_scene_spawn_words(); // Xbox 0x00116DA0
extern LiveScript g_live_anchor; // DAT_0050E4D8: its next is DAT_0050E4DC.
extern LiveScript*& g_live_scripts;
// The body (node +0x18, actor +0x95C) of a DAT_00511B48 node (actor +0x944), bound by the scene's actor services.
extern std::function<std::span<const u8>(const void* node)> g_alt_node_body;
// The class key (node - 0x93E, actor +6) of a DAT_00511B48 node, bound with g_alt_node_body.
extern std::function<u16(const void* node)> g_alt_node_class;
// The actor (node - 0x944) of a DAT_00511B48 node, with its bytes; bound with g_alt_node_body.
extern std::function<Actor&(const void* node)> g_alt_node_actor;
extern std::function<std::span<u8>(const void* node)> g_alt_node_actor_bytes;
// The mind (node +0x14, actor +0x958) of a DAT_00511B48 node; empty for an actor without one.
extern std::function<std::span<u8>(const void* node)> g_alt_node_mind;
// 0x00029C80 (ECX the node's mind, EAX its +0x10 target record, EDX 1) with that player's views.
extern std::function<bool(const void* node)> g_alt_node_target_eligible;
// The node's actor vtable +0xC (mind +8).
extern std::function<void(const void* node, std::span<const u32> message)> g_alt_node_message;
// Appends a tag to DAT_0050E22C (count DAT_0050E228), the classes the player's targeting skips.
extern std::function<void(u16 tag)> g_script_exclude_actor_tag;
// The DAT_00511AA4 object whose +0x1FC word is the key, as its retail address (0 for none), and any scene object's
// vtable +0xC by address; bound by the scene's actor services.
extern std::function<u32(u16 key)> g_script_actor_by_id;
extern std::function<void(u32 address, std::span<u32> message)> g_script_object_message;
// The DAT_00511B40 player's world position when it is set and its body (+0x95C) +0x94 is nonzero; and 0x0005BEC0 over
// the scene's doors. Bound by the scene's actor services.
extern std::function<bool(Vec3& position)> g_script_live_player_position;
extern std::function<bool(u16 door, const Vec3& from, float& distance)> g_script_door_distance;
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
// The statistics block from DAT_00512B78 (0x0012AE40 / 0x0012AF10) by retail address.
i32& script_statistic_at_address(u32 addr);
// The ten statistics rows' kill counts of a category, summed.
int script_sum_statistics(i32 category); // Xbox 0x0012AE40, ECX category
int script_sum_paired_statistics(i32 category); // Xbox 0x0012AE90, ECX category
// The DAT_00511B48 actors with body +0x94 energy, of class `key` (any class for 0).
int script_count_actors_by_class(u16 key); // Xbox 0x00129240, DX key
// A live 0xB6 event on DAT_0050E4DC whose word at +0x14 is `key` (any for a key at or below 1).
int script_has_tagged_event(u16 key); // Xbox 0x001168F0, DX key

void cond_put_u32(ScriptCond& rec, u32 at, u32 v);

float cond_f32(const ScriptCond& rec, u32 at);

void cond_put_f32(ScriptCond& rec, u32 at, float v);

u32 node_u32(const SceneScript& script, const ScriptNode& node, u32 rel);

// The node at `offset` (`node + *(u32*)node` walks), or none when its header lies outside the resource.
std::optional<ScriptNode> script_node_at(const SceneScript& script, u32 offset);

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
void release_script_events(); // Xbox 0x00116790, every DAT_0050E4DC node

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
