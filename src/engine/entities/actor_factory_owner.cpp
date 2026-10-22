#include "actor_factory_owner.h"
#include "actor_collision_queries.h"
#include "physical_mesh_queries.h"
#include "entity_heap.h"
#include "actor_shape_push.h"
#include "entity_collision.h"
#include "contact_prop_actor.h"
#include "../graphics/vector_normalize.h"
#include <cstring>
#include "gbtg/model_runtime.h"
#include <bit>

namespace gbtg
{
namespace
{
NativeActorFactoryOwner* active_owner = nullptr;

// Xbox 0x001628B0
void destroy_empty_factory_object(FactoryObject&)
{
}

// Native address and pointer-width boundary for the retail actor collision slots.
u32 actor_query(entity_detail::SegmentProbe& probe, Vec3& candidate, Vec3& kept,
    float radius, i32 mode, u32 mask, u8* record, u32 slot, GhoulySpawnPool** pool = nullptr, u32 filter = 0)
{
    auto& instance = active_owner->view(probe);
    auto& actor = instance.bat.actor;
    const auto cpu = actor.model != nullptr ? actor.model->cpu : std::span<const u8>{};
    if (actor.model != nullptr && actor.model->runtime != nullptr && !active_owner->bindings.resource_address)
    {
        throw Error("actor collision resource address callback is absent");
    }
    const u32 base = actor.model != nullptr && actor.model->runtime != nullptr
        ? active_owner->bindings.resource_address(actor.model->runtime->source->cpu.data()) : 0;
    const auto resolve = [cpu, base](u32 address, size_t size)
    {
        const size_t offset = address >= base && base != 0 ? address - base : address;
        if (offset > cpu.size() || size > cpu.size() - offset)
        {
            throw Error("actor collision address is outside its model CPU resource");
        }
        return cpu.subspan(offset, size);
    };
    std::unordered_map<u32, std::function<u32(RuntimeCollisionShapeCall&)>> methods;
    bind_runtime_physical_mesh_methods(methods, resolve);
    std::array<u32, 21> words{};
    if (actor.model != nullptr && actor.model->runtime != nullptr)
    {
        const auto& model = *actor.model->runtime;
        words[0] = model.alternate != nullptr ? active_owner->bindings.pose_address(model.alternate) : 0;
        std::copy(model.shader_indices.begin(), model.shader_indices.end(), words.begin() + 1);
    }
    const Vec3 stationary{}; // DAT_0050DD80.
    RuntimeCollisionShapeDispatch dispatch{RuntimeCollisionShapeCall{nullptr, 0, nullptr, nullptr,
        nullptr, 0, candidate, kept, radius, mask, mode,
        record != nullptr ? std::span<u8>(record, 0x54) : std::span<u8>{}, pool, stationary},
        words.data() + 1, {}, [cpu, base](const void* pointer)
        {
            return base + u32(static_cast<const u8*>(pointer) - cpu.data());
        }, methods};
    RuntimeActorCollision context{actor, instance.bytes, dispatch};
    switch (slot)
    {
    case 0x3DF70:
        return query_runtime_actor_physical_segment(context);
    case 0x3E110:
        return query_runtime_actor_physical_sweep(context);
    case 0x3E200:
        return query_runtime_actor_model_sweep(context);
    case 0x3E3C0:
        return collect_runtime_actor_model_contacts(context);
    case 0x3E050:
        return collect_runtime_actor_physical_contacts(context, filter);
    default:
        throw Error("unbound actor collision query address");
    }
}
// The list walkers (0x00122480 and its siblings) push the caller's two words after the radius in order;
// 0x0003E110 and 0x0003DF70 AND the first with actor +0x1F0 as the shape mask and pass the second on as the
// excluded-flag mode.
i32 actor_segment_query(entity_detail::SegmentProbe& probe, Vec3& candidate, Vec3& kept,
    u32 radius, i32 mask, u32 mode, u8* record)
{
    return i32(actor_query(probe, candidate, kept, std::bit_cast<float>(radius), i32(mode), u32(mask), record,
        0x3E110));
}
i32 actor_segment_sweep(entity_detail::SegmentProbe& probe, Vec3& candidate, Vec3& kept,
    i32 mask, u32 mode, u8* record)
{
    return i32(actor_query(probe, candidate, kept, 0.f, i32(mode), u32(mask), record, 0x3DF70));
}
i32 actor_segment_point(entity_detail::SegmentProbe& probe, Vec3& candidate, Vec3& kept,
    float radius, u32 mask, u8* record)
{
    return i32(actor_query(probe, candidate, kept, radius, 0, mask, record, 0x3E200));
}
// SegmentPool stores a 32-bit retail header separately from its native pointer view.
// Copy the complete capacity through the native pool boundary, retaining untouched bytes.
i32 actor_collect(entity_detail::SegmentProbe& probe, Vec3& origin, float radius,
    i32 mode, u32 mask, entity_detail::SegmentPool& target, u32 slot)
{
    const auto capacity = u16((target.limit - target.begin) / target.stride);
    GhoulySpawnPool* pool = allocate_runtime_shape_pool(target.stride, capacity);
    pool->grow = target.grow;
    std::memcpy(pool->begin, target.begin, size_t(target.limit - target.begin));
    pool->cursor = pool->begin + (target.cursor - target.begin);
    Vec3 kept = origin;
    try
    {
        const u32 result = actor_query(probe, origin, kept, radius, mode, mask, nullptr,
            slot, &pool, u32(mode));
        const size_t bytes = size_t(pool->limit - pool->begin);
        target.block = static_cast<u8*>(engine_heap_reallocate(target.block, i32(bytes + 0x10)));
        target.block_size = i32(bytes + 0x10);
        target.stride = pool->stride;
        target.grow = pool->grow;
        std::memcpy(target.block, &target.stride, 2);
        std::memcpy(target.block + 2, &target.grow, 2);
        target.begin = target.block + 0x10;
        target.cursor = target.begin + (pool->cursor - pool->begin);
        target.limit = target.begin + bytes;
        std::memcpy(target.begin, pool->begin, bytes);
        engine_heap_free(pool);
        return i32(result);
    }
    catch (...)
    {
        engine_heap_free(pool);
        throw;
    }
}
i32 actor_segment_contacts(entity_detail::SegmentProbe& probe, Vec3& origin, float radius,
    u32 mask, entity_detail::SegmentPool& pool)
{
    return actor_collect(probe, origin, radius, 0, mask, pool, 0x3E3C0);
}
i32 actor_segment_sphere_contacts(entity_detail::SegmentProbe& probe, Vec3& origin, float radius,
    i32 mode, u32 mask, entity_detail::SegmentPool& pool)
{
    return actor_collect(probe, origin, radius, mode, mask, pool, 0x3E050);
}
i32 actor_shape_push(entity_detail::ShapeProbe& probe, const Actor* other, const Vec3* query,
    EntityCylinderBound& merged, EntityCylinderBound* shapes, i32 count, Vec3& output)
{
    auto& instance = active_owner->view(probe);
    Actor& actor = instance.bat.actor;
    std::array<u32, 21> words{};
    if (actor.model != nullptr && actor.model->runtime != nullptr)
    {
        const auto& model = *actor.model->runtime;
        words[0] = model.alternate != nullptr ? active_owner->bindings.pose_address(model.alternate) : 0;
        std::copy(model.shader_indices.begin(), model.shader_indices.end(), words.begin() + 1);
    }
    RuntimeActorShapePush context{actor, words.data() + 1,
        entity_detail::g_contact_points[entity_detail::g_contact_slot], normalize_runtime_graphics_vector,
        [&actor](const Actor& other_actor, const Vec3& push, const Vec3& contact, u16 bone)
        {
            active_owner->bindings.shape_message(actor, other_actor, push, contact, bone);
        }};
    return i32(push_runtime_actor_shapes(other, query, merged, shapes, count, output, context));
}

// Native lifecycle slot adapters. The translated constructors/destructor keep
// their retail branches; these associate Xbox instance bytes with host fields.
bool construct_actor_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& instance = active_owner->construct_view(object);
    const bool result = initialize_runtime_actor(instance.startup, parameters,
        active_owner->bindings.startup);
    std::memcpy(object.bytes.get() + 0x208, instance.startup.models.name.data(), instance.startup.models.name.size());
    // 0x0003E940 stores parameters +0x2AA / +0x2AC at actor +0x1FC / +0x1FE; the instance bytes carry them for the
    // readers that take the retail words (triggers, strategies, weapons).
    std::memcpy(object.bytes.get() + 0x1FC, &instance.startup.setting_1fc, 2);
    std::memcpy(object.bytes.get() + 0x1FE, &instance.startup.setting_1fe, 2);
    instance.frame.bone = instance.startup.bone;
    refresh_actor_model_animation_view(instance);
    return result;
}

bool construct_button_bat_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& instance = active_owner->view(object);
    auto& context = active_owner->bindings.startup;
    button_bat_initialize(instance.bat,
        ButtonBatPoint{read_f32(parameters, 0x334), read_f32(parameters, 0x338)},
        read_f32(parameters, 0x33C), instance.animation, context.registry,
        context.error, context.random_state);
    return true; // 0x0006D5B1 MOV EAX,1, immediately before the constructor RET.
}

void destroy_actor_view(FactoryObject& object)
{
    active_owner->release_view(object);
}

bool construct_contact_prop_view(FactoryObject& object, std::span<const u8>)
{
    return initialize_runtime_contact_prop({object.bytes.get(), object.size}, active_owner->view(object).bat.actor);
}

void destroy_contact_prop_view(FactoryObject& object)
{
    release_runtime_contact_prop({object.bytes.get(), object.size}, active_owner->view(object).prop_contacts);
}

const ObjectFactoryVtable* contact_prop_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504DD8, runtime_contact_prop_sizes,
        construct_contact_prop_view, destroy_contact_prop_view};
    return &table;
}

bool construct_knockdown_sphere_view(FactoryObject& object, std::span<const u8> parameters)
{
    return initialize_runtime_knockdown_sphere({object.bytes.get(), object.size}, parameters); // 0x0006EB80
}

// Getter 0x0006EB40 (vtable 0x00504D60): +8 is the contact prop's 0x0006EEA0.
const ObjectFactoryVtable* knockdown_sphere_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504D60, runtime_knockdown_sphere_sizes,
        construct_knockdown_sphere_view, destroy_contact_prop_view};
    return &table;
}

const ObjectFactoryVtable* base_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050E0C0, base_object_sizes,
        initialize_base_object, destroy_empty_factory_object};
    return &table;
}

const ObjectFactoryVtable* actor_lifecycle()
{
    static const ObjectFactoryVtable table{0x004E1608, actor_object_sizes,
        construct_actor_view, destroy_actor_view};
    return &table;
}

const ObjectFactoryVtable* button_bat_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504C34, button_bat_object_sizes,
        construct_button_bat_view, destroy_empty_factory_object};
    return &table;
}
}

void refresh_actor_model_animation_view(NativeActorFactoryInstance& instance)
{
    // A nested message refreshes mid-way through its caller's: keep what the view has written so far.
    store_actor_model_animation_view(instance);
    ModelAnimationState& pose = instance.pose;
    pose = ModelAnimationState{};
    instance.pose_blocks.clear();
    const Actor& actor = instance.bat.actor;
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return;
    }
    const RuntimeModelInstance& model = *actor.model->runtime;
    pose.primary = model.primary;
    pose.alternate = model.alternate;
    if (model.skeleton_node != nullptr)
    {
        pose.skeleton = model.skeleton_node->joints;
        pose.joint_count = int(model.skeleton_node->joints.size());
    }
    pose.auxiliary_targets = model.auxiliary_targets;
    std::copy(model.node_flags.begin(), model.node_flags.end(), pose.node_flags.begin());
    instance.pose_model = actor.model->runtime;
    std::copy(model.node_flags.begin(), model.node_flags.end(), instance.pose_flags_loaded.begin());
    for (const RuntimeModelResourceBlock& block : model.source->blocks)
    {
        instance.pose_blocks.push_back(ModelResourceBlock{block.type, block.word});
    }
    pose.resource_blocks = instance.pose_blocks;
}

void store_actor_model_animation_view(NativeActorFactoryInstance& instance)
{
    // Only bytes the view changed since it was loaded; the model's own writes in between stay.
    const Actor& actor = instance.bat.actor;
    if (instance.pose_model == nullptr || actor.model == nullptr || actor.model->runtime != instance.pose_model)
    {
        return;
    }
    RuntimeModelInstance& model = *instance.pose_model;
    for (size_t index = 0; index < model.node_flags.size(); ++index)
    {
        if (instance.pose.node_flags[index] != instance.pose_flags_loaded[index])
        {
            model.node_flags[index] = instance.pose.node_flags[index];
            instance.pose_flags_loaded[index] = instance.pose.node_flags[index];
        }
    }
}

NativeActorFactoryOwner::NativeActorFactoryOwner(NativeActorFactoryBindings bindings)
    : bindings(std::move(bindings))
{
}

void install_runtime_base_object_class(std::span<ObjectFactoryType> types)
{
    types[1] = ObjectFactoryType{0, base_lifecycle};
}

NativeActorFactoryOwner::~NativeActorFactoryOwner()
{
    if (active_owner == this)
    {
        active_owner = nullptr;
    }
}

void NativeActorFactoryOwner::install(std::span<ObjectFactoryType> types)
{
    if (active_owner != nullptr && active_owner != this)
    {
        throw Error("native actor factory already has an owner");
    }
    // Lifecycle views of retail getters 1074F0, 3C070 and 6D4A0. Other types
    // retain their caller-provided registrations and are not substituted.
    install_runtime_base_object_class(types);
    types[2] = ObjectFactoryType{1, actor_lifecycle};
    types[0x15] = ObjectFactoryType{2, button_bat_lifecycle};
    types[0x1A] = ObjectFactoryType{2, contact_prop_lifecycle}; // Getter 0x0006EE20.
    types[0x19] = ObjectFactoryType{2, knockdown_sphere_lifecycle}; // Getter 0x0006EB40.
    active_owner = this;
}

NativeActorFactoryInstance& NativeActorFactoryOwner::construct_view(FactoryObject& object)
{
    auto instance = std::make_unique<NativeActorFactoryInstance>();
    instance->bytes = {object.bytes.get(), object.size};
    instance->startup.attachments.pending = &instance->pending; // Actor +0x1B4.
    Actor& actor = instance->bat.actor;
    actor.class_key = read_u16(std::span<const u8>(object.bytes.get(), object.size), 6);
    actor.link = &instance->bind; // Retail embedded actor+0x918 link.
    instance->bind.actor = &actor;
    actor.render_registration = &instance->render;
    instance->segment.query = actor_segment_query;
    instance->segment.sweep = actor_segment_sweep;
    instance->segment.point = actor_segment_point;
    instance->segment.contacts = actor_segment_contacts;
    instance->segment.sphere_contacts = actor_segment_sphere_contacts;
    instance->shape.push = actor_shape_push;
    instance->render.message = [](ActorRenderRegistration& registration, u32 id)
    {
        active_owner->bindings.message(registration.actor, &id);
    };
    if (bindings.animation_message)
    {
        instance->animation.message_handler = [this, &actor](const ActorAnimationMessage& message)
        {
            bindings.animation_message(actor, message); // 0x0003B3C0
        };
    }
    const u8* address = object.bytes.get();
    actors.emplace(address, &actor);
    animations.emplace(&actor, &instance->animation);
    render.emplace(&actor, &instance->render);
    cutscene_bindings.emplace(address, &instance->cutscene);
    auto* result = instance.get();
    instances_.emplace(address, std::move(instance));
    return *result;
}

NativeActorFactoryInstance& NativeActorFactoryOwner::view(const FactoryObject& object)
{
    return *instances_.at(object.bytes.get());
}

NativeActorFactoryInstance& NativeActorFactoryOwner::view(const entity_detail::SegmentProbe& probe)
{
    for (const auto& [address, instance] : instances_)
    {
        if (&instance->segment == &probe)
        {
            return *instance;
        }
    }
    throw Error("segment probe has no actor factory view");
}

NativeActorFactoryInstance& NativeActorFactoryOwner::view(const entity_detail::ShapeProbe& probe)
{
    for (const auto& [address, instance] : instances_)
    {
        if (&instance->shape == &probe)
        {
            return *instance;
        }
    }
    throw Error("shape probe has no actor factory view");
}

void NativeActorFactoryOwner::release_view(FactoryObject& object)
{
    auto& instance = view(object);
    Actor& actor = instance.bat.actor;
    ActorFrameReleaseContext frames{bindings.factory, bindings.frame_objects, bindings.object_of,
        [&](const ActorReferenceMessage& message)
        {
            // Message 0x14 names the released object by its retail address, as the owner's body reads it (0x00027B20).
            std::array<u32, 0x42> words{message.id, bindings.object_address(static_cast<const u8*>(message.value))};
            bindings.message(actor, words.data());
        }};
    ActorReleaseContext context{instance.startup, bindings.startup, instance.render,
        frames, instance.frame.updates, instance.pending, instance.anchors,
        instance.live_attachments, instance.allocation_8fc, instance.matrices,
        bindings.sounds, instance.pose, bindings.empty_name};
    if (bindings.unlink_active_node)
    {
        context.unlink_active_node = [&]()
        {
            bindings.unlink_active_node(object.bytes.get());
        };
    }
    release_runtime_actor(context);
    if (released)
    {
        released(actor);
    }
    animations.erase(&actor);
    render.erase(&actor);
    actors.erase(object.bytes.get());
    cutscene_bindings.erase(object.bytes.get());
    instances_.erase(object.bytes.get());
}
}
