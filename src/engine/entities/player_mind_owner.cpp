#include "player_mind_owner.h"
#include "mind_fields.h"
#include "player_mind_control.h"
#include "cinema_mind_slots.h"
#include "cinema_mind_requests.h"

namespace gbtg
{
namespace
{
NativePlayerMindOwner* active_owner = nullptr;

// Native lifecycle slot adapters. The translated constructors and destructors keep their retail
// branches; these associate the Xbox instance bytes with the player's native contexts.
bool construct_base_mind_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& slots = active_owner->construct_view(object, parameters);
    return initialize_runtime_base_mind(slots.mind, parameters, slots.actor_class, slots.settings, slots.random,
        slots.resources, slots.script_table);
}

void destroy_base_mind_view(FactoryObject& object)
{
    auto& slots = active_owner->view(object);
    destroy_runtime_base_mind(slots.requests, slots.events, slots.storage, slots.script_table);
    active_owner->release_view(object);
}

bool construct_player_mind_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& slots = active_owner->view(object);
    return initialize_runtime_player_mind(slots.mind, parameters, slots.setup);
}

void destroy_player_mind_view(FactoryObject& object)
{
    auto& slots = active_owner->view(object);
    destroy_runtime_player_mind(slots.actor_bytes, slots.runtime_flags, slots.input_activation);
}

const ObjectFactoryVtable* base_mind_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD6F4, base_mind_object_sizes, construct_base_mind_view,
        destroy_base_mind_view};
    return &table;
}

const ObjectFactoryVtable* player_mind_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505498, player_mind_object_sizes, construct_player_mind_view,
        destroy_player_mind_view};
    return &table;
}
bool construct_cinema_mind_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& slots = active_owner->view(object);
    if (slots.cinema == nullptr)
    {
        throw Error("cinema mind factory requires its native method contexts before construction");
    }
    return initialize_runtime_cinema_mind(slots.cinema->lifecycle, parameters);
}
void destroy_cinema_mind_view(FactoryObject& object)
{
    destroy_runtime_cinema_mind(active_owner->view(object).cinema->lifecycle);
}
const ObjectFactoryVtable* cinema_mind_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505478, runtime_cinema_mind_storage_sizes,
        construct_cinema_mind_view, destroy_cinema_mind_view};
    return &table;
}
}

NativePlayerMindOwner::NativePlayerMindOwner(NativePlayerMindBindings bindings)
    : bindings(std::move(bindings))
{
}

NativePlayerMindOwner::~NativePlayerMindOwner()
{
    if (active_owner == this)
    {
        active_owner = nullptr;
    }
}

void NativePlayerMindOwner::install(std::span<ObjectFactoryType> types)
{
    if (active_owner != nullptr && active_owner != this)
    {
        throw Error("native player mind factory already has an owner");
    }
    // Lifecycle views of retail getters 0x00029D30 and 0x0007E100; type 1 stays with the actor owner.
    types[7] = ObjectFactoryType{1, base_mind_lifecycle};
    types[0x34] = ObjectFactoryType{7, player_mind_lifecycle};
    active_owner = this;
}

NativePlayerMindSlots& NativePlayerMindOwner::construct_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto slots = bindings.create_slots(object, parameters);
    auto* result = slots.get();
    instances_[object.bytes.get()] = std::move(slots);
    return *result;
}

void NativePlayerMindOwner::install_cinema(std::span<ObjectFactoryType> types)
{
    install(types);
    types[0x35] = ObjectFactoryType{7, cinema_mind_lifecycle};
}

NativePlayerMindSlots& NativePlayerMindOwner::view(const FactoryObject& object)
{
    return *instances_.at(object.bytes.get());
}

void NativePlayerMindOwner::release_view(const FactoryObject& object)
{
    instances_.erase(object.bytes.get());
    if (bindings.released)
    {
        bindings.released(object);
    }
}

void NativePlayerMindOwner::message(FactoryObject& mind, const void* message)
{
    auto& slots = view(mind);
    if (slots.cinema != nullptr)
    {
        if (slots.cinema->messages == nullptr)
        {
            throw Error("cinema mind message slot 0x0007C8B0 is not attached");
        }
        dispatch_runtime_cinema_mind_message(*slots.cinema->messages,
            {static_cast<const u32*>(message), 0xC});
        return;
    }
    // 0x0007F1E0 reads at most message word 0xB, and only for the ids that carry it.
    const std::span<const u32> words(static_cast<const u32*>(message), 0xC);
    std::span<const u8> target;
    // The parent slot dereferences message +4 only on 0x8D's matching-target branch.
    if (words[0] == 0x8D && get(slots.mind, mind_field::target_record) == words[1]
        && words[2] != 0 && get(slots.mind, mind_field::target) != 0)
    {
        target = slots.object_at(words[1]);
    }
    dispatch_runtime_player_mind_message(*slots.messages, words, target);
}

void NativePlayerMindOwner::tick(FactoryObject& mind, u32 input)
{
    auto& slots = view(mind);
    if (slots.cinema != nullptr)
    {
        if (slots.cinema->tick == nullptr)
        {
            throw Error("cinema mind tick slot 0x0007CE30 is not attached");
        }
        tick_runtime_cinema_mind(*slots.cinema->tick);
        return;
    }
    tick_runtime_player_mind(*slots.tick, slots.input_at(input));
}

i32 NativePlayerMindOwner::update_request(FactoryObject& mind, RuntimePlayerMindRequest& request)
{
    auto& slots = view(mind);
    if (slots.cinema != nullptr)
    {
        if (slots.cinema->request_update == nullptr)
        {
            throw Error("cinema mind request slot 0x0007C0E0 is not attached");
        }
        return update_runtime_cinema_mind_request(*slots.cinema->request_update, request.bytes);
    }
    if (slots.request_update == nullptr)
    {
        throw Error("player mind request slot 0x0007D360 is not attached");
    }
    return update_runtime_player_mind_request(*slots.request_update, request);
}

u32 NativePlayerMindOwner::initialize_request(FactoryObject& mind, RuntimePlayerMindRequest& request)
{
    auto& slots = view(mind);
    if (slots.cinema != nullptr)
    {
        return initialize_runtime_cinema_mind_request(slots.actor, slots.cinema->target_bytes(),
            request.bytes, slots.random);
    }
    return prepare_runtime_player_mind_request(slots.mind, slots.actor, slots.target_position(), request.bytes,
        slots.random);
}

// Xbox 0x001628B0
void NativePlayerMindOwner::release_request(FactoryObject&, RuntimePlayerMindRequest&)
{
}
}
