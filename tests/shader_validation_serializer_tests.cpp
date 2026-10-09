#include "../src/engine/graphics/shader_validation_channel_ops.h"
#include <array>
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

void check_shader_validation_serializer()
{
    using namespace gbtg;
    check_shader_validation_accessors();
    check_shader_validation_channel_ops();
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
