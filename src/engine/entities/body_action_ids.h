#pragma once
#include "body_action_selection.h"

namespace gbtg
{
struct RuntimePlayerGoalRing;
u16 runtime_body_action_name_id(const char* name,
    const char* const* names); // Xbox 0x00026930, 0x174 sorted names.
u32 runtime_player_goal_flag_name_id(const char* name,
    const char* const* names); // Xbox 0x00028810, 3 sorted names.
bool runtime_body_timer_is_idle(std::span<const u8> body); // Xbox 0x00024C20
u16 classify_runtime_attached_actor(std::span<const u8> attached, u16 tag); // Xbox 0x00145FF0
u16 classify_runtime_attached_owner(std::span<const u8> attached,
    const Actor* owner); // Xbox 0x00146030
bool runtime_body_has_action_id(const RuntimeScriptLookupTable* table, u16 key); // Xbox 0x00027620
bool runtime_attached_actor_allows_range(const RuntimeBodyAttachedActor& attached); // Xbox 0x001460C0
struct RuntimeBodyActionIds
{
    RuntimeBodyAttachedActor* attached; // Body +0x48.
    u16 tag; // Owner +6.
    u32 body_address;
};
u16 runtime_player_body_primary_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& flags, const RuntimeScriptLookupTable* table,
    const RuntimePlayerGoalRing* goals, std::span<const u8> address_zero,
    const std::function<u16()>& primary_id, u16 retained_key); // Xbox 0x00075B90
u32 runtime_body_primary_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00026B00
u32 runtime_body_secondary_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00026BB0
u32 runtime_body_contact_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00027220
u32 runtime_body_effect_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00027320
u32 runtime_body_damage_action_id(const RuntimeBodyActionIds& context); // Xbox 0x000273D0
u32 runtime_body_attachment_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00027480
u32 runtime_body_idle_action_id(const RuntimeBodyActionIds& context); // Xbox 0x00026A70
u32 runtime_body_range_action_id(const RuntimeBodyActionIds& context); // Xbox 0x000272D0
void runtime_body_primary_action_pair(const RuntimeBodyActionIds& context,
    u16& key, u16& fallback); // Xbox 0x00027060
void runtime_body_secondary_action_pair(const RuntimeBodyActionIds& context,
    u16& key, u16& fallback); // Xbox 0x00027140
u16 runtime_body_primary_gated_id(const RuntimeBodyActionIds& context,
    const RuntimeScriptLookupTable* table); // Xbox 0x000269C0
u16 runtime_body_effect_gated_id(const RuntimeBodyActionIds& context,
    const RuntimeScriptLookupTable* table); // Xbox 0x00026DE0
u16 runtime_body_contact_gated_id(const RuntimeBodyActionIds& context,
    const RuntimeScriptLookupTable* table); // Xbox 0x00026EC0
u16 runtime_body_damage_gated_id(const RuntimeBodyActionIds& context,
    const RuntimeScriptLookupTable* table); // Xbox 0x00026F90
u16 runtime_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00026950
u16 runtime_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00026A90
u16 runtime_player_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00075C30
u16 runtime_zombie_body_primary_parameter_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00076210
u16 runtime_zombie_body_secondary_parameter_id(std::span<const u8> body, const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00076250
// entityBodyJessieClyde (type 0x29): 2 (or 3) while the body's bit block byte 1 has its top bit, else the parent's.
u16 runtime_jessie_clyde_body_primary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00075A70
u16 runtime_jessie_clyde_body_secondary_parameter_id(const RuntimeBodyActionIds& context,
    const u8* parameters, const EntityBitBlock& bits,
    const RuntimeScriptLookupTable* table, u16 retained_key); // Xbox 0x00075AA0
u16 runtime_body_death_action_key(); // Xbox 0x00027500
u16 runtime_body_near_action_id(std::span<const u8> body, Actor& owner,
    RuntimeBodyAttachedActor& attached, const RuntimeScriptLookupTable* table); // Xbox 0x00026C60
}
