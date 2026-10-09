#include "shader_validation_instruction_ops.h"
#include "shader_validation_registers.h"
#include <array>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
using namespace gbtg;

namespace
{
int index_of(const std::vector<ShaderValidationPassNode>& nodes, const ShaderValidationPassNode* node)
{
    return node == nullptr ? -1 : static_cast<int>(node - nodes.data());
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "rotate" || kind == "heights")
        {
            int count, target, direction;
            std::cin >> count >> target >> direction;
            std::vector<ShaderValidationPassNode> nodes(static_cast<std::size_t>(count));
            auto at = [&](int index) -> ShaderValidationPassNode*
            {
                return index < 0 ? nullptr : &nodes[static_cast<std::size_t>(index)];
            };
            for (auto& node : nodes)
            {
                int parent, first, second;
                std::cin >> parent >> first >> second >> node.height;
                node.parent = at(parent);
                node.first = at(first);
                node.second = at(second);
            }
            if (kind == "heights")
            {
                update_shader_validation_pass_ancestor_heights(at(target));
            }
            else if (direction == 2)
            {
                balance_shader_validation_pass_ancestors(nodes[static_cast<std::size_t>(target)]);
            }
            else if (direction == 3)
            {
                // Rebalance after attaching node `target`; the tree root is node 0.
                ShaderValidationPassTree tree{&nodes[0], 0};
                rebalance_shader_validation_pass_tree(tree, nodes[static_cast<std::size_t>(target)]);
                std::printf("%d | ", index_of(nodes, tree.root));
            }
            else if (direction == 5)
            {
                ShaderValidationPassTree tree{&nodes[0], static_cast<u32>(count)};
                int destroyed = -1;
                ShaderValidationPassNode* next = erase_shader_validation_pass_node(tree, &nodes[static_cast<std::size_t>(target)],
                    [&](ShaderValidationPassNode& node, u32 flags)
                    {
                        destroyed = static_cast<int>(&node - nodes.data()) * 10 + static_cast<int>(flags);
                    });
                std::printf("%d %d %u %d | ", index_of(nodes, next), index_of(nodes, tree.root), tree.count, destroyed);
            }
            else if (direction == 4)
            {
                // Join the children of node `target`.
                ShaderValidationPassNode* joined = join_shader_validation_pass_subtrees(nodes[static_cast<std::size_t>(target)].first,
                    nodes[static_cast<std::size_t>(target)].second);
                std::printf("%d | ", index_of(nodes, joined));
            }
            else if (direction == 0)
            {
                rotate_shader_validation_pass_tree_left(nodes[static_cast<std::size_t>(target)]);
            }
            else
            {
                rotate_shader_validation_pass_tree_right(nodes[static_cast<std::size_t>(target)]);
            }
            for (const auto& node : nodes)
            {
                std::printf("%d %d %d %d ", index_of(nodes, node.parent), index_of(nodes, node.first), index_of(nodes, node.second), node.height);
            }
            std::printf("\n");
        }
        else if (kind == "tables")
        {
            int owner_count, register_count, special_count;
            std::cin >> owner_count;
            std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(owner_count));
            auto owner_at = [&](int index) -> ShaderValidationOwner*
            {
                return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            for (auto& owner : owners)
            {
                int derived, channel;
                std::cin >> derived >> channel;
                owner.derived_owner = owner_at(derived);
                owner.channel_owner = owner_at(channel);
            }
            ShaderValidationPass tables{};
            std::vector<ShaderValidationPassNode> registers, special;
            for (auto* nodes : {&registers, &special})
            {
                std::cin >> register_count;
                nodes->resize(static_cast<std::size_t>(register_count));
                for (auto& node : *nodes)
                {
                    int first, second, data;
                    std::cin >> node.value >> first >> second >> data;
                    node.first = first < 0 ? nullptr : &(*nodes)[static_cast<std::size_t>(first)];
                    node.second = second < 0 ? nullptr : &(*nodes)[static_cast<std::size_t>(second)];
                    node.data = owner_at(data);
                }
            }
            special_count = static_cast<int>(special.size());
            tables.registers.tree.nodes.root = registers.empty() ? nullptr : registers.data();
            tables.special.tree.nodes.root = special_count == 0 ? nullptr : special.data();
            int queries;
            std::cin >> queries;
            for (int query = 0; query < queries; ++query)
            {
                u32 register_kind;
                int owner;
                std::cin >> register_kind >> owner;
                const ShaderValidationOwner* found = find_shader_validation_register_owner(tables, register_kind);
                std::printf("%d %d %d ", found == nullptr ? -1 : static_cast<int>(found - owners.data()),
                    find_shader_validation_owner_register_kind(tables, owner_at(owner)),
                    owner >= 0 && shader_validation_owner_chain_has_kind_one(tables, owner_at(owner)) ? 1 : 0);
            }
            std::printf("\n");
        }
        else if (kind == "equal")
        {
            int owner_count, active;
            std::cin >> owner_count >> active;
            std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(owner_count));
            std::vector<ShaderValidationOperand> operands(static_cast<std::size_t>(owner_count));
            auto owner_at = [&](int index) -> ShaderValidationOwner*
            {
                return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            for (int index = 0; index < owner_count; ++index)
            {
                int operand, parent;
                std::cin >> owners[static_cast<std::size_t>(index)].kind >> owners[static_cast<std::size_t>(index)].source >> operand >> parent;
                owners[static_cast<std::size_t>(index)].operand = operand < 0 ? nullptr : &operands[static_cast<std::size_t>(operand)];
                operands[static_cast<std::size_t>(index)].parent = owner_at(parent);
            }
            int queries;
            std::cin >> queries;
            for (int query = 0; query < queries; ++query)
            {
                int first, second;
                std::cin >> first >> second;
                std::printf("%d ", shader_validation_owners_equivalent(owner_at(first), owner_at(second), owner_at(active)) ? 1 : 0);
            }
            std::printf("\n");
        }
        else if (kind == "masks")
        {
            ShaderValidationOwner owner{};
            ShaderValidationOperand operand{};
            ShaderValidationChannelOrder order{0x00203794, {}};
            ShaderValidationChannelOrder other{0x00203794, {}};
            ShaderValidationChannelMask mask{0x00203790, {}};
            for (auto& channel : owner.channels)
            {
                std::cin >> channel;
            }
            for (auto& channel : operand.channel_order.channels)
            {
                std::cin >> channel;
            }
            for (auto& channel : mask.channels)
            {
                u32 value;
                std::cin >> value;
                channel = static_cast<u8>(value);
            }
            for (auto& channel : order.channels)
            {
                std::cin >> channel;
            }
            for (auto& channel : other.channels)
            {
                std::cin >> channel;
            }
            ShaderValidationChannelMask owner_mask{0, {9, 9, 9, 9}}, source_mask{0, {9, 9, 9, 9}};
            ShaderValidationChannelOrder composed{};
            const bool a = shader_validation_owner_channel_mask(owner, owner_mask) == &owner_mask;
            const bool b = shader_validation_operand_source_mask(operand, mask, source_mask) == &source_mask;
            const bool c = compose_shader_validation_channel_orders(order, other, composed) == &composed;
            std::printf("%08X %02X%02X%02X%02X %08X %02X%02X%02X%02X %08X %08X %08X %08X %08X %d%d%d\n", owner_mask.vtable_address,
                owner_mask.channels[0], owner_mask.channels[1], owner_mask.channels[2], owner_mask.channels[3], source_mask.vtable_address,
                source_mask.channels[0], source_mask.channels[1], source_mask.channels[2], source_mask.channels[3], composed.vtable_address,
                composed.channels[0], composed.channels[1], composed.channels[2], composed.channels[3], a, b, c);
        }
        else if (kind == "instr")
        {
            ShaderValidationInstruction instruction{};
            std::array<ShaderValidationOperand, 3> operands{};
            std::array<ShaderValidationOwner, 3> owners{};
            u32 flag, has_output, vector_destination, scalar_destination, destination;
            std::cin >> instruction.vector_opcode >> instruction.scalar_opcode >> flag >> has_output >> vector_destination >>
                scalar_destination >> destination;
            auto owner_at = [&](u32 index) -> ShaderValidationOwner*
            {
                return index >= 3 ? nullptr : &owners[index];
            };
            instruction.output_flag = static_cast<u8>(flag);
            instruction.output_register = has_output ? &owners[2] : nullptr;
            instruction.vector_destination = owner_at(vector_destination);
            instruction.scalar_destination = owner_at(scalar_destination);
            std::array<ShaderValidationOwner, 3> parents{};
            for (std::size_t index = 0; index < 3; ++index)
            {
                u32 marker, has_parent;
                std::cin >> marker >> operands[index].kind >> has_parent;
                for (auto& channel : parents[index].channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = value == 0xFFFFFFFFu ? shader_validation_unwritten_channel : value;
                }
                operands[index].marker = static_cast<u8>(marker);
                operands[index].instruction = &instruction;
                operands[index].parent = has_parent ? &parents[index] : nullptr;
                instruction.operands[index] = &operands[index];
            }
            for (auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
            {
                for (auto& channel : mask->channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
            }
            for (u32 slot = 0; slot < 4; ++slot)
            {
                std::printf("%d", shader_validation_instruction_uses_slot(instruction, slot) ? 1 : 0);
            }
            std::printf(" ");
            for (u32 slot = 0; slot < 4; ++slot)
            {
                for (i32 channel = 0; channel < 4; ++channel)
                {
                    std::printf("%d", shader_validation_instruction_reads_channel(instruction, slot, channel) ? 1 : 0);
                }
            }
            std::printf(" ");
            for (const auto& operand : operands)
            {
                for (i32 channel = 0; channel < 4; ++channel)
                {
                    std::printf("%d", shader_validation_operand_reads_channel(operand, channel) ? 1 : 0);
                }
            }
            for (u32 slot = 0; slot < 4; ++slot)
            {
                ShaderValidationChannelMask read{0, {9, 9, 9, 9}};
                shader_validation_instruction_read_mask(instruction, read, slot);
                std::printf(" %08X%02X%02X%02X%02X", read.vtable_address, read.channels[0], read.channels[1], read.channels[2], read.channels[3]);
            }
            std::printf(" %d", shader_validation_instruction_reads_unset_channel(instruction) ? 1 : 0);
            std::printf(" %d", negate_shader_validation_move_source(instruction, owner_at(destination)) ? 1 : 0);
            std::printf(" %d", convert_shader_validation_scalar_move(instruction) ? 1 : 0);
            auto owner_index = [&](const ShaderValidationOwner* owner)
            {
                return owner == nullptr ? -1 : static_cast<int>(owner - owners.data());
            };
            std::printf(" %u %u %u %d %d", instruction.vector_opcode, instruction.scalar_opcode, instruction.output_flag,
                owner_index(instruction.vector_destination), owner_index(instruction.scalar_destination));
            for (auto* operand : instruction.operands)
            {
                std::printf(" %d", static_cast<int>(operand - operands.data()));
            }
            for (auto& operand : operands)
            {
                std::printf(" %u %u", operand.marker, operand.kind);
            }
            std::printf(" %02X%02X%02X%02X %02X%02X%02X%02X\n", instruction.vector_mask.channels[0], instruction.vector_mask.channels[1],
                instruction.vector_mask.channels[2], instruction.vector_mask.channels[3], instruction.scalar_mask.channels[0],
                instruction.scalar_mask.channels[1], instruction.scalar_mask.channels[2], instruction.scalar_mask.channels[3]);
        }
        else if (kind == "setops")
        {
            ShaderValidationChannelMask mask{0x00203790, {}}, other{0x00203790, {}};
            for (auto* target : {&mask, &other})
            {
                for (auto& channel : target->channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
            }
            ShaderValidationChannelMask both{0, {9, 9, 9, 9}}, either{0, {9, 9, 9, 9}};
            const bool a = intersect_shader_validation_channel_masks(mask, other, both) == &both;
            const bool b = unite_shader_validation_channel_masks(mask, other, either) == &either;
            std::printf("%08X %02X%02X%02X%02X %08X %02X%02X%02X%02X %d%d\n", both.vtable_address, both.channels[0], both.channels[1],
                both.channels[2], both.channels[3], either.vtable_address, either.channels[0], either.channels[1], either.channels[2],
                either.channels[3], a, b);
        }
        else if (kind == "tomove")
        {
            int owner_count, writer_count;
            u32 check, vop, sop, flag;
            int vector_destination;
            std::cin >> owner_count >> writer_count >> check >> vop >> sop >> flag >> vector_destination;
            std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(owner_count));
            std::vector<ShaderValidationInstruction> writers(static_cast<std::size_t>(writer_count));
            auto owner_at = [&](int index) -> ShaderValidationOwner*
            {
                return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            for (auto& writer : writers)
            {
                int destination;
                std::cin >> destination;
                writer.scalar_destination = owner_at(destination);
            }
            for (auto& owner : owners)
            {
                int derived, channel_owner;
                std::cin >> derived >> channel_owner;
                owner.derived_owner = owner_at(derived);
                owner.channel_owner = owner_at(channel_owner);
                for (auto& channel : owner.channels)
                {
                    int code;
                    std::cin >> code;
                    channel = code == 0 ? 0 : code < 0 ? shader_validation_unwritten_channel
                                                       : reinterpret_cast<ShaderValidationChannelWriter>(&writers[static_cast<std::size_t>(code - 1)]);
                }
            }
            ShaderValidationInstruction instruction{};
            std::array<ShaderValidationOperand, 3> operands{};
            for (std::size_t index = 0; index < 3; ++index)
            {
                operands[index].kind = static_cast<u32>(index) + 7;
                instruction.operands[index] = &operands[index];
            }
            instruction.vector_opcode = vop;
            instruction.scalar_opcode = sop;
            instruction.output_flag = static_cast<u8>(flag);
            instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0x80, 0}};
            instruction.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 1}};
            instruction.vector_destination = owner_at(vector_destination);
            const bool converted = convert_shader_validation_vector_move(instruction, static_cast<u8>(check));
            auto owner_index = [&](const ShaderValidationOwner* owner)
            {
                return owner == nullptr ? -1 : static_cast<int>(owner - owners.data());
            };
            std::printf("%d %u %u %u %d %d", converted ? 1 : 0, instruction.vector_opcode, instruction.scalar_opcode, instruction.output_flag,
                owner_index(instruction.vector_destination), owner_index(instruction.scalar_destination));
            for (auto* operand : instruction.operands)
            {
                std::printf(" %d %u", static_cast<int>(operand - operands.data()), operand->kind);
            }
            std::printf(" %02X%02X%02X%02X %02X%02X%02X%02X\n", instruction.vector_mask.channels[0], instruction.vector_mask.channels[1],
                instruction.vector_mask.channels[2], instruction.vector_mask.channels[3], instruction.scalar_mask.channels[0],
                instruction.scalar_mask.channels[1], instruction.scalar_mask.channels[2], instruction.scalar_mask.channels[3]);
        }
        else if (kind == "reach")
        {
            int owner_count;
            std::cin >> owner_count;
            std::vector<ShaderValidationOwner> owners(static_cast<std::size_t>(owner_count));
            auto owner_at = [&](int index) -> ShaderValidationOwner*
            {
                return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            for (auto& owner : owners)
            {
                int channel_owner;
                std::cin >> channel_owner;
                owner.channel_owner = owner_at(channel_owner);
                for (auto& channel : owner.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = value;
                }
            }
            ShaderValidationInstruction reader{}, writer{};
            std::array<ShaderValidationOperand, 3> operands{};
            auto read_masks = [&](ShaderValidationInstruction& instruction)
            {
                for (auto* mask : {&instruction.output_mask, &instruction.vector_mask, &instruction.scalar_mask})
                {
                    mask->vtable_address = 0x00203790;
                    for (auto& channel : mask->channels)
                    {
                        u32 value;
                        std::cin >> value;
                        channel = static_cast<u8>(value);
                    }
                }
                u32 flag;
                int output, vector_destination, scalar_destination;
                std::cin >> instruction.vector_opcode >> instruction.scalar_opcode >> flag >> output >> vector_destination >> scalar_destination;
                instruction.output_flag = static_cast<u8>(flag);
                instruction.output_register = owner_at(output);
                instruction.vector_destination = owner_at(vector_destination);
                instruction.scalar_destination = owner_at(scalar_destination);
            };
            read_masks(reader);
            read_masks(writer);
            int active;
            std::cin >> active;
            for (auto& owner : owners)
            {
                int linked;
                std::cin >> linked;
                owner.operand = linked < 0 ? nullptr : &operands[static_cast<std::size_t>(linked)];
            }
            std::array<ShaderValidationChannelMask, 3> masks{};
            for (auto& mask : masks)
            {
                for (auto& channel : mask.channels)
                {
                    u32 value;
                    std::cin >> value;
                    channel = static_cast<u8>(value);
                }
            }
            for (std::size_t index = 0; index < 3; ++index)
            {
                int parent;
                std::cin >> operands[index].kind >> parent;
                for (auto& channel : operands[index].channel_order.channels)
                {
                    std::cin >> channel;
                }
                operands[index].parent = owner_at(parent);
                operands[index].instruction = &reader;
                reader.operands[index] = &operands[index];
            }
            for (u32 slot = 0; slot < 3; ++slot)
            {
                std::printf("%d%d", shader_validation_vector_result_reaches(reader, slot, writer) ? 1 : 0,
                    shader_validation_scalar_result_reaches(reader, slot, writer) ? 1 : 0);
            }
            std::printf(" ");
            for (const auto& operand : operands)
            {
                for (u32 channel = 0; channel < 5; ++channel)
                {
                    std::printf("%d", shader_validation_operand_reads_order_channel(operand, channel) ? 1 : 0);
                }
            }
            const bool parents_present = operands[0].parent != nullptr && operands[1].parent != nullptr && operands[2].parent != nullptr;
            if (writer.vector_opcode != 0xD || parents_present)
            {
                std::printf(" %d", shader_validation_instruction_must_follow(reader, writer, owner_at(active)) ? 1 : 0);
            }
            else
            {
                std::printf(" x");
            }
            std::printf(" %d%d", shader_validation_instruction_depends_on(reader, writer, owner_at(active)) ? 1 : 0,
                shader_validation_instructions_can_pair(reader, writer) ? 1 : 0);
            for (std::size_t index = 0; index < 3; ++index)
            {
                std::printf("%d", shader_validation_operand_reads_masked_channel(operands[index], masks[index]) ? 1 : 0);
            }
            std::printf("\n");
        }
        else if (kind == "diag")
        {
            int count;
            u32 channel;
            std::cin >> count >> channel;
            ShaderValidationInstruction instruction{};
            std::array<ShaderValidationOperand, 3> operands{};
            instruction.vector_opcode = 4;
            instruction.scalar_opcode = 1;
            for (auto* mask : {&instruction.vector_mask, &instruction.scalar_mask})
            {
                for (auto& value : mask->channels)
                {
                    u32 read;
                    std::cin >> read;
                    value = static_cast<u8>(read);
                }
            }
            for (std::size_t index = 0; index < 3; ++index)
            {
                std::cin >> operands[index].kind;
                for (auto& value : operands[index].channel_order.channels)
                {
                    std::cin >> value;
                }
                operands[index].instruction = &instruction;
                instruction.operands[index] = &operands[index];
            }
            std::vector<ShaderValidationBalancedNode> nodes(static_cast<std::size_t>(count));
            auto at = [&](int index) -> ShaderValidationBalancedNode*
            {
                return index < 0 ? nullptr : &nodes[static_cast<std::size_t>(index)];
            };
            for (auto& node : nodes)
            {
                int parent, first, second;
                std::cin >> parent >> first >> second >> node.value;
                node.parent = at(parent);
                node.first = at(first);
                node.second = at(second);
            }
            ShaderValidationOwner owner{};
            owner.diagnostics.nodes.root = count > 0 ? nodes.data() : nullptr;
            const bool reads = shader_validation_owner_diagnostic_reads_channel(owner, channel, [&](u32 value) -> const ShaderValidationOperand*
                {
                    return &operands[value];
                });
            std::printf("%d\n", reads ? 1 : 0);
        }
        else if (kind == "swap")
        {
            ShaderValidationInstruction instruction{};
            std::array<ShaderValidationOperand, 3> operands{};
            std::array<ShaderValidationOwner, 2> owners{};
            auto owner_at = [&](int index) -> ShaderValidationOwner*
            {
                return index < 0 ? nullptr : &owners[static_cast<std::size_t>(index)];
            };
            u32 flag, has_output, match_first, match_second, first, second, query;
            int vector_destination, scalar_destination, destination;
            std::cin >> instruction.vector_opcode >> instruction.scalar_opcode >> flag >> has_output >> vector_destination >> scalar_destination >>
                destination >> match_first >> match_second >> first >> second >> query;
            instruction.output_flag = static_cast<u8>(flag);
            instruction.output_register = has_output ? &owners[1] : nullptr;
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
            for (std::size_t index = 0; index < 3; ++index)
            {
                for (auto& channel : operands[index].channel_order.channels)
                {
                    std::cin >> channel;
                }
                instruction.operands[index] = &operands[index];
            }
            int share_owner;
            u32 share_first, share_second;
            std::cin >> share_owner >> share_first >> share_second;
            for (auto& owner : owners)
            {
                for (auto& channel : owner.channels)
                {
                    int code;
                    std::cin >> code;
                    channel = code == 0 ? 0 : code < 0 ? shader_validation_unwritten_channel : reinterpret_cast<ShaderValidationChannelWriter>(&instruction);
                }
            }
            const bool shared = shader_validation_owner_channels_share_writer(owners[static_cast<std::size_t>(share_owner)], share_first, share_second);
            const bool match = shader_validation_destination_channels_match(instruction, owner_at(destination), match_first, match_second);
            const bool swapped = swap_shader_validation_destination_channels(instruction, owner_at(destination), first, second, static_cast<u8>(query));
            std::printf("%d %d %d %02X%02X%02X%02X %02X%02X%02X%02X", shared ? 1 : 0, match ? 1 : 0, swapped ? 1 : 0, instruction.vector_mask.channels[0],
                instruction.vector_mask.channels[1], instruction.vector_mask.channels[2], instruction.vector_mask.channels[3],
                instruction.scalar_mask.channels[0], instruction.scalar_mask.channels[1], instruction.scalar_mask.channels[2],
                instruction.scalar_mask.channels[3]);
            for (const auto& operand : operands)
            {
                std::printf(" %u%u%u%u", operand.channel_order.channels[0], operand.channel_order.channels[1], operand.channel_order.channels[2],
                    operand.channel_order.channels[3]);
            }
            std::printf("\n");
        }
        else if (kind == "append")
        {
            int initial, appends;
            std::cin >> initial >> appends;
            std::vector<ShaderValidationListNode> nodes(32);
            std::size_t used = 0;
            std::vector<int> failures;
            int failure_count, calls = 0;
            std::cin >> failure_count;
            failures.resize(static_cast<std::size_t>(failure_count));
            for (auto& failure : failures)
            {
                std::cin >> failure;
            }
            auto allocate = [&](std::size_t, u32) -> void*
            {
                const int call = calls++;
                for (int failure : failures)
                {
                    if (failure == call)
                    {
                        return nullptr;
                    }
                }
                return &nodes[used++];
            };
            static const ShaderValidationListVtable vtable{nullptr};
            ShaderValidationList list{0, {nullptr, nullptr, 0}};
            std::vector<u32> values(static_cast<std::size_t>(appends));
            for (int index = 0; index < appends; ++index)
            {
                append_shader_validation_list_value(list, &vtable, allocate, &values[static_cast<std::size_t>(index)]);
            }
            auto node_index = [&](const ShaderValidationListNode* node)
            {
                return node == nullptr ? -1 : static_cast<int>(node - nodes.data());
            };
            std::printf("%d %d %u |", node_index(list.storage.first), node_index(list.storage.last), list.storage.count);
            for (std::size_t index = 0; index < used; ++index)
            {
                std::printf(" %d %d %d", static_cast<int>(static_cast<u32*>(nodes[index].value) - values.data()), node_index(nodes[index].previous),
                    node_index(nodes[index].next));
            }
            std::printf("\n");
            (void)initial;
        }
    }
}
