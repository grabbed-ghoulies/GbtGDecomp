#include "player_parent_checks.h"
#include "../src/engine/entities/body_message_helpers.h"
#include "body_frame_checks.h"
#include "../src/engine/entities/player_parent_lifecycle.h"
#include "../src/engine/entities/player_attributes.h"
#include "../src/engine/entities/player_mind_goals.h"
#include "../src/engine/entities/player_mind_events.h"
#include "../src/engine/entities/player_attachment_helpers.h"
#include "../src/engine/entities/player_attachment.h"
#include "../src/engine/entities/player_construction.h"
#include "../src/engine/entities/player_destruction.h"
#include "../src/engine/entities/player_strategy_entry.h"
#include "../src/engine/entities/player_state.h"
#include "../src/engine/entities/body_motion_controller.h"
#include "../src/engine/entities/body_health.h"
#include "../src/engine/entities/body_resources.h"
#include "../src/engine/entities/body_animation_setup.h"
#include "../src/engine/entities/body_action_selection.h"
#include "../src/engine/entities/body_attachment_cleanup.h"
#include "../src/engine/entities/body_construction.h"
#include "../src/engine/entities/player_body_motion.h"
#include "../src/engine/entities/entity_heap.h"
#include "gbtg/model_runtime.h"
#include <bit>
#include "../src/engine/graphics/actor_effect_nodes.h"
#include <new>
#include <memory>
#include <cmath>

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
float fraction(u32& state)
{
    state = state * 1664525u + 1013904223u;
    return std::bit_cast<float>(0x3F800000u | (state & 0x7FFFFFu)) - 1.f;
}
struct PlayerConstructionFactoryFixture
{
    RuntimePlayerAttachment& attachment;
    RuntimePlayerAttachmentActor& actor;
    std::unordered_map<const u8*, RuntimePlayerAttachmentActor*>& views;
    u32 constructed = 0;
    u32 destroyed = 0;
    bool reject_attachment = false;
    bool channel = false;
};
PlayerConstructionFactoryFixture* construction_factory_fixture = nullptr;
void player_fixture_sizes(ObjectSizes& sizes)
{
    sizes.instance = 0xF00;
    sizes.parameters = 0xE00;
}
bool player_fixture_construct(FactoryObject& object, std::span<const u8> parameters)
{
    auto& fixture = *construction_factory_fixture;
    ++fixture.constructed;
    if (object.type == 1)
    {
        require(read_u32(parameters, 0x1B0) == fixture.attachment.host.address,
            "player body factory must receive clone with actual actor token override");
        fixture.attachment.body = {object.bytes.get(), object.size};
        put(fixture.attachment.body, 0xFC, fixture.channel ? 1 : 0);
        return true;
    }
    std::memcpy(object.bytes.get() + 6, parameters.data() + 6, parameters.size() - 6);
    fixture.actor.object.bytes = {object.bytes.get(), object.size};
    fixture.views[object.bytes.get()] = &fixture.actor;
    return !fixture.reject_attachment;
}
void player_fixture_destroy(FactoryObject& object)
{
    auto& fixture = *construction_factory_fixture;
    ++fixture.destroyed;
    fixture.views.erase(object.bytes.get());
}
const ObjectFactoryVtable* player_fixture_vtable()
{
    static const ObjectFactoryVtable table{0x12340000, player_fixture_sizes,
        player_fixture_construct, player_fixture_destroy};
    return &table;
}
struct MotionDispatchFixture
{
    RuntimeBodyMotionController& controller;
    std::vector<u16> calls;
};
void second_motion_callback(float*, const float*, float*, u16 joint, void* data, float elapsed)
{
    auto& fixture = *static_cast<MotionDispatchFixture*>(data);
    require(elapsed == 0.25f, "motion elapsed argument");
    fixture.calls.push_back(joint);
}
void first_motion_callback(float*, const float*, float*, u16 joint, void* data, float)
{
    auto& fixture = *static_cast<MotionDispatchFixture*>(data);
    fixture.calls.push_back(joint);
    const u16 changed = 14;
    std::memcpy(fixture.controller.joints->begin, &changed, 2);
    auto* row = reinterpret_cast<RuntimeBodyMotionCallbacks*>(fixture.controller.callbacks->begin);
    row->second = second_motion_callback;
    row->second_data = data;
}
void check_body_dependencies(SceneExecutableResources& executable)
{
    {
        std::array<u8, 0xF00> actor_bytes{};
        std::array<u8, 0x950> body_bytes{};
        std::span<u8> body = body_bytes;
        Actor native_actor;
        RuntimeBodyAttachedActor actor{actor_bytes, 0x12345678, {}};
        actor.actor = &native_actor;
        u32 enabled = 1, value = 0x3F800001, argument = 9, name = 10;
        u32 player_enabled = 0, player_value = 0x40000001, player_argument = 11, player_name = 12;
        RuntimePlayerScriptOverrides overrides{enabled, value, argument, name,
            player_enabled, player_value, player_argument, player_name};
        std::vector<std::array<u32, 5>> notices;
        actor.message = [&](std::span<const u32> message)
        {
            require(message.size() == 5 && message[4] == 0, "override notices retain all five dwords");
            notices.push_back({message[0], message[1], message[2], message[3], message[4]});
            if (message[2] == 7)
            {
                player_enabled = 1;
                put(actor_bytes, 0xCDC, 5);
            }
        };
        put(actor_bytes, 0xCDC, 1);
        require(restore_runtime_player_script_overrides(actor, overrides)
            && notices.size() == 2 && notices[0] == std::array<u32, 5>{0x86, 10, 7, 9, 0}
            && notices[1] == std::array<u32, 5>{0x86, 12, 0x13, 11, 0}
            && std::bit_cast<u32>(native_actor.animation_pause) == value
            && read_u32(actor_bytes, 0x978) == player_value,
            "override restoration snapshots player mode but rereads globals after first virtual notice");
        notices.clear();
        require(restore_runtime_player_script_overrides(actor, overrides) && notices.size() == 1,
            "mode five receives only the shared override");
        put(actor_bytes, 0xCDC, 2);
        require(!restore_runtime_player_script_overrides(actor, overrides) && notices.size() == 1,
            "other actor modes do not restore script overrides");
        entity_detail::LatchNode* first = nullptr;
        body_bytes[0x260] = 1;
        body_bytes[0x265] = 1;
        require(runtime_body_latch_mask(body) == 0x21, "body mask uses six independent byte counters");
        decrement_runtime_body_latches(body, native_actor, first);
        require(runtime_body_latch_mask(body) == 0x1E && body_bytes[0x261] == 255,
            "latch decrement wraps zeros without a saturation guard");
        increment_runtime_body_latches(body, native_actor, first);
        require(runtime_body_latch_mask(body) == 0x21 && body_bytes[0x261] == 0,
            "latch increment wraps 255 to zero");
        struct LatchFixture : entity_detail::LatchNode
        {
            Actor* expected;
            u32 calls = 0;
        };
        LatchFixture second{};
        LatchFixture leading{};
        leading.expected = second.expected = &native_actor;
        leading.next = &second;
        auto receive = [](entity_detail::LatchNode& node, const Message& message)
        {
            auto& fixture = static_cast<LatchFixture&>(node);
            require(message.id == 0x47 && message.arg == fixture.expected, "latch notice carries actual body owner");
            ++fixture.calls;
            node.next = nullptr;
        };
        leading.handle_message = second.handle_message = receive;
        first = &leading;
        std::fill(body_bytes.begin() + 0x260, body_bytes.begin() + 0x266, 1);
        decrement_runtime_body_latches(body, native_actor, first);
        require(leading.calls == 1 && second.calls == 1 && runtime_body_latch_mask(body) == 0,
            "latch notification saves next node before a virtual callback mutates links");
        leading.next = &second;
        std::fill(body_bytes.begin() + 0x260, body_bytes.begin() + 0x266, 255);
        increment_runtime_body_latches(body, native_actor, first);
        require(leading.calls == 2 && second.calls == 2, "increment overflow also broadcasts departure from full latch mask");
        first = nullptr;
        for (u16 kind = 0; kind < 64; ++kind)
        {
            const bool excluded = kind == 2 || kind == 3 || kind == 4 || kind == 7
                || kind == 0xD || kind == 0x10 || kind == 0x13 || kind == 0x25;
            require(runtime_goal_requires_latches(kind) == !excluded, "goal classifier must preserve all exclusions");
        }
        std::array<u8, 2> zero{};
        auto* allocation = static_cast<u8*>(engine_heap_allocate(sizeof(RuntimePlayerGoalRing) + 12));
        auto* ring = new (allocation) RuntimePlayerGoalRing{4, 3, 2, 0, {allocation + sizeof(RuntimePlayerGoalRing), 12}};
        auto* rows = allocation + sizeof(RuntimePlayerGoalRing);
        std::memset(rows, 0, 12);
        rows[8] = 1;
        rows[0] = 2;
        std::fill(body_bytes.begin() + 0x260, body_bytes.begin() + 0x266, 1);
        RuntimePlayerGoalCleanup goals{actor, ring, body, native_actor, first, zero, overrides};
        put(actor_bytes, 0x980, 123);
        put(actor_bytes, 0x984, 456);
        actor_bytes[0x98B] = 1;
        put(actor_bytes, 0xCDC, 1);
        notices.clear();
        remove_runtime_player_goal(goals);
        require(ring != nullptr && ring->head == 0 && runtime_body_latch_mask(body) == 0
            && read_u32(actor_bytes, 0x984) == 456 && notices.empty(),
            "goal pop wraps head and does not latch an excluded next goal");
        remove_runtime_player_goal(goals);
        require(ring == nullptr && read_u32(actor_bytes, 0x980) == 0 && read_u32(actor_bytes, 0x984) == 0
            && actor_bytes[0x98B] == 0 && notices.size() == 2,
            "final goal frees retained heap storage, clears runtime goal and restores enabled overrides");
        remove_runtime_player_goal(goals);
        require(notices.size() == 2, "null ring removal has no side effects");
        allocation = static_cast<u8*>(engine_heap_allocate(sizeof(RuntimePlayerGoalRing) + 4));
        ring = new (allocation) RuntimePlayerGoalRing{4, 1, 0, 1, {allocation + sizeof(RuntimePlayerGoalRing), 4}};
        zero[0] = 2;
        remove_runtime_player_goal(goals);
        require(ring == nullptr && runtime_body_latch_mask(body) == 0,
            "empty ring classification consumes the actual address-zero word rather than invented kind zero");

        entity_detail::NavGrid grid;
        grid.cells.resize(2);
        grid.cells[0].value_6 = 0xE0;
        grid.cells[0].flags_7 = 0xA5;
        grid.cells[1].value_6 = 0x1F;
        grid.cells[1].flags_7 = 0xB4;
        auto* saved_grid = entity_detail::g_nav_grids[0];
        entity_detail::g_nav_grids[0] = &grid;
        allocation = static_cast<u8*>(engine_heap_allocate(sizeof(entity_detail::NavigationCellRegistration) + 24));
        rows = allocation + sizeof(entity_detail::NavigationCellRegistration);
        auto* registration = new (allocation) entity_detail::NavigationCellRegistration{rows, rows + 24};
        std::memset(rows, 0, 24);
        put({rows, 24}, 12, 1);
        put({rows, 24}, 16, 99);
        entity_detail::release_navigation_cell_registration(registration, 0);
        require(registration == nullptr && grid.cells[0].value_6 == 0xFF && grid.cells[0].flags_7 == 0xA5
            && grid.cells[1].value_6 == 0xFF && grid.cells[1].flags_7 == 0xB5,
            "navigation release wraps only selected five/four-bit counters and preserves unrelated bits");
        entity_detail::g_nav_grids[0] = saved_grid;
        entity_detail::release_navigation_cell_registration(registration, 0xFFFFFFFF);
    }
    {
        std::array<u8, 0x950> body{};
        std::array<u8, 0xF00> host_bytes{};
        std::array<u8, 0xF00> attached_bytes{};
        std::array<u8, 0xF00> secondary_bytes{};
        Actor host_actor;
        Actor attached_actor;
        actor_refresh_transform(host_actor);
        host_actor.class_key = 7;
        put(host_bytes, 6, 7);
        std::vector<u32> calls;
        RuntimeBodyAttachedActor host{host_bytes, 0x1111,
            [&](std::span<const u32> message)
            {
                require((message[0] == 0x5E || message[0] == 0x5F) && message[1] == 0x2222, "attachment host notice");
                calls.push_back(message[0]);
            }};
        RuntimeBodyAttachedActor object{attached_bytes, 0x2222,
            [&](std::span<const u32> message)
            {
                if (message[0] == 0x91)
                {
                    require(read_u32(attached_bytes, 0xD8C) == host.address,
                        "attached host token must be visible to virtual notice");
                }
                calls.push_back(message[0]);
            }};
        object.actor = &attached_actor;
        RuntimeBodyAttachedActor secondary_object{secondary_bytes, 0x3333,
            [&](std::span<const u32> message)
            {
                require(message[0] == 0x97 && message[1] == 42, "secondary release notice");
                calls.push_back(0x197);
            }};
        RuntimeBodyAttachedActor* attached = nullptr;
        RuntimeBodyAttachedActor* secondary = nullptr;
        RuntimeBodyAttachedActor* mind_action = &secondary_object;
        bool mind_present = true;
        RuntimeBodyAnimationReceiver receiver{
            [&](const RuntimeBodyAnimationNotice& notice)
            {
                require(notice.id == 0x28 && notice.actor == host.address, "body animation attachment notice");
                calls.push_back(0x28);
            }};
        RuntimeBodyAnimationReceiver* first_receiver = nullptr;
        RuntimeBodyAnimationReceiver* second_receiver = nullptr;
        u32 host_address = host.address;
        RuntimeBodyBitStorage* flags = allocate_runtime_body_bits(0x41);
        RuntimeBodyActorAttachment ownership{attached, host_address, host_actor, flags, first_receiver};
        ActorAnimationState animation;
        ActorAnimationState attached_animation;
        ModelAnimationState model;
        ModelNodeFlagCache cache{};
        AnimationMatrix reset{};
        RuntimePlayerAttachmentActor actor_view{object, attached_actor, receiver, attached_animation,
            model, 0, nullptr, cache, reset};
        const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> actors{{&object, &actor_view}};
        ObjectFactoryRegistry factory;
        RuntimePlayerChildContext child{factory, {}, host.address};
        FactoryObject first_action;
        FactoryObject second_action;
        const u8* row = nullptr;
        const RuntimeScriptLookupTable* table = nullptr;
        const std::unordered_map<u32, const char*> names;
        AnimationResourceRegistry registry;
        ResourceErrorState error;
        u32 random = 1;
        RuntimeBodyActionSelection actions{animation, attached, first_receiver, second_receiver,
            first_action, second_action, row, table, names, child, registry, error, random, 0x4444};
        std::array<const char*, 5> weapon_names{"unarmed", "weapon", "item", "special", "other"};
        std::array<const char*, 4> class_names{"none", "first", "second", "third"};
        RuntimePlayerAttachment context{body, host, ownership, actions, secondary, mind_action,
            mind_present, animation, actors, weapon_names.data(), class_names.data(),
            [&]()
            {
                calls.push_back(0x20);
                return u16(0);
            },
            [&]()
            {
                return mind_action;
            }};
        require(!attach_runtime_player_actor(actor_view, 1, context) && calls.empty(),
            "attachment with no accepted classes must fail without side effects");
        put(attached_bytes, 0x944, 1);
        put(attached_bytes, 0x948, 0xFFFF0009);
        require(!attach_runtime_player_actor(actor_view, 1, context) && attached == nullptr,
            "attachment class miss must fail before ownership changes");
        put(attached_bytes, 0x948, 0xFFFF0007);
        put(attached_bytes, 0x94C, 2);
        attached_bytes[0x93D] = 3;
        require(attach_runtime_player_actor(actor_view, 1, context)
            && attached == &object && first_receiver == &receiver && object.host == &host
            && attached_actor.bind != nullptr && attached_actor.bind->parent == &host_actor
            && attached_actor.bind->mode_parent == 1 && attached_actor.bind->flags == 1
            && flags->bytes[4] == 4 && read_u32(body, 0x48) == object.address
            && read_u32(body, 0x10) == object.address && body[0x30B] == 1
            && std::strcmp(reinterpret_cast<const char*>(body.data() + 0x288), "special_second") == 0
            && calls == std::vector<u32>{0x91, 0x20, 0x28, 0x5E},
            "full attach must format class variant, bind real actor, select action and notify in exact order");
        calls.clear();
        require(!attach_runtime_player_actor(actor_view, 0, context) && calls.empty(),
            "occupied body must reject another attach without notices");
        attached_bytes[0x93F] = 1;
        secondary = &secondary_object;
        second_receiver = &receiver;
        require(!release_runtime_player_attachment(0, 42, context) && attached == &object
            && secondary == nullptr && second_receiver == nullptr && calls == std::vector<u32>{0x197},
            "protected primary attachment must survive while distinct secondary is released");
        calls.clear();
        require(release_runtime_player_attachment(1, 42, context) && attached == nullptr
            && attached_actor.bind == nullptr && body[0x30B] == 0
            && calls == std::vector<u32>{0x9D, 0x9C, 0x97},
            "forced release must detach actual attached actor then notify reason");
        calls.clear();
        flags->bytes[5] = 0x40;
        require(!release_runtime_player_attachment(0, 42, context) && calls == std::vector<u32>{0x197},
            "unattached body with mind-action flag must notify actual mind recipient");
        calls.clear();
        mind_present = false;
        require(!release_runtime_player_attachment(0, 42, context) && calls.empty(),
            "null mind must suppress action notice");
        attached_bytes[0x93F] = 0;
        require(attach_runtime_player_actor(actor_view, 0, context), "reattach fixture for replacement");
        animation.controller = std::make_unique<AnimationController>();
        animation.controller->slots.resize(1);
        animation.controller->slots[0].player = make_mina_player();
        auto& player = *animation.controller->slots[0].player;
        player.old_pose = std::make_unique<AnimationPose>();
        player.blend_remaining = 5.f;
        calls.clear();
        require(replace_runtime_player_attachment(context) && attached == nullptr
            && player.old_pose == nullptr && player.blend_remaining == 0.f && animation.controller->dirty
            && calls == std::vector<u32>{0x9D, 0x9C, 0x97, 0x20},
            "replacement must release attachment, select body action and reset retained animation blend");
        require(!replace_runtime_player_attachment(context), "empty attachment replacement must return false");
        calls.clear();
        put(attached_bytes, 0x948, 0xFFFF0009);
        require(!attach_runtime_player_secondary(actor_view, context) && secondary == nullptr && calls.empty(),
            "secondary class miss must preserve body ownership and suppress notices");
        put(attached_bytes, 0x948, 0xFFFF0007);
        const u8 attachment_flags = flags->bytes[4];
        const u8 attachment_count = body[0x30B];
        require(attach_runtime_player_secondary(actor_view, context) && attached == nullptr
            && secondary == &object && second_receiver == &receiver
            && read_u32(body, 0x4C) == object.address && read_u32(body, 0x14) == object.address
            && attached_actor.bind != nullptr && attached_actor.bind->parent == &host_actor
            && attached_actor.bind->mode_parent == 1 && flags->bytes[4] == attachment_flags
            && body[0x30B] == attachment_count && calls == std::vector<u32>{0x91, 0x28, 0x5F},
            "secondary attach must bind and notify without primary action, flag or counter changes");
        calls.clear();
        require(detach_runtime_body_secondary(body, secondary, second_receiver) == &object
            && secondary == nullptr && second_receiver == nullptr && attached_actor.bind == nullptr
            && calls == std::vector<u32>{0x9D, 0x9C},
            "secondary ownership must detach through actual bind cleanup and virtual notices");
        format_runtime_body_variant(body, "%s_%d_%08x", "variant", -12, 0xABCD);
        require(std::strcmp(reinterpret_cast<const char*>(body.data() + 0x288), "variant_-12_0000abcd") == 0,
            "body formatter must forward real varargs to reconstructed CRT output");

        std::array<u8, 0xF00> body_template{};
        std::array<u8, 0xF00> attachment_template{};
        put(body_template, 0, 0xE00);
        put(body_template, 4, 1);
        put(attachment_template, 0, 0xE00);
        put(attachment_template, 4, 2);
        put(attachment_template, 0x944, 1);
        put(attachment_template, 0x948, 0xFFFF0007);
        put(attachment_template, 0x94C, 2);
        attachment_template[0x93D] = 3;
        std::array<ExecutableResourceRecord, 2> records
        {{ {"aid_test_body", body_template}, {"aid_test_attachment", attachment_template} }};
        std::vector<ExecutableResourcePool> pools;
        register_executable_resource_pool(pools, records, "test", true);
        std::array<ObjectFactoryType, 3> types
        {{ {0, player_fixture_vtable}, {0, player_fixture_vtable}, {0, player_fixture_vtable} }};
        factory.types = types;
        child.resources = pools;
        std::unordered_map<const u8*, RuntimePlayerAttachmentActor*> created_views;
        PlayerConstructionFactoryFixture fixture{context, actor_view, created_views};
        construction_factory_fixture = &fixture;
        RuntimePlayerAttachmentCreation creation{context, child,
            [&created_views](FactoryObject& object) -> RuntimePlayerAttachmentActor&
            {
                return *created_views.at(object.bytes.get());
            }};
        require(create_runtime_player_attachment("missing_prefix", 0, creation).bytes == nullptr,
            "attachment creation must preserve clone prefix rejection");
        fixture.reject_attachment = true;
        require(create_runtime_player_attachment("aid_test_attachment", 0, creation).bytes == nullptr
            && factory.count == 0 && created_views.empty(),
            "failed factory construction must clean object and clone without registering attachment");
        fixture.reject_attachment = false;
        put(attachment_template, 0x948, 0xFFFF0009);
        require(create_runtime_player_attachment("aid_test_attachment", 0, creation).bytes == nullptr
            && factory.count == 0 && created_views.empty() && attached == nullptr,
            "failed body attachment must destroy successfully constructed factory object");
        put(attachment_template, 0x948, 0xFFFF0007);

        std::array<u8, 0x7C8> parameters{};
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x43A), "aid_test_body");
        std::array<u8, 0x100> zero{};
        std::span<const u8> attribute_resource;
        std::array<const char*, 0xE9> actor_tags{};
        for (size_t index = 0; index < actor_tags.size(); ++index)
        {
            const u32 address = read_u32(executable.memory(0x42A854 + u32(index) * 4, 4), 0);
            actor_tags[index] = reinterpret_cast<const char*>(executable.memory(address, 1).data());
        }
        RuntimePlayerAttributes attributes{attribute_resource, registry, error, random, actor_tags.data(), zero};
        GhoulyController controller;
        RuntimePlayerSpawnController spawn{nullptr, 0, controller};
        FactoryObject body_object;
        FactoryObject mind_object;
        FactoryObject strategy_object;
        FactoryObject created_attachment;
        RuntimePlayerMindRequests* mind_requests = nullptr;
        RuntimePlayerStrategySetup strategy_setup{child, mind_requests, {}, {}};
        std::span<u8> input;
        const std::unordered_map<u32, std::span<u8>> input_addresses;
        const RuntimeInputReleaseContext input_release{input_addresses};
        std::array<u8, 0x87> tag_rows{};
        std::strcpy(reinterpret_cast<char*>(tag_rows.data()), "chosen");
        const char* tag_names[]{"end", "other", "chosen"};
        u32 player_count = 5;
        const u32 story_flags = 0x10;
        RuntimeActorMatrixState matrices{};
        RuntimeShaderMatrixBoundaries boundaries{};
        RuntimePlayerConstruction construction{host_actor, attributes, spawn, body_object, mind_object,
            strategy_object, strategy_setup, input, input_release, tag_rows.data(), tag_names, creation,
            created_attachment, read_u32(executable.memory(0x338EE0, 4), 0), player_count, story_flags,
            matrices, boundaries, [](const void*) -> u32
            {
                return 0x30001230;
            }};
        host_actor.position = Vec3{1.f, 2.f, 3.f};
        host_actor.euler = Vec3{0.1f, 0.2f, 0.3f};
        put(parameters, 0x63A, 17);
        put(host_bytes, 0xCF4, 0xBEEF);
        random = 7;
        u32 expected = random;
        const float phase = fraction(expected) * 0.5f;
        require(initialize_runtime_player(host_bytes, parameters, construction)
            && random == expected && read_f32(host_bytes, 0x994) == phase
            && read_f32(host_bytes, 0xCC0) == 1.f && read_f32(host_bytes, 0xCC4) == 2.f
            && read_f32(host_bytes, 0xCC8) == 3.f && read_f32(host_bytes, 0xCCC) == 0.1f
            && read_u16(host_bytes, 0x990) == 17 && read_u16(host_bytes, 0xCF4) == 0xBEEF
            && input.data() != nullptr && input.size() == 0xB0 && read_u32(input, 0) == 1
            && read_u32(input, 4) == 0 && host_bytes[0x298] == 1 && read_u32(host_bytes, 0x960) == 0x30001230
            && player_count == 5 && factory.count == 1 && created_attachment.bytes == nullptr,
            "full player ctor must copy transforms, consume actual LCG, preserve zero-tag field and create base input/body");
        engine_heap_free(input.data());
        input = {};
        fixture.channel = true;
        put(parameters, 0x334, 0x00040001);
        const u16 tag = 2;
        std::memcpy(parameters.data() + 0x338, &tag, 2);
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x4BA), "aid_test_attachment");
        expected = random;
        const float next_phase = fraction(expected) * 0.5f;
        for (i32 index = 0; index < 6; ++index)
        {
            fraction(expected);
        }
        const float channel = fraction(expected);
        require(initialize_runtime_player(host_bytes, parameters, construction)
            && random == expected && read_f32(host_bytes, 0x994) == next_phase
            && read_u16(host_bytes, 0xCDC) == 1 && host_actor.mask_mode == 1
            && read_u16(host_bytes, 0xCDE) == 4 && read_u16(host_bytes, 0xCF4) == 1 && host_actor.damage_class == 1
            && read_u16(host_bytes, 0xCF6) == 2 && player_count == 6
            && read_f32(context.body, 0x8C) == channel && context.body[0x90] == 2
            && context.body[0x91] == 0 && context.body[0x92] == 2
            && created_attachment.bytes != nullptr && attached == &object
            && read_u32(object.bytes, 0xDA0) == 1 && read_u32(object.bytes, 0x9A4) == 1,
            "full player ctor must share attribute/channel random state, resolve tag, retain attachment and apply story flag");
        require(release_runtime_player_attachment(1, 0, context), "constructor attachment teardown");
        release_factory_object(factory, created_attachment);
        std::array<entity_detail::ListHead, 4> links{};
        std::array<entity_detail::ListHead, 4> preceding{};
        std::array<entity_detail::ListHead, 4> following{};
        for (size_t index = 0; index < links.size(); ++index)
        {
            links[index] = {&preceding[index], &following[index]};
            preceding[index].next = following[index].prev = &links[index];
            put(host_bytes, 0x934 + index * 8, 100);
            put(host_bytes, 0x938 + index * 8, 200);
        }
        RuntimePlayerGoalRing* ring = nullptr;
        entity_detail::LatchNode* latch = nullptr;
        u32 enabled = 1, override_value = 0, override_argument = 0, override_name = 0;
        RuntimePlayerScriptOverrides overrides{enabled, override_value, override_argument, override_name,
            enabled, override_value, override_argument, override_name};
        RuntimePlayerGoalCleanup goals{host, ring, context.body, host_actor, latch, zero, overrides};
        entity_detail::NavigationCellRegistration* navigation = nullptr;
        RuntimePlayerDestruction destruction{construction, goals, links, navigation};
        auto* goal_allocation = static_cast<u8*>(engine_heap_allocate(sizeof(RuntimePlayerGoalRing) + 8));
        ring = new (goal_allocation) RuntimePlayerGoalRing{4, 2, 0, 1,
            {goal_allocation + sizeof(RuntimePlayerGoalRing), 8}};
        auto* goal_rows = goal_allocation + sizeof(RuntimePlayerGoalRing);
        std::memset(goal_rows, 0, 8);
        goal_rows[0] = 2; // Excluded first goal; next goal takes then drops all six latches.
        goal_rows[4] = 1;
        std::fill(context.body.begin() + 0x260, context.body.begin() + 0x266, 0);
        host_bytes[0x98B] = 1;
        host_bytes[0xBA0] = 1;
        put(host_bytes, 0x960, 0x1234);
        const auto retained_input = input.data();
        destroy_runtime_player(host_bytes, destruction);
        require(player_count == 5 && ring == nullptr && body_object.bytes == nullptr && mind_object.bytes == nullptr
            && strategy_object.bytes == nullptr && host_bytes[0x98B] == 0 && host_bytes[0xBA0] == 0
            && read_u16(host_bytes, 0xCF4) == 0 && read_u32(host_bytes, 0x960) == 0x1234
            && input.data() == retained_input, "player destructor releases children/input and preserves retail input word");
        for (size_t index = 0; index < links.size(); ++index)
        {
            require(preceding[index].next == &following[index] && following[index].prev == &preceding[index]
                && links[index].prev == nullptr && links[index].next == nullptr
                && read_u32(host_bytes, 0x934 + index * 8) == 0 && read_u32(host_bytes, 0x938 + index * 8) == 0,
                "player destructor unlinks all four lists and clears raw link words");
        }
        input = {}; // Fixture forgets dangling view only after checking retail teardown semantics.
        require(factory.count == 0 && created_views.empty(), "constructor fixtures must release retained factory ownership");
        construction_factory_fixture = nullptr;
        engine_heap_free(flags);
    }
    {
        std::array<u8, 0x950> body{};
        std::array<u8, 0x7C8> parameters{};
        std::array<u8, 8> first_bytes{};
        std::array<u8, 16> second_bytes{};
        std::array<u8, 48> third_bytes{};
        put(first_bytes, 0, 99);
        put(second_bytes, 0, 1);
        put(second_bytes, 8, 88);
        for (u32 i = 0; i < 6; ++i)
        {
            put(third_bytes, i * 8, i + 1);
        }
        std::array<AnimationBundleRecord, 3> records{};
        records[0].name = "aid_body_hash_zero";
        records[0].cpu = first_bytes;
        records[1].name = "aid_body_hash_first";
        records[1].cpu = second_bytes;
        records[2].name = "aid_body_hash_next";
        records[2].cpu = third_bytes;
        std::array<AnimationBundlePool, 1> pools{AnimationBundlePool{records, true}};
        AnimationResourceRegistry registry;
        register_animation_resource_provider(registry,
            [&](u32, const AnimationResourceRecord& entry, const void*& resolved)
            {
                return find_bundle_resource_provider(pools, entry.name.c_str(), resolved);
            },
            [&](u32, const void* resolved, AnimationLoadedResource& loaded)
            {
                return load_bundle_resource_provider(resolved, loaded);
            },
            [&](u32, const char* name, const AnimationLoadedResource&)
            {
                unload_bundle_resource_provider(pools, name);
            }, 3, nullptr);
        for (size_t i = 0; i < records.size(); ++i)
        {
            std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x2C8 + i * 0x80),
                records[i].name.c_str());
        }
        const std::array<RuntimeBodyLoadedResourceSize, 3> sizes
        {{{first_bytes.data(), 8, nullptr, 0}, {second_bytes.data(), 16, nullptr, 0},
            {third_bytes.data(), 40, first_bytes.data(), 8}}};
        std::array<std::span<const u8>, 10> resources{};
        std::array<u32, 10> counts{};
        RuntimeBodyResourceHash* hashes = nullptr;
        u32 hash_count = 0;
        ResourceErrorState error{};
        // The third raw size intentionally includes the storage header's first dword.
        put(first_bytes, 0, 0);
        acquire_runtime_body_resource_hashes(parameters, resources, counts, hashes,
            hash_count, registry, sizes, error);
        require(counts[0] == 1 && counts[1] == 2 && counts[2] == 6 && hash_count == 6,
            "body hash acquisition must use exact size terms and grow past four rows");
        for (u32 i = 0; i < 6; ++i)
        {
            require(hashes[i].key == i + 1 && hashes[i].first == 0 && hashes[i].second == 0,
                "body hashes must skip slot zero, deduplicate and keep first allocation's first row only");
        }
        put(first_bytes, 0, 7);
        require(runtime_body_resource_size(third_bytes.data(), registry.records, sizes) == 55
            && runtime_body_resource_size(body.data(), registry.records, sizes) == 0,
            "resource size must add optional storage dword and return zero for no identity match");
        release_runtime_body_resource_hashes(resources, hashes, hash_count, registry);
        require(hashes == nullptr && hash_count == 0 && counts[2] == 6
            && find_animation_resource(registry.records, records[0].name.c_str())->references == 0
            && find_animation_resource(registry.records, records[1].name.c_str())->references == 0
            && find_animation_resource(registry.records, records[2].name.c_str())->references == 0,
            "hash cleanup must release all references without clearing resource counts");

        ActorAttachmentList first_list{}, second_list{};
        // Each list here stands for one raw list: new nodes link at its own head.
        first_list.pending = &first_list.sentinel;
        second_list.pending = &second_list.sentinel;
        auto* primary = actor_create_attachment(first_list, "primary", 0, 0);
        auto* secondary = actor_create_attachment(second_list, "secondary", 0, 0);
        const u16 secondary_token = 2;
        std::memcpy(secondary->bytes.data() + 0x18, &secondary_token, 2);
        put(secondary->bytes, 8, 0);
        std::array<u8, 32> timed{};
        ActorEffectNode timed_effect{std::span<u8>(timed), {}, {}, nullptr, 0};
        std::array<ActorEffectNode*, 1> timed_rows{&timed_effect};
        auto* first_head = &first_list.sentinel;
        auto* second_head = &second_list.sentinel;
        RuntimeBodyAttachmentLists lists{first_head, second_head, timed_rows};
        require(remove_runtime_body_attachment(1, lists) && first_list.sentinel.next == nullptr
            && !remove_runtime_body_attachment(99, lists),
            "primary attachment removal must unlink its real sentinel and report absent tokens");
        put(body, 0x2D8, 1);
        put(body, 0x2DC, 2);
        clear_runtime_body_attachments(body, lists);
        require(second_list.sentinel.next == nullptr && read_u32(body, 0x2D8) == 0
            && timed[0x18] == 8 && read_u32(timed, 8) == 0xBF800000,
            "secondary attachment removal must invalidate its timed record before unlinking");
        (void)primary;
        Actor actor{};
        Actor attached_actor{};
        auto* host_bind = ensure_runtime_actor_bind(actor);
        ensure_runtime_actor_bind(attached_actor);
        u32 actor_address = 0x1111;
        auto* flags = allocate_runtime_body_bits(0x41);
        flags->bytes[4] = 0xFF;
        std::array<u8, 0xF00> attached_bytes{};
        attached_bytes[0x93D] = 3;
        std::vector<u32> messages;
        RuntimeBodyAttachedActor attached{attached_bytes, 0x2222,
            [&](std::span<const u32> message)
            {
                require(message[1] == 0x1111 && message[2] == 0,
                    "detach notices retain the original actor token and zero third word");
                messages.push_back(message[0]);
                actor_address = 0x3333;
            }};
        auto* attached_pointer = &attached;
        attached.actor = &attached_actor;
        RuntimeBodyAnimationReceiver* receiver = nullptr;
        RuntimeBodyActorAttachment attachment{attached_pointer, actor_address, actor, flags, receiver};
        body[0x30B] = 4;
        require(detach_runtime_body_actor(body, attachment) == &attached
            && attached_pointer == nullptr && messages == std::vector<u32>{0x9D, 0x9C}
            && flags->bytes[4] == 0xFB && body[0x30B] == 3,
            "body detach must preserve message order, clear binding and decrement class-three byte");
        require(attached_actor.bind == nullptr && actor.bind == host_bind && read_u32(body, 0x48) == 0,
            "body detach must receive attached actor in EBX and preserve host actor bind");
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
        std::span<const u8> animation_resource;
        const std::unordered_map<const u8*, const u8*> sequences;
        auto* groups = static_cast<RuntimeBodyAnimationGroup*>(engine_heap_allocate(sizeof(RuntimeBodyAnimationGroup)));
        RuntimeBodyBitStorage* animation_bits = allocate_runtime_body_bits(9);
        std::array<RuntimeBodyResourceCacheEntry, 1> cache{{{1, 1, nullptr, groups}}};
        i32 cache_count = 1;
        u8 group_count = 1;
        RuntimeBodyAnimationSetup animation{animation_resource, sequences, groups, group_count,
            animation_bits, cache, cache_count, registry, error};
        RuntimeBodyCollisionState* collision = nullptr;
        std::span<const u8> attributes, actions;
        RuntimeBodyShapeStorage shapes{};
        shapes.special = static_cast<RuntimeBodyCollisionSphere*>(engine_heap_allocate(sizeof(RuntimeBodyCollisionSphere)));
        std::array<u8, 0xF00> health_actor{};
        std::array<u8, 12> curve{};
        std::array<u8, 1> tags{};
        std::array<std::span<const u8>, 4> textures{};
        RuntimeBodyHealth health{health_actor, curve, tags, textures, registry, error};
        FactoryObject first_action, second_action;
        ObjectFactoryRegistry factory{};
        RuntimeParentBodyCleanup cleanup{animation, attachment, resources, hashes, hash_count,
            lists, collision, attributes, shapes, health, first_action, second_action, actions, factory};
        destroy_runtime_parent_body(body, cleanup);
        // Exercise the whole parent constructor with real translated callees, then its destructor.
        error.suppressed = 1;
        actor.position = {3.f, 4.f, 5.f};
        actor.scale_200 = 2.f;
        actor.scale_x = 1.f;
        actor.radius_y = 2.f;
        actor.radius_xz = 3.f;
        actor.class_key = 5;
        health_actor[6] = 5;
        actor.xform_valid = 1;
        put(parameters, 0x18C, std::bit_cast<u32>(0.25f));
        put(parameters, 0x1B0, 0x4444);
        put(parameters, 0x190, 0x7FC01234);
        parameters[0x1A9] = 0x89;
        parameters[0x1AA] = 3;
        body[0x2D2] = 0xA5;
        FactoryObject action_object;
        std::array<ExecutableResourcePool, 0> executable_pools{};
        RuntimePlayerChildContext child{factory, executable_pools, 0x4444};
        RuntimeBodyAction action{action_object, child, {}};
        RuntimeBodyAnimationReceiver* second_receiver = nullptr;
        const u8* selected_row = nullptr;
        const RuntimeScriptLookupTable* action_table = nullptr;
        const std::unordered_map<u32, const char*> names;
        ActorAnimationState animation_state;
        u32 random = 7;
        RuntimeBodyActionSelection selection{animation_state, attached_pointer, receiver, second_receiver,
            first_action, second_action, selected_row, action_table, names, child, registry, error, random, 0x5555};
        std::array<u8, 16> shape_data{};
        shape_data[12] = 0x11;
        const RuntimeBodyModelNode model_node{4, shape_data.data()};
        const RuntimeBodyModelNodes nodes{{&model_node, 1}};
        std::array<u8, 0x1D0> channel_bytes{};
        put(channel_bytes, 0x1CC, 1);
        const std::unordered_map<const u8*, const RuntimeScriptLookupTable*> tables;
        RuntimeParentBodyConstruction construction{cleanup, selection, action, health_actor, &nodes,
            channel_bytes.data(), {}, 0, {}, 0, curve, tags, counts, sizes, tables};
        u32 expected_random = 7;
        const float value = (0.25f * fraction(expected_random) - 0.125f) + 1.f;
        require(initialize_runtime_parent_body(body, parameters, construction)
            && random == expected_random && actor.scale_x == value && actor.radius_y == 2.f * value
            && actor.radius_xz == 3.f * value && read_u32(body, 8) == 0x4444,
            "complete body constructor must apply its exact shared random scale perturbation");
        require(read_f32(body, 0x18) == 3.f && read_f32(body, 0x1C) == 4.f
            && read_f32(body, 0x20) == 5.f && read_u32(body, 0x884) == 0x7FC01234
            && read_u32(body, 0x738) == 0x89 && read_u32(body, 0x71C) == 0x3F0EFA35
            && read_u32(body, 0x720) == 0x41490FDB && read_u32(body, 0x724) == 0x40C00000
            && body[0x2D2] == 0xA5 && body[0x92] == 1 && read_u32(body, 0xFC) == 1,
            "complete body constructor must preserve raw copies, conditional byte and exact constants");
        require(flags != nullptr && flags->bit_count == 0x41 && flags->bytes[3] == 0x20
            && collision != nullptr && collision->count == 1 && body[0x714] == 1 && body[0x715] == 0
            && reinterpret_cast<const u8*>(shapes.special->node) == shape_data.data() + 8 && hash_count == 6,
            "complete body constructor must own real bit, collision, special shape and hash allocations");
        destroy_runtime_parent_body(body, cleanup);
        const std::array<u16, 1> excluded{5};
        construction.scale_exclusions = excluded;
        construction.scale_exclusion_count = 1;
        parameters[0x1BC] = 1;
        const u32 retained_random = random;
        const float retained_scale = actor.scale_x;
        initialize_runtime_parent_body(body, parameters, construction);
        require(random == retained_random && actor.scale_x == retained_scale && body[0x2D2] == 0,
            "excluded body class must skip randomness and nonzero parameter must clear its conditional byte");
        destroy_runtime_parent_body(body, cleanup);
        require(groups == nullptr && animation_bits == nullptr && flags == nullptr && cache_count == 0
            && shapes.special == nullptr && body[0x714] == 0 && group_count == 1,
            "complete parent destructor must release owned storage while preserving group count byte");
        destroy_runtime_parent_body(body, cleanup);
    }
    {
        std::array<u8, 0x950> body{};
        std::array<u8, 0x108> row{};
        row[0] = 7;
        const RuntimeScriptLookupTable action_table{1, row.data()};
        const RuntimeScriptLookupTable* table = &action_table;
        ActorAnimationState animation;
        RuntimeBodyAttachedActor* attached = nullptr;
        RuntimeBodyAnimationReceiver* first_receiver = nullptr;
        RuntimeBodyAnimationReceiver* second_receiver = nullptr;
        FactoryObject first, second;
        const u8* selected_row = nullptr;
        const std::unordered_map<u32, const char*> names{{1, "missing"}};
        ObjectFactoryRegistry factory{};
        std::array<ExecutableResourcePool, 0> pools{};
        RuntimePlayerChildContext child{factory, pools, 0x7777};
        AnimationResourceRegistry registry;
        ResourceErrorState error{};
        u32 random = 5;
        RuntimeBodyActionSelection context{animation, attached, first_receiver, second_receiver,
            first, second, selected_row, table, names, child, registry, error, random, 0x9999};
        put(body, 0x84, 0xFFFFFFFF);
        body[0x6C] = 9;
        put(row, 0x104, 1);
        require(!select_runtime_body_action(body, 0, context) && !select_runtime_body_action(body, 8, context)
            && select_runtime_body_action(body, 7, context) && selected_row == row.data()
            && read_u16(body, 0x748) == 7 && read_u32(body, 0x84) == 0 && body[0x6C] == 0,
            "body action selection must find exact row and apply the zero-gate reset");
        std::vector<u32> notices;
        RuntimeBodyAnimationReceiver second_notice{[&](const RuntimeBodyAnimationNotice& notice)
        {
            require(notice.id == 0x28 && notice.actor == 0x7777 && notice.record == nullptr
                && notice.position == 0.f && notice.zero == 0, "animation notice must carry empty-controller fields");
            notices.push_back(2);
        }};
        RuntimeBodyAnimationReceiver first_notice{[&](const RuntimeBodyAnimationNotice&)
        {
            notices.push_back(1);
            second_receiver = nullptr;
        }};
        first_receiver = &first_notice;
        second_receiver = &second_notice;
        require(select_runtime_body_animation("missing", context) == nullptr
            && notices == std::vector<u32>{1, 2} && random == 5,
            "animation notification must snapshot both recipients before the first callback");
        first_receiver = nullptr;
        put(body, 0x74C, 1);
        put(body, 0x84, 123);
        select_runtime_body_action(body, 7, context);
        require(read_u32(body, 0x84) == 123, "nonzero action gate must preserve body animation field");
        static const ObjectFactoryVtable action_vtable{0,
            [](ObjectSizes& sizes)
            {
                sizes = {16, 16};
            },
            [](FactoryObject& object, std::span<const u8> parameters)
            {
                initialize_base_object(object, parameters);
                std::memcpy(object.bytes.get() + 8, parameters.data() + 8, 4);
                return true;
            }, nullptr};
        const std::array<ObjectFactoryType, 2> types
        {{{0, nullptr}, {0, []() -> const ObjectFactoryVtable*
        {
            return &action_vtable;
        }}}};
        factory.types = types;
        std::array<u8, 16> first_template{}, second_template{};
        first_template[0] = second_template[0] = 16;
        first_template[4] = second_template[4] = 1;
        first_template[2] = 0x11;
        second_template[2] = 0x22;
        put(first_template, 8, 0x11111111);
        put(second_template, 8, 0x22222222);
        const RuntimeBodyActionTemplates templates{second_template, first_template};
        std::strcpy(reinterpret_cast<char*>(row.data() + 2), "aid_second_action");
        std::strcpy(reinterpret_cast<char*>(row.data() + 0x82), "aid_first_action");
        select_runtime_body_action_row(body, row.data(), &templates, context);
        require(factory.count == 2 && first.bytes[6] == 0x11 && second.bytes[6] == 0x22
            && read_u32({first.bytes.get(), first.size}, 8) == 0x9999
            && read_u32({second.bytes.get(), second.size}, 8) == 0x9999
            && read_u32(first_template, 8) == 0x11111111,
            "action creators must clone their distinct template slots and override body token without mutating sources");
        row[2] = row[0x82] = 0;
        select_runtime_body_action_row(body, row.data(), nullptr, context);
        require(first.bytes == nullptr && second.bytes == nullptr && factory.count == 0,
            "row selection must destroy both old actions before testing replacement names");
    }
    {
        std::array<u8, 0x30> animation{};
        const std::array<u16, 10> sequence_words{11, 12, 0, 21, 0, 31, 32, 0, 0, 0};
        const u8* sequence = reinterpret_cast<const u8*>(sequence_words.data());
        const std::unordered_map<const u8*, const u8*> sequences{{animation.data(), sequence}};
        std::span<const u8> resource = animation;
        put(animation, 0xC, 9);
        put(animation, 0x10, 0x12345678);
        animation[4] = 1;
        animation[6] = 3;
        animation[0x1C] = 10;
        std::array<RuntimeBodyResourceCacheEntry, 8> cache{};
        i32 cache_count = 0;
        RuntimeBodyAnimationGroup* groups = nullptr;
        RuntimeBodyBitStorage* bits = nullptr;
        u8 group_count = 0;
        AnimationResourceRegistry registry;
        ResourceErrorState error{};
        RuntimeBodyAnimationSetup setup{resource, sequences, groups, group_count, bits,
            cache, cache_count, registry, error};
        std::array<u8, 0x200> parameters{};
        std::array<u8, 0x950> body{};
        std::array<u8, 8> actor{};
        actor[6] = 5;
        std::array<u8, 16> classes{};
        classes[0] = 4;
        classes[8] = 5;
        put(classes, 12, 0xABCDEF01);
        put(parameters, 0x198, 0x7FC01234);
        parameters[0x1A4] = 9;
        initialize_runtime_body_animation_settings(body, parameters, actor, classes, 2, setup);
        require(cache_count == 1 && group_count == 2 && groups[0].length == 2
            && groups[0].count == 2 && groups[1].length == 1 && groups[1].count == 1,
            "body animation must group sequences by length in first-seen order");
        require(groups[0].values[0] == sequence && groups[0].values[2] == sequence + 10
            && groups[1].values[0] == sequence + 6,
            "body animation groups must retain pointers to individual sequence words");
        require(bits != nullptr && bits->bit_count == 10 && bits->byte_count == 2
            && bits->bytes[0] == 0 && bits->bytes[1] == 0,
            "body animation bits must use maximum resource count and clear rounded bytes");
        require(read_u32(body, 0x70) == 0xABCDEF01 && read_u32(body, 0x78) == 0x7FC01234
            && read_u16(body, 0x6E) == 9, "body settings must preserve raw parameter bits and class value");
        engine_heap_free(bits);
        bits = nullptr;
        auto* shared = groups;
        groups = nullptr;
        group_count = 0;
        acquire_runtime_body_animation(parameters, setup);
        require(groups == shared && group_count == 2 && cache_count == 1 && cache[0].references == 2,
            "second body must retain cached group storage without rebuilding");
        require(!release_runtime_body_cached_resource(groups, cache, cache_count)
            && release_runtime_body_cached_resource(groups, cache, cache_count) && cache_count == 0,
            "group cache must release shared storage only after its last body");
        engine_heap_free(groups);
        engine_heap_free(bits);
        bits = allocate_runtime_body_bits(0);
        require(bits != nullptr && bits->bit_count == 0 && bits->byte_count == 0,
            "zero-bit allocation must retain a real header");
        engine_heap_free(bits);
        std::array<u8, 4 + 3 * 0x34> records{};
        put(records, 0, 3);
        records[4] = 4;
        records[4 + 0x34] = 5;
        records[4 + 2 * 0x34] = 4;
        const RuntimeBodyModelNode node{2, records.data()};
        const RuntimeBodyModelNodes nodes{{&node, 1}};
        require(count_runtime_body_model_kind(&nodes, 4) == 2
            && count_runtime_body_model_kind(&nodes, 8) == 0
            && count_runtime_body_model_kind(nullptr, 4) == 0,
            "model-kind count must use node two and 0x34-byte records");
    }
    {
        std::array<u8, 0x950> body{};
        std::array<u8, 0xF00> actor{};
        std::array<u8, 0x200> parameters{};
        std::array<u8, 12> curve{};
        std::array<u8, 0x87> tags{};
        curve[0] = 10;
        curve[1] = 20;
        put(curve, 4, std::bit_cast<u32>(4.f));
        put(curve, 8, std::bit_cast<u32>(12.f));
        actor[0xCF4] = 1;
        tags[0x50] = 15;
        parameters[0x1AA] = 0x81;
        initialize_runtime_body_friction(body, parameters, actor, curve, tags);
        require(read_f32(body, 0x170) == 2.f && read_u32(body, 0x16C) == 0x3F7D70A4
            && read_u32(actor, 0x120) == 0x81 && body[0x244] == 1,
            "friction must interpolate its own tag column and retain raw damping bits");
        tags[0x50] = 30;
        initialize_runtime_body_friction(body, parameters, actor, curve, tags);
        require(read_f32(body, 0x170) == 3.f, "friction must clamp its upper endpoint");
        initialize_runtime_body_model_channels(body, nullptr);
        require(read_u32(body, 0xFC) == 0 && read_f32(body, 0xE4) == 1.f
            && read_f32(body, 0xF0) == 1.f, "null model must initialize both channels");
        std::array<u8, 0x1D0> model{};
        put(model, 0x1CC, 9);
        initialize_runtime_body_model_channels(body, model.data());
        require(read_u32(body, 0xFC) == 1, "model channel zero must enable body channels");
        std::array<RuntimeBodyResourceCacheEntry, 3> entries
        {{ {2, 7, nullptr, actor.data()}, {1, 9, nullptr, body.data()}, {0, 11, nullptr, model.data()} }};
        i32 count = 3;
        require(!release_runtime_body_cached_resource(actor.data(), entries, count)
            && entries[0].references == 1 && count == 3, "retained cache entry must survive");
        require(release_runtime_body_cached_resource(body.data(), entries, count)
            && count == 2 && entries[1].resource == model.data() && entries[1].first == 11,
            "last-reference cache release must compact the entire last row");
        require(!release_runtime_body_cached_resource(model.data(), entries, count)
            && entries[1].references == 0xFFFFFFFFu, "zero cache reference must wrap without invented guard");
        std::array<u8, 24> shapes{};
        shapes[9] = 8;
        shapes[12] = 1;
        shapes[20] = 1;
        const RuntimeBodyModelNode node{4, shapes.data()};
        const RuntimeBodyModelNodes nodes{{&node, 1}};
        auto* state = create_runtime_body_collision_state(&nodes);
        require(state != nullptr && state->count == 2 && state->records[1].shape == shapes.data() + 16
            && state->records[1].bytes[0x16] == 1 && state->records[1].bytes[0x17] == 1
            && read_f32(state->records[1].bytes, 0x18) == 1.f,
            "collision state must include matching terminal shape and initialize records");
        auto filtered = std::make_unique<Actor>();
        std::array<RuntimeBodyCollisionOverride, 2> overrides
        {{ {filtered.get(), 0x12345678, 3, 1}, {filtered.get(), 0x87654321, 4, 0} }};
        GhoulySpawnPool pool{};
        pool.begin = reinterpret_cast<u8*>(overrides.data());
        pool.cursor = reinterpret_cast<u8*>(overrides.data() + overrides.size());
        restore_runtime_body_collision_overrides(pool);
        require(filtered->model_flags == 0x12345678 && overrides[0].active == 0
            && overrides[1].retained == 4, "collision restore must touch only active actor filters");
        destroy_runtime_body_collision_state(state);
        shapes[12] = 0;
        shapes[20] = 0;
        require(create_runtime_body_collision_state(&nodes) == nullptr,
            "model without matching collision shapes must return null");
    }
    RuntimeBodyMotionController controller{};
    MotionDispatchFixture fixture{controller, {}};
    register_runtime_body_motion_callback(controller, 10, first_motion_callback, &fixture);
    require(controller.count == 1 && read_u16({controller.joints->begin, 4}, 2) == 0xFFFF,
        "motion controller sentinel and initial count");
    dispatch_runtime_body_motion_callbacks(nullptr, nullptr, nullptr, 0, &controller, 0.25f);
    require(fixture.calls == std::vector<u16>{10, 14}, "motion dispatch must reread second callback and joint");
    register_runtime_body_motion_callback(controller, 14, second_motion_callback, &fixture);
    require(controller.count == 1, "duplicate joint replaces first callback without insertion");
    register_runtime_body_motion_callback(controller, 5, second_motion_callback, &fixture);
    register_runtime_body_motion_callback(controller, 20, second_motion_callback, &fixture);
    require(controller.count == 3 && read_u16({controller.joints->begin, 8}, 0) == 5
        && read_u16({controller.joints->begin, 8}, 2) == 14
        && read_u16({controller.joints->begin, 8}, 4) == 20
        && read_u16({controller.joints->begin, 8}, 6) == 0xFFFF,
        "motion insertion must move sorted joints and preserve sentinel during growth");
    fixture.calls.clear();
    dispatch_runtime_body_motion_callbacks(nullptr, nullptr, nullptr, 1, &controller, 0.25f);
    require(fixture.calls == std::vector<u16>{14, 14}, "motion insertion must move both callback fields together");
    engine_heap_free(controller.joints);
    engine_heap_free(controller.dispatch);
    engine_heap_free(controller.callbacks);
    std::array<float, 12> matrix{};
    matrix[0] = matrix[5] = matrix[10] = 1.f;
    float roll, pitch, yaw;
    extract_runtime_body_angles(matrix.data(), roll, pitch, yaw);
    require(roll == 0.f && pitch == 0.f && yaw == 0.f, "identity motion orientation");
    matrix[6] = 2.5f;
    extract_runtime_body_angles(matrix.data(), roll, pitch, yaw);
    require(std::abs(pitch - float(retail_asin(0.5f))) < 0.00001f, "pitch wraps by one before asin");
    matrix[6] = -1.f;
    extract_runtime_body_angles(matrix.data(), roll, pitch, yaw);
    require(std::bit_cast<u32>(pitch) == 0xBFC90FDB, "negative pitch endpoint");
    matrix[1] = matrix[5] = 0.f;
    matrix[4] = 1.f;
    extract_runtime_body_angles(matrix.data(), roll, pitch, yaw);
    require(roll == float(retail_atan2(-1.f, 1.f)), "roll zero-pair fallback");
    require(constrain_runtime_body_angle(2.f, 0.f, 0.5f) == 0.5f
        && constrain_runtime_body_angle(-2.f, 0.f, 0.5f) == -0.5f
        && constrain_runtime_body_angle(0.25f, 0.f, 0.5f) == 0.25f,
        "body angular constraint directions and interior preservation");
    const float pi = std::bit_cast<float>(0x40490FDBu);
    require(constrain_runtime_body_angle(pi, pi, 0.f) == -pi, "positive pi normalizes to negative pi");
    require(runtime_body_angle_distance(0.25f, 0.75f, 0.5f) == 0.5f,
        "angle distance sums the two normalized anchor legs");
    float current = 0.f, velocity = 0.f;
    const float positive_target = 1.f;
    accelerate_runtime_body_angle(current, positive_target, velocity, 0.f, 2.f, 2.f, 0.25f);
    require(current == 0.125f && velocity == 0.5f, "positive angular acceleration");
    current = velocity = 0.f;
    const float negative_target = -1.f;
    accelerate_runtime_body_angle(current, negative_target, velocity, 0.f, 2.f, 2.f, 0.25f);
    require(current == -0.125f && velocity == -0.5f, "negative angular acceleration");
    current = 0.9f; velocity = 1.f;
    accelerate_runtime_body_angle(current, positive_target, velocity, 0.f, 2.f, 1.f, 0.25f);
    require(current == 1.f && velocity == 0.f, "angular target crossing clamps and stops");
    current = 1.f; velocity = -0.1f;
    accelerate_runtime_body_angle(current, positive_target, velocity, 0.f, 2.f, 1.f, 0.25f);
    require(current == 1.f && velocity == 0.f, "zero distance brakes velocity without advancing angle");
    std::array<float, 12> local{}, parent{}, output{};
    local[0] = local[5] = local[10] = 1.f;
    parent[0] = parent[5] = parent[10] = 1.f;
    local[3] = 3.f;
    parent[7] = 4.f;
    alignas(float) std::array<u8, 0x30> joint_record{};
    put(joint_record, 0x1C, std::bit_cast<u32>(-1.f));
    put(joint_record, 0x2C, std::bit_cast<u32>(-1.f));
    update_runtime_body_joint_motion(local.data(), parent.data(), output.data(), 5, joint_record.data(), 0.25f);
    require(output[3] == 3.f && output[7] == 4.f, "zero-record joint still multiplies matrices");
    const u16 enabled = 1;
    std::memcpy(joint_record.data(), &enabled, 2);
    const auto disabled_record = joint_record;
    update_runtime_body_joint_motion(local.data(), parent.data(), output.data(), 5, joint_record.data(), 0.25f);
    require(joint_record == disabled_record, "minus-one limits skip both joint adjustments");
    put(joint_record, 0x1C, 0);
    put(joint_record, 0x2C, 0);
    put(joint_record, 0x20, std::bit_cast<u32>(1.f));
    put(joint_record, 0x28, std::bit_cast<u32>(2.f));
    update_runtime_body_joint_motion(local.data(), parent.data(), output.data(), 5, joint_record.data(), 0.25f);
    require(read_f32(joint_record, 0x24) == 0.f && read_f32(joint_record, 0x28) == 0.f,
        "zero-weight joint copies constrained pitch and clears velocity");
    std::array<u8, 0x820> motion_parameters{};
    for (size_t offset : {0x800u, 0x80Cu, 0x7F4u, 0x7E8u, 0x7D0u, 0x7DCu})
    {
        put(motion_parameters, offset, 0xFFFFFFFF);
        put(motion_parameters, offset + 4, std::bit_cast<u32>(-1.f));
        put(motion_parameters, offset + 8, std::bit_cast<u32>(-1.f));
    }
    std::array<u8, 64> model_bytes{};
    put(model_bytes, 0, 8); put(model_bytes, 4, 1);
    put(model_bytes, 8, 9); put(model_bytes, 12, 32);
    model_bytes[32] = 3;
    ActorModel model{model_bytes};
    RuntimeBodyMotionController* primary = nullptr;
    RuntimeBodyMotionController* secondary = nullptr;
    u32 selector = 9;
    std::array<u8*, 6> records{};
    RuntimeBodyShapeStorage shape_storage{};
    RuntimePlayerBodyMotion motion{&model, primary, secondary, selector, records, nullptr, shape_storage};
    ObjectSizes parent_sizes{};
    ObjectSizes child_sizes{};
    parent_player_body_object_sizes(parent_sizes);
    player_body_object_sizes(child_sizes);
    require(parent_sizes.instance == 0x924 && child_sizes.instance == 0x950
        && parent_sizes.parameters == 0x820 && child_sizes.parameters == 0x820,
        "type 0x27 and its player child retain distinct instance sizes");
    std::array<u8, 0x950> player_body{};
    put(motion_parameters, 0x800, 0);
    put(motion_parameters, 0x804, std::bit_cast<u32>(0.5f));
    put(motion_parameters, 0x808, std::bit_cast<u32>(1.5f));
    put(motion_parameters, 0x818, std::bit_cast<u32>(10.f));
    put(motion_parameters, 0x81C, std::bit_cast<u32>(20.f));
    require(initialize_runtime_player_body(player_body, motion_parameters, motion), "player body child constructor return");
    require(primary == secondary && selector == 0 && primary->count == 1 && records[0] != nullptr
        && records[1] == nullptr && read_u16({records[0], 0x30}, 0) == 3
        && read_f32({records[0], 0x30}, 4) == 1.f && read_f32({records[0], 0x30}, 8) == 2.f
        && read_f32({records[0], 0x30}, 0x2C) == 0.5f && read_f32({records[0], 0x30}, 0x1C) == 1.5f
        && read_u32(player_body, 0x258) == 1, "player joint setup creates actual controller and callback records");
    for (size_t offset : {0x800u, 0x80Cu, 0x7F4u, 0x7E8u, 0x7D0u, 0x7DCu})
    {
        put(motion_parameters, offset, 0xFFFFFFFF);
    }
    put(motion_parameters, 0x7C8, 0x7FC00001);
    u8* saved_record = records[0];
    initialize_runtime_player_body(player_body, motion_parameters, motion);
    require(records[0] == saved_record && read_u32(player_body, 0x258) == 1,
        "zero configured joints preserve records and unordered body factor skips count increment");
    std::array<u8, 12> missing_joint{};
    put(missing_joint, 0, 0xFFFFFFFF);
    put(missing_joint, 4, std::bit_cast<u32>(2.f));
    put(missing_joint, 8, std::bit_cast<u32>(3.f));
    float first_accumulator = 1.f, second_accumulator = 4.f;
    u8* unused_cursor = nullptr;
    require(append_runtime_player_joint_motion(missing_joint, unused_cursor, first_accumulator,
        second_accumulator, 1.f, 1.f, motion) == nullptr
        && first_accumulator == 3.f && second_accumulator == 7.f,
        "missing joint preserves newly accumulated values without allocating a record");
    shape_storage.selected = static_cast<const u8**>(engine_heap_allocate(4 * sizeof(const u8*)));
    shape_storage.selected_state = static_cast<u8*>(engine_heap_allocate(0x20));
    shape_storage.selected_count = 9;
    player_body[0x8B0] = 3;
    put(player_body, 0x89C, 0x100);
    put(player_body, 0x8A0, 0x200);
    put(player_body, 0x8AC, 0x300);
    put(player_body, 0x910, 0x400);
    records[5] = records[0] + 0x30;
    destroy_runtime_player_body(player_body, shape_storage, records);
    require(shape_storage.selected == nullptr && shape_storage.selected_state == nullptr
        && shape_storage.selected_count == 9 && player_body[0x8B0] == 3
        && read_u32(player_body, 0x89C) == 0 && read_u32(player_body, 0x8A0) == 0
        && read_u32(player_body, 0x8AC) == 0 && read_u32(player_body, 0x910) == 0
        && records[0] == nullptr && records[5] == nullptr,
        "player child destruction frees the lowest live joint slice and preserves counts and latches");
    put(player_body, 0x8C0, 0xABCD);
    destroy_runtime_player_body(player_body, shape_storage, records);
    require(read_u32(player_body, 0x8C0) == 0xABCD,
        "absent native joint slot preserves untouched retail word");
    engine_heap_free(primary->joints); engine_heap_free(primary->dispatch); engine_heap_free(primary->callbacks);
    engine_heap_free(primary);
    {
        std::array<u8, 0x924> target_body{};
        std::array<u8, 8> target_bits{};
        RuntimeBodyBitStorage bit_storage{};
        bit_storage.bytes = target_bits.data();
        RuntimeBodyBitStorage* flags = &bit_storage;
        Actor target_actor;
        target_actor.animation_rate = 1.f;
        float freeze = 0.f, dt = 0.1f, scale = 1.f;
        u32 block = 0, index = 0;
        std::array<u8, 1> enabled{1};
        RuntimeActorClock clock{freeze, dt, scale, block, enabled, index};
        std::array<u8*, 6> target_records{};
        RuntimePlayerJointTargets targets{target_actor, flags, target_records, clock, 0x10001000};
        put(target_body, 0x304, std::bit_cast<u32>(10.f));
        put(target_body, 0x8A8, std::bit_cast<u32>(1.f));
        target_body[0x308] = 1;
        u32 tracking = 0;
        require(select_runtime_player_joint_target(target_body, 0, {}, tracking, targets) == 0.f
            && tracking == 1 && target_body[0x8B0] == 2 && read_f32(target_body, 0x8B4) == 0.2f,
            "joint targeting starts the acquisition timer and zero blend");
        require(std::fabs(select_runtime_player_joint_target(target_body, 0, {}, tracking, targets) - 0.5f) < 0.000001f
            && tracking == 1 && target_body[0x8B0] == 2,
            "joint acquisition blends during the countdown");
        require(select_runtime_player_joint_target(target_body, 0, {}, tracking, targets) == 1.f
            && target_body[0x8B0] == 3, "joint acquisition completes at timer zero");
        target_body[0x308] = 0;
        select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(tracking == 0 && target_body[0x8B0] == 1 && read_f32(target_body, 0x8B4) == 0.3f,
            "losing the target starts the release timer");
        const float released = select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(released > 0.66f && released < 0.67f, "joint release preserves partial blend");
        target_body[0x308] = 1;
        const float acquired = select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(std::fabs(acquired - released) < 0.000001f && target_body[0x8B0] == 2,
            "reacquiring a target converts timer without a blend discontinuity");
        target_body[0x308] = 0;
        const float reversed = select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(std::fabs(reversed - acquired) < 0.000001f && target_body[0x8B0] == 1,
            "losing a partially acquired target preserves blend");
        dt = 1.f;
        require(select_runtime_player_joint_target(target_body, 0, {}, tracking, targets) == 0.f
            && target_body[0x8B0] == 0 && read_f32(target_body, 0x8B4) == 0.f,
            "joint release finishes by clearing the state and timer");
        target_body[0x8B0] = 7;
        require(std::bit_cast<u32>(select_runtime_player_joint_target(target_body, 0, {}, tracking, targets)) == 0x10001000,
            "unknown untracked joint state returns the original body argument word");
        target_body[0x308] = 1;
        put(target_body, 0x2FC, std::bit_cast<u32>(1.f));
        const float unknown = select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(tracking == 1 && unknown == read_f32(target_body, 0x8BC)
            && unknown > 0.09f && unknown < 0.11f,
            "unknown tracked joint state returns the reused yaw stack word");
        constexpr std::array<size_t, 6> disable{2, 2, 1, 0, 3, 3};
        for (size_t slot = 0; slot < 6; ++slot)
        {
            target_body[0x309 + disable[slot]] = 1;
            select_runtime_player_joint_target(target_body, slot, {}, tracking, targets);
            require(tracking == 0, "joint slot uses its retail disable byte");
            target_body[0x309 + disable[slot]] = 0;
        }
        put(target_body, 0x8A8, 0x7FC00001);
        select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(tracking == 1, "unordered joint yaw limit retains the assembly JBE acceptance");
        put(target_body, 0x8A8, std::bit_cast<u32>(-1.f));
        select_runtime_player_joint_target(target_body, 0, {}, tracking, targets);
        require(tracking == 0, "negative joint yaw limit rejects the target");
        put(target_body, 0x8A8, std::bit_cast<u32>(1.f));
        std::array<Joint, 2> joints{};
        RuntimeModelNode skeleton{};
        skeleton.joints = joints;
        ModelPoseBuffer pose;
        pose.count = 2;
        pose.dirty = true;
        pose.matrices.assign(2, AnimationMatrix{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f});
        RuntimeModelInstance runtime{};
        runtime.primary = &pose;
        runtime.skeleton_node = &skeleton;
        ActorModel target_model{};
        target_model.runtime = &runtime;
        target_actor.model = &target_model;
        target_actor.xform[0] = target_actor.xform[5] = target_actor.xform[10] = 1.f;
        std::array<u8, 0x30> target_record{};
        const u16 bone = 1;
        std::memcpy(target_record.data(), &bone, 2);
        put(target_record, 0x2C, std::bit_cast<u32>(-1.f));
        put(target_record, 0x1C, 0x7FC00001);
        put(target_record, 0x20, 0x12345678);
        target_records[0] = target_record.data();
        target_body[0x8B0] = 3;
        target_bits[4] = 0xA5;
        apply_runtime_player_joint_targets(target_body, targets);
        require(target_bits[4] == 0xE5 && read_u32(target_record, 0x20) == 0x12345678
            && read_u32(target_record, 0x10) == read_u32(target_body, 0x8BC)
            && read_f32(target_record, 0xC) == 1.f,
            "joint application preserves the pitch sentinel, accepts NaN yaw and stores tracking blend");
        put(target_body, 0x310, 1);
        put(target_record, 0xC, 0x12345678);
        apply_runtime_player_joint_targets(target_body, targets);
        require(target_bits[4] == 0xA5 && read_u32(target_record, 0xC) == 0x12345678,
            "body joint override clears the tracking bit while skipping all records");
        put(target_body, 0x310, 0);
        std::memcpy(target_record.data(), &index, 2);
        apply_runtime_player_joint_targets(target_body, targets);
        require(read_u32(target_record, 0xC) == 0x12345678,
            "joint index zero is skipped even with a non-null record");
        std::vector<u32> relay_order;
        RuntimePlayerBodyRelayChild* action = nullptr;
        RuntimePlayerBodyRelayChild* attachment = nullptr;
        RuntimePlayerBodyRelayChild new_attachment{[&](u32 message)
        {
            require(message == 0x1234, "relay child receives the original word");
            relay_order.push_back(2);
            target_body[0x308] = 0;
        }};
        RuntimePlayerBodyRelayChild action_view{[&](u32 message)
        {
            require(message == 0x1234, "relay action receives the original word");
            relay_order.push_back(1);
            attachment = &new_attachment;
        }};
        action = &action_view;
        target_bits[4] = 0xE5;
        relay_runtime_player_body(target_body, 0x1234, action, attachment, targets);
        require(relay_order == std::vector<u32>{1, 2} && target_bits[4] == 0xA5,
            "body relay rereads the attachment after action and runs targeting after both children");
        std::array<u8, 0xF00> action_actor{};
        std::array<u8, 2> address_zero{};
        EntityBitBlock action_bits{target_bits.data()};
        target_bits.fill(0);
        require(runtime_player_body_can_select_action(target_body, action_actor, nullptr, action_bits, address_zero),
            "unpaused free body without goals or blocker bits can select an action");
        for (u32 bit : {48u, 44u, 30u, 49u, 46u, 42u, 52u, 54u})
        {
            target_bits[bit >> 3] = u8(1u << (bit & 7));
            require(!runtime_player_body_can_select_action(target_body, action_actor, nullptr, action_bits, address_zero),
                "each retail blocker bit prevents action selection");
            target_bits[bit >> 3] = 0;
        }
        put(action_actor, 0xCD8, 0xFFFFFFFF);
        RuntimePlayerGoalRing empty_goal{4, 1, 0, 1, {}};
        require(runtime_player_body_can_select_action(target_body, action_actor, &empty_goal, action_bits, address_zero),
            "negative pause count passes and empty goal ring reads the actual zero address view");
        address_zero[0] = 1;
        require(!runtime_player_body_can_select_action(target_body, action_actor, &empty_goal, action_bits, address_zero),
            "nonzero goal kind at address zero blocks body action selection");
        address_zero[0] = 0;
        put(target_body, 0x34, 0x7FC00001);
        require(!runtime_player_body_can_select_action(target_body, action_actor, nullptr, action_bits, address_zero),
            "unordered body timer fails the exact zero comparison");
        float direction_yaw = 9.f, direction_pitch = 9.f;
        const float quiet_nan = std::bit_cast<float>(0x7FC00001u);
        require(direction_to_yaw_pitch(0.f, quiet_nan, 0.f, direction_yaw, direction_pitch) == 0
            && direction_yaw == 0.f && direction_pitch == 0.f,
            "direction helper treats unordered magnitudes as quiet through its ordered JA gates");
        require(direction_to_yaw_pitch(0.f, quiet_nan, 1.f, direction_yaw, direction_pitch) == 1
            && direction_yaw == 0.f && std::isnan(direction_pitch),
            "live horizontal length still reaches atan2 for an unordered vertical component");
        require(direction_to_yaw_pitch(1.f, -1.f, std::bit_cast<float>(0xFF800000u),
            direction_yaw, direction_pitch) == 1
            && direction_pitch == std::bit_cast<float>(0x40490FDBu),
            "direction helper retains the retail square-root and atan2 infinity behavior");
    }
    require(ease_runtime_body_angle(1.f, 0.f, 0.f, 2.f, 1.f, 0.25f) == 0.25f
        && ease_runtime_body_angle(1.f, 0.f, 0.5f, 2.f, 0.1f, 0.25f) == 0.125f
        && ease_runtime_body_angle(1.f, 0.f, 0.f, 0.5f, 2.f, 0.25f) == 0.125f,
        "angle easing applies gain then minimum and maximum speed");
    require(ease_runtime_body_angle(1.f, 0.f, 0.f, 2.f, 1.f, 2.f) == 0.f,
        "positive overshoot retains current angle rather than snapping to target");
    const float circle = std::bit_cast<float>(0x40C90FDBu);
    require(ease_runtime_body_angle(-1.f, 0.f, 0.f, 2.f, 1.f, 0.25f) == circle - 0.25f
        && ease_runtime_body_angle(-1.f, 0.f, 0.f, 2.f, 1.f, 2.f) == circle - 2.f,
        "negative step uses the signed overshoot comparison and wraps into a positive turn");
    require(std::isnan(ease_runtime_body_angle(std::bit_cast<float>(0x7FC00001u),
        0.f, 0.f, 2.f, 1.f, 0.25f)), "unordered target reaches remainder arithmetic");
    {
        Actor rotation_actor;
        Actor output_actor;
        output_actor.euler = {3.f, 4.f, 5.f};
        ActorEulerOut output_view;
        output_view.actor = &output_actor;
        rotation_actor.euler_out = &output_view;
        rotation_actor.animation_rate = 1.f;
        rotation_actor.xform_valid = 7;
        float freeze = 0.f, dt = 0.25f, scale = 1.f;
        u32 block = 0, decay_index = 0;
        std::array<u8, 1> decay{1};
        RuntimeActorClock clock{freeze, dt, scale, block, decay, decay_index};
        std::array<u8, 0x950> rotation_body{};
        put(rotation_body, 0x720, std::bit_cast<u32>(2.f));
        put(rotation_body, 0x724, std::bit_cast<u32>(1.f));
        put(rotation_body, 0x72C, std::bit_cast<u32>(1.f));
        put(rotation_body, 0x730, std::bit_cast<u32>(-1.f));
        ease_runtime_body_rotation(rotation_body, rotation_actor, clock);
        require(rotation_actor.xform_valid == 7 && output_view.euler.x == 0.f,
            "disabled rotation preserves transform and output");
        put(rotation_body, 0x728, 1);
        ease_runtime_body_rotation(rotation_body, rotation_actor, clock);
        require(rotation_actor.euler.x == 0.25f && std::fabs(rotation_actor.euler.y + 0.25f) < 0.000001f
            && rotation_actor.xform_valid == 0 && output_view.euler.x == 3.f
            && output_view.euler.y == 4.f && output_view.euler.z == 5.f,
            "rotation normalizes local angles and rereads the actual body output owner");
        require(read_f32(rotation_body, 0x72C) == 3.f && read_f32(rotation_body, 0x730) == 4.f
            && read_f32(rotation_body, 0x734) == 5.f,
            "rotation writes the actor +0x95C body pose at +0x72C");
        rotation_actor.animation_pause = 1.f;
        ease_runtime_body_rotation(rotation_body, rotation_actor, clock);
        require(rotation_actor.euler.x == 0.25f, "paused actor has zero rotation step");
    }
    matrix.fill(0.f);
    matrix[0] = matrix[5] = matrix[10] = 1.f;
    matrix[3] = 3.f; matrix[7] = 4.f; matrix[11] = 5.f;
    adjust_runtime_body_pitch(matrix.data(), 0.5f, 1.f);
    require(matrix[6] == 0.f - float(retail_sin(0.5f)) && matrix[5] == float(retail_cos(0.5f))
        && matrix[3] == 3.f && matrix[7] == 4.f && matrix[11] == 5.f,
        "pitch adjustment postmultiplies the axis rotation and preserves translation");
    matrix.fill(0.f);
    matrix[0] = matrix[5] = matrix[10] = 1.f;
    adjust_runtime_body_yaw(matrix.data(), 0.5f, 1.f);
    require(matrix[8] == 0.f - float(retail_sin(0.5f)) && matrix[0] == float(retail_cos(0.5f)),
        "yaw adjustment uses target minus extracted yaw");
    const auto preserved = matrix;
    adjust_runtime_body_yaw(matrix.data(), -2.f, 0.f);
    require(matrix == preserved, "zero yaw adjustment weight preserves the matrix");
    std::array<u8, 0x950> body{};
    std::array<u8, 0x1B0> parameters{};
    std::array<u8, 0xF00> actor{};
    std::array<u8, 12> curve{};
    std::array<u8, 0x87> tags{};
    std::array<std::span<const u8>, 4> textures{};
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    error.suppressed = 1;
    RuntimeBodyHealth health{actor, curve, tags, textures, registry, error};
    curve[0] = 10;
    curve[1] = 30;
    put(curve, 4, std::bit_cast<u32>(20.f));
    put(curve, 8, std::bit_cast<u32>(100.f));
    const u16 tag = 1;
    std::memcpy(actor.data() + 0xCF4, &tag, 2);
    parameters[0x1A8] = 7;
    for (const auto [value, expected] : {std::pair{u8(0), u16(20)}, {u8(20), u16(60)}, {u8(255), u16(100)}})
    {
        tags[0x40] = value;
        initialize_runtime_body_health(body, parameters, health);
        require(read_u16(body, 0x94) == expected && read_u16(body, 0x96) == expected
            && body[0xA4] == 0 && body[0xA6] == 7, "body health interpolation and ascending clamps");
    }
    put(curve, 4, std::bit_cast<u32>(100.f));
    put(curve, 8, std::bit_cast<u32>(20.f));
    tags[0x40] = 255;
    initialize_runtime_body_health(body, parameters, health);
    require(read_u16(body, 0x94) == 20, "descending health range must clamp at maximum endpoint");
    curve[1] = curve[0];
    initialize_runtime_body_health(body, parameters, health);
    require(read_u16(body, 0x94) == 20, "equal byte endpoints use maximum without division");
    release_runtime_body_health(health);
    FactoryObject first, second;
    ObjectFactoryRegistry factory{};
    std::span<const u8> resource;
    body[0x749] = body[0x74A] = 0xA5;
    put(body, 0x740, 0xFFFFFFFF);
    release_runtime_body_actions(body, first, second, resource, factory, registry);
    require(read_u32(body, 0x740) == 0 && read_u16(body, 0x748) == 0 && body[0x74A] == 0xA5,
        "body action cleanup clears mode and word");
}
}

void check_player_parent_lifecycle(gbtg::SceneExecutableResources& executable)
{
    check_body_frame_methods();
    {
        Actor source;
        Actor first_actor;
        Actor second_actor;
        first_actor.class_key = 7;
        second_actor.class_key = 9;
        std::array<u8, 0x68> first_bytes{};
        std::array<u8, 0x68> second_bytes{};
        std::array<u8, 0xF00> first_actor_bytes{};
        std::array<u8, 0xF00> second_actor_bytes{};
        std::array<u8, 8> bit_bytes{};
        EntityBitBlock bits{bit_bytes.data()};
        RuntimeScriptMindQuery body_query{};
        body_query.bits = &bits;
        RuntimeMindTarget second{nullptr, second_bytes, 0x1002, second_actor, second_actor_bytes, body_query, nullptr};
        RuntimeMindTarget first{&second, first_bytes, 0x1001, first_actor, first_actor_bytes, body_query, nullptr};
        put(first_bytes, 0x20, std::bit_cast<u32>(3.f));
        put(first_bytes, 0x28, std::bit_cast<u32>(4.f));
        put(second_bytes, 0x20, std::bit_cast<u32>(3.f));
        put(second_bytes, 0x28, std::bit_cast<u32>(4.f));
        float distance = -7.f;
        require(find_runtime_mind_target(source, nullptr, 0, 0, &distance, 1, 0, &first) == &first
            && distance == 5.f, "target scan must retain earlier equal-distance candidate and compute sqrt");
        require(find_runtime_mind_target(source, nullptr, 9, 0, nullptr, 1, 0, &first) == &second
            && find_runtime_mind_target(source, nullptr, 0, 7, nullptr, 1, 0, &first) == &second,
            "target scan must honor include and exclude class filters");
        distance = -7.f;
        require(find_runtime_mind_target(source, nullptr, 0, 0, &distance, 0, 8, &first) == nullptr
            && distance == -7.f, "category mask must remain separate from predicate flags and preserve miss output");
        first_bytes[0x14] = 33;
        require(find_runtime_mind_target(source, nullptr, 0, 0, nullptr, 2, 0, &first) == &first,
            "target category shifts must mask to five bits");
        first_bytes[0x14] = 0;
        put(first_bytes, 0x20, 0x7FC12345);
        require(find_runtime_mind_target(source, nullptr, 0, 0, nullptr, 1, 0, &first) == &first,
            "first NaN distance must retain candidate under unordered COMISS branches");
        put(first_bytes, 0x20, std::bit_cast<u32>(3.f));
        first_bytes[0x18] = 1;
        require(!runtime_mind_target_allowed(first, 0), "disabled target must fail filter");
        first_bytes[0x18] = 0;
        require(!runtime_mind_target_allowed(first, 4), "required target flag must be present");
        first_bytes[0x16] = 1;
        require(runtime_mind_target_allowed(first, 4), "required target flag must permit candidate");
        first_bytes[0x15] = 1;
        require(!runtime_mind_target_allowed(first, 0) && runtime_mind_target_allowed(first, 8),
            "predicate bit eight must permit flagged target independently of category mask");
        first_bytes[0x15] = 0;
        first_actor.mask_mode = 2;
        require(!runtime_mind_target_allowed(first, 0x10), "active actor filter must call real actor activity query");
        first_actor.mask_mode = 0;
        first_bytes[0x16] = 4;
        require(!runtime_mind_target_allowed(first, 0x200), "target flag four must be excluded when requested");
        first_bytes[0x16] = 0;
        put(first_actor_bytes, 0x98C, 1);
        require(!runtime_mind_target_allowed(first, 0x400), "actor status filter must read actual actor storage");
        put(first_actor_bytes, 0x98C, 0);
        bit_bytes[3] = 8;
        require(!runtime_mind_target_allowed(first, 0x800), "body bit filter must call real bit query");
        first_bytes[0x14] = 1;
        require(runtime_mind_target_allowed(first, 0xC10), "nonzero categories bypass actor-only predicates");
        first_bytes[0x14] = 0;
        bit_bytes[3] = 0;
        OccupancyRecord occupied;
        occupied.group = 7;
        first.occupancy = &occupied;
        source.class_key = 7;
        require(!runtime_mind_target_reachable(source, nullptr, first, 0x80)
            && runtime_mind_target_reachable(source, &occupied, first, 0x80),
            "same-group occupancy must reject unless it is actual allowed record");
        source.class_key = 9;
        require(runtime_mind_target_reachable(source, nullptr, first, 0x80), "different occupancy group permits target");
        occupied.group = 0;
        require(!runtime_mind_target_reachable(source, nullptr, first, 0x80), "zero occupancy group matches all actors");
        first.occupancy = nullptr;
        auto* saved_grid = entity_detail::g_nav_grids[0];
        entity_detail::g_nav_grids[0] = nullptr;
        require(!runtime_mind_target_reachable(source, nullptr, first, 0x20),
            "path predicate must call actual navigation and reject missing grid");
        entity_detail::g_nav_grids[0] = saved_grid;

        const i32 saved_count = entity_detail::g_nav_count;
        const i32 saved_x = entity_detail::g_nav_origin_x;
        const i32 saved_z = entity_detail::g_nav_origin_z;
        const float saved_min = entity_detail::g_nav_min_y[0];
        const float saved_max = entity_detail::g_nav_max_y[0];
        entity_detail::NavGrid grid;
        grid.width = 1;
        grid.depth = 1;
        grid.cell = 10;
        grid.cells.resize(1);
        grid.cells[0].top = 10;
        entity_detail::g_nav_grids[0] = &grid;
        entity_detail::g_nav_count = 1;
        entity_detail::g_nav_origin_x = 0;
        entity_detail::g_nav_origin_z = 0;
        entity_detail::g_nav_min_y[0] = -1.f;
        entity_detail::g_nav_max_y[0] = 10.f;
        source.position = Vec3{5.f, 0.f, 5.f};
        put(first_bytes, 0x20, std::bit_cast<u32>(25.f));
        put(first_bytes, 0x28, std::bit_cast<u32>(5.f));
        put(first_bytes, 0x38, std::bit_cast<u32>(20.f));
        require(runtime_mind_target_reachable(source, nullptr, first, 0x20),
            "path target must offset by its radius toward source using retail sine and cosine");
        put(first_bytes, 0x38, 0);
        require(!runtime_mind_target_reachable(source, nullptr, first, 0x20),
            "unoffset out-of-grid target must fail actual navigation");
        entity_detail::g_nav_grids[0] = saved_grid;
        entity_detail::g_nav_count = saved_count;
        entity_detail::g_nav_origin_x = saved_x;
        entity_detail::g_nav_origin_z = saved_z;
        entity_detail::g_nav_min_y[0] = saved_min;
        entity_detail::g_nav_max_y[0] = saved_max;
        source.position = Vec3{};
        put(first_bytes, 0x20, std::bit_cast<u32>(3.f));
        put(first_bytes, 0x28, std::bit_cast<u32>(4.f));

        std::array<u8, 0x38> strategy{};
        std::array<u8, 0xF00> actor_bytes{};
        std::array<u8, 0x950> body{};
        std::array<u8, 0x7C> mind{};
        std::array<u8, 0x20> goal_row{};
        std::array<u8, 0x20> zero{};
        LiveScript event_anchor;
        u32 notifications = 0;
        RuntimeMindEvents events{event_anchor, mind, executable.memory(0x3398C8, 5 * 12),
            [&](std::span<const u32>)
            {
                ++notifications;
            }};
        RuntimePlayerMindRequest request_anchor{};
        std::array<const char*, 49> names{};
        for (size_t i = 0; i < names.size(); ++i)
        {
            const u32 address = read_u32(executable.memory(0x3397BC + u32(i) * 4, 4), 0);
            names[i] = reinterpret_cast<const char*>(executable.memory(address, 1).data());
        }
        std::string scratch;
        u32 random = 17;
        AnimationResourceRegistry registry;
        ResourceErrorState error;
        RuntimeMindGoalContext goals{nullptr, request_anchor, executable.memory(0x217F18, 50 * 8),
            names.data(), scratch, random, registry, error};
        RuntimePlayerGoalRing ring_storage{0x20, 1, 0, 0, goal_row};
        RuntimePlayerGoalRing* ring = &ring_storage;
        RuntimeMindTarget* targets = &first;
        const OccupancyRecord* allowed = nullptr;
        auto name_at = [&](u32 address)
        {
            return reinterpret_cast<const char*>(executable.memory(address, 1).data());
        };
        RuntimePlayerStrategyEntry context{strategy, source, actor_bytes, body, events, goals, ring, zero,
            allowed, targets, name_at(0x31B554), name_at(0x31B548), name_at(0x31B540),
            name_at(0x31B538), name_at(0x31C7AC)};
        put(actor_bytes, 0x98C, 1);
        body[0x328] = 99;
        enter_runtime_player_goal(context);
        require(body[0x328] == 99 && request_anchor.next == nullptr, "actor status must block goal entry entirely");
        put(actor_bytes, 0x98C, 0);
        ring = nullptr;
        enter_runtime_player_goal(context);
        require(body[0x328] == 7 && request_anchor.next == nullptr, "null goal ring must set body state without request");
        ring = &ring_storage;
        put(goal_row, 4, 0x7FC12345);
        put(goal_row, 8, 0x80000000);
        put(goal_row, 12, std::bit_cast<u32>(2.f));
        for (u32 kind : {1u, 8u, 0x15u, 0x22u})
        {
            put(goal_row, 0, kind);
            enter_runtime_base_strategy_state(context, 0x3FB);
            auto* request = request_anchor.next;
            require(request != nullptr && read_u32(request->bytes, 0x10) == 0x7FC12345
                && read_u32(request->bytes, 0x14) == 0x80000000
                && read_f32(request->bytes, 0x28) == (kind == 0x22 ? 0.f : 30.f),
                "strategy entry must append exact position request and preserve timing per kind");
            RuntimePlayerMindRequests requests{request_anchor.next, {}};
            clear_runtime_player_mind_requests(requests);
        }
        put(goal_row, 0, 1);
        actor_bytes[0x989] = 1;
        enter_runtime_player_goal(context);
        require(read_f32(request_anchor.next->bytes, 0x28) == 0.f, "actor byte must suppress request duration");
        RuntimePlayerMindRequests requests{request_anchor.next, {}};
        clear_runtime_player_mind_requests(requests);
        actor_bytes[0x989] = 0;
        put(goal_row, 0x1C, 1);
        enter_runtime_player_goal(context);
        require(read_f32(request_anchor.next->bytes, 0x28) == 0.f, "ring flags must suppress request duration");
        clear_runtime_player_mind_requests(requests);
        put(goal_row, 0x1C, 0);
        put(goal_row, 0, 10);
        put(mind, 0x64, 0x20);
        put(mind, 0x68, 1);
        first_bytes[0x15] = 1;
        enter_runtime_player_goal(context);
        require(read_u32(mind, 0x10) == first.address && event_anchor.next != nullptr
            && read_u32(static_cast<RuntimeMindEvent*>(event_anchor.next)->bytes, 0x20) == first.address,
            "kind ten must clear navigation flag, add predicate eight and schedule chosen target event");
        clear_runtime_player_mind_requests(requests);
        clear_runtime_mind_events(event_anchor.next);
        std::array<u8, 5 * 12> duplicate_descriptors{};
        std::memcpy(duplicate_descriptors.data(), events.descriptors.data(), duplicate_descriptors.size());
        duplicate_descriptors[3 * 12 + 4] &= 0xFE;
        events.descriptors = duplicate_descriptors;
        allocate_runtime_mind_event(events, 3, 1.f);
        auto* second_event = allocate_runtime_mind_event(events, 3, 2.f);
        allocate_runtime_mind_event(events, 4, 3.f);
        strategy[0x1C] = 255;
        enter_runtime_base_strategy_state(context, 0x3FC);
        require(read_u32(mind, 0x10) == 0 && strategy[0x1C] == 0 && notifications == 0
            && event_anchor.next->next != second_event && event_anchor.next->next != nullptr,
            "state 3FC must directly clear target, remove only first kind three and wrap state byte");
        clear_runtime_mind_events(event_anchor.next);
        enter_runtime_base_strategy_state(context, 0x3F4);
        require(body[0x328] == 8, "state 3F4 must set body state eight");
        enter_runtime_base_strategy_state(context, 0x3FF);
        require(body[0x328] == 7 && strategy[0x1C] == 1, "state 3FF must set body state seven and increment");
        enter_runtime_base_strategy_state(context, 0x415);
        enter_runtime_base_strategy_state(context, 0xFFFF);
        require(strategy[0x1C] == 2, "state 415 increments and unknown state preserves fields");
        ring_storage.tail = ring_storage.capacity;
        put(zero, 0, 0x22);
        put(zero, 4, 0x3F800000);
        enter_runtime_player_goal(context);
        require(read_f32(request_anchor.next->bytes, 0x10) == 1.f,
            "empty nonnull goal ring must read kind and position from real zero-address view");
        clear_runtime_player_mind_requests(requests);
    }
    {
        std::array<u8, 0x7C> mind{};
        LiveScript anchor;
        std::vector<u32> messages;
        RuntimeMindEvents events{anchor, mind, executable.memory(0x3398C8, 5 * 12),
            [&](std::span<const u32> message)
            {
                require(message.size() == 2 && message[0] == 0x64 && message[1] == 0
                    && read_u32(mind, 0x10) == 0x1234,
                    "target clear must notify actual actor before overwriting target");
                messages.push_back(message[0]);
            }};
        put(mind, 0x2C, 0x3F000000);
        auto* first = allocate_runtime_mind_event(events, 3, 7.f);
        auto* second = allocate_runtime_mind_event(events, 4, 20.f);
        require(first != nullptr && second != nullptr && anchor.next == second
            && second->next == first && first->prev == second
            && read_u32(second->bytes, 0x14) == 0x3F000000,
            "mind events must use executable sizes and prepend with correct backlinks");
        put(second->bytes, 0xC, 0);
        put(second->bytes, 0x1C, 0xDEADBEEF);
        second->bytes[0xA] = 1;
        put(mind, 0x2C, 0x40000000);
        refresh_runtime_mind_event(events, *first);
        require(anchor.next == first && first->next == second && second->prev == first,
            "refresh must unlink and rehome existing event at root");
        refresh_runtime_mind_event(events, *second);
        require(anchor.next == second && read_f32(second->bytes, 0xC) == 20.f
            && read_u32(second->bytes, 0x14) == 0x40000000
            && read_u32(second->bytes, 0x1C) == 0 && second->bytes[0xA] == 0,
            "refresh must restore duration, clock and clear active payload");
        set_runtime_mind_target(events, 0x1234);
        auto* target = static_cast<RuntimeMindEvent*>(anchor.next);
        require(read_u32(target->bytes, 0x20) == 0x1234,
            "target setter must schedule kind four with actual target token");
        set_runtime_mind_target(events, 0x1234);
        require(anchor.next == target, "same target must refresh its existing event");
        set_runtime_mind_target(events, 0);
        set_runtime_mind_target(events, 0);
        require(messages.size() == 1 && read_u32(mind, 0x10) == 0,
            "only nonzero to zero transition must send target-clear notification");
        clear_runtime_mind_events(anchor.next);
        require(anchor.next == nullptr, "base mind cleanup must release new event storage");

        std::array<u8, 2 * 12> descriptors{};
        put(descriptors, 12, 0x24);
        descriptors[16] = 1;
        RuntimeMindEvents unique{anchor, mind, descriptors, {}};
        auto* reused = allocate_runtime_mind_event(unique, 1, 3.f);
        put(reused->bytes, 0xC, 0);
        require(allocate_runtime_mind_event(unique, 1, 99.f) == reused
            && read_f32(reused->bytes, 0xC) == 3.f && reused->next == nullptr,
            "unique descriptor must ignore replacement duration and refresh existing event");
        clear_runtime_mind_events(anchor.next);

        RuntimePlayerMindRequest request_anchor{};
        std::array<u8, 12> position{};
        put(position, 0, 0x7FC12345);
        put(position, 4, 0x80000000);
        put(position, 8, 0x40400000);
        auto* request = append_runtime_position_request(request_anchor,
            executable.memory(0x217F18, 50 * 8), position);
        require(request->kind == 0x2E && read_u32(request->bytes, 0x10) == 0x7FC12345
            && read_u32(request->bytes, 0x14) == 0x80000000
            && read_f32(request->bytes, 0x1C) == 1.f && read_f32(request->bytes, 0x20) == 1.f
            && read_f32(request->bytes, 0x28) == 30.f,
            "position request must preserve raw coordinates and retail timing constants");
        RuntimePlayerMindRequests requests{request_anchor.next, {}};
        clear_runtime_player_mind_requests(requests);
        std::array<u8, 0x40> rows{};
        std::array<u8, 0x20> zero{};
        put(rows, 0x3C, 0x12345678);
        put(zero, 0x1C, 0x87654321);
        RuntimePlayerGoalRing ring{0x20, 2, 1, 0, rows};
        require(runtime_player_goal_flags(nullptr, zero) == 0
            && runtime_player_goal_flags(&ring, zero) == 0x12345678,
            "goal flags must select current ring head");
        ring.tail = ring.capacity;
        require(runtime_player_goal_flags(&ring, zero) == 0x87654321,
            "empty ring must retain retail zero-address read");
        float angle = 5.f;
        require(!runtime_direction_angle(Vec3{0.f, 9.f, 0.f}, angle) && angle == 0.f
            && !runtime_direction_angle(Vec3{std::bit_cast<float>(0x7FC00000u), 0.f, 0.f}, angle),
            "direction angle must preserve zero and unordered JA branches");
        require(runtime_direction_angle(Vec3{1.f, 0.f, 0.f}, angle) && angle > 1.5f && angle < 1.6f,
            "direction angle must call retail atan2 with x and z");
    }
    {
        std::array<u8, 0xF00> child_bytes{};
        std::array<u8, 0xF00> host_bytes{};
        put(child_bytes, 0x944, 2);
        put(child_bytes, 0x948, 0xFFFE0007);
        put(child_bytes, 0x958, 0x00050009);
        put(host_bytes, 6, 9);
        RuntimeBodyAttachedActor host{host_bytes, 0x1234, {}};
        RuntimeBodyAttachedActor child{child_bytes, 0x5678,
            [&](std::span<const u32> message)
            {
                require(message[0] == 0x91 && message[1] == host.address
                    && read_u16(child_bytes, 0x946) == 1
                    && read_u32(child_bytes, 0xD8C) == host.address,
                    "host assignment must commit index and token before virtual notification");
            }};
        require(runtime_attachment_class_value(child_bytes, 7) == -2
            && runtime_attachment_class_value(child_bytes, 9) == 5
            && runtime_attachment_class_value(child_bytes, 10) == -1,
            "attachment class lookup must sign extend row values and preserve misses");
        set_runtime_attachment_host(child, host);
        child_bytes[0x93F] = 1;
        require(!runtime_attachment_available(child_bytes, &host), "mode one forbids attachment");
        child_bytes[0x93F] = 2;
        require(!runtime_attachment_available(child_bytes, nullptr)
            && !runtime_attachment_available(child_bytes, &host), "mode two requires eligible host");
        put(host_bytes, 0xCDC, 2);
        require(runtime_attachment_available(child_bytes, &host), "host mode two permits attachment");
        child_bytes[0x941] = 1;
        require(!runtime_attachment_available(child_bytes, &host), "busy attachment must fail final gate");
        host.message = [&](std::span<const u32> message)
        {
            require(message[0] == 0x5E && message[1] == child.address, "host notification payload");
            put(host_bytes, 0xCDC, 0);
        };
        notify_runtime_attachment_host(host, child);
        require(read_u32(child_bytes, 0x9A4) == 1, "attachment must reread host mode after virtual message");
        put(child_bytes, 0x9A4, 0);
        put(host_bytes, 0x93C, 1);
        notify_runtime_attachment_host(host, child);
        require(read_u32(child_bytes, 0x9A4) == 0, "host child pointer must suppress activation");
        Actor actor;
        Actor parent;
        ActorModel model;
        std::array<u8, 64> model_bytes{};
        put(model_bytes, 0, 8);
        put(model_bytes, 4, 1);
        put(model_bytes, 8, 9);
        put(model_bytes, 12, 32);
        model_bytes[34] = 7;
        model.cpu = model_bytes;
        parent.model = &model;
        bind_runtime_actor_joint(actor, &parent, -1, 3);
        require(actor.bind == nullptr, "invalid joint key must leave bind unallocated");
        bind_runtime_actor_joint(actor, &parent, 2, 3);
        require(actor.bind != nullptr && actor.bind->mode_parent == 2 && actor.bind->parent == &parent
            && actor.bind->joint == 7 && actor.bind->flags == 3
            && actor.bind->basis[0] == 1.f && actor.bind->basis[5] == 1.f
            && actor.bind->basis[10] == 1.f && actor.bind->m[10] == 1.f,
            "joint binding must use model lookup and initialize both identity matrices");
        ActorBind* retained = actor.bind;
        actor.bind->m[0] = 9.f;
        require(ensure_runtime_actor_bind(actor) == retained && actor.bind->m[0] == 9.f,
            "existing bind must preserve retained transform");
        engine_heap_free(actor.bind);
        actor.bind = nullptr;
    }
    using namespace gbtg;
    check_body_dependencies(executable);
    {
        std::array<u8, 0x950> body;
        body.fill(0xA5);
        initialize_runtime_body_motion(body);
        require(read_u32(body, 0x324) == read_u32(executable.memory(0x32B224, 4), 0)
            && read_f32(body, 0x334) == 10.f && body[0x328] == 2
            && body[0x329] == 0xA5 && body[0x33F] == 0xA5 && body[0x704] == 0xA5,
            "body motion constants and untouched gaps");
        for (size_t i = 0x344; i < 0x704; ++i)
        {
            require(body[i] == 0, "body motion must clear all 80 vectors");
        }
        std::array<u8, 0x820> parameters{};
        require(count_runtime_player_body_joints(nullptr, parameters) == 0, "null model joint count");
        std::array<u8, 64> shapes{};
        shapes[8] = 0; shapes[9] = 8; shapes[12] = 0x14;
        shapes[16] = 1; shapes[17] = 8; shapes[20] = 4;
        shapes[24] = 0; shapes[25] = 0; shapes[28] = 0x14;
        const std::array<RuntimeBodyModelNode, 2> nodes{{{3, nullptr}, {4, shapes.data()}}};
        RuntimeBodyModelNodes model{nodes};
        RuntimeBodyShapeStorage storage{};
        collect_runtime_player_body_shapes(&model, storage);
        collect_runtime_body_special_shapes(body, &model, storage);
        require(storage.selected_count == 3 && storage.selected[2] == shapes.data() + 24
            && body[0x714] == 2 && reinterpret_cast<const u8*>(storage.special[1].node) == shapes.data() + 24
            && storage.special[0].contact[0x26] == 0, "shape terminal must be tested before zero stride");
        engine_heap_free(storage.selected);
        engine_heap_free(storage.selected_state);
        engine_heap_free(storage.special);
        std::array<u8, 64> many_shapes{};
        for (size_t i = 0; i < 6; ++i)
        {
            many_shapes[8 + i * 8] = 1;
            many_shapes[9 + i * 8] = i == 5 ? 0 : 8;
            many_shapes[12 + i * 8] = 4;
        }
        const RuntimeBodyModelNode many_node{4, many_shapes.data()};
        RuntimeBodyModelNodes many_model{{&many_node, 1}};
        storage = {};
        collect_runtime_player_body_shapes(&many_model, storage);
        require(storage.selected_count == 6 && storage.selected[5] == many_shapes.data() + 48,
            "shape pointers must survive growth from four to eight slots");
        auto* retained = storage.selected_state;
        many_shapes.fill(0);
        collect_runtime_player_body_shapes(&many_model, storage);
        require(storage.selected_count == 0 && storage.selected_state == retained,
            "zero matching shapes must preserve prior state allocation");
        engine_heap_free(storage.selected);
        engine_heap_free(storage.selected_state);
        FactoryObject action_object;
        action_object.bytes = std::make_unique<u8[]>(4);
        ObjectFactoryRegistry factory{};
        RuntimePlayerChildContext child{factory, executable.pools, 0x1234};
        u32 action_messages = 0;
        RuntimeBodyAction action{action_object, child, [&](FactoryObject&, u32 message)
        {
            require(message == 3, "existing action receives reset message");
            ++action_messages;
        }};
        parameters[0x1C0] = 7;
        parameters[0x1C1] = 1;
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x1C2), "first_action");
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x242), "second_action");
        put(parameters, 0x2C4, 0x7F800001);
        put(body, 0x754, 0);
        initialize_runtime_body_action(body, parameters, action);
        require(read_u32(body, 0x750) == 7 && read_u32(body, 0x754) == 1
            && read_u32(body, 0x758) == 0 && read_u32(body, 0x7EC) == 0x7F800001
            && action_messages == 1, "body action initialization preserves raw bits and writes a full kind word");
        activate_runtime_body_action(body, 0xFF, 2, action);
        require(action_messages == 2 && read_u32(body, 0x754) == 1 && read_u32(body, 0x758) == 0,
            "active action receives message but preserves existing mode and value");
        std::array<u8, 0xF00> actor0{}, actor1{};
        const u16 class_two = 2;
        std::memcpy(actor1.data() + 0xCDC, &class_two, 2);
        std::vector<u32> messages;
        RuntimePauseActor second{nullptr, actor1, [&](u32 message)
        {
            messages.push_back(message | 0x100);
        }};
        RuntimePauseActor first{&second, actor0, [&](u32 message)
        {
            messages.push_back(message);
            first.next = nullptr;
        }};
        RuntimePauseActor* head = &first;
        std::array<u32, 2> references{};
        RuntimeActorPauseGroups pause{references, head};
        change_runtime_actor_pause(3, 0, pause);
        require(messages == std::vector<u32>{0x27}, "pause must save next before callback and reread head for next group");
        first.next = &second;
        change_runtime_actor_pause(3, 0, pause);
        require(messages.size() == 1, "nested pause must not rebroadcast");
        change_runtime_actor_pause(3, 1, pause);
        require(messages.size() == 1, "nested release must not rebroadcast");
        change_runtime_actor_pause(1, 1, pause);
        require(messages.back() == 0x26 && references[0] == 0, "final pause release message");
        std::array<u8, 0x54> strategy{};
        u32 active = 0;
        leave_runtime_player_strategy(strategy, 0x3F1, 0, body, active, pause);
        require(active == 0xFFFFFFFF && strategy[0x1C] == 255 && read_u16(strategy, 0x12) == 0x408,
            "player leaving state counter wrap");
        leave_runtime_player_strategy(strategy, 0x3FB, 0x3FB, body, active, pause);
        require(read_u32(strategy, 0x38) == 0, "same state must preserve body counter");
        leave_runtime_player_strategy(strategy, 0x3FB, 0, body, active, pause);
        require(read_u32(strategy, 0x38) == 1 && read_u16(body, 0x30) == 0xA5A6, "body counter update");
        const std::array<RuntimeMindGoalRow, 4> rows{{{"go1", {}}, {"go2", {}}, {"go4", {}}, {"x[", {}}}};
        RuntimeMindGoalTable table{4, rows};
        require(find_runtime_mind_goal(table, "GO2") == &rows[1]
            && find_runtime_mind_goal(table, "x{") == &rows[3], "mind goal folds punctuation and case");
        std::string scratch;
        u32 random = 7, expected = random;
        const char* name = "go?ignored";
        const auto* chosen = choose_runtime_mind_goal(table, name, name + 2, scratch, random);
        const size_t choice = size_t(fraction(expected) * 2.f);
        require(chosen == &rows[choice] && random == expected, "wildcard contiguous range and one random draw");
        table.count = -1;
        require(find_runtime_mind_goal(table, "go1") == nullptr, "negative signed goal count");
        const u32 saved = random;
        require(choose_runtime_mind_goal(table, name, name + 2, scratch, random) == nullptr && random == saved,
            "wildcard miss must preserve random state");
        RuntimePlayerMindRequest anchor{};
        const auto sizes = executable.memory(0x217F18, 50 * 8);
        auto* request = allocate_runtime_mind_request(anchor, 1, sizes);
        auto* next = allocate_runtime_mind_request(anchor, 2, sizes);
        require(anchor.next == request && request->next == next && next->previous == request
            && read_u16(request->bytes, 8) == 1 && request->active == 0, "request allocation and tail links");
        RuntimePlayerMindRequests requests{anchor.next, {}};
        clear_runtime_player_mind_requests(requests);
        require(anchor.next == nullptr, "allocated request cleanup must clear actual anchor");
    }
    ObjectSizes sizes{};
    base_mind_object_sizes(sizes);
    require(sizes.instance == 0x7C && sizes.parameters == 0xA0, "base mind sizes");
    base_strategy_object_sizes(sizes);
    require(sizes.instance == 0x38 && sizes.parameters == 0x18, "base strategy sizes");
    auto retail_mind = clone_executable_resource(executable.pools, "aid_objparams_ghoulies_mind_player");
    std::vector<u8> parameters(retail_mind.bytes.get(), retail_mind.bytes.get() + retail_mind.size);
    put(parameters, 0x9C, 0x12345678);
    put(parameters, 0x88, std::bit_cast<u32>(2.f));
    put(parameters, 0x8C, std::bit_cast<u32>(5.f));
    put(parameters, 0x90, 0x1234);
    put(parameters, 0x94, 0xABCD);
    put(parameters, 0x98, std::bit_cast<u32>(9.f));
    const std::array<u16, 6> settings_rows{11, 1, 42, 65535, 42, 3};
    RuntimeMindClassSettings settings{3, {reinterpret_cast<const u8*>(settings_rows.data()), sizeof(settings_rows)}};
    std::array<u8, 0x7C> mind;
    for (i32 count : {3, 0, -1})
    {
        settings.count = count;
        mind.fill(0xA6);
        initialize_runtime_mind_settings(mind, parameters, 42, settings);
        require(read_u32(mind, 0x64) == 0x1234 && read_u32(mind, 0x68) == 0xABCD
            && read_f32(mind, 0x6C) == (count > 0 ? 65535.f : 9.f)
            && mind[0x70] == 0xA6, "mind class override signed count or unsigned value");
    }
    settings.count = 3;
    initialize_runtime_mind_settings(mind, parameters, 99, settings);
    require(read_f32(mind, 0x6C) == 9.f, "mind class miss preserves parameter setting");
    std::span<const u8> script_table;
    u32 random = 123;
    u32 expected = random;
    const float sampled = 2.f + 3.f * fraction(expected);
    initialize_runtime_base_mind(mind, parameters, 42, settings, random, executable.pools, script_table);
    const auto* record = find_executable_resource(executable.pools,
        reinterpret_cast<const char*>(parameters.data() + 8), true);
    require(random == expected && read_f32(mind, 0x2C) == sampled
        && read_u32(mind, 8) == 0x12345678 && read_u32(mind, 0x3C) == 0
        && read_u32(mind, 0x4C) == 0x40 && script_table.data() == (record == nullptr ? nullptr : record->cpu.data()),
        "base mind retail table lookup or random initialization");
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 8), "aid_objparams_ghoulies_mind_player");
    initialize_runtime_base_mind(mind, parameters, 42, settings, random, executable.pools, script_table);
    record = find_executable_resource(executable.pools, "aid_objparams_ghoulies_mind_player", true);
    require(record != nullptr && script_table.data() == record->cpu.data(), "base mind borrowed resource lookup");
    parameters[8] = 'x';
    const auto retained_table = script_table;
    initialize_runtime_base_mind(mind, parameters, 42, settings, random, executable.pools, script_table);
    require(script_table.data() == retained_table.data(), "non-AID mind table must remain untouched");
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 8), "aid_missing_parent_mind_table");
    initialize_runtime_base_mind(mind, parameters, 42, settings, random, executable.pools, script_table);
    require(script_table.data() == nullptr, "missing mind table must clear native view");
    std::array<u8, 0x1C> request;
    request.fill(0xA6);
    const auto original_request = request;
    const u32 saved_random = random;
    require(initialize_runtime_mind_request(request, random) && request == original_request
        && random == saved_random, "unhandled mind request must preserve bytes and random");
    put(request, 8, 0x29);
    put(request, 0x10, std::bit_cast<u32>(5.f));
    put(request, 0x14, std::bit_cast<u32>(-2.f));
    expected = random;
    const float expected_request = 5.f - 7.f * fraction(expected);
    require(initialize_runtime_mind_request(request, random) && random == expected
        && read_f32(request, 0x18) == expected_request, "mind request reversed random range");

    std::array<u8, 0x24> range_rows{};
    for (size_t row = 0; row < 3; ++row)
    {
        put(range_rows, row * 0xC, row == 0 ? 11 : 42);
        put(range_rows, row * 0xC + 4, std::bit_cast<u32>(row == 2 ? 100.f : 2.f));
        put(range_rows, row * 0xC + 8, std::bit_cast<u32>(row == 2 ? 100.f : 5.f));
    }
    i32 range_count = 3;
    RuntimeStrategyClassRanges ranges{range_count, range_rows};
    std::array<u8, 4> result{0x01, 0x00, 0x80, 0x7F}; // Signaling NaN must stay byte-identical on miss.
    const auto original_result = result;
    for (i32 count : {3, 0, -1})
    {
        range_count = count;
        const u32 before = random;
        require(!sample_runtime_strategy_class_range(result, 99, ranges, random)
            && result == original_result && random == before, "strategy range miss must preserve output and random");
    }
    range_count = 3;
    expected = random;
    const float expected_range = 2.f + 3.f * fraction(expected);
    require(sample_runtime_strategy_class_range(result, 42, ranges, random)
        && random == expected && read_f32(result, 0) == expected_range, "strategy range first duplicate match");
    auto retail_strategy = clone_executable_resource(executable.pools, "aid_objparams_ghoulies_strategy_player");
    std::span<u8> strategy_parameters{retail_strategy.bytes.get(), retail_strategy.size};
    put(strategy_parameters, 8, 0x12345678);
    std::array<u8, 0x38> strategy;
    strategy.fill(0xA6);
    const auto original_strategy = strategy;
    require(initialize_runtime_base_strategy(strategy, strategy_parameters, 99, ranges, random)
        && read_u32(strategy, 8) == 0x12345678 && read_u16(strategy, 0xC) == read_u16(strategy_parameters, 0xC)
        && read_u16(strategy, 0xE) == 0xA6A6 && read_u32(strategy, 0x34) == read_u32(original_strategy, 0x34),
        "base strategy preserves current state and missing range");
    for (size_t index = 0; index < 5; ++index)
    {
        require(read_u16(strategy, 0x10 + index * 2) == read_u16(strategy_parameters, 0xE + index * 2),
            "base strategy state parameter offsets");
    }
    for (u16 state : {u16{0x3FC}, u16{0x3FF}, u16{0x415}, u16{1000}})
    {
        strategy[0x1C] = 0;
        leave_runtime_base_strategy_state(strategy, state);
        require(strategy[0x1C] == (state == 1000 ? 0 : 255), "strategy leaving byte wrap or gate");
    }
    RuntimeStrategyLink anchor{}, first{}, second{};
    anchor.owner = &strategy;
    anchor.next = &first;
    first = {&anchor, &second, nullptr, 0xABCD, 1};
    second = {&first, nullptr, nullptr, 0xBCDE, 1};
    detach_runtime_strategy_link(first);
    require(anchor.next == &second && second.previous == &anchor && anchor.owner == &strategy
        && first.previous == nullptr && first.next == nullptr && first.active == 0 && first.retained == 0xABCD,
        "strategy middle detach lost anchor or retained field");
    detach_runtime_strategy_link(second);
    require(anchor.next == nullptr && anchor.owner == nullptr && second.active == 0,
        "strategy final detach did not clear anchor owner");
    first.owner = &strategy;
    detach_runtime_strategy_link(first);
    require(first.owner == &strategy, "unlinked strategy must preserve its own owner");

    LiveScript event_anchor;
    auto* event = new (engine_heap_allocate(sizeof(LiveScript))) LiveScript;
    event->prev = &event_anchor;
    event_anchor.next = event;
    RuntimePlayerMindRequest request_anchor{};
    auto* pending = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
    *pending = {&request_anchor, nullptr, 1, 1};
    request_anchor.next = pending;
    int released = 0;
    RuntimePlayerMindRequests requests{request_anchor.next, [&](RuntimePlayerMindRequest& node)
    {
        require(node.previous == nullptr && node.next == nullptr && event_anchor.next != nullptr,
            "base mind must clear detached requests before events");
        ++released;
    }};
    auto* storage = static_cast<RuntimeMindStorage*>(engine_heap_allocate(sizeof(RuntimeMindStorage)));
    storage->scratch = engine_heap_allocate(16);
    script_table = retained_table;
    destroy_runtime_base_mind(requests, event_anchor.next, storage, script_table);
    require(released == 1 && request_anchor.next == nullptr && event_anchor.next == nullptr
        && storage == nullptr && script_table.data() == nullptr, "base mind destructor ownership cleanup");
    destroy_runtime_base_mind(requests, event_anchor.next, storage, script_table);
    ObjectFactoryRegistry unused_factory{};
    RuntimePlayerChildContext child{unused_factory, executable.pools, 0x12345678};
    RuntimePlayerMindRequests* request_view = &requests;
    FactoryObject object;
    object.bytes = std::make_unique<u8[]>(0x38);
    object.size = 0x38;
    const u16 previous = 7;
    std::memcpy(object.bytes.get() + 0xE, &previous, 2);
    std::vector<int> order;
    RuntimePlayerStrategySetup callbacks{child, request_view,
        [&](FactoryObject& item, u16 old, u16 next)
        {
            require(old == 7 && next == 1000 && first.active == 0, "base strategy detach before leaving");
            order.push_back(1);
            const u16 modified = 9;
            std::memcpy(item.bytes.get() + 0xE, &modified, 2);
        },
        [&](FactoryObject& item, u16 next, u16 old)
        {
            require(next == 1000 && old == 9 && read_u16({item.bytes.get(), item.size}, 0xE) == 1000,
                "base strategy entering must observe callback-modified previous state");
            order.push_back(2);
        }};
    destroy_runtime_base_strategy(object, first, callbacks);
    require(order == std::vector<int>{1, 2}, "base strategy destructor dispatch order");

    const char* tags[]{"objTag_Null", "objTag_Actor_Boy", "objTag_Actor_Imp"};
    std::array<u8, 0x87 * 3> tag_rows{};
    std::strcpy(reinterpret_cast<char*>(tag_rows.data()), tags[2]);
    std::strcpy(reinterpret_cast<char*>(tag_rows.data() + 0x87), tags[1]);
    std::strcpy(reinterpret_cast<char*>(tag_rows.data() + 0x10E), tags[0]);
    require(find_runtime_player_tag_index(1, tag_rows.data(), tags) == 2
        && find_runtime_player_tag_index(2, tag_rows.data(), tags) == 1
        && find_runtime_player_tag_index(0, tag_rows.data(), tags) == 0, "player tag sentinel and stride");
    std::array<std::string, 0xE9> tag_names;
    std::array<const char*, 0xE9> tag_views;
    for (size_t index = 0; index < tag_names.size(); ++index)
    {
        tag_names[index] = "tag_" + std::to_string(1000 + index);
        tag_views[index] = tag_names[index].c_str();
    }
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    error.suppressed = 1;
    std::array<u8, 0xB8> attributes{};
    const size_t chances[]{0x14, 0x10, 0x20, 0x24, 0x30, 0x34};
    const size_t fields[]{0x96E, 0x96D, 0x96F, 0x970, 0x96C, 0x969};
    for (size_t offset : chances)
    {
        put(attributes, offset, std::bit_cast<u32>(1.f));
    }
    put(attributes, 0x1C, 0x7FC12345);
    std::strcpy(reinterpret_cast<char*>(attributes.data() + 0x38), tag_views[3]);
    std::strcpy(reinterpret_cast<char*>(attributes.data() + 0x78), "missing");
    std::span<const u8> attribute_view = attributes;
    std::array<u8, 0xF00> actor;
    actor.fill(0xA6);
    std::array<u8, 0x63C> player_parameters{};
    RuntimePlayerAttributes attribute_context{attribute_view, registry, error, random, tag_views.data(), attributes};
    const auto before_actor = actor;
    const u32 before_random = random;
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    require(actor == before_actor && random == before_random && attribute_view.data() == attributes.data(),
        "nonplayer attributes must preserve fields and resource");
    const u16 mode = 1;
    std::memcpy(actor.data() + 0xCDC, &mode, 2);
    expected = random;
    for (int index = 0; index < 6; ++index)
    {
        fraction(expected);
    }
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    require(random == expected && read_u32(actor, 0x974) == 0x7FC12345
        && read_u16(actor, 0x97C) == 4 && read_u16(actor, 0x97E) == 0 && actor[0x971] == 0xA6,
        "player attributes random count, raw timer or tag lookup");
    for (size_t field : fields)
    {
        require(actor[field] == 1, "player attributes chance one");
    }
    for (size_t offset : chances)
    {
        put(attributes, offset, 0x7FC12345);
    }
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    for (size_t field : fields)
    {
        require(actor[field] == 0, "player attributes unordered chance rejection");
    }
    expected = random;
    const float equality = fraction(expected);
    put(attributes, 0x14, std::bit_cast<u32>(equality));
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    require(actor[0x96E] == 1, "player attribute chance equality must be accepted");
    AnimationBundleRecord attribute_record;
    attribute_record.name = "aid_test_player_attributes";
    attribute_record.cpu = attributes;
    std::array<AnimationBundlePool, 1> pools{AnimationBundlePool{{&attribute_record, 1}, true}};
    register_animation_resource_provider(registry,
        [&](u32, const AnimationResourceRecord& resource, const void*& resolved)
        {
            return find_bundle_resource_provider(pools, resource.name.c_str(), resolved);
        },
        [&](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            return load_bundle_resource_provider(resolved, loaded);
        },
        [&](u32, const char* name, const AnimationLoadedResource&)
        {
            unload_bundle_resource_provider(pools, name);
        }, 3, nullptr);
    attribute_view = {};
    std::strcpy(reinterpret_cast<char*>(player_parameters.data() + 0x5BA), attribute_record.name.c_str());
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    require(attribute_view.data() == attributes.data() && attribute_record.references == 1,
        "player attributes must acquire and retain registry resource");
    release_animation_resource(registry, attribute_view);
    require(find_animation_resource(registry.records, attribute_record.name.c_str())->references == 0,
        "player attribute fixture cleanup must release its retained registry reference");
    std::strcpy(reinterpret_cast<char*>(player_parameters.data() + 0x5BA), "aid_missing_player_attributes");
    initialize_runtime_player_attributes(actor, player_parameters, attribute_context);
    require(attribute_view.data() == nullptr && read_u32(actor, 0x974) == 0
        && read_u16(actor, 0x97C) == 4, "missing attribute resource must retain actual zero-address name reads");
    {
        std::array<const char*, 49> names{};
        for (size_t i = 0; i < names.size(); ++i)
        {
            const u32 address = read_u32(executable.memory(0x3397BC + u32(i) * 4, 4), 0);
            names[i] = reinterpret_cast<const char*>(executable.memory(address, 1).data());
        }
        const auto sizes = executable.memory(0x217F18, 50 * 8);
        const u32 payload = read_u32(sizes, 8) - read_u32(sizes, 12) - 0x10;
        std::vector<u8> resource(0x44 + payload + 0x44);
        const u16 size = u16(0x44 + payload);
        std::memcpy(resource.data(), &size, 2);
        std::strcpy(reinterpret_cast<char*>(resource.data() + 4), names[0]);
        std::memset(resource.data() + 0x44, 0x5A, payload);
        std::strcpy(reinterpret_cast<char*>(resource.data() + size + 4), "unrecognized_goal_end");
        AnimationBundleRecord record;
        record.name = "aid_test_mind_goal";
        record.cpu = resource;
        std::array<AnimationBundlePool, 1> goal_pools{AnimationBundlePool{{&record, 1}, true}};
        AnimationResourceRegistry goal_registry;
        register_animation_resource_provider(goal_registry,
            [&](u32, const AnimationResourceRecord& entry, const void*& resolved)
            {
                return find_bundle_resource_provider(goal_pools, entry.name.c_str(), resolved);
            },
            [&](u32, const void* resolved, AnimationLoadedResource& loaded)
            {
                return load_bundle_resource_provider(resolved, loaded);
            },
            [&](u32, const char* name, const AnimationLoadedResource&)
            {
                unload_bundle_resource_provider(goal_pools, name);
            }, 3, nullptr);
        std::array<u8, 0x84> row_bytes{};
        std::strcpy(reinterpret_cast<char*>(row_bytes.data() + 4), record.name.c_str());
        const RuntimeMindGoalRow row{"test", row_bytes};
        const RuntimeMindGoalTable table{1, {&row, 1}};
        RuntimePlayerMindRequest anchor{};
        std::string scratch;
        u32 state = 7;
        RuntimeMindGoalContext goals{&table, anchor, sizes, names.data(), scratch, state, goal_registry, error};
        require(load_runtime_mind_goal("test", goals) && anchor.next != nullptr
            && anchor.next->kind == 1 && anchor.next->bytes[0x10] == 0x5A && state == 7,
            "goal loader must copy exact payload using executable sizes and names");
        require(find_animation_resource(goal_registry.records, record.name.c_str())->references == 0,
            "goal loader must release acquired registry reference");
        RuntimePlayerMindRequests requests{anchor.next, {}};
        clear_runtime_player_mind_requests(requests);
        const u16 empty_size = 0x44;
        std::memcpy(resource.data(), &empty_size, 2);
        std::strcpy(reinterpret_cast<char*>(resource.data() + 0x48), "unrecognized_goal_end");
        require(load_runtime_mind_goal("test", goals) && anchor.next == nullptr,
            "empty recognized node must still release resource and return success");

        std::memcpy(resource.data(), &size, 2);
        auto name_at = [&](u32 address)
        {
            return reinterpret_cast<const char*>(executable.memory(address, 1).data());
        };
        RuntimeMindGoalRow strategy_row{name_at(0x31B554), row_bytes};
        const RuntimeMindGoalTable strategy_table{1, {&strategy_row, 1}};
        goals.table = &strategy_table;
        Actor actor;
        std::array<u8, 0x38> strategy_bytes{};
        std::array<u8, 0xF00> actor_bytes{};
        std::array<u8, 0x950> body_bytes{};
        std::array<u8, 0x7C> mind_bytes{};
        std::array<u8, 0x20> ring_bytes{};
        std::array<u8, 0x20> zero{};
        LiveScript event_anchor;
        RuntimeMindEvents events{event_anchor, mind_bytes, executable.memory(0x3398C8, 5 * 12), {}};
        RuntimePlayerGoalRing ring_storage{0x20, 1, 0, 0, ring_bytes};
        RuntimePlayerGoalRing* ring = &ring_storage;
        RuntimeMindTarget* targets = nullptr;
        const OccupancyRecord* allowed = nullptr;
        RuntimePlayerStrategyEntry entry{strategy_bytes, actor, actor_bytes, body_bytes, events, goals,
            ring, zero, allowed, targets, name_at(0x31B554), name_at(0x31B548), name_at(0x31B540),
            name_at(0x31B538), name_at(0x31C7AC)};
        for (const auto selection : {std::pair<u16, const char*>{0x3E9, entry.goal_3e9},
            {0x3F2, entry.goal_default}, {0x3F3, entry.goal_default}, {0x3F4, entry.goal_3f4},
            {0x418, entry.goal_418}})
        {
            strategy_row.name = selection.second;
            enter_runtime_base_strategy_state(entry, selection.first);
            require(anchor.next != nullptr && anchor.next->kind == 1 && anchor.next->next == nullptr,
                "base strategy states must load their exact executable goal name through real resource loader");
            clear_runtime_player_mind_requests(requests);
        }
        put(ring_bytes, 0, 10);
        for (const char* selected_name : {entry.goal_default, entry.goal_target})
        {
            strategy_row.name = selected_name;
            enter_runtime_player_goal(entry);
            require(anchor.next != nullptr && anchor.next->kind == 0x2E
                && anchor.next->next != nullptr && anchor.next->next->kind == 1
                && anchor.next->next->next == nullptr,
                "target goal must use real requested goal or fallback and retain preceding position request");
            clear_runtime_player_mind_requests(requests);
        }
        require(find_animation_resource(goal_registry.records, record.name.c_str())->references == 0,
            "strategy goal loading must release actual registry references");
        goals.table = nullptr;
        require(!load_runtime_mind_goal("test", goals), "null mind table must return failure");
    }
}
