#include "player_update_checks.h"
#include "../src/engine/entities/player_update.h"
#include "../src/engine/entities/actor_transform.h"
#include "../src/engine/entities/entity_collision.h"
#include "gbtg/model_runtime.h"
#include <cmath>
#include <memory>
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

// Saves the floor-grid globals and clears them for the fixture.
struct NavigationGlobals
{
    std::array<entity_detail::NavGrid*, entity_detail::kNavSlots> grids;
    i32 count;
    std::array<float, entity_detail::kNavBands> min_y;
    std::array<float, entity_detail::kNavBands> max_y;
    i32 origin_x;
    i32 origin_z;
    NavigationGlobals()
    {
        using namespace entity_detail;
        std::copy(std::begin(g_nav_grids), std::end(g_nav_grids), grids.begin());
        count = g_nav_count;
        std::copy(std::begin(g_nav_min_y), std::end(g_nav_min_y), min_y.begin());
        std::copy(std::begin(g_nav_max_y), std::end(g_nav_max_y), max_y.begin());
        origin_x = g_nav_origin_x;
        origin_z = g_nav_origin_z;
        std::fill(std::begin(g_nav_grids), std::end(g_nav_grids), nullptr);
        g_nav_count = 0;
    }
    ~NavigationGlobals()
    {
        using namespace entity_detail;
        std::copy(grids.begin(), grids.end(), std::begin(g_nav_grids));
        g_nav_count = count;
        std::copy(min_y.begin(), min_y.end(), std::begin(g_nav_min_y));
        std::copy(max_y.begin(), max_y.end(), std::begin(g_nav_max_y));
        g_nav_origin_x = origin_x;
        g_nav_origin_z = origin_z;
    }
};

// Native and retail views the update reads; one instance drives several frames.
struct PlayerUpdateFixture
{
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();
    std::vector<u8> bytes = std::vector<u8>(0xD00);
    std::vector<std::vector<u32>> messages;
    RuntimeBodyAttachedActor self{bytes, 0x9000, [this](std::span<const u32> message)
        {
            messages.emplace_back(message.begin(), message.end());
        }};
    std::array<u8, 0x100> device{};
    std::unordered_map<u32, std::span<u8>> devices;
    GhoulySpawnPool entries{0x50, 1, nullptr, nullptr, nullptr};
    i32 selected = 0;
    std::array<u8, 0x3C> keyboard{};
    std::array<u32, 5> ports{};
    RuntimeInputActivationContext activation{entries, selected, {}, keyboard, ports.data() + 1, devices};
    std::unordered_map<u32, std::span<u8>> release_addresses;
    RuntimeInputReleaseContext release{release_addresses};
    u32 frame_id = 3;
    RuntimePlayerGoalRing* ring = nullptr;
    std::array<u8, 4> zero{};
    FactoryObject strategy;
    FactoryObject mind;
    FactoryObject body_object;
    std::vector<int> calls;
    entity_detail::NavigationPathSearch search;
    entity_detail::NavigationPathSearch* search_pointer = &search;
    std::array<u8, 0x20> hint{};
    const u8* hint_pointer = hint.data();
    std::vector<u8> body = std::vector<u8>(0x950);
    std::array<u8, 8> bit_bytes{};
    EntityBitBlock bits{bit_bytes.data()};
    EntityBitBlock* bits_pointer = &bits;
    EntityMaskHost mask;
    float freeze = 0.f;
    float dt = 0.1f;
    float scale = 2.f;
    u32 block_decay = 0;
    std::array<u8, 4> decay{1, 1, 1, 1};
    u32 decay_index = 0;
    RuntimeActorClock clock{freeze, dt, scale, block_decay, decay, decay_index};
    RuntimeBodyCollision collision_view{body, *actor, &bits, nullptr, mask, clock};
    RuntimeBodyCollisionState* collision = nullptr;
    ActorAnimationState animation{};
    RuntimeBodyAttachedActor* attached = nullptr;
    RuntimeBodyAnimationReceiver* first_receiver = nullptr;
    RuntimeBodyAnimationReceiver* second_receiver = nullptr;
    FactoryObject first_action;
    FactoryObject second_action;
    const u8* row = nullptr;
    const RuntimeScriptLookupTable* table = nullptr;
    std::unordered_map<u32, const char*> names;
    ObjectFactoryRegistry factory;
    RuntimePlayerChildContext child{factory, {}, 0x9000};
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    u32 random = 1;
    RuntimeBodyActionSelection actions{animation, attached, first_receiver, second_receiver, first_action,
        second_action, row, table, names, child, registry, error, random, 0xA000};
    entity_detail::ListHead players{};
    ModelAnimationState model{};
    ActorAttachmentList attachments{{}, 0, &pending.sentinel}; // Pending view: actor +0x1B4.
    ActorAttachmentList pending{};
    ActorFrameAttachment frame_update{nullptr, [](ActorFrameAttachment&, Actor&)
        {
        }};
    ActorFrameAttachment* updates = nullptr;
    entity_detail::ShapeProbe shape_probe;
    RuntimeModelResourceViews views;
    RuntimeModelDeferredPool deferred{};
    u32 fence = 0;
    RuntimeShaderRegistrationState shaders;
    RuntimeShaderAnimationScratch scratch;
    std::function<void(u8*)> free_model;
    ActorEffectModelContext models{registry, error, views, deferred, fence, {}, shaders, scratch, free_model};
    RuntimeActorSoundQueues sounds{};
    u32 effect_random = 7;
    Vec3 origin{};
    Vec3 default_euler{};
    ActorEffectStreamContext effects{models, sounds, effect_random, origin, default_euler};
    ModelNodeFlagCache cache{};
    const AnimationMatrix reset{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const RuntimeShaderMatrixBoundaries shader_matrices{};
    ActorFrameContext frame{effects, nullptr, cache, reset, shader_matrices, 0.f, 0.f, 0.f, 1, 0, 1};
    entity_detail::NavigationCellRegistration* navigation = nullptr;
    const OccupancyRecord* allowed = nullptr;
    const OccupancyRecord* current = nullptr;
    RuntimePlayerRoomTracking room{self, body, allowed, current, true};
    u8 bounds_enabled = 0;
    float bound_value = 0.f;
    RuntimeWorldBounds bounds{bounds_enabled, bound_value, bound_value, bound_value, bound_value, bound_value,
        bound_value};
    std::unique_ptr<RuntimePlayerUpdate> update;

    PlayerUpdateFixture()
    {
        self.actor = actor.get();
        strategy.bytes = std::make_unique<u8[]>(0x40);
        mind.bytes = std::make_unique<u8[]>(0x40);
        body_object.bytes = std::make_unique<u8[]>(0x40);
        second_action.bytes = std::make_unique<u8[]>(0x20);
        devices.emplace(0x7000, std::span<u8>(device));
        update = std::make_unique<RuntimePlayerUpdate>(RuntimePlayerUpdate{self, device, 0x7000, activation, release,
            frame_id, ring, zero, strategy, mind, body_object,
            [this](FactoryObject&)
            {
                calls.push_back(1);
            },
            [this](FactoryObject&, u32 input)
            {
                calls.push_back(input == 0x7000 ? 2 : -2);
            },
            [this](FactoryObject&, u32 input)
            {
                calls.push_back(input == 0x7000 ? 3 : -3);
            },
            [this](FactoryObject&)
            {
                calls.push_back(4);
            },
            [this](FactoryObject&)
            {
                calls.push_back(5);
            },
            [this](FactoryObject&)
            {
                calls.push_back(6);
                return 0u;
            },
            [this](FactoryObject&, u32 input)
            {
                calls.push_back(input == 0x7000 ? 7 : -7);
                return u16(9);
            },
            search_pointer, hint_pointer, collision_view, bytes, bits_pointer, collision, actions, nullptr,
            [](entity_detail::SegmentProbe&) -> RuntimeSegmentContactActor&
            {
                std::abort();
            },
            players, animation, model, attachments, pending.sentinel, updates, shape_probe, frame, clock,
            navigation, room, bounds});
        put(bytes, 0x960, 0x7000);
    }
};

void check_update_order_and_body_frame()
{
    NavigationGlobals saved;
    PlayerUpdateFixture f;
    Actor& actor = *f.actor;
    actor.position = Vec3{4.f, 5.f, 6.f};
    actor.euler = Vec3{0.1f, 0.2f, 0.3f};
    actor.animation_rate = 1.5f;
    actor.animation_pause = 0.f;
    actor.model_flags = 0x33;
    actor.model_mode = 0x44;
    put(f.device, 0, 1);
    put(f.device, 0x38, 0x12345678);
    f.bit_bytes[0] = 0xFF;
    f.bit_bytes[1] = 0x01;
    put_float(f.bytes, 0x324, 10.f);
    put_float(f.bytes, 0x318, 0.f);
    put_float(f.bytes, 0x924, 1.f);
    put(f.bytes, 0x928, 7);
    put_float(f.body, 0x318, 3.f);
    put(f.bytes, 0xCEC, 0);
    f.updates = &f.frame_update;
    update_runtime_player(*f.update);
    require(f.calls == std::vector<int>{1, 2, 3, 4, 6, 5, 7},
        "the player update ticks strategy, mind and body before animating, supporting and settling the body");
    require(read_u32(f.device, 0x38) == 0, "an analog input device is reset each frame");
    require(f.bit_bytes[0] == 0xF1 && f.bit_bytes[1] == 0x11,
        "body bits 2, 3 and 1 clear and an unsupported body sets the airborne bit");
    require(read_f32(f.body, 0x24) == 4.f && read_f32(f.body, 0x28) == 5.f && read_f32(f.body, 0x2C) == 6.f,
        "the supported-body probe records the world position");
    require(actor.model_flags == 0x33 && actor.model_mode == 0x44,
        "the collision filter and shape mode are restored after the body sphere pass");
    require(read_f32(f.bytes, 0x318) == 1.f, "eased positions move a tenth toward the target");
    require(read_f32(f.body, 0x18) == 4.f && read_f32(f.body, 0x318) == 0.f
            && read_f32(f.body, 0x33C) == actor.euler.y,
        "the body copies the owner position and yaw and clears its push accumulator");
    require(std::abs(read_f32(f.bytes, 0x924) - 1.3f) < 0.00001f && read_u32(f.bytes, 0x928) == 8,
        "the actor clock advances by rate times step once per frame");

    f.calls.clear();
    put(f.bytes, 0xCD8, 1);
    put(f.device, 0, 2);
    put(f.device, 4, 0);
    actor.animation_pause = 0.25f;
    update_runtime_player(*f.update);
    require(f.calls == std::vector<int>{3, 4, 5, 7},
        "a paused player skips strategy, mind, body support and the contact walk");
    require(actor.animation_pause == 0.25f - 0.2f && read_u32(f.bytes, 0x928) == 9,
        "the pause timer decays and stops the clock while it is still running");
}

void check_body_follow_and_navigation()
{
    NavigationGlobals saved;
    PlayerUpdateFixture f;
    Actor& actor = *f.actor;
    actor.position = Vec3{1.f, 2.f, 3.f};
    actor.animation_rate = 1.f;
    put(f.device, 0, 2);
    put(f.body, 0x258, 1);
    put_float(f.body, 0x18, 4.f);
    put_float(f.body, 0x1C, 6.f);
    put_float(f.body, 0x20, 3.f);
    f.body[0x716] = 5;
    put(f.body, 0x340, 0x4F);
    update_runtime_player(*f.update);
    require(f.calls == std::vector<int>{1, 2, 3, 4, 5, 7}, "a carried body skips the support probe");
    require(f.body[0x716] == 0 && read_u32(f.body, 0x340) == 0 && read_f32(f.body, 0x704) == 3.f
            && read_f32(f.body, 0x708) == 4.f && read_f32(f.body, 0x70C) == 0.f,
        "a carried body records its offset in the wrapped motion history");
    require(read_f32(f.body, 0x344 + 0x4F * 12) == 15.f,
        "the motion sample is divided by rate times step");
    require(actor.position.x == 4.f && actor.position.y == 6.f && read_f32(f.body, 0x18) == 4.f,
        "the actor moves to the body and the body copies the moved owner");

    using namespace entity_detail;
    NavGrid grid;
    grid.width = 4;
    grid.depth = 4;
    grid.cell = 4;
    grid.cells.assign(16, NavCell{});
    for (size_t index = 0; index < grid.cells.size(); ++index)
    {
        grid.cells[index].x = i16(index % 4 * 4 + 2);
        grid.cells[index].z = i16(index / 4 * 4 + 2);
    }
    g_nav_grids[0] = &grid;
    g_nav_grids[1] = &grid;
    g_nav_count = 2;
    g_nav_min_y[0] = -100.f;
    g_nav_max_y[0] = -50.f;
    g_nav_min_y[1] = -10.f;
    g_nav_max_y[1] = 10.f;
    g_nav_origin_x = 0;
    g_nav_origin_z = 0;
    ActorModel model{};
    std::array<u8, 8> cpu{};
    model.cpu = cpu;
    auto runtime = std::make_unique<RuntimeModelInstance>();
    model.runtime = runtime.get();
    actor.model = &model;
    actor.radius_xz = 2.f;
    put_float(f.body, 0x888, 1.5f);
    put(f.body, 0x258, 0);
    put(f.bytes, 0xCEC, 0);
    f.hint[0x19] = 9;
    std::array<u8, 0x14 * 16> nodes{};
    f.search.nodes = nodes.data();
    f.search.count = 77;
    f.messages.clear();
    f.calls.clear();
    actor.position = Vec3{6.f, 0.f, 6.f};
    update_runtime_player(*f.update);
    require(read_u32(f.bytes, 0xCEC) == 1, "a higher floor band selects floor 1");
    require(f.search.count == 16 && f.search.first == -1 && read_u16(nodes, 0x14 * 15 + 0xA) == 15,
        "a floor change resets the mind path search over the new grid");
    require(!f.messages.empty() && f.messages.back() == std::vector<u32>{0x24}, "a floor change notifies 0x24");
    require(f.navigation != nullptr && f.navigation->end != f.navigation->begin,
        "a modelled player registers the cells inside its scaled radius");
    actor.model = nullptr;
    update_runtime_player(*f.update);
    require(f.navigation == nullptr, "a missing model releases the registered navigation cells");
}
}

void check_player_update()
{
    check_update_order_and_body_frame();
    check_body_follow_and_navigation();
}
