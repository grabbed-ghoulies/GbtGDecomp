// Built from SEH-shimmed copies (see run.sh): only the normal, non-exceptional path is compared.
#include "shader_validation_instruction_constructor.h"
#include "shader_validation_sets.h"
#include "shader_validation_pass_lifecycle.h"
#include "shader_validation_owner_sets.h"
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
void run_pass_destructor(int list_count, int tree_seed)
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
    ShaderValidationPass pass{};
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
    destroy_shader_validation_pass(context, pass);
    std::printf("%08X %08X %08X %08X %u %u %u %u %u %u", pass.base.vtable_address, pass.values.tree.vtable_address, pass.registers.tree.vtable_address,
        pass.special.tree.vtable_address, pass.values.tree.nodes.count, pass.registers.tree.nodes.count, pass.special.tree.nodes.count,
        pass.instructions.storage.count, expressions.count, objects.count);
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
    if (kind == "retire")
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
        const bool result = retire_shader_validation_register(pass, retire_kind, registry, destroy_pass_node, allocate_pass_node, context);
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
        for (u32 key : freed)
        {
            std::printf(" f%u", key);
        }
        std::printf("\n");
        return;
    }
    if (kind == "readers")
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
        if (kind == "passdtor")
        {
            run_pass_destructor(fail, active_kind);
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
        if (kind == "readers" || kind == "negate" || kind == "equiv" || kind == "retire")
        {
            run_owner_sets(kind, fail, active_kind);
            continue;
        }
        alignas(16) static std::array<std::array<unsigned char, 256>, 64> pool;
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
                return base + static_cast<u32>((static_cast<const unsigned char*>(pointer) - pool[0].data()) / 256) * 0x80;
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
        if (kind == "owner")
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
            ShaderValidationOwner* owner = create_shader_validation_constant_owner(context, pass, source, static_cast<u8>(use_owner));
            if (owner == nullptr)
            {
                std::printf("-1 | %u\n", objects.count);
                continue;
            }
            const auto parent_code = [&](const ShaderValidationOwner* parent)
            {
                return parent == nullptr ? 0 : parent == active_owner ? 1 : parent == &pass_owner ? 2 : 3;
            };
            std::printf("%llX %08X %u %u %08X %u %u %u %u %d", address(owner), owner->base.vtable_address, owner->kind, owner->source,
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
