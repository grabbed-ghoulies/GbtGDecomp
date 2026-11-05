#include "player_parent_lifecycle.h"
#include "strategy_fields.h"
#include "mind_fields.h"
#include "entity_heap.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
float random_fraction(u32& random)
{
    random = random * 0x19660Du + 0x3C6EF35Fu;
    return std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
}
void write_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
void write_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}
}

// Xbox 0x00029D10
void base_mind_object_sizes(ObjectSizes& output)
{
    output.instance = 0x7C;
    output.parameters = 0xA0;
}

// Xbox 0x0002B6A0
void base_strategy_object_sizes(ObjectSizes& output)
{
    output.instance = 0x38;
    output.parameters = 0x18;
}

// Xbox 0x00029A60
void initialize_runtime_mind_settings(std::span<u8> mind, std::span<const u8> parameters,
    u16 actor_class, const RuntimeMindClassSettings& settings)
{
    mind_record(mind).target_flags = read_u32(parameters, 0x90);
    mind_record(mind).target_categories = read_u32(parameters, 0x94);
    mind_record(mind).target_range = read_f32(parameters, 0x98);
    i32 index = 0;
    if (settings.count > 0)
    {
        while (actor_class != read_u16(settings.rows, static_cast<size_t>(index) * 4))
        {
            ++index;
            if (index >= settings.count)
            {
                return;
            }
        }
        mind_record(mind).target_range = static_cast<float>(read_u16(settings.rows, static_cast<size_t>(index) * 4
            + 2));
    }
}

// Xbox 0x00029F20
bool initialize_runtime_base_mind(std::span<u8> mind, std::span<const u8> parameters,
    u16 actor_class, const RuntimeMindClassSettings& settings, u32& random,
    std::span<ExecutableResourcePool> resources, std::span<const u8>& script_table)
{
    mind_record(mind).actor = read_u32(parameters, 0x9C);
    const float fraction = random_fraction(random);
    const float low = read_f32(parameters, 0x88);
    mind_record(mind).unk_2C = (read_f32(parameters, 0x8C) - low) * fraction + low;
    initialize_runtime_mind_settings(mind, parameters, actor_class, settings);
    mind_record(mind).gaze_point_set = 0;
    mind_record(mind).alert_mode = 0x40;
    if (read_u32(parameters, 8) == 0x5F646961)
    {
        const char* name = reinterpret_cast<const char*>(parameters.data() + 8);
        auto* record = find_executable_resource(resources, name, true);
        if (record != nullptr)
        {
            script_table = record->cpu;
            return true;
        }
        script_table = {};
    }
    return true;
}

// Xbox 0x00029EC0
bool initialize_runtime_mind_request(std::span<u8> request, u32& random)
{
    if (read_u16(request, 8) == 0x29)
    {
        const float fraction = random_fraction(random);
        const float low = read_f32(request, 0x10);
        write_float(request, 0x18, (read_f32(request, 0x14) - low) * fraction + low);
    }
    return true;
}

// Xbox 0x0002A240
void clear_runtime_mind_events(LiveScript*& first)
{
    LiveScript* event = first;
    while (event != nullptr)
    {
        LiveScript* next = event->next;
        if (event->prev != nullptr)
        {
            event->prev->next = next;
        }
        if (event->next != nullptr)
        {
            event->next->prev = event->prev;
        }
        event->prev = nullptr;
        event->next = nullptr;
        script_release_event(event);
        event = next;
    }
}

// Xbox 0x00036B60
void release_runtime_mind_storage(RuntimeMindStorage& storage)
{
    if (storage.scratch != nullptr)
    {
        engine_heap_free(storage.scratch);
        storage.scratch = nullptr;
    }
    engine_heap_free(&storage);
}

// Xbox 0x00029FF0
void destroy_runtime_base_mind(RuntimePlayerMindRequests& requests, LiveScript*& events,
    RuntimeMindStorage*& storage, std::span<const u8>& script_table)
{
    clear_runtime_player_mind_requests(requests);
    clear_runtime_mind_events(events);
    if (storage != nullptr)
    {
        release_runtime_mind_storage(*storage);
        storage = nullptr;
    }
    if (script_table.data() != nullptr)
    {
        script_table = {};
    }
}

// Xbox 0x0010F6C0
void set_runtime_strategy_class_range(u16 actor_class, float low, float high, std::span<u8> rows, i32& count)
{
    if (low == 0.f && high == 0.f)
    {
        for (i32 index = 0; index < count; ++index)
        {
            const size_t row = size_t(index) * 0xC;
            if (read_u16(rows, row) == actor_class)
            {
                std::memcpy(rows.data() + row, rows.data() + size_t(count - 1) * 0xC, 0xC);
                count = count - 1;
                return;
            }
        }
    }
    const size_t row = size_t(count) * 0xC;
    if (row + 0xC > rows.size())
    {
        throw Error("0x0010F6C0 overflows DAT_0050E1C8's rows");
    }
    std::memcpy(rows.data() + row, &actor_class, 2);
    std::memcpy(rows.data() + row + 4, &low, 4);
    std::memcpy(rows.data() + row + 8, &high, 4);
    count = count + 1;
}

// Xbox 0x0010F760
bool sample_runtime_strategy_class_range(std::span<u8, 4> output, u16 actor_class,
    const RuntimeStrategyClassRanges& ranges, u32& random)
{
    i32 index = 0;
    if (ranges.count > 0)
    {
        do
        {
            const size_t row = static_cast<size_t>(index) * 0xC;
            if (actor_class == read_u16(ranges.rows, row))
            {
                const float fraction = random_fraction(random);
                const float low = read_f32(ranges.rows, row + 4);
                write_float(output, 0, (read_f32(ranges.rows, row + 8) - low) * fraction + low);
                return true;
            }
            ++index;
        }
        while (index < ranges.count);
    }
    return false;
}

// Xbox 0x0002B740
bool initialize_runtime_base_strategy(std::span<u8> strategy, std::span<const u8> parameters,
    u16 actor_class, const RuntimeStrategyClassRanges& ranges, u32& random)
{
    StrategyBase& base = strategy_base(strategy);
    base.actor = read_u32(parameters, 8);
    base.unk_0C = read_u16(parameters, 0xC);
    base.unk_10 = read_u16(parameters, 0xE);
    base.after_wait_state = read_u16(parameters, 0x10);
    base.resume_state = read_u16(parameters, 0x12);
    base.flagged_resume_state = read_u16(parameters, 0x14);
    base.unk_18 = read_u16(parameters, 0x16);
    // A miss preserves +0x34; the retail caller ignores the sampler's result.
    sample_runtime_strategy_class_range(retail_bytes(base.wait_timer).first<4>(), actor_class, ranges, random);
    return true;
}

// Xbox 0x00112A20
void detach_runtime_strategy_link(RuntimeStrategyLink& link)
{
    RuntimeStrategyLink* first = link.previous;
    RuntimeStrategyLink* anchor;
    if (first == nullptr)
    {
        anchor = nullptr;
    }
    else
    {
        anchor = &link;
        RuntimeStrategyLink* node = first;
        while (node != nullptr)
        {
            anchor = node;
            node = node->previous;
        }
    }
    if (first != nullptr)
    {
        first->next = link.next;
    }
    if (link.next != nullptr)
    {
        link.next->previous = link.previous;
    }
    link.previous = nullptr;
    link.next = nullptr;
    if (anchor != nullptr && anchor->next == nullptr)
    {
        anchor->owner = nullptr;
    }
    link.active = 0;
}

// Xbox 0x0002B7A0
void destroy_runtime_base_strategy(FactoryObject& strategy, RuntimeStrategyLink& link,
    RuntimePlayerStrategySetup& context)
{
    detach_runtime_strategy_link(link);
    context.leaving(strategy, read_u16({strategy.bytes.get(), strategy.size}, 0xE), 1000);
    const u16 previous = read_u16({strategy.bytes.get(), strategy.size}, 0xE);
    const u16 next = 1000;
    std::memcpy(strategy.bytes.get() + 0xE, &next, 2);
    clear_runtime_player_mind_requests(*context.mind);
    context.entering(strategy, next, previous);
}

// Xbox 0x000BFA50
void leave_runtime_base_strategy_state(std::span<u8> strategy, u16 state)
{
    if (state == 0x3FC || state == 0x3FF || state == 0x415)
    {
        --strategy_base(strategy).pause_count;
    }
}
}
