#include "player_mind_owner_checks.h"
#include "../src/engine/entities/player_mind_owner.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
using namespace gbtg;
void require(bool value, const char* message)
{
    if (!value)
    {
        throw Error(message);
    }
}

// Contexts the type-7 lifecycle never reads; they only satisfy the slot references.
template <typename T>
T& unused()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}

void empty_destroy(FactoryObject&)
{
}

const ObjectFactoryVtable* base_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050E0C0, base_object_sizes, initialize_base_object, empty_destroy};
    return &table;
}

struct MindState
{
    std::span<const u8> script_table;
    RuntimePlayerMindRequest* first = nullptr;
    RuntimePlayerMindRequests requests{first, nullptr};
    LiveScript* events = nullptr;
    RuntimeMindStorage* storage = nullptr;
    Actor actor;
};
}

void check_player_mind_owner(SceneExecutableResources& executable)
{
    const std::array<u16, 2> settings_rows{0, 0};
    const RuntimeMindClassSettings settings{0, {reinterpret_cast<const u8*>(settings_rows.data()), sizeof(settings_rows)}};
    u32 random = 77;
    const u32 runtime_flags = 0;
    std::vector<std::unique_ptr<MindState>> states;
    u32 created = 0;
    NativePlayerMindOwner owner(NativePlayerMindBindings{
        [&](FactoryObject& object, std::span<const u8>)
        {
            ++created;
            states.push_back(std::make_unique<MindState>());
            auto& state = *states.back();
            return std::make_unique<NativePlayerMindSlots>(NativePlayerMindSlots{
                std::span<u8>(object.bytes.get(), object.size), 42, settings, random, executable.pools,
                state.script_table, unused<RuntimePlayerMindSetup>(), state.requests, state.events, state.storage, {},
                runtime_flags, unused<RuntimeInputActivationContext>(), unused<RuntimePlayerMindMessages>(),
                unused<RuntimePlayerMindTick>(), unused<RuntimePlayerMindRequestUpdate>(), state.actor, nullptr, nullptr,
                nullptr});
        }});
    std::vector<ObjectFactoryType> types(0x60);
    types[1] = ObjectFactoryType{0, base_lifecycle};
    owner.install(types);
    require(types[7].parent == 1 && types[7].vtable()->address == 0x004DD6F4 && types[0x34].parent == 7
        && types[0x34].vtable()->address == 0x00505498, "player mind owner must install getters 0x29D30 and 0x7E100");
    ObjectSizes sizes{};
    types[0x34].vtable()->sizes(sizes);
    require(sizes.instance == 0x198, "player mind instance size comes from 0x7E0E0");

    // A parent-type mind runs base construction 0x00107700, then 0x00029F20 through the view.
    ObjectFactoryRegistry registry{types, 0};
    std::vector<u8> parameters(0xA0);
    const u16 type = 7;
    std::memcpy(parameters.data() + 4, &type, 2);
    const u32 owner_word = 0x13572468;
    std::memcpy(parameters.data() + 0x9C, &owner_word, 4);
    FactoryObject mind = create_factory_object(registry, parameters);
    require(mind.bytes != nullptr && created == 1 && registry.count == 1, "parent mind construction through owner");
    require(read_u32({mind.bytes.get(), mind.size}, 8) == owner_word && read_u32({mind.bytes.get(), mind.size}, 0x4C)
        == 0x40, "0x00029F20 must initialize the owned view's bytes");
    require(&owner.view(mind).mind[0] == mind.bytes.get(), "owner view maps mind bytes");
    destroy_factory_object(registry, 7, mind, nullptr);
    bool released = false;
    try
    {
        owner.view(mind);
    }
    catch (const std::out_of_range&)
    {
        released = true;
    }
    require(released, "0x00029FF0 adapter must release the owner view");

    NativePlayerMindOwner second(NativePlayerMindBindings{});
    bool rejected = false;
    try
    {
        second.install(types);
    }
    catch (const Error&)
    {
        rejected = true;
    }
    require(rejected, "a second player mind owner must not replace the installed one");
}
