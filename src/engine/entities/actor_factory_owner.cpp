#include "actor_factory_owner.h"

namespace gbtg
{
namespace
{
NativeActorFactoryOwner* active_owner = nullptr;

// Xbox 0x001628B0
void destroy_empty_factory_object(FactoryObject&)
{
}

// Actor vtable collision slots set by 0x0003C080. Their shape routines are not translated yet, so a query that
// reaches one raises instead of reading through a null slot.
[[noreturn]] void untranslated_collision_slot(const char* slot)
{
    throw Error(std::string("actor collision slot is not translated: ") + slot);
}
i32 actor_segment_query(entity_detail::SegmentProbe&, Vec3&, Vec3&, u32, i32, u32, u8*)
{
    untranslated_collision_slot("vtable +0x20 (0x0003E110)");
}
i32 actor_segment_sweep(entity_detail::SegmentProbe&, Vec3&, Vec3&, i32, u32, u8*)
{
    untranslated_collision_slot("vtable +0x1C (0x0003DF70)");
}
i32 actor_segment_point(entity_detail::SegmentProbe&, Vec3&, Vec3&, float, u32, u8*)
{
    untranslated_collision_slot("vtable +0x2C (0x0003E200)");
}
i32 actor_segment_contacts(entity_detail::SegmentProbe&, Vec3&, float, u32, entity_detail::SegmentPool&)
{
    untranslated_collision_slot("vtable +0x30 (0x0003E3C0)");
}
i32 actor_segment_sphere_contacts(entity_detail::SegmentProbe&, Vec3&, float, i32, u32, entity_detail::SegmentPool&)
{
    untranslated_collision_slot("vtable +0x24 (0x0003E050)");
}
i32 actor_shape_push(entity_detail::ShapeProbe&, const Actor*, const Vec3*, EntityCylinderBound&, EntityCylinderBound*,
    i32, Vec3&)
{
    untranslated_collision_slot("vtable +0x34 (0x0003C0F0)");
}

// Native lifecycle slot adapters. The translated constructors/destructor keep
// their retail branches; these associate Xbox instance bytes with host fields.
bool construct_actor_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& instance = active_owner->construct_view(object);
    const bool result = initialize_runtime_actor(instance.startup, parameters,
        active_owner->bindings.startup);
    instance.frame.bone = instance.startup.bone;
    if (instance.bat.actor.model != nullptr)
    {
        auto& model = *instance.bat.actor.model->runtime;
        instance.pose.primary = model.primary;
        instance.pose.alternate = model.alternate;
    }
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
    active_owner = this;
}

NativeActorFactoryInstance& NativeActorFactoryOwner::construct_view(FactoryObject& object)
{
    auto instance = std::make_unique<NativeActorFactoryInstance>();
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

void NativeActorFactoryOwner::release_view(FactoryObject& object)
{
    auto& instance = view(object);
    Actor& actor = instance.bat.actor;
    ActorFrameReleaseContext frames{bindings.factory, bindings.frame_objects,
        [&](const ActorReferenceMessage& message)
        {
            bindings.message(actor, &message);
        }};
    ActorReleaseContext context{instance.startup, bindings.startup, instance.render,
        frames, instance.frame.updates, instance.pending, instance.anchors,
        instance.live_attachments, instance.allocation_8fc, instance.matrices,
        bindings.sounds, instance.pose, bindings.empty_name};
    release_runtime_actor(context);
    animations.erase(&actor);
    render.erase(&actor);
    actors.erase(object.bytes.get());
    cutscene_bindings.erase(object.bytes.get());
    instances_.erase(object.bytes.get());
}
}
