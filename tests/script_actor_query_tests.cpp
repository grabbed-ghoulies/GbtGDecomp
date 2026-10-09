#include "../src/engine/scripts/script_actor_query.h"
#include <array>
#include <limits>
#include <stdexcept>

namespace
{
using namespace gbtg;
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
Actor* accepted_actor;
i32 predicate_calls;
u32 accept_selected(Actor* requester, Actor* candidate)
{
    require(requester == nullptr, "tag predicate requester must be null");
    ++predicate_calls;
    return candidate == accepted_actor;
}
void test_query_flags()
{
    Actor actor, requester, case_actor;
    scene_detail::CamFocus host;
    u16 tag = 42;
    std::array<u8, 8> bytes{};
    EntityBitBlock bits{bytes.data()};
    RuntimeScriptMindQuery mind{};
    mind.bits = &bits;
    RuntimeScriptActorQueryView view{actor, tag, &mind, nullptr, nullptr};
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> views;
    std::array<u8, 2> zero{0x34, 0x12};
    RuntimeScriptActorQueryContext context{nullptr, views, zero.data()};
    auto query = [&](u32 flags)
    {
        return matches_runtime_script_actor_query(view, &requester, flags, context) != 0;
    };
    require(query(0), "zero mask accepts distinct actors");
    require(matches_runtime_script_actor_query(view, &actor, 0, context) == 0, "self exclusion");
    bytes[7] = 0x10;
    require(!query(0x20000), "mind byte7 exclusion");
    bytes[7] = 0;
    require(query(0x20000), "mind byte7 clear");
    mind.maximum_energy = 1;
    require(!query(0x40), "zero energy with nonzero maximum excluded");
    mind.energy = 1;
    require(query(0x40), "nonzero energy accepted");
    mind.energy = mind.maximum_energy = 0;
    require(query(0x40), "zero maximum accepted");
    require(!query(4), "required bit3.5 absent");
    bytes[3] = 0x20;
    require(query(4), "required bit3.5 present");
    bytes[3] = 0;
    mind.blocked = 1;
    require(!query(2), "blocked mind excluded");
    mind.blocked = 0;
    require(query(2), "unblocked mind accepted");
    actor.mask_mode = 2;
    view.marker = &host;
    require(!query(8) && query(0x2000), "active marker exclusion/inclusion masks");
    view.marker = nullptr;
    require(query(8) && !query(0x2000), "missing marker exclusion/inclusion masks");
    require(!query(0x80), "case actor required");
    mind.case_actor = &case_actor;
    require(query(0x80), "case actor present");
    ActorBind bind;
    bind.parent = &requester;
    actor.bind = &bind;
    require(!query(0x100), "requester ancestor excluded");
    actor.bind = nullptr;
    require(query(0x100), "no requester ancestor accepted");
    actor.mask_mode = 1;
    require(query(0x200), "mode1 accepted");
    actor.mask_mode = 5;
    require(!query(0x200) && query(0x201), "mode5 requires low bit");
    actor.mask_mode = 2;
    require(!query(0x201), "other modes excluded even with low bit");
    RuntimeScriptActorQueryBinding owner{&requester};
    RuntimeScriptActorQueryReference reference{&requester, &owner};
    view.reference = &reference;
    require(query(0x400), "matching reference binding owner");
    owner.owner = &case_actor;
    require(!query(0x400), "mismatched reference binding owner");
    owner.owner = &requester;
    require(matches_runtime_script_actor_query(view, nullptr, 0x400, context) == 0, "owner mask requires requester");
    require(query(0x800), "idle mind accepted");
    mind.suspended = 1;
    require(!query(0x800), "nonidle inactive mind excluded");
    mind.active = 1;
    require(query(0x800), "active mind bypasses idle rejection");
    mind.suspended = mind.active = 0;
    actor.mask_mode = 1;
    actor.mask_flag = 0;
    require(!query(0x1000), "inactive actor excluded");
    actor.mask_mode = 2;
    require(query(0x1000), "mode2 active actor accepted");
    actor.mask_mode = 1;
    actor.mask_flag = 7;
    require(query(0x1000), "live mask actor accepted");
    actor.mask_flag = 0;
    case_actor.mask_flag = 1 << 24;
    require(!query(0x8000), "unavailable case actor excluded");
    case_actor.mask_flag = 0;
    require(query(0x8000), "available case actor accepted");
    mind.case_actor = nullptr;
    require(query(0x8000), "missing case actor does not trigger unavailable filter");
    bytes[5] = 1;
    require(!query(0x10000), "mind bit28 excluded");
    bytes[5] = 0;
    require(query(0x10000), "mind bit28 clear");
    require(query(0x40000), "matching reference source accepted");
    reference.source = &case_actor;
    require(!query(0x40000), "mismatched reference source excluded");
    require(matches_runtime_script_actor_query(view, nullptr, 0x40000, context) != 0, "source filter skipped without requester");
    require(query(0x80000), "missing maskspan reports zero");
    std::array<u8, 4> mask_bytes{0, 0, 1, 0};
    EntityMaskSpan span{2, 0, 1, 1, mask_bytes.data()};
    actor.mask_span = &span;
    require(!query(0x80000), "current mask nonzero excluded");
    span.index = 0;
    require(query(0x80000), "current mask zero accepted");
    span.end = span.mark;
    require(script_actor_current_mask(actor, context) == 0x1234, "empty maskspan reads actual address-zero view");
    actor.mask_span = nullptr;
    require(!query(0x100000), "active mind node required");
    RuntimeScriptMindQueryNode node{};
    mind.node = &node;
    require(!query(0x100000), "node active field required");
    node.active = &host;
    require(query(0x100000), "active mind node accepted");
}
void test_idle_and_ancestry()
{
    std::array<u8, 8> bytes{};
    EntityBitBlock bits{bytes.data()};
    RuntimeScriptMindQuery mind{};
    mind.bits = &bits;
    require(script_mind_is_idle(mind, 0), "clear mind idle");
    mind.active = mind.permitted = 1;
    require(!script_mind_is_idle(mind, 0) && script_mind_is_idle(mind, 1), "permitted active branch");
    mind.active = mind.permitted = 0;
    mind.timer = 1.f;
    require(!script_mind_is_idle(mind, 0), "positive mind timer");
    mind.timer = std::numeric_limits<float>::quiet_NaN();
    require(script_mind_is_idle(mind, 0), "unordered mind timer preserves COMISS/JA branch");
    mind.timer = 0.f;
    mind.state = 1;
    require(!script_mind_is_idle(mind, 0), "mind state");
    mind.state = 0;
    mind.state_time = 1.f;
    require(!script_mind_is_idle(mind, 0), "mind state time");
    mind.state_time = std::numeric_limits<float>::quiet_NaN();
    require(!script_mind_is_idle(mind, 0), "unordered mind state time preserves UCOMISS parity branch");
    mind.state_time = 0.f;
    bytes[3] = 8;
    require(!script_mind_is_idle(mind, 0), "mind status bit3.3");
    bytes[3] = 0;
    bytes[5] = 1 << 4;
    require(!script_mind_is_idle(mind, 0), "mind bit2c");
    bytes[5] = 1 << 2;
    require(!script_mind_is_idle(mind, 0), "mind bit2a");
    Actor actor, parent, unrelated;
    ActorBind bind;
    bind.parent = &parent;
    actor.bind = &bind;
    require(script_actor_has_ancestor(&actor, &parent), "bound parent ancestry");
    require(!script_actor_has_ancestor(&actor, &unrelated), "unrelated ancestry");
    require(script_actor_has_ancestor(&actor, &actor), "self ancestry");
    ActorLink link;
    link.head = &parent;
    link.owner = &parent;
    bind.parent = nullptr;
    actor.link = &link;
    require(script_actor_has_ancestor(&actor, &parent), "link owner ancestry branch");
}
void test_tag_count()
{
    Actor first_actor, second_actor, other_actor;
    scene_detail::CamFocus first_host, second_host, other_host;
    first_host.tag_6 = second_host.tag_6 = 77;
    other_host.tag_6 = 78;
    scene_detail::RangeLink third{nullptr, &other_host};
    scene_detail::RangeLink second{&third, &second_host};
    scene_detail::RangeLink first{&second, &first_host};
    std::array<u8, 8> bytes{};
    EntityBitBlock bits{bytes.data()};
    RuntimeScriptMindQuery mind{};
    mind.bits = &bits;
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> views;
    views.emplace(&first_host, RuntimeScriptActorQueryView{first_actor, first_host.tag_6, &mind, nullptr, nullptr});
    views.emplace(&second_host, RuntimeScriptActorQueryView{second_actor, second_host.tag_6, &mind, nullptr, nullptr});
    views.emplace(&other_host, RuntimeScriptActorQueryView{other_actor, other_host.tag_6, &mind, nullptr, nullptr});
    const std::array<u8, 2> zero{};
    RuntimeScriptActorQueryContext context{&first, views, zero.data()};
    require(count_runtime_script_actor_tags(0, 77, nullptr, context) == 2, "matching tag count");
    require(count_runtime_script_actor_tags(0, 78, nullptr, context) == 1, "other tag count");
    require(count_runtime_script_actor_tags(0, 79, nullptr, context) == 0, "missing tag count");
    first_actor.mask_mode = 2;
    require(count_runtime_script_actor_tags(0x1000, 77, nullptr, context) == 1, "hidden flags reach full filter");
    accepted_actor = &second_actor;
    predicate_calls = 0;
    require(count_runtime_script_actor_tags(0, 77, accept_selected, context) == 1, "genuine callback controls acceptance");
    require(predicate_calls == 2, "predicate only invoked for matching tag and filter");
    context.first = nullptr;
    require(count_runtime_script_actor_tags(0, 77, nullptr, context) == 0, "empty actor list count");
}
}
void check_script_actor_query()
{
    test_query_flags();
    test_idle_and_ancestry();
    test_tag_count();
}
