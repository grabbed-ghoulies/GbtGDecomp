#include "player_mind_goals.h"
#include "entity_heap.h"
#include "../runtime/name_lookup.h"
#include "gbtg/resource_error.h"
#include <bit>
#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x0002A770
const RuntimeMindGoalRow* find_runtime_mind_goal(const RuntimeMindGoalTable& table,
    const char* name)
{
    i32 high = std::bit_cast<i32>(u32(table.count) - 1u);
    i32 low = 0;
    if (high >= 0)
    {
        do
        {
            const i32 middle = std::bit_cast<i32>(u32(high) + u32(low)) >> 1;
            const auto& row = table.rows[middle];
            const auto* left = reinterpret_cast<const u8*>(row.name);
            const auto* right = reinterpret_cast<const u8*>(name);
            i32 comparison;
            for (;;)
            {
                u8 a = *left++;
                u8 b = *right++;
                if (a > 0x20)
                {
                    a &= 0xDF;
                }
                if (b > 0x20)
                {
                    b &= 0xDF;
                }
                comparison = i32(a) - i32(b);
                if (comparison != 0)
                {
                    break;
                }
                if (a == 0)
                {
                    return &row;
                }
            }
            if (comparison < 0)
            {
                low = middle + 1;
            }
            else
            {
                high = middle - 1;
            }
        } while (low <= high);
    }
    return nullptr;
}

// Xbox 0x0002A810
const RuntimeMindGoalRow* choose_runtime_mind_goal(const RuntimeMindGoalTable& table,
    const char* name, const char* wildcard, std::string& scratch, u32& random)
{
    const size_t prefix = wildcard - name;
    scratch.assign(name, prefix);
    append_animation_number(scratch, 1);
    i32 count = 0;
    if (find_runtime_mind_goal(table, scratch.c_str()) != nullptr)
    {
        do
        {
            count = std::bit_cast<i32>(u32(count) + 1u);
            scratch.resize(prefix);
            append_animation_number(scratch, std::bit_cast<i32>(u32(count) + 1u));
        } while (find_runtime_mind_goal(table, scratch.c_str()) != nullptr);
        if (count != 0)
        {
            random = random * 0x19660Du + 0x3C6EF35Fu;
            const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
            const i32 index = _mm_cvtt_ss2si(_mm_set_ss(fraction * float(count)));
            scratch.resize(prefix);
            append_animation_number(scratch, std::bit_cast<i32>(u32(index) + 1u));
            return find_runtime_mind_goal(table, scratch.c_str());
        }
    }
    return nullptr;
}

// Xbox 0x00029D90
RuntimePlayerMindRequest* allocate_runtime_mind_request(RuntimePlayerMindRequest& anchor,
    u16 kind, std::span<const u8> sizes)
{
    const u32 size = read_u32(sizes, size_t(kind) * 8);
    auto* block = static_cast<u8*>(engine_heap_allocate(i32(sizeof(RuntimePlayerMindRequest) + size)));
    if (block == nullptr)
    {
        return nullptr;
    }
    auto* request = new (block) RuntimePlayerMindRequest{};
    request->bytes = {block + sizeof(RuntimePlayerMindRequest), size};
    std::memset(request->bytes.data(), 0, size);
    request->kind = kind;
    std::memcpy(request->bytes.data() + 8, &kind, 2);
    RuntimePlayerMindRequest* tail = &anchor;
    while (tail->next != nullptr)
    {
        tail = tail->next;
    }
    tail->next = request;
    request->previous = tail;
    request->next = nullptr;
    return request;
}

// Xbox 0x0002A8E0
bool load_runtime_mind_goal(const char* name, RuntimeMindGoalContext& context)
{
    if (context.table != nullptr)
    {
        const char* wildcard = std::strchr(name, '?');
        const RuntimeMindGoalRow* row;
        if (wildcard == nullptr)
        {
            row = find_runtime_mind_goal(*context.table, name);
        }
        else
        {
            row = choose_runtime_mind_goal(*context.table, name, wildcard, context.scratch, context.random);
        }
        if (row != nullptr && row->bytes.data() + 4 != nullptr && read_u32(row->bytes, 4) == 0x5F646961)
        {
            const auto resource = acquire_animation_resource(context.registry,
                reinterpret_cast<const char*>(row->bytes.data() + 4));
            if (resource.data() == nullptr)
            {
                resource_error(context.error, 2);
            }
            if (resource.data() != nullptr)
            {
                const u8* node = resource.data();
                i32 index = binary_search_names(0x31, context.names, reinterpret_cast<const char*>(node + 4));
                if (index != -1)
                {
                    u16 kind = u16(index + 1);
                    while (kind != 0)
                    {
                        const u32 size = read_u16({node, 2}, 0);
                        const u32 payload = size - 0x44u;
                        if (kind < 0x32 && payload != 0 && payload ==
                            read_u32(context.sizes, size_t(kind) * 8) -
                            read_u32(context.sizes, size_t(kind) * 8 + 4) - 0x10u)
                        {
                            auto* request = allocate_runtime_mind_request(context.anchor, kind, context.sizes);
                            std::memcpy(request->bytes.data() + 0x10, node + 0x44, payload);
                        }
                        node += size;
                        index = binary_search_names(0x31, context.names, reinterpret_cast<const char*>(node + 4));
                        if (index == -1)
                        {
                            break;
                        }
                        kind = u16(index + 1);
                    }
                }
                release_animation_resource(context.registry, resource);
                return true;
            }
        }
    }
    return false;
}
}
