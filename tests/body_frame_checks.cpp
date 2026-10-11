#include "body_frame_checks.h"
#include "../src/engine/entities/body_apply.h"
#include "../src/engine/entities/player_body_motion.h"
#include "../src/engine/entities/entity_heap.h"
#include "../src/engine/entities/body_shape_push.h"
#include "../src/engine/entities/body_message_helpers.h"
#include "../src/engine/entities/body_action_ids.h"
#include "../src/engine/entities/body_contacts.h"
#include "../src/engine/entities/body_animation_commands.h"
#include "../src/engine/entities/entity_collision.h"
#include "../src/engine/entities/entity.h"
#include <bit>
#include <cstring>

namespace
{
using namespace gbtg;
void require(bool value, const char* message)
{
    if (!value)
    {
        throw Error(message);
    }
}
void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void floating(std::span<u8> bytes, size_t offset, float value)
{
    put(bytes, offset, std::bit_cast<u32>(value));
}
}

void check_body_frame_methods()
{
    using namespace gbtg;
    std::array<u8, 0x924> body{};
    std::array<u8, 0xE00> actor_storage{};
    std::span<u8> actor_bytes = actor_storage;
    Actor actor;
    actor.animation_rate = 1.f;
    float freeze = 0.f, dt = 0.1f, scale = 1.f;
    u32 block = 0, index = 0;
    std::array<u8, 1> enabled{1};
    RuntimeActorClock clock{freeze, dt, scale, block, enabled, index};
    RuntimeModelInstance instance{};
    RuntimeModelInstance* model = &instance;
    RuntimeModelInstance* secondary = nullptr;
    std::array<u8, 24> row_storage{};
    std::span<u8> rows = row_storage;
    put(body, 0x154, 2);
    put(rows, 0, 0);
    put(rows, 12, 1);
    floating(rows, 4, 0.4f);
    floating(rows, 16, 0.7f);
    float first_target = 0.9f, second_target = 0.2f;
    instance.auxiliary_targets[0] = &first_target;
    instance.auxiliary_targets[1] = &second_target;
    std::array<u32, 2> binding_words{0, 1};
    RuntimeBodyChannelBindings::Asset asset{binding_words.data()};
    RuntimeBodyChannelBindings::Resource resource{&asset};
    std::array<RuntimeBodyChannelBindings::Row, 1> binding_rows{{{&resource}}};
    RuntimeBodyChannelBindings binding{binding_rows};
    RuntimeBodyChannelBindings* animation = &binding;
    RuntimeBodyChannelContext channels{actor, clock, animation, model, rows, {}, {}, {}};
    apply_runtime_body_channels(body, channels);
    require(read_f32(rows, 4) == first_target && read_f32(rows, 16) == second_target,
        "one bound animation channel must capture all model targets instead of overwriting any");
    animation = nullptr;
    floating(rows, 4, 0.4f);
    floating(rows, 16, 0.7f);
    put(body, 0xFC, 1);
    body[0xEC] = 1;
    apply_runtime_body_channels(body, channels);
    require(first_target == 0.9f && second_target == 0.7f,
        "active default channel must merge channel zero and still directly write other channels");
    floating(rows, 4, std::bit_cast<float>(0x7FC00001u));
    apply_runtime_body_channels(body, channels);
    require(first_target == 0.9f, "unordered merge must preserve existing channel target");
    floating(body, 0xE8, 0.3f);
    floating(body, 0xF4, 0.6f);
    apply_runtime_body_default_blend(body, channels);
    require(first_target == 0.6f, "default blend must make both writes to the same model channel");
    std::array<u8, 8> bank{};
    put(bank, 0, 1);
    floating(bank, 4, 1.f);
    channels.banks[0] = bank;
    floating(rows, 4, 0.4f);
    floating(rows, 16, 0.7f);
    ease_runtime_body_channel_targets(body, channels);
    require(read_f32(rows, 4) < 0.4f && read_f32(rows, 16) > 0.7f
        && read_u32(rows, 8) == 0 && read_u32(rows, 20) == 0,
        "channel bank must drive selected id and clear all targets after easing");
    std::array<u8, 36> keys{};
    floating(keys, 4, 0.1f);
    floating(keys, 12, 0.2f);
    floating(keys, 16, 0.1f);
    put(keys, 20, 1);
    put(keys, 32, 2);
    channels.keys[1] = keys;
    channels.keys[2] = keys;
    body[0x90] = 1;
    body[0x91] = 1;
    floating(body, 0x8C, 0.1f);
    tick_runtime_body_channel_keys(body, channels);
    require(body[0x90] == 2 && body[0x91] == 0 && body[0x92] == 2 && body[0xF8] == 1,
        "terminal key must switch mode one to actual mode two and still mark selected channel");
    put(body, 0xFC, 0);
    RuntimeBodyChannelChild child;
    RuntimeBodyChannelChild* attachment = &child;
    bool child_called = false;
    child.step = [&]()
    {
        child_called = true;
        put(body, 0x154, 0);
    };
    floating(rows, 4, 0.4f);
    step_runtime_body_channels(body, attachment, channels);
    require(child_called && read_f32(rows, 4) == 0.4f,
        "body channel step must reread row count after child virtual changes it");

    std::array<u8, 8> flags_storage{};
    EntityBitBlock flags{flags_storage.data()};
    std::span<u8> bound;
    GhoulySpawnPool* shapes = nullptr;
    entity_detail::ListHead anchors{};
    entity_detail::LatchNode* first = nullptr;
    RuntimeBodyTimerRecipient first_timer, second_timer;
    RuntimeBodyTimerRecipient* first_recipient = &first_timer;
    RuntimeBodyTimerRecipient* second_recipient = &second_timer;
    const u8* actor_bound = nullptr;
    std::array<u32, 48> states{};
    RuntimeBodyChannelChild action_child;
    RuntimeBodyChannelChild* action = &action_child;
    RuntimeBodyApplyContext apply{channels, actor_bytes, anchors, flags, bound, shapes, first,
        first_recipient, second_recipient, actor_bound, secondary, states, action};
    first_timer.message = [&](std::span<const u32> message)
    {
        require(message.size() == 2 && message[0] == 5 && message[1] == 0,
            "first timer must send the original two-word expiry message");
        body[0x7E4] = 1;
        floating(body, 0x7E8, 0.1f);
    };
    bool second_called = false, action_called = false;
    second_timer.message = [&](std::span<const u32> message)
    {
        require(message[0] == 5 && body[0x7E4] == 1,
            "second expiry must run before latch owner flag is cleared");
        second_called = true;
    };
    action_child.step = [&]()
    {
        require(body[0x7E4] == 0 && read_u16(body, 0x880) == 0 && body[0x87E] == 0,
            "action apply must follow countdown, timer expiry and owner flag cleanup");
        action_called = true;
    };
    floating(body, 0x758, 0.1f);
    body[0x880] = 1;
    body[0x87E] = 1;
    body[0x308] = 1;
    for (size_t offset = 0x260; offset < 0x266; ++offset)
    {
        body[offset] = 1;
    }
    floating(body, 0x70, 0.1f);
    floating(body, 0x34, 0.1f);
    apply_runtime_player_body(body, apply);
    require(second_called && action_called && body[0x308] == 0
        && read_f32(body, 0x70) == 0.f && read_f32(body, 0x34) == 0.f,
        "complete body apply must preserve callback-sensitive timer order and decay fields");
    action = nullptr;
    block = 1;
    floating(body, 0x70, 0.5f);
    apply_runtime_body(body, apply);
    require(read_f32(body, 0x70) == 0.5f, "blocked decay must preserve body timer");
    body[0x2D0] = 1;
    body[0x2D1] = 3;
    body[0x2D2] = 2;
    secondary = &instance;
    apply_runtime_body_model_state(body, model, secondary, states);
    require(states[3] == 2 && (instance.node_flags[0] & 0xC0) == 0x80,
        "body model state must pass slot and hidden AL value to actual model state routine");

    std::array<u8, 0x100> cpu{};
    put(cpu, 0, 8);
    put(cpu, 4, 1);
    put(cpu, 8, 4);
    put(cpu, 12, 0x20);
    EntityShapeNode sphere{};
    sphere.kind = 0;
    sphere.bone = 0xFFFF;
    sphere.mask = 1;
    sphere.along_x = 2.f;
    const Vec3 local{1.f, 3.f, 4.f};
    std::memcpy(reinterpret_cast<u8*>(&sphere) + 0x14, &local, 12);
    std::memcpy(cpu.data() + 0x28, &sphere, sizeof(sphere));
    ActorModel actor_model{cpu};
    actor.model = &actor_model;
    actor.xform_valid = 1;
    std::array<u8, 0x20> output{};
    require(build_runtime_actor_bound(actor, output.data(), 1, true, shapes) == 1
        && read_f32(output, 4) == 1.f && read_f32(output, 8) == 1.f
        && read_f32(output, 0x14) == 4.f && read_f32(output, 0x1C) == 2.f,
        "actual sphere transform must produce averaged cylinder bound and exact vertical extent");
    require(shapes->stride == 0x28 && shapes->cursor - shapes->begin == 0x28,
        "bound collection must retain actual sphere rows in shape pool");
    engine_heap_free(shapes);
    shapes = allocate_runtime_shape_pool(0x28, 0);
    require(shapes->grow == 10 && shapes->cursor == shapes->limit,
        "zero initial shape pool must defer allocation and retain retail ten-row growth");
    shapes = grow_runtime_shape_pool(shapes);
    require(shapes->limit - shapes->begin == 0x190,
        "shape pool growth must use stride times original grow count");
    engine_heap_free(shapes);
    std::array<u8, 0xE00> source{};
    put(actor_bytes, 0xCE0, 1);
    put(actor_bytes, 0xCDC, 2);
    body[0x6A] = 1;
    actor.anchor = Vec3{2.f, 3.f, 4.f};
    receive_runtime_body_push(body, actor, actor_bytes, source, nullptr, Vec3{1.f, -1.f, 2.f});
    require(actor.anchor.x == 3.f && actor.anchor.y == 2.f && actor.anchor.z == 6.f,
        "body push message must move qualifying recipient with default mode one");
    put(source, 0xCE0, 1);
    put(source, 0xCDC, 2);
    receive_runtime_body_push(body, actor, actor_bytes, source, nullptr, Vec3{10.f, 10.f, 10.f});
    require(actor.anchor.x == 3.f, "qualified sender must block body push message");

    entity_detail::ShapeProbe probe;
    probe.push = [](entity_detail::ShapeProbe&, const Actor*, const Vec3* query,
        EntityCylinderBound& merged, EntityCylinderBound* values, i32 count, Vec3& delta)
    {
        require(query != nullptr && count == 1 && values[0].height == 6.f
            && values[0].radius == 2.f && merged.height == 6.f,
            "fallback shape probe must receive scaled actor height and radius");
        delta = Vec3{1.f, 2.f, 3.f};
        return 1;
    };
    entity_detail::ShapeProbe* shape_first = &probe;
    i32 contact_count = 5;
    std::array<entity_detail::ShapeProbe*, 8> contacts{};
    RuntimeBodyShapePush shape_context{shape_first, contact_count, contacts};
    RuntimeBodyShapeStorage shape_storage{};
    actor.radius_y = 3.f;
    actor.radius_xz = 4.f;
    floating(body, 0x884, 2.f);
    floating(body, 0x888, 0.5f);
    body[0x244] = 1;
    Vec3 position{10.f, 20.f, 30.f};
    push_runtime_player_body_shapes(body, actor, position, shape_storage, shape_context);
    require(position.x == 11.f && position.y == 22.f && position.z == 33.f
        && read_f32(body, 0x248) == 1.f && read_f32(body, 0x24C) == 2.f
        && read_f32(body, 0x250) == 3.f && contact_count == 1 && contacts[0] == &probe,
        "shape push must retain owner contact and apply actual accumulated correction");
    RuntimePlayerCollisionAction collision_action;
    RuntimePlayerCollisionAction* current_action = &collision_action;
    collision_action.message = [](RuntimePlayerCollisionReply& message)
    {
        message.accepted = 1;
    };
    require(!notify_runtime_player_collision_action(position, position, position, nullptr, 3, current_action),
        "collision action must require both reply words");
    collision_action.message = [](RuntimePlayerCollisionReply& message)
    {
        message.accepted = 1;
        message.handled = 1;
        message.target->y = 40.f;
    };
    require(notify_runtime_player_collision_action(position, position, position, nullptr, 3, current_action)
        && position.y == 40.f, "collision action must preserve actual reply and target writes");
    std::array<u8, 0xD00> attached_bytes{};
    RuntimeBodyAttachedActor attached;
    attached.bytes = attached_bytes;
    RuntimeBodyActionIds ids{&attached, 7, 0x10001234};
    attached_bytes[0x93D] = 1;
    put(attached_bytes, 0x944, 2);
    put(attached_bytes, 0x948, 6);
    put(attached_bytes, 0x94C, 1);
    put(attached_bytes, 0x958, 7);
    put(attached_bytes, 0x95C, 3);
    require(classify_runtime_attached_actor(attached_bytes, 7) == 3
        && classify_runtime_attached_actor(attached_bytes, 8) == 1
        && runtime_body_primary_action_id(ids) == 0x11C
        && runtime_body_attachment_action_id(ids) == 0x17,
        "action ids must use matching class rows and first-row fallback");
    u16 action_key = 0x1234, fallback = 0x5678;
    runtime_body_primary_action_pair(ids, action_key, fallback);
    require(action_key == 0x108 && fallback == 0x113,
        "paired action ids must preserve class-specific key and fallback");
    put(attached_bytes, 0x95C, 5);
    runtime_body_primary_action_pair(ids, action_key, fallback);
    require(action_key == 0x108 && fallback == 0x113,
        "unrecognised attachment class must preserve both incoming pair words");
    put(attached_bytes, 0x944, 0xFFFF);
    require(classify_runtime_attached_actor(attached_bytes, 7) == 1,
        "negative signed row count must skip search and use nonzero-count fallback");
    attached_bytes[0x93D] = 5;
    require(runtime_body_primary_action_id(ids) == 0,
        "out-of-range attachment state must return actual zero action id");
    Actor first_contact_actor, second_contact_actor;
    first_contact_actor.model_flags = 0x111;
    second_contact_actor.model_flags = 0x222;
    entity_detail::ListHead players;
    RuntimeSegmentContactActor first_contact{first_contact_actor, 0, nullptr, {}},
        second_contact{second_contact_actor, 0, nullptr, {}};
    RuntimeBodyCollisionState collision_state{};
    RuntimeBodyCollisionRecord active{};
    collision_state.active = &active;
    request_runtime_body_contact_override(&collision_state, first_contact, players);
    request_runtime_body_contact_override(&collision_state, second_contact, players);
    require(collision_state.overrides->cursor - collision_state.overrides->begin
        == 2 * sizeof(RuntimeBodyCollisionOverride), "contact notices must retain both native actors");
    collision_state.walking = 1;
    remove_runtime_body_contact_actor(collision_state, first_contact_actor);
    remove_runtime_body_contact_actor(collision_state, first_contact_actor);
    require(collision_state.removed->cursor - collision_state.removed->begin == sizeof(Actor*)
        && collision_state.overrides->cursor - collision_state.overrides->begin
        == 2 * sizeof(RuntimeBodyCollisionOverride),
        "active contact walk must defer removal and suppress duplicate pending actors");
    collision_state.walking = 0;
    remove_runtime_body_contact_actor(collision_state, first_contact_actor);
    auto* override_row = reinterpret_cast<RuntimeBodyCollisionOverride*>(collision_state.overrides->begin);
    require(override_row->actor == &second_contact_actor
        && collision_state.overrides->cursor - collision_state.overrides->begin == sizeof(*override_row)
        && first_contact_actor.model_flags == 0x111,
        "immediate contact removal must compact final row without changing actor filter");
    engine_heap_free(collision_state.overrides);
    engine_heap_free(collision_state.removed);
    std::array<EntityShapeNode, 3> contact_shapes{};
    std::array<RuntimeBodyCollisionRecord, 3> contact_records{};
    for (size_t shape_index = 0; shape_index < contact_shapes.size(); ++shape_index)
    {
        contact_shapes[shape_index].bone = 0xFFFF;
        contact_shapes[shape_index].along_x = 2.f;
        const u16 tag = shape_index == 2 ? 3 : 2;
        std::memcpy(reinterpret_cast<u8*>(&contact_shapes[shape_index]) + 2, &tag, 2);
        contact_records[shape_index].shape = reinterpret_cast<u8*>(&contact_shapes[shape_index]);
    }
    collision_state = {};
    collision_state.records = contact_records.data();
    collision_state.count = 3;
    actor.model = nullptr;
    activate_runtime_body_contact_shapes(collision_state, actor, 2, 3, 0.5f);
    require(collision_state.active == &contact_records[1]
        && contact_records[1].next == &contact_records[0]
        && contact_records[0].next == nullptr && contact_records[2].previous_link == nullptr,
        "contact activation must insert only matching tags in retail head order");
    activate_runtime_body_contact_shapes(collision_state, actor, 2, 5, 2.f);
    require(contact_records[1].next == &contact_records[0] && contact_records[1].bytes[0x17] == 5,
        "reactivation must update flags without linking a record twice");
    activate_runtime_body_contact_shapes(collision_state, actor, 3, 1, 1.f);
    deactivate_runtime_body_contact_shapes(collision_state, 2);
    require(collision_state.active == &contact_records[2] && contact_records[2].next == nullptr
        && contact_records[0].previous_link == nullptr && contact_records[1].previous_link == nullptr,
        "contact deactivation must unlink matching nodes and keep unmatched head");
    deactivate_runtime_body_contact_shapes(collision_state, 3);
    require(collision_state.active == nullptr, "last contact deactivation must clear sentinel next");

    auto* saved_segment = entity_detail::g_world.segment;
    const auto saved_slot = entity_detail::g_segment_slots[0];
    const i32 saved_count = entity_detail::g_segment_count;
    entity_detail::g_segment_slots[0] = {};
    entity_detail::SegmentProbe segment;
    segment.sphere_contacts = [](entity_detail::SegmentProbe&, Vec3&, float radius,
        i32 kind, u32 mask, entity_detail::SegmentPool& pool)
    {
        require(radius == 2.f && kind == 4 && mask == 0xC4000,
            "sphere collection must pass all actual query arguments");
        std::memset(pool.cursor, 0x11, pool.stride);
        pool.cursor += pool.stride;
        std::memset(pool.cursor, 0x22, pool.stride);
        pool.cursor += pool.stride;
        return 1;
    };
    entity_detail::g_world.segment = &segment;
    require(entity_collect_sphere_contacts(position, 2.f, 4, 0xC4000) == 1,
        "sphere collection must retain actual accepting segment owner");
    const auto merged_records = entity_merge_contact_records();
    require(merged_records.stride == 0x54 && merged_records.grow == 2
        && merged_records.cursor - merged_records.begin == 0xA8
        && merged_records.begin[0] == 0x11 && merged_records.begin[0x54] == 0x22,
        "contact aggregation must preserve every record and its order");
    engine_heap_free(merged_records.block);
    engine_heap_free(entity_detail::g_segment_slots[0].pool.block);
    entity_detail::g_segment_slots[0] = saved_slot;
    entity_detail::g_segment_count = saved_count;
    entity_detail::g_world.segment = saved_segment;
}
