#include "background_class_owner.h"

namespace gbtg
{
namespace
{
NativeBackgroundClassOwner* active_owner = nullptr;

bool construct_background_view(FactoryObject& object, std::span<const u8> parameters)
{
    return active_owner->construct(object, parameters);
}

void release_background_view(FactoryObject& object)
{
    active_owner->release(object);
}

const ObjectFactoryVtable* background_lifecycle()
{
    static const ObjectFactoryVtable table{0x004E1644, runtime_background_storage_sizes,
        construct_background_view, release_background_view};
    return &table;
}
}

NativeBackgroundInstance::NativeBackgroundInstance(std::span<u8> object, Actor& actor)
    : bytes(object),
      state{object, actor, matrices, lifetime, animation, {}, {}}
{
}

NativeBackgroundClassOwner::NativeBackgroundClassOwner(NativeBackgroundClassBindings owner_bindings)
    : bindings(std::move(owner_bindings))
{
}

NativeBackgroundClassOwner::~NativeBackgroundClassOwner()
{
    if (active_owner == this)
    {
        active_owner = nullptr;
    }
}

void NativeBackgroundClassOwner::install(std::span<ObjectFactoryType> types)
{
    if (active_owner != nullptr && active_owner != this)
    {
        throw Error("native background class already has an owner");
    }
    types[4] = {2, background_lifecycle};
    active_owner = this;
}

NativeBackgroundInstance* NativeBackgroundClassOwner::find(const u8* bytes)
{
    const auto found = instances_.find(bytes);
    return found == instances_.end() ? nullptr : found->second.get();
}

NativeBackgroundInstance& NativeBackgroundClassOwner::instance(const FactoryObject& object)
{
    auto* found = find(object.bytes.get());
    if (found == nullptr)
    {
        throw Error("factory object is not a native background");
    }
    return *found;
}

NativeBackgroundInstance* NativeBackgroundClassOwner::first_background()
{
    for (const auto& [bytes, other] : instances_)
    {
        if (&other->lifetime.link == bindings.backgrounds.next)
        {
            return other.get();
        }
    }
    return nullptr;
}

bool NativeBackgroundClassOwner::construct(FactoryObject& object, std::span<const u8> parameters)
{
    auto& view = bindings.actors.view(object);
    auto created = std::make_unique<NativeBackgroundInstance>(std::span<u8>(object.bytes.get(), object.size),
        view.bat.actor);
    auto& background = *created;
    instances_.emplace(object.bytes.get(), std::move(created));
    // 0x00044490 reads the first background on the +0x934 list (DAT_00511B20), which is this one when the
    // list is empty because the constructor appends before it runs.
    if (bindings.backgrounds.next == nullptr)
    {
        first_animation = &background.animation;
    }
    else
    {
        auto* first = first_background();
        if (first == nullptr)
        {
            throw Error("first +0x934 list node is not a native background");
        }
        first_animation = &first->animation;
    }
    background.services = bindings.services(background, view, first_animation);
    return initialize_runtime_background(background.state, parameters, *background.services);
}

void NativeBackgroundClassOwner::release(FactoryObject& object)
{
    auto& background = instance(object);
    u8 count = 0;
    std::vector<BackgroundAnimationAllocation> rows;
    if (background.lifetime.animation != nullptr)
    {
        // 0x00044620 reads the first background's model header count and its +0x950 rows.
        auto* first = first_background();
        if (first == nullptr || first->state.actor.model == nullptr || first->state.actor.model->cpu.data() == nullptr)
        {
            throw Error("background release reads the first background's model through address zero");
        }
        const auto cpu = first->state.actor.model->cpu;
        count = cpu[read_u32(cpu, read_u32(cpu, 0x10) + 0x10) + 1];
        for (const auto& graph : first->state.graphs)
        {
            rows.push_back({graph.allocation});
        }
        if (rows.size() < count)
        {
            throw Error("first background has fewer animation rows than its header count");
        }
    }
    RuntimeBackgroundRelease services{count, rows, bindings.disable_framebuffer_clear, bindings.model_sound,
        bindings.heap_free};
    destroy_runtime_background(background.lifetime, services);
    instances_.erase(object.bytes.get());
}

void NativeBackgroundClassOwner::message(FactoryObject& object, std::span<u32> message)
{
    bindings.message(instance(object), message);
}
}
