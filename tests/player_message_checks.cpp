#include "player_message_checks.h"
#include "../src/engine/entities/player_messages.h"
#include "../src/engine/entities/player_overlay.h"
#include "../src/engine/entities/entity_heap.h"
#include "../src/engine/camera/camera.h"
#include <cmath>
#include <limits>
#include <new>
#include <vector>

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
void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
using Messages = std::vector<std::vector<u32>>;
std::function<void(std::span<const u32>)> recorder(Messages& messages)
{
    return [&messages](std::span<const u32> message)
    {
        messages.emplace_back(message.begin(), message.end());
    };
}

void check_animation_and_attribute_helpers()
{
    AnimationController controller;
    controller.slots.resize(3);
    controller.slots[1].player = std::make_unique<MinaPlayer>();
    controller.slots[1].player->duration = 8.0f;
    controller.slots[2].fade_mode = 7;
    std::array<u8, 0x10> event{};
    event[0xC] = 5;
    ActorAnimationMessage message{7, 0.25f, 1, event, 0};
    require(fade_runtime_player_animation_slot(&controller, message) == 0 && controller.slots[1].fade_duration == 6.0f
            && controller.slots[1].fade_mode == 3 && controller.dirty,
        "animation event kind 5 fades the slot by its remaining duration");
    controller.dirty = false;
    message.slot = 2;
    fade_runtime_player_animation_slot(&controller, message);
    require(controller.slots[2].fade_mode == 7 && controller.dirty, "an empty slot is untouched but still marks the controller dirty");
    controller.dirty = false;
    message.slot = 0;
    controller.slots[0].player = std::make_unique<MinaPlayer>();
    fade_runtime_player_animation_slot(&controller, message);
    require(controller.slots[0].fade_mode == 0 && !controller.dirty, "slot zero is never faded");
    event[0xC] = 4;
    message.slot = 1;
    controller.slots[1].fade_mode = 0;
    fade_runtime_player_animation_slot(&controller, message);
    event[0xC] = 5;
    require(controller.slots[1].fade_mode == 0 && fade_runtime_player_animation_slot(nullptr, message) == 0,
        "other event kinds and a missing controller do nothing");

    std::vector<u8> bytes(0xF00);
    Messages sent;
    RuntimeBodyAttachedActor actor{bytes, 0x1000, recorder(sent)};
    bytes[0x96F] = 3;
    bytes[0x970] = 4;
    put_float(bytes, 0x974, 2.0f);
    reset_runtime_player_attribute_timer(actor, 9);
    require(bytes[0x96D] == 9 && bytes[0x96F] == 0 && bytes[0x970] == 0 && read_u32(bytes, 0x974) == 0
            && sent == Messages{{0x55}}, "an active attribute timer is cleared with one 0x55 notice");
    put_float(bytes, 0x974, std::numeric_limits<float>::quiet_NaN());
    reset_runtime_player_attribute_timer(actor, 1);
    put_float(bytes, 0x974, -1.0f);
    reset_runtime_player_attribute_timer(actor, 1);
    require(sent.size() == 1, "NaN and negative timers send nothing");

    std::array<u8, 0x2C> attributes{};
    put(attributes, 0x28, 0x77);
    require(runtime_player_attribute_word_28(attributes) == 0x77 && runtime_player_attribute_word_28({}) == 0,
        "attribute word +0x28 is read only from a present resource");
}

void check_contact_and_spawn_messages()
{
    Messages received;
    RuntimeMessageReceiver sender{0x2000, recorder(received)};
    RuntimePlayerContactMessage contact{0xE, &sender, {1, 2, 3, 4, 5, 6}, 0xBEEF};
    relay_runtime_player_contact(contact, 0x1000);
    require(received == Messages{{0xF, 0x1000, 1, 2, 3, 4, 5, 6, 0xBEEF}}, "contact relays six words and the zero-extended +0x20 word");
    received.clear();
    std::vector<u8> bytes(0xF00);
    Messages self_messages;
    RuntimeBodyAttachedActor actor{bytes, 0x1000, recorder(self_messages)};
    acknowledge_runtime_player_contact(actor, contact);
    require(self_messages == Messages{{0x2D, 0x1000, 0x2000}} && received == Messages{{0x2D, 0x2000, 0x1000}},
        "contact acknowledgement notifies self, then the sender with owner and sender swapped");

    GhoulyController controller;
    RuntimePlayerSpawnController spawn{nullptr, 0, controller};
    GhoulySpawnMessage request;
    std::strcpy(request.name, "aid_spawn_name");
    std::strcpy(request.attributes, "aid_spawn_attributes");
    put(bytes, 0xB9C, 0xFFFF);
    accept_runtime_player_spawn(bytes, request, spawn);
    require(request.result == 1 && std::strcmp(reinterpret_cast<const char*>(bytes.data() + 0xBA0), request.name) == 0
            && std::strcmp(reinterpret_cast<const char*>(bytes.data() + 0xC20), request.attributes) == 0
            && read_u16(bytes, 0xB9C) == 0 && spawn.active == 'a', "spawn request copies both names and selects mode 0");
    GhoulyVariantMessage variant;
    std::strcpy(variant.name, "plain");
    accept_runtime_player_variant(bytes, variant, spawn);
    require(variant.result == -1, "a pending aid_ name rejects further requests");
    bytes[0xBA0] = 'x';
    accept_runtime_player_variant(bytes, variant, spawn);
    require(variant.result == 1 && bytes[0xC20] == 0 && read_u16(bytes, 0xB9C) == 1 && spawn.active == 'p',
        "variant request clears the attribute name and selects mode 1");
    bytes[0xBA0] = 'x';
    put(bytes, 0xB9C, 0x00010000); // Busy +0xB9E word.
    GhoulyVariantMessage alternate{0xAA};
    std::strcpy(alternate.name, "alt");
    accept_runtime_player_alternate_variant(bytes, alternate, spawn);
    require(alternate.result == -1 && bytes[0xBA0] == 'x', "a busy spawn slot rejects requests");
    put(bytes, 0xB9C, 0);
    accept_runtime_player_alternate_variant(bytes, alternate, spawn);
    require(alternate.result == 1 && read_u16(bytes, 0xB9C) == 2, "alternate variant selects mode 2");
}

void check_pause_and_strategy()
{
    std::vector<u8> bytes(0xF00);
    std::array<u32, 2> references{0, 0};
    std::array<u8, 4> decay{0, 1, 0, 0};
    u32 decay_index = 1;
    RuntimePlayerStrategyResume resume{references, decay, decay_index};
    require(runtime_player_can_resume_strategy(bytes, resume) == 1, "an unreferenced group follows the decay table");
    references[0] = 1;
    require(runtime_player_can_resume_strategy(bytes, resume) == 0, "a referenced group blocks resumption");
    put(bytes, 0x93C, 0x1234);
    require(runtime_player_can_resume_strategy(bytes, resume) == 1, "a linked +0x93C selects the second group");
    put(bytes, 0x93C, 0);
    put(bytes, 0xCDC, 2);
    references[1] = 1;
    require(runtime_player_can_resume_strategy(bytes, resume) == 0, "mode 2 selects the second group");
    references = {0, 0};
    put(bytes, 0xCDC, 1);

    std::vector<std::array<u32, 3>> transitions;
    ObjectFactoryRegistry factory{};
    RuntimePlayerChildContext child{factory, {}, 0x1000};
    RuntimePlayerMindRequest* first = nullptr;
    RuntimePlayerMindRequests mind_requests{first, [](RuntimePlayerMindRequest&)
        {
        }};
    RuntimePlayerMindRequests* mind = &mind_requests;
    RuntimePlayerStrategySetup setup{child, mind,
        [&](FactoryObject&, u16 current, u16 next)
        {
            transitions.push_back({1, current, next});
        },
        [&](FactoryObject&, u16 next, u16 previous)
        {
            transitions.push_back({2, next, previous});
        }};
    FactoryObject strategy;
    strategy.bytes = std::make_unique<u8[]>(0x20);
    strategy.size = 0x20;
    std::span<u8> strategy_bytes{strategy.bytes.get(), strategy.size};
    strategy_bytes[0xC] = 0x21;
    strategy_bytes[0xE] = 0x22;
    change_runtime_player_strategy_state(strategy, 0x30, setup);
    require(read_u16(strategy_bytes, 0xE) == 0x30 && transitions.size() == 2 && transitions[0] == std::array<u32, 3>{1, 0x22, 0x30}
            && transitions[1] == std::array<u32, 3>{2, 0x30, 0x22}, "strategy transition leaves, stores, then enters");
    transitions.clear();

    Actor actor;
    Messages sent;
    RuntimeBodyAttachedActor self{bytes, 0x1000, recorder(sent)};
    RuntimePlayerGoalRing*& ring = actor.goal_ring; // Actor +0x980.
    std::vector<u8> body(0x950);
    std::span<u8> body_view = body;
    entity_detail::LatchNode* latch = nullptr;
    std::array<u8, 2> zero{};
    u32 disabled = 0, value = 0, argument = 0, name = 0;
    RuntimePlayerScriptOverrides overrides{disabled, value, argument, name, disabled, value, argument, name};
    RuntimePlayerGoalCleanup goals{self, ring, body_view, actor, latch, zero, overrides};
    RuntimeScriptActorQueryContext query{[] { return std::vector<RuntimeScriptActorQueryView>{}; },
        zero.data()}; // An empty DAT_00511B38.
    RuntimePlayerPause pause{actor, strategy, setup, query, goals, resume};
    require(handle_runtime_player_pause_message(bytes, 9, pause) == 1 && read_u32(bytes, 0xCD8) == 1 && actor.parked_gate == 1
            && read_u16(strategy_bytes, 0xE) == 0x3E8 && transitions.size() == 2, "first pause enters strategy state 1000");
    // Second pause only counts; the goal ring now reports a goal so a first pause would drain.
    auto* allocation = static_cast<u8*>(engine_heap_allocate(sizeof(RuntimePlayerGoalRing) + 4));
    ring = new (allocation) RuntimePlayerGoalRing{4, 2, 0, 0, {allocation + sizeof(RuntimePlayerGoalRing), 4}};
    allocation[sizeof(RuntimePlayerGoalRing)] = 2;
    require(handle_runtime_player_pause_message(bytes, 9, pause) == 1 && read_u32(bytes, 0xCD8) == 2 && ring != nullptr
            && transitions.size() == 2, "nested pauses only count");
    handle_runtime_player_pause_message(bytes, 10, pause);
    put(bytes, 0xCD8, 0);
    actor.parked_gate = 0;
    require(handle_runtime_player_pause_message(bytes, 9, pause) == 1 && ring == nullptr && transitions.size() == 4,
        "a first pause with an active goal drains the goal ring");
    decay[1] = 0;
    require(handle_runtime_player_pause_message(bytes, 10, pause) == 1 && read_u32(bytes, 0xCD8) == 0 && transitions.size() == 4,
        "resumption needs the decay table");
    put(bytes, 0xCD8, 1);
    decay[1] = 1;
    handle_runtime_player_pause_message(bytes, 10, pause);
    require(read_u16(strategy_bytes, 0xE) == 0x21 && transitions.size() == 6, "the last resume restores the strategy's +0xC state");
    require(handle_runtime_player_pause_message(bytes, 0xC, pause) == 1 && handle_runtime_player_pause_message(bytes, 0x40, pause) == 0,
        "messages 0xB..0xD are accepted, others are not");
}

void check_activation_and_navigation()
{
    using namespace entity_detail;
    auto saved_grids = std::to_array(g_nav_grids);
    const i32 saved_count = g_nav_count;
    const i32 saved_x = g_nav_origin_x;
    const i32 saved_z = g_nav_origin_z;
    void* saved_pool = g_occupancy_pool;
    const i32 saved_mask = g_occupancy_mask;
    NavGrid grid;
    grid.width = 3;
    grid.depth = 3;
    grid.cell = 10;
    grid.cells.resize(9);
    for (i32 index = 0; index < 9; ++index)
    {
        grid.cells[size_t(index)].x = i16(5 + (index % 3) * 10);
        grid.cells[size_t(index)].z = i16(5 + (index / 3) * 10);
    }
    g_nav_grids[0] = &grid;
    g_nav_grids[1] = g_nav_grids[2] = g_nav_grids[3] = nullptr;
    g_nav_origin_x = 0;
    g_nav_origin_z = 0;
    g_nav_count = 1;
    g_nav_min_y[0] = -100.0f;
    g_nav_max_y[0] = 100.0f;

    NavigationCellRegistration* records = nullptr;
    register_navigation_cells(records, Vec3{15.0f, 0.0f, 15.0f}, 0, 6.0f, 0);
    // Centre cell and the four edge neighbours are within radius 6 + half-cell 5; the centre is
    // inside radius 6 and takes the low counter, the edge cells take bits 5..8.
    require(records != nullptr && records->end - records->begin == 5 * 12 && records->capacity - records->begin == 8 * 12
            && grid.cells[4].value_6 == 1 && grid.cells[1].value_6 == 0x20 && grid.cells[0].value_6 == 0,
        "cell registration counts inner and outer cells and grows by four rows");
    register_navigation_cells(records, Vec3{15.0f, 0.0f, 15.0f}, 0, 6.0f, 1);
    require(grid.cells[4].value_6 == 1 && grid.cells[1].value_6 == 1 && records->end - records->begin == 5 * 12,
        "re-registration first releases earlier rows; include_outer uses the low counter");
    release_navigation_cell_registration(records, 0);
    require(records == nullptr && grid.cells[4].value_6 == 0 && grid.cells[1].value_6 == 0, "release returns every counter");

    std::vector<u8> bytes(0xF00);
    std::vector<u8> body(0x950);
    Actor actor;
    actor.radius_xz = 2.0f;
    put_float(body, 0x888, 3.0f);
    put_float(bytes, 0xCC0, 15.0f);
    put_float(bytes, 0xCC8, 15.0f);
    RuntimePlayerNavigationRegistration navigation{body, actor, records};
    register_runtime_player_navigation(bytes, navigation);
    require(records != nullptr && grid.cells[4].value_6 == 1, "message 0x7D registers the body radius around +0xCC0");
    release_navigation_cell_registration(records, 0);
    put_float(body, 0x888, 0.0f);
    register_runtime_player_navigation(bytes, navigation);
    require(records == nullptr, "a zero radius registers nothing");

    Messages sent;
    RuntimeBodyAttachedActor self{bytes, 0x1000, recorder(sent), nullptr, &actor};
    std::array<ListHead, 4> links{};
    WorldLists world{};
    ListHead existing_case{};
    world.case_head.next = &existing_case;
    existing_case.prev = &world.case_head;
    std::array<u16, 2> excluded{0x55, 0x66};
    i32 excluded_count = 2;
    OccupancyRecord sphere{};
    sphere.mask = 6;
    sphere.center = Vec3{15.0f, 0.0f, 15.0f};
    sphere.extent_x = 4.0f;
    OccupancyPool pool{0x38, 4, &sphere, &sphere + 1, &sphere + 1};
    g_occupancy_pool = &pool;
    g_occupancy_mask = 6;
    const OccupancyRecord* room = nullptr;
    const OccupancyRecord* allowed = nullptr;
    RuntimePlayerGoalRing* ring = nullptr;
    std::array<u8, 2> zero{};
    u32 enabled = 1, value = 0x11, argument = 0x22, name = 0x33;
    u32 player_enabled = 0;
    RuntimePlayerScriptOverrides overrides{enabled, value, argument, name, player_enabled, value, argument, name};
    RuntimePlayerActivation activation{self, actor, links, world,
        [&](const void* node) -> u32
        {
            return node == &existing_case ? 0x5000u : 0x9999u;
        },
        excluded_count, excluded.data(), room, allowed, ring, zero, overrides};
    actor.position = Vec3{15.0f, 1.0f, 15.0f};
    actor.euler = Vec3{0.5f, 0.25f, 0.0f};
    put(bytes, 0xCDC, 2);
    bytes[6] = 0x66;
    put(bytes, 0x1FC, 1);
    put_float(bytes, 0x974, 1.0f);
    activate_runtime_player(activation);
    require(world.mark.next == &links[0] && links[0].prev == &world.mark && world.slot.next == &links[3]
            && existing_case.next == &links[1] && links[1].prev == &existing_case && links[1].next == nullptr
            && read_u32(bytes, 0x93C) == 0x5000 && actor.mask_flag == 0x5000 && world.alt.next == nullptr,
        "mode 2 activation links the mark, slot and case lists");
    require(read_f32(bytes, 0xCC0) == 15.0f && read_f32(bytes, 0xCC4) == 1.0f && read_f32(bytes, 0xCCC) == 0.5f
            && actor.parked_pos.y == 1.0f && room == nullptr && allowed == nullptr,
        "activation stores the world transform; mode 2 skips occupancy");
    // Without a goal 0x00138030 runs, but mode 2 is neither player mode so it sends nothing.
    require(bytes[0x96D] == 1 && sent == Messages{{0x55}} && read_u32(bytes, 0x930) == 0 && read_u32(bytes, 0xCEC) == 0,
        "an excluded tag resets the attribute timer and the floor is stored");

    std::vector<u8> second_bytes(0xF00);
    Actor second_actor;
    second_actor.position = Vec3{15.0f, 0.0f, 15.0f};
    RuntimeBodyAttachedActor second{second_bytes, 0x1100, recorder(sent), nullptr, &second_actor};
    std::array<ListHead, 4> second_links{};
    auto* allocation = static_cast<u8*>(engine_heap_allocate(sizeof(RuntimePlayerGoalRing) + 4));
    ring = new (allocation) RuntimePlayerGoalRing{4, 2, 0, 0, {allocation + sizeof(RuntimePlayerGoalRing), 4}};
    allocation[sizeof(RuntimePlayerGoalRing)] = 3;
    RuntimePlayerActivation second_activation{second, second_actor, second_links, world, activation.address_of, excluded_count,
        excluded.data(), room, allowed, ring, zero, overrides};
    put(second_bytes, 0xCDC, 1);
    put(second_bytes, 0x1FC, 1);
    const size_t before = sent.size();
    activate_runtime_player(second_activation);
    require(world.mark.next == &second_links[0] && second_links[0].next == &links[0] && links[0].prev == &second_links[0]
            && world.alt.next == &second_links[2] && room == &sphere && allowed == &sphere,
        "mode 1 activation inserts after the mark head and records both occupancy hits");
    require(second_bytes[0x98B] == 1 && sent.size() == before, "an active goal defers script override restoration");
    engine_heap_free(ring);

    g_occupancy_pool = saved_pool;
    g_occupancy_mask = saved_mask;
    for (size_t index = 0; index < 4; ++index)
    {
        g_nav_grids[index] = saved_grids[index];
    }
    g_nav_count = saved_count;
    g_nav_origin_x = saved_x;
    g_nav_origin_z = saved_z;
}

bool near(float value, float expected)
{
    return std::fabs(value - expected) < 1e-5f;
}

void check_player_overlay()
{
    std::array<std::array<u8, 0x20>, 10> digit_sprites{};
    std::array<const u8*, 10> digits{};
    for (size_t index = 0; index < 10; ++index)
    {
        digit_sprites[index][8] = 50; // Width 50 * 0.02 = 1.
        digits[index] = digit_sprites[index].data();
    }
    DisplayView view;
    view.forward = Vec3{0.0f, 0.0f, 1.0f};
    static BillboardQueue queue;
    queue.count = 0;
    queue_runtime_overlay_digits(42, Vec3{0.0f, 2.0f, 0.0f}, 0x11223344, digits.data(), view, queue);
    require(queue.count == 2 && queue.entries[0].sprite == digits[4] && queue.entries[1].sprite == digits[2]
            && near(queue.entries[0].position.x, 0.6f) && near(queue.entries[1].position.x, -0.6f)
            && queue.entries[0].position.y == 2.0f && queue.entries[0].size[0] == 1.0f && queue.entries[0].frame == 0xFF
            && queue.entries[1].color == 0x11223344, "digits are queued highest first, centred along the camera side vector");
    queue.count = 0;
    queue_runtime_overlay_digits(0, Vec3{}, 0, digits.data(), view, queue);
    require(queue.count == 1 && queue.entries[0].sprite == digits[0] && queue.entries[0].position.x == 0.0f, "zero draws one centred digit");
    queue.count = 0x800;
    queue_billboard(queue, digits[1], 0, 0, Vec3{}, std::array<float, 2>{1.0f, 1.0f}.data(), 0.0f);
    require(queue.count == 0x800, "a full billboard queue drops new sprites");

    std::vector<u8> body(0x950);
    body[0x96] = 100;
    body[0x94] = 24;
    require(runtime_body_health_color(body) == 0xFF2020FF, "health below a quarter is red");
    body[0x94] = 50;
    require(runtime_body_health_color(body) == 0xFF00FFFF, "exactly half falls into the third band");
    body[0x94] = 75;
    require(runtime_body_health_color(body) == 0xFF00FF00, "three quarters and above use the full colour");
    body[0x96] = 0;
    require(runtime_body_health_color(body) == 0xFF00FF00, "no maximum uses the full colour");

    std::vector<u8> owner_bytes(0xF00);
    std::vector<u8> attributes(0xC0);
    require(runtime_player_uses_alternate_overlay(owner_bytes, {}) == 0, "no timer and no flag keeps the health sprite");
    owner_bytes[0x969] = 1;
    require(runtime_player_uses_alternate_overlay(owner_bytes, attributes) == 1, "the +0x969 flag selects the alternate sprites");
    put(attributes, 0xB8, 1);
    require(runtime_player_uses_alternate_overlay(owner_bytes, attributes) == 0, "attribute word +0xB8 suppresses it");

    Actor owner;
    owner.position = Vec3{1.0f, 2.0f, 3.0f};
    owner.radius_y = 2.0f;
    owner.model_size_y = 3.0f;
    owner.model_size_z = 1.0f;
    std::array<u8, 0x20> counter{};
    std::array<u8, 0x20> unused{};
    body.assign(0x950, 0);
    put(body, 0xE0, 1);
    put_float(body, 0xDC, 7.9f);
    queue.count = 0;
    RuntimeBodyOverlay overlay{body, owner, owner_bytes, {}, nullptr, unused.data(), unused.data(), unused.data(), counter.data(),
        digits.data(), view, queue};
    queue_runtime_body_overlay(1, overlay);
    require(queue.count == 0, "the overlay only draws in pass 2");
    queue_runtime_body_overlay(2, overlay);
    // Anchor y = 2 + 2 * 1 = 4, plus |(3 - 1) * 2| = 4 and rise 3 gives 11; digits rise 3 - 3.
    require(queue.count == 2 && queue.entries[0].sprite == counter.data() && queue.entries[0].color == 0xFFFFFFFF
            && queue.entries[0].position.y == 11.0f && queue.entries[0].size[0] == 3.0f && queue.entries[1].sprite == digits[7]
            && queue.entries[1].position.y == 11.0f && near(queue.entries[1].position.x, -2.0f),
        "a counter overlay queues its sprite, then the truncated value shifted three units sideways");
}
}

void check_player_messages()
{
    check_animation_and_attribute_helpers();
    check_contact_and_spawn_messages();
    check_pause_and_strategy();
    check_activation_and_navigation();
    check_player_overlay();
}
