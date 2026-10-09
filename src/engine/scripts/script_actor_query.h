#pragma once
#include "gbtg/entity.h"
#include "../scene/scene_types.h"
#include <unordered_map>

namespace gbtg
{
struct RuntimeScriptMindQueryNode
{
    void* active; // mind+0x5C -> node+0x2C
};
struct RuntimeScriptMindQuery
{
    i16 state; // +0x30
    float state_time; // +0x34
    Actor* case_actor; // +0x48
    RuntimeScriptMindQueryNode* node; // +0x5C
    u8 blocked; // +0x69
    u8 active; // +0x6A
    u8 suspended; // +0x6B
    u8 permitted; // +0x6C
    float timer; // +0x70
    i16 energy; // +0x94
    i16 maximum_energy; // +0x96
    EntityBitBlock* bits; // +0x15C
};
struct RuntimeScriptActorQueryBinding
{
    Actor* owner; // reference+0x10 -> binding+0x10
};
struct RuntimeScriptActorQueryReference
{
    Actor* source; // actor+0x958 -> reference+0xC
    RuntimeScriptActorQueryBinding* binding; // reference+0x10
};
struct RuntimeScriptActorQueryView
{
    Actor& actor;
    u16& tag; // object+6, native identity shared with RangeLink.host->tag_6
    RuntimeScriptMindQuery* mind; // actor+0x95C
    RuntimeScriptActorQueryReference* reference; // actor+0x958
    void* marker; // actor+0xCE0
};
using RuntimeScriptActorPredicate = u32 (*)(Actor* requester, Actor* candidate);
struct RuntimeScriptActorQueryContext
{
    scene_detail::RangeLink* first; // DAT_00511B38
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView>& views;
    const u8* address_zero; // Native memory view for the dump's literal address-zero u16 read.
};
bool script_actor_has_active_marker(const RuntimeScriptActorQueryView& actor); // Xbox 0x00028D20
bool script_actor_is_active(const Actor& actor); // Xbox 0x00028D00
Actor* script_actor_reference_source(const RuntimeScriptActorQueryView& actor); // Xbox 0x00028C10
bool script_actor_has_ancestor(Actor* actor, Actor* ancestor); // Xbox 0x0003B910
bool script_mind_test_bit(const RuntimeScriptMindQuery& mind, u32 bit); // Xbox 0x0001EF10
bool script_mind_is_idle(const RuntimeScriptMindQuery& mind, i32 allow_permitted); // Xbox 0x0001D440
u16 script_actor_current_mask(const Actor& actor,
    const RuntimeScriptActorQueryContext& context); // Xbox 0x00028360
u32 matches_runtime_script_actor_query(RuntimeScriptActorQueryView& candidate,
    Actor* requester, u32 flags, const RuntimeScriptActorQueryContext& context); // Xbox 0x00129420
i32 count_runtime_script_actor_tags(u32 flags, i16 tag, RuntimeScriptActorPredicate predicate,
    RuntimeScriptActorQueryContext& context); // Xbox 0x001291A0
}
