#include "player_state.h"
#include "entity_heap.h"
#include <cstring>

namespace gbtg
{
// Xbox 0x00027B60
void activate_runtime_body_action(std::span<u8> body, u32 value, u32 mode, RuntimeBodyAction& action)
{
    if (action.object.bytes == nullptr)
    {
        auto clone = clone_executable_resource(action.child.resources,
            reinterpret_cast<const char*>(body.data() + 0x764));
        FactoryObject object;
        if (clone.bytes != nullptr)
        {
            std::memcpy(clone.bytes.get() + 8, &action.child.actor_address, 4);
            object = create_factory_object(action.child.factory, {clone.bytes.get(), clone.size});
            clone.bytes.reset();
        }
        action.object = std::move(object);
    }
    else
    {
        action.message(action.object, 3);
    }
    if (read_u32(body, 0x754) == 0)
    {
        std::memcpy(body.data() + 0x758, &value, 4);
        std::memcpy(body.data() + 0x754, &mode, 4);
    }
}

// Xbox 0x000278F0
void initialize_runtime_body_action(std::span<u8> body, std::span<const u8> parameters,
    RuntimeBodyAction& action)
{
    const u32 kind = parameters[0x1C0];
    std::memcpy(body.data() + 0x750, &kind, 4);
    std::strcpy(reinterpret_cast<char*>(body.data() + 0x764), reinterpret_cast<const char*>(parameters.data() + 0x1C2));
    std::strcpy(reinterpret_cast<char*>(body.data() + 0x7F4), reinterpret_cast<const char*>(parameters.data() + 0x242));
    const u32 value = read_u32(parameters, 0x2C4);
    std::memcpy(body.data() + 0x7EC, &value, 4);
    if (parameters[0x1C1] != 0)
    {
        activate_runtime_body_action(body, 0, 1, action);
    }
}

std::function<void(u8 mask, i32 release)> g_actor_pause;

// Xbox 0x00117400
void change_runtime_actor_pause(u8 mask, i32 release, RuntimeActorPauseGroups& groups)
{
    std::array<bool, 2> changed{};
    for (size_t i = 0; i < 2; ++i)
    {
        if ((mask & (1u << i)) != 0)
        {
            if (release == 0)
            {
                ++groups.references[i];
                changed[i] = groups.references[i] == 1;
            }
            else
            {
                --groups.references[i];
                changed[i] = groups.references[i] == 0;
            }
        }
    }
    for (size_t i = 0; i < 2; ++i)
    {
        if (changed[i])
        {
            const u32 message = release == 0 ? 0x27 : 0x26;
            RuntimePauseActor* node = groups.first;
            while (node != nullptr)
            {
                RuntimePauseActor* next = node->next;
                const size_t group = read_u16(node->actor, 0xCDC) == 2 ||
                    read_u32(node->actor, 0x93C) != 0 ? 1 : 0;
                if (group == i)
                {
                    node->message(message);
                }
                node = next;
            }
        }
    }
}

// Xbox 0x000BF410
void leave_runtime_player_strategy(std::span<u8> strategy, u16 old_state, u16 next_state,
    std::span<u8> body, u32& active_count, RuntimeActorPauseGroups& groups)
{
    switch (old_state)
    {
    case 0x3F1:
    {
        const u16 state = 0x408;
        std::memcpy(strategy.data() + 0x18, &state, 2);
        std::memcpy(strategy.data() + 0x12, &state, 2);
        --strategy[0x1C];
        --active_count;
        break;
    }
    case 0x3F2:
        change_runtime_actor_pause(1, 1, groups);
        break;
    case 0x3FB:
        if (next_state != 0x3FB)
        {
            const u32 one = 1;
            std::memcpy(strategy.data() + 0x38, &one, 4);
            const u16 value = u16(read_u16(body, 0x30) + 1);
            std::memcpy(body.data() + 0x30, &value, 2);
        }
        break;
    case 0x3FC:
        if (read_u32(strategy, 0x48) != 0)
        {
            --active_count;
            const u32 zero = 0;
            std::memcpy(strategy.data() + 0x48, &zero, 4);
        }
        break;
    }
    leave_runtime_base_strategy_state(strategy, old_state);
}

// Xbox 0x00025800
void initialize_runtime_body_motion(std::span<u8> body)
{
    const auto put = [&](size_t offset, u32 value)
    {
        std::memcpy(body.data() + offset, &value, 4);
    };
    put(0x334, 0x41200000); // DAT_0032B10C, 10.
    put(0x324, 0xC3480000); // DAT_0032B224, -200.
    put(0x338, 0);
    put(0x32C, 0x3F800000);
    body[0x328] = 2;
    put(0x340, 0);
    for (size_t i = 0; i < 0x50; ++i)
    {
        put(0x34C + i * 12, 0);
        put(0x348 + i * 12, 0);
        put(0x344 + i * 12, 0);
    }
}

// Xbox 0x00073920
u8 count_runtime_player_body_joints(const ActorModel* model, std::span<const u8> parameters)
{
    u8 count = 0;
    for (size_t offset : {0x800u, 0x80Cu, 0x7F4u, 0x7E8u, 0x7D0u, 0x7DCu})
    {
        if (read_u32(parameters, offset) != 0xFFFFFFFFu && model != nullptr && model->cpu.data() != nullptr)
        {
            if ((actor_lookup_joint_index(model->cpu, i16(read_u16(parameters, offset))) & 0xFFu) != 0xFFu)
            {
                ++count;
            }
        }
    }
    return count;
}

// Xbox 0x00075270
void collect_runtime_player_body_shapes(const RuntimeBodyModelNodes* model, RuntimeBodyShapeStorage& storage)
{
    if (model != nullptr && model->nodes.data() != nullptr)
    {
        for (const auto& entry : model->nodes)
        {
            if (entry.kind == 4)
            {
                if (entry.data != nullptr)
                {
                    u32 count = 0;
                    u32 capacity = 0;
                    const u8* shape = entry.data + 8;
                    while (shape != nullptr)
                    {
                        if ((shape[4] & 4) != 0 && shape[0] < 2)
                        {
                            if (count == capacity)
                            {
                                capacity += 4;
                                const i32 bytes = i32(capacity * sizeof(const u8*));
                                if (storage.selected == nullptr)
                                {
                                    storage.selected = static_cast<const u8**>(engine_heap_allocate(bytes));
                                }
                                else
                                {
                                    storage.selected = static_cast<const u8**>(engine_heap_reallocate(storage.selected, bytes));
                                }
                            }
                            storage.selected[count++] = shape;
                        }
                        if (shape[1] == 0)
                        {
                            break;
                        }
                        shape += shape[1];
                    }
                    storage.selected_count = count;
                    if (count != 0)
                    {
                        storage.selected_state = static_cast<u8*>(engine_heap_allocate(i32(count << 5)));
                    }
                }
                return;
            }
        }
    }
}

// Xbox 0x00026010
void collect_runtime_body_special_shapes(const RuntimeBodyModelNodes* model, RuntimeBodyShapeStorage& storage)
{
    if (model != nullptr && model->nodes.data() != nullptr)
    {
        for (const auto& entry : model->nodes)
        {
            if (entry.kind == 4)
            {
                if (entry.data != nullptr)
                {
                    u32 count = 0;
                    const u8* shape = entry.data + 8;
                    do
                    {
                        if ((shape[4] & 0x10) != 0 && shape[0] == 0)
                        {
                            ++count;
                        }
                        if (shape[1] == 0)
                        {
                            break;
                        }
                        shape += shape[1];
                    } while (shape != nullptr);
                    if (count != 0)
                    {
                        storage.special_count = u8(count);
                        storage.special = static_cast<RuntimeBodySpecialShape*>(engine_heap_allocate(i32(count * sizeof(RuntimeBodySpecialShape))));
                        std::memset(storage.special, 0, count * sizeof(RuntimeBodySpecialShape));
                        shape = entry.data + 8;
                        u32 index = 0;
                        do
                        {
                            if ((shape[4] & 0x10) != 0 && shape[0] == 0)
                            {
                                storage.special[index].bytes[0x26] = 0;
                                storage.special[index++].shape = shape;
                            }
                            if (shape[1] == 0)
                            {
                                break;
                            }
                            shape += shape[1];
                        } while (shape != nullptr);
                        storage.special_active = 0;
                    }
                }
                return;
            }
        }
    }
}
}
