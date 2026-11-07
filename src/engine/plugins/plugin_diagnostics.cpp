#if !defined(GBTG_RETAIL_GAME_ONLY)
// The host table's read-only diagnostics (include/gbtg/plugin_api.h), which the debug UI plugin shows.
#include "plugin_libraries.h"

#include "../entities/actor_factory_owner.h"
#include "../entities/actor_fields.h"
#include "../scene/factory_scene_runtime.h"
#include "../scene/scene_actor_services_state.h"
#include "object_schemas.h"
#include "gbtg/engine_internal.h"
#include "gbtg/game.h"
#include "gbtg/gfx.h"

#include <cstring>
#include <string>
#include <vector>

namespace gbtg
{
namespace
{
const Game& game_of(const GbtgGame* game)
{
    return *reinterpret_cast<const Game*>(game);
}

void host_runtime_info(const GbtgGame* game, GbtgRuntimeInfo* out)
{
    if (game == nullptr || out == nullptr || out->size < sizeof(uint32_t))
    {
        return;
    }
    const Game& state = game_of(game);
    // requested_script need not end in a zero inside its array.
    static std::string requested;
    requested.assign(state.requested_script, strnlen(state.requested_script, sizeof(state.requested_script)));

    GbtgRuntimeInfo info{};
    info.size = sizeof(info);
    info.scene_name = state.scene_name.c_str();
    info.frame = state.frame;
    info.clock_frame = state.clock.frame;
    info.clock_time = state.clock.time;
    info.dt = state.clock.dt;
    info.dt_raw = state.clock.dt_raw;
    info.target_hz = clock_fps(state.clock);
    info.pal = state.clock.pal ? 1 : 0;
    info.script_phase = state.script_phase;
    info.script_hold = state.script_hold ? 1 : 0;
    info.requested_script = requested.c_str();
    info.presenter = gfx_backend_name();
    gfx_size(info.width, info.height);
    info.last_missing_resource = "";
    if (state.factory_scene)
    {
        const auto& scene = *state.factory_scene;
        info.has_native_scene = 1;
        info.actor_start = scene.actor_start;
        info.native_objects = scene.objects.size();
        info.registered_objects = scene.factory.count;
        info.address_bindings = scene.address_count();
        info.loading = scene.resources.loading;
        info.last_missing_resource = scene.resources.last_missing.c_str();
    }
    for (const auto& slot : state.clock.delayed)
    {
        if (slot.fn != nullptr)
        {
            ++info.delayed_active;
        }
    }
    info.delayed_slots = state.clock.delayed.size();
    info.cached_bundles = state.aids.bundles.size();
    info.demand_resources = state.aids.demand_resources.size();

    // A plugin built against an older header gets the members it knows.
    const uint32_t size = (std::min)(out->size, uint32_t(sizeof(info)));
    std::memcpy(out, &info, size);
    out->size = size;
}

void host_native_objects(const GbtgGame* game, void (*visit)(void*, const GbtgNativeObject*), void* context)
{
    if (game == nullptr || visit == nullptr || !game_of(game).factory_scene)
    {
        return;
    }
    const auto& scene = *game_of(game).factory_scene;
    for (const auto& object : scene.objects)
    {
        const GbtgNativeObject entry{scene.find_address(object.bytes.get()), uint32_t(object.type), uint32_t(object.size),
            object.vtable ? object.vtable->address : 0};
        visit(context, &entry);
    }
}

void host_delayed_callbacks(const GbtgGame* game, void (*visit)(void*, size_t, uint32_t), void* context)
{
    if (game == nullptr || visit == nullptr)
    {
        return;
    }
    const auto& delayed = game_of(game).clock.delayed;
    for (size_t i = 0; i < delayed.size(); ++i)
    {
        if (delayed[i].fn != nullptr)
        {
            visit(context, i, delayed[i].frames_left);
        }
    }
}

void host_resource_bundles(const GbtgGame* game, void (*visit)(void*, const GbtgBundleInfo*), void* context)
{
    if (game == nullptr || visit == nullptr)
    {
        return;
    }
    const auto row = [&](const char* name, const Bundle& bundle)
    {
        const GbtgBundleInfo info{name, bundle.entries.size(), bundle.cpu_heap.size(), bundle.gpu_heap.size()};
        visit(context, &info);
    };
    const AidStore& aids = game_of(game).aids;
    row("common", aids.common);
    for (const auto& [name, bundle] : aids.bundles)
    {
        row(name.c_str(), bundle);
    }
}

size_t host_scene_count(const GbtgGame* game)
{
    return game != nullptr ? game_of(game).scene_order.size() : 0;
}

const char* host_scene_at(const GbtgGame* game, size_t index)
{
    if (game == nullptr || index >= game_of(game).scene_order.size())
    {
        return "";
    }
    return game_of(game).scene_order[index].c_str();
}

void host_request_scene(GbtgGame* game, const char* name)
{
    if (game != nullptr && name != nullptr && *name != '\0')
    {
        script_request(*reinterpret_cast<Game*>(game), name);
    }
}

const FactoryObject* object_at(const SceneFactoryRuntime& scene, u32 address)
{
    for (const auto& object : scene.objects)
    {
        if (scene.find_address(object.bytes.get()) == address)
        {
            return &object;
        }
    }
    return nullptr;
}

// A type's name string sits in the factory table's previous row: "entityStrategyBaronPlane", type 0x38's, is row
// 0x37's +8 word (see baron_plane_strategy.h, body_drone_appear_state.h).
const char* retail_type_name(const SceneFactoryRuntime& scene, u32 type)
{
    if (type == 0 || type > scene.retail_names.size() || scene.retail_names[type - 1] == 0 || !scene.executable)
    {
        return "";
    }
    const char* name = scene.executable->string(scene.retail_names[type - 1]);
    return name != nullptr ? name : "";
}

// The type's own struct, else its nearest parent's that fits in `size` bytes.
const ObjectSchema* schema_for(const SceneFactoryRuntime& scene, u32 type, u32 size)
{
    for (int guard = 0; guard < 16 && type < scene.types.size(); ++guard)
    {
        const ObjectSchema* schema = object_schema_for_type(type);
        if (schema != nullptr && schema->size <= size)
        {
            return schema;
        }
        const u32 parent = scene.types[type].parent;
        if (parent == type || type == 0)
        {
            break;
        }
        type = parent;
    }
    return nullptr;
}

const Actor* actor_of(const FactoryObject& object)
{
    if (active_services == nullptr || !active_services->actors)
    {
        return nullptr;
    }
    const auto& actors = active_services->actors->actors;
    const auto found = actors.find(object.bytes.get());
    return found != actors.end() ? found->second : nullptr;
}

int host_object_view(const GbtgGame* game, uint32_t address, GbtgObjectView* out)
{
    if (game == nullptr || out == nullptr || out->size < sizeof(uint32_t) || !game_of(game).factory_scene)
    {
        return 0;
    }
    const auto& scene = *game_of(game).factory_scene;
    const FactoryObject* object = object_at(scene, address);
    if (object == nullptr)
    {
        return 0;
    }
    const ObjectSchema* schema = schema_for(scene, object->type, object->size);
    GbtgObjectView view{};
    view.size = sizeof(view);
    view.address = address;
    view.type = object->type;
    view.parent_type = object->type < scene.types.size() ? scene.types[object->type].parent : 0;
    view.bytes = object->size;
    view.vtable = object->vtable != nullptr ? object->vtable->address : 0;
    view.retail_name = retail_type_name(scene, object->type);
    view.schema = schema != nullptr ? schema->name : "";
    view.data = object->bytes.get();
    view.is_actor = actor_of(*object) != nullptr ? 1 : 0;
    const uint32_t size = (std::min)(out->size, uint32_t(sizeof(view)));
    std::memcpy(out, &view, size);
    out->size = size;
    return 1;
}

struct FieldWalk
{
    const u8* data;
    u32 limit;
    void (*visit)(void*, const GbtgField*);
    void* context;
};

void visit_gap(FieldWalk& walk, u32 from, u32 to, u32 depth)
{
    if (to <= from || from >= walk.limit)
    {
        return;
    }
    to = (std::min)(to, walk.limit);
    const GbtgField row{"", "", "", from, to - from, GBTG_FIELD_BYTES, to - from, depth, walk.data + from};
    walk.visit(walk.context, &row);
}

void walk_schema(FieldWalk& walk, const ObjectSchema& schema, u32 base, u32 depth)
{
    u32 cursor = base;
    for (const ObjectSchemaField& field : schema.fields)
    {
        const u32 offset = base + field.offset;
        if (offset + field.size > walk.limit)
        {
            break;
        }
        visit_gap(walk, cursor, offset, depth);
        const GbtgField row{field.name, field.note, field.nested != nullptr ? field.nested->name : "", offset,
            field.size, field.kind, field.count, depth, walk.data + offset};
        walk.visit(walk.context, &row);
        if (field.kind == GBTG_FIELD_GROUP && field.nested != nullptr)
        {
            if (field.count == 1)
            {
                walk_schema(walk, *field.nested, offset, depth + 1);
            }
            else
            {
                for (u32 i = 0; i < field.count; ++i)
                {
                    const u32 at = offset + i * field.nested->size;
                    const std::string label = "[" + std::to_string(i) + "]";
                    const GbtgField element{label.c_str(), "", field.nested->name, at, field.nested->size,
                        GBTG_FIELD_GROUP, 1, depth + 1, walk.data + at};
                    walk.visit(walk.context, &element);
                    walk_schema(walk, *field.nested, at, depth + 2);
                }
            }
        }
        cursor = (std::max)(cursor, offset + field.size);
    }
    visit_gap(walk, cursor, base + schema.size, depth);
}

void host_object_fields(const GbtgGame* game, uint32_t address, const char* schema_name,
    void (*visit)(void*, const GbtgField*), void* context)
{
    if (game == nullptr || visit == nullptr || !game_of(game).factory_scene)
    {
        return;
    }
    const auto& scene = *game_of(game).factory_scene;
    const FactoryObject* object = object_at(scene, address);
    if (object == nullptr)
    {
        return;
    }
    const ObjectSchema* schema = schema_name != nullptr && *schema_name != '\0' ? find_object_schema(schema_name)
        : schema_for(scene, object->type, object->size);
    FieldWalk walk{object->bytes.get(), object->size, visit, context};
    u32 end = 0;
    if (schema != nullptr)
    {
        walk_schema(walk, *schema, 0, 0);
        end = schema->size;
    }
    // What the struct does not cover: a derived type's own bytes past its parent's struct.
    visit_gap(walk, end, object->size, 0);
}

const char* host_type_name(const GbtgGame* game, uint32_t type)
{
    if (game == nullptr || !game_of(game).factory_scene)
    {
        return "";
    }
    return retail_type_name(*game_of(game).factory_scene, type);
}

const char* host_type_schema(const GbtgGame* game, uint32_t type)
{
    if (game == nullptr || !game_of(game).factory_scene)
    {
        const ObjectSchema* own = object_schema_for_type(type);
        return own != nullptr ? own->name : "";
    }
    const ObjectSchema* schema = schema_for(*game_of(game).factory_scene, type, 0xFFFFFFFFu);
    return schema != nullptr ? schema->name : "";
}

size_t host_schema_count()
{
    return object_schemas().size();
}

const char* host_schema_at(size_t index, uint32_t* size)
{
    const auto list = object_schemas();
    if (index >= list.size())
    {
        return "";
    }
    if (size != nullptr)
    {
        *size = list[index]->size;
    }
    return list[index]->name;
}

void host_actors(const GbtgGame* game, void (*visit)(void*, const GbtgActorInfo*), void* context)
{
    if (game == nullptr || visit == nullptr || !game_of(game).factory_scene)
    {
        return;
    }
    const auto& scene = *game_of(game).factory_scene;
    for (const auto& object : scene.objects)
    {
        const Actor* actor = actor_of(object);
        if (actor == nullptr)
        {
            continue;
        }
        GbtgActorInfo info{};
        info.address = scene.find_address(object.bytes.get());
        info.type = object.type;
        info.retail_name = retail_type_name(scene, object.type);
        info.class_key = actor->class_key;
        if (object.size >= sizeof(ActorBase))
        {
            const auto& base = *reinterpret_cast<const ActorBase*>(object.bytes.get());
            info.marker_id = base.marker_id;
            info.marker_next = base.marker_next;
            info.age = base.age;
            info.ticks = base.ticks;
        }
        info.position[0] = actor->position.x;
        info.position[1] = actor->position.y;
        info.position[2] = actor->position.z;
        info.euler[0] = actor->euler.x;
        info.euler[1] = actor->euler.y;
        info.euler[2] = actor->euler.z;
        info.animation_rate = actor->animation_rate;
        info.model_mode = actor->model_mode;
        info.shape_mode = actor->shape_mode;
        info.model_flags = actor->model_flags;
        info.damage_class = actor->damage_class;
        info.has_model = actor->model != nullptr ? 1 : 0;
        if (actor->attach != nullptr)
        {
            for (const auto& [bytes, other] : active_services->actors->actors)
            {
                if (other == actor->attach)
                {
                    info.attached_to = scene.find_address(bytes);
                    break;
                }
            }
        }
        visit(context, &info);
    }
}
}

void fill_diagnostics_calls(GbtgPluginHost& host)
{
    host.runtime_info = host_runtime_info;
    host.native_objects = host_native_objects;
    host.delayed_callbacks = host_delayed_callbacks;
    host.resource_bundles = host_resource_bundles;
    host.scene_count = host_scene_count;
    host.scene_at = host_scene_at;
    host.request_scene = host_request_scene;
    host.object_view = host_object_view;
    host.object_fields = host_object_fields;
    host.type_name = host_type_name;
    host.type_schema = host_type_schema;
    host.schema_count = host_schema_count;
    host.schema_at = host_schema_at;
    host.actors = host_actors;
}
}
#endif
