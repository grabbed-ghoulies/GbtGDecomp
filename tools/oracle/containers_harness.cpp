#include "shader_validation_containers.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
using namespace gbtg;

namespace
{
void print_tree(const ShaderValidationBalancedNode* node)
{
    if (node == nullptr)
    {
        std::printf(" .");
        return;
    }
    std::printf(" (%08X %d", node->value, node->height);
    print_tree(node->first);
    print_tree(node->second);
    std::printf(")");
}

std::vector<int>* destroyed = nullptr;
std::vector<ShaderValidationListNode>* list_nodes = nullptr;

void record_destroy(ShaderValidationListNode& node, u32)
{
    destroyed->push_back(static_cast<int>(&node - list_nodes->data()));
}
}

int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        if (kind == "init")
        {
            u32 value, data;
            std::cin >> value >> data;
            ShaderValidationBalancedNode parent{};
            ShaderValidationBalancedNode nodes[3];
            ShaderValidationBalancedNode* (*initializers[3])(ShaderValidationBalancedNode&, u32, ShaderValidationBalancedNode*) = {
                initialize_shader_validation_node_2037ec, initialize_shader_validation_node_2037f0,
                initialize_shader_validation_node_2037f4};
            for (int index = 0; index < 3; ++index)
            {
                nodes[index] = ShaderValidationBalancedNode{9, 9, nullptr, &parent, &parent, 9};
                const bool returned = initializers[index](nodes[index], value, index == 1 ? nullptr : &parent) == &nodes[index];
                std::printf("%08X %08X %d %d %d %d %d ", nodes[index].vtable_address, nodes[index].value, nodes[index].parent == &parent ? 1 : 0,
                    nodes[index].first == nullptr ? 1 : 0, nodes[index].second == nullptr ? 1 : 0, nodes[index].height, returned ? 1 : 0);
            }
            ShaderValidationPassNode pass_parent{};
            ShaderValidationPassNode pass{9, 9, nullptr, nullptr, &pass_parent, &pass_parent, 9};
            const bool returned = initialize_shader_validation_pass_node(pass, value, reinterpret_cast<void*>(static_cast<std::uintptr_t>(data)), &pass_parent) == &pass;
            std::printf("%08X %08X %08X %d %d %d %d %d\n", pass.vtable_address, pass.value, static_cast<u32>(reinterpret_cast<std::uintptr_t>(pass.data)), pass.parent == &pass_parent ? 1 : 0,
                pass.first == nullptr ? 1 : 0, pass.second == nullptr ? 1 : 0, pass.height, returned ? 1 : 0);
        }
        else if (kind == "height")
        {
            int has_first, has_second;
            i32 first_height, second_height;
            std::cin >> has_first >> first_height >> has_second >> second_height;
            ShaderValidationPassNode first{}, second{}, node{};
            first.height = first_height;
            second.height = second_height;
            node.first = has_first ? &first : nullptr;
            node.second = has_second ? &second : nullptr;
            std::printf("%d\n", calculate_shader_validation_pass_node_height(node));
        }
        else if (kind == "tree")
        {
            int count;
            std::cin >> count;
            std::vector<ShaderValidationBalancedNode> nodes(static_cast<std::size_t>(count));
            auto at = [&](int index) -> ShaderValidationBalancedNode*
            {
                return index < 0 ? nullptr : &nodes[static_cast<std::size_t>(index)];
            };
            for (auto& node : nodes)
            {
                int parent, first, second;
                std::cin >> parent >> first >> second;
                node.parent = at(parent);
                node.first = at(first);
                node.second = at(second);
            }
            int queries;
            std::cin >> queries;
            for (int query = 0; query < queries; ++query)
            {
                int index;
                std::cin >> index;
                ShaderValidationBalancedNode* iterator = at(index);
                const bool returned = retreat_shader_validation_node_iterator(&iterator) != nullptr;
                std::printf("%d ", iterator == nullptr ? -1 : static_cast<int>(iterator - nodes.data()));
                (void)returned;
            }
            std::printf("\n");
        }
        else if (kind == "pass")
        {
            int count;
            std::cin >> count;
            std::vector<ShaderValidationPassNode> nodes(static_cast<std::size_t>(count));
            for (auto& node : nodes)
            {
                int first, second;
                std::cin >> node.value >> first >> second;
                node.first = first < 0 ? nullptr : &nodes[static_cast<std::size_t>(first)];
                node.second = second < 0 ? nullptr : &nodes[static_cast<std::size_t>(second)];
            }
            const ShaderValidationPassTree tree{count > 0 ? &nodes[0] : nullptr, 0};
            int queries;
            std::cin >> queries;
            for (int query = 0; query < queries; ++query)
            {
                u32 key;
                std::cin >> key;
                ShaderValidationPassNode* found = find_shader_validation_pass_node(tree, key);
                std::printf("%d ", found == nullptr ? -1 : static_cast<int>(found - nodes.data()));
            }
            std::printf("\n");
        }
        else if (kind == "last")
        {
            int count;
            u32 stored_count;
            std::cin >> stored_count >> count;
            std::vector<ShaderValidationTreeNode> nodes(static_cast<std::size_t>(count));
            std::vector<u32> values(static_cast<std::size_t>(count));
            for (int index = 0; index < count; ++index)
            {
                std::cin >> values[static_cast<std::size_t>(index)];
                nodes[static_cast<std::size_t>(index)].value = &values[static_cast<std::size_t>(index)];
                nodes[static_cast<std::size_t>(index)].first = index + 1 < count ? &nodes[static_cast<std::size_t>(index + 1)] : nullptr;
            }
            const ShaderValidationTreeStorage storage{count > 0 ? &nodes[0] : nullptr, stored_count};
            void* value = last_shader_validation_tree_value(storage);
            std::printf("%08X\n", value == nullptr ? 0u : *static_cast<u32*>(value));
        }
        else if (kind == "list")
        {
            int count;
            std::cin >> count;
            std::vector<ShaderValidationListNode> nodes(static_cast<std::size_t>(count));
            std::vector<int> order;
            destroyed = &order;
            list_nodes = &nodes;
            static const ShaderValidationListVtable vtable{record_destroy};
            for (int index = 0; index < count; ++index)
            {
                auto& node = nodes[static_cast<std::size_t>(index)];
                node.vtable = &vtable;
                node.previous = index > 0 ? &nodes[static_cast<std::size_t>(index - 1)] : nullptr;
                node.next = index + 1 < count ? &nodes[static_cast<std::size_t>(index + 1)] : nullptr;
            }
            ShaderValidationList list{0, {count > 0 ? &nodes[0] : nullptr, count > 0 ? &nodes.back() : nullptr, static_cast<u32>(count)}};
            destroy_shader_validation_list(list);
            std::printf("%08X %d %d %u |", list.vtable_address, list.storage.first == nullptr ? -1 : static_cast<int>(list.storage.first - nodes.data()),
                list.storage.last == nullptr ? -1 : static_cast<int>(list.storage.last - nodes.data()), list.storage.count);
            for (int index : order)
            {
                std::printf(" %d", index);
            }
            std::printf("\n");
        }
        else if (kind == "single")
        {
            u32 channel;
            std::cin >> channel;
            ShaderValidationChannelMask mask{0, {9, 9, 9, 9}};
            const bool returned = construct_shader_validation_single_channel_mask(mask, channel) == &mask;
            std::printf("%08X %02X%02X%02X%02X %d\n", mask.vtable_address, mask.channels[0], mask.channels[1], mask.channels[2],
                mask.channels[3], returned ? 1 : 0);
        }
        else if (kind == "link")
        {
            int has_previous, has_next;
            std::cin >> has_previous >> has_next;
            ShaderValidationListNode previous{}, next{}, node{};
            u32 value = 0;
            static const ShaderValidationListVtable vtable{nullptr};
            const bool returned = construct_shader_validation_list_node(node, &vtable, &value, has_previous ? &previous : nullptr,
                                      has_next ? &next : nullptr) == &node;
            std::printf("%d %d %d %d %d %d\n", node.value == &value ? 1 : 0, node.previous == (has_previous ? &previous : nullptr) ? 1 : 0,
                node.next == (has_next ? &next : nullptr) ? 1 : 0, previous.next == &node ? 1 : 0, next.previous == &node ? 1 : 0,
                returned ? 1 : 0);
        }
        else if (kind == "base")
        {
            int count;
            std::cin >> count;
            ShaderValidationBalancedTree tree{nullptr, 0};
            std::vector<ShaderValidationBalancedNode> storage(64);
            std::size_t used = 0;
            std::vector<int> failures;
            int calls = 0;
            ShaderValidationInsertContext context{tree, [&](std::size_t, u32) -> void*
                {
                    const int call = calls++;
                    for (int failure : failures)
                    {
                        if (failure == call)
                        {
                            return nullptr;
                        }
                    }
                    return &storage[used++];
                }};
            int failure_count;
            std::cin >> failure_count;
            failures.resize(static_cast<std::size_t>(failure_count));
            for (auto& failure : failures)
            {
                std::cin >> failure;
            }
            for (int index = 0; index < count; ++index)
            {
                u32 address;
                std::cin >> address;
                ShaderValidationInstruction instruction{};
                instruction.base.vtable_address = 0;
                instruction.output_mask = instruction.vector_mask = instruction.scalar_mask = ShaderValidationChannelMask{1, {7, 7, 7, 7}};
                const bool returned = construct_shader_validation_instruction_base(context, instruction, address) == &instruction;
                std::printf("%08X %08X %02X%02X%02X%02X %08X %08X %d ", instruction.base.vtable_address, instruction.output_mask.vtable_address,
                    instruction.output_mask.channels[0], instruction.output_mask.channels[3], instruction.vector_mask.channels[1],
                    instruction.scalar_mask.channels[2], instruction.vector_mask.vtable_address, instruction.scalar_mask.vtable_address,
                    returned ? 1 : 0);
            }
            std::printf("| %u", tree.count);
            print_tree(tree.root);
            std::printf("\n");
        }
    }
}
