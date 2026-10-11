// Factory capture is an oracle boundary only; it is never installed in the game.
#include "scene/scene_marker_handlers.h"
#include "entities/entity_heap.h"
#include "navigation/navigation.h"
#include <cstring>
#include <iostream>
#include <vector>

using namespace gbtg;

namespace
{
std::vector<std::vector<u8>> captured;
bool succeeds;
u32 input()
{
    u32 value;
    std::cin >> value;
    return value;
}
std::vector<u8> blob()
{
    std::vector<u8> bytes(input());
    for (u8& byte : bytes)
    {
        byte = u8(input());
    }
    return bytes;
}
void output(std::span<const u8> bytes)
{
    std::cout << bytes.size() << ' ';
    for (u8 byte : bytes)
    {
        std::cout << u32(byte) << ' ';
    }
}
void sizes(ObjectSizes& result)
{
    result = {0x1000, 0x1000};
}
bool capture(FactoryObject&, std::span<const u8> parameters)
{
    captured.emplace_back(parameters.begin(), parameters.end());
    return succeeds;
}
const ObjectFactoryVtable* table()
{
    static const ObjectFactoryVtable value{0xF20000, sizes, capture, nullptr};
    return &value;
}
}

int main()
{
    const u32 cases = input();
    for (u32 index = 0; index < cases; ++index)
    {
        const u32 kind = input();
        succeeds = input() != 0;
        const bool player_present = input() != 0;
        auto records = blob();
        auto templates = blob();
        auto template_names = blob();
        auto dropper_names = blob();
        std::array<u32, 9> positions;
        for (u32& word : positions)
        {
            word = input();
        }
        Vec3 nearest, dropper, origin;
        std::memcpy(&nearest, positions.data(), 12);
        std::memcpy(&dropper, positions.data() + 3, 12);
        std::memcpy(&origin, positions.data() + 6, 12);
        std::array<ObjectFactoryType, 0x100> types;
        for (auto& type : types)
        {
            type = {0, table};
        }
        ObjectFactoryRegistry factory{types};
        std::vector<ExecutableResourceRecord> resources;
        auto add = [&](const char* name)
        {
            for (const auto& existing : resources)
            {
                if (existing.name == name)
                {
                    return;
                }
            }
            resources.push_back({name, templates, {}, animation_resource_hash(name)});
        };
        add("aid_objparams_test_alpha");
        for (size_t offset = 0; offset < template_names.size(); offset += 0x80)
        {
            add(reinterpret_cast<const char*>(template_names.data() + offset));
        }
        for (size_t offset = 0; offset < dropper_names.size(); offset += 0x80)
        {
            add(reinterpret_cast<const char*>(dropper_names.data() + offset));
        }
        std::array<ExecutableResourcePool, 1> pools{{{resources, "objparams", true}}};
        AnimationResourceRegistry registry{};
        ResourceErrorState error{};
        u32 random = 1;
        std::vector<FactoryObject> objects;
        const u8* record_pointer = records.data();
        std::array<u8, 0x100> zero{};
        RuntimeMarkerObjects context{factory, pools, record_pointer, zero, "", registry, error,
            random, objects, {}, {}};
        std::array<u8, 0x900> names{};
        u32 name_count = 0;
        Actor actor{};
        Actor* player = player_present ? &actor : nullptr;
        ListHead owner{}, global{};
        RuntimeMarkerAdditionalBindings additional{names, name_count, template_names.data(), dropper_names.data(),
            nearest, dropper, player, owner, global, origin, u8(positions[1] >> 24)};
        std::array<u8, 0x210> overrides{};
        const u8 scene_flags = 0;
        const u32 runtime_flags = 0, enabled = 1;
        const i32 chapter = 1;
        RuntimePlayerSpawnContext spawn{factory, pools, "", overrides, scene_flags, runtime_flags, ""};
        RuntimeMarkerPlayerStart start{spawn, record_pointer, zero, enabled, chapter,
            scene_flags, runtime_flags, {}, objects, {}};
        RuntimeMarkerCameraStart camera{};
        RuntimeSceneMarkerBindings bindings{start, context, nullptr, camera, &additional};
        const auto handlers = bind_runtime_scene_marker_handlers(bindings);
        for (u32 slot = 0; slot < 0x1C; ++slot)
        {
            const bool present = slot != 0 && slot != 1 && slot != 9 && slot != 0xC && slot != 0x1A && slot != 0x1B;
            if (bool(handlers.handlers[slot]) != present || handlers.untranslated[slot])
            {
                throw Error("native marker handler table differs from retail");
            }
        }
        captured.clear();
        u8* row = records.data() + 8;
        entity_detail::g_nav_count = 2;
        entity_detail::g_nav_min_y[0] = -15.f;
        entity_detail::g_nav_min_y[1] = 0.f;
        entity_detail::g_nav_max_y[0] = 0.f;
        entity_detail::g_nav_max_y[1] = 30.f;
        handlers.handlers[kind](row);
        output(records);
        std::cout << captured.size() << ' ';
        for (const auto& parameters : captured)
        {
            output(parameters);
        }
        output(std::span<const u8>{names.data(), name_count * 0x90});
        auto* attachment = static_cast<ActorLiveAttachment*>(owner.next);
        if (attachment != nullptr)
        {
            std::array<u8, 0x50> values{};
            values[0] = attachment->mode;
            values[1] = attachment->flag;
            std::memcpy(values.data() + 2, &attachment->tag, 2);
            std::memcpy(values.data() + 4, attachment->shape.data(), 0x20);
            std::memcpy(values.data() + 0x24, attachment->secondary_shape.data(), 0x20);
            std::memcpy(values.data() + 0x44, &attachment->origin, 12);
            output(values);
            engine_heap_free(attachment);
        }
        else
        {
            output({});
        }
        std::cout << '\n';
    }
}
