#include "../src/engine/graphics/shader_schedule_issue.h"
#include "../src/engine/graphics/shader_schedule_init.h"
#include "../src/engine/graphics/shader_validation_arena.h"
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}

void check_shader_schedule_dependencies()
{
    using namespace gbtg;
    std::array<u32, runtime_shader_schedule_storage_size / 4> schedule;
    schedule.fill(0xA5A5A5A5);
    require(configure_runtime_shader_schedule(reinterpret_cast<u8*>(schedule.data()), 1, 0x12345678) == 0,
        "schedule mode setup forwards actual initializer return");
    require(schedule[4] == 0xA5A5A501 && schedule[5] == 0 && schedule[6] == 0xFFFFFFFA
        && schedule[7] == 0xFFFFFFFA && schedule.back() == 0x12345678,
        "schedule setup preserves mode padding and stores actual option and issued fields");
    for (u32 index = 0; index < 52; ++index)
    {
        require(schedule[8 + index] == 0xFFFFFFFA && schedule[60 + index] == 4
            && schedule[112 + index] == 0x40200000 && schedule[168 + index] == 0x40200000
            && schedule[224 + index] == 0x3FC00000 && schedule[280 + index] == 0x3FC00000
            && schedule[336 + index] == 0x41 && schedule[392 + index] == 0x41
            && schedule[448 + index] == 0x41 && schedule[504 + index] == 0x41,
            "schedule initialization fills all thirteen rows at native offsets");
    }
    require(schedule[0x290 / 4] == 0 && schedule[0x610 / 4] == 0x20
        && schedule[0x294 / 4] == 0xA5A5A5A5 && schedule[0x614 / 4] == 0xA5A5A5A5,
        "schedule initialization retains untouched gap channels");
    configure_runtime_shader_schedule(reinterpret_cast<u8*>(schedule.data()), 0x81, 9);
    require(schedule[4] == 0xA5A5A500 && schedule.back() == 9,
        "only exact low-byte mode one enables scheduling");
    std::array<u32, 449> table;
    for (u32 index = 0; index < table.size(); ++index)
    {
        table[index] = 0x7FC00000 + index;
    }
    const auto previous = table;
    for (u32 selected = 0; selected < 16; ++selected)
    {
        table = previous;
        write_runtime_shader_register_schedule(reinterpret_cast<u8*>(table.data()), 7, 12,
            static_cast<u8>(selected), 0xFFFFFFFE);
        for (u32 index = 0; index < table.size(); ++index)
        {
            u32 expected = previous[index];
            if (index >= 48 && index < 52 && (selected & (1u << (index - 48))) != 0)
            {
                expected = 4;
            }
            if (index >= 100 && index < 104 && (selected & (1u << (index - 100))) != 0)
            {
                expected = 7;
            }
            require(table[index] == expected, "register schedule writes only selected channels with wrapping cycle addition");
        }
    }
    require(runtime_shader_swizzle_channel_mask(0, 1, 2, 3) == 15
        && runtime_shader_swizzle_channel_mask(2, 2, 2, 2) == 2
        && runtime_shader_swizzle_channel_mask(0xFF, 0xFF, 0xFF, 0xFF) == 16,
        "operand mask retains x86 shift masking and duplicate channel union");
    const std::array<u32, 3> operand_masks{0, 0x000001B0, 0x00000AA0};
    require(runtime_shader_operand_channel_mask(operand_masks.data(), 0) == 2
        && runtime_shader_operand_channel_mask(operand_masks.data(), 1) == 8
        && runtime_shader_operand_channel_mask(operand_masks.data(), 2) == 15
        && runtime_shader_operand_channel_mask(operand_masks.data(), 3) == 0,
        "operand channel masks use each native bitfield and reject unknown operand");
    constexpr u32 latencies[16] = {4, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 4, 4, 4};
    for (u32 opcode = 0; opcode < 16; ++opcode)
    {
        std::array<u32, 3> instruction{0, 0, opcode << 25};
        require(runtime_shader_vector_latency(instruction.data()) == latencies[opcode],
            "vector scheduling latency follows all sixteen retail entries");
        instruction[2] |= 0x20000000;
        require(runtime_shader_vector_latency(instruction.data()) == 0,
            "combined scalar operation suppresses vector scheduling latency");
    }
    table = previous;
    advance_runtime_shader_issue_schedule(reinterpret_cast<u8*>(table.data()));
    for (u32 index = 0; index < 56; ++index)
    {
        require(table[index] == previous[112 + index] && table[56 + index] == previous[168 + index]
            && table[112 + index] == 0 && table[168 + index] == 0
            && table[224 + index] == previous[336 + index] && table[280 + index] == previous[392 + index]
            && table[336 + index] == 0x20 && table[392 + index] == 0x20,
            "issue schedule shifts raw words and reasons for all fourteen rows");
    }
    require(table.back() == previous.back() && table[0xD0 / 4] == previous[0x290 / 4]
        && table[0x450 / 4] == previous[0x610 / 4],
        "issue schedule loop overwrites initial stores at actual final-row aliases");

    ShaderValidationArenaNode root{}, first{}, second{}, leaf{};
    root.first = &first;
    root.second = &second;
    first.parent = second.parent = &root;
    second.first = &leaf;
    leaf.parent = &second;
    auto* iterator = &root;
    advance_shader_validation_arena_iterator(iterator);
    require(iterator == &leaf, "arena iterator enters second subtree and descends first children");
    advance_shader_validation_arena_iterator(iterator);
    require(iterator == &second, "arena iterator climbs from first child to parent");
    advance_shader_validation_arena_iterator(iterator);
    require(iterator == nullptr, "arena iterator climbs through second child to terminal null");
    iterator = &first;
    advance_shader_validation_arena_iterator(iterator);
    require(iterator == &root, "arena iterator first-child successor is parent");

    struct ArenaNode : ShaderValidationArenaNode
    {
        u32 id;
        std::vector<u32>* visits;
        ShaderValidationArenaNode* owner;
        ShaderValidationArenaNode* replacement;
        ShaderValidationArenaStorage* storage;
    };
    const ShaderValidationArenaVtable arena_vtable
    {
        [](ShaderValidationArenaNode& base, u32 release)
        {
            auto& node = static_cast<ArenaNode&>(base);
            require(release == 1 && node.parent == nullptr, "arena deletion clears parent before callback");
            require(node.storage->root != nullptr && node.storage->count == 4,
                "arena reset retains storage until all destruction callbacks return");
            node.visits->push_back(node.id);
            if (node.owner != nullptr)
            {
                node.owner->second = node.replacement;
            }
        }
    };
    for (u32 kind = 0; kind < 2; ++kind)
    {
        std::vector<u32> visits;
        ArenaNode owner{}, child{}, replaced{}, replacement{};
        ShaderValidationArenaStorage storage{&owner, 4};
        owner.vtable = child.vtable = replaced.vtable = replacement.vtable = &arena_vtable;
        owner.visits = child.visits = replaced.visits = replacement.visits = &visits;
        owner.storage = child.storage = replaced.storage = replacement.storage = &storage;
        owner.id = 0;
        child.id = 1;
        replaced.id = 2;
        replacement.id = 3;
        owner.first = &child;
        owner.second = &replaced;
        child.parent = replaced.parent = replacement.parent = &owner;
        child.owner = &owner;
        child.replacement = &replacement;
        if (kind == 0)
        {
            reset_shader_validation_expression_arena(storage);
        }
        else
        {
            reset_shader_validation_instruction_arena(storage);
        }
        require(visits == std::vector<u32>{1, 3, 0} && replaced.parent == &owner
            && storage.root == nullptr && storage.count == 0,
            "both arena resets read callback-mutated second link and clear storage after deletion");
    }
    struct ListNode : ShaderValidationListNode
    {
        ShaderValidationListStorage* storage;
        u32 expected_count;
        u32* calls;
    };
    const ShaderValidationListVtable list_vtable
    {
        [](ShaderValidationListNode& base, u32 release)
        {
            auto& node = static_cast<ListNode&>(base);
            require(release == 1 && node.storage->count == node.expected_count,
                "list deleting callback observes count before decrement");
            require(node.storage->first != &node && node.storage->last != &node,
                "list deletion changes endpoints before virtual callback");
            ++*node.calls;
        }
    };
    ListNode start{}, middle{}, end{};
    u32 calls = 0;
    ShaderValidationListStorage list{&start, &end, 3};
    start.vtable = middle.vtable = end.vtable = &list_vtable;
    start.storage = middle.storage = end.storage = &list;
    start.calls = middle.calls = end.calls = &calls;
    start.next = &middle;
    middle.previous = &start;
    middle.next = &end;
    end.previous = &middle;
    middle.expected_count = 3;
    erase_shader_validation_list_node(list, &middle);
    require(list.first == &start && list.last == &end && start.next == &middle && end.previous == &middle
        && list.count == 2 && calls == 1, "list erase delegates neighbour relinking to deleting callback");
    start.expected_count = 2;
    erase_shader_validation_list_node(list, &start);
    require(list.first == &middle && list.count == 1, "list erase updates first endpoint");
    end.expected_count = 1;
    erase_shader_validation_list_node(list, &end);
    require(list.last == &middle && list.count == 0, "list erase updates last endpoint");
    erase_shader_validation_list_node(list, nullptr);
    require(list.count == 0xFFFFFFFF && calls == 3, "null list erase wraps count with populated endpoints");
}
