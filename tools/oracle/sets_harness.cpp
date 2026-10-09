#include "shader_validation_sets.h"
#include <cstdio>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace gbtg;

namespace
{
template <typename Node>
int index_of(const std::vector<Node>& nodes, const Node* node)
{
    return node == nullptr ? -1 : static_cast<int>(node - nodes.data());
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "owners")
        {
            int count;
            std::cin >> count;
            std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(count));
            for (auto& owner : owners)
            {
                int derived, channel_owner;
                std::cin >> derived >> channel_owner;
                owner.derived_owner = derived < 0 ? nullptr : &owners[static_cast<std::size_t>(derived)];
                owner.channel_owner = channel_owner < 0 ? nullptr : &owners[static_cast<std::size_t>(channel_owner)];
                for (auto& channel : owner.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = value;
                }
            }
            for (int index = 0; index < count; ++index)
            {
                for (int other = -1; other < count; ++other)
                {
                    std::printf("%d", shader_validation_owner_derives_from(owners[static_cast<std::size_t>(index)],
                                          other < 0 ? nullptr : &owners[static_cast<std::size_t>(other)]) ? 1 : 0);
                }
                for (u32 channel = 0; channel < 4; ++channel)
                {
                    std::printf("%d%d", shader_validation_derived_owner_writes_channel(owners[static_cast<std::size_t>(index)], channel) ? 1 : 0,
                        shader_validation_channel_owner_writes_channel(owners[static_cast<std::size_t>(index)], channel) ? 1 : 0);
                }
                std::printf(" ");
            }
            std::printf("\n");
        }
        else if (kind == "destroy")
        {
            int function, count, flags;
            std::cin >> function >> count >> flags;
            std::vector<int> events;
            const auto release = [&](void*, u32 tag)
            {
                events.push_back(tag == 0x24870000 ? -100 : -200);
            };
            if (function >= 4)
            {
                std::vector<ShaderValidationPassNode> nodes(static_cast<std::size_t>(count));
                for (auto& node : nodes)
                {
                    int parent, first, second;
                    std::cin >> parent >> first >> second;
                    node.parent = parent < 0 ? nullptr : &nodes[static_cast<std::size_t>(parent)];
                    node.first = first < 0 ? nullptr : &nodes[static_cast<std::size_t>(first)];
                    node.second = second < 0 ? nullptr : &nodes[static_cast<std::size_t>(second)];
                }
                const auto destroy = [&](ShaderValidationPassNode& node, u32 value)
                {
                    events.push_back(index_of(nodes, &node) * 10 + static_cast<int>(value));
                };
                ShaderValidationPassTreeObject tree{0, 0x1234, {count > 0 ? nodes.data() : nullptr, static_cast<u32>(count)}};
                if (function == 4)
                {
                    destroy_shader_validation_pass_nodes(tree.nodes.root, destroy);
                }
                else if (function == 5)
                {
                    clear_shader_validation_pass_tree(tree, destroy);
                }
                else
                {
                    delete_shader_validation_pass_tree(tree, static_cast<u8>(flags), destroy, release);
                }
                std::printf("%08X %d %u |", tree.vtable_address, tree.nodes.root == nullptr ? 0 : 1, tree.nodes.count);
                for (const auto& node : nodes)
                {
                    std::printf(" %d", node.parent == nullptr ? 0 : 1);
                }
            }
            else
            {
                std::vector<ShaderValidationBalancedNode> nodes(static_cast<std::size_t>(count));
                for (auto& node : nodes)
                {
                    int parent, first, second;
                    std::cin >> parent >> first >> second;
                    node.parent = parent < 0 ? nullptr : &nodes[static_cast<std::size_t>(parent)];
                    node.first = first < 0 ? nullptr : &nodes[static_cast<std::size_t>(first)];
                    node.second = second < 0 ? nullptr : &nodes[static_cast<std::size_t>(second)];
                }
                const auto destroy = [&](ShaderValidationBalancedNode& node, u32 value)
                {
                    events.push_back(index_of(nodes, &node) * 10 + static_cast<int>(value));
                };
                ShaderValidationBalancedChildTree tree{0, 0x1234, {count > 0 ? nodes.data() : nullptr, static_cast<u32>(count)}};
                if (function == 0)
                {
                    destroy_shader_validation_balanced_nodes_18f2aa(tree.nodes.root, destroy);
                }
                else if (function == 1)
                {
                    destroy_shader_validation_balanced_nodes_18f2d3(tree.nodes.root, destroy);
                }
                else if (function == 2)
                {
                    clear_shader_validation_balanced_tree_190322(tree, destroy);
                }
                else
                {
                    delete_shader_validation_balanced_tree(tree, static_cast<u8>(flags), destroy, release);
                }
                std::printf("%08X %d %u |", tree.vtable_address, tree.nodes.root == nullptr ? 0 : 1, tree.nodes.count);
                for (const auto& node : nodes)
                {
                    std::printf(" %d", node.parent == nullptr ? 0 : 1);
                }
            }
            for (int event : events)
            {
                std::printf(" e%d", event);
            }
            std::printf("\n");
        }
        else if (kind == "setval")
        {
            int count, calls_count;
            std::cin >> count;
            std::vector<ShaderValidationPassNode> nodes(64);
            std::size_t used = 0;
            const auto allocate = [&](std::size_t, u32) -> void*
            {
                return &nodes[used++];
            };
            ShaderValidationPassTree tree{nullptr, 0};
            std::vector<u32> data(16);
            std::cin >> calls_count;
            std::printf("|");
            for (int call = 0; call < calls_count; ++call)
            {
                u32 key, value;
                std::cin >> key >> value;
                ShaderValidationPassNode* result = reinterpret_cast<ShaderValidationPassNode*>(1);
                ShaderValidationPassNode** returned = set_shader_validation_pass_value(tree, allocate, &result, key, &data[value]);
                std::printf(" %d%d", index_of(nodes, result), returned == &result ? 1 : 0);
            }
            std::printf(" | %d %u", index_of(nodes, tree.root), tree.count);
            for (std::size_t index = 0; index < used; ++index)
            {
                std::printf(" %08X %d %d %d %d %d", nodes[index].value, index_of(nodes, nodes[index].parent), index_of(nodes, nodes[index].first),
                    index_of(nodes, nodes[index].second), nodes[index].height, static_cast<int>(static_cast<u32*>(nodes[index].data) - data.data()));
            }
            std::printf("\n");
            (void)count;
        }
        else if (kind == "copyset")
        {
            int source_count, initial_count;
            std::cin >> source_count >> initial_count;
            std::vector<u32> source_values(static_cast<std::size_t>(source_count)), initial_values(static_cast<std::size_t>(initial_count));
            for (auto& value : source_values)
            {
                std::cin >> value;
            }
            for (auto& value : initial_values)
            {
                std::cin >> value;
            }
            std::map<u32, ShaderValidationInstruction> instructions;
            int instruction_count;
            std::cin >> instruction_count;
            for (int index = 0; index < instruction_count; ++index)
            {
                u32 address, sequence;
                std::cin >> address >> sequence;
                instructions[address].sequence = sequence;
            }
            const ShaderValidationInstructionAt instruction_at = [&](u32 address) -> const ShaderValidationInstruction*
            {
                return &instructions.at(address);
            };
            std::vector<ShaderValidationBalancedNode> nodes(128);
            std::size_t used = 0;
            const auto allocate = [&](std::size_t, u32) -> void*
            {
                return &nodes[used++];
            };
            ShaderValidationBalancedChildTree source{0x002037d0, 0, {nullptr, 0}};
            ShaderValidationBalancedChildTree target{0x002037d0, 0, {nullptr, 0}};
            for (u32 value : source_values)
            {
                ShaderValidationInsertContext context{source.nodes, allocate};
                register_shader_validation_instruction(context, value, instruction_at);
            }
            for (u32 value : initial_values)
            {
                ShaderValidationInsertContext context{target.nodes, allocate};
                register_shader_validation_instruction(context, value, instruction_at);
            }
            const std::size_t first_new = used;
            ShaderValidationInstruction instruction{};
            instruction.instruction_set = &target;
            instruction.instruction_set_count = 0x77;
            copy_shader_validation_instruction_set(instruction, source, allocate, instruction_at);
            std::printf("%u %u |", instruction.instruction_set_count, target.nodes.count);
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(target.nodes, &node);
            while (node != nullptr)
            {
                std::printf(" %08X", node->value);
                advance_shader_validation_balanced_iterator(&node);
            }
            std::printf(" | %zu\n", used - first_new);
        }
        else if (kind == "release")
        {
            int has_set, registered, other_count;
            std::cin >> has_set >> registered >> other_count;
            std::vector<u32> others(static_cast<std::size_t>(other_count));
            for (auto& value : others)
            {
                std::cin >> value;
            }
            u32 set_address;
            std::cin >> set_address;
            std::vector<ShaderValidationBalancedNode> nodes(64);
            std::size_t used = 0;
            ShaderValidationBalancedTree arena{nullptr, 0};
            ShaderValidationInsertContext insert{arena, [&](std::size_t, u32) -> void*
                {
                    return &nodes[used++];
                }};
            for (u32 value : others)
            {
                register_shader_validation_value_2037ec(insert, value);
            }
            if (registered)
            {
                register_shader_validation_value_2037ec(insert, set_address);
            }
            std::vector<int> events;
            ShaderValidationEraseContext erase{arena, [&](ShaderValidationBalancedNode& node, u32 flags)
                {
                    events.push_back(index_of(nodes, &node) * 10 + static_cast<int>(flags));
                }};
            ShaderValidationBalancedChildTree set{0x002037d0, 0, {nullptr, 0}};
            ShaderValidationInstruction instruction{};
            instruction.instruction_set = has_set ? &set : nullptr;
            release_shader_validation_instruction_set(instruction, erase, [&](const void*) -> u32
                {
                    return set_address;
                }, [&](ShaderValidationBalancedChildTree&, u32 flags)
                {
                    events.push_back(-1000 - static_cast<int>(flags));
                });
            std::printf("%d %u", instruction.instruction_set == nullptr ? 1 : 0, arena.count);
            for (int event : events)
            {
                std::printf(" e%d", event);
            }
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(arena, &node);
            while (node != nullptr)
            {
                std::printf(" %08X", node->value);
                advance_shader_validation_balanced_iterator(&node);
            }
            std::printf("\n");
        }
        else if (kind == "swapown" || kind == "writers")
        {
            // Owners, instruction writers and operands, shared by 0x00190FFE and 0x00190774.
            int owner_count, instruction_count;
            std::cin >> owner_count >> instruction_count;
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
                std::cin >> instruction.vector_opcode >> instruction.scalar_opcode >> flag >> output >> vector_destination >> scalar_destination;
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
                std::cin >> instruction.sequence;
                for (int slot = 0; slot < 3; ++slot)
                {
                    auto& operand = operands[static_cast<std::size_t>(index * 3 + slot)];
                    for (auto& channel : operand.channel_order.channels)
                    {
                        std::cin >> channel;
                    }
                    operand.instruction = &instruction;
                    operand.kind = static_cast<u32>(slot);
                    instruction.operands[static_cast<std::size_t>(slot)] = &operand;
                }
            }
            for (int index = 0; index < owner_count; ++index)
            {
                auto& owner = owners[static_cast<std::size_t>(index)];
                int derived, channel_owner, operand, parent, registry_count;
                std::cin >> derived >> channel_owner >> operand >> parent;
                owner.derived_owner = owner_at(derived);
                owner.channel_owner = owner_at(channel_owner);
                for (auto& channel : owner.channels)
                {
                    int code;
                    std::cin >> code;
                    channel = code == 0 ? 0 : code < 0 ? shader_validation_unwritten_channel
                                                       : reinterpret_cast<ShaderValidationChannelWriter>(&instructions[static_cast<std::size_t>(code - 1)]);
                }
                if (operand >= 0)
                {
                    owner.operand = &operands[static_cast<std::size_t>(operand)];
                    operands[static_cast<std::size_t>(operand)].parent = owner_at(parent);
                }
                std::cin >> registry_count;
                auto& registry = registries[static_cast<std::size_t>(index)];
                registry.resize(static_cast<std::size_t>(registry_count));
                for (auto& node : registry)
                {
                    int node_parent, first, second;
                    std::cin >> node.value >> node_parent >> first >> second;
                    node.parent = node_parent < 0 ? nullptr : &registry[static_cast<std::size_t>(node_parent)];
                    node.first = first < 0 ? nullptr : &registry[static_cast<std::size_t>(first)];
                    node.second = second < 0 ? nullptr : &registry[static_cast<std::size_t>(second)];
                }
                owner.diagnostics.nodes.root = registry.empty() ? nullptr : registry.data();
            }
            const ShaderValidationOperandAt operand_at = [&](u32 value) -> ShaderValidationOperand*
            {
                return &operands[value];
            };
            const auto code_of = [&](ShaderValidationChannelWriter writer) -> int
            {
                if (writer == 0)
                {
                    return 0;
                }
                if (writer == shader_validation_unwritten_channel)
                {
                    return -1;
                }
                return static_cast<int>(reinterpret_cast<ShaderValidationInstruction*>(writer) - instructions.data()) + 1;
            };
            if (kind == "writers")
            {
                int target, active, set_count;
                std::cin >> target >> active >> set_count;
                std::vector<u32> set_values(static_cast<std::size_t>(set_count));
                for (auto& value : set_values)
                {
                    std::cin >> value;
                }
                ShaderValidationChannelMask mask{0x00203790, {}};
                for (auto& channel : mask.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
                // Instruction k has retail address 1000 + k.
                const ShaderValidationInstructionAt instruction_at = [&](u32 address) -> const ShaderValidationInstruction*
                {
                    return &instructions[address - 1000];
                };
                const ShaderValidationAddressOf address_of = [&](const void* object) -> u32
                {
                    return static_cast<u32>(static_cast<const ShaderValidationInstruction*>(object) - instructions.data()) + 1000;
                };
                std::vector<ShaderValidationBalancedNode> set_nodes(16);
                std::size_t used = 0;
                ShaderValidationBalancedTree set{nullptr, 0};
                ShaderValidationInsertContext context{set, [&](std::size_t, u32) -> void*
                    {
                        return &set_nodes[used++];
                    }};
                for (u32 value : set_values)
                {
                    register_shader_validation_instruction(context, value, instruction_at);
                }
                int operand_index;
                std::cin >> operand_index;
                const bool result = operand_index < 0
                    ? shader_validation_owner_writers_in_set(owners[static_cast<std::size_t>(target)], 0, mask, set, owner_at(active), address_of,
                          instruction_at)
                    : shader_validation_operand_writers_in_set(operands[static_cast<std::size_t>(operand_index)], 0, mask, set, owner_at(active),
                          address_of, instruction_at);
                std::printf("%d\n", result ? 1 : 0);
                continue;
            }
            int target;
            u32 first, second, climb, propagate;
            std::cin >> target >> first >> second >> climb >> propagate;
            const bool swapped = swap_shader_validation_owner_channels(owners[static_cast<std::size_t>(target)], first, second,
                static_cast<u8>(climb), static_cast<u8>(propagate), operand_at);
            std::printf("%d", swapped ? 1 : 0);
            for (const auto& owner : owners)
            {
                std::printf(" %d,%d,%d,%d", code_of(owner.channels[0]), code_of(owner.channels[1]), code_of(owner.channels[2]),
                    code_of(owner.channels[3]));
            }
            for (const auto& instruction : instructions)
            {
                std::printf(" %02X%02X%02X%02X%02X%02X%02X%02X", instruction.vector_mask.channels[0], instruction.vector_mask.channels[1],
                    instruction.vector_mask.channels[2], instruction.vector_mask.channels[3], instruction.scalar_mask.channels[0],
                    instruction.scalar_mask.channels[1], instruction.scalar_mask.channels[2], instruction.scalar_mask.channels[3]);
            }
            for (const auto& operand : operands)
            {
                std::printf(" %u%u%u%u", operand.channel_order.channels[0], operand.channel_order.channels[1], operand.channel_order.channels[2],
                    operand.channel_order.channels[3]);
            }
            std::printf("\n");
        }
        else if (kind == "register")
        {
            int function, count, fail;
            std::cin >> function >> count >> fail;
            std::vector<u32> values(static_cast<std::size_t>(count));
            for (auto& value : values)
            {
                std::cin >> value;
            }
            std::map<u32, ShaderValidationInstruction> instructions;
            for (int index = 0; index < count; ++index)
            {
                u32 sequence;
                std::cin >> sequence;
                instructions[values[static_cast<std::size_t>(index)]].sequence = sequence;
            }
            const ShaderValidationInstructionAt instruction_at = [&](u32 address) -> const ShaderValidationInstruction*
            {
                return &instructions.at(address);
            };
            int calls = 0;
            if (function == 3)
            {
                std::vector<ShaderValidationPassNode> nodes(64);
                std::size_t used = 0;
                const auto allocate = [&](std::size_t, u32) -> void*
                {
                    return calls++ == fail ? nullptr : &nodes[used++];
                };
                ShaderValidationPassTree tree{nullptr, 0};
                u32 data = 0;
                for (u32 value : values)
                {
                    register_shader_validation_pass_value(tree, allocate, value, &data);
                }
                std::printf("%d %u |", index_of(nodes, tree.root), tree.count);
                for (std::size_t index = 0; index < used; ++index)
                {
                    const auto& node = nodes[index];
                    std::printf(" %08X %08X %d %d %d %d", node.vtable_address, node.value, index_of(nodes, node.parent), index_of(nodes, node.first),
                        index_of(nodes, node.second), node.height);
                }
                std::printf("\n");
                continue;
            }
            std::vector<ShaderValidationBalancedNode> nodes(64);
            std::size_t used = 0;
            ShaderValidationBalancedTree tree{nullptr, 0};
            ShaderValidationInsertContext context{tree, [&](std::size_t, u32) -> void*
                {
                    return calls++ == fail ? nullptr : &nodes[used++];
                }};
            for (u32 value : values)
            {
                if (function == 0)
                {
                    register_shader_validation_value_2037ec(context, value);
                }
                else if (function == 1)
                {
                    register_shader_validation_value_2037f4(context, value);
                }
                else
                {
                    register_shader_validation_instruction(context, value, instruction_at);
                }
            }
            std::printf("%d %u |", index_of(nodes, tree.root), tree.count);
            for (std::size_t index = 0; index < used; ++index)
            {
                const auto& node = nodes[index];
                std::printf(" %08X %08X %d %d %d %d", node.vtable_address, node.value, index_of(nodes, node.parent), index_of(nodes, node.first),
                    index_of(nodes, node.second), node.height);
            }
            if (function == 2)
            {
                std::printf(" |");
                for (u32 value : values)
                {
                    std::printf(" %d", index_of(nodes, find_shader_validation_instruction_node(tree, value, instruction_at)));
                    for (u32 other : values)
                    {
                        std::printf("%d", shader_validation_instruction_precedes(value, other, instruction_at) ? 1 : 0);
                    }
                }
            }
            std::printf("\n");
        }
    }
}
