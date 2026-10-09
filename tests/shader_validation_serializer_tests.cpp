#include "../src/engine/graphics/shader_validation_channel_ops.h"
#include "../src/engine/graphics/shader_validation_instruction_ops.h"
#include "../src/engine/graphics/shader_validation_registers.h"
#include "../src/engine/graphics/shader_validation_sets.h"
#include <array>
#include <map>
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

// Expected words were produced by executing the retail 0x0018F8B7 and 0x001914E8
// machine code against the same object graph in a CPU emulator.
struct SerializerFixture
{
    std::array<gbtg::ShaderValidationOwner, 4> owners{};
    std::array<gbtg::ShaderValidationOperand, 6> operands{};
    std::array<gbtg::ShaderValidationInstruction, 2> instructions{};

    SerializerFixture()
    {
        using namespace gbtg;
        const std::array<std::array<u32, 2>, 4> owner_words{{{3, 0x1A5}, {2, 7}, {0, 0x12345678}, {1, 9}}};
        for (std::size_t index = 0; index < owners.size(); ++index)
        {
            owners[index].kind = owner_words[index][0];
            owners[index].source = owner_words[index][1];
        }
        owners[0].operand = &operands[3];
        owners[3].operand = &operands[4];
        ShaderValidationOwner* const parents[6] = {&owners[0], &owners[1], &owners[2], &owners[1], &owners[3], nullptr};
        const u8 markers[6] = {1, 0, 0xFE, 0, 0, 3};
        const std::array<std::array<u32, 4>, 6> channels{{
            {0, 4, 2, 4}, {4, 4, 4, 4}, {3, 2, 1, 0}, {0, 1, 2, 3}, {4, 1, 4, 4}, {7, 4, 4, 0x80000002}}};
        for (std::size_t index = 0; index < operands.size(); ++index)
        {
            operands[index].parent = parents[index];
            operands[index].marker = markers[index];
            operands[index].channel_order = ShaderValidationChannelOrder{0x00203794, channels[index]};
        }
        ShaderValidationInstruction& arl = instructions[0];
        arl.operands = {&operands[0], &operands[1], &operands[2]};
        arl.output_flag = 1;
        arl.output_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0x80, 0}};
        arl.vector_mask = ShaderValidationChannelMask{0x00203790, {0xFF, 0, 1, 0}};
        arl.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 1}};
        arl.vector_opcode = 0xD;
        arl.scalar_opcode = 3;
        arl.output_register = &owners[0];
        arl.vector_destination = &owners[1];
        arl.scalar_destination = &owners[2];
        ShaderValidationInstruction& scalar = instructions[1];
        scalar.operands = {&operands[4], &operands[5], &operands[0]};
        scalar.output_flag = 0;
        scalar.output_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 0, 0}};
        scalar.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 1, 1, 1}};
        scalar.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 0, 0}};
        scalar.vector_opcode = 0;
        scalar.scalar_opcode = 0x15;
        scalar.output_register = nullptr;
        scalar.vector_destination = &owners[1];
        scalar.scalar_destination = nullptr;
    }

    bool channels_normalized() const
    {
        using Channels = std::array<gbtg::u32, 4>;
        const Channels expected[6] = {{0, 0, 2, 2}, {0, 0, 0, 0}, {3, 2, 1, 0}, {0, 1, 2, 3}, {1, 1, 1, 1}, {7, 7, 7, 0x80000002}};
        for (std::size_t index = 0; index < operands.size(); ++index)
        {
            if (operands[index].channel_order.channels != expected[index] ||
                operands[index].channel_order.vtable_address != 0x00203794)
            {
                return false;
            }
        }
        return true;
    }
};
}

namespace
{
void check_shader_validation_accessors()
{
    using namespace gbtg;
    ShaderValidationChannelMask mask{0x11111111, {9, 9, 9, 9}};
    require(construct_shader_validation_channel_mask(mask, 1, 0, 0x80, 0xFF) == &mask && mask.vtable_address == 0x00203790 &&
            mask.channels == std::array<u8, 4>{1, 0, 0x80, 0xFF},
        "mask constructor stores four bytes and vtable");
    ShaderValidationChannelOrder order{};
    require(construct_shader_validation_channel_order(order, 2, 4, 0x80000000, 0) == &order &&
            order.vtable_address == 0x00203794 && order.channels == std::array<u32, 4>{2, 4, 0x80000000, 0},
        "order constructor stores four words and vtable");
    require(shader_validation_channel_order_contains(order, 0x80000000) && !shader_validation_channel_order_contains(order, 1),
        "order contains compares complete words");
    require(shader_validation_channel_order_keeps(order, 4) == 1 && shader_validation_channel_order_keeps(order, 1) == 1 &&
            shader_validation_channel_order_keeps(order, 3) == 0 && shader_validation_channel_order_keeps(order, 0) == 0,
        "order keeps identity, missing and index-four channels only");
    require(find_shader_validation_channel_order_index(order, 4) == 1 && find_shader_validation_channel_order_index(order, 0) == 3 &&
            find_shader_validation_channel_order_index(order, 7) == 4,
        "order index search returns first match or four");

    ShaderValidationInstruction instruction{};
    instruction.output_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 1, 0}};
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 0, 0x80}};
    instruction.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 0}};
    ShaderValidationChannelMask copied{0, {}};
    require(copy_shader_validation_scalar_mask(instruction, copied) == &copied && copied.vtable_address == 0x00203790 &&
            copied.channels == std::array<u8, 4>{0, 1, 0, 0},
        "scalar mask getter copies +0x28 bytes");
    copy_shader_validation_output_mask(instruction, copied);
    require(copied.channels == std::array<u8, 4>{1, 0, 1, 0}, "output mask getter copies +0x18 bytes");
    copy_shader_validation_vector_mask(instruction, copied);
    require(copied.channels == std::array<u8, 4>{0, 0, 0, 0x80}, "vector mask getter copies +0x20 bytes");
    require(shader_validation_vector_writes_channel(instruction, 0) == 1 && shader_validation_scalar_writes_channel(instruction, 0) == 0 &&
            shader_validation_vector_writes_channel(instruction, 1) == 0 && shader_validation_scalar_writes_channel(instruction, 1) == 1 &&
            shader_validation_vector_writes_channel(instruction, 3) == 1,
        "output mask belongs to the vector unit when the output flag is clear");
    instruction.output_flag = 0x80;
    require(shader_validation_vector_writes_channel(instruction, 0) == 0 && shader_validation_scalar_writes_channel(instruction, 2) == 1,
        "any nonzero output flag moves the output mask to the scalar unit");
    const std::array<std::array<u32, 5>, 8> opcodes{{
        {3, 0, 1, 1, 0}, {4, 0, 1, 1, 0}, {5, 0, 0, 1, 0}, {7, 0, 0, 1, 0}, {8, 0, 0, 0, 0},
        {0x80000005, 0, 0, 0, 0}, {1, 0, 0, 0, 1}, {0, 1, 0, 0, 1}}};
    for (const auto& row : opcodes)
    {
        instruction.vector_opcode = row[0];
        instruction.scalar_opcode = row[1];
        require(shader_validation_vector_opcode_is_add_or_mad(instruction) == row[2] &&
                shader_validation_vector_opcode_is_add_through_dp4(instruction) == row[3] &&
                shader_validation_instruction_is_single_move(instruction) == row[4],
            "opcode predicates use exact and signed range comparisons");
    }
    instruction.vector_opcode = 1;
    instruction.scalar_opcode = 1;
    require(shader_validation_instruction_is_single_move(instruction) == 0, "paired moves are not a single move");

    std::array<ShaderValidationOwner, 4> owners{};
    set_shader_validation_owner_derived(owners[0], &owners[1]);
    require(owners[0].derived_owner == &owners[1] && owners[1].channel_owner == &owners[0], "derived link is reciprocal");
    set_shader_validation_owner_derived(owners[2], &owners[1]);
    require(owners[0].derived_owner == nullptr && owners[2].derived_owner == &owners[1] && owners[1].channel_owner == &owners[2],
        "relinking a derived owner clears its previous channel owner");
    set_shader_validation_owner_channel_owner(owners[3], &owners[2]);
    require(owners[3].channel_owner == &owners[2] && owners[2].derived_owner == &owners[3] && owners[1].channel_owner == nullptr,
        "channel-owner link clears the previous derived owner");
    set_shader_validation_owner_derived(owners[3], nullptr);
    set_shader_validation_owner_channel_owner(owners[1], nullptr);
    require(owners[3].derived_owner == nullptr && owners[3].channel_owner == &owners[2] && owners[1].channel_owner == nullptr,
        "null links clear only the caller field");
    owners[2].channels = {0, 0, 7, 0};
    owners[3].channels = {0, 5, 0, 0};
    require(find_shader_validation_owner_channel(&owners[3], 1) == 5 && find_shader_validation_owner_channel(&owners[3], 2) == 7 &&
            find_shader_validation_owner_channel(&owners[3], 0) == 0,
        "owner channel search follows channel owners to the first nonzero channel");

    std::array<ShaderValidationPassNode, 4> nodes{};
    nodes[0].value = 0;
    nodes[1].value = 0xFFFFFFFE;
    nodes[2].value = 5;
    nodes[3].value = 0x80000000;
    nodes[0].first = &nodes[1];
    nodes[0].second = &nodes[2];
    nodes[1].first = &nodes[3];
    const ShaderValidationPassTree tree{&nodes[0], 4};
    require(find_shader_validation_pass_node_signed(tree, 0xFFFFFFFE) == &nodes[1] &&
            find_shader_validation_pass_node_signed(tree, 0x80000000) == &nodes[3] &&
            find_shader_validation_pass_node_signed(tree, 5) == &nodes[2] &&
            find_shader_validation_pass_node_signed(tree, 3) == nullptr,
        "pass lookup orders keys as signed integers");
    require(find_shader_validation_pass_node_signed(ShaderValidationPassTree{nullptr, 0}, 0) == nullptr, "empty pass tree has no node");
}
}

namespace
{
gbtg::ShaderValidationChannelOrder order_of(gbtg::u32 first, gbtg::u32 second, gbtg::u32 third, gbtg::u32 fourth)
{
    return gbtg::ShaderValidationChannelOrder{0x00203794, {first, second, third, fourth}};
}

void check_shader_validation_channel_ops()
{
    using namespace gbtg;
    const ShaderValidationChannelOrder swizzle = order_of(1, 0, 3, 2);
    const ShaderValidationChannelOrder partial = order_of(0, 4, 0, 3);
    require(shader_validation_channel_order_covers_all(swizzle) && !shader_validation_channel_order_covers_all(partial) &&
            find_shader_validation_uncovered_channel(partial) == 1 && find_shader_validation_uncovered_channel(swizzle) == 4,
        "coverage reports the first channel absent from an order");
    require(!shader_validation_channel_order_keeps_all(swizzle) && find_shader_validation_moved_channel(swizzle) == 0 &&
            shader_validation_channel_order_keeps_all(order_of(0, 4, 2, 4)) && find_shader_validation_moved_channel(partial) == 2,
        "kept channels are identity or missing");
    require(shader_validation_channel_orders_compatible(partial, order_of(4, 2, 0, 4)) &&
            !shader_validation_channel_orders_compatible(partial, order_of(4, 2, 1, 4)),
        "orders are compatible unless both define a channel differently");

    ShaderValidationChannelMask inverted{0, {9, 9, 9, 9}};
    require(invert_shader_validation_channel_mask(ShaderValidationChannelMask{0x00203790, {0, 1, 0x80, 0}}, inverted) == &inverted &&
            inverted.vtable_address == 0x00203790 && inverted.channels == std::array<u8, 4>{1, 0, 0, 1},
        "mask inversion maps zero bytes to one");

    require(!shader_validation_channel_repeats_before(partial, 0) && !shader_validation_channel_repeats_before(partial, 1) &&
            shader_validation_channel_repeats_before(partial, 2) && !shader_validation_channel_repeats_before(partial, 4) &&
            !shader_validation_channel_repeats_before(partial, -1),
        "repeat search compares earlier defined channels only");
    require(find_shader_validation_repeated_channel(partial) == 2 && find_shader_validation_repeated_channel(swizzle) == 4,
        "first repeated position is reported");
    ShaderValidationChannelOrder repeated = order_of(2, 2, 2, 1);
    clear_shader_validation_repeated_channels(repeated);
    require(repeated.channels == std::array<u32, 4>{2, 4, 4, 1}, "repeated channels become missing");

    ShaderValidationChannelOrder inverse{};
    require(invert_shader_validation_channel_order(order_of(2, 4, 0, 2), inverse) == &inverse &&
            inverse.vtable_address == 0x00203794 && inverse.channels == std::array<u32, 4>{2, 4, 0, 4},
        "order inversion records the first destination of each channel");
    ShaderValidationChannelMask used{0, {9, 9, 9, 9}};
    require(shader_validation_channel_order_used_mask(partial, used) == &used && used.vtable_address == 0x00203790 &&
            used.channels == std::array<u8, 4>{1, 0, 0, 1},
        "used mask marks channels read by an order");
}
}

namespace
{
std::vector<int>* destroyed_list_nodes = nullptr;
gbtg::ShaderValidationListNode* destroyed_list_base = nullptr;

void record_list_destroy(gbtg::ShaderValidationListNode& node, gbtg::u32 flags)
{
    destroyed_list_nodes->push_back(static_cast<int>(&node - destroyed_list_base) * 10 + static_cast<int>(flags));
}

void check_shader_validation_containers()
{
    using namespace gbtg;
    ShaderValidationBalancedNode parent{};
    ShaderValidationBalancedNode node{9, 9, nullptr, &parent, &parent, 9};
    require(initialize_shader_validation_node_2037f0(node, 0x1234, &parent) == &node && node.vtable_address == 0x002037f0 &&
            node.value == 0x1234 && node.parent == &parent && node.first == nullptr && node.second == nullptr && node.height == 1,
        "balanced node initializer clears children and sets height one");
    initialize_shader_validation_node_2037ec(node, 1, nullptr);
    require(node.vtable_address == 0x002037ec && node.parent == nullptr, "0x001902D9 installs its vtable");
    initialize_shader_validation_node_2037f4(node, 1, nullptr);
    require(node.vtable_address == 0x002037f4, "0x0019042A installs its vtable");
    ShaderValidationPassNode pass_parent{};
    ShaderValidationPassNode pass{9, 9, nullptr, nullptr, &pass_parent, &pass_parent, 9};
    require(initialize_shader_validation_pass_node(pass, 5, &pass_parent, &pass_parent) == &pass && pass.vtable_address == 0x002037f8 &&
            pass.value == 5 && pass.data == &pass_parent && pass.parent == &pass_parent && pass.first == nullptr && pass.height == 1,
        "pass node initializer stores key, data and parent");

    ShaderValidationPassNode first{}, second{};
    first.height = 3;
    second.height = 3;
    pass.first = &first;
    pass.second = &second;
    require(calculate_shader_validation_pass_node_height(pass) == 4, "equal pass heights use the second child");
    second.height = -2;
    require(calculate_shader_validation_pass_node_height(pass) == 4, "pass heights compare as signed integers");
    pass.first = nullptr;
    require(calculate_shader_validation_pass_node_height(pass) == 1, "a missing first child outranks a negative second height");
    second.height = 6;
    require(calculate_shader_validation_pass_node_height(pass) == 7, "a lone second child supplies its height plus one");
    pass.second = nullptr;
    require(calculate_shader_validation_pass_node_height(pass) == 1, "a leaf pass node has height one");

    std::array<ShaderValidationBalancedNode, 5> tree{};
    tree[0].first = &tree[1];
    tree[0].second = &tree[2];
    tree[1].parent = &tree[0];
    tree[1].second = &tree[3];
    tree[3].parent = &tree[1];
    tree[2].parent = &tree[0];
    tree[2].first = &tree[4];
    tree[4].parent = &tree[2];
    ShaderValidationBalancedNode* iterator = &tree[0];
    require(retreat_shader_validation_node_iterator(&iterator) == &iterator && iterator == &tree[3],
        "predecessor descends to the last node of the first subtree");
    iterator = &tree[4];
    retreat_shader_validation_node_iterator(&iterator);
    require(iterator == &tree[0], "predecessor climbs out of a first-child chain");
    iterator = &tree[3];
    retreat_shader_validation_node_iterator(&iterator);
    require(iterator == &tree[1], "predecessor of a second child is its parent");
    ShaderValidationBalancedNode lone{};
    iterator = &lone;
    retreat_shader_validation_node_iterator(&iterator);
    require(iterator == nullptr, "predecessor of a lone root is null");

    std::array<ShaderValidationPassNode, 2> pass_nodes{};
    pass_nodes[0].value = 0;
    pass_nodes[0].first = &pass_nodes[1];
    pass_nodes[1].value = 0xFFFFFFFF;
    require(find_shader_validation_pass_node(ShaderValidationPassTree{&pass_nodes[0], 2}, 0xFFFFFFFF) == &pass_nodes[1],
        "pass lookup wrapper forwards the signed key");

    std::array<ShaderValidationTreeNode, 2> chain{};
    u32 values[2] = {11, 22};
    chain[0].value = &values[0];
    chain[0].first = &chain[1];
    chain[1].value = &values[1];
    require(last_shader_validation_tree_value(ShaderValidationTreeStorage{&chain[0], 2}) == &values[1] &&
            last_shader_validation_tree_value(ShaderValidationTreeStorage{&chain[0], 0}) == nullptr,
        "last tree value follows first links only when the count is nonzero");

    std::array<ShaderValidationListNode, 3> list_nodes{};
    std::vector<int> destroyed;
    destroyed_list_nodes = &destroyed;
    destroyed_list_base = list_nodes.data();
    const ShaderValidationListVtable list_vtable{record_list_destroy};
    for (std::size_t index = 0; index < list_nodes.size(); ++index)
    {
        list_nodes[index].vtable = &list_vtable;
        list_nodes[index].previous = index > 0 ? &list_nodes[index - 1] : nullptr;
        list_nodes[index].next = index + 1 < list_nodes.size() ? &list_nodes[index + 1] : nullptr;
    }
    ShaderValidationList list{0, {&list_nodes[0], &list_nodes[2], 3}};
    destroy_shader_validation_list(list);
    require(list.vtable_address == 0x002037c8 && list.storage.count == 0 && list.storage.first == &list_nodes[1] && list.storage.last == nullptr &&
            destroyed == std::vector<int>{21, 11, 1},
        "list destruction deletes from the last node; unrelinked neighbours leave a stale first link");

    ShaderValidationChannelMask single{0, {9, 9, 9, 9}};
    require(construct_shader_validation_single_channel_mask(single, 2) == &single && single.vtable_address == 0x00203790 &&
            single.channels == std::array<u8, 4>{0, 0, 1, 0},
        "single-channel mask selects one channel");
    construct_shader_validation_single_channel_mask(single, 33);
    require(single.channels == std::array<u8, 4>{0, 1, 0, 0}, "single-channel shift uses the low five bits");
    construct_shader_validation_single_channel_mask(single, 4);
    require(single.channels == std::array<u8, 4>{0, 0, 0, 0}, "channel four selects nothing");

    ShaderValidationListNode previous{}, next{}, linked{};
    u32 payload = 0;
    require(construct_shader_validation_list_node(linked, &list_vtable, &payload, &previous, &next) == &linked &&
            linked.value == &payload && linked.previous == &previous && linked.next == &next && previous.next == &linked &&
            next.previous == &linked && linked.vtable == &list_vtable,
        "list node constructor links both neighbours");

    ShaderValidationBalancedTree objects{nullptr, 0};
    std::array<ShaderValidationBalancedNode, 2> object_nodes{};
    std::size_t used = 0;
    ShaderValidationInsertContext context{objects, [&](std::size_t, u32) -> void*
        {
            return &object_nodes[used++];
        }};
    ShaderValidationInstruction instruction{};
    require(construct_shader_validation_instruction_base(context, instruction, 0x00480000) == &instruction &&
            instruction.base.vtable_address == 0x00203810 && instruction.output_mask.channels == std::array<u8, 4>{1, 1, 1, 1} &&
            instruction.vector_mask.vtable_address == 0x00203790 && instruction.scalar_mask.channels == std::array<u8, 4>{1, 1, 1, 1} &&
            objects.count == 1 && objects.root == &object_nodes[0] && object_nodes[0].value == 0x00480000,
        "instruction base registers its address and sets full masks");
}
}

namespace
{
void check_shader_validation_registers()
{
    using namespace gbtg;
    // Rotations and ancestor heights on the pass-node layout.
    std::array<ShaderValidationPassNode, 4> nodes{};
    nodes[0].second = &nodes[1];
    nodes[1].parent = &nodes[0];
    nodes[1].first = &nodes[2];
    nodes[1].second = &nodes[3];
    nodes[2].parent = &nodes[1];
    nodes[3].parent = &nodes[1];
    nodes[2].height = 1;
    nodes[3].height = 1;
    nodes[1].height = 2;
    nodes[0].height = 3;
    rotate_shader_validation_pass_tree_left(nodes[0]);
    require(nodes[1].parent == nullptr && nodes[1].first == &nodes[0] && nodes[0].parent == &nodes[1] && nodes[0].second == &nodes[2] &&
            nodes[2].parent == &nodes[0] && nodes[0].height == 2 && nodes[1].height == 3,
        "left pass rotation promotes the second child and recomputes heights");
    rotate_shader_validation_pass_tree_right(nodes[1]);
    require(nodes[0].parent == nullptr && nodes[0].second == &nodes[1] && nodes[1].first == &nodes[2] && nodes[2].parent == &nodes[1] &&
            nodes[1].height == 2 && nodes[0].height == 3,
        "right pass rotation restores the original shape");
    nodes[2].height = 5;
    update_shader_validation_pass_ancestor_heights(&nodes[1]);
    require(nodes[1].height == 6 && nodes[0].height == 7, "ancestor heights propagate until a height is unchanged");

    // Register tables map kinds to the newest owner of each register.
    std::array<ShaderValidationOwner, 4> owners{};
    set_shader_validation_owner_derived(owners[0], &owners[1]);
    owners[0].channel_owner = &owners[2];
    std::array<ShaderValidationPassNode, 2> registers{};
    registers[0].value = 4;
    registers[0].data = &owners[0];
    registers[0].first = &registers[1];
    registers[1].value = 0;
    registers[1].data = &owners[3];
    ShaderValidationPassNode special{};
    special.value = 0x100;
    special.data = &owners[2];
    ShaderValidationPass tables{};
    tables.registers.tree.nodes.root = &registers[0];
    tables.special.tree.nodes.root = &special;
    require(find_shader_validation_register_owner(tables, 4) == &owners[0] && find_shader_validation_register_owner(tables, 12) == &owners[2] &&
            find_shader_validation_register_owner(tables, 7) == nullptr,
        "register lookup uses the special table only for kind twelve");
    require(find_shader_validation_owner_register_kind(tables, &owners[1]) == 4 && find_shader_validation_owner_register_kind(tables, &owners[2]) == 4 &&
            find_shader_validation_owner_register_kind(tables, &owners[3]) == 0,
        "owner kind search follows derived links, then channel-owner links");
    ShaderValidationOwner stray{};
    require(find_shader_validation_owner_register_kind(tables, &stray) == -1, "unregistered owners have kind -1");

    // Constant owners are equivalent when their source and foreign parent match.
    ShaderValidationOwner first{}, second{}, active{};
    first.kind = second.kind = 3;
    first.source = second.source = 9;
    require(shader_validation_owners_equivalent(&first, &first, &active) && !shader_validation_owners_equivalent(&first, nullptr, &active) &&
            shader_validation_owners_equivalent(&first, &second, &active),
        "identical constant owners without operands are equivalent");
    ShaderValidationOperand operand{};
    operand.parent = &active;
    second.operand = &operand;
    require(shader_validation_owners_equivalent(&first, &second, &active), "the active owner is not a foreign parent");
    operand.parent = &stray;
    require(!shader_validation_owners_equivalent(&first, &second, &active), "different foreign parents are not equivalent");
    second.kind = 2;
    operand.parent = nullptr;
    require(!shader_validation_owners_equivalent(&first, &second, &active), "non-constant owners are equivalent only by identity");

    // Channel helpers.
    ShaderValidationOwner present{};
    present.channels = {0, 5, 0, 0x80000000};
    ShaderValidationChannelMask mask{0, {9, 9, 9, 9}};
    require(shader_validation_owner_channel_mask(present, mask) == &mask && mask.vtable_address == 0x00203790 &&
            mask.channels == std::array<u8, 4>{0, 1, 0, 1},
        "owner channel mask marks nonzero channels");
    ShaderValidationOperand swizzled{};
    swizzled.channel_order.channels = {2, 2, 4, 0};
    shader_validation_operand_source_mask(swizzled, ShaderValidationChannelMask{0x00203790, {1, 0, 1, 1}}, mask);
    require(mask.channels == std::array<u8, 4>{1, 0, 1, 0}, "source mask maps written destinations through the operand order");
    ShaderValidationChannelOrder composed{};
    require(compose_shader_validation_channel_orders(ShaderValidationChannelOrder{0x00203794, {3, 4, 0, 0}},
                ShaderValidationChannelOrder{0x00203794, {7, 6, 5, 4}}, composed) == &composed &&
            composed.vtable_address == 0x00203794 && composed.channels == std::array<u32, 4>{4, 4, 7, 7},
        "order composition indexes the second order through the first");

    // List append, including a failed allocation.
    std::array<ShaderValidationListNode, 2> appended{};
    std::size_t used = 0;
    int calls = 0;
    const ShaderValidationListVtable vtable{nullptr};
    auto allocate = [&](std::size_t, u32) -> void*
    {
        return calls++ == 1 ? nullptr : &appended[used++];
    };
    ShaderValidationList list{0, {nullptr, nullptr, 0}};
    u32 values[3] = {};
    append_shader_validation_list_value(list, &vtable, allocate, &values[0]);
    append_shader_validation_list_value(list, &vtable, allocate, &values[1]);
    require(list.storage.first == &appended[0] && list.storage.last == nullptr && list.storage.count == 2,
        "failed append clears the last link but still counts");
    append_shader_validation_list_value(list, &vtable, allocate, &values[2]);
    require(list.storage.first == &appended[0] && list.storage.last == &appended[1] && list.storage.count == 3 &&
            appended[1].previous == nullptr && appended[1].value == &values[2] && appended[0].next == nullptr,
        "append after a failure links to the cleared last node");
}
}

namespace
{
void check_shader_validation_instruction_ops()
{
    using namespace gbtg;
    // A left-heavy chain is balanced by a right rotation and parents are revisited.
    std::array<ShaderValidationPassNode, 3> chain{};
    chain[0].first = &chain[1];
    chain[1].parent = &chain[0];
    chain[1].first = &chain[2];
    chain[2].parent = &chain[1];
    chain[2].height = 1;
    chain[1].height = 2;
    chain[0].height = 3;
    balance_shader_validation_pass_ancestors(chain[0]);
    require(chain[1].parent == nullptr && chain[1].first == &chain[2] && chain[1].second == &chain[0] && chain[0].first == nullptr &&
            chain[1].height == 2 && chain[0].height == 1,
        "pass balancing rotates a left-heavy chain");
    std::array<ShaderValidationPassNode, 3> zigzag{};
    zigzag[0].second = &zigzag[1];
    zigzag[1].parent = &zigzag[0];
    zigzag[1].first = &zigzag[2];
    zigzag[2].parent = &zigzag[1];
    zigzag[2].height = 1;
    zigzag[1].height = 2;
    zigzag[0].height = 3;
    balance_shader_validation_pass_ancestors(zigzag[0]);
    require(zigzag[2].parent == nullptr && zigzag[2].first == &zigzag[0] && zigzag[2].second == &zigzag[1] && zigzag[2].height == 2,
        "pass balancing double-rotates a right-left zigzag");

    std::array<ShaderValidationOwner, 3> owners{};
    owners[0].derived_owner = &owners[1];
    ShaderValidationPassNode registers{};
    registers.value = 1;
    registers.data = &owners[1];
    ShaderValidationPass tables{};
    tables.registers.tree.nodes.root = &registers;
    require(shader_validation_owner_chain_has_kind_one(tables, &owners[0]) && !shader_validation_owner_chain_has_kind_one(tables, &owners[2]),
        "kind-one search follows derived owners until a register is found");
    registers.value = 2;
    require(!shader_validation_owner_chain_has_kind_one(tables, &owners[0]), "other register kinds are not kind one");

    ShaderValidationChannelMask both{0, {}}, either{0, {}};
    const ShaderValidationChannelMask mask{0x00203790, {1, 0, 0x80, 0}};
    const ShaderValidationChannelMask other{0x00203790, {0xFF, 1, 0, 0}};
    require(intersect_shader_validation_channel_masks(mask, other, both) == &both && both.vtable_address == 0x00203790 &&
            both.channels == std::array<u8, 4>{1, 0, 0, 0} &&
            unite_shader_validation_channel_masks(mask, other, either) == &either && either.channels == std::array<u8, 4>{1, 1, 1, 0},
        "mask intersection and union normalize channels to one");

    ShaderValidationInstruction instruction{};
    instruction.vector_opcode = 4;
    require(shader_validation_instruction_uses_slot(instruction, 0) && shader_validation_instruction_uses_slot(instruction, 1) &&
            shader_validation_instruction_uses_slot(instruction, 2) && !shader_validation_instruction_uses_slot(instruction, 3),
        "MAD uses all three operand slots");
    instruction.vector_opcode = 0xD;
    require(shader_validation_instruction_uses_slot(instruction, 0) && !shader_validation_instruction_uses_slot(instruction, 1) &&
            !shader_validation_instruction_uses_slot(instruction, 2),
        "ARL uses only the first slot");

    std::array<ShaderValidationOperand, 3> operands{};
    ShaderValidationOwner destination{}, output{};
    instruction = ShaderValidationInstruction{};
    instruction.operands = {&operands[0], &operands[1], &operands[2]};
    instruction.scalar_opcode = 1;
    instruction.scalar_destination = &destination;
    instruction.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 1, 0}};
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 1, 1, 1}};
    operands[2].marker = 0;
    require(negate_shader_validation_move_source(instruction, &destination) && operands[2].marker == 1,
        "scalar move negation toggles the third operand");
    instruction.output_register = &output;
    instruction.output_flag = 1;
    require(!negate_shader_validation_move_source(instruction, &destination) && operands[2].marker == 1,
        "a scalar output register blocks scalar move negation");
    instruction.output_register = nullptr;
    require(convert_shader_validation_scalar_move(instruction) && instruction.vector_opcode == 1 && instruction.scalar_opcode == 0 &&
            instruction.vector_destination == &destination && instruction.scalar_destination == nullptr && instruction.output_flag == 0 &&
            instruction.operands[0] == &operands[2] && instruction.operands[2] == &operands[0] && operands[2].kind == 0 &&
            operands[0].kind == 2 && instruction.vector_mask.channels == std::array<u8, 4>{0, 0, 1, 0} &&
            instruction.scalar_mask.channels == std::array<u8, 4>{0, 0, 0, 0},
        "scalar move conversion moves masks, destination, flag and operands to the vector unit");
    require(!convert_shader_validation_scalar_move(instruction), "only scalar moves are converted");
    operands[2].marker = 0;
    require(negate_shader_validation_move_source(instruction, &destination) && operands[2].marker == 1,
        "vector move negation toggles the first operand");
}
}

namespace
{
void check_shader_validation_pass_tree_edits()
{
    using namespace gbtg;
    // Joining a taller second subtree hangs the first under its leftmost path.
    std::array<ShaderValidationPassNode, 4> nodes{};
    nodes[1].height = 1; // First subtree: a single node.
    nodes[2].second = &nodes[3];
    nodes[3].parent = &nodes[2];
    nodes[3].height = 1;
    nodes[2].height = 2;
    nodes[2].parent = &nodes[0];
    nodes[1].parent = &nodes[0];
    ShaderValidationPassNode* joined = join_shader_validation_pass_subtrees(&nodes[1], &nodes[2]);
    require(joined == &nodes[2] && nodes[2].parent == nullptr && nodes[2].first == &nodes[1] && nodes[1].parent == &nodes[2] &&
            nodes[2].height == 2,
        "join attaches the shorter first subtree to the taller second subtree");
    require(join_shader_validation_pass_subtrees(nullptr, nullptr) == nullptr, "joining two empty subtrees is empty");
    ShaderValidationPassNode lone{};
    lone.parent = &nodes[0];
    lone.height = 1;
    require(join_shader_validation_pass_subtrees(&lone, nullptr) == &lone && lone.parent == nullptr,
        "joining with an empty second subtree detaches the first");

    // Rebalancing after attaching a leaf rotates and republishes the root.
    std::array<ShaderValidationPassNode, 3> chain{};
    chain[0].second = &chain[1];
    chain[1].parent = &chain[0];
    chain[0].height = 2;
    chain[1].height = 1;
    chain[1].second = &chain[2];
    chain[2].parent = &chain[1];
    chain[2].height = 1;
    ShaderValidationPassTree tree{&chain[0], 3};
    rebalance_shader_validation_pass_tree(tree, chain[2]);
    require(tree.root == &chain[1] && chain[1].first == &chain[0] && chain[1].second == &chain[2] && chain[1].height == 2,
        "rebalance rotates a right chain and climbs to the new root");

    ShaderValidationInstruction instruction{};
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 1, 0, 0}};
    instruction.vector_opcode = 8; // DST reads y/z from the first slot and y/w from the second.
    require(!shader_validation_instruction_reads_channel(instruction, 0, 0) && shader_validation_instruction_reads_channel(instruction, 0, 1) &&
            !shader_validation_instruction_reads_channel(instruction, 0, 2) && shader_validation_instruction_reads_channel(instruction, 1, 1) &&
            !shader_validation_instruction_reads_channel(instruction, 1, 3),
        "DST reads only its fixed channels when they are written");
    instruction.vector_opcode = 5;
    require(shader_validation_instruction_reads_channel(instruction, 1, 2) && !shader_validation_instruction_reads_channel(instruction, 1, 3),
        "DP3 reads xyz of its first two slots");
    instruction.vector_opcode = 0;
    instruction.scalar_opcode = 7;
    instruction.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 1, 0}};
    require(shader_validation_instruction_reads_channel(instruction, 2, 3) && !shader_validation_instruction_reads_channel(instruction, 2, 2) &&
            !shader_validation_instruction_reads_channel(instruction, 0, 0),
        "scalar opcode seven reads other channels when z is written");
}
}

namespace
{
std::vector<int>* erased_pass_nodes = nullptr;

void check_shader_validation_instruction_reads()
{
    using namespace gbtg;
    ShaderValidationInstruction instruction{};
    std::array<ShaderValidationOperand, 3> operands{};
    std::array<ShaderValidationOwner, 3> parents{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        operands[index].instruction = &instruction;
        operands[index].kind = static_cast<u32>(index);
        instruction.operands[index] = &operands[index];
    }
    instruction.vector_opcode = 1; // MOV reads the written channels of slot zero.
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 1, 0}};
    require(shader_validation_operand_reads_channel(operands[0], 1) && !shader_validation_operand_reads_channel(operands[0], 0) &&
            !shader_validation_operand_reads_channel(operands[1], 1),
        "operand reads use the instruction slot");
    operands[1].kind = 3;
    require(shader_validation_operand_reads_channel(operands[1], 0) && !shader_validation_operand_reads_channel(operands[1], 1),
        "owner operands read only channel zero");
    ShaderValidationChannelMask read{0, {9, 9, 9, 9}};
    require(shader_validation_instruction_read_mask(instruction, read, 0) == &read && read.vtable_address == 0x00203790 &&
            read.channels == std::array<u8, 4>{0, 1, 1, 0},
        "read mask collects every read channel of a slot");
    require(!shader_validation_instruction_reads_unset_channel(instruction), "operands without owners read no unset channel");
    operands[0].parent = &parents[0];
    parents[0].channels = {shader_validation_unwritten_channel, 0, 0, shader_validation_unwritten_channel};
    require(!shader_validation_instruction_reads_unset_channel(instruction), "unread unset channels are ignored");
    parents[0].channels[2] = shader_validation_unwritten_channel;
    require(shader_validation_instruction_reads_unset_channel(instruction), "a read unset channel is reported");

    // Vector moves become scalar moves when a scalar writer exists along the destination chain.
    std::array<ShaderValidationOwner, 2> owners{};
    ShaderValidationInstruction writer{};
    owners[0].derived_owner = &owners[1];
    owners[1].channels[2] = reinterpret_cast<ShaderValidationChannelWriter>(&writer);
    instruction = ShaderValidationInstruction{};
    instruction.operands = {&operands[0], &operands[1], &operands[2]};
    instruction.vector_opcode = 1;
    instruction.vector_destination = &owners[0];
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    require(!convert_shader_validation_vector_move(instruction, 1) && instruction.vector_opcode == 1,
        "a vector move without a scalar writer is kept");
    writer.scalar_destination = &owners[1];
    require(convert_shader_validation_vector_move(instruction, 1) && instruction.scalar_opcode == 1 && instruction.vector_opcode == 0 &&
            instruction.scalar_destination == &owners[0] && instruction.vector_destination == nullptr && instruction.output_flag == 1 &&
            instruction.scalar_mask.channels == std::array<u8, 4>{1, 0, 0, 0} && instruction.vector_mask.channels == std::array<u8, 4>{0, 0, 0, 0} &&
            instruction.operands[0] == &operands[2] && instruction.operands[2] == &operands[0] && operands[2].kind == 0 && operands[0].kind == 2,
        "vector move conversion moves masks, destination and operands to the scalar unit");
    instruction = ShaderValidationInstruction{};
    instruction.operands = {&operands[0], &operands[1], &operands[2]};
    instruction.vector_opcode = 1;
    instruction.vector_destination = &owners[0];
    writer.scalar_destination = nullptr;
    require(convert_shader_validation_vector_move(instruction, 0), "the writer check is skipped when the phase byte is clear");

    // Erasing a pass node joins its children and returns the in-order successor.
    std::array<ShaderValidationPassNode, 3> nodes{};
    nodes[0].first = &nodes[1];
    nodes[0].second = &nodes[2];
    nodes[1].parent = &nodes[0];
    nodes[2].parent = &nodes[0];
    nodes[0].height = 2;
    nodes[1].height = 1;
    nodes[2].height = 1;
    ShaderValidationPassTree tree{&nodes[0], 3};
    std::vector<int> erased;
    erased_pass_nodes = &erased;
    ShaderValidationPassNode* base = nodes.data();
    auto destroy = [base](ShaderValidationPassNode& node, u32 flags)
    {
        erased_pass_nodes->push_back(static_cast<int>(&node - base) * 10 + static_cast<int>(flags));
    };
    require(erase_shader_validation_pass_node(tree, &nodes[0], destroy) == &nodes[2] && tree.root == &nodes[1] && tree.count == 2 &&
            nodes[1].second == &nodes[2] && nodes[2].parent == &nodes[1] && erased == std::vector<int>{1},
        "erasing the root joins its subtrees");
    require(erase_shader_validation_pass_node(tree, &nodes[2], destroy) == nullptr && nodes[1].second == nullptr && tree.count == 1 &&
            erased == std::vector<int>{1, 21},
        "erasing a leaf detaches it from its parent");
    require(erase_shader_validation_pass_node(tree, nullptr, destroy) == nullptr && tree.count == 1, "erasing null does nothing");
}
}

namespace
{
void check_shader_validation_channel_swaps()
{
    using namespace gbtg;
    ShaderValidationInstruction instruction{};
    std::array<ShaderValidationOperand, 3> operands{};
    ShaderValidationOwner destination{}, stray{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        operands[index].instruction = &instruction;
        operands[index].kind = static_cast<u32>(index);
        instruction.operands[index] = &operands[index];
    }
    instruction.vector_destination = &destination;
    instruction.vector_opcode = 3; // ADD reads slots zero and two.
    instruction.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0x80, 0}};
    operands[0].channel_order.channels = {0, 1, 0, 3};
    operands[1].channel_order.channels = {0, 1, 2, 3};
    operands[2].channel_order.channels = {2, 1, 2, 4};
    require(shader_validation_destination_channels_match(instruction, &destination, 0, 2) &&
            !shader_validation_destination_channels_match(instruction, &destination, 0, 1) &&
            !shader_validation_destination_channels_match(instruction, &stray, 0, 2),
        "ADD channels match when both read slots agree");
    require(shader_validation_destination_channels_match(instruction, &destination, 3, 4) == false &&
            shader_validation_destination_channels_match(instruction, &destination, 4, 4),
        "channel four compares as the missing channel");
    require(swap_shader_validation_destination_channels(instruction, &destination, 1, 2, 1) &&
            instruction.vector_mask.channels == std::array<u8, 4>{1, 0, 0x80, 0},
        "query-only swaps leave the instruction unchanged");
    require(swap_shader_validation_destination_channels(instruction, &destination, 1, 2, 0) &&
            instruction.vector_mask.channels == std::array<u8, 4>{1, 1, 0, 0} &&
            operands[0].channel_order.channels == std::array<u32, 4>{0, 0, 1, 3} &&
            operands[1].channel_order.channels == std::array<u32, 4>{0, 1, 2, 3} &&
            operands[2].channel_order.channels == std::array<u32, 4>{2, 2, 1, 4},
        "vector swaps toggle differing mask bytes and swap only the slots ADD uses");
    instruction.output_register = &destination;
    require(!swap_shader_validation_destination_channels(instruction, &destination, 0, 1, 1), "vector output registers block swaps");
    instruction.output_register = nullptr;
    instruction.vector_opcode = 8;
    require(!swap_shader_validation_destination_channels(instruction, &destination, 0, 1, 1) &&
            !shader_validation_destination_channels_match(instruction, &destination, 0, 0),
        "DST channels cannot be swapped");

    instruction.vector_destination = nullptr;
    instruction.vector_opcode = 0;
    instruction.scalar_destination = &destination;
    instruction.scalar_opcode = 3;
    instruction.scalar_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    require(swap_shader_validation_destination_channels(instruction, &destination, 0, 3, 0) &&
            instruction.scalar_mask.channels == std::array<u8, 4>{0, 0, 0, 1} &&
            operands[2].channel_order.channels == std::array<u32, 4>{2, 2, 1, 4},
        "scalar opcodes two to four swap only the scalar mask");
    instruction.scalar_opcode = 5;
    require(!swap_shader_validation_destination_channels(instruction, &destination, 0, 3, 1) &&
            !shader_validation_destination_channels_match(instruction, &destination, 0, 0),
        "scalar opcodes above four are not swappable");

    // A writer's result reaches a reader slot through the reader operand's owner.
    ShaderValidationInstruction reader{}, writer{};
    std::array<ShaderValidationOperand, 3> reads{};
    ShaderValidationOwner shared{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        reads[index].instruction = &reader;
        reads[index].kind = static_cast<u32>(index);
        reads[index].channel_order.channels = {0, 1, 2, 3};
        reader.operands[index] = &reads[index];
    }
    reader.vector_opcode = 1;
    reader.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    reads[0].parent = &shared;
    writer.vector_destination = &shared;
    writer.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    require(shader_validation_vector_result_reaches(reader, 0, writer) && !shader_validation_scalar_result_reaches(reader, 0, writer),
        "a vector write reaches the reading operand's owner");
    writer.vector_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 0}};
    require(!shader_validation_vector_result_reaches(reader, 0, writer), "unread written channels do not reach the reader");
    writer.scalar_destination = &shared;
    writer.scalar_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    require(shader_validation_scalar_result_reaches(reader, 0, writer), "a scalar write reaches through the scalar destination");
    require(shader_validation_operand_reads_order_channel(reads[0], 0) && !shader_validation_operand_reads_order_channel(reads[0], 1),
        "order-channel reads require a read destination channel");

    std::array<ShaderValidationPassNode, 2> nodes{};
    nodes[0].value = 7;
    nodes[0].first = &nodes[1];
    nodes[0].height = 2;
    nodes[1].value = 3;
    nodes[1].parent = &nodes[0];
    nodes[1].height = 1;
    ShaderValidationPassTree tree{&nodes[0], 2};
    int destroyed = 0;
    const auto count_destroyed = [&](ShaderValidationPassNode&, u32)
    {
        ++destroyed;
    };
    require(erase_shader_validation_pass_key(tree, 3, count_destroyed) == &nodes[0] &&
            tree.count == 1 && nodes[0].first == nullptr && destroyed == 1,
        "erasing by key removes the matching pass node");
    require(erase_shader_validation_pass_key(tree, 9, count_destroyed) == nullptr && destroyed == 1,
        "erasing a missing key does nothing");
}
}

namespace
{
void check_shader_validation_dependencies()
{
    using namespace gbtg;
    ShaderValidationInstruction reader{}, writer{};
    std::array<ShaderValidationOperand, 3> reads{};
    ShaderValidationOwner shared{}, active{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        reads[index].instruction = &reader;
        reads[index].kind = static_cast<u32>(index);
        reads[index].channel_order.channels = {0, 1, 2, 3};
        reader.operands[index] = &reads[index];
    }
    reader.vector_opcode = 1;
    reader.vector_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 0}};
    require(shader_validation_operand_reads_masked_channel(reads[0], ShaderValidationChannelMask{0x00203790, {0, 1, 0, 0}}) &&
            !shader_validation_operand_reads_masked_channel(reads[0], ShaderValidationChannelMask{0x00203790, {1, 0, 1, 1}}),
        "masked reads test only selected source channels");

    reads[0].parent = &shared;
    writer.vector_destination = &shared;
    writer.vector_opcode = 3;
    writer.vector_mask = ShaderValidationChannelMask{0x00203790, {0, 1, 0, 0}};
    require(shader_validation_instruction_depends_on(reader, writer, &active) && !shader_validation_instructions_can_pair(reader, writer),
        "a vector ADD result read by the next instruction prevents pairing");
    writer.vector_opcode = 1;
    require(shader_validation_instructions_can_pair(reader, writer), "a plain vector move result may still pair");
    writer.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    require(!shader_validation_instruction_depends_on(reader, writer, &active), "unread channels are not a dependency");
    ShaderValidationOperand parent_operand{};
    ShaderValidationOwner source{};
    shared.operand = &parent_operand;
    parent_operand.parent = &source;
    writer.vector_destination = &source;
    require(shader_validation_instruction_depends_on(reader, writer, &active), "a foreign parent of a read owner is a dependency");
    require(!shader_validation_instruction_depends_on(reader, writer, &source), "the active owner is not a foreign parent");

    // Channels share a writer when one instruction writes both and its sources agree.
    ShaderValidationInstruction both{};
    std::array<ShaderValidationOperand, 3> sources{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        sources[index].channel_order.channels = {0, 0, 2, 3};
        both.operands[index] = &sources[index];
    }
    ShaderValidationOwner written{};
    both.vector_destination = &written;
    both.vector_opcode = 1;
    written.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&both), reinterpret_cast<ShaderValidationChannelWriter>(&both),
        reinterpret_cast<ShaderValidationChannelWriter>(&both), shader_validation_unwritten_channel};
    require(shader_validation_owner_channels_share_writer(written, 0, 1) && !shader_validation_owner_channels_share_writer(written, 0, 2) &&
            !shader_validation_owner_channels_share_writer(written, 2, 3),
        "shared writers require identical sources and a real writer");

    // Diagnostic registries map retail operand addresses to operands.
    std::array<ShaderValidationBalancedNode, 2> nodes{};
    nodes[0].value = 1;
    nodes[0].first = &nodes[1];
    nodes[1].parent = &nodes[0];
    nodes[1].value = 2;
    ShaderValidationOwner registry{};
    registry.diagnostics.nodes.root = &nodes[0];
    const auto operand_at = [&](u32 value) -> const ShaderValidationOperand*
    {
        return &reads[value];
    };
    reader.scalar_opcode = 1;
    reader.scalar_mask = ShaderValidationChannelMask{0x00203790, {0, 0, 0, 1}};
    require(shader_validation_owner_diagnostic_reads_channel(registry, 3, operand_at) &&
            !shader_validation_owner_diagnostic_reads_channel(registry, 1, operand_at),
        "registry search visits each registered operand in order");
    registry.diagnostics.nodes.root = nullptr;
    require(!shader_validation_owner_diagnostic_reads_channel(registry, 3, operand_at), "an empty registry reads nothing");
}
}

namespace
{
void check_shader_validation_tree_instances()
{
    using namespace gbtg;
    // Inserting into the 0x002037EC tree allocates a node with that vtable and keeps it balanced.
    ShaderValidationBalancedTree tree{nullptr, 0};
    std::array<ShaderValidationBalancedNode, 4> storage{};
    std::size_t used = 0;
    ShaderValidationInsertContext insert{tree, [&](std::size_t, u32) -> void*
        {
            return &storage[used++];
        }};
    initialize_shader_validation_node_2037ec(storage[used++], 10, nullptr);
    tree.root = &storage[0];
    tree.count = 1;
    require(insert_shader_validation_node_2037ec(insert, 20, tree.root) == &storage[1] &&
            insert_shader_validation_node_2037ec(insert, 30, tree.root) == &storage[2] && tree.count == 3 && tree.root == &storage[1] &&
            storage[2].vtable_address == 0x002037ec,
        "0x002037EC insertion appends and rebalances");
    require(insert_shader_validation_node_2037ec(insert, 20, tree.root) == &storage[1] && tree.count == 3,
        "duplicate keys return the existing node");
    require(insert_shader_validation_node_2037f4(insert, 5, tree.root) == &storage[3] && storage[3].vtable_address == 0x002037f4,
        "0x002037F4 insertion installs its own vtable");

    int destroyed = 0;
    ShaderValidationEraseContext erase{tree, [&](ShaderValidationBalancedNode&, u32)
        {
            ++destroyed;
        }};
    unregister_shader_validation_value_1926fb(erase, 30);
    require(tree.count == 3 && destroyed == 1 && storage[1].second == nullptr, "unregistering removes the matching value");
    unregister_shader_validation_value_1927b9(erase, 99);
    require(tree.count == 3 && destroyed == 1, "unregistering a missing value is ignored");

    // Signed pass-tree insertion.
    ShaderValidationPassTree pass{nullptr, 0};
    std::array<ShaderValidationPassNode, 3> pass_nodes{};
    std::size_t pass_used = 1;
    const auto allocate = [&](std::size_t, u32) -> void*
    {
        return &pass_nodes[pass_used++];
    };
    u32 payload = 0;
    initialize_shader_validation_pass_node(pass_nodes[0], 0, &payload, nullptr);
    pass.root = &pass_nodes[0];
    pass.count = 1;
    require(insert_shader_validation_pass_node(pass, allocate, 0xFFFFFFFF, &payload, pass.root) == &pass_nodes[1] &&
            pass_nodes[0].first == &pass_nodes[1] && pass.count == 2 && pass_nodes[1].data == &payload,
        "pass insertion orders keys as signed integers");
    require(insert_shader_validation_pass_node(pass, allocate, 0, &payload, pass.root) == &pass_nodes[0] && pass.count == 2,
        "pass insertion returns an existing key");
}
}

namespace
{
void check_shader_validation_sets()
{
    using namespace gbtg;
    std::array<ShaderValidationOwner, 3> owners{};
    owners[0].derived_owner = &owners[1];
    owners[1].derived_owner = &owners[2];
    owners[2].channels[1] = 5;
    require(shader_validation_owner_derives_from(owners[0], &owners[2]) && !shader_validation_owner_derives_from(owners[2], &owners[0]) &&
            shader_validation_owner_derives_from(owners[1], &owners[1]),
        "derived chains are followed recursively");
    require(shader_validation_derived_owner_writes_channel(owners[0], 1) && !shader_validation_derived_owner_writes_channel(owners[0], 0) &&
            !shader_validation_channel_owner_writes_channel(owners[0], 1),
        "channel writes are searched along the chosen chain");

    // Instruction sets order by signed sequence, then by retail address.
    std::map<u32, ShaderValidationInstruction> instructions;
    instructions[0x1000].sequence = 2;
    instructions[0x2000].sequence = 0xFFFFFFFF;
    instructions[0x3000].sequence = 2;
    const ShaderValidationInstructionAt instruction_at = [&](u32 address) -> const ShaderValidationInstruction*
    {
        return &instructions.at(address);
    };
    require(shader_validation_instruction_precedes(0x2000, 0x1000, instruction_at) &&
            shader_validation_instruction_precedes(0x1000, 0x3000, instruction_at) &&
            !shader_validation_instruction_precedes(0x1000, 0x1000, instruction_at),
        "instruction order uses signed sequences and address ties");
    ShaderValidationBalancedTree set{nullptr, 0};
    std::array<ShaderValidationBalancedNode, 3> nodes{};
    std::size_t used = 0;
    ShaderValidationInsertContext context{set, [&](std::size_t, u32) -> void*
        {
            return &nodes[used++];
        }};
    register_shader_validation_instruction(context, 0x1000, instruction_at);
    register_shader_validation_instruction(context, 0x3000, instruction_at);
    register_shader_validation_instruction(context, 0x2000, instruction_at);
    register_shader_validation_instruction(context, 0x1000, instruction_at);
    require(set.count == 3 && set.root == &nodes[0] && nodes[0].first == &nodes[2] && nodes[0].second == &nodes[1] &&
            nodes[2].vtable_address == 0x002037f0 && find_shader_validation_instruction_node(set, 0x3000, instruction_at) == &nodes[1],
        "instruction registration builds an ordered, duplicate-free set");

    // Tree object destruction releases every node, then optionally the tree itself.
    std::array<ShaderValidationPassNode, 2> pass_nodes{};
    pass_nodes[0].first = &pass_nodes[1];
    pass_nodes[1].parent = &pass_nodes[0];
    ShaderValidationPassTreeObject pass{0, 0, {&pass_nodes[0], 2}};
    std::vector<int> events;
    delete_shader_validation_pass_tree(pass, 1, [&](ShaderValidationPassNode& node, u32)
        {
            events.push_back(static_cast<int>(&node - pass_nodes.data()));
        }, [&](void*, u32 tag)
        {
            events.push_back(tag == 0x24870000 ? -1 : -2);
        });
    require(pass.vtable_address == 0x002037d8 && pass.nodes.root == nullptr && pass.nodes.count == 0 && events == std::vector<int>{1, 0, -1},
        "pass tree deletion destroys children first and frees the tree for flag one");
}
}

namespace
{
void check_shader_validation_owner_swaps()
{
    using namespace gbtg;
    // 0x00192830 updates an existing key's data or registers a new node.
    std::array<ShaderValidationPassNode, 2> pass_nodes{};
    std::size_t pass_used = 0;
    const auto allocate_pass = [&](std::size_t, u32) -> void*
    {
        return &pass_nodes[pass_used++];
    };
    ShaderValidationPassTree tree{nullptr, 0};
    u32 first_data = 0, second_data = 0;
    ShaderValidationPassNode* result = nullptr;
    require(set_shader_validation_pass_value(tree, allocate_pass, &result, 4, &first_data) == &result && result == &pass_nodes[0] &&
            tree.count == 1,
        "setting a missing key registers it");
    set_shader_validation_pass_value(tree, allocate_pass, &result, 4, &second_data);
    require(result == &pass_nodes[0] && pass_nodes[0].data == &second_data && tree.count == 1, "setting an existing key replaces its data");

    // Swapping owner channels swaps writer channels, writer masks and registered operand orders.
    ShaderValidationInstruction writer{};
    std::array<ShaderValidationOperand, 3> sources{};
    ShaderValidationOwner owner{};
    for (std::size_t index = 0; index < 3; ++index)
    {
        sources[index].instruction = &writer;
        sources[index].kind = static_cast<u32>(index);
        sources[index].channel_order.channels = {0, 1, 2, 3};
        writer.operands[index] = &sources[index];
    }
    writer.vector_opcode = 1;
    writer.vector_destination = &owner;
    writer.vector_mask = ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}};
    owner.channels[0] = reinterpret_cast<ShaderValidationChannelWriter>(&writer);
    ShaderValidationOperand reader{};
    reader.channel_order.channels = {0, 1, 0, 4};
    ShaderValidationBalancedNode registered{};
    registered.value = 7;
    owner.diagnostics.nodes.root = &registered;
    const ShaderValidationOperandAt operand_at = [&](u32) -> ShaderValidationOperand*
    {
        return &reader;
    };
    require(swap_shader_validation_owner_channels(owner, 0, 1, 0, 0, operand_at) && owner.channels[0] == 0 &&
            owner.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&writer) &&
            writer.vector_mask.channels == std::array<u8, 4>{0, 1, 0, 0} && sources[0].channel_order.channels == std::array<u32, 4>{1, 0, 2, 3} &&
            reader.channel_order.channels == std::array<u32, 4>{1, 0, 1, 4},
        "a single writer channel moves to the other channel");
    owner.channels[2] = shader_validation_unwritten_channel;
    require(!swap_shader_validation_owner_channels(owner, 1, 2, 0, 0, operand_at), "unwritten channels cannot be swapped");

    // 0x00190774 requires every masked writer to be in the instruction set.
    std::map<u32, ShaderValidationInstruction*> by_address{{0x100, &writer}};
    const ShaderValidationInstructionAt instruction_at = [&](u32 address) -> const ShaderValidationInstruction*
    {
        return by_address.at(address);
    };
    const ShaderValidationAddressOf address_of = [](const void*) -> u32
    {
        return 0x100;
    };
    ShaderValidationBalancedTree set{nullptr, 0};
    ShaderValidationChannelMask mask{0x00203790, {0, 1, 0, 0}};
    require(!shader_validation_owner_writers_in_set(owner, 0, mask, set, nullptr, address_of, instruction_at),
        "a missing writer fails the set test");
    ShaderValidationBalancedNode member{};
    initialize_shader_validation_node_2037f0(member, 0x100, nullptr);
    set.root = &member;
    set.count = 1;
    require(shader_validation_owner_writers_in_set(owner, 0, mask, set, nullptr, address_of, instruction_at) &&
            shader_validation_owner_writers_in_set(owner, 0, ShaderValidationChannelMask{0x00203790, {1, 0, 1, 0}}, set, nullptr, address_of,
                instruction_at),
        "set members, empty and unwritten channels pass the set test");

    // 0x00190813 applies the same test to an operand's parent through the channels it sources.
    ShaderValidationOperand source{};
    const ShaderValidationBalancedTree empty{nullptr, 0};
    require(shader_validation_operand_writers_in_set(source, 0, mask, empty, nullptr, address_of, instruction_at),
        "an operand without a parent passes the set test");
    source.parent = &owner;
    source.kind = 3;
    source.channel_order.channels = {1, 1, 1, 1};
    require(shader_validation_operand_writers_in_set(source, 0, ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}}, set, nullptr, address_of,
                instruction_at) &&
            !shader_validation_operand_writers_in_set(source, 0, ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}}, empty, nullptr, address_of,
                instruction_at),
        "an operand's parent writers must be in the set");
}
}

void check_shader_validation_serializer()
{
    using namespace gbtg;
    check_shader_validation_accessors();
    check_shader_validation_channel_ops();
    check_shader_validation_containers();
    check_shader_validation_registers();
    check_shader_validation_instruction_ops();
    check_shader_validation_pass_tree_edits();
    check_shader_validation_instruction_reads();
    check_shader_validation_channel_swaps();
    check_shader_validation_dependencies();
    check_shader_validation_tree_instances();
    check_shader_validation_sets();
    check_shader_validation_owner_swaps();
    {
        ShaderValidationOwner owner{};
        ShaderValidationOperand operand{};
        ShaderValidationOwner active{};
        ShaderValidationOwner other{};
        require(find_shader_validation_foreign_parent(owner, &active) == nullptr, "missing owner operand has no foreign parent");
        owner.operand = &operand;
        operand.parent = &active;
        require(find_shader_validation_foreign_parent(owner, &active) == nullptr, "active parent is not foreign");
        operand.parent = &other;
        require(find_shader_validation_foreign_parent(owner, &active) == &other, "other parent is returned");
        operand.parent = nullptr;
        require(find_shader_validation_foreign_parent(owner, nullptr) == nullptr, "null parent equals null active owner");

        ShaderValidationInstruction instruction{};
        require(select_shader_validation_destination_index(instruction) == 0xF, "missing scalar destination selects fifteen");
        other.source = 0x123;
        instruction.scalar_destination = &other;
        require(select_shader_validation_destination_index(instruction) == 0x123, "opcode zero selects scalar destination unmasked");
        instruction.vector_opcode = 1;
        require(select_shader_validation_destination_index(instruction) == 0xF, "vector opcode ignores scalar destination");
        instruction.vector_destination = &active;
        active.source = 4;
        require(select_shader_validation_destination_index(instruction) == 4, "vector opcode selects vector destination");

        ShaderValidationChannelOrder destination{0x11111111, {1, 2, 3, 4}};
        const ShaderValidationChannelOrder source{0x22222222, {5, 6, 7, 8}};
        require(assign_shader_validation_channel_order(destination, source) == &destination &&
                destination.vtable_address == 0x11111111 && destination.channels == std::array<u32, 4>{5, 6, 7, 8},
            "channel order assignment retains destination vtable");
        ShaderValidationOperand target{};
        target.channel_order.vtable_address = 0x33333333;
        ShaderValidationChannelOrder result{0, {9, 9, 9, 9}};
        require(replace_shader_validation_operand_channel_order(result, target, source) == &result &&
                target.channel_order.vtable_address == 0x33333333 &&
                target.channel_order.channels == std::array<u32, 4>{5, 6, 7, 8} &&
                result.vtable_address == 0x00203794 && result.channels == target.channel_order.channels,
            "operand channel replacement returns a fresh order copy");
    }
    {
        SerializerFixture fixture;
        ShaderValidationSerializerFrame frame{0xC0000001, 0xC0000000, 0xFFFFFFFF};
        u32 output[3] = {};
        require(serialize_shader_validation_instruction(fixture.instructions[0], output, frame, &fixture.owners[3]) == output,
            "serializer returns caller output");
        require(output[0] == 0xE075AD2F && output[1] == 0xF003CE48 && output[2] == 0x7B4AF0A5,
            "ARL serialization retains frame bits and foreign constant flag");
        require(frame.word0 == output[0] && frame.word1 == output[1] && frame.word2 == output[2],
            "serializer leaves packed words in its retained frame");
        serialize_shader_validation_instruction(fixture.instructions[1], output, frame, &fixture.owners[3]);
        require(output[0] == 0xFFF00003 && output[1] == 0xDFF050A5 && output[2] == 0xA14A0559,
            "second serialization consumes the retained frame and default destination");
        require(fixture.channels_normalized(), "serialization fills missing operand channels in place");
    }
    {
        SerializerFixture fixture;
        fixture.operands[4].parent = nullptr; // This operand's parent is the active owner, now null.
        std::array<ShaderValidationListNode, 2> nodes{};
        nodes[0].value = &fixture.instructions[1];
        nodes[0].next = &nodes[1];
        nodes[1].value = &fixture.instructions[0];
        ShaderValidationSerializerFrame frame{0, 0x40000000, 0x5A5A5A5A};
        std::vector<u32> program(10, 0xCCCCCCCC);
        require(serialize_shader_validation_program(&nodes[0], program.data(), frame, nullptr),
            "program serializer returns true");
        const std::vector<u32> expected{2, 0x3FB00002, 0x6FF050A5, 0xA14A0550, 0x2075AD2E, 0x7003CE48, 0x7B4AF0A5, 0xCCCCCCCC, 0xCCCCCCCC, 0xCCCCCCCC};
        require(program == expected, "program serializer counts, packs and resolves the reserved register");
        require(fixture.channels_normalized(), "program serialization normalizes each operand");
        std::vector<u32> empty(2, 0xCCCCCCCC);
        require(serialize_shader_validation_program(nullptr, empty.data(), frame, nullptr) &&
                empty == std::vector<u32>{0, 0xCCCCCCCC},
            "empty program writes only its count");
    }
}
