#include "factory_scene_runtime.h"
#include "../entities/entity_heap.h"
#include "../entities/actor_factory_owner.h"
#include "../../gbtg/runtime/object_lifecycle.h"
#include <limits>
#include <utility>

namespace gbtg
{
namespace
{
template<size_t Type>
const ObjectFactoryVtable* missing_class()
{
    throw Error("scene factory type " + std::to_string(Type) + " is not translated/installed");
}

template<size_t... Types>
constexpr auto missing_getters(std::index_sequence<Types...>)
{
    return std::array<const ObjectFactoryVtable* (*)(), sizeof...(Types)>{missing_class<Types>...};
}

SceneFactoryRuntime* active_scene = nullptr;

// Base object type 1 (vtable 0x0050E0C0): the retail constructor 0x00107700 and empty destructor
// 0x001628B0, with the native address mapping made when the instance is first constructed, so the
// derived constructors can name their own instance.
bool construct_scene_base_object(FactoryObject& object, std::span<const u8> parameters)
{
    active_scene->bind_address({object.bytes.get(), object.size});
    return initialize_base_object(object, parameters);
}

void destroy_scene_base_object(FactoryObject& object)
{
    if (active_scene->released)
    {
        active_scene->released(object.bytes.get());
    }
    active_scene->unbind_address(object.bytes.get());
}

const ObjectFactoryVtable* scene_base_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050E0C0, base_object_sizes, construct_scene_base_object,
        destroy_scene_base_object};
    return &table;
}
}

SceneFactoryRuntime::SceneFactoryRuntime(AidStore& aids, std::shared_ptr<SceneExecutableResources> image)
    : executable(std::move(image)),
      resources(aids, executable != nullptr ? std::span<ExecutableResourcePool>(executable->pools)
          : std::span<ExecutableResourcePool>{})
{
    if (executable == nullptr)
    {
        throw Error("scene factory requires its executable resource owner");
    }
    const auto table = executable->memory(0x0030D41C, type_count * 12);
    constexpr auto getters = missing_getters(std::make_index_sequence<type_count>{});
    for (size_t type = 0; type < type_count; ++type)
    {
        const u32 parent = read_u32(table, type * 12);
        if (parent >= type_count)
        {
            throw Error("scene factory parent is outside the retail type table");
        }
        types[type] = {u16(parent), getters[type]};
        retail_getters[type] = read_u32(table, type * 12 + 4);
        retail_names[type] = read_u32(table, type * 12 + 8);
    }
    if (active_scene != nullptr)
    {
        throw Error("another scene factory runtime is active");
    }
    active_scene = this;
    install_base_class();
}

SceneFactoryRuntime::~SceneFactoryRuntime()
{
    release_objects();
    active_scene = nullptr;
}

void SceneFactoryRuntime::install_base_class()
{
    types[1] = ObjectFactoryType{0, scene_base_lifecycle};
}

FactoryObject* SceneFactoryRuntime::keep_object(FactoryObject object)
{
    if (object.bytes == nullptr)
    {
        return nullptr;
    }
    objects.push_back(std::move(object));
    return &objects.back();
}

void SceneFactoryRuntime::release_objects()
{
    for (auto object = objects.rbegin(); object != objects.rend(); ++object)
    {
        if (object->bytes != nullptr)
        {
            release_factory_object(factory, *object);
        }
    }
    objects.clear();
}

u32 SceneFactoryRuntime::bind_address(std::span<u8> bytes)
{
    if (bytes.empty() || bytes.data() == nullptr)
    {
        throw Error("cannot register an empty native instance");
    }
    const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
    for (const auto& [address, existing] : addresses_)
    {
        const auto other = reinterpret_cast<std::uintptr_t>(existing.data());
        if (begin == other && bytes.size() == existing.size())
        {
            return address;
        }
        if (begin < other + existing.size() && other < begin + bytes.size())
        {
            throw Error("native address registration overlaps an existing instance");
        }
    }
    const u64 step = (u64(bytes.size()) + 15) & ~u64(15);
    if (step > std::numeric_limits<u32>::max() - u64(next_address_))
    {
        throw Error("native scene address token arena is exhausted");
    }
    const u32 address = next_address_;
    addresses_.emplace(address, bytes);
    next_address_ += u32(step);
    return address;
}

void SceneFactoryRuntime::unbind_address(const void* pointer)
{
    for (auto row = addresses_.begin(); row != addresses_.end(); ++row)
    {
        if (row->second.data() == pointer)
        {
            addresses_.erase(row);
            return;
        }
    }
    throw Error("native scene address release has no registered instance");
}

u32 SceneFactoryRuntime::find_address(const void* pointer) const
{
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    for (const auto& [address, bytes] : addresses_)
    {
        const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
        if (value >= begin && value - begin < bytes.size())
        {
            return address + u32(value - begin);
        }
    }
    return 0;
}

u32 SceneFactoryRuntime::address_of(const void* pointer) const
{
    if (pointer == nullptr)
    {
        return 0;
    }
    const auto value = reinterpret_cast<std::uintptr_t>(pointer);
    for (const auto& [address, bytes] : addresses_)
    {
        const auto begin = reinterpret_cast<std::uintptr_t>(bytes.data());
        if (value >= begin && value - begin < bytes.size())
        {
            return address + u32(value - begin);
        }
    }
    throw Error("native pointer has no scene address mapping");
}

std::span<u8> SceneFactoryRuntime::resolve_address(u32 address, size_t size) const
{
    auto row = addresses_.upper_bound(address);
    if (row == addresses_.begin())
    {
        throw Error("scene address is not registered");
    }
    --row;
    const size_t offset = address - row->first;
    if (offset > row->second.size() || size > row->second.size() - offset)
    {
        throw Error("scene address range exceeds its native instance");
    }
    return row->second.subspan(offset, size);
}

size_t SceneFactoryRuntime::address_count() const
{
    return addresses_.size();
}

void SceneFactoryRuntime::MarkerDeleter::operator()(u8* pointer) const
{
    engine_heap_free(pointer);
}

void SceneFactoryRuntime::load_markers(const char* scene_record)
{
    markers_.reset(load_runtime_scene_markers(scene_record, resources.registry, resources.error));
}

SceneActorServices& SceneFactoryRuntime::start_actors(Game& game)
{
    if (actor_services == nullptr)
    {
        actor_services = std::make_unique<SceneActorServices>(*this, game);
    }
    return *actor_services;
}

std::span<const u8> SceneFactoryRuntime::markers() const
{
    if (markers_ == nullptr)
    {
        return {};
    }
    return {markers_.get(), read_u32({markers_.get(), 8}, 4)};
}
}
