#include "gbtg/object_factory.h"
#include "../src/gbtg/runtime/object_lifecycle.h"
#include "gbtg/actor_spawn_factory.h"
#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace gbtg;

namespace
{

std::vector<int> calls;
bool fail_child = false;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

void sizes(ObjectSizes& output)
{
    output = ObjectSizes{19, 8};
}

bool parent_constructor(FactoryObject& object, std::span<const u8> parameters)
{
    require(object.type == read_u16(parameters, 4) && object.bytes[18] == 0,
            "constructor did not receive derived type or zeroed remainder byte");
    object.bytes[8] = 1;
    calls.push_back(1);
    return true;
}

bool child_constructor(FactoryObject& object, std::span<const u8>)
{
    require(object.bytes[8] == 1, "child constructor ran before its parent");
    object.bytes[9] = 2;
    calls.push_back(2);
    return !fail_child;
}

void parent_destructor(FactoryObject& object)
{
    require(object.bytes[8] == 1, "parent destructor lost parent state");
    calls.push_back(4);
    object.bytes[8] = 0;
}

void child_destructor(FactoryObject& object)
{
    require(object.bytes[9] == 2 && object.bytes[8] == 1,
            "child destructor ran after its parent or before child initialization");
    calls.push_back(3);
    object.bytes[9] = 0;
}

const ObjectFactoryVtable parent_vtable{0x100, sizes, parent_constructor, parent_destructor};
const ObjectFactoryVtable child_vtable{0x200, sizes, child_constructor, child_destructor};
const ObjectFactoryVtable shared_vtable{0x300, sizes, parent_constructor, parent_destructor};
const ObjectFactoryVtable null_vtable{0x400, sizes, nullptr, nullptr};

const ObjectFactoryVtable* parent_factory()
{
    return &parent_vtable;
}

const ObjectFactoryVtable* child_factory()
{
    return &child_vtable;
}

const ObjectFactoryVtable* shared_factory()
{
    return &shared_vtable;
}

const ObjectFactoryVtable* null_factory()
{
    return &null_vtable;
}

void spawn_sizes(ObjectSizes& output)
{
    output = ObjectSizes{0x640, 0x640};
}

bool spawn_constructor(FactoryObject& object, std::span<const u8> parameters)
{
    std::memcpy(object.bytes.get() + 8, parameters.data() + 8, parameters.size() - 8);
    return true;
}

const ObjectFactoryVtable spawn_vtable{0x500, spawn_sizes, spawn_constructor, nullptr};
const ObjectFactoryVtable* spawn_factory()
{
    return &spawn_vtable;
}

void check_spawn_factory()
{
    const std::array<ObjectFactoryType, 2> types
    {
        ObjectFactoryType{0, nullptr}, ObjectFactoryType{0, spawn_factory}
    };
    ObjectFactoryRegistry registry{types};
    std::array<u8, 0x640> source{};
    const u16 size = u16(source.size());
    const u16 type = 1;
    std::memcpy(source.data(), &size, 2);
    std::memcpy(source.data() + 4, &type, 2);
    source[0x400] = 0xA5;
    std::array<u8, 0x640> gold_source = source;
    gold_source[0x400] = 0xB6;
    const float retained_depth = 0.75f;
    std::memcpy(source.data() + 0x33C, &retained_depth, 4);
    ExecutableResourceRecord bat_records[2];
    bat_records[0].name = "aid_objparams_ghoulies_misc_buttonbat";
    bat_records[1].name = "aid_objparams_ghoulies_misc_buttonbatgold";
    bat_records[0].cpu = source;
    bat_records[1].cpu = gold_source;
    for (auto& bat_record : bat_records)
    {
        bat_record.hash = animation_resource_hash(bat_record.name.c_str());
    }
    ExecutableResourcePool bat_pool{{bat_records, 2}, "objparams", false};
    auto bat = create_dialogue_button_bat(registry, {&bat_pool, 1}, 0, {30.f, 50.f}, 2.f, -1.f);
    std::span<const u8> bat_bytes{bat.bytes.get(), bat.size};
    require(read_f32(bat_bytes, 0x334) == 26.f && read_f32(bat_bytes, 0x338) == 8.f &&
        read_f32(bat_bytes, 0x33C) == retained_depth && bat_bytes[0x400] == 0xA5,
        "button bat factory lost position adjustment or retained depth");
    auto gold_bat = create_dialogue_button_bat(registry, {&bat_pool, 1}, 0x40, {30.f, 50.f}, 1.f,
        std::numeric_limits<float>::quiet_NaN());
    std::span<const u8> gold_bytes{gold_bat.bytes.get(), gold_bat.size};
    require(read_f32(gold_bytes, 0x334) == 28.f && read_f32(gold_bytes, 0x338) == 29.f &&
        read_f32(gold_bytes, 0x33C) != read_f32(gold_bytes, 0x33C) && gold_bytes[0x400] == 0xB6 &&
        read_f32(source, 0x334) == 0.f, "gold button bat selection or NaN depth override changed");
    ExecutableResourceRecord map_record;
    map_record.name = "aid_objparams_ghoulies_scenecontrol_mapview";
    map_record.hash = animation_resource_hash(map_record.name.c_str());
    map_record.cpu = source;
    ExecutableResourcePool map_pool{{&map_record, 1}, "objparams", false};
    auto map_control = create_dialogue_map_control(registry, {&map_pool, 1});
    require(map_control.bytes != nullptr && map_control.bytes[0x400] == 0xA5
        && source[0x400] == 0xA5, "dialogue map control lost its cloned parameters");
    const auto count_after_map = registry.count;
    auto missing_map = create_dialogue_map_control(registry, {&bat_pool, 1});
    require(missing_map.bytes == nullptr && registry.count == count_after_map,
        "missing dialogue map parameters fabricated a factory object");
    registry.count = 0;

    ExecutableResourceRecord record;
    record.name = "aid_actor_spawn_test";
    record.hash = animation_resource_hash(record.name.c_str());
    record.cpu = source;
    ExecutableResourcePool pool{{&record, 1}, "actor", false};
    std::array<u8, 20> scene{};
    const u32 count = 1;
    const u32 stride = 12;
    const i16 key = 9;
    const u16 mapped = 77;
    std::memcpy(scene.data(), &count, 4);
    std::memcpy(scene.data() + 8, &stride, 4);
    std::memcpy(scene.data() + 14, &key, 2);
    std::memcpy(scene.data() + 16, &mapped, 2);
    auto object = create_spawn_actor(registry, {&pool, 1}, scene.data(), record.name.c_str(),
        Vec3{1.f, 2.f, 3.f}, Vec3{4.f, 5.f, 6.f}, 2.f, "variant", "attributes",
        "second", nullptr, nullptr, key);
    require(object.bytes != nullptr && registry.count == 1, "spawn resource did not construct");
    std::span<const u8> bytes{object.bytes.get(), object.size};
    require(read_f32(bytes, 8) == 1.f && read_f32(bytes, 0x1C) == 6.f &&
        read_f32(bytes, 0x20) == 2.f && read_u16(bytes, 0x2AA) == 9 &&
        read_u16(bytes, 0x2AC) == 77, "spawn factory lost pose or scene type");
    require(std::strcmp(reinterpret_cast<const char*>(bytes.data() + 0x3BA), "variant") == 0 &&
        std::strcmp(reinterpret_cast<const char*>(bytes.data() + 0x33A), "attributes") == 0 &&
        std::strcmp(reinterpret_cast<const char*>(bytes.data() + 0x4BA), "second") == 0 &&
        bytes[0x400] == 0xA5 && source[0x3BA] == 0,
        "spawn override offsets or cloned source ownership changed");
    require(find_scene_spawn_type(scene.data(), 10) == 0, "missing scene key did not return zero");
    auto unit = create_spawn_actor_unit_scale(registry, {&pool, 1}, nullptr, record.name.c_str(),
        Vec3{}, Vec3{}, "unit_variant", "unit_attributes", "unit_second", nullptr, nullptr);
    require(unit.bytes != nullptr && read_f32({unit.bytes.get(), unit.size}, 0x20) == 1.f,
        "unit scale wrapper changed scale or dereferenced absent scene records");
    require(std::strcmp(reinterpret_cast<const char*>(unit.bytes.get() + 0x4BA), "unit_variant") == 0 &&
        std::strcmp(reinterpret_cast<const char*>(unit.bytes.get() + 0x53A), "unit_second") == 0 &&
        std::strcmp(reinterpret_cast<const char*>(unit.bytes.get() + 0x5BA), "unit_attributes") == 0,
        "unit scale register arguments did not reach derived actor overrides");
    auto missing = create_spawn_actor_unit_scale(registry, {&pool, 1}, nullptr, "aid_actor_missing",
        Vec3{}, Vec3{}, nullptr, nullptr, nullptr, nullptr, nullptr);
    require(missing.bytes == nullptr && registry.count == 2, "missing spawn resource fabricated an actor");
}

} // namespace

int main()
{
    try
    {
        const std::array<ObjectFactoryType, 5> types
        {
            ObjectFactoryType{0, nullptr}, ObjectFactoryType{0, parent_factory},
            ObjectFactoryType{1, child_factory}, ObjectFactoryType{1, shared_factory},
            ObjectFactoryType{0, null_factory}
        };
        ObjectFactoryRegistry registry{types};
        std::array<u8, 8> parameters{};
        u16 type = 2;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto object = create_factory_object(registry, parameters);
        require(object.bytes != nullptr && object.size == 19 && registry.count == 1 &&
                read_u32(std::span<const u8>(object.bytes.get(), object.size), 0) == 0x200 &&
                calls == std::vector<int>{1, 2},
                "factory did not allocate the derived object and construct parents first");
        destroy_factory_object(registry, type, object, nullptr);
        require(calls == std::vector<int>{1, 2, 3, 4}, "destructor order differs");
        calls.clear();
        fail_child = true;
        auto failed = create_factory_object(registry, parameters);
        require(failed.bytes == nullptr && registry.count == 1 &&
                calls == std::vector<int>{1, 2, 3, 4},
                "failed construction did not roll back or incremented live count");
        calls.clear();
        type = 3;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto shared = create_factory_object(registry, parameters);
        require(shared.bytes != nullptr && calls == std::vector<int>{1},
                "shared constructor was called twice");
        destroy_factory_object(registry, type, shared, nullptr);
        require(calls == std::vector<int>{1, 4}, "shared destructor was called twice");
        calls.clear();
        type = 4;
        std::memcpy(parameters.data() + 4, &type, 2);
        auto no_callbacks = create_factory_object(registry, parameters);
        destroy_factory_object(registry, type, no_callbacks, nullptr);
        require(no_callbacks.bytes != nullptr && calls.empty() && registry.count == 3,
                "null constructor/destructor entry changed allocation behavior");
        release_factory_object(registry, no_callbacks);
        require(no_callbacks.bytes == nullptr && registry.count == 2 &&
                no_callbacks.type == 4 && no_callbacks.size != 0 && calls.empty(),
                "object release did not free storage and decrement the live count");
        check_spawn_factory();
        ObjectSizes retail_sizes{};
        button_bat_object_sizes(retail_sizes);
        require(retail_sizes.instance == 0x958 && retail_sizes.parameters == 0x340,
                "button bat sizes differ from executable");
        actor_object_sizes(retail_sizes);
        require(retail_sizes.instance == 0x934 && retail_sizes.parameters == 0x334,
                "actor sizes differ from executable");
        std::cout << "object factory lifecycle checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
