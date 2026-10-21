#include "fx_custom_electric.h"
#include "actor_runtime_queries.h"
#include "actor_scare_requests.h"
#include "actor_transform.h"
#include "shape_transform.h"
#include "../scene/scene_shadow_casters.h"
#include "../frontend/dialogue_draw_context.h"
#include "gbtg/anim.h"

#include <array>
#include <bit>
#include <xmmintrin.h>
#include <cstring>
#include <memory>
#include <vector>

namespace gbtg
{
namespace
{
// The host halves of an effect: its node on the actor's frame list, and for the electric effect the texture, the
// {shape, frame} rows (+0xA0) and the light record (+0xA4's slot).
struct FxCustomInstance
{
    ActorFrameAttachment node{};
    u8* bytes = nullptr; // The object's bytes, which stay put while the scene moves its FactoryObject.
    u16 type = 0;
    Actor* actor = nullptr;
    std::span<const u8> texture{};
    struct Spark
    {
        const u8* shape;
        i32 frame;
    };
    std::vector<Spark> sparks;
    std::unique_ptr<std::array<u8, 0x98>> light;
};
std::unordered_map<const u8*, std::unique_ptr<FxCustomInstance>> fx_instances;
std::unordered_map<const ActorFrameAttachment*, FxCustomInstance*> fx_nodes;
RuntimeFxCustomContext* fx_context = nullptr;

u32& deferred(RuntimeDialogueDrawContext& c, u32 address)
{
    return c.deferred[(address - 0x181968) / 4];
}

std::span<u8> bytes_of(FactoryObject& object)
{
    return {object.bytes.get(), object.size};
}

FxCustomInstance& instance_of(FactoryObject& object)
{
    return *fx_instances.at(object.bytes.get());
}

void put_word(u8* bytes, size_t offset, u32 value)
{
    std::memcpy(bytes + offset, &value, 4);
}

void put_float(u8* bytes, size_t offset, float value)
{
    std::memcpy(bytes + offset, &value, 4);
}

float frame_step(const RuntimeFxCustomContext& context)
{
    if (context.pause > 0.f)
    {
        return 0.f;
    }
    return context.frame_delta * context.time_scale;
}

u32 next_random(RuntimeFxCustomContext& context)
{
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    return context.random;
}

float random_fraction(RuntimeFxCustomContext& context)
{
    return std::bit_cast<float>((next_random(context) & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
}

// The light's position: the actor's world position, half its model height up.
Vec3 light_position(const Actor& actor)
{
    Vec3 position = actor_world_position(actor); // 0x000412A0
    position.y = float(actor_model_height(actor) * 0.5L + position.y); // 0x0003DB90, DAT_0032B0F4
    return position;
}

// Xbox 0x00051BC0, EDX the record
void initialize_runtime_light(u8* record)
{
    std::memset(record, 0, 0x64);
    record[0x64] = 0;
    std::memset(record + 0x66, 0, 2);
    record[0x68] = 0xFF;
    put_word(record, 0x6C, 0);
    put_word(record, 0x70, 0);
    put_word(record, 0x74, 0);
    put_word(record, 0x78, 0xFFFFFFFFu); // DAT_00314F48.
    put_word(record, 0x7C, 0xFF000000u); // DAT_00314F4C.
    put_word(record, 0x80, 0xFFFFFFFFu);
    put_word(record, 0x84, 0xFFFFFFFFu);
    std::memset(record + 0x88, 0, 0x10);
}

// Xbox 0x000EC710: the base update counts the effect's seconds.
void update_fx_custom(FxCustomInstance& instance)
{
    u8* bytes = instance.bytes;
    float time;
    std::memcpy(&time, bytes + 0x10, 4);
    put_float(bytes, 0x10, time + frame_step(*fx_context));
}

// Xbox 0x00076720: the light follows the actor and each spark may jump to another frame.
void update_fx_custom_electric(FxCustomInstance& instance)
{
    RuntimeFxCustomContext& context = *fx_context;
    u8* bytes = instance.bytes;
    float time;
    std::memcpy(&time, bytes + 0x10, 4);
    put_float(bytes, 0x10, time + frame_step(context));
    const std::int8_t slot = std::int8_t(bytes[0xA4]);
    if (slot != -1)
    {
        move_runtime_light(slot, light_position(*instance.actor), context.lights); // 0x000FE0F0
    }
    const u8 frames = instance.texture[0x11];
    for (auto& spark : instance.sparks)
    {
        const float roll = random_fraction(context);
        if (!(0.4f < roll)) // DAT_0032B11C.
        {
            spark.frame = i32(random_fraction(context) * float(frames)); // CVTTSS2SI
        }
    }
}

// Xbox 0x00076850: each spark a billboard on its shape's sphere, twice its radius across, in the opaque pass.
void draw_fx_custom_electric(FxCustomInstance& instance, Actor& actor, i32 kind)
{
    if (kind != 0)
    {
        return;
    }
    if (actor.model == nullptr || instance.sparks.empty()) // Actor +0x204.
    {
        return;
    }
    std::optional<BillboardFlushContext> pass;
    for (const auto& spark : instance.sparks)
    {
        const EntityBonePose* pose = actor.model->alternate_shape_pose; // Model +0x168, else +0x164.
        if (pose == nullptr)
        {
            pose = actor.model->primary_shape_pose;
        }
        Vec3 center;
        const float radius = shape_sphere_metrics(*reinterpret_cast<const EntityShapeNode*>(spark.shape),
            actor.xform, pose, center); // 0x00056330, actor +0x2AC
        if (!pass)
        {
            pass.emplace(fx_context->billboards());
            begin_billboard_draw(*pass); // 0x0005C410
            pass->draw.d3d.simple_state(0x40354, 0x203);
            deferred(pass->draw, 0x181C54) = 0x203;
            pass->draw.d3d.simple_state(0x4035C, 0);
            deferred(pass->draw, 0x181C70) = 0;
            pass->draw.d3d.depth(1);
        }
        const std::array<float, 2> size{radius * 2.f, radius * 2.f}; // DAT_0032B110.
        set_billboard_transform(center, 0.f, *pass); // 0x0005C580
        float width;
        float height;
        bind_billboard_sprite(instance.texture.data(), spark.frame, 0xFFFFFFFFu, width, height,
            *pass); // 0x0005C7F0, DAT_00314F48
        push_billboard_quad(size.data(), width, height, *pass); // 0x0005C8C0
    }
    if (pass && --pass->draw.override_count == 0) // DAT_00509B5C.
    {
        pass->draw.fog_disabled = 0; // DAT_00509B00.
    }
}

// The actor's +0x1D8 frame list walks call the object's vtable +0x10 and +0x14 through the node.
// Xbox 0x00078110, EAX effect: the fade starts again from its first phase with a time.
void restart_fx_solid_fade_in(u8* bytes)
{
    put_float(bytes, 0x20, 0.f);
    if (0.f < read_f32({bytes, 0x54}, 0x24))
    {
        put_word(bytes, 0x18, 0);
        put_word(bytes, 0x1C, read_u32({bytes, 0x54}, 0x24));
        return;
    }
    put_float(bytes, 0x20, 1.f); // DAT_0032B0F8.
    if (0.f < read_f32({bytes, 0x54}, 0x28))
    {
        put_word(bytes, 0x18, 1);
        put_word(bytes, 0x1C, read_u32({bytes, 0x54}, 0x28));
        return;
    }
    put_word(bytes, 0x18, 2);
    put_word(bytes, 0x1C, read_u32({bytes, 0x54}, 0x2C));
}

// Xbox 0x00077ED0: the phase's time runs down by the actor's step; the level sets the actor's tint.
void update_fx_solid_fade_in(FxCustomInstance& instance, Actor& actor)
{
    RuntimeFxCustomContext& context = *fx_context;
    u8* bytes = instance.bytes;
    const std::span<const u8> view{bytes, 0x54};
    put_float(bytes, 0x10, read_f32(view, 0x10) + frame_step(context));
    bool ended = false;
    if (0.f < read_f32(view, 0x1C))
    {
        const float rate = actor.animation_pause > 0.f ? 0.f : actor.animation_rate; // Actor +0x930, +0x92C.
        if (0.f < read_f32(view, 0x1C))
        {
            const float left = read_f32(view, 0x1C) - rate * frame_step(context);
            put_float(bytes, 0x1C, left);
            if (!(0.f < left)) // COMISS; JC keeps a positive time.
            {
                put_float(bytes, 0x1C, 0.f);
                ended = true;
            }
        }
    }
    bool release = false;
    switch (read_u32(view, 0x18))
    {
    case 0:
        put_float(bytes, 0x20, 1.f - read_f32(view, 0x1C) / read_f32(view, 0x24)); // DAT_0032B0F8.
        if (ended)
        {
            if (read_f32(view, 0x28) == 0.f)
            {
                put_word(bytes, 0x18, 2);
                put_word(bytes, 0x1C, read_u32(view, 0x2C));
            }
            else
            {
                put_word(bytes, 0x18, 1);
                put_word(bytes, 0x1C, read_u32(view, 0x28));
            }
        }
        break;
    case 1:
        if (ended || read_u32(view, 0x50) != 0)
        {
            put_word(bytes, 0x18, 2);
            put_word(bytes, 0x1C, read_u32(view, 0x2C));
        }
        break;
    case 2:
        if (ended)
        {
            if (read_f32(view, 0x30) <= 0.f)
            {
                release = true;
                put_float(bytes, 0x20, 0.f);
            }
            else
            {
                put_word(bytes, 0x18, 3);
                put_word(bytes, 0x1C, read_u32(view, 0x30));
            }
        }
        else
        {
            put_float(bytes, 0x20, read_f32(view, 0x1C) / read_f32(view, 0x2C));
        }
        break;
    case 3:
        if (ended)
        {
            restart_fx_solid_fade_in(bytes); // 0x00078110
        }
        break;
    default:
        break;
    }
    RuntimeActorStartupState& state = context.startup_of(actor);
    u32 color = 0xFFFFFFFFu; // DAT_00314F48.
    const i32 level = _mm_cvttss_si32(_mm_set_ss(read_f32(view, 0x38) * read_f32(view, 0x20) * 255.f)); // DAT_0032B158.
    if (level == 0)
    {
        state.flags[0] = 0; // Actor +0x19C..+0x19E.
        state.flags[1] = 0;
        state.flags[2] = 0;
    }
    else
    {
        state.flags[0] = u8(level);
        state.flags[1] = u8(level);
        state.flags[2] = u8(level);
        color = read_u32(view, 0x34);
    }
    state.color = color; // Actor +0x198.
    if (release)
    {
        context.release_frame_object(actor, context.address_of(bytes)); // 0x0003DE30, ECX actor, ESI effect
    }
}

void update_fx_node(ActorFrameAttachment& node, Actor& actor)
{
    FxCustomInstance& instance = *fx_nodes.at(&node);
    if (instance.type == 0x2E)
    {
        update_fx_custom_electric(instance); // 0x00076720
        return;
    }
    if (instance.type == 0x32)
    {
        update_fx_solid_fade_in(instance, actor); // 0x00077ED0
        return;
    }
    update_fx_custom(instance); // 0x000EC710
}

void draw_fx_node(ActorFrameAttachment& node, Actor& actor, i32 kind)
{
    FxCustomInstance& instance = *fx_nodes.at(&node);
    if (instance.type == 0x2E)
    {
        draw_fx_custom_electric(instance, actor, kind); // 0x00076850
    }
    // fxCustom's own +0x14 is the empty 0x001628B0.
}

// Xbox 0x000EC6A0: the effect joins the tail of its actor's frame list.
bool construct_fx_custom_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    auto& slot = fx_instances[object.bytes.get()];
    slot = std::make_unique<FxCustomInstance>();
    FxCustomInstance& instance = *slot;
    instance.bytes = object.bytes.get();
    instance.type = object.type;
    const u32 actor_address = read_u32(parameters, 8);
    put_word(object.bytes.get(), 0x14, actor_address);
    instance.actor = context.actor_at(actor_address);
    if (instance.actor == nullptr)
    {
        throw Error("0x000EC6A0 walks the frame list of an actor with no scene view");
    }
    ActorFrameState& frame = context.frame_of(*instance.actor);
    ActorFrameAttachment** tail = &frame.updates; // Actor +0x1D4's +4.
    while (*tail != nullptr)
    {
        tail = &(*tail)->next;
    }
    instance.node.next = nullptr; // +0xC.
    instance.node.update = update_fx_node;
    *tail = &instance.node;
    fx_nodes[&instance.node] = &instance;
    context.frame_objects[&instance.node] = object.bytes.get();
    context.draws[&instance.node] = draw_fx_node;
    return true;
}

// Xbox 0x000EC6E0: the effect leaves its actor's frame list.
void destroy_fx_custom_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    ActorFrameState& frame = context.frame_of(*instance.actor);
    for (ActorFrameAttachment** link = &frame.updates; *link != nullptr; link = &(*link)->next)
    {
        if (*link == &instance.node)
        {
            *link = instance.node.next; // The previous node's +4 takes this one's next.
            break;
        }
    }
    instance.node.next = nullptr;
    fx_nodes.erase(&instance.node);
    context.frame_objects.erase(&instance.node);
    context.draws.erase(&instance.node);
    fx_instances.erase(found);
}

// Xbox 0x000769D0, EAX actor, ESI effect: a {shape, random frame} row for each contact shape of kind 0.
void collect_fx_electric_sparks(FxCustomInstance& instance, u8* bytes)
{
    const u8* block = runtime_actor_model_block(*instance.actor, 4); // Actor +0x204's kind-4 block.
    if (block == nullptr)
    {
        return;
    }
    const u32 mask = (runtime_shape_list_mask(block) & 0x80) != 0 ? 0x80u : 0x20002u; // 0x00056C80
    u32 count = 0;
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if ((read_u32({record, 8}, 4) & mask) != 0 && record[0] == 0)
        {
            ++count;
        }
        if (record[1] == 0)
        {
            break;
        }
    }
    put_word(bytes, 0x9C, count);
    if (count == 0)
    {
        return;
    }
    put_word(bytes, 0xA0, 1); // 0x000F72D0(count * 8): the rows live on the host.
    const u8 frames = instance.texture[0x11];
    for (const u8* record = block + 8; record != nullptr; record = record + record[1])
    {
        if ((read_u32({record, 8}, 4) & mask) != 0 && record[0] == 0)
        {
            instance.sparks.push_back({record, i32(random_fraction(*fx_context) * float(frames))}); // CVTTSS2SI
        }
        if (record[1] == 0)
        {
            break;
        }
    }
}

// Xbox 0x00076B00, EAX actor: a kind-2 light at the actor, its radius twice and four times the actor's view radius.
void start_fx_electric_light(FxCustomInstance& instance, u8* bytes)
{
    RuntimeFxCustomContext& context = *fx_context;
    Actor& actor = *instance.actor;
    const float reach = runtime_actor_view_radius(actor, context.actor_bytes(actor)); // 0x0003D860
    instance.light = std::make_unique<std::array<u8, 0x98>>(); // 0x000F72D0(0x98)
    u8* record = instance.light->data();
    const std::int8_t slot = allocate_runtime_light(2, light_position(actor), record, context.lights); // 0x000FE080
    record[0x66] |= 0x48;
    put_float(record, 0x6C, reach * 2.f); // DAT_0032B110.
    record[0x66] |= 1;
    put_float(record, 0x70, reach * 4.f); // DAT_0032B1B0.
    put_word(record, 0x78, 0xFFFFFF00u); // DAT_00314F40.
    put_word(record, 0x80, 0xFFFFFF00u);
    put_word(record, 0x84, 0xFFC0C0C0u); // DAT_00314F94.
    put_float(record, 0x88, 2.f);
    bytes[0xA4] = u8(slot);
}

// Xbox 0x000765E0
bool construct_fx_custom_electric_view(FactoryObject& object, std::span<const u8> parameters)
{
    RuntimeFxCustomContext& context = *fx_context;
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x2E;
    u8* bytes = object.bytes.get();
    const char* texture_name = reinterpret_cast<const char*>(parameters.data() + 0x8C);
    u32 prefix;
    std::memcpy(&prefix, texture_name, 4);
    if (prefix == 0x5F646961) // "aid_".
    {
        instance.texture = acquire_animation_resource(context.registry, texture_name); // 0x00035E90
        if (instance.texture.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
    }
    put_word(bytes, 0x98, instance.texture.data() != nullptr ? 1u : 0u);
    bytes[0xA4] = 0xFF;
    if (instance.texture.data() == nullptr)
    {
        return false;
    }
    collect_fx_electric_sparks(instance, bytes); // 0x000769D0
    if (read_u32(bytes_of(object), 0x9C) == 0)
    {
        return false;
    }
    start_fx_electric_light(instance, bytes); // 0x00076B00
    const char* sound = reinterpret_cast<const char*>(parameters.data() + 0xC);
    char* name = reinterpret_cast<char*>(bytes + 0x18);
    size_t index = 0;
    do
    {
        name[index] = sound[index];
    }
    while (sound[index++] != 0);
    if (name[0] != 0)
    {
        request_actor_sound(*instance.actor, 4, name, context.sounds); // 0x00041D00
    }
    return true;
}

// Xbox 0x00076690
void destroy_fx_custom_electric_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    FxCustomInstance& instance = *found->second;
    u8* bytes = object.bytes.get();
    if (read_u32(bytes_of(object), 0x98) != 0)
    {
        release_animation_resource(context.registry, instance.texture); // 0x00035C00
        instance.texture = {};
        put_word(bytes, 0x98, 0);
    }
    if (read_u32(bytes_of(object), 0xA0) != 0)
    {
        instance.sparks.clear(); // 0x000663E0
        put_word(bytes, 0xA0, 0);
    }
    const std::int8_t slot = std::int8_t(bytes[0xA4]);
    if (slot != -1)
    {
        u8*& record = context.lights.records.at(size_t(slot));
        // The host keeps the record in the instance; the scene's light reset (0x000FDED0) may have emptied the slot.
        if (record != nullptr && (record[0x66] & 0x10) == 0)
        {
            instance.light.reset(); // 0x000663E0
        }
        record = nullptr;
        --context.lights.count;
        bytes[0xA4] = 0xFF;
    }
    if (bytes[0x18] != 0)
    {
        release_actor_sound(instance.actor->sound_slots[4], context.sounds); // 0x0012A3D0, actor +0x614
    }
}

const ObjectFactoryVtable* fx_custom_lifecycle()
{
    static const ObjectFactoryVtable table{0x005068F4, fx_custom_object_sizes, construct_fx_custom_view,
        destroy_fx_custom_view};
    return &table;
}

const ObjectFactoryVtable* fx_custom_electric_lifecycle()
{
    static const ObjectFactoryVtable table{0x005053E0, fx_custom_electric_object_sizes,
        construct_fx_custom_electric_view, destroy_fx_custom_electric_view};
    return &table;
}

// Xbox 0x00077E00
bool construct_fx_solid_fade_in_view(FactoryObject& object, std::span<const u8> parameters)
{
    FxCustomInstance& instance = instance_of(object);
    instance.type = 0x32;
    u8* bytes = object.bytes.get();
    put_word(bytes, 0x24, read_u32(parameters, 0x14));
    put_word(bytes, 0x28, read_u32(parameters, 0x18));
    put_word(bytes, 0x2C, read_u32(parameters, 0x1C));
    put_word(bytes, 0x30, read_u32(parameters, 0x20));
    put_word(bytes, 0x34, read_u32(parameters, 0xC));
    put_word(bytes, 0x38, read_u32(parameters, 0x10));
    restart_fx_solid_fade_in(bytes); // The inlined 0x00078110.
    return true;
}

// Xbox 0x00077E90: the actor's tint and its +0x1A0..+0x1A8 draw callbacks back to none.
void destroy_fx_solid_fade_in_view(FactoryObject& object)
{
    RuntimeFxCustomContext& context = *fx_context;
    const auto found = fx_instances.find(object.bytes.get());
    if (found == fx_instances.end())
    {
        return;
    }
    Actor& actor = *found->second->actor; // Effect +0x14.
    const std::span<u8> bytes = context.object_bytes(actor);
    put_word(bytes.data(), 0x1A0, 0);
    put_word(bytes.data(), 0x1A4, 0);
    put_word(bytes.data(), 0x1A8, 0);
    RuntimeActorStartupState& state = context.startup_of(actor);
    state.color = 0xFFFFFFFFu; // DAT_00314F48.
    state.flags[0] = 0;
    state.flags[1] = 0;
    state.flags[2] = 0;
}

const ObjectFactoryVtable* fx_solid_fade_in_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505440, fx_custom_solid_fade_in_object_sizes,
        construct_fx_solid_fade_in_view, destroy_fx_solid_fade_in_view};
    return &table;
}
}

// Xbox 0x00077D90
void fx_custom_solid_fade_in_object_sizes(ObjectSizes& output)
{
    output.instance = 0x54;
    output.parameters = 0x24;
}

// Xbox 0x000780C0
void message_fx_custom_solid_fade_in(std::span<u8> effect, std::span<const u32> message)
{
    if (message[0] == 3)
    {
        const i32 phase = i32(read_u32(effect, 0x18));
        if (0 < phase && phase < 3)
        {
            put_word(effect.data(), 0x18, 1);
            put_word(effect.data(), 0x1C, read_u32(effect, 0x28));
            put_float(effect.data(), 0x20, 1.f); // DAT_0032B0F8.
            put_word(effect.data(), 0x50, 0);
        }
    }
    else if (message[0] == 5)
    {
        put_word(effect.data(), 0x50, 1);
    }
}

// Xbox 0x00095530
void fx_custom_object_sizes(ObjectSizes& output)
{
    output.instance = 0x18;
    output.parameters = 0xC;
}

// Xbox 0x00076570
void fx_custom_electric_object_sizes(ObjectSizes& output)
{
    output.instance = 0xA8;
    output.parameters = 0x10C;
}

// Xbox 0x000FE080, ESI position
std::int8_t allocate_runtime_light(u8 kind, const Vec3& position, u8* record, RuntimeShaderRegistrationState& lights)
{
    std::int8_t slot = -1;
    for (std::int8_t index = 0; index < 0x28; ++index)
    {
        if (lights.records[size_t(index)] == nullptr)
        {
            slot = index;
            break;
        }
    }
    if (slot == -1)
    {
        throw Error("0x000FE080 stores a light record through DAT_00509B7C[-1]: all 40 slots are taken");
    }
    initialize_runtime_light(record); // 0x00051BC0
    record[0x64] = kind;
    if (kind == 2 || kind == 3)
    {
        record[0x66] |= 0xA;
    }
    std::memcpy(record + 0x40, &position, 12);
    lights.records[size_t(slot)] = record;
    ++lights.count;
    return slot;
}

// Xbox 0x000FE0F0, AL slot, ECX position
void move_runtime_light(std::int8_t slot, const Vec3& position, RuntimeShaderRegistrationState& lights)
{
    u8* record = lights.records.at(size_t(slot));
    if (record == nullptr)
    {
        throw Error("0x000FE0F0 moves an empty light slot");
    }
    std::memcpy(record + 0x40, &position, 12);
    if (record[0x64] != 2)
    {
        return;
    }
    const Vec3 toward{0.f - position.x, 0.f - position.y, 0.f - position.z};
    Vec3 direction;
    runtime_direction_or_axis(toward, direction, 0x79); // 0x00106B90, EDX record +0x4C
    std::memcpy(record + 0x4C, &direction, 12);
}

// Xbox 0x00056C80, EDX the list
u32 runtime_shape_list_mask(const u8* list)
{
    i32 count;
    std::memcpy(&count, list, 4);
    u32 mask = 0;
    const u8* record = list + 8;
    for (i32 index = 0; index < count; ++index)
    {
        if (record == nullptr)
        {
            throw Error("0x00056C80 reads past a shape list's last record");
        }
        mask |= read_u32({record, 8}, 4);
        record = record[1] != 0 ? record + record[1] : nullptr;
    }
    return mask;
}

void install_runtime_fx_custom_classes(std::span<ObjectFactoryType> types, RuntimeFxCustomContext& context)
{
    fx_context = &context;
    types[0xC] = ObjectFactoryType{1, fx_custom_lifecycle}; // Getter 0x000EC650.
    types[0x2E] = ObjectFactoryType{0xC, fx_custom_electric_lifecycle}; // Getter 0x00076590.
    types[0x32] = ObjectFactoryType{0xC, fx_solid_fade_in_lifecycle}; // Getter 0x00077DB0.
}
}
