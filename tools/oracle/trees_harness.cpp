#include "shader_validation_tree_instances.h"
#include <cstdio>
#include <iostream>
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

// Input per case: kind function count root then per node: value parent first second height.
// Remaining arguments depend on kind. Output: result, tree root/count, then every node's links.
int main()
{
    std::string kind;
    while (std::cin >> kind)
    {
        int function, count, root;
        std::cin >> function >> count >> root;
        if (kind == "pass")
        {
            std::vector<ShaderValidationPassNode> nodes(64);
            auto at = [&](int index) -> ShaderValidationPassNode*
            {
                return index < 0 ? nullptr : &nodes[static_cast<std::size_t>(index)];
            };
            for (int index = 0; index < count; ++index)
            {
                auto& node = nodes[static_cast<std::size_t>(index)];
                int parent, first, second;
                std::cin >> node.value >> parent >> first >> second >> node.height;
                node.parent = at(parent);
                node.first = at(first);
                node.second = at(second);
            }
            u32 key, fail;
            int start;
            std::cin >> key >> start >> fail;
            ShaderValidationPassTree tree{at(root), static_cast<u32>(count)};
            std::size_t used = static_cast<std::size_t>(count);
            int calls = 0;
            const auto allocate = [&](std::size_t, u32) -> void*
            {
                return calls++ == 0 && fail != 0 ? nullptr : &nodes[used++];
            };
            u32 data = 0;
            ShaderValidationPassNode* result = insert_shader_validation_pass_node(tree, allocate, key, &data, at(start));
            std::printf("%d %d %u |", index_of(nodes, result), index_of(nodes, tree.root), tree.count);
            for (std::size_t index = 0; index < used; ++index)
            {
                const auto& node = nodes[index];
                std::printf(" %08X %d %d %d %d %d", node.value, index_of(nodes, node.parent), index_of(nodes, node.first),
                    index_of(nodes, node.second), node.height, node.data == &data ? 1 : 0);
            }
            std::printf("\n");
            continue;
        }
        std::vector<ShaderValidationBalancedNode> nodes(64);
        auto at = [&](int index) -> ShaderValidationBalancedNode*
        {
            return index < 0 ? nullptr : &nodes[static_cast<std::size_t>(index)];
        };
        for (int index = 0; index < count; ++index)
        {
            auto& node = nodes[static_cast<std::size_t>(index)];
            int parent, first, second;
            std::cin >> node.value >> parent >> first >> second >> node.height;
            node.parent = at(parent);
            node.first = at(first);
            node.second = at(second);
        }
        ShaderValidationBalancedTree tree{at(root), static_cast<u32>(count)};
        std::vector<int> destroyed;
        ShaderValidationEraseContext erase{tree, [&](ShaderValidationBalancedNode& node, u32 flags)
            {
                destroyed.push_back(static_cast<int>(&node - nodes.data()) * 10 + static_cast<int>(flags));
            }};
        std::size_t used = static_cast<std::size_t>(count);
        u32 fail = 0;
        int calls = 0;
        ShaderValidationInsertContext insert{tree, [&](std::size_t, u32) -> void*
            {
                return calls++ == 0 && fail != 0 ? nullptr : &nodes[used++];
            }};
        int result = -2;
        if (kind == "join")
        {
            int parent, first, second;
            std::cin >> parent >> first >> second;
            using Join = ShaderValidationBalancedNode* (*)(ShaderValidationBalancedNode*, ShaderValidationBalancedNode*, ShaderValidationBalancedNode*);
            const Join joins[] = {join_shader_validation_balanced_nodes_19164f, join_shader_validation_balanced_nodes_191714,
                join_shader_validation_balanced_nodes_191806, join_shader_validation_balanced_nodes_1918cb, join_shader_validation_balanced_nodes_191b1c};
            result = index_of(nodes, joins[function](at(parent), at(first), at(second)));
        }
        else if (kind == "erase")
        {
            int target;
            std::cin >> target;
            using Erase = ShaderValidationBalancedNode* (*)(ShaderValidationEraseContext&, ShaderValidationBalancedNode*);
            const Erase erases[] = {erase_shader_validation_balanced_node_19204a, erase_shader_validation_balanced_node_19214a,
                erase_shader_validation_balanced_node_19224a, erase_shader_validation_balanced_node_19234a, erase_shader_validation_balanced_node_192529};
            result = index_of(nodes, erases[function](erase, at(target)));
        }
        else if (kind == "unreg")
        {
            u32 value;
            std::cin >> value;
            if (function == 0)
            {
                unregister_shader_validation_value_1926fb(erase, value);
            }
            else
            {
                unregister_shader_validation_value_1927b9(erase, value);
            }
        }
        else if (kind == "insert")
        {
            u32 value;
            int start;
            std::cin >> value >> start >> fail;
            ShaderValidationBalancedNode* inserted = function == 0 ? insert_shader_validation_node_2037ec(insert, value, at(start))
                                                                   : insert_shader_validation_node_2037f4(insert, value, at(start));
            result = index_of(nodes, inserted);
        }
        std::printf("%d %d %u |", result, index_of(nodes, tree.root), tree.count);
        for (int value : destroyed)
        {
            std::printf(" d%d", value);
        }
        for (std::size_t index = 0; index < used; ++index)
        {
            const auto& node = nodes[index];
            std::printf(" %08X %08X %d %d %d %d", node.vtable_address, node.value, index_of(nodes, node.parent), index_of(nodes, node.first),
                index_of(nodes, node.second), node.height);
        }
        std::printf("\n");
    }
}
