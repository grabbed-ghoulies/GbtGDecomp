// Linux uses SEH-shimmed copies (see run.sh); Windows can build the original modules with MSVC.
// Only the normal, non-exceptional path is compared.
#include "shader_validation_instruction_constructor.h"
#include "shader_validation_sets.h"
#include "shader_validation_destinations.h"
#include "shader_validation_pass_lifecycle.h"
#include "shader_validation_owner_sets.h"
#include "shader_validation_instruction_lifecycle.h"
#include "shader_validation_packed_program.h"
#include "shader_validation_optimizer.h"
#include "shader_validation_scheduler.h"
#include "shader_validation_program.h"
#include <map>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
using namespace gbtg;

namespace gbtg
{
void raise_invalid_shader_validation_operand(const RuntimeCppExceptionContext&)
{
    std::puts("RAISE");
    std::exit(0);
}
}

namespace
{
// Pass destructor: list nodes, three pass trees and the registries are described on stdin.
void run_pass_destructor(int list_count, int tree_seed, bool derived_pass, bool deleting_pass)
{
    (void)tree_seed;
    std::vector<int> events;
    std::vector<ShaderValidationListNode> list_nodes(static_cast<std::size_t>(list_count));
    static std::vector<int>* sink = nullptr;
    static ShaderValidationListNode* list_base = nullptr;
    sink = &events;
    list_base = list_nodes.data();
    static const ShaderValidationListVtable list_vtable{[](ShaderValidationListNode& node, u32 flags)
        {
            sink->push_back(1000 + static_cast<int>(&node - list_base) * 10 + static_cast<int>(flags));
        }};
    for (int index = 0; index < list_count; ++index)
    {
        auto& node = list_nodes[static_cast<std::size_t>(index)];
        node.vtable = &list_vtable;
        node.previous = index > 0 ? &list_nodes[static_cast<std::size_t>(index - 1)] : nullptr;
        node.next = index + 1 < list_count ? &list_nodes[static_cast<std::size_t>(index + 1)] : nullptr;
    }
    ShaderValidationProgramPass program{};
    ShaderValidationPass& pass = program.base;
    pass.instructions = ShaderValidationList{0x002037c8, {list_count ? list_nodes.data() : nullptr, list_count ? &list_nodes.back() : nullptr,
        static_cast<u32>(list_count)}};
    std::array<std::vector<ShaderValidationPassNode>, 3> trees;
    std::array<ShaderValidationRegisteredPassTree*, 3> owners_of_trees{&pass.values, &pass.registers, &pass.special};
    for (std::size_t which = 0; which < 3; ++which)
    {
        int count;
        std::cin >> count;
        trees[which].resize(static_cast<std::size_t>(count));
        for (auto& node : trees[which])
        {
            int parent, first, second;
            std::cin >> parent >> first >> second;
            auto at = [&](int index) -> ShaderValidationPassNode*
            {
                return index < 0 ? nullptr : &trees[which][static_cast<std::size_t>(index)];
            };
            node.parent = at(parent);
            node.first = at(first);
            node.second = at(second);
        }
        owners_of_trees[which]->tree = ShaderValidationPassTreeObject{0x002037d8, 0, {count ? trees[which].data() : nullptr, static_cast<u32>(count)}};
    }
    // Retail addresses: pass at 0x10001000, its registered sub-objects at +0x24/+0x38/+0x4C.
    const auto retail_address = [&](const void* pointer) -> u32
    {
        if (pointer == &pass)
        {
            return 0x10001000;
        }
        if (pointer == &pass.values.registered)
        {
            return 0x10001024;
        }
        if (pointer == &pass.registers.registered)
        {
            return 0x10001038;
        }
        return 0x1000104C;
    };
    std::vector<ShaderValidationBalancedNode> registry_nodes(64);
    std::size_t used = 0;
    ShaderValidationBalancedTree expressions{nullptr, 0}, objects{nullptr, 0};
    const auto allocate = [&](std::size_t, u32) -> void*
    {
        return &registry_nodes[used++];
    };
    for (auto* tree : {&expressions, &objects})
    {
        int count;
        std::cin >> count;
        ShaderValidationInsertContext insert{*tree, allocate};
        for (int index = 0; index < count; ++index)
        {
            u32 value;
            std::cin >> value;
            register_shader_validation_object(insert, value);
        }
    }
    const auto node_event = [&](ShaderValidationBalancedNode& node, u32 flags)
    {
        events.push_back(2000 + static_cast<int>(&node - registry_nodes.data()) * 10 + static_cast<int>(flags));
    };
    ShaderValidationEraseContext expressions_erase{expressions, node_event};
    ShaderValidationEraseContext objects_erase{objects, node_event};
    ShaderValidationPassDestroyContext context{expressions_erase, objects_erase, [&](ShaderValidationPassNode& node, u32 flags)
        {
            for (std::size_t which = 0; which < 3; ++which)
            {
                if (!trees[which].empty() && &node >= trees[which].data() && &node < trees[which].data() + trees[which].size())
                {
                    events.push_back(static_cast<int>(which) * 100 + static_cast<int>(&node - trees[which].data()) * 10 + static_cast<int>(flags));
                }
            }
        },
        retail_address};
    std::vector<ShaderValidationBalancedNode> instruction_nodes;
    u32 pass_frees = 0;
    u32 free_flags = 0;
    bool returned_pass = true;
    if (derived_pass)
    {
        int count;
        std::cin >> count;
        instruction_nodes.resize(static_cast<std::size_t>(count));
        for (auto& node : instruction_nodes)
        {
            int parent, first, second;
            std::cin >> parent >> first >> second;
            const auto at = [&](int index) -> ShaderValidationBalancedNode*
            {
                return index < 0 ? nullptr : &instruction_nodes[static_cast<std::size_t>(index)];
            };
            node.parent = at(parent);
            node.first = at(first);
            node.second = at(second);
        }
        program.instruction_set.nodes = {count ? instruction_nodes.data() : nullptr, static_cast<u32>(count)};
        const std::function<void(ShaderValidationBalancedNode&, u32)> delete_node = [&](ShaderValidationBalancedNode& node, u32 flags)
        {
            events.push_back(300 + static_cast<int>(&node - instruction_nodes.data()) * 10 + static_cast<int>(flags));
        };
        if (deleting_pass)
        {
            u32 flags;
            std::cin >> flags;
            returned_pass = delete_shader_validation_program_pass(context, &program, static_cast<u8>(flags), delete_node,
                [&](void* allocation, u32 flags)
                {
                    if (allocation == &program)
                    {
                        ++pass_frees;
                        free_flags = flags;
                    }
                }) == &program;
        }
        else
        {
            destroy_shader_validation_program_pass(context, program, delete_node);
        }
    }
    else
    {
        destroy_shader_validation_pass(context, pass);
    }
    std::printf("%08X %08X %08X %08X %u %u %u %u %u %u", pass.base.vtable_address, pass.values.tree.vtable_address, pass.registers.tree.vtable_address,
        pass.special.tree.vtable_address, pass.values.tree.nodes.count, pass.registers.tree.nodes.count, pass.special.tree.nodes.count,
        pass.instructions.storage.count, expressions.count, objects.count);
    if (derived_pass)
    {
        std::printf(" %08X %u", program.instruction_set.vtable_address, program.instruction_set.nodes.count);
    }
    if (deleting_pass)
    {
        std::printf(" %u %08X %d", pass_frees, free_flags, returned_pass ? 1 : 0);
    }
    for (int event : events)
    {
        std::printf(" e%d", event);
    }
    std::printf("\n");
}
}

namespace
{
// Shared owner/instruction graph for 0x00190E27 and 0x00192B84. Retail addresses:
// instruction k = 0x2000 + k, operand j = 0x3000 + j.
void run_owner_sets(const std::string& kind, int owner_count, int instruction_count)
{
    std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(owner_count));
    std::vector<ShaderValidationInstruction> instructions(static_cast<std::size_t>(instruction_count));
    std::vector<ShaderValidationOperand> operands(static_cast<std::size_t>(instruction_count * 3));
    std::vector<std::vector<ShaderValidationBalancedNode>> registries(static_cast<std::size_t>(owner_count));
    auto owner_at = [&](int index) -> ShaderValidationOwner*
    {
        return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
    };
    for (int index = 0; index < instruction_count; ++index)
    {
        auto& instruction = instructions[static_cast<std::size_t>(index)];
        u32 flag;
        int output, vector_destination, scalar_destination;
        std::cin >> instruction.vector_opcode >> instruction.scalar_opcode >> flag >> output >> vector_destination >> scalar_destination >>
            instruction.sequence;
        instruction.output_flag = static_cast<u8>(flag);
        instruction.output_register = owner_at(output);
        instruction.vector_destination = owner_at(vector_destination);
        instruction.scalar_destination = owner_at(scalar_destination);
        for (auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
        {
            for (auto& channel : mask->channels)
            {
                u32 value;
                std::cin >> value;
                channel = static_cast<u8>(value);
            }
        }
        for (int slot = 0; slot < 3; ++slot)
        {
            auto& operand = operands[static_cast<std::size_t>(index * 3 + slot)];
            u32 marker;
            int parent;
            std::cin >> marker >> parent;
            for (auto& channel : operand.channel_order.channels)
            {
                std::cin >> channel;
            }
            operand.marker = static_cast<u8>(marker);
            operand.parent = owner_at(parent);
            operand.instruction = &instruction;
            operand.kind = static_cast<u32>(slot);
            instruction.operands[static_cast<std::size_t>(slot)] = &operand;
        }
    }
    for (int index = 0; index < owner_count; ++index)
    {
        auto& owner = owners[static_cast<std::size_t>(index)];
        int derived, channel_owner, operand, registry_count;
        std::cin >> owner.kind >> derived >> channel_owner >> operand;
        owner.derived_owner = owner_at(derived);
        owner.channel_owner = owner_at(channel_owner);
        owner.operand = operand < 0 ? nullptr : &operands[static_cast<std::size_t>(operand)];
        for (auto& channel : owner.channels)
        {
            int code;
            std::cin >> code;
            channel = code == 0 ? 0 : code < 0 ? shader_validation_unwritten_channel
                                               : reinterpret_cast<ShaderValidationChannelWriter>(&instructions[static_cast<std::size_t>(code - 1)]);
        }
        std::cin >> registry_count;
        auto& registry = registries[static_cast<std::size_t>(index)];
        registry.resize(static_cast<std::size_t>(registry_count));
        for (auto& node : registry)
        {
            int node_parent, first, second;
            std::cin >> node.value >> node_parent >> first >> second;
            node.value += 0x3000;
            node.parent = node_parent < 0 ? nullptr : &registry[static_cast<std::size_t>(node_parent)];
            node.first = first < 0 ? nullptr : &registry[static_cast<std::size_t>(first)];
            node.second = second < 0 ? nullptr : &registry[static_cast<std::size_t>(second)];
        }
        owner.diagnostics.nodes.root = registry.empty() ? nullptr : registry.data();
    }
    std::vector<ShaderValidationBalancedNode> set_nodes(64);
    std::size_t used = 0;
    std::vector<int> destroyed;
    ShaderValidationSetContext context{[&](std::size_t, u32) -> void*
        {
            return &set_nodes[used++];
        },
        [&](ShaderValidationBalancedNode& node, u32 flags)
        {
            // Retail's node deleting destructor frees the node through XMemFree when flag bit 0 is set.
            if ((flags & 1u) != 0)
            {
                destroyed.push_back(static_cast<int>(&node - set_nodes.data()) * 10 + 1);
            }
        },
        [&](u32 address) -> const ShaderValidationInstruction*
        {
            return &instructions[address - 0x2000];
        },
        [&](u32 address) -> ShaderValidationOperand*
        {
            return &operands[address - 0x3000];
        },
        [&](const void* object) -> u32
        {
            for (std::size_t index = 0; index < instructions.size(); ++index)
            {
                if (object == &instructions[index])
                {
                    return 0x2000 + static_cast<u32>(index);
                }
            }
            for (std::size_t index = 0; index < operands.size(); ++index)
            {
                if (object == &operands[index])
                {
                    return 0x3000 + static_cast<u32>(index);
                }
            }
            return 0;
        }};
    int target;
    std::cin >> target;
    if (kind == "destinations")
    {
        int mode, instruction_index, owner_index, writer_code;
        u32 channel;
        ShaderValidationChannelMask mask{0xDEADBEEF, {}};
        std::cin >> mode >> instruction_index >> owner_index >> writer_code >> channel;
        for (auto& value : mask.channels)
        {
            u32 input;
            std::cin >> input;
            value = static_cast<u8>(input);
        }
        for (auto& instruction : instructions)
        {
            for (auto& value : instruction.output_mask.channels)
            {
                u32 input;
                std::cin >> input;
                value = static_cast<u8>(input);
            }
        }
        auto& instruction = instructions[static_cast<std::size_t>(instruction_index)];
        auto* owner = owner_at(owner_index);
        const auto writer = mode >= 9 || writer_code == 0 ? 0 : writer_code < 0 ? shader_validation_unwritten_channel :
            reinterpret_cast<ShaderValidationChannelWriter>(&instructions[static_cast<std::size_t>(writer_code - 1)]);
        int result = -2;
        switch (mode)
        {
        case 0:
            set_shader_validation_channel_writer(owners[static_cast<std::size_t>(target)], channel, writer);
            break;
        case 1:
            remove_shader_validation_destination_channel(instruction, channel, owner);
            break;
        case 2:
            result = replace_shader_validation_output_destination(instruction, owner, mask) == owner ? owner_index : -3;
            break;
        case 3:
            result = replace_shader_validation_vector_destination(instruction, owner, mask) == owner ? owner_index : -3;
            break;
        case 4:
            result = replace_shader_validation_scalar_destination(instruction, owner, mask) == owner ? owner_index : -3;
            break;
        case 5:
            set_shader_validation_output_destination(instruction, owner);
            break;
        case 6:
            set_shader_validation_vector_destination(instruction, owner);
            break;
        case 7:
            set_shader_validation_scalar_destination(instruction, owner);
            break;
        case 8:
            set_shader_validation_masked_writers(owners[static_cast<std::size_t>(target)], mask, writer);
            break;
        case 9:
            result = merge_shader_validation_duplicate_channel(owners[static_cast<std::size_t>(target)], channel,
                static_cast<u32>(writer_code), context.operand_at) ? 1 : 0;
            break;
        case 10:
            merge_shader_validation_duplicate_channels(owners[static_cast<std::size_t>(target)], context.operand_at);
            break;
        }
        std::printf("%d |", result);
        for (const auto& current : owners)
        {
            for (const auto value : current.channels)
            {
                std::printf(" %u", value == 0 ? 0 : value == shader_validation_unwritten_channel ? 0xFFFFFFFFu :
                    context.address_of(reinterpret_cast<const void*>(value)) - 0x2000 + 1);
            }
        }
        std::printf(" |");
        for (const auto& current : instructions)
        {
            for (const auto* destination : {current.output_register, current.vector_destination, current.scalar_destination})
            {
                std::printf(" %d", destination == nullptr ? -1 : static_cast<int>(destination - owners.data()));
            }
            for (const auto* current_mask : {&current.output_mask, &current.vector_mask, &current.scalar_mask})
            {
                std::printf(" %08X", current_mask->vtable_address);
                for (const auto value : current_mask->channels)
                {
                    std::printf(" %u", value);
                }
            }
        }
        std::printf(" |");
        for (const auto& operand : operands)
        {
            for (const auto channel : operand.channel_order.channels)
            {
                std::printf(" %u", channel);
            }
        }
        std::printf("\n");
        return;
    }
    if (kind == "equiv")
    {
        int first, second, mode, active;
        std::cin >> first >> second >> mode >> active;
        auto& operand = operands[static_cast<std::size_t>(first)];
        auto& other = operands[static_cast<std::size_t>(second)];
        if (mode == 1 || mode == 2)
        {
            other.marker = operand.marker;
        }
        if (mode >= 1)
        {
            other.parent = operand.parent;
        }
        // The other operand's final order is always sent, covering mode 2 and its perturbation.
        for (auto& channel : other.channel_order.channels)
        {
            std::cin >> channel;
        }
        for (auto& owner : owners)
        {
            std::cin >> owner.source;
        }
        std::printf("%d\n", shader_validation_operands_equivalent(operand, other, owner_at(active)) ? 1 : 0);
        return;
    }
    if (kind == "unwritten" || kind == "unuseddest")
    {
        const bool result = kind == "unwritten" ?
            shader_validation_owner_has_unwritten_dependency(owners[static_cast<std::size_t>(target)], context.operand_at) :
            shader_validation_instruction_destinations_unused(instructions[static_cast<std::size_t>(target)], context.operand_at);
        std::printf("%d\n", result ? 1 : 0);
        return;
    }
    if (kind == "ownerreader" || kind == "channelreader")
    {
        u32 channel;
        std::cin >> channel;
        const auto& owner = owners[static_cast<std::size_t>(target)];
        const bool result = kind == "ownerreader" ? shader_validation_owner_channel_has_reader(owner, channel, context.operand_at) :
            shader_validation_channel_owner_has_reader(owner, channel, context.operand_at);
        std::printf("%d\n", result ? 1 : 0);
        return;
    }
    if (kind == "retire" || kind == "installreg" || kind == "installpass")
    {
        int registry_count, key_count, retire_kind;
        std::cin >> registry_count;
        std::vector<ShaderValidationBalancedNode> registry_nodes(32);
        std::size_t registry_used = 0;
        ShaderValidationBalancedTree registry{nullptr, 0};
        ShaderValidationInsertContext insert{registry, [&](std::size_t, u32) -> void*
            {
                return &registry_nodes[registry_used++];
            }};
        for (int index = 0; index < registry_count; ++index)
        {
            u32 value;
            std::cin >> value;
            register_shader_validation_object(insert, value + 0x3000);
        }
        std::vector<ShaderValidationPassNode> pass_nodes(32);
        std::size_t pass_used = 0;
        const std::function<void*(std::size_t, u32)> allocate_pass_node = [&](std::size_t, u32) -> void*
        {
            return &pass_nodes[pass_used++];
        };
        std::vector<u32> freed;
        const std::function<void(ShaderValidationPassNode&, u32)> destroy_pass_node = [&](ShaderValidationPassNode& node, u32 flags)
        {
            if ((flags & 1u) != 0)
            {
                freed.push_back(node.value);
            }
        };
        ShaderValidationPass pass{};
        std::cin >> key_count;
        for (int index = 0; index < key_count; ++index)
        {
            u32 key;
            int data;
            std::cin >> key >> data;
            ShaderValidationPassNode* result;
            set_shader_validation_pass_value(pass.registers.tree.nodes, allocate_pass_node, &result, key, &owners[static_cast<std::size_t>(data)]);
        }
        std::cin >> retire_kind;
        bool result;
        if (kind == "installreg")
        {
            int special_owner, allocation_fail;
            std::cin >> special_owner >> allocation_fail;
            ShaderValidationPassNode special{};
            special.value = 0x100;
            special.data = owner_at(special_owner);
            if (special_owner != -2)
            {
                pass.special.tree.nodes = {&special, 1};
            }
            owners[static_cast<std::size_t>(target)].source = static_cast<u32>(retire_kind);
            int calls = 0;
            context.allocate = [&](std::size_t size, u32 flags) -> void*
            {
                if (calls++ == allocation_fail)
                {
                    return nullptr;
                }
                return allocate_pass_node(size, flags);
            };
            result = replace_shader_validation_register_owner(pass, owners[static_cast<std::size_t>(target)], destroy_pass_node, context) ==
                owner_at(target);
        }
        else if (kind == "installpass")
        {
            pass.owner = owner_at(retire_kind);
            result = set_shader_validation_pass_owner(pass, owner_at(target), context) == owner_at(target);
        }
        else
        {
            result = retire_shader_validation_register(pass, retire_kind, registry, destroy_pass_node, allocate_pass_node, context);
        }
        std::printf("%d %u |", result ? 1 : 0, pass.registers.tree.nodes.count);
        const std::function<void(const ShaderValidationPassNode*)> walk = [&](const ShaderValidationPassNode* node)
        {
            if (node != nullptr)
            {
                walk(node->first);
                std::printf(" %u:%d", node->value, static_cast<int>(static_cast<const ShaderValidationOwner*>(node->data) - owners.data()));
                walk(node->second);
            }
        };
        walk(pass.registers.tree.nodes.root);
        std::printf(" |");
        for (const auto& owner : owners)
        {
            std::printf(" %u,%u", owner.kind, owner.source);
        }
        if (kind == "installreg" || kind == "installpass")
        {
            const auto index_of = [&](const ShaderValidationOwner* owner) -> int
            {
                return owner == nullptr ? -1 : static_cast<int>(owner - owners.data());
            };
            std::printf(" | %d", index_of(pass.owner));
            for (const auto& owner : owners)
            {
                std::printf(" %d %d", index_of(owner.derived_owner), index_of(owner.channel_owner));
            }
        }
        for (u32 key : freed)
        {
            std::printf(" f%u", key);
        }
        std::printf("\n");
        return;
    }
    if (kind == "movechannels" || kind == "reorder" || kind == "link")
    {
        ShaderValidationChannelMask mask{0x00203790, {}};
        ShaderValidationChannelOrder order{0x00203794, {}};
        for (u32 index = 0; index < 4; ++index)
        {
            u32 value;
            std::cin >> value;
            mask.channels[index] = static_cast<u8>(value);
            order.channels[index] = value;
        }
        u32 climb, propagate;
        std::cin >> climb >> propagate;
        bool result;
        if (kind == "link")
        {
            ShaderValidationChannelMask other_mask{0x00203790, {}};
            for (auto& channel : other_mask.channels)
            {
                u32 value;
                std::cin >> value;
                channel = static_cast<u8>(value);
            }
            const int other = climb == 0xFFFFFFFF ? -1 : static_cast<int>(climb);
            result = link_shader_validation_owners(owner_at(target), owner_at(other),
                static_cast<u8>(propagate), mask, other_mask, context);
        }
        else if (kind == "reorder")
        {
            result = reorder_shader_validation_owner_channels(owners[static_cast<std::size_t>(target)], order, context);
        }
        else
        {
            result = move_shader_validation_masked_channels(owners[static_cast<std::size_t>(target)], mask,
                static_cast<u8>(climb), static_cast<u8>(propagate), context);
        }
        std::printf("%d |", result ? 1 : 0);
        for (const auto& owner : owners)
        {
            for (const auto writer : owner.channels)
            {
                std::printf(" %u", writer == 0 ? 0 : writer == shader_validation_unwritten_channel ? 0xFFFFFFFFu :
                    context.address_of(reinterpret_cast<const void*>(writer)) - 0x2000 + 1);
            }
        }
        std::printf(" |");
        for (const auto& instruction : instructions)
        {
            for (const auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
            {
                for (const auto channel : mask->channels)
                {
                    std::printf(" %u", channel);
                }
            }
        }
        std::printf(" |");
        for (const auto& operand : operands)
        {
            for (const auto channel : operand.channel_order.channels)
            {
                std::printf(" %u", channel);
            }
        }
        if (kind == "link")
        {
            std::printf(" |");
            const auto index_of = [&](const ShaderValidationOwner* owner) -> int
            {
                return owner == nullptr ? -1 : static_cast<int>(owner - owners.data());
            };
            for (const auto& owner : owners)
            {
                std::printf(" %d %d", index_of(owner.derived_owner), index_of(owner.channel_owner));
            }
        }
        std::printf("\n");
        return;
    }
    if (kind == "readers" || kind == "readmask" || kind == "dependencies")
    {
        int registry_count, instruction, climb_derived, climb_channel;
        std::cin >> registry_count;
        std::vector<ShaderValidationBalancedNode> registry_nodes(32);
        std::size_t registry_used = 0;
        ShaderValidationBalancedTree registry{nullptr, 0};
        ShaderValidationInsertContext insert{registry, [&](std::size_t, u32) -> void*
            {
                return &registry_nodes[registry_used++];
            }};
        for (int index = 0; index < registry_count; ++index)
        {
            u32 value;
            std::cin >> value;
            register_shader_validation_object(insert, value + 0x3000);
        }
        ShaderValidationChannelMask mask{0x00203790, {}};
        for (auto& channel : mask.channels)
        {
            u32 value;
            std::cin >> value;
            channel = static_cast<u8>(value);
        }
        std::cin >> instruction >> climb_derived >> climb_channel;
        if (kind == "dependencies")
        {
            int pass_owner, writer_count;
            std::cin >> pass_owner >> writer_count;
            ShaderValidationPass pass{};
            pass.owner = owner_at(pass_owner);
            std::vector<ShaderValidationBalancedNode> writer_nodes(32);
            std::size_t writer_used = 0;
            ShaderValidationBalancedTree writers{nullptr, 0};
            ShaderValidationInsertContext insert_writers{writers, [&](std::size_t, u32) -> void*
                {
                    return &writer_nodes[writer_used++];
                }};
            for (int index = 0; index < writer_count; ++index)
            {
                u32 value;
                std::cin >> value;
                register_shader_validation_instruction(insert_writers, value + 0x2000, context.instruction_at);
            }
            std::printf("%d\n", shader_validation_owner_dependencies_registered(owners[static_cast<std::size_t>(target)], pass,
                writers, registry, instruction < 0 ? nullptr : &instructions[static_cast<std::size_t>(instruction)], mask, context) ? 1 : 0);
            return;
        }
        if (kind == "readmask")
        {
            ShaderValidationChannelMask output;
            auto* returned = shader_validation_unregistered_reader_channels(owners[static_cast<std::size_t>(target)], output, registry,
                instruction < 0 ? nullptr : &instructions[static_cast<std::size_t>(instruction)], context);
            std::printf("%d %08X %u %u %u %u\n", returned == &output ? 1 : 0, output.vtable_address,
                output.channels[0], output.channels[1], output.channels[2], output.channels[3]);
            return;
        }
        const bool result = shader_validation_owner_readers_registered(owners[static_cast<std::size_t>(target)], registry, mask,
            instruction < 0 ? nullptr : &instructions[static_cast<std::size_t>(instruction)], static_cast<u8>(climb_derived),
            static_cast<u8>(climb_channel), context);
        std::printf("%d\n", result ? 1 : 0);
        return;
    }
    const bool result = negate_shader_validation_owner_writers(owners[static_cast<std::size_t>(target)], context);
    std::printf("%d %zu", result ? 1 : 0, used);
    for (int event : destroyed)
    {
        std::printf(" d%d", event);
    }
    for (const auto& operand : operands)
    {
        std::printf(" %u", operand.marker);
    }
    std::printf("\n");
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        int fail, active_kind;
        std::cin >> fail >> active_kind;
        if (kind == "passdtor" || kind == "programdtor" || kind == "programdelete")
        {
            run_pass_destructor(fail, active_kind, kind != "passdtor", kind == "programdelete");
            continue;
        }
        if (kind == "regtree")
        {
            ShaderValidationRegisteredPassTree tree{};
            u32 words[5];
            for (u32& word : words)
            {
                std::cin >> word;
            }
            std::memcpy(&tree.tree.vtable_address, &words[0], 4);
            std::memcpy(&tree.tree.retained, &words[1], 4);
            tree.tree.nodes.root = reinterpret_cast<ShaderValidationPassNode*>(static_cast<std::uintptr_t>(words[2]));
            tree.tree.nodes.count = words[3];
            tree.registered.vtable_address = words[4];
            ShaderValidationRegisteredPassTree* returned = construct_shader_validation_registered_pass_tree(tree);
            std::printf("%d %08X %08X %08X %08X %08X\n", returned == &tree ? 1 : 0, tree.tree.vtable_address, tree.tree.retained,
                static_cast<u32>(reinterpret_cast<std::uintptr_t>(tree.tree.nodes.root)), tree.tree.nodes.count, tree.registered.vtable_address);
            continue;
        }
        if (kind == "destinations" || kind == "readers" || kind == "readmask" || kind == "negate" || kind == "equiv" || kind == "retire" ||
            kind == "movechannels" || kind == "reorder" || kind == "link" || kind == "dependencies" ||
            kind == "unwritten" || kind == "unuseddest" || kind == "ownerreader" || kind == "channelreader" || kind == "installreg" || kind == "installpass")
        {
            run_owner_sets(kind, fail, active_kind);
            continue;
        }
        alignas(16) static std::array<std::array<unsigned char, 512>, 1024> pool;
        for (auto& block : pool)
        {
            block.fill(0xCD);
        }
        int calls = 0;
        std::size_t used = 0;
        const u32 base = 0x10040000;
        ShaderValidationBalancedTree objects{nullptr, 0};
        ShaderValidationOwner active{};
        ShaderValidationOwner* active_owner = active_kind ? &active : nullptr;
        std::vector<ShaderValidationBalancedTree> diagnostic_trees(4, ShaderValidationBalancedTree{nullptr, 0});
        const auto allocate = [&](std::size_t, u32) -> void*
        {
            if (calls++ == fail)
            {
                return nullptr;
            }
            return pool[used++].data();
        };
        ShaderValidationInsertContext insert{objects, allocate};
        ShaderValidationEraseContext erase{objects, [](ShaderValidationBalancedNode&, u32)
            {
            }};
        RuntimeCppExceptionContext exceptions{retail_cpp_exception_template, nullptr};
        ShaderValidationInsertContext diagnostics{diagnostic_trees[0], allocate};
        ShaderValidationConstructorContext context{insert, erase, exceptions, active_owner, allocate, [](void*, u32)
            {
            },
            [&](const void* pointer) -> u32
            {
                return base + static_cast<u32>((static_cast<const unsigned char*>(pointer) - pool[0].data()) / 512) * 0x100;
            },
            [&](ShaderValidationOwner&) -> ShaderValidationInsertContext&
            {
                return diagnostics;
            },
            [](ShaderValidationBalancedNode&, u32)
            {
            }};
        const auto address = [&](const void* pointer) -> long long
        {
            return pointer == nullptr ? -1 : static_cast<long long>(context.retail_address(pointer));
        };
        if (kind == "dependencybuild")
        {
            std::array<ShaderValidationOwner, 4> owners{};
            std::array<ShaderValidationInstruction*, 4> instructions{};
            std::array<ShaderValidationBalancedNode, 4> diagnostic_nodes{};
            const auto instruction_at = [&](u32 value) -> ShaderValidationInstruction*
            {
                return reinterpret_cast<ShaderValidationInstruction*>(pool[(value - base) / 0x100].data());
            };
            context.retail_address = [&](const void* pointer) -> u32
            {
                if (pointer == nullptr)
                {
                    return 0;
                }
                if (pointer == &active)
                {
                    return 0x10009000;
                }
                for (u32 index = 0; index < 4; ++index)
                {
                    if (pointer == &owners[index])
                    {
                        return 0x1000B000 + index * 0x40;
                    }
                }
                return base + static_cast<u32>((static_cast<const unsigned char*>(pointer) - pool[0].data()) / 512) * 0x100;
            };
            for (auto& instruction : instructions)
            {
                instruction = create_shader_validation_instruction(context);
            }
            u32 include, assignment_kind, query, existing;
            std::cin >> include >> assignment_kind >> query >> existing;
            for (u32 index = 0; index < 4; ++index)
            {
                u32 reader, chain, writer_mask;
                auto& instruction = *instructions[index];
                std::cin >> instruction.sequence >> instruction.vector_opcode >> reader >> chain >> writer_mask >> owners[index].kind;
                instruction.scalar_opcode = 0;
                instruction.output_register = &owners[index];
                for (auto& channel : instruction.output_mask.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
                for (u32 slot = 0; slot < 3; ++slot)
                {
                    instruction.operands[slot]->parent = &owners[index];
                    for (auto& channel : instruction.operands[slot]->channel_order.channels)
                    {
                        std::cin >> channel;
                    }
                }
                if (index < 3)
                {
                    if (reader != 0)
                    {
                        diagnostic_nodes[index].value = context.retail_address(instructions[index + 1]->operands[0]);
                        owners[index].diagnostics.nodes = {&diagnostic_nodes[index], 1};
                    }
                    if (chain != 0)
                    {
                        owners[index].channel_owner = &owners[index + 1];
                    }
                    for (u32 channel = 0; channel < 4; ++channel)
                    {
                        if ((writer_mask & (1u << channel)) != 0)
                        {
                            owners[index].channels[channel] = reinterpret_cast<ShaderValidationChannelWriter>(instructions[index + 1]);
                        }
                    }
                }
            }
            ShaderValidationBalancedTree set_registry{nullptr, 0};
            ShaderValidationInsertContext set_insert{set_registry, allocate};
            ShaderValidationSetContext sets{allocate, context.deleting_destructor, instruction_at,
                [&](u32 value) -> ShaderValidationOperand*
                {
                    return reinterpret_cast<ShaderValidationOperand*>(pool[(value - base) / 0x100].data());
                }, context.retail_address};
            ShaderValidationDependencyContext dependency_context{context, set_insert, sets, static_cast<u8>(include)};
            for (auto* instruction : instructions)
            {
                build_shader_validation_instruction_dependencies(dependency_context, *instruction);
                std::printf("%08X %u |", context.retail_address(instruction->instruction_set), instruction->instruction_set_count);
                ShaderValidationBalancedNode* node;
                find_shader_validation_tree_last(instruction->instruction_set->nodes, &node);
                while (node != nullptr)
                {
                    std::printf(" %08X", node->value);
                    advance_shader_validation_arena_iterator(node);
                }
                std::printf(" ; ");
            }
            for (u32 first = 0; first < 4; ++first)
            {
                for (u32 second = 0; second < 4; ++second)
                {
                    std::printf("%d%d", shader_validation_instruction_has_dependency(dependency_context, *instructions[first],
                        context.retail_address(instructions[second])) ? 1 : 0,
                        shader_validation_instruction_priority_precedes(dependency_context, context.retail_address(instructions[first]),
                            context.retail_address(instructions[second])) ? 1 : 0);
                }
            }
            std::printf(" |");
            for (auto* instruction : instructions)
            {
                for (auto* operand : instruction->operands)
                {
                    for (auto& owner : owners)
                    {
                        ShaderValidationChannelMask full;
                        construct_shader_validation_channel_mask(full, 0xF);
                        std::printf("%d", shader_validation_operand_accepts_parent(dependency_context, *operand, owner, full) ? 1 : 0);
                    }
                }
            }
            std::array<ShaderValidationListNode, 4> list{};
            for (u32 index = 0; index < 4; ++index)
            {
                list[index].value = instructions[index];
                list[index].next = index < 3 ? &list[index + 1] : nullptr;
            }
            select_shader_validation_list_priority(dependency_context, list[0], nullptr);
            std::printf(" |");
            for (auto& node : list)
            {
                std::printf(" %08X", context.retail_address(node.value));
            }
            std::map<ShaderValidationOwner*, ShaderValidationInsertContext> registries;
            context.diagnostic_registry = [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, allocate}).first->second;
            };
            std::printf(" M");
            for (u32 index = 1; index < 4; ++index)
            {
                const bool merged = merge_shader_validation_owners(dependency_context, owners[0], &owners[index], 1,
                    instructions[0]->output_mask, instructions[index]->output_mask);
                std::printf("%d", merged ? 1 : 0);
            }
            for (auto& owner : owners)
            {
                for (auto writer : owner.channels)
                {
                    std::printf(",%08X", context.retail_address(reinterpret_cast<void*>(writer)));
                }
            }
            for (auto* instruction : instructions)
            {
                for (auto* operand : instruction->operands)
                {
                    for (auto channel : operand->channel_order.channels)
                    {
                        std::printf(",%u", channel);
                    }
                }
                for (auto channel : instruction->output_mask.channels)
                {
                    std::printf(",%u", channel);
                }
            }
            instructions[0]->vector_opcode = 1;
            instructions[0]->scalar_opcode = 0;
            instructions[0]->output_flag = 1;
            instructions[0]->vector_destination = &owners[0];
            instructions[0]->vector_mask = instructions[0]->output_mask;
            const ShaderValidationMoveFrame move_frame{static_cast<u8>(instructions[0]->sequence)};
            const bool rewritten = rewrite_shader_validation_move(dependency_context, *instructions[0], move_frame);
            std::printf(" W%d,%08X", rewritten ? 1 : 0, context.retail_address(instructions[0]->vector_destination));
            for (auto* instruction : instructions)
            {
                for (auto channel : instruction->output_mask.channels)
                {
                    std::printf(",%u", channel);
                }
                for (auto channel : instruction->vector_mask.channels)
                {
                    std::printf(",%u", channel);
                }
                for (auto* operand : instruction->operands)
                {
                    for (auto channel : operand->channel_order.channels)
                    {
                        std::printf(",%u", channel);
                    }
                }
            }
            owners[0].kind = 1;
            owners[3].kind = 6;
            owners[0].channels.fill(0);
            owners[3].channels.fill(0);
            owners[0].channel_owner = nullptr;
            owners[0].derived_owner = &owners[3];
            owners[3].channel_owner = &owners[0];
            if (owners[0].diagnostics.nodes.count != 0)
            {
                instructions[1]->operands[0]->parent = &owners[0];
            }
            move_shader_validation_readers_to_derived(dependency_context, owners[0], 0);
            const bool rotated = rotate_shader_validation_kind_six_owner(dependency_context, owners[0]);
            const bool detached = detach_unused_shader_validation_derived_owner(dependency_context, owners[0]);
            std::printf(" | %d %d", rotated ? 1 : 0, detached ? 1 : 0);
            for (auto& owner : owners)
            {
                std::printf(" %08X,%08X,%u", context.retail_address(owner.derived_owner),
                    context.retail_address(owner.channel_owner), owner.diagnostics.nodes.count);
            }
            std::printf(" %08X", context.retail_address(instructions[1]->operands[0]->parent));
            for (auto& owner : owners)
            {
                owner.channels.fill(0);
                owner.derived_owner = nullptr;
                owner.channel_owner = nullptr;
                owner.diagnostics.nodes = {nullptr, 0};
            }
            owners[0].kind = 0;
            owners[0].source = instructions[0]->sequence % 14;
            owners[1].kind = 1;
            owners[1].source = assignment_kind;
            ShaderValidationPass assignment_pass{};
            const auto original_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                return pointer == &assignment_pass ? 0x1000A000 : original_address(pointer);
            };
            if (existing != 0)
            {
                ShaderValidationPassNode* inserted;
                set_shader_validation_pass_value(assignment_pass.registers.tree.nodes, allocate, &inserted, assignment_kind, &owners[1]);
            }
            ShaderValidationBalancedTree empty_readers{nullptr, 0};
            ShaderValidationChannelMask assignment_mask;
            construct_shader_validation_channel_mask(assignment_mask, 0xF);
            const bool assigned = assign_shader_validation_owner_register(dependency_context, assignment_pass, assignment_kind,
                owners[0], empty_readers, instructions[0], assignment_mask, static_cast<u8>(query));
            const i32 chosen = choose_shader_validation_owner_register(dependency_context, assignment_pass, owners[2],
                empty_readers, *instructions[0], assignment_mask, static_cast<u8>(query));
            const bool scalar_assigned = assign_shader_validation_owner_scalar_register(dependency_context, assignment_pass,
                owners[3], empty_readers, instructions[0], assignment_mask, static_cast<u8>(query));
            std::printf(" | %d %d %d %u", assigned ? 1 : 0, chosen, scalar_assigned ? 1 : 0, assignment_pass.registers.tree.nodes.count);
            for (auto& owner : owners)
            {
                std::printf(" %u,%u,%08X,%08X", owner.kind, owner.source, context.retail_address(owner.derived_owner),
                    context.retail_address(owner.channel_owner));
            }
            for (auto* instruction : instructions)
            {
                i32 available;
                const bool accepted = shader_validation_instruction_registers_available(dependency_context, assignment_pass,
                    *instruction, empty_readers, &available);
                std::printf(" %d,%d", accepted ? 1 : 0, available);
            }
            for (auto* instruction : instructions)
            {
                instruction->vector_destination = (instruction->sequence & 1) != 0 ? &owners[0] : nullptr;
                instruction->scalar_destination = (instruction->sequence & 2) != 0 ? &owners[3] : nullptr;
                instruction->scalar_opcode = instruction->sequence % 8;
                instruction->vector_mask = assignment_mask;
                instruction->scalar_mask = assignment_mask;
                const bool ready = shader_validation_instruction_ready(dependency_context, assignment_pass,
                    *instruction, empty_readers, empty_readers);
                std::printf(" R%d,%u", ready ? 1 : 0, assignment_pass.registers.tree.nodes.count);
            }
            for (auto* instruction : instructions)
            {
                instruction->output_register = nullptr;
            }
            std::printf(" C");
            for (auto* instruction : instructions)
            {
                for (auto* other : instructions)
                {
                    const bool compatible = shader_validation_instructions_merge_compatible(dependency_context, *instruction, *other);
                    const bool on_pass = shader_validation_instructions_merge_on_pass(dependency_context, assignment_pass,
                        *instruction, *other, empty_readers);
                    std::printf("%d%d", compatible ? 1 : 0, on_pass ? 1 : 0);
                }
            }
            for (auto& owner : owners)
            {
                owner.channels.fill(0);
                owner.derived_owner = nullptr;
                owner.channel_owner = nullptr;
                owner.diagnostics.nodes = {nullptr, 0};
            }
            owners[0].source = (instructions[0]->sequence & 1) != 0 ? 0x100 : 0;
            owners[1].source = owners[0].source;
            owners[0].channels[instructions[0]->sequence % 4] = shader_validation_unwritten_channel;
            owners[1].channels[(instructions[0]->sequence >> 2) % 4] = shader_validation_unwritten_channel;
            ShaderValidationPassNode* inserted;
            set_shader_validation_pass_value(assignment_pass.special.tree.nodes, allocate, &inserted, owners[0].source, &owners[1]);
            const auto pass_delete = [](ShaderValidationPassNode&, u32)
            {
            };
            auto* special = replace_shader_validation_special_owner(dependency_context, assignment_pass, owners[0], pass_delete);
            std::printf(" S%08X,%u,%u", context.retail_address(special), assignment_pass.special.tree.nodes.count,
                assignment_pass.registers.tree.nodes.count);
            for (u32 index = 0; index < 2; ++index)
            {
                std::printf(",%08X,%08X", context.retail_address(owners[index].derived_owner),
                    context.retail_address(owners[index].channel_owner));
            }
            auto* merge_first = create_shader_validation_instruction(context);
            auto* merge_second = create_shader_validation_instruction(context);
            merge_first->vector_opcode = instructions[0]->sequence % 5;
            merge_second->vector_opcode = merge_first->vector_opcode;
            merge_second->scalar_opcode = (instructions[0]->sequence >> 4) % 8;
            owners[2].kind = 1;
            for (u32 slot = 0; slot < 3; ++slot)
            {
                reparent_shader_validation_operand(context, *merge_first->operands[slot], &owners[2]);
                reparent_shader_validation_operand(context, *merge_second->operands[slot], &owners[2]);
            }
            ShaderValidationInstructionDestroyContext destroy_context{dependency_context,
                [&](u32 value) -> ShaderValidationBalancedChildTree*
                {
                    return reinterpret_cast<ShaderValidationBalancedChildTree*>(pool[(value - base) / 0x100].data());
                }};
            const bool merged = merge_shader_validation_instructions(destroy_context, *merge_first, *merge_second);
            std::printf(" I%d,%u,%u,%u,%u,%u,%u", merged ? 1 : 0, merge_first->vector_opcode, merge_first->scalar_opcode,
                merge_first->instruction_set_count, set_registry.count, objects.count, owners[2].diagnostics.nodes.count);
            delete_shader_validation_instruction(destroy_context, merge_first, 1);
            std::printf(" D%u,%u,%u", set_registry.count, objects.count, owners[2].diagnostics.nodes.count);
            ShaderValidationProgramPass program{};
            const auto previous_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                if (pointer == &program || pointer == &program.base)
                {
                    return 0x1000A400;
                }
                if (pointer == &program.base.values.registered)
                {
                    return 0x1000A424;
                }
                if (pointer == &program.base.registers.registered)
                {
                    return 0x1000A438;
                }
                if (pointer == &program.base.special.registered)
                {
                    return 0x1000A44C;
                }
                return previous_address(pointer);
            };
            context.active_owner = &active;
            ShaderValidationBalancedTree expression_registry{nullptr, 0};
            ShaderValidationInsertContext expression_insert{expression_registry, allocate};
            const ShaderValidationListVtable list_vtable{[](ShaderValidationListNode&, u32)
                {
                }};
            ShaderValidationProgramConstructorContext program_context{dependency_context, expression_insert, pass_delete, &list_vtable};
            std::array<u32, 64> packed{2};
            for (u32 index = 0; index < 2; ++index)
            {
                const u32 value = instructions[index]->sequence;
                packed[1 + 3 * index] = (value & 0x0F0FFFFF) | ((value % 12) << 20) | 0x2000F000;
                packed[2 + 3 * index] = (value & 0x0FF80000) | (value & 0x1FF0) | (value % 12) | 0x20004000 | (((value >> 4) % 12) << 15);
                packed[3 + 3 * index] = (value & 0x01FFFFF0) | (value % 12) | ((value % 14) << 25) | (((value >> 8) % 8) << 29);
                if ((instructions[0]->sequence & 0x40) != 0)
                {
                    packed[1 + 3 * index] = (index == 0 ? 0x2F000000 : 0x2000F000) | ((value & 7) << 3);
                    packed[2 + 3 * index] = (value & 0x0FF80000) | (value & 0x1FF0)
                        | (index == 0 ? 0x20004000 : 0x10004000);
                    packed[3 + 3 * index] = (value & 0x01FFFFF0) | 0x02000000;
                }
            }
            construct_shader_validation_program_pass(program_context, program, packed.data());
            std::printf(" P%u,%u,%u,%u", program.base.registers.tree.nodes.count, program.base.special.tree.nodes.count,
                program.instruction_set.nodes.count, expression_registry.count);
            for (auto* owner : program.base.initial_registers)
            {
                std::printf(";%08X,%u,%u,%08X,%08X,%u", context.retail_address(owner), owner->kind, owner->source,
                    context.retail_address(owner->derived_owner), context.retail_address(owner->channel_owner), owner->diagnostics.nodes.count);
            }
            ShaderValidationBalancedNode* program_node;
            find_shader_validation_tree_last(program.instruction_set.nodes, &program_node);
            while (program_node != nullptr)
            {
                const auto& instruction = *instruction_at(program_node->value);
                std::printf(";%08X,%u,%u,%u,%08X,%08X,%08X", program_node->value, instruction.sequence,
                    instruction.vector_opcode, instruction.scalar_opcode, context.retail_address(instruction.output_register),
                    context.retail_address(instruction.vector_destination), context.retail_address(instruction.scalar_destination));
                for (const auto* mask : {&instruction.output_mask, &instruction.vector_mask, &instruction.scalar_mask})
                {
                    for (auto channel : mask->channels)
                    {
                        std::printf(",%u", channel);
                    }
                }
                for (const auto* operand : instruction.operands)
                {
                    std::printf(",%08X,%u", context.retail_address(operand->parent), operand->marker);
                    for (auto channel : operand->channel_order.channels)
                    {
                        std::printf(",%u", channel);
                    }
                }
                advance_shader_validation_arena_iterator(program_node);
            }
            sets.address_of = [&](const void* pointer)
            {
                return context.retail_address(pointer);
            };
            std::map<const void*, u32> stack_addresses;
            const auto program_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                const auto raw = reinterpret_cast<std::uintptr_t>(pointer);
                const auto start = reinterpret_cast<std::uintptr_t>(pool.data());
                if (pointer != nullptr && (raw < start || raw >= start + sizeof(pool))
                    && pointer != &active && pointer != &program && pointer != &program.base
                    && pointer != &program.base.values.registered && pointer != &program.base.registers.registered
                    && pointer != &program.base.special.registered)
                {
                    return stack_addresses.try_emplace(pointer, 0x100F0000 + static_cast<u32>(stack_addresses.size()) * 0x100).first->second;
                }
                return program_address(pointer);
            };
            ShaderValidationOwnerDestroyContext owner_destroy{destroy_context, expression_registry,
                [&](u32 value, ShaderValidationOwner& owner)
                {
                    for (auto* tree : {&program.base.values, &program.base.registers, &program.base.special})
                    {
                        if (value == context.retail_address(&tree->registered))
                        {
                            remove_shader_validation_pass_owner(*tree, owner, pass_delete);
                            return;
                        }
                    }
                    std::abort();
                }};
            ShaderValidationMoveFrame optimizer_frame{0};
            ShaderValidationOptimizationContext optimization{owner_destroy, expression_insert, optimizer_frame,
                [&](u32 value) -> ShaderValidationOwner*
                {
                    if (value == 0x10009000)
                    {
                        return &active;
                    }
                    return reinterpret_cast<ShaderValidationOwner*>(pool[(value - base) / 0x100].data());
                }};
            dependency_context.include_operand_writers = 1;
            optimize_shader_validation_program(optimization, program, 1);
            std::printf(" Q%u,%u,%u,%u", program.instruction_set.nodes.count, expression_registry.count,
                program.base.registers.tree.nodes.count, program.base.special.tree.nodes.count);
            u8 conservative = 0;
            ShaderValidationSerializerFrame serializer_frame{};
            ShaderValidationScheduleContext schedule{program_context, destroy_context, conservative, serializer_frame};
            dependency_context.include_operand_writers = 0;
            const bool scheduled = schedule_shader_validation_program(schedule, program);
            std::printf(" T%d,%u", scheduled ? 1 : 0, program.base.instructions.storage.count);
            for (auto* node = program.base.instructions.storage.first; node != nullptr; node = node->next)
            {
                const auto& instruction = *static_cast<ShaderValidationInstruction*>(node->value);
                std::printf(";%08X,%u,%u,%u", context.retail_address(&instruction), instruction.sequence,
                    instruction.vector_opcode, instruction.scalar_opcode);
                for (auto* operand : instruction.operands)
                {
                    std::printf(",%u,%u,%u", operand->parent->kind, operand->parent->source, operand->marker);
                }
            }
            expression_registry = {nullptr, 0};
            set_registry = {nullptr, 0};
            objects = {nullptr, 0};
            context.active_owner = nullptr;
            const auto stack_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                const auto raw = reinterpret_cast<std::uintptr_t>(pointer);
                const auto start = reinterpret_cast<std::uintptr_t>(pool.data());
                if (raw >= start && raw < start + sizeof(pool))
                {
                    const auto index = (raw - start) / 512;
                    auto* value = reinterpret_cast<ShaderValidationProgramPass*>(pool[index].data());
                    if (value->base.base.vtable_address == 0x00203818 || value->base.base.vtable_address == 0x00203820)
                    {
                        for (u32 kind = 0; kind < 3; ++kind)
                        {
                            const std::array<const void*, 3> registered{&value->base.values.registered,
                                &value->base.registers.registered, &value->base.special.registered};
                            if (pointer == registered[kind])
                            {
                                return base + static_cast<u32>(index) * 0x100 + 0x24 + kind * 0x14;
                            }
                        }
                    }
                }
                return stack_address(pointer);
            };
            owner_destroy.unregister_owner = [&](u32 value, ShaderValidationOwner& owner)
            {
                auto* pass = reinterpret_cast<ShaderValidationProgramPass*>(pool[(value - base) / 0x100].data());
                const std::array<ShaderValidationRegisteredPassTree*, 3> trees{&pass->base.values, &pass->base.registers, &pass->base.special};
                const u32 offset = (value - base) % 0x100;
                remove_shader_validation_pass_owner(*trees[(offset - 0x24) / 0x14], owner, pass_delete);
            };
            ShaderValidationEraseContext expression_erase{expression_registry, context.deleting_destructor};
            ShaderValidationPassDestroyContext pass_destroy{expression_erase, erase, pass_delete,
                [&](const void* pointer)
                {
                    return context.retail_address(pointer);
                }};
            u8 split_scalar = 1;
            u8 assigning = 0;
            ShaderValidationProgramContext driver{optimization, schedule, pass_destroy, split_scalar, assigning,
                [&](u32 value, u8 flags)
                {
                    auto* registered = reinterpret_cast<ShaderValidationRegisteredObject*>(pool[(value - base) / 0x100].data());
                    switch (registered->vtable_address)
                    {
                    case 0x0020380C:
                        delete_shader_validation_operand(context, reinterpret_cast<ShaderValidationOperand*>(registered), flags);
                        break;
                    case 0x00203810:
                        delete_shader_validation_instruction(destroy_context, reinterpret_cast<ShaderValidationInstruction*>(registered), flags);
                        break;
                    case 0x00203814:
                        delete_shader_validation_owner(owner_destroy, reinterpret_cast<ShaderValidationOwner*>(registered), flags);
                        break;
                    case 0x00203820:
                        delete_shader_validation_program_pass(pass_destroy, reinterpret_cast<ShaderValidationProgramPass*>(registered), flags,
                            context.deleting_destructor, context.release);
                        break;
                    default:
                        std::abort();
                    }
                }};
            const bool validated = validate_shader_validation_program(driver, packed.data());
            std::printf(" V%d,%u,%u,%u,%u", validated ? 1 : 0, packed[0], objects.count, expression_registry.count, set_registry.count);
            for (u32 word = 1; word <= packed[0] * 3; ++word)
            {
                std::printf(",%08X", packed[word]);
            }
            std::printf(" | %d %u\n", calls, set_registry.count);
            continue;
        }
        if (kind == "split" || kind == "splitregister" || kind == "splitpass")
        {
            std::array<ShaderValidationOwner, 3> destinations{};
            std::map<ShaderValidationOwner*, ShaderValidationInsertContext> registries;
            context.diagnostic_registry = [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, allocate}).first->second;
            };
            context.retail_address = [&](const void* pointer) -> u32
            {
                if (pointer == nullptr)
                {
                    return 0;
                }
                if (pointer == &active)
                {
                    return 0x10009000;
                }
                for (u32 index = 0; index < 3; ++index)
                {
                    if (pointer == &destinations[index])
                    {
                        return 0x1000B000 + index * 0x40;
                    }
                }
                return base + static_cast<u32>((static_cast<const unsigned char*>(pointer) - pool[0].data()) / 512) * 0x100;
            };
            auto* source = create_shader_validation_instruction(context);
            u32 flag, marker;
            std::cin >> source->vector_opcode >> source->scalar_opcode >> flag >> source->sequence >> marker;
            source->output_flag = static_cast<u8>(flag);
            source->operands[2]->marker = static_cast<u8>(marker);
            for (auto& channel : source->operands[2]->channel_order.channels)
            {
                std::cin >> channel;
            }
            std::array<ShaderValidationOwner**, 3> destination_fields{&source->output_register, &source->vector_destination, &source->scalar_destination};
            std::array<ShaderValidationChannelMask*, 3> masks{&source->output_mask, &source->vector_mask, &source->scalar_mask};
            for (u32 slot = 0; slot < 3; ++slot)
            {
                int index;
                std::cin >> index;
                *destination_fields[slot] = index < 0 ? nullptr : &destinations[index];
                for (auto& channel : masks[slot]->channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
                if (*destination_fields[slot] != nullptr)
                {
                    for (u32 channel = 0; channel < 4; ++channel)
                    {
                        if (masks[slot]->channels[channel] != 0)
                        {
                            (*destination_fields[slot])->channels[channel] = reinterpret_cast<ShaderValidationChannelWriter>(source);
                        }
                    }
                }
            }
            ShaderValidationProgramPass split_pass{};
            const auto instruction_at = [&](u32 value) -> ShaderValidationInstruction*
            {
                return reinterpret_cast<ShaderValidationInstruction*>(pool[(value - base) / 0x100].data());
            };
            ShaderValidationInstruction* result = nullptr;
            if (kind == "split")
            {
                result = split_shader_validation_scalar_instruction(context, *source);
            }
            else
            {
                ShaderValidationInsertContext pass_insert{split_pass.instruction_set.nodes, allocate};
                register_shader_validation_instruction(pass_insert, context.retail_address(source), instruction_at);
                if (kind == "splitregister")
                {
                    split_and_register_shader_validation_scalar_instruction(context, split_pass, *source, instruction_at);
                }
                else
                {
                    split_shader_validation_program_scalar_instructions(context, split_pass, instruction_at);
                }
                ShaderValidationBalancedNode* cursor;
                find_shader_validation_tree_last(split_pass.instruction_set.nodes, &cursor);
                while (cursor != nullptr)
                {
                    if (cursor->value != context.retail_address(source))
                    {
                        result = instruction_at(cursor->value);
                    }
                    advance_shader_validation_arena_iterator(cursor);
                }
            }
            std::printf("%08X %d", context.retail_address(result), calls);
            for (auto* instruction : {source, result})
            {
                if (instruction == nullptr)
                {
                    continue;
                }
                std::printf(" | %08X %u %u %u %u %u", instruction->base.vtable_address, instruction->vector_opcode,
                    instruction->scalar_opcode, instruction->sequence, instruction->instruction_set_count, instruction->output_flag);
                for (auto* owner : {instruction->output_register, instruction->vector_destination, instruction->scalar_destination})
                {
                    std::printf(" %08X", context.retail_address(owner));
                }
                for (auto* mask : {&instruction->output_mask, &instruction->vector_mask, &instruction->scalar_mask})
                {
                    std::printf(" %02X%02X%02X%02X", mask->channels[0], mask->channels[1], mask->channels[2], mask->channels[3]);
                }
                for (auto* operand : instruction->operands)
                {
                    std::printf(" %08X:%08X,%u,%08X,%u,%u,%u,%u,%u", context.retail_address(operand), operand->base.vtable_address,
                        operand->kind, context.retail_address(operand->parent), operand->marker, operand->channel_order.channels[0],
                        operand->channel_order.channels[1], operand->channel_order.channels[2], operand->channel_order.channels[3]);
                }
            }
            for (auto& owner : destinations)
            {
                std::printf(" |");
                for (auto writer : owner.channels)
                {
                    std::printf(" %08X", context.retail_address(reinterpret_cast<const void*>(writer)));
                }
            }
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(objects, &node);
            std::printf(" | %u", objects.count);
            while (node != nullptr)
            {
                std::printf(" %08X", node->value);
                advance_shader_validation_balanced_iterator(&node);
            }
            if (kind != "split")
            {
                find_shader_validation_tree_last(split_pass.instruction_set.nodes, &node);
                std::printf(" | %u", split_pass.instruction_set.nodes.count);
                while (node != nullptr)
                {
                    std::printf(" %08X", node->value);
                    advance_shader_validation_arena_iterator(node);
                }
            }
            std::printf("\n");
            continue;
        }
        if (kind == "moveunwritten")
        {
            int derived, reader_count;
            std::cin >> derived >> reader_count;
            std::array<ShaderValidationOwner, 4> owners{};
            std::vector<ShaderValidationOperand> readers(static_cast<std::size_t>(reader_count));
            ShaderValidationInstruction instruction{};
            std::vector<u32> deleted;
            const auto pool_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                if (pointer == nullptr)
                {
                    return 0;
                }
                if (pointer == &active)
                {
                    return 0x10009000;
                }
                for (std::size_t index = 0; index < owners.size(); ++index)
                {
                    if (pointer == &owners[index])
                    {
                        return 0x1000D000 + static_cast<u32>(index) * 0x40;
                    }
                }
                for (std::size_t index = 0; index < readers.size(); ++index)
                {
                    if (pointer == &readers[index])
                    {
                        return 0x1000E000 + static_cast<u32>(index) * 0x40;
                    }
                }
                return pool_address(pointer);
            };
            std::map<ShaderValidationOwner*, ShaderValidationInsertContext> registries;
            context.diagnostic_registry = [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, allocate}).first->second;
            };
            context.deleting_destructor = [&](ShaderValidationBalancedNode& node, u32 flags)
            {
                deleted.push_back(node.value);
                deleted.push_back(flags);
            };
            for (auto& owner : owners)
            {
                int next;
                std::cin >> owner.kind >> next;
                owner.channel_owner = next < 0 ? nullptr : &owners[static_cast<std::size_t>(next)];
                owner.diagnostics.vtable_address = 0x002037B8;
                for (auto& writer : owner.channels)
                {
                    u32 value;
                    std::cin >> value;
                    writer = value == 0 ? 0 : shader_validation_unwritten_channel;
                }
            }
            owners[0].derived_owner = derived < 0 ? nullptr : &owners[static_cast<std::size_t>(derived)];
            std::cin >> instruction.vector_opcode >> instruction.scalar_opcode;
            for (auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
            {
                for (auto& channel : mask->channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
            }
            for (auto& operand : readers)
            {
                u32 parent;
                std::cin >> operand.kind >> parent;
                operand.instruction = &instruction;
                operand.parent = &owners[parent];
                for (auto& channel : operand.channel_order.channels)
                {
                    std::cin >> channel;
                }
                register_shader_validation_diagnostic(context.diagnostic_registry(*operand.parent), context.retail_address(&operand));
            }
            move_shader_validation_unwritten_readers_to_derived(context, owners[0], [&](u32 address) -> ShaderValidationOperand*
                {
                    return &readers[(address - 0x1000E000) / 0x40];
                });
            for (const auto& operand : readers)
            {
                std::printf("%08X ", context.retail_address(operand.parent));
            }
            std::printf("|");
            for (const auto& owner : owners)
            {
                ShaderValidationBalancedNode* node;
                find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
                std::printf(" %u", owner.diagnostics.nodes.count);
                while (node != nullptr)
                {
                    std::printf(",%08X", node->value);
                    advance_shader_validation_balanced_iterator(&node);
                }
            }
            std::printf(" |");
            for (u32 value : deleted)
            {
                std::printf(" %08X", value);
            }
            std::printf("\n");
            continue;
        }
        if (kind == "reparent" || kind == "ownerparent" || kind == "clearslot" || kind == "clearslots")
        {
            int previous_kind, old_parent, new_parent, retain_reader, lazy;
            std::cin >> previous_kind >> old_parent >> new_parent >> retain_reader >> lazy;
            std::array<ShaderValidationOwner, 4> owners{};
            std::array<ShaderValidationOperand, 4> operands{};
            std::map<ShaderValidationOwner*, ShaderValidationInsertContext> registries;
            std::vector<u32> deleted;
            const auto parent_at = [&](int index) -> ShaderValidationOwner*
            {
                return index == -2 ? active_owner : index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            const auto pooled_address = context.retail_address;
            context.retail_address = [&](const void* pointer) -> u32
            {
                if (pointer == nullptr)
                {
                    return 0;
                }
                if (pointer == &active)
                {
                    return 0x10009000;
                }
                for (std::size_t index = 0; index < owners.size(); ++index)
                {
                    if (pointer == &owners[index])
                    {
                        return 0x1000D000 + static_cast<u32>(index) * 0x40;
                    }
                    if (pointer == &operands[index])
                    {
                        return 0x1000E000 + static_cast<u32>(index) * 0x40;
                    }
                }
                return pooled_address(pointer);
            };
            context.diagnostic_registry = [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, allocate}).first->second;
            };
            context.deleting_destructor = [&](ShaderValidationBalancedNode& node, u32 flags)
            {
                deleted.push_back(node.value);
                deleted.push_back(flags);
            };
            for (std::size_t index = 0; index < owners.size(); ++index)
            {
                owners[index].kind = index == 1 ? static_cast<u32>(previous_kind) : 3;
                owners[index].diagnostics.vtable_address = 0x002037B8;
                owners[index].operand = &operands[index];
                operands[index].kind = 3;
                operands[index].owner = &owners[index];
                operands[index].parent = active_owner;
            }
            operands[0].parent = parent_at(old_parent);
            operands[1].parent = &owners[2];
            register_shader_validation_diagnostic(context.diagnostic_registry(owners[2]), context.retail_address(&operands[1]));
            if (old_parent >= 0)
            {
                register_shader_validation_diagnostic(context.diagnostic_registry(*parent_at(old_parent)), context.retail_address(&operands[0]));
            }
            if (retain_reader != 0)
            {
                register_shader_validation_diagnostic(context.diagnostic_registry(owners[1]), context.retail_address(&operands[3]));
            }
            if (lazy != 0)
            {
                owners[0].operand = nullptr;
            }
            ShaderValidationInstruction instruction{};
            bool changed = false;
            if (kind == "clearslot" || kind == "clearslots")
            {
                std::cin >> instruction.vector_opcode >> instruction.scalar_opcode;
                for (auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
                {
                    for (auto& channel : mask->channels)
                    {
                        u32 value;
                        std::cin >> value;
                        channel = static_cast<u8>(value);
                    }
                }
                for (u32 slot = 0; slot < 3; ++slot)
                {
                    operands[slot].instruction = &instruction;
                    operands[slot].kind = slot;
                    instruction.operands[slot] = &operands[slot];
                    for (auto& channel : operands[slot].channel_order.channels)
                    {
                        std::cin >> channel;
                    }
                }
                changed = kind == "clearslot" ? clear_unused_shader_validation_operand_channels(context, operands[0]) :
                    clear_unused_shader_validation_instruction_channels(context, instruction);
                std::printf("%d ", changed ? 1 : 0);
                for (u32 slot = 0; slot < 3; ++slot)
                {
                    for (u32 channel : operands[slot].channel_order.channels)
                    {
                        std::printf("%u,", channel);
                    }
                }
                std::printf(" ");
            }
            else if (kind == "reparent")
            {
                reparent_shader_validation_operand(context, operands[0], parent_at(new_parent));
            }
            else
            {
                reparent_shader_validation_owner_operand(context, owners[0], parent_at(new_parent));
            }
            for (const auto& owner : owners)
            {
                std::printf("%08X:%08X", context.retail_address(owner.operand), context.retail_address(owner.operand->parent));
                ShaderValidationBalancedNode* node;
                find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
                std::printf("/%u", owner.diagnostics.nodes.count);
                while (node != nullptr)
                {
                    std::printf(",%08X", node->value);
                    advance_shader_validation_balanced_iterator(&node);
                }
                std::printf(" ");
            }
            std::printf("|%u", objects.count);
            for (u32 value : deleted)
            {
                std::printf(" %08X", value);
            }
            std::printf("\n");
            continue;
        }
        if (kind == "owner" || kind == "input" || kind == "select")
        {
            u32 source, use_owner;
            int pass_owner_kind;
            std::cin >> source >> use_owner >> pass_owner_kind;
            ShaderValidationOwner pass_owner{};
            ShaderValidationPass pass{};
            pass.owner = pass_owner_kind == 0 ? nullptr : pass_owner_kind == 1 ? active_owner : &pass_owner;
            std::map<ShaderValidationOwner*, ShaderValidationInsertContext> registries;
            context.diagnostic_registry = [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, allocate}).first->second;
            };
            ShaderValidationOwner* owner;
            ShaderValidationPassNode registered{};
            if (kind == "select")
            {
                u32 selector, input_source, register_kind, tree, presence;
                ShaderValidationChannelMask mask{0xDEADBEEF, {}};
                std::cin >> selector >> input_source >> register_kind >> tree >> registered.value >> presence;
                for (auto& channel : mask.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
                registered.data = presence == 2 ? &pass_owner : nullptr;
                if (presence != 0)
                {
                    auto& nodes = tree == 0 ? pass.registers.tree.nodes : pass.special.tree.nodes;
                    nodes = {&registered, 1};
                }
                owner = select_shader_validation_owner(context, pass, selector, input_source, source, register_kind,
                    static_cast<u8>(use_owner), mask);
            }
            else
            {
                owner = kind == "input" ? get_shader_validation_input_owner(context, pass, source) :
                    create_shader_validation_constant_owner(context, pass, source, static_cast<u8>(use_owner));
            }
            if (kind == "input")
            {
                const std::size_t before = used;
                ShaderValidationOwner* again = get_shader_validation_input_owner(context, pass, source);
                std::printf("%d %d %u ", again == owner ? 1 : 0, used == before ? 1 : 0, pass.values.tree.nodes.count);
            }
            if (owner == nullptr)
            {
                std::printf("-1 | %u\n", objects.count);
                continue;
            }
            const auto parent_code = [&](const ShaderValidationOwner* parent)
            {
                return parent == nullptr ? 0 : parent == active_owner ? 1 : parent == &pass_owner ? 2 : 3;
            };
            std::printf("%llX %08X %u %u %08X %u %u %u %u %d", owner == &pass_owner ? 0x1000B000LL : address(owner),
                owner->base.vtable_address, owner->kind, owner->source,
                owner->diagnostics.vtable_address, owner->diagnostics.nodes.count, static_cast<u32>(owner->channels[0]),
                static_cast<u32>(owner->channels[3]), owner->derived_owner == nullptr && owner->channel_owner == nullptr ? 1 : 0,
                owner->operand == nullptr ? -1 : 0);
            if (owner->operand != nullptr)
            {
                const auto* operand = owner->operand;
                std::printf(" %llX:%08X,%d,%u,%d,%u", address(operand), operand->base.vtable_address, operand->owner == owner ? 1 : 0,
                    operand->kind, parent_code(operand->parent), operand->instruction == nullptr ? 1 : 0);
            }
            std::printf(" %u |", pass_owner.diagnostics.nodes.count);
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(objects, &node);
            std::printf(" %u", objects.count);
            while (node != nullptr)
            {
                std::printf(" %08X", node->value);
                advance_shader_validation_balanced_iterator(&node);
            }
            std::printf("\n");
            continue;
        }
        ShaderValidationInstruction* instruction = create_shader_validation_instruction(context);
        std::printf("%llX %08X", address(instruction), instruction->base.vtable_address);
        for (const auto* mask : {&instruction->output_mask, &instruction->vector_mask, &instruction->scalar_mask})
        {
            std::printf(" %08X%02X%02X%02X%02X", mask->vtable_address, mask->channels[0], mask->channels[1], mask->channels[2], mask->channels[3]);
        }
        std::printf(" %u %u %u %u %u %u %d%d%d", instruction->sequence, instruction->instruction_set_count, instruction->instruction_set == nullptr ? 0 : 1,
            instruction->vector_opcode, instruction->scalar_opcode, instruction->output_flag, instruction->output_register == nullptr ? 0 : 1,
            instruction->vector_destination == nullptr ? 0 : 1, instruction->scalar_destination == nullptr ? 0 : 1);
        for (const auto* operand : instruction->operands)
        {
            if (operand == nullptr)
            {
                std::printf(" -1");
            }
            else
            {
                std::printf(" %llX", address(operand));
            }
            if (operand != nullptr)
            {
                std::printf(":%08X,%llX,%d,%u,%d,%u,%08X,%u%u%u%u", operand->base.vtable_address, address(operand->instruction),
                    operand->owner == nullptr ? 0 : 1, operand->kind, operand->parent == active_owner ? 1 : 0, operand->marker,
                    operand->channel_order.vtable_address, operand->channel_order.channels[0], operand->channel_order.channels[1],
                    operand->channel_order.channels[2], operand->channel_order.channels[3]);
            }
        }
        std::printf(" | %u", objects.count);
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(objects, &node);
        while (node != nullptr)
        {
            std::printf(" %08X", node->value);
            advance_shader_validation_balanced_iterator(&node);
        }
        std::printf("\n");
    }
}
