#include "scene_actor_services_state.h"
#include "../entities/mind_fields.h"
#include <cstddef>
#include "../entities/player_fields.h"

// Retail addresses of native objects, object lookup, message routing, contacts, attachments and activation.
namespace gbtg
{
// Retail address of a native pointer: a scene object, file-backed XBE memory, or a relocated resource.
u32 SceneActorServices::State::heap_address(const void* pointer, size_t size)
{
    auto [found, inserted] = heap_addresses.try_emplace(pointer, next_heap_address);
    if (inserted)
    {
        next_heap_address += u32((size + 15) & ~size_t(15));
    }
    // The block's bytes for memory_at; a later registration with a larger size widens the view (its address
    // range stays the first registration's).
    auto& block = heap_blocks[found->second];
    if (size > block.size())
    {
        block = {static_cast<const u8*>(pointer), size};
    }
    return found->second;
}

RuntimeStrategyMessageActor* SceneActorServices::State::strategy_message_actor(u32 address)
{
    if (address == 0)
    {
        unbound("strategy message actor at address zero");
    }
    auto* object = object_of(scene.resolve_address(address, 0).data());
    if (object == nullptr || !actors->actors.contains(object->bytes.get()))
    {
        unbound("strategy message actor that is no scene actor");
    }
    auto& view = actors->view(*object);
    refresh_mind_targets();
    // Actor +0x8F8: the first record on the actor's +0x8F4 live attachment list, or 0.
    RuntimeMindTarget* target = nullptr;
    if (auto* link = view.live_attachments.next; link != nullptr)
    {
        auto* record = reinterpret_cast<ActorLiveAttachment*>(link);
        if (const auto found = mind_target_rows.find(record); found != mind_target_rows.end())
        {
            target = found->second->target.get();
        }
    }
    auto& entry = strategy_message_actors[address];
    if (entry == nullptr)
    {
        entry = std::make_unique<RuntimeStrategyMessageActor>();
    }
    else if (strategy_message_building.contains(address))
    {
        return entry.get(); // A holder chain back to an actor being built: its view is the one in progress.
    }
    strategy_message_building.insert(address);
    *entry = RuntimeStrategyMessageActor{address, &view.bat.actor, {object->bytes.get(), object->size}, target,
        nullptr};
    // Body +0x274 is a player's; the other classes' +0x95C word is no body (0x00138B40 asks only about players).
    if (object->type == 5)
    {
        const u32 holder = read_u32(players->player(*object).body, 0x274);
        RuntimeStrategyMessageActor* own = entry.get();
        own->holder = holder == 0 ? nullptr : strategy_message_actor(holder);
    }
    strategy_message_building.erase(address);
    return strategy_message_actors.at(address).get();
}

u32 SceneActorServices::State::first_live_attachment_at(u32 actor_address)
{
    auto* object = object_of(scene.resolve_address(actor_address, 0).data());
    if (object == nullptr || !actors->actors.contains(object->bytes.get()))
    {
        unbound("actor +0x8F8 of an address that is no scene actor");
    }
    auto* link = actors->view(*object).live_attachments.next;
    if (link == nullptr)
    {
        return 0;
    }
    return heap_address(reinterpret_cast<ActorLiveAttachment*>(link), 0x68); // As refresh_mind_targets keys it.
}

RuntimeMindTarget* SceneActorServices::State::mind_target_at(u32 address)
{
    refresh_mind_targets();
    if (const auto found = target_records.find(address); found != target_records.end())
    {
        return found->second;
    }
    if (const auto retired = retired_targets.find(address); retired != retired_targets.end())
    {
        return retired->second->target.get();
    }
    unbound("mind target record " + std::to_string(address) + " is neither live nor retired");
}

// DAT_00511B48 (the DAT_00511B44 list's first link) as the strategies walk it, e.g. 0x00129280's count of live
// players that 0x00138410 holds an attack-limited ghouly back with.
void SceneActorServices::State::refresh_active_actor_links()
{
    active_actor_links.clear();
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.alt.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (static_cast<void*>(&player->links[2]) == static_cast<void*>(node))
            {
                owner = player;
                break;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B44 node with no native player");
        }
        const auto query = target_queries.find(&owner->actor);
        if (query == target_queries.end())
        {
            unbound("DAT_00511B44 player with no body view");
        }
        active_actor_links.push_back(RuntimeActiveActorLink{nullptr, query->second, &owner->actor.goal_ring});
    }
    for (size_t index = 0; index + 1 < active_actor_links.size(); ++index)
    {
        active_actor_links[index].next = &active_actor_links[index + 1];
    }
    active_actors = active_actor_links.empty() ? nullptr : &active_actor_links[0];
}

void SceneActorServices::State::refresh_mind_targets()
{
    std::unordered_set<const ActorLiveAttachment*> live;
    target_views.clear();
    target_records.clear();
    RuntimeMindTarget* previous = nullptr;
    targets = nullptr;
    for (auto* link = static_cast<entity_detail::ListHead*>(global_attachments.next); link != nullptr;
         link = static_cast<entity_detail::ListHead*>(link->next))
    {
        auto* record = reinterpret_cast<ActorLiveAttachment*>(reinterpret_cast<u8*>(link)
            - offsetof(ActorLiveAttachment, global_link));
        live.insert(record);
        auto& row = mind_target_rows[record];
        if (row == nullptr)
        {
            row = std::make_unique<SceneMindTargetRow>();
        }
        Actor& actor = *record->actor;
        const u32 address = heap_address(record, 0x68);
        const u32 actor_address = address_of(&actor);
        std::memcpy(row->bytes.data() + 0x10, &actor_address, 4);
        std::memcpy(row->bytes.data() + 0x14, &record->mode, 0x54);
        std::span<const u8> actor_bytes;
        const OccupancyRecord* occupancy = nullptr;
        FactoryObject& object = object_of_actor(actor);
        actor_bytes = {object.bytes.get(), object.size};
        if (object.type == 5)
        {
            occupancy = players->player(object).current; // Actor +0xCA8.
        }
        RuntimeScriptMindQuery* query_view = &absent_target_query;
        if (const auto found = target_queries.find(&actor); found != target_queries.end())
        {
            query_view = found->second;
        }
        // A live record keeps its target object, so strategy and mind contexts holding it across a nested refresh
        // stay valid; only the reference members force a new one.
        if (row->target == nullptr || &row->target->actor != &actor || &row->target->body != query_view)
        {
            row->target = std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, row->bytes, address, actor,
                actor_bytes, *query_view, occupancy, record});
        }
        else
        {
            row->target->next = nullptr;
            row->target->bytes = row->bytes;
            row->target->address = address;
            row->target->actor_bytes = actor_bytes;
            row->target->occupancy = occupancy;
            row->target->record = record;
        }
        row->actor_bytes.assign(actor_bytes.begin(), actor_bytes.end());
        row->body = *query_view;
        row->bits.clear();
        if (object.type == 5)
        {
            if (const auto* flags = players->player(object).body_flags; flags != nullptr)
            {
                row->bits.assign(flags->bytes, flags->bytes + flags->byte_count);
            }
        }
        row->view = RuntimePlayerTargetView{row->bytes, address, &actor, row->target.get()};
        if (previous == nullptr)
        {
            targets = row->target.get();
        }
        else
        {
            previous->next = row->target.get();
        }
        previous = row->target.get();
        target_views[address] = &row->view;
        target_records[address] = row->target.get();
    }
    // Player minds' own +0xB8 records are not on DAT_00545988 but are addressed like its records.
    for (const auto& [address, embedded] : embedded_targets)
    {
        target_views[address] = embedded.first;
        target_records[address] = embedded.second;
    }
    std::erase_if(mind_target_rows, [this, &live](const auto& row)
    {
        if (live.contains(row.first))
        {
            return false;
        }
        // A record unlinked while its actor lives on keeps its last bytes too (a released actor's are kept by
        // the release hook).
        if (row.second->target != nullptr && !retired_targets.contains(row.second->target->address))
        {
            Actor* actor = &row.second->target->actor;
            const bool alive = std::any_of(actors->actors.begin(), actors->actors.end(), [actor](const auto& entry)
            {
                return entry.second == actor;
            });
            if (alive)
            {
                retired_targets[row.second->target->address] = retire_target(*row.second, *row.second->target);
            }
        }
        return true;
    });
    std::erase_if(retired_targets, [this](const auto& retired)
    {
        return target_views.contains(retired.first);
    });
}

u32 SceneActorServices::State::address_of(const void* pointer)
{
    if (pointer == nullptr)
    {
        return 0;
    }
    if (const u32 address = scene.find_address(pointer))
    {
        return address;
    }
    if (const u32 address = image.address_of(pointer))
    {
        return address;
    }
    // A scene actor's native Actor is the retail actor object.
    for (const auto& [bytes, actor] : actors->actors)
    {
        if (pointer == actor)
        {
            return scene.address_of(bytes);
        }
    }
    // World list heads (DAT_00511B34..DAT_00511B4C) and each player's list nodes (actor +0x934 + 8n).
    auto& world = entity_detail::g_world;
    const std::pair<const void*, u32> heads[]{{&world.body_a, 0x00511B1C}, {&world.body_b, 0x00511B24},
        {&world.body_c, 0x00511B2C}, {&world.mark, 0x00511B34}, {&world.case_head, 0x00511B3C},
        {&world.alt, 0x00511B44}, {&world.slot, 0x00511B4C}, {&world.child, 0x00511B54}};
    for (const auto& [head, address] : heads)
    {
        if (pointer == head)
        {
            return address;
        }
    }
    for (auto* player : players->instances())
    {
        // 0x00136700 assigns this real heap block before storing its address in mind +0x1C.
        if (player->mind_storage != nullptr && pointer == player->mind_storage)
        {
            return heap_address(pointer, sizeof(RuntimeMindStorage));
        }
        for (u32 index = 0; index < 4; ++index)
        {
            if (pointer == &player->links[index])
            {
                return player->actor_address + 0x934 + index * 8;
            }
        }
        // Actor +0x960, the 0x00107830 input device.
        if (!player->input.empty() && pointer == player->input.data())
        {
            const u32 address = heap_address(pointer, player->input.size());
            input_addresses[address] = player->input;
            return address;
        }
    }
    if (const auto found = heap_addresses.find(pointer); found != heap_addresses.end())
    {
        return found->second;
    }
    // Occupancy records (actor +0x98C, +0xCA0, +0xCA4) live in the record block of the DAT_00512344 pool.
    if (g_occupancy_pool != nullptr)
    {
        const auto& pool = *static_cast<const OccupancyPool*>(g_occupancy_pool);
        const auto* begin = reinterpret_cast<const u8*>(pool.begin);
        const auto* limit = reinterpret_cast<const u8*>(pool.limit);
        const auto* bytes = static_cast<const u8*>(pointer);
        if (begin != nullptr && bytes >= begin && bytes < limit)
        {
            return heap_address(begin, size_t(limit - begin)) + u32(bytes - begin);
        }
    }
    // Model anchors are heap blocks from 0x0011C0B0 on the DAT_00510654 list.
    for (auto* node = static_cast<entity_detail::ListHead*>(g_actor_model_anchors.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        if (pointer == reinterpret_cast<u8*>(node) - offsetof(ActorModelAnchor, global_link))
        {
            return heap_address(pointer, sizeof(ActorModelAnchor));
        }
    }
    try
    {
        return resources.memory.address_of(static_cast<const u8*>(pointer));
    }
    catch (const std::exception&)
    {
        unbound("retail address of a native pointer outside scene objects, XBE data and resources");
    }
}

// Xbox 0x0003DE30, ECX actor, ESI object: the object on the actor's +0x1D8 list is told 0x14 and released.
bool SceneActorServices::State::release_actor_frame_object(Actor& actor, u32 object)
{
    auto& view = actors->view(object_of_actor(actor));
    for (auto* update = view.frame.updates; update != nullptr; update = update->next)
    {
        FactoryObject* owned = object_of(frame_objects.at(update));
        if (owned == nullptr)
        {
            unbound("0x0003DE30 frame object outside the scene");
        }
        if (address_of(owned->bytes.get()) != object)
        {
            continue;
        }
        // Message 0x14 names the released object by its retail address (the owner's body reads it, 0x00027B20).
        std::array<u32, 0x42> notice{0x14, object};
        actors->bindings.message(actor, notice.data());
        release_factory_object(scene.factory, *owned); // 0x001075C0
        return true;
    }
    return false;
}

FactoryObject& SceneActorServices::State::scene_object(const u8* bytes)
{
    FactoryObject* object = object_of(bytes);
    if (object == nullptr)
    {
        throw Error("scene object update finds its object released");
    }
    return *object;
}

FactoryObject* SceneActorServices::State::object_of(const u8* bytes)
{
    // A moved-from object keeps null bytes; an unresolved address must not match it.
    if (bytes == nullptr)
    {
        return nullptr;
    }
    for (auto* owned : {&scene.objects, &cameras, &callout_objects, &cutscene_actors, &dialogue_bats})
    {
        for (auto& object : *owned)
        {
            if (object.bytes.get() == bytes)
            {
                return &object;
            }
        }
    }
    for (auto& object : released_weapons)
    {
        if (object.bytes.get() == bytes)
        {
            return &object;
        }
    }
    for (auto* player : players->instances())
    {
        for (FactoryObject* object : {&player->created_attachment, &player->body_object,
            &player->mind_object, &player->strategy_object, &player->first_action,
            &player->second_action, &player->body_action_object})
        {
            if (object->bytes.get() == bytes)
            {
                return object;
            }
        }
        // A player still in its constructor: a weapon it attaches there (0x0001B2B0) names it in message 0x91.
        if (player->constructing != nullptr && player->constructing->bytes.get() == bytes)
        {
            return player->constructing;
        }
    }
    // Only while its creation runs: the comparison never reads the pending object itself.
    if (attaching_bytes == bytes)
    {
        return attaching_object;
    }
    return nullptr;
}

// The body constructors read the model's block directory and the instance channel word (+0x1CC).
NativePlayerModelViews SceneActorServices::State::model_views_of(const Actor& actor)
{
    if (actor.model == nullptr)
    {
        return NativePlayerModelViews{nullptr, nullptr};
    }
    auto& views = model_views[&actor];
    views = std::make_unique<SceneActorModelViews>();
    const auto cpu = actor.model->cpu;
    const u32 table = read_u32(cpu, 0);
    const u32 count = read_u32(cpu, 4);
    for (u32 index = 0; index < count; ++index)
    {
        const u32 offset = read_u32(cpu, table + index * 8 + 4);
        views->rows.push_back({read_u32(cpu, table + index * 8), offset == 0 ? nullptr : cpu.data() + offset});
    }
    views->nodes.nodes = views->rows;
    const u32 channel = actor.model->runtime != nullptr && actor.model->runtime->auxiliary_targets[0] != nullptr;
    std::memcpy(views->channels.data() + 0x1CC, &channel, 4);
    return NativePlayerModelViews{&views->nodes, views->channels.data()};
}

SceneActorBase& SceneActorServices::State::base_of(FactoryObject& object)
{
    auto& slot = bases[object.bytes.get()];
    if (slot == nullptr)
    {
        auto& view = actors->view(object);
        const u32 address = scene.address_of(object.bytes.get());
        slot = std::make_unique<SceneActorBase>(
            ActorTrailContext{scene.factory, scene.objects, address, pause, frame_delta, time_scale, pal,
                pal_override, empty_name},
            view.startup, effects, scares, voices, object,
            ActorAttachmentEventContext{view.bat.actor, view.startup.attachments, effects, scene.factory,
                image.pools, scene.objects, address, linked_records, {}},
            view, world_bind, node_cache, reset_matrix);
    }
    return *slot;
}

// Actor +0x204's model instance poses (+0x164, +0x168), as the shape code reads them.
void SceneActorServices::State::refresh_shape_poses(FactoryObject& object)
{
    if (!actors->actors.contains(object.bytes.get()))
    {
        return;
    }
    auto& actor = actors->view(object).bat.actor;
    if (actor.model == nullptr || actor.model->runtime == nullptr)
    {
        return;
    }
    auto& base = base_of(object);
    const auto& runtime = *actor.model->runtime;
    base.primary_pose.buffer = runtime.primary;
    base.alternate_pose.buffer = runtime.alternate;
    actor.model->primary_shape_pose = runtime.primary != nullptr ? &base.primary_pose : nullptr;
    actor.model->alternate_shape_pose = runtime.alternate != nullptr ? &base.alternate_pose : nullptr;
}

void SceneActorServices::State::object_message(FactoryObject& object, std::span<u32> message)
{
    refresh_shape_poses(object);
    // The pose routines read the actor's model instance through its native animation view.
    const u8* bytes = object.bytes.get();
    if (actors->actors.contains(bytes))
    {
        refresh_actor_model_animation_view(actors->view(object));
    }
    route_object_message(object, message);
    if (auto* live = object_of(bytes); live != nullptr && actors->actors.contains(bytes))
    {
        store_actor_model_animation_view(actors->view(*live));
    }
}

void SceneActorServices::State::route_object_message(FactoryObject& object, std::span<u32> message)
{
    const u16 type = object.type;
    if (type == 0x31)
    {
        message_fx_custom_outline({object.bytes.get(), object.size}, message); // 0x00077A80
        return;
    }
    if (type == 0x30 || type == 0x33)
    {
        message_fx_custom_fade_to_grey({object.bytes.get(), object.size}, message); // 0x00076DF0
        return;
    }
    if (type == 0x32)
    {
        message_fx_custom_solid_fade_in({object.bytes.get(), object.size}, message); // 0x000780C0
        return;
    }
    if (type == 0x19)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_knockdown_sphere(*object_of(bytes));
        };
        const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actor_or_retired(address);
        };
        auto bindings = word_messages;
        if (message[0] == 0x7C)
        {
            bindings.reference_at = [actor_at](u32 address) -> void*
            {
                return actor_at(address);
            };
        }
        handle_runtime_knockdown_sphere_message({object.bytes.get(), object.size}, view.prop_contacts,
            message, actor_at, base_of(object).base, bindings); // 0x0006EBD0
        return;
    }
    if (type == 0x17)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_furnace(*object_of(bytes));
        };
        const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actor_or_retired(address);
        };
        auto bindings = word_messages;
        if (message[0] == 0x7C)
        {
            bindings.reference_at = [actor_at](u32 address) -> void*
            {
                return actor_at(address);
            };
        }
        handle_runtime_furnace_message({object.bytes.get(), object.size}, view.prop_contacts, view.furnace_contacts,
            message, actor_at, base_of(object).base, bindings); // 0x0006E2A0
        return;
    }
    if (type == 0x18)
    {
        // Getter 0x0006E950: +0x10 is 0x0006EA00 (the actor update 0x000420D0 first) and +0xC is 0x0006C9B0, a
        // jump to the actor's 0x0003EF70.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_ghouly_key(updated);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        return;
    }
    if (type == 0x1B)
    {
        // Getter 0x000704D0: +0x10 is 0x000705B0 (the actor update 0x000420D0 first) and +0xC is 0x000706E0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_spider_web(*object_of(bytes));
        };
        refresh_actor_model_animation_view(view);
        RuntimeSpiderWeb web{{object.bytes.get(), object.size}, view.bat.actor, view.animation, resources.registry,
            resources.error, random};
        RuntimeSpiderWebMessage handlers{
            [this, &object](std::span<u32> words)
            {
                dispatch_base_actor_word_message(base_of(object).base, words, word_messages); // 0x0003EF70
            },
            view.render, view.shape, word_messages.memory_at};
        dispatch_runtime_spider_web_message(web, message, handlers); // 0x000706E0
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x1A)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            update_contact_prop(*object_of(bytes));
        };
        const std::function<Actor*(u32)> actor_at = [this](u32 address) -> Actor*
        {
            if (address == 0)
            {
                return nullptr;
            }
            return actor_or_retired(address);
        };
        auto bindings = word_messages;
        if (message[0] == 0x7C)
        {
            bindings.reference_at = [actor_at](u32 address) -> void*
            {
                return actor_at(address);
            };
        }
        handle_runtime_contact_prop_message({object.bytes.get(), object.size}, view.prop_contacts,
            message, actor_at, base_of(object).base, bindings);
        return;
    }
    if (type == 0xAA || type == 0xA9 || type == 0xA8 || type == 0xA7 || type == 0xA0 || type == 0xA1 || type == 0xA2
        || type == 0xA3 || type == 0xA5 || type == 0xA6)
    {
        // Getters 0x000CEF70, 0x000CD640 and 0x000CB570: +0x10 is the weapon update 0x0014A2E0; +0xC is the chair message
        // 0x000CF0D0 or the large projectile's 0x000CDBE0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_weapon(updated); // 0x0014A2E0
            store_actor_model_animation_view(updated_view);
        };
        refresh_shape_poses(object);
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x1D)
    {
        // Getter 0x0006EFB0: +0x10 is 0x0006F1C0, which ends with the actor update 0x000420D0 and the bound size
        // 0x0003D670 (mask 0x200).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor& actor)
        {
            // Both updates can create scene objects, which may move the object storage, so the object is found again
            // by its bytes after each; the view belongs to the actor class and stays put.
            auto& updated_view = actors->view(scene_object(bytes));
            refresh_actor_model_animation_view(updated_view);
            classes->update_scare_figure(scene_object(bytes));
            store_actor_model_animation_view(updated_view);
            update_base_actor(scene_object(bytes)); // 0x000420D0
            measure_runtime_actor_bound_size(actor, 0x200,
                reinterpret_cast<GhoulySpawnPool*&>(updated_view.allocation_8fc)); // 0x0003D670
        };
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x20)
    {
        // Getter 0x000711A0: +0x10 is 0x00071380, which ends each phase with the actor update 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_sss_door(updated, [this, &updated_view]()
                {
                    update_actor_frame(updated_view.frame, *frame); // 0x000420D0
                });
            store_actor_model_animation_view(updated_view);
        };
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x21)
    {
        // Getter 0x00071820: +0x10 is 0x00071960, which ends each phase with the actor update 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_sss_dropper(updated, [this, &updated_view]()
                {
                    update_actor_frame(updated_view.frame, *frame); // 0x000420D0
                });
            store_actor_model_animation_view(updated_view);
        };
        refresh_actor_model_animation_view(view);
        classes->message(object, message);
        store_actor_model_animation_view(view);
        return;
    }
    if (type == 0x22)
    {
        // Getter 0x00072270: +0x10 is 0x00072480, which runs the actor update 0x000420D0 between the ditty and the
        // watch for the player.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            classes->update_super_scary_shock(scene_object(bytes), [this, bytes]()
                {
                    update_base_actor(scene_object(bytes)); // 0x000420D0; the storage may have moved since.
                });
        };
        classes->message(object, message);
        return;
    }
    if (type == 0x26)
    {
        // Getter 0x0006BB30: +0x10 is 0x0006C0A0 itself, with no actor update 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            classes->update_swap_setup(*object_of(bytes));
        };
        classes->message(object, message);
        return;
    }
    if (type == 0x25 || type == 0x24 || type == 0x1E)
    {
        // Getter 0x0006B110: +0x10 is 0x0006B8B0; getter 0x0006A4F0: +0x10 is 0x0006ADC0; getter 0x0006CC70: +0x10
        // is 0x0006CFD0. Each runs the actor update 0x000420D0 first.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes, type](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            if (type == 0x25)
            {
                classes->update_cutscene_setup(updated);
            }
            else if (type == 0x24)
            {
                classes->update_bg_break_setup(updated);
            }
            else
            {
                classes->update_bg_break_key(updated);
            }
        };
        classes->message(object, message);
        return;
    }
    if (type == 0xD || type == 0x1F || type == 0x23)
    {
        // Vtable +0x10: the emitter's 0x0010A2E0 (getter 0x00109DD0), the trigger's 0x00069D20 (getter
        // 0x00069620) and, for areas (getter 0x0006DBD0 keeps the base slot), 0x000420D0.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes, type](Actor&)
        {
            auto& updated = *object_of(bytes);
            if (type == 0x1F)
            {
                update_base_actor(updated);
                return;
            }
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            if (type == 0xD)
            {
                classes->update_emitter(updated);
            }
            else
            {
                classes->update_trigger(updated);
            }
            store_actor_model_animation_view(updated_view);
        };
        classes->message(object, message);
        return;
    }
    if (type == 4)
    {
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            backgrounds->update(*object_of(bytes));
        };
        backgrounds->message(object, message);
        return;
    }
    if (type == 5)
    {
        auto& player = players->player(object);
        actor_updates[&player.actor] = [this, player = &player](Actor&)
        {
            update_player(*player);
        };
        if (!player.actor_message)
        {
            attach_player_messages(object, player);
        }
        if (message[0] == 0x7C || message[0] == 0x8B)
        {
            auto& graph = *player_messages.at(object.bytes.get());
            ActorReferenceMessage native{message[0], message[1] == 0 ? nullptr :
                word_messages.reference_at(message[1])};
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = &input_selected;
            return;
        }
        if (message[0] == 0xB)
        {
            // Retail words {0xB, slot, mix, name}: the player's handler and the base actor read the native message.
            auto& graph = *player_messages.at(object.bytes.get());
            ActorAnimationMixMessage native = decode_actor_animation_mix_message(message, word_messages);
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = &input_selected;
            return;
        }
        if (message[0] == 0xE || message[0] == 0xF)
        {
            auto& graph = *player_messages.at(object.bytes.get());
            const u32 sender_address = message[1];
            RuntimeMessageReceiver sender{sender_address,
                [this, sender_address](std::span<const u32> words)
                {
                    auto* sender_object = object_of(scene.resolve_address(sender_address, 0).data());
                    object_message(*sender_object, {const_cast<u32*>(words.data()), words.size()});
                }};
            RuntimePlayerContactMessage native{message[0], &sender, {}, u16(message[8])};
            std::copy_n(message.data() + 2, 6, native.words.data());
            handle_runtime_player_message(&native, graph.context);
            for (FactoryObject* child : {&graph.children.strategy, &graph.children.mind, &graph.children.body})
            {
                if (child->bytes != nullptr)
                {
                    child_message(*child, message.data());
                }
            }
            input_state.devices = input_activation.entries;
            input_state.selected = &input_selected;
            return;
        }
        player.self.message(message);
        return;
    }
    if (type == 3 || type == 0x91 || type == 0x92 || type == 0x93 || type == 0x97 || type == 0x98 || type == 0x9A)
    {
        scene.controls->message(object, message);
        return;
    }
    if (type == 0xF)
    {
        // Getter 0x00066900: +0x10 is 0x00066FE0 and +0xC is 0x00070370.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_motion_blur(updated);
            store_actor_model_animation_view(updated_view);
        };
        // Xbox 0x00070370
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (message[0] == 0x23)
        {
            unregister_actor_segment_probes(view.render);
            clear_actor_shape_probe(view.bat.actor, view.shape);
        }
        return;
    }
    if (type == 0x14)
    {
        // Getter 0x0006C9E0: +0x10 is 0x0006CAF0 (the actor update 0x000420D0 first) and +0xC is 0x00070370.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            classes->update_anim_delete(updated);
        };
        // Xbox 0x00070370
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (message[0] == 0x23)
        {
            unregister_actor_segment_probes(view.render);
            clear_actor_shape_probe(view.bat.actor, view.shape);
        }
        return;
    }
    if (type == 0x1C)
    {
        // Getter 0x0006FCC0: +0x10 is 0x0006FF90 (the actor update 0x000420D0 first) and +0xC is 0x00070370.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            classes->update_scare(updated);
        };
        // Xbox 0x00070370
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (message[0] == 0x23)
        {
            unregister_actor_segment_probes(view.render);
            clear_actor_shape_probe(view.bat.actor, view.shape);
        }
        return;
    }
    if (type == 0x13)
    {
        // Getter 0x0006C260: +0x10 is 0x0006C5A0 (the actor update 0x000420D0 first) and +0xC is 0x0006C9B0, a jump
        // to the actor's 0x0003EF70.
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            update_base_actor(updated); // 0x000420D0
            classes->update_animated_light(updated);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        return;
    }
    if (type == 0x12)
    {
        // Getter 0x00067E60: +0x10 is 0x00068060 and +0xC is 0x00068700 (the base 0x0003EF70 first).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_bone_debris(updated);
            store_actor_model_animation_view(updated_view);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        classes->message_bone_debris(object, message);
        return;
    }
    if (type == 0x10)
    {
        // Getter 0x000690A0: +0x10 is 0x00069250 and +0xC is 0x00069540 (the base 0x0003EF70 first).
        auto& view = actors->view(object);
        const u8* bytes = object.bytes.get();
        actor_updates[&view.bat.actor] = [this, bytes](Actor&)
        {
            auto& updated = *object_of(bytes);
            refresh_shape_poses(updated);
            auto& updated_view = actors->view(updated);
            refresh_actor_model_animation_view(updated_view);
            frame->freeze = pause;
            frame->dt = frame_delta;
            frame->scene_scale = time_scale;
            frame->block_decay = pause_references[1];
            frame->decay_enabled = decay_enabled[decay_index];
            classes->update_afterimage(updated);
            store_actor_model_animation_view(updated_view);
        };
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages); // 0x0003EF70
        classes->message_afterimage(object, message[0]);
        return;
    }
    if (type == 2 || type == 0x15 || type == 0x16)
    {
        if (type == 2 || type == 0x16)
        {
            // Getter 0x0003C080 keeps the base update 0x000420D0 at +0x10; type 0x16's 0x0006DBA0 jumps to it, and its
            // +0xC 0x0006C9B0 to the actor message 0x0003EF70.
            auto& view = actors->view(object);
            const u8* bytes = object.bytes.get();
            actor_updates[&view.bat.actor] = [this, bytes](Actor&)
            {
                update_base_actor(*object_of(bytes));
            };
        }
        dispatch_base_actor_word_message(base_of(object).base, message, word_messages);
        if (type == 0x15 && message[0] == 0x11)
        {
            unregister_actor_render_bins(actors->view(object).render); // 0x0006D8A0's tail: 0x0003CBB0
        }
        return;
    }
    throw Error("scene object message for factory type " + std::to_string(type) + " has no class view");
}

void SceneActorServices::State::actor_message(Actor& actor, const void* message)
{
    for (const auto& [bytes, candidate] : actors->actors)
    {
        if (candidate == &actor)
        {
            auto* object = object_of(bytes);
            if (object == nullptr)
            {
                unbound("message to an actor whose object is still being created");
            }
            object_message(*object, {static_cast<u32*>(const_cast<void*>(message)), 0x42});
            return;
        }
    }
    throw Error("actor message has no scene actor");
}

RuntimeSegmentContactActor& SceneActorServices::State::prop_contact_actor(entity_detail::SegmentProbe& probe)
{
    auto& view = actors->view(probe);
    auto& actor = view.bat.actor;
    auto& slot = prop_contact_views[&actor];
    if (slot == nullptr)
    {
        prop_contact_owners[view.bytes.data()] = &actor;
        slot = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{actor, 0, &view.body_node,
            [this, &actor, bytes = view.bytes.data()](void* payload)
            {
                // The receiver reads a contact pool as its retail image: the header's three addresses, then the
                // records, bound to a scene address for the call.
                std::vector<u8> image;
                const auto bind_pool = [this, &image](const GhoulySpawnPool* pool) -> u32
                {
                    if (pool == nullptr)
                    {
                        return 0;
                    }
                    const size_t used = size_t(pool->cursor - pool->begin);
                    const size_t reserved = size_t(pool->limit - pool->begin);
                    image.resize(0x10 + reserved);
                    std::memcpy(image.data(), &pool->stride, 2);
                    std::memcpy(image.data() + 2, &pool->grow, 2);
                    std::memcpy(image.data() + 0x10, pool->begin, used);
                    const u32 address = scene.bind_address(image);
                    const std::array<u32, 3> addresses{address + 0x10, address + 0x10 + u32(used),
                        address + 0x10 + u32(reserved)};
                    std::memcpy(image.data() + 4, addresses.data(), sizeof(addresses));
                    return address;
                };
                const auto send = [this, bytes, &image](std::span<u32> words, u32 records)
                {
                    try
                    {
                        object_message(*object_of(bytes), words);
                    }
                    catch (...)
                    {
                        if (records != 0)
                        {
                            scene.unbind_address(image.data());
                        }
                        throw;
                    }
                    if (records != 0)
                    {
                        scene.unbind_address(image.data());
                    }
                };
                const u32 id = *static_cast<const u32*>(payload);
                if (id == 0x17 || id == 0x16)
                {
                    // {0x17, reply, sender, contacts, point} from a knock-down sphere (0x0006EC10), or 0x16 in the
                    // same layout from a furnace (0x0006E4D0).
                    auto& notice = *static_cast<RuntimeKnockdownSphereNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = records;
                    words[4] = std::bit_cast<u32>(notice.point.x);
                    words[5] = std::bit_cast<u32>(notice.point.y);
                    words[6] = std::bit_cast<u32>(notice.point.z);
                    send(words, records);
                    notice.reply = words[1];
                    return;
                }
                if (id == 8)
                {
                    // {8, reply, contacts, player, actor} from the knockdown sweep (0x0013FC50).
                    auto& sweep = *static_cast<RuntimeSweepNotice*>(payload);
                    const u32 records = bind_pool(sweep.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = sweep.id;
                    words[1] = sweep.reply;
                    words[2] = records;
                    words[3] = sweep.player;
                    words[4] = sweep.actor;
                    send(words, records);
                    sweep.reply = words[1];
                    return;
                }
                if (id == 0x21)
                {
                    // {0x21, reply, sender, point} from an exploding body's blast (0x0008EBD0).
                    auto& notice = *static_cast<RuntimeBlastNotice*>(payload);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = std::bit_cast<u32>(notice.point.x);
                    words[4] = std::bit_cast<u32>(notice.point.y);
                    words[5] = std::bit_cast<u32>(notice.point.z);
                    send(words, 0);
                    notice.reply = words[1];
                    return;
                }
                if (id == 0x15)
                {
                    // {0x15, reply, sender, velocity, contacts, point} from a player's contact (0x0001F8C0).
                    auto& notice = *static_cast<RuntimeContactNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = std::bit_cast<u32>(notice.velocity.x);
                    words[4] = std::bit_cast<u32>(notice.velocity.y);
                    words[5] = std::bit_cast<u32>(notice.velocity.z);
                    words[6] = records;
                    words[7] = std::bit_cast<u32>(notice.point.x);
                    words[8] = std::bit_cast<u32>(notice.point.y);
                    words[9] = std::bit_cast<u32>(notice.point.z);
                    send(words, records);
                    notice.reply = words[1];
                    return;
                }
                if (id == 0x19)
                {
                    // {0x19, kind, strength, style, reply, 0, 0, weapon, velocity, contacts, point, 0} from the
                    // large projectile's contact test (0x00141A30).
                    auto& notice = *static_cast<RuntimeProjectileHitNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.kind;
                    words[2] = notice.strength;
                    words[3] = notice.style;
                    words[4] = notice.reply;
                    words[5] = notice.result;
                    words[6] = notice.standing;
                    words[7] = address_of(notice.weapon);
                    std::memcpy(words.data() + 8, &notice.velocity, 12);
                    words[0xB] = records;
                    std::memcpy(words.data() + 0xC, &notice.point, 12);
                    words[0xF] = notice.tail;
                    send(words, records);
                    notice.reply = words[4];
                    notice.result = words[5];
                    notice.standing = words[6];
                    return;
                }
                if (id == 0x1D || id == 0x18)
                {
                    // {0x1D, kind, strength, style, reply, weapon, velocity, contacts, point} from a weapon's
                    // contact test (0x00141890), or 0x18 in the same layout from a spray (0x00141C00), as the player
                    // route lays it out.
                    auto& notice = *static_cast<RuntimeWeaponHitNotice*>(payload);
                    const u32 records = bind_pool(notice.contacts);
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.kind;
                    words[2] = notice.strength;
                    words[3] = notice.style;
                    words[4] = notice.reply;
                    words[5] = address_of(notice.weapon);
                    std::memcpy(words.data() + 6, &notice.velocity, 12);
                    words[9] = records;
                    std::memcpy(words.data() + 0xA, &notice.point, 12);
                    send(words, records);
                    notice.reply = words[4];
                    return;
                }
                if (id != 0x1B)
                {
                    unbound("prop contact notice " + std::to_string(id));
                }
                auto& notice = *static_cast<RuntimePropContactNotice*>(payload);
                const u32 contacts = scene.bind_address({reinterpret_cast<u8*>(notice.contacts), sizeof(GhoulySpawnPool)});
                std::array<u32, 8> words{notice.id, notice.reply, address_of(notice.sender), notice.value, contacts,
                    std::bit_cast<u32>(notice.point.x), std::bit_cast<u32>(notice.point.y), std::bit_cast<u32>(notice.point.z)};
                try
                {
                    object_message(*object_of(bytes), words);
                }
                catch (...)
                {
                    scene.unbind_address(notice.contacts);
                    throw;
                }
                notice.reply = words[1];
                scene.unbind_address(notice.contacts);
            }});
    }
    slot->child_link = view.render.child.prev != nullptr ? 1u : 0u;
    return *slot;
}

// A player's actor message with a contact pool word: the pool is bound to a scene address while it runs.
void SceneActorServices::State::dispatch_contact_words(NativePlayerInstance& player, std::span<u32> words,
    GhoulySpawnPool* contacts, size_t contacts_word)
{
    const bool bound = contacts != nullptr && scene.find_address(contacts) == 0;
    if (contacts != nullptr)
    {
        words[contacts_word] = bound ? scene.bind_address({reinterpret_cast<u8*>(contacts), sizeof(GhoulySpawnPool)})
            : scene.find_address(contacts);
    }
    try
    {
        player.actor_message(words);
    }
    catch (...)
    {
        if (bound)
        {
            scene.unbind_address(contacts);
        }
        throw;
    }
    if (bound)
    {
        scene.unbind_address(contacts);
    }
}

// The contact walk's view of the actor whose segment probe it hit: a player (actor +0x934 node, message 0x15
// from 0x0001F8C0) or a prop.
RuntimeSegmentContactActor& SceneActorServices::State::segment_contact_actor(entity_detail::SegmentProbe& probe)
{
    for (auto* player : players->instances())
    {
        if (&player->actor_instance.segment != &probe)
        {
            continue;
        }
        auto& slot = player_contact_views[player];
        if (slot == nullptr)
        {
            slot = std::make_unique<RuntimeSegmentContactActor>(RuntimeSegmentContactActor{player->actor, 0,
                &player->links[0],
                [this, player](void* payload)
                {
                    if (*static_cast<const u32*>(payload) == 8)
                    {
                        // {8, reply, contacts, player, actor} from the knockdown sweep (0x0013FC50).
                        auto& sweep = *static_cast<RuntimeSweepNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = sweep.id;
                        words[1] = sweep.reply;
                        words[3] = sweep.player;
                        words[4] = sweep.actor;
                        dispatch_contact_words(*player, words, sweep.contacts, 2);
                        sweep.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x17 || *static_cast<const u32*>(payload) == 0x16)
                    {
                        // {0x17, reply, sender, contacts, point} from a knock-down sphere (0x0006EC10), or 0x16 in
                        // the same layout from a furnace (0x0006E4D0).
                        auto& notice = *static_cast<RuntimeKnockdownSphereNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.reply;
                        words[2] = address_of(notice.sender);
                        words[4] = std::bit_cast<u32>(notice.point.x);
                        words[5] = std::bit_cast<u32>(notice.point.y);
                        words[6] = std::bit_cast<u32>(notice.point.z);
                        dispatch_contact_words(*player, words, notice.contacts, 3);
                        notice.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x1B)
                    {
                        // {0x1B, reply, sender, value, contacts, point} from a contact prop's shapes (0x0006EB00).
                        auto& notice = *static_cast<RuntimePropContactNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.reply;
                        words[2] = address_of(notice.sender);
                        words[3] = notice.value;
                        words[5] = std::bit_cast<u32>(notice.point.x);
                        words[6] = std::bit_cast<u32>(notice.point.y);
                        words[7] = std::bit_cast<u32>(notice.point.z);
                        dispatch_contact_words(*player, words, notice.contacts, 4);
                        notice.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x19)
                    {
                        // {0x19, kind, strength, style, reply, 0, 0, weapon, velocity, contacts, point, 0} from the
                        // large projectile's contact test (0x00141A30).
                        auto& notice = *static_cast<RuntimeProjectileHitNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.kind;
                        words[2] = notice.strength;
                        words[3] = notice.style;
                        words[4] = notice.reply;
                        words[5] = notice.result;
                        words[6] = notice.standing;
                        words[7] = address_of(notice.weapon);
                        std::memcpy(words.data() + 8, &notice.velocity, 12);
                        std::memcpy(words.data() + 0xC, &notice.point, 12);
                        words[0xF] = notice.tail;
                        dispatch_contact_words(*player, words, notice.contacts, 0xB);
                        notice.reply = words[4];
                        notice.result = words[5];
                        notice.standing = words[6];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x1D || *static_cast<const u32*>(payload) == 0x18)
                    {
                        // {0x1D, kind, strength, style, reply, weapon, velocity, contacts, point} from a weapon's
                        // contact test (0x00141890), or 0x18 in the same layout from a spray (0x00141C00).
                        auto& notice = *static_cast<RuntimeWeaponHitNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.kind;
                        words[2] = notice.strength;
                        words[3] = notice.style;
                        words[4] = notice.reply;
                        words[5] = address_of(notice.weapon);
                        std::memcpy(words.data() + 6, &notice.velocity, 12);
                        std::memcpy(words.data() + 0xA, &notice.point, 12);
                        dispatch_contact_words(*player, words, notice.contacts, 9);
                        notice.reply = words[4];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x21)
                    {
                        // {0x21, reply, sender, point} from an exploding body's blast (0x0008EBD0).
                        auto& notice = *static_cast<RuntimeBlastNotice*>(payload);
                        std::array<u32, 0x42> words{};
                        words[0] = notice.id;
                        words[1] = notice.reply;
                        words[2] = address_of(notice.sender);
                        words[3] = std::bit_cast<u32>(notice.point.x);
                        words[4] = std::bit_cast<u32>(notice.point.y);
                        words[5] = std::bit_cast<u32>(notice.point.z);
                        dispatch_contact_words(*player, words, nullptr, 0);
                        notice.reply = words[1];
                        return;
                    }
                    if (*static_cast<const u32*>(payload) == 0x4F)
                    {
                        // {0x4F, 0xC} from a flat spider web (0x00070780): plain words, so the player's vtable +0xC
                        // takes the 0x42-word message as it is.
                        player->self.message(std::span<const u32>(static_cast<const u32*>(payload), 0x42));
                        return;
                    }
                    if (*static_cast<const u32*>(payload) != 0x15)
                    {
                        unbound("player contact payload " + std::to_string(*static_cast<const u32*>(payload)));
                    }
                    auto& notice = *static_cast<RuntimeContactNotice*>(payload);
                    // {0x15, reply, sender, velocity, contacts, point}.
                    std::array<u32, 0x42> words{};
                    words[0] = notice.id;
                    words[1] = notice.reply;
                    words[2] = address_of(notice.sender);
                    words[3] = std::bit_cast<u32>(notice.velocity.x);
                    words[4] = std::bit_cast<u32>(notice.velocity.y);
                    words[5] = std::bit_cast<u32>(notice.velocity.z);
                    words[7] = std::bit_cast<u32>(notice.point.x);
                    words[8] = std::bit_cast<u32>(notice.point.y);
                    words[9] = std::bit_cast<u32>(notice.point.z);
                    dispatch_contact_words(*player, words, notice.contacts, 6);
                    notice.reply = words[1];
                }});
        }
        slot->child_link = player->actor_instance.render.child.prev != nullptr ? 1u : 0u;
        return *slot;
    }
    return prop_contact_actor(probe);
}

// Base actor vtable +0x10 (0x000420D0) over the scene's frame context.
void SceneActorServices::State::update_base_actor(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    update_actor_frame(view.frame, *frame);
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::update_knockdown_sphere(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor =
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return segment_contact_actor(probe);
        };
    RuntimeKnockdownSphereContacts contacts{contact_actor, entity_detail::g_world.body_a,
        [this]()
        {
            return marked_players();
        }};
    update_runtime_knockdown_sphere({object.bytes.get(), object.size}, view.prop_contacts, view.frame, *frame, clock,
        contacts);
    const u32 pool = view.prop_contacts != nullptr ? heap_address(view.prop_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x934, &pool, 4);
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::update_furnace(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor =
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return segment_contact_actor(probe);
        };
    RuntimeFurnaceContacts contacts{contact_actor, entity_detail::g_world.body_a,
        [this]()
        {
            return marked_players();
        },
        clock, view.startup.resource.resource.data(), base_of(object).base.callouts};
    update_runtime_furnace(view.prop_contacts, view.furnace_contacts, view.frame, *frame, contacts); // 0x0006E260
    const u32 burns = view.prop_contacts != nullptr ? heap_address(view.prop_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x934, &burns, 4);
    const u32 warmths = view.furnace_contacts != nullptr
        ? heap_address(view.furnace_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x938, &warmths, 4);
    store_actor_model_animation_view(view);
}

std::vector<RuntimeSegmentContactActor*> SceneActorServices::State::marked_players()
{
    std::vector<RuntimeSegmentContactActor*> marked;
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.mark.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (&player->links[0] == node)
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B38 node without a native player");
        }
        marked.push_back(&segment_contact_actor(owner->actor_instance.segment));
    }
    return marked;
}

void SceneActorServices::State::update_spider_web(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    RuntimeSpiderWeb web{{object.bytes.get(), object.size}, view.bat.actor, view.animation, resources.registry,
        resources.error, random};
    RuntimeSpiderWebUpdate update{clock,
        [this, &view]()
        {
            update_actor_frame(view.frame, *frame); // 0x000420D0
        },
        [this]()
        {
            return marked_players();
        }};
    update_runtime_spider_web(web, update); // 0x000705B0
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::update_contact_prop(FactoryObject& object)
{
    refresh_shape_poses(object);
    auto& view = actors->view(object);
    refresh_actor_model_animation_view(view);
    frame->freeze = pause;
    frame->dt = frame_delta;
    frame->scene_scale = time_scale;
    frame->block_decay = pause_references[1];
    frame->decay_enabled = decay_enabled[decay_index];
    const std::function<RuntimeSegmentContactActor&(entity_detail::SegmentProbe&)> contact_actor =
        [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
        {
            return prop_contact_actor(probe);
        };
    RuntimeActorShapeContacts contacts{contact_actor, entity_detail::g_world.body_a};
    update_runtime_contact_prop({object.bytes.get(), object.size}, view.prop_contacts,
        view.frame, *frame, clock, contacts);
    const u32 pool = view.prop_contacts != nullptr ? heap_address(view.prop_contacts, sizeof(GhoulySpawnPool)) : 0;
    std::memcpy(object.bytes.get() + 0x934, &pool, 4);
    store_actor_model_animation_view(view);
}

void SceneActorServices::State::player_controller_message(GhoulyController& controller, void* data)
{
    for (auto* player : players->instances())
    {
        if (&player->controller != &controller)
        {
            continue;
        }
        // As the setups': the scheduler's 0xA7 / 0xA9 / 0xAA requests are shorter than a full message.
        auto* words = static_cast<u32*>(data);
        size_t count = 0x42;
        if (words[0] == 0xA7)
        {
            count = sizeof(GhoulySpawnMessage) / 4;
        }
        else if (words[0] == 0xA9 || words[0] == 0xAA)
        {
            count = sizeof(GhoulyVariantMessage) / 4;
        }
        object_message(object_of_actor(player->actor), {words, count});
        return;
    }
    unbound("ghouly spawn controller that is no player's");
}

void SceneActorServices::State::attach_player_messages(FactoryObject& object, NativePlayerInstance& player)
{
    player.controller.message = [](GhoulyController& controller, void* data)
    {
        active_services->player_controller_message(controller, data);
    };
    auto& slot = player_messages[object.bytes.get()];
    auto& instance = player.actor_instance;
    slot = std::make_unique<ScenePlayerMessages>(
        RuntimePlayerActivation{player.self, player.actor, player.links, entity_detail::g_world,
            [this](const void* pointer)
            {
                return address_of(pointer);
            },
            excluded_count, excluded_tags.data(), player.room, player.allowed, player.ring, address_zero, overrides},
        RuntimePlayerPause{player.actor, player.strategy_object, player.strategy_setup, query, player.goal_cleanup,
            resume},
        RuntimePlayerNavigationRegistration{player.body, player.actor, player.navigation}, player,
        base_of(object).base,
        RuntimePlayerChildMessages{player.strategy_object, player.mind_object, player.body_object,
            [this](FactoryObject& child, void* message)
            {
                try
                {
                    child_message(child, message);
                }
                catch (const std::exception& error)
                {
                    throw Error("player child type " + std::to_string(child.type) + " message "
                        + std::to_string(*static_cast<const u32*>(message)) + ": " + error.what());
                }
            }});
    auto* messages = slot.get();
    messages->context.event_at = [this](u32 address) -> std::span<const u8>
    {
        return scene.resolve_tail(address);
    };
    // 0x0003B3C0: an animation event becomes message {7, position, slot, event, phase} to actor vtable +0xC.
    instance.animation.message_handler = [this, &player](const ActorAnimationMessage& message)
    {
        auto* event = const_cast<u8*>(message.event.data());
        u32 address = scene.find_address(event);
        const bool bound = address == 0;
        if (bound)
        {
            address = scene.bind_address({event, message.event.size()});
        }
        std::array<u32, 0x42> words{};
        words[0] = 7;
        words[1] = std::bit_cast<u32>(message.position);
        words[2] = message.slot;
        words[3] = address;
        words[4] = u32(message.phase);
        try
        {
            player.actor_message(words);
        }
        catch (...)
        {
            if (bound)
            {
                scene.unbind_address(event);
            }
            throw;
        }
        if (bound)
        {
            scene.unbind_address(event);
        }
    };
    player.actor_message = [this, messages, &player](std::span<const u32> words)
    {
        // Actor +0x50 is read when the message runs; the controller can be created after attachment.
        messages->context.animation = player.actor_instance.animation.controller.get();
        dispatch_runtime_player_message(const_cast<u32*>(words.data()), messages->context, messages->children);
        input_state.devices = input_activation.entries;
        input_state.selected = &input_selected;
    };
}

SceneBodyMessages& SceneActorServices::State::body_graph(NativePlayerInstance& player)
{
    player.refresh_views();
    auto& graph = body_messages[player.body.data()];
    if (graph == nullptr)
    {
        graph = std::make_unique<SceneBodyMessages>(*body_services, player);
    }
    return *graph;
}

// Child vtable +0xC of a player's strategy, mind or body.
void SceneActorServices::State::child_message(FactoryObject& child, void* message)
{
    const std::span<const u32> words{static_cast<const u32*>(message), 0x42};
    // A strategy's goals read the body's vtable slots (body_slot_key), which a just-spawned player's body graph
    // has not attached yet.
    if (child.type == 0x4F || child.type == 0x46 || child.type == 0x3B || child.type == 0x51 || child.type == 0x52
        || child.type == 0x58 || child.type == 0x44 || child.type == 0x40 || child.type == 0x4C || child.type == 0x53
        || child.type == 0x42 || child.type == 0x3A || child.type == 0x47 || child.type == 0x48 || child.type == 0x49 || child.type == 0x55 || child.type == 0x50 || child.type == 0x3E || child.type == 0x41 || child.type == 0x43 || child.type == 0x4B
        || child.type == 0x4E || child.type == 0x37 || child.type == 0x38 || child.type == 0x45 || child.type == 0x54 || child.type == 0x57)
    {
        if (auto* player = players->find_player(child); player != nullptr && !player->body.empty())
        {
            body_graph(*player);
        }
    }
    switch (child.type)
    {
    case 0x4F:
        players->strategy_message(child, words);
        return;
    case 0x46:
        players->imp_fire_message(child, words);
        return;
    case 0x3B:
        players->butler_message(child, words);
        return;
    case 0x51:
        players->skeleton_message(child, words);
        return;
    case 0x58:
        players->zombie_message(child, words);
        return;
    case 0x52:
        players->spider_message(child, words);
        return;
    case 0x44:
        players->haunted_tv_message(child, words);
        return;
    case 0x40:
        players->haunted_chair_message(child, words);
        return;
    case 0x3C:
        players->generic_message(child, words);
        return;
    case 0x3D:
        players->ghoulie_girl_message(child, words);
        return;
    case 0x3F:
        players->grim_reaper_message(child, words);
        return;
    case 0x4C:
        players->mummy_message(child, words);
        return;
    case 0x54:
        players->vampire_message(child, words);
        return;
    case 0x57:
        players->worm_message(child, words);
        return;
    case 0x53:
        players->ugly_girl_message(child, words);
        return;
    case 0x50:
        players->ribs_message(child, words);
        return;
    case 0x3E:
        players->giant_hand_message(child, words);
        return;
    case 0x41:
        players->haunted_coat_message(child, words);
        return;
    case 0x43:
        players->haunted_picture_message(child, words);
        return;
    case 0x56:
        players->warlock_message(child, words);
        return;
    case 0x42:
        players->haunted_door_message(child, words);
        return;
    case 0x47:
        players->imp_flamethrower_message(child, words);
        return;
    case 0x3A:
        players->chicken_message(child, words);
        return;
    case 0x48:
        players->imp_flying_message(child, words);
        return;
    case 0x55:
        players->vampire_chicken_message(child, words);
        return;
    case 0x49:
        players->imp_ninja_message(child, words);
        return;
    case 0x4A:
        players->jessie_clyde_message(child, words);
        return;
    case 0x4D:
        players->mummy_cursed_message(child, words);
        return;
    case 0x4B:
        players->medusa_message(child, words);
        return;
    case 0x4E:
        players->pirate_message(child, words);
        return;
    case 0x37:
        players->baron_message(child, words);
        return;
    case 0x38:
        players->baron_plane_message(child, words);
        return;
    case 0x45:
        players->hunchback_message(child, words);
        return;
    case 0x39:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        RuntimeDirectorMessages director{child, player->actor, player->mind, player->strategy_setup,
            player->strategy_messages,
            [this](Vec3& position, Vec3& euler)
            {
                return read_runtime_marker_camera_start(position, euler, camera_start);
            },
            [this, player]()
            {
                update_player(*player);
            }};
        dispatch_runtime_director_message(words, director);
        return;
    }
    case 0x34:
    case 0x35:
    {
        auto& slots = players->minds.view(child);
        auto* player = players->find_player(child);
        player->refresh_views();
        auto& graph = mind_messages[child.bytes.get()];
        if (graph == nullptr)
        {
            graph = std::make_unique<SceneMindMessages>(*mind_services, *player, slots);
        }
        graph->refresh();
        players->minds.message(child, message);
        return;
    }
    case 0x36:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        const u32 target = mind_record(player->mind).target_record;
        if (words[0] == 0x7D && navigation_nodes == nullptr)
        {
            unbound("navigation node buffer DAT_0050E24C for 0x00136700");
        }
        RuntimeCameraMindMessages messages{player->mind_events, player->mind,
            target == 0 ? std::span<const u8>() : std::span<const u8>(scene.resolve_address(target, 0x24)),
            i32(player_record(player->actor_bytes).floor), g_nav_grids, navigation_nodes, player->mind_storage,
            [this](const void* pointer)
            {
                return address_of(pointer);
            }};
        dispatch_runtime_camera_mind_message(words, messages);
        return;
    }
    case 0x27:
    case 0x28:
    case 0x29:
    case 0x2A:
    case 0x2B:
    case 0x2C:
    case 0x2D:
    {
        auto* player = players->find_player(child);
        player->refresh_views();
        auto& graph = body_messages[child.bytes.get()];
        if (graph == nullptr)
        {
                graph = std::make_unique<SceneBodyMessages>(*body_services, *player);
        }
        try
        {
            graph->dispatch(child, {static_cast<u32*>(message), 0x42});
        }
        catch (const std::exception& error)
        {
            throw Error("body type " + std::to_string(child.type) + " message "
                + std::to_string(words[0]) + ": " + error.what());
        }
        return;
    }
    default:
        throw Error("scene actor service is not bound: child message slot of factory type "
            + std::to_string(child.type));
    }
}

// The host latch mirrors an actor's +0x900 node; 0x0003EBA0 takes it off DAT_00511AA4 before the rest of the
// release, so later broadcasts during that release skip the actor.
void SceneActorServices::State::unlink_active_node(const u8* bytes)
{
    const auto found = active_nodes.find(scene.address_of(bytes));
    if (found == active_nodes.end())
    {
        return;
    }
    auto** link = &latches;
    while (*link != nullptr && *link != found->second.get())
    {
        link = &(*link)->next;
    }
    if (*link != nullptr)
    {
        *link = (*link)->next;
    }
    found->second->next = nullptr;
}

// Active-list adapter: the native Message carries the actor argument the retail words hold at +4.
void SceneActorServices::State::latch_message(entity_detail::LatchNode& node, const Message& message)
{
    for (const auto& [address, latch] : active_nodes)
    {
        if (latch.get() != &node)
        {
            continue;
        }
        u32 words[0x42]{};
        words[0] = u32(message.id);
        if (message.arg != nullptr)
        {
            const auto* argument = static_cast<const Actor*>(message.arg);
            bool found = false;
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (actor == argument)
                {
                    words[1] = scene.address_of(bytes);
                    found = true;
                }
            }
            if (!found)
            {
                throw Error("active-list message argument is not a scene actor");
            }
        }
        else
        {
            words[1] = u32(message.arg_i);
        }
        auto* object = object_of(scene.resolve_address(address, 0).data());
        object_message(*object, words);
        return;
    }
    throw Error("active-list node has no scene actor");
}

// Scene startup after the marker pass: 0x00123070 activates every pending actor.
void SceneActorServices::State::activate_actors()
{
    auto activation = activation_context();
    activate_runtime_scene_actors(activation);
}

RuntimeSceneActivation SceneActorServices::State::activation_context()
{
    return RuntimeSceneActivation{live_actors, latches, scene_nodes, scene_controls,
        [this](RuntimeActorLink& node)
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (&actors->view(*object_of(bytes)).startup.live == &node)
                {
                    return scene.address_of(bytes);
                }
            }
            throw Error("pending actor node has no scene actor");
        },
        [this](RuntimeActorLink& node) -> entity_detail::LatchNode&
        {
            for (const auto& [bytes, actor] : actors->actors)
            {
                if (&actors->view(*object_of(bytes)).startup.live == &node)
                {
                    const u32 address = scene.address_of(bytes);
                    auto& latch = active_nodes[address];
                    latch = std::make_unique<entity_detail::LatchNode>();
                    latch->tag = read_u16({bytes, 8}, 6);
                    latch->handle_message = [](entity_detail::LatchNode& self, const Message& message)
                    {
                        active_services->latch_message(self, message);
                    };
                    return *latch;
                }
            }
            throw Error("pending actor node has no scene actor");
        },
        [this](u32 address, std::span<const u32> words)
        {
            auto* object = object_of(scene.resolve_address(address, 0).data());
            try
            {
                object_message(*object, {const_cast<u32*>(words.data()), words.size()});
            }
            catch (const std::exception& error)
            {
                throw Error("activation actor " + std::to_string(address) + " type "
                    + std::to_string(object->type) + " message " + std::to_string(words[0])
                    + ": " + error.what());
            }
        },
        [this](u32 address) -> const Actor*
        {
            return actor_or_retired(address);
        }};
}

RuntimeDirectorActor SceneActorServices::State::director_actor(NativePlayerInstance& player)
{
    player.refresh_views();
    return RuntimeDirectorActor{player.actor_address, &player.actor, player.actor_bytes, player.strategy, player.mind,
        player.body, player.body_bits};
}

RuntimeBodyAttachedActor& SceneActorServices::State::attached_actor_at(u32 address)
{
    if (auto* player = players->find_player_at(address); player != nullptr)
    {
        return player->self;
    }
    if (const auto released = released_actors.find(address); released != released_actors.end())
    {
        // A sender released since (a knock-down sphere outlives its owner): retail reads the freed actor's memory.
        auto& view = weapon_attachments[address];
        if (view == nullptr)
        {
            view = std::make_unique<RuntimeBodyAttachedActor>();
        }
        view->bytes = released->second.bytes;
        view->address = address;
        view->message = [](std::span<const u32> message)
        {
            unbound("message " + std::to_string(message.empty() ? 0u : message[0])
                + " to a released actor (retail calls the freed actor's vtable)");
        };
        view->actor = nullptr;
        view->host = nullptr;
        return *view;
    }
    const auto bytes = scene.resolve_address(address, 0);
    FactoryObject* object = object_of(bytes.data());
    // A weapon, a knock-down sphere (type 0x19) as the sender of its contact message 0x17, or a furnace (type 0x17)
    // as the sender of 0x16, or a background (type 4) as the source of the knock-back hit 0x00020690.
    if (object == nullptr || (object->type != 0x0B && object->type != 4 && object->type != 0x19 && object->type != 0x17 && object->type != 0xAA
        && object->type != 0xA9 && object->type != 0xA8
        && object->type != 0xA7 && object->type != 0xA0 && object->type != 0xA1 && object->type != 0xA2
        && object->type != 0xA3 && object->type != 0xA5 && object->type != 0xA6))
    {
        unbound("attached actor at an address that is neither a player nor a weapon (factory type "
            + (object == nullptr ? std::string("none") : std::to_string(object->type)) + ")");
    }
    auto& view = weapon_attachments[address];
    if (view == nullptr)
    {
        view = std::make_unique<RuntimeBodyAttachedActor>();
    }
    view->bytes = {object->bytes.get(), object->size};
    view->address = address;
    view->message = [this, address](std::span<const u32> message)
    {
        std::array<u32, 0x42> words{};
        std::copy_n(message.begin(), std::min(message.size(), words.size()), words.begin());
        object_message(*object_of(scene.resolve_address(address, 0).data()), words);
    };
    view->actor = actors->actors.at(object->bytes.get());
    // Actor +0xD8C (Actor::spawn_owner): the holding player's own view, when the weapon is held.
    view->host = nullptr;
    for (auto* player : players->instances())
    {
        if (&player->actor == view->actor->spawn_owner)
        {
            view->host = &player->self;
        }
    }
    return *view;
}

RuntimePlayerAttachmentActor& SceneActorServices::State::weapon_attachment_at(u32 address)
{
    auto& object = attached_actor_at(address);
    if (players->find_player_at(address) != nullptr)
    {
        unbound("weapon pick-up views of a player");
    }
    auto& view = weapon_pickups[address];
    if (view == nullptr)
    {
        view = std::make_unique<WeaponPickupView>();
    }
    view->receiver.message = [this, address](const RuntimeBodyAnimationNotice& notice)
    {
        // Message 0x28 {id, actor, record, 0, position}. The holder's record goes as a bound word for the
        // weapon's animation table lookup (0x00144B80) and is released after the message.
        std::array<u32, 0x42> words{};
        words[0] = notice.id;
        words[1] = notice.actor;
        words[3] = notice.zero;
        std::memcpy(&words[4], &notice.position, 4);
        auto* record = const_cast<AnimationRecord*>(notice.record);
        const bool bind = record != nullptr && scene.find_address(record) == 0;
        if (record != nullptr)
        {
            words[2] = bind ? scene.bind_address({reinterpret_cast<u8*>(record), sizeof(AnimationRecord)})
                            : scene.find_address(record);
        }
        try
        {
            object_message(*object_of(scene.resolve_address(address, 0).data()), words);
        }
        catch (...)
        {
            if (bind)
            {
                scene.unbind_address(record);
            }
            throw;
        }
        if (bind)
        {
            scene.unbind_address(record);
        }
    };
    auto& instance = actors->view(*object_of(scene.resolve_address(address, 0).data()));
    view->pose.emplace(RuntimePlayerAttachmentActor{object, instance.bat.actor, view->receiver, instance.animation,
        instance.pose, instance.startup.bone, nullptr, node_cache, reset_matrix});
    attachment_actors[&object] = &*view->pose;
    return *view->pose;
}

std::vector<RuntimeDirectorActor> SceneActorServices::State::marked_actors()
{
    std::vector<RuntimeDirectorActor> marks;
    for (auto* node = static_cast<entity_detail::ListHead*>(entity_detail::g_world.mark.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        NativePlayerInstance* owner = nullptr;
        for (auto* player : players->instances())
        {
            if (&player->links[0] == node)
            {
                owner = player;
            }
        }
        if (owner == nullptr)
        {
            unbound("DAT_00511B38 node without a native player");
        }
        marks.push_back(director_actor(*owner));
    }
    return marks;
}

RuntimeActorPauseGroups& SceneActorServices::State::refresh_pause_groups()
{
    if (pause_walking)
    {
        // A walk is delivering 0x26/0x27: a nested 0x00117400 walks the same nodes.
        return pause_groups;
    }
    pause_nodes.clear();
    for (const auto& mark : marked_actors())
    {
        const u32 address = mark.address;
        pause_nodes.push_back(RuntimePauseActor{nullptr, mark.bytes, [this, address](u32 id)
        {
            std::array<u32, 0x42> words{id};
            object_message(*object_of(scene.resolve_address(address, 0).data()), words);
        }});
    }
    for (size_t index = 0; index + 1 < pause_nodes.size(); ++index)
    {
        pause_nodes[index].next = &pause_nodes[index + 1];
    }
    first_pause = pause_nodes.empty() ? nullptr : &pause_nodes.front();
    return pause_groups;
}
} // namespace gbtg
