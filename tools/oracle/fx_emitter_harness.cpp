// Host side of fx_emitter_oracle.py. The emitter, actor query, frustum and intro notice modules
// are linked with logging stubs for every other callee, fed from the retail hooks' queues.
#include "entities/fx_anim_emitter.h"
#include "camera/view_frustum.h"
#include "entities/body_animation_commands.h"
#include "gbtg/game.h"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <deque>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <xmmintrin.h>
using namespace gbtg;

namespace
{
constexpr u32 kEmitter = 0x10000000;
const char* const kQueues[] = {"acquire", "find", "height", "bound", "sqdist", "soundactive", "sphere", "joint", "draw"};

std::map<std::string, std::deque<u32>> g_queues;
std::vector<std::string> g_log;
std::vector<u32> g_line;
size_t g_cursor = 0;

u32 next()
{
    return g_line.at(g_cursor++);
}

float next_float()
{
    const u32 bits = next();
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

u32 pop(const char* name)
{
    auto& queue = g_queues[name];
    if (queue.empty())
    {
        return 0;
    }
    const u32 value = queue.front();
    queue.pop_front();
    return value;
}

u32 bits_of(float value)
{
    u32 bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

float float_of(u32 bits)
{
    float value;
    std::memcpy(&value, &bits, 4);
    return value;
}

void logf(const char* format, ...)
{
    char buffer[512];
    va_list args;
    va_start(args, format);
    std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    g_log.emplace_back(buffer);
}

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

template<class T>
T& unused()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}

std::map<const Actor*, std::string> g_names;
std::map<const u8*, std::string> g_resources;
const u8* g_sprite = nullptr;
const u8* g_block4 = nullptr;
const EntityBonePose* g_alternate = nullptr;
const EntityBonePose* g_primary = nullptr;
const AnimationRecord* g_record = nullptr;
Actor* g_emitter_actor = nullptr;
entity_detail::ListHead* g_anchor_base = nullptr;

const char* who(const Actor* actor)
{
    if (actor == nullptr)
    {
        return "none";
    }
    auto found = g_names.find(actor);
    return found != g_names.end() ? found->second.c_str() : "?";
}
}

// The real 0x00152B30, 0x00153320 and 0x00153230, wrapped the way anim.cpp includes them.
namespace gbtg
{
namespace
{
#include "../../src/gbtg/animation/anim_math.inl"
}
long double retail_square_root(float value)
{
    return animation_sqrt(value);
}
#include "../../src/gbtg/animation/anim_scalar.inl"
}

namespace gbtg
{
// Verified retail helpers the modules call; harness copies.
float engine_absolute(float value)
{
    if (0.f <= value)
    {
        return value;
    }
    return 0.f - value;
}
void dispatch_actor_resource_callout(const RuntimeActorResourceReference& resource, i32 id,
    const RuntimeActorCreationOptions* options, ActorAttachmentEventContext& context)
{
    if (context.actor_address + 0x8Cu == 0)
    {
        return;
    }
    dispatch_actor_attachment_records(resource.resource.data(), id, nullptr, options, context, 0);
}
void clear_runtime_player_attachment_events(std::span<u8> events, RuntimeBodyAttachmentLists& lists)
{
    i32 index = 0;
    while (index < std::bit_cast<i32>(read_u32(events, 0)))
    {
        remove_runtime_body_attachment(u16(read_u32(events, size_t(index + 1) * 4)), lists);
        ++index;
    }
    const u32 zero = 0;
    std::memcpy(events.data(), &zero, 4);
}

// Logging stubs.
void update_actor_frame(ActorFrameState& state, ActorFrameContext&)
{
    logf("frame %s", who(&state.actor));
}
Vec3 actor_world_position(const Actor& actor)
{
    return actor.position;
}
void actor_set_world_position(Actor& actor, const Vec3& point)
{
    logf("setposition %s %08x %08x %08x", who(&actor), bits_of(point.x), bits_of(point.y), bits_of(point.z));
    actor.position = point;
}
i32 dispatch_actor_attachment_records(const u8* record, i32 id, ActorAttachmentEventCallback callback,
    const RuntimeActorCreationOptions* options, ActorAttachmentEventContext& context, u32 value)
{
    auto found = g_resources.find(record);
    logf("records %s %x %x %x %s %x", found != g_resources.end() ? found->second.c_str() : "?", id,
        callback == record_runtime_body_attachment_event ? 0x13B8B0u : 0u, options == nullptr ? 0u : 1u,
        who(&context.actor), value != 0 ? value - kEmitter : 0u);
    return 0;
}
void record_runtime_body_attachment_event(const u8*, const RuntimeActorCreationOptions*, ActorAttachmentEventContext&,
    u32)
{
}
bool remove_runtime_body_attachment(u16 token, RuntimeBodyAttachmentLists&)
{
    logf("remove %x", token);
    return true;
}
std::span<const u8> acquire_named_animation_resource(AnimationResourceRegistry&, const char* name, ResourceErrorState&)
{
    const u32 value = pop("acquire");
    logf("acquire %s", name);
    return value != 0 ? std::span<const u8>(g_sprite, 4) : std::span<const u8>();
}
void release_animation_resource(AnimationResourceRegistry&, std::span<const u8> resource)
{
    logf("release %s", resource.data() == g_sprite ? "sprite" : "?");
}
const AnimationRecord* find_actor_animation(const AnimationTable*, const char* name, const char*, u32&)
{
    const u32 value = pop("find");
    logf("find %s", name);
    return value != 0 ? g_record : nullptr;
}
void start_actor_animation(ActorAnimationState&, const AnimationRecord& record, AnimationResourceRegistry&,
    ResourceErrorState&, u32&)
{
    logf("start %x", &record == g_record ? 0x40 : 0xFFFF);
}
void release_animation_controller(std::unique_ptr<AnimationController>& controller, bool send_end,
    AnimationResourceRegistry&)
{
    logf("releasectl %d", send_end ? 1 : 0);
    controller.reset();
}
long double actor_model_height(const Actor& actor)
{
    const u32 value = pop("height");
    logf("height %s", who(&actor));
    return value % 2 != 0 ? 3.0f : 0.5f;
}
long double actor_model_bound_radius(const Actor& actor)
{
    const u32 value = pop("bound");
    logf("bound %s", who(&actor));
    return value % 2 != 0 ? 2.0f : 0.25f;
}
float runtime_actor_squared_distance(const Actor& first, const Actor& second)
{
    const u32 value = pop("sqdist");
    logf("sqdist %s %s", who(&first), who(&second));
    const float distances[]{100.f, 2000.f, 900.f};
    return distances[value % 3];
}
bool actor_sound_is_active(const RuntimeActorSound& sound, const RuntimeActorSoundQueues&)
{
    const u32 value = pop("soundactive");
    const size_t index = static_cast<const ActorSoundSlot*>(&sound) - g_emitter_actor->sound_slots.data();
    logf("soundactive %x", u32(0x334 + 0xB8 * index));
    return value != 0;
}
void activate_actor_bind_tree(Actor&)
{
    logf("bindtree");
}
float shape_sphere_metrics(const EntityShapeNode& node, const float*, const EntityBonePose* pose, Vec3& center)
{
    const u32 value = pop("sphere");
    const u8* record = reinterpret_cast<const u8*>(&node);
    logf("sphere %x %d", u32(record - g_block4), pose == nullptr ? 0 : pose == g_alternate ? 1 : pose == g_primary ? 2 : 9);
    const float scale = float(value + 1);
    const float heights[]{1.0f, -6.0f, 9.0f};
    center = Vec3{0.5f * scale, heights[value % 3], -2.0f * scale};
    const float radii[]{0.3f, 1.5f, 4.0f};
    return radii[value % 3];
}
void initialize_actor_effect_parameters(u8* parameters, const char*)
{
    logf("params");
    std::memset(parameters, 0x5A, 0x354);
}
FactoryObject create_factory_object(ObjectFactoryRegistry&, std::span<const u8> p)
{
    logf("create %x %x %x %x %x %x %x %x %x %x %x %x %x", read_u16(p, 0), read_u16(p, 2), read_u16(p, 4),
        read_u32(p, 0x24), read_u32(p, 0x334) - kEmitter, read_u32(p, 0x338), p[0x33C], read_u32(p, 0x340),
        read_u32(p, 0x344), read_u32(p, 0x348), read_u32(p, 0x34C), read_u32(p, 0x350), p[0x10]);
    return {};
}
void unregister_actor_segment_probes(ActorRenderRegistration&)
{
    logf("unregister");
}
void clear_actor_shape_probe(Actor& actor, entity_detail::ShapeProbe&)
{
    actor.model_mode = 0;
    logf("probe %x", u32(actor.model_mode));
}
void clear_actor_attachment_effects(ActorAttachmentList&, ActorAttachmentNode&)
{
    logf("effects");
}
void release_actor_model_anchor(ActorModelAnchor& anchor)
{
    // Retail test anchors sit 0x40 apart.
    const size_t index = &anchor - reinterpret_cast<ActorModelAnchor*>(g_anchor_base);
    logf("anchor %x", u32(index * 0x40));
}
void queue_billboard(BillboardQueue&, const u8* sprite, u32 color, u32 frame, const Vec3& position, const float* size,
    float rotation)
{
    logf("billboard %s %08x %x %08x %08x %08x %08x %08x %08x", sprite == g_sprite ? "sprite" : "?", color, frame,
        bits_of(position.x), bits_of(position.y), bits_of(position.z), bits_of(size[0]), bits_of(size[1]),
        bits_of(rotation));
}
void actor_transform_joint_point(const Actor& actor, u16 joint, const Vec3& point, Vec3& output)
{
    const u32 value = pop("joint");
    (void)point;
    logf("joint %s %x origin", who(&actor), joint);
    output = Vec3{1.0f * float(value), -1.0f * float(value), 2.5f * float(value)};
}
u32 draw_runtime_actor(ActorDrawState&, i32 kind, const ActorFrameDrawContext&, SceneModelDrawContext&)
{
    const u32 value = pop("draw");
    logf("draw %s %x", who(g_emitter_actor), u32(kind));
    return value;
}
}

namespace
{
struct PlayerSeed
{
    std::unique_ptr<Actor> actor = std::make_unique<Actor>();
    std::vector<u8> actor_bytes = std::vector<u8>(0xD00);
    std::vector<u8> body = std::vector<u8>(0x900);
    std::unique_ptr<Actor> body_actor = std::make_unique<Actor>();
    std::unique_ptr<Actor> case_actor = std::make_unique<Actor>();
    bool case_present = false;
    std::array<u8, 4> resource_bytes{};
    std::array<u8, 8> bit_bytes{};
    EntityBitBlock bits{};
    RuntimeActorResourceReference resource{};
};

void run_case()
{
    g_log.clear();
    g_queues.clear();
    g_line.clear();
    g_cursor = 0;
    std::string text;
    std::getline(std::cin >> std::ws, text);
    {
        size_t position = 0;
        while (position < text.size())
        {
            size_t end = text.find(' ', position);
            if (end == std::string::npos)
            {
                end = text.size();
            }
            if (end > position)
            {
                g_line.push_back(u32(std::stoul(text.substr(position, end - position))));
            }
            position = end + 1;
        }
    }
    const u32 entry = next();
    for (const char* name : kQueues)
    {
        for (int index = 0; index < 12; ++index)
        {
            g_queues[name].push_back(next());
        }
    }
    // Emitter actor.
    auto actor = std::make_unique<Actor>();
    g_emitter_actor = actor.get();
    g_names.clear();
    g_names[actor.get()] = "emitter";
    std::vector<u8> bytes(0xA00);
    actor->position = Vec3{next_float(), next_float(), next_float()};
    actor->scale_x = next_float();
    actor->radius_y = next_float();
    actor->radius_xz = next_float();
    actor->scale_200 = next_float();
    actor->model_factor = next_float();
    put(bytes, 0x1D0, next());
    actor->anchor = Vec3{next_float(), next_float(), next_float()};
    ActorAnimationState animation{};
    static AnimationTable table_storage{};
    animation.table = next() != 0 ? &table_storage : nullptr;
    if (next() != 0)
    {
        animation.controller = std::make_unique<AnimationController>();
    }
    bytes[0x19B] = u8(next());
    actor->animation_rate = next_float();
    actor->animation_pause = next_float();
    actor->model_mode = 7;
    for (auto& slot : actor->sound_slots)
    {
        slot.previous_bone = u16(next());
    }
    // Model.
    const bool model_present = next() != 0;
    const u32 first_kind = next();
    std::vector<u8> cpu(0x400);
    put(cpu, 0, 8);
    put(cpu, 4, 2);
    put(cpu, 8, first_kind);
    put(cpu, 0xC, 0x100);
    put(cpu, 0x10, 4);
    put(cpu, 0x14, 0x200);
    put(cpu, 0x100, next());
    const u32 shape_count = next();
    for (u32 index = 0; index < shape_count; ++index)
    {
        const size_t record = 0x200 + 8 + 0x24 * index;
        cpu[record] = u8(next());
        cpu[record + 1] = index + 1 < shape_count ? 0x24 : 0;
        put_u16(cpu, record + 2, u16(next()));
        put(cpu, record + 4, next());
    }
    g_block4 = cpu.data() + 0x200;
    ActorModel model{};
    model.cpu = cpu;
    actor->model = model_present ? &model : nullptr;
    // Matrix records.
    ActorMatrixSetup matrices{};
    const u32 matrix_count = next();
    matrices.flags = next();
    matrices.progress = next_float();
    std::vector<u8> matrix_records(0x48 * 4);
    for (u32 index = 0; index < matrix_count; ++index)
    {
        for (int axis = 0; axis < 3; ++axis)
        {
            put(matrix_records, 0x48 * index + 4 + 4 * axis, next());
        }
        put(matrix_records, 0x48 * index + 0x44, next());
    }
    matrices.matrices.records = matrix_records;
    matrices.matrices.count = i32(matrix_count);
    // Emitter fields.
    put(bytes, 0x93C, next());
    put(bytes, 0x940, next());
    put(bytes, 0x944, next());
    put(bytes, 0x948, next());
    put(bytes, 0x94C, next());
    const u32 tokens = next();
    put(bytes, 0x950, tokens);
    for (u32 index = 0; index < tokens; ++index)
    {
        put(bytes, 0x954 + 4 * index, next());
    }
    for (size_t offset : {0x974, 0x978, 0x97C, 0x980, 0x984, 0x988})
    {
        put(bytes, offset, next());
    }
    put(bytes, 0x98C, next());
    put(bytes, 0x990, next());
    static const std::array<u8, 4> sprite_bytes{};
    g_sprite = sprite_bytes.data();
    std::span<const u8> sprite = next() != 0 ? std::span<const u8>(sprite_bytes) : std::span<const u8>();
    for (size_t offset : {0x998, 0x99C, 0x9A0, 0x9A4, 0x9A8, 0x9AC})
    {
        put(bytes, offset, next());
    }
    for (int index = 0; index < 8; ++index)
    {
        bytes[0x184 + index] = u8(next());
    }
    const u32 anchor_count = next();
    std::vector<ActorModelAnchor> anchor_nodes(3);
    std::memset(anchor_nodes.data(), 0, sizeof(ActorModelAnchor) * 3);
    entity_detail::ListHead anchors{};
    for (u32 index = 0; index < anchor_count; ++index)
    {
        anchor_nodes[index].owner_link.next = index + 1 < anchor_count ? &anchor_nodes[index + 1] : nullptr;
    }
    anchors.next = anchor_count != 0 ? &anchor_nodes[0] : nullptr;
    g_anchor_base = reinterpret_cast<entity_detail::ListHead*>(anchor_nodes.data());
    // Players.
    const u32 player_count = next();
    std::array<PlayerSeed, 2> seeds;
    static EntityBonePose alternate_pose{};
    static EntityBonePose primary_pose{};
    g_alternate = &alternate_pose;
    g_primary = &primary_pose;
    u32 alternate_word = 0;
    u32 primary_word = 0;
    for (int index = 0; index < 2; ++index)
    {
        PlayerSeed& seed = seeds[index];
        g_names[seed.actor.get()] = index == 0 ? "p0" : "p1";
        g_names[seed.body_actor.get()] = index == 0 ? "b0" : "b1";
        g_names[seed.case_actor.get()] = index == 0 ? "c0" : "c1";
        put(seed.actor_bytes, 0xCE0, next());
        put_u16(seed.actor_bytes, 0xCDC, u16(next()));
        put(seed.actor_bytes, 0x93C, next());
        put_u16(seed.body, 0x94, u16(next()));
        put(seed.body, 0x884, next());
        put(seed.body, 0x888, next());
        seed.bit_bytes[3] = u8(next());
        seed.body_actor->position = Vec3{next_float(), next_float(), next_float()};
        seed.body_actor->radius_y = next_float();
        seed.body_actor->radius_xz = next_float();
        seed.case_present = next() != 0;
        seed.case_actor->model = next() != 0 ? &model : nullptr;
        alternate_word = next();
        primary_word = next();
        seed.resource.resource = seed.resource_bytes;
        g_resources[seed.resource_bytes.data()] = index == 0 ? "r0" : "r1";
    }
    model.alternate_shape_pose = alternate_word != 0 ? &alternate_pose : nullptr;
    model.primary_shape_pose = primary_word != 0 ? &primary_pose : nullptr;
    // Globals.
    std::array<u8, 8> challenge{};
    for (int index = 0; index < 4; ++index)
    {
        challenge[index] = u8(next());
    }
    const u8 scene_flags = u8(next());
    const u32 runtime_flags = next();
    u32 random = next();
    const float freeze = next_float();
    const float dt = next_float();
    const float scale = next_float();
    u32 references[2];
    references[0] = next();
    references[1] = next();
    std::array<u8, 4> decay_enabled{};
    decay_enabled[0] = u8(next());
    u8 story_byte = u8(next());
    u32 intro_pending = next();
    Vec3 origin{next_float(), next_float(), next_float()};
    entity_detail::DisplayView view{};
    view.position = Vec3{next_float(), next_float(), next_float()};
    for (int plane = 0; plane < 6; ++plane)
    {
        for (int axis = 0; axis < 4; ++axis)
        {
            view.frustum[18 + plane * 4 + axis] = next_float();
        }
    }
    entity_detail::DisplayView* view_pointer = &view;
    entity_detail::ListHead emitters{};
    entity_detail::ListHead tail{};
    if (next() != 0)
    {
        emitters.next = &tail;
        tail.prev = &emitters;
    }
    std::vector<u8> parameters(0x3C4);
    put(parameters, 0x334, next());
    put(parameters, 0x338, next());
    put(parameters, 0x33C, next());
    put(parameters, 0x3C0, next());
    if (next() != 0)
    {
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x340), "aid_tex_spark");
    }
    else
    {
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x340), "none");
    }
    // Message, floats and extras.
    const u32 ident = next();
    const u32 event_kind = next();
    std::array<u32, 3> landing{next(), next(), next()};
    std::array<u32, 4> floats{next(), next(), next(), next()};
    std::array<u8, 0x10> event_bytes{};
    put(event_bytes, 0xC, event_kind);
    std::array<u32, 4> message{};
    message[0] = ident;
    if (ident == 0x96)
    {
        message[1] = landing[0];
        message[2] = landing[1];
        message[3] = landing[2];
    }
    else
    {
        message[3] = 0x10009900;
    }

    // Contexts.
    static u32 block_decay = 0;
    static u32 decay_index = 0;
    const RuntimeActorClock clock{freeze, dt, scale, block_decay, decay_enabled, decay_index};
    alignas(64) static u8 registry_storage[4096];
    AnimationResourceRegistry& registry = *reinterpret_cast<AnimationResourceRegistry*>(registry_storage);
    ResourceErrorState error{};
    std::array<u8, 4> own_resource_bytes{};
    RuntimeActorResourceReference resource{};
    resource.resource = own_resource_bytes;
    g_resources[own_resource_bytes.data()] = "resource";
    Vec3 default_euler{};
    ActorEffectStreamContext effects{unused<ActorEffectModelContext>(), unused<RuntimeActorSoundQueues>(), random,
        origin, default_euler};
    ActorAttachmentList attachments{};
    ActorAttachmentNode pending{};
    ObjectFactoryRegistry factory;
    std::vector<FactoryObject> objects;
    const std::unordered_map<const u8*, const u8*> linked_records;
    ActorAttachmentEventContext callouts{*actor, attachments, effects, factory, {}, objects, kEmitter, linked_records,
        {}};
    void* watched = nullptr;
    ModelNodeFlagCache node_cache{};
    const AnimationMatrix reset{};
    entity_detail::ListHead model_anchors{};
    entity_detail::ListHead live_attachments{};
    ActorBaseContext base{unused<ActorEventContext>(), unused<ActorRenderRegistration>(), callouts, model_anchors,
        live_attachments, unused<ActorLink>(), watched, unused<ModelAnimationState>(), nullptr, node_cache, reset};
    std::vector<std::unique_ptr<ActorAttachmentEventContext>> player_callouts;
    std::vector<std::unique_ptr<RuntimeFxEmitterPlayer>> players;
    std::vector<RuntimeFxEmitterPlayer*> player_list;
    for (int index = 0; index < 2; ++index)
    {
        PlayerSeed& seed = seeds[index];
        player_callouts.push_back(std::make_unique<ActorAttachmentEventContext>(ActorAttachmentEventContext{*seed.actor,
            attachments, effects, factory, {}, objects, 0x10003000u + 0x3000u * index, linked_records, {}}));
        players.push_back(std::make_unique<RuntimeFxEmitterPlayer>(RuntimeFxEmitterPlayer{*seed.actor,
            seed.actor_bytes, seed.body, *seed.body_actor, seed.case_present ? seed.case_actor.get() : nullptr,
            seed.resource, *player_callouts.back()}));
        if (u32(index) < player_count)
        {
            player_list.push_back(players.back().get());
        }
    }
    ActorAttachmentNode* first_attachment = nullptr;
    ActorAttachmentNode* second_attachment = nullptr;
    RuntimeBodyAttachmentLists lists{first_attachment, second_attachment, {}};
    const u32 resume_index = 0;
    const RuntimePlayerStrategyResume resume{std::span<const u32, 2>(references), decay_enabled, resume_index};
    std::array<char, 0x40> intro_name{};
    // DAT_00511B3C's next is DAT_00511B40, the same player list the emitter scans.
    entity_detail::ListHead cases{};
    entity_detail::ListHead case_node{};
    if (player_count != 0)
    {
        cases.next = &case_node;
    }
    seeds[0].bits.bytes = seeds[0].bit_bytes.data();
    PlayerSeed* first_seed = &seeds[0];
    const std::function<const EntityBitBlock*(const entity_detail::ListHead*)> case_body_bits =
        [first_seed](const entity_detail::ListHead*) -> const EntityBitBlock*
    {
        return &first_seed->bits;
    };
    RuntimeIntroNotices intro{runtime_flags, scene_flags, intro_pending, intro_name, cases, case_body_bits};
    entity_detail::ListHead link{};
    static AnimationRecord record{};
    g_record = &record;
    RuntimeFxAnimEmitter context{bytes, *actor, kEmitter, base,
        [](ActorBaseContext&, std::span<u32> words)
        {
            logf("base %x", words[0]);
        },
        [&event_bytes](u32) -> std::span<const u8>
        {
            return event_bytes;
        },
        animation, unused<ActorFrameState>(), unused<ActorFrameContext>(), matrices, attachments, pending, anchors,
        resource, lists, unused<RuntimeActorSoundQueues>(), registry, error, random, sprite, link, emitters, clock,
        view_pointer, challenge, scene_flags, runtime_flags, player_list, resume, story_byte, intro, factory,
        [](FactoryObject) -> FactoryObject*
        {
            return nullptr;
        },
        "default",
        [](u32 id, RuntimeFxEmitterPlayer& player, u32 kind, float amount, u32 extra)
        {
            logf("award %x %s %x %08x %x", id, who(&player.actor), kind, bits_of(amount), extra);
        }};
    // The frame state's actor is what the frame stub reports.
    ActorFrameState frame{*actor, animation, unused<ModelAnimationState>(), attachments, pending, nullptr, anchors, 0,
        {}, {}, 0.f, 0};
    RuntimeFxAnimEmitter& live = context;
    RuntimeFxAnimEmitter adjusted{live.bytes, live.actor, live.address, live.base, live.base_message, live.memory_at,
        live.animation, frame, live.frame_context, live.matrices, live.attachments, live.pending, live.anchors,
        live.resource, live.attachment_lists, live.sounds, live.registry, live.error, live.random, live.sprite,
        live.link, live.emitters, live.clock, live.view, live.challenge_bits, live.scene_flags, live.runtime_flags,
        live.players, live.resume, live.story_byte, live.intro, live.factory, live.keep, live.default_name, live.award};

    std::string out = "-";
    char buffer[512];
    switch (entry)
    {
    case 0:
    {
        ObjectSizes sizes{};
        fx_anim_emitter_object_sizes(sizes);
        std::snprintf(buffer, sizeof(buffer), "%x %x", sizes.instance, sizes.parameters);
        out = buffer;
        break;
    }
    case 1:
    {
        const bool empty = next() != 0;
        out = initialize_runtime_fx_anim_emitter(empty ? std::span<const u8>() : std::span<const u8>(parameters),
            adjusted) ? "1" : "0";
        break;
    }
    case 2:
        destroy_runtime_fx_anim_emitter(adjusted);
        break;
    case 3:
        dispatch_runtime_fx_anim_emitter_message(message, adjusted);
        break;
    case 4:
        launch_runtime_fx_anim_emitter(std::span<const u32, 3>(message.data() + 1, 3), adjusted);
        break;
    case 5:
        update_runtime_fx_emitter_flight(adjusted);
        break;
    case 6:
        update_runtime_fx_anim_emitter(adjusted);
        break;
    case 7:
        scan_runtime_fx_emitter_players(adjusted);
        break;
    case 8:
    {
        EntityCylinderBound bounds{};
        bounds.x = float_of(floats[0]);
        bounds.y = float_of(floats[1]);
        bounds.z = float_of(floats[2]);
        bounds.height = float_of(floats[3]);
        bounds.radius = std::fabs(float_of(floats[3]));
        const u32 which = next();
        out = std::to_string(collect_runtime_fx_anim_emitter(bounds, *players[which], adjusted));
        break;
    }
    case 9:
    {
        const u32 pass = next();
        const u32 slot = next();
        out = runtime_fx_anim_emitter_visible(pass, slot, adjusted) ? "1" : "0";
        break;
    }
    case 10:
    {
        const i32 kind = i32(next());
        RuntimeFxEmitterDraw draw{unused<ActorDrawState>(), unused<ActorFrameDrawContext>(),
            unused<SceneModelDrawContext>(), unused<BillboardQueue>()};
        out = std::to_string(draw_runtime_fx_anim_emitter(kind, draw, adjusted));
        break;
    }
    case 11:
    {
        std::array<u8, 0x354> burst{};
        build_runtime_emitter_burst_parameters(burst, *actor, kEmitter, float_of(floats[0]), "default");
        std::snprintf(buffer, sizeof(buffer), "%08x %08x %08x %08x %08x %08x %08x %08x %08x %08x %08x",
            read_u32(burst, 0), read_u32(burst, 4), read_u32(burst, 0x24), read_u32(burst, 0x334),
            read_u32(burst, 0x338), read_u32(burst, 0x33C), read_u32(burst, 0x340), read_u32(burst, 0x344),
            read_u32(burst, 0x348), read_u32(burst, 0x34C), read_u32(burst, 0x350));
        out = buffer;
        break;
    }
    case 12:
        std::snprintf(buffer, sizeof(buffer), "%08x", bits_of(runtime_actor_view_radius(*actor, bytes)));
        out = buffer;
        break;
    case 13:
        std::snprintf(buffer, sizeof(buffer), "%08x", bits_of(runtime_actor_matrix_reach(*actor, matrices)));
        out = buffer;
        break;
    case 14:
        out = runtime_actor_sound_playing(*actor, unused<RuntimeActorSoundQueues>()) ? "1" : "0";
        break;
    case 15:
    {
        const u32 mask = next();
        const u8 kind = u8(next());
        const i16 id = i16(next());
        const u8* found = find_runtime_actor_shape(*actor, mask, kind, id);
        std::string sequence;
        int guard = 0;
        while (found != nullptr && guard < 8)
        {
            std::snprintf(buffer, sizeof(buffer), "%x", u32(found - g_block4));
            sequence += (sequence.empty() ? "" : " ") + std::string(buffer);
            found = next_runtime_actor_shape(*actor, found, mask, kind, id);
            ++guard;
        }
        out = sequence.empty() ? "-" : sequence;
        break;
    }
    case 16:
    {
        EntityCylinderBound first{};
        EntityCylinderBound second{};
        for (EntityCylinderBound* bound : {&first, &second})
        {
            bound->x = next_float();
            bound->y = next_float();
            bound->z = next_float();
            bound->height = next_float();
            bound->radius = next_float();
        }
        out = runtime_cylinders_apart(first, second) ? "1" : "0";
        break;
    }
    case 18:
        out = std::to_string(queue_runtime_intro_notice("intros_test", intro));
        break;
    default:
    {
        u8 plane = u8(next());
        const u32 override = next();
        const u32 poisoned = next();
        if (override != 0)
        {
            for (int index = 0; index < 6; ++index)
            {
                view.frustum[18 + index * 4 + 3] = 1000.f;
            }
            view.frustum[18 + poisoned * 4] = float_of(0x7FC00000u);
        }
        const Vec3 center{float(double(float_of(floats[1])) * 10.0), float(double(float_of(floats[2])) * 10.0),
            float(double(float_of(floats[3])) * 10.0)};
        const u32 result = classify_runtime_sphere_frustum(view.frustum, center, float_of(floats[0] & 0x7FFFFFFFu),
            plane);
        std::snprintf(buffer, sizeof(buffer), "%u %u", result, plane);
        out = buffer;
        break;
    }
    }
    // Snapshot.
    std::string snapshot;
    for (size_t offset = 0x93C; offset < 0x9B0; offset += 4)
    {
        if (offset == 0x994)
        {
            continue;
        }
        snapshot += std::to_string(read_u32(bytes, offset)) + " ";
    }
    snapshot += std::to_string(sprite.data() != nullptr ? 1 : 0) + " ";
    for (float value : {actor->anchor.x, actor->anchor.y, actor->anchor.z, actor->position.x, actor->position.y,
             actor->position.z})
    {
        snapshot += std::to_string(bits_of(value)) + " ";
    }
    snapshot += std::to_string(animation.controller != nullptr ? 1 : 0) + " " + std::to_string(bits_of(matrices.progress))
        + " " + std::to_string(matrices.flags) + " " + std::to_string(random) + " ";
    for (int index = 0; index < 8; ++index)
    {
        snapshot += std::to_string(bytes[0x184 + index]) + " ";
    }
    snapshot += std::to_string(story_byte) + " " + std::to_string(intro_pending) + " "
        + (intro_name[0] != 0 ? std::string(intro_name.data()) : std::string("-")) + " ";
    const char* head = emitters.next == nullptr ? "n" : emitters.next == &link ? "e" : emitters.next == &tail ? "t" : "?";
    const char* after = tail.next == nullptr ? "n" : tail.next == &link ? "e" : "?";
    snapshot += std::string(head) + " " + after;
    out += " | " + snapshot + " |";
    for (size_t index = 0; index < g_log.size(); ++index)
    {
        out += (index == 0 ? " " : " ; ") + g_log[index];
    }
    std::printf("%s\n", out.c_str());
    std::fflush(stdout);
}
}

int main()
{
    int cases = 0;
    std::cin >> cases;
    for (int index = 0; index < cases; ++index)
    {
        try
        {
            run_case();
        }
        catch (const std::exception& error)
        {
            std::fprintf(stderr, "case %d: %s\n", index, error.what());
            return 1;
        }
    }
}
