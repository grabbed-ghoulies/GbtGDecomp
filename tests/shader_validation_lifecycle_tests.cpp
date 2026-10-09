#include "../src/engine/graphics/shader_validation_pass_lifecycle.h"
#include "../src/engine/graphics/shader_validation_owner_sets.h"
#include <array>
#include <functional>
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

std::vector<int>* destroyed_list_nodes = nullptr;

void record_list_node(gbtg::ShaderValidationListNode&, gbtg::u32 flags)
{
    destroyed_list_nodes->push_back(static_cast<int>(flags));
}
}

void check_shader_validation_lifecycle()
{
    using namespace gbtg;
    alignas(16) std::array<std::array<unsigned char, 256>, 32> pool{};
    std::size_t used = 0;
    const auto allocate = [&](std::size_t, u32) -> void*
    {
        return pool[used++].data();
    };
    ShaderValidationBalancedTree objects{nullptr, 0};
    ShaderValidationInsertContext insert{objects, allocate};
    ShaderValidationEraseContext erase{objects, [](ShaderValidationBalancedNode&, u32)
        {
        }};
    RuntimeCppExceptionContext exceptions{retail_cpp_exception_template, nullptr};
    ShaderValidationOwner active{};
    ShaderValidationOwner* active_owner = &active;
    ShaderValidationBalancedTree diagnostics_tree{nullptr, 0};
    ShaderValidationInsertContext diagnostics{diagnostics_tree, allocate};
    int released = 0;
    ShaderValidationConstructorContext context{insert, erase, exceptions, active_owner, allocate, [&](void*, u32)
        {
            ++released;
        },
        [&](const void* pointer) -> u32
        {
            return 0x10040000 + static_cast<u32>((static_cast<const unsigned char*>(pointer) - pool[0].data()) / 256) * 0x80;
        },
        [&](ShaderValidationOwner&) -> ShaderValidationInsertContext&
        {
            return diagnostics;
        },
        [](ShaderValidationBalancedNode&, u32)
        {
        }};

    // Emulator-verified shape of 0x00192DF1: one instruction and three operands, all registered.
    ShaderValidationInstruction* instruction = create_shader_validation_instruction(context);
    require(instruction != nullptr && instruction->base.vtable_address == 0x00203810 && objects.count == 4 && released == 0,
        "instruction construction registers the instruction and three operands");
    for (u32 slot = 0; slot < 3; ++slot)
    {
        const ShaderValidationOperand* operand = instruction->operands[slot];
        require(operand != nullptr && operand->base.vtable_address == 0x0020380c && operand->instruction == instruction &&
                operand->kind == slot && operand->parent == active_owner && operand->owner == nullptr,
            "each operand belongs to its slot");
    }
    require(instruction->vector_mask.channels == std::array<u8, 4>{0, 0, 0, 0} && instruction->output_flag == 0 &&
            instruction->instruction_set == nullptr && instruction->vector_destination == nullptr,
        "instruction fields start empty");

    // 0x001935E6 creates a constant owner, optionally parented to the pass owner.
    ShaderValidationPass pass{};
    pass.owner = active_owner;
    ShaderValidationOwner* owner = create_shader_validation_constant_owner(context, pass, 9, 1);
    require(owner != nullptr && owner->kind == 3 && owner->source == 9 && owner->operand != nullptr && owner->operand->kind == 3 &&
            owner->operand->owner == owner && owner->operand->parent == active_owner && objects.count == 6,
        "constant owners are registered with a kind-three operand");

    // 0x001934A5 empties the pass trees and list and unregisters the pass and its sub-objects.
    std::vector<int> list_events;
    destroyed_list_nodes = &list_events;
    static const ShaderValidationListVtable list_vtable{record_list_node};
    std::array<ShaderValidationListNode, 2> list_nodes{};
    list_nodes[0].vtable = &list_vtable;
    list_nodes[1].vtable = &list_vtable;
    list_nodes[0].next = &list_nodes[1];
    list_nodes[1].previous = &list_nodes[0];
    pass.instructions = ShaderValidationList{0x002037c8, {&list_nodes[0], &list_nodes[1], 2}};
    ShaderValidationPassNode register_node{};
    pass.registers.tree = ShaderValidationPassTreeObject{0x002037d8, 0, {&register_node, 1}};
    ShaderValidationBalancedTree expressions{nullptr, 0};
    ShaderValidationInsertContext expression_insert{expressions, allocate};
    register_shader_validation_object(expression_insert, 0x2038);
    register_shader_validation_object(insert, 0x2000);
    ShaderValidationEraseContext expression_erase{expressions, [](ShaderValidationBalancedNode&, u32)
        {
        }};
    int pass_nodes_destroyed = 0;
    ShaderValidationPassDestroyContext destroy{expression_erase, erase, [&](ShaderValidationPassNode&, u32)
        {
            ++pass_nodes_destroyed;
        },
        [&](const void* pointer) -> u32
        {
            return pointer == &pass ? 0x2000u : pointer == &pass.registers.registered ? 0x2038u : 0x9999u;
        }};
    const u32 objects_before = objects.count;
    destroy_shader_validation_pass(destroy, pass);
    require(pass.base.vtable_address == 0x00203808 && pass.registers.tree.nodes.root == nullptr && pass.registers.tree.nodes.count == 0 &&
            pass.instructions.storage.count == 0 && list_events == std::vector<int>{1, 1} && pass_nodes_destroyed == 1 &&
            expressions.count == 0 && objects.count == objects_before - 1,
        "pass destruction clears its containers and unregisters its addresses");

    // 0x00192B84 negates every move writing an unlinked owner, or undoes them all.
    {
        ShaderValidationOwner target{};
        target.kind = 1;
        std::array<ShaderValidationInstruction, 2> writers{};
        std::array<ShaderValidationOperand, 6> sources{};
        for (std::size_t index = 0; index < 2; ++index)
        {
            writers[index].sequence = static_cast<u32>(index);
            for (std::size_t slot = 0; slot < 3; ++slot)
            {
                writers[index].operands[slot] = &sources[index * 3 + slot];
            }
            writers[index].vector_opcode = 1;
            writers[index].vector_destination = &target;
        }
        target.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]), reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]),
            0, 0};
        ShaderValidationOperand reader{};
        ShaderValidationBalancedNode registered{};
        registered.value = 0x3000;
        target.diagnostics.nodes.root = &registered;
        std::array<ShaderValidationBalancedNode, 4> set_nodes{};
        std::size_t set_used = 0;
        int freed = 0;
        ShaderValidationSetContext sets{[&](std::size_t, u32) -> void*
            {
                return &set_nodes[set_used++];
            },
            [&](ShaderValidationBalancedNode&, u32)
            {
                ++freed;
            },
            [&](u32 address) -> const ShaderValidationInstruction*
            {
                return &writers[address - 0x2000];
            },
            [&](u32) -> ShaderValidationOperand*
            {
                return &reader;
            },
            [&](const void* object) -> u32
            {
                return object == &writers[1] ? 0x2001u : 0x2000u;
            }};
        require(negate_shader_validation_owner_writers(target, sets) && sources[0].marker == 1 && sources[3].marker == 1 && reader.marker == 1 &&
                freed == 2,
            "owner writer negation toggles each move source and the registered readers");
        writers[1].output_register = &target;
        require(!negate_shader_validation_owner_writers(target, sets) && sources[0].marker == 1 && sources[3].marker == 1 && reader.marker == 1,
            "a blocked writer undoes earlier negations");
        target.kind = 2;
        require(!negate_shader_validation_owner_writers(target, sets), "input and constant owners are rejected");
    }

    // 0x00194311 compares parents, markers, then channel orders; 0x00193C2A never accepts.
    {
        ShaderValidationOwner parent{};
        ShaderValidationInstruction instruction{};
        ShaderValidationOperand operand{};
        ShaderValidationOperand other{};
        operand.parent = &parent;
        other.parent = &parent;
        operand.instruction = &instruction;
        other.instruction = &instruction;
        operand.channel_order.channels = {0, 1, 2, 3};
        other.channel_order.channels = {0, 4, 2, 4};
        require(shader_validation_operands_equivalent(operand, other, nullptr), "unused channels match any order");
        require(!shader_validation_instruction_accepts_order(instruction, 0, ShaderValidationChannelOrder{0x00203794, {0, 1, 2, 3}}),
            "instruction order acceptance is always false");
        other.channel_order.channels = {0, 2, 2, 3};
        require(!shader_validation_operands_equivalent(operand, other, nullptr), "a mismatched channel is not equivalent");
        other.channel_order.channels = {0, 1, 2, 3};
        other.marker = 1;
        require(!shader_validation_operands_equivalent(operand, other, nullptr), "marker mismatch is not equivalent");
        other.marker = 0;
        ShaderValidationOwner other_parent{};
        other.parent = &other_parent;
        require(!shader_validation_operands_equivalent(operand, other, nullptr), "distinct non-register parents are not equivalent");
    }

    // 0x00193EE2 drops a register kind once its owners' readers are registered, or hands it to the first unready channel owner.
    {
        ShaderValidationOwner first{};
        ShaderValidationOwner second{};
        first.kind = 1;
        second.kind = 1;
        first.channel_owner = &second;
        second.channels = {shader_validation_unwritten_channel, shader_validation_unwritten_channel, shader_validation_unwritten_channel,
            shader_validation_unwritten_channel};
        ShaderValidationOperand reader{};
        reader.kind = 3; // Kind 3 reads channel 0 without an instruction.
        ShaderValidationBalancedNode diagnostic{};
        diagnostic.value = 0x3000;
        second.diagnostics.nodes.root = &diagnostic;
        std::array<ShaderValidationBalancedNode, 8> set_nodes{};
        std::size_t set_used = 0;
        ShaderValidationSetContext sets{[&](std::size_t, u32) -> void*
            {
                return &set_nodes[set_used++];
            },
            [](ShaderValidationBalancedNode&, u32)
            {
            },
            [](u32) -> const ShaderValidationInstruction*
            {
                return nullptr;
            },
            [&](u32) -> ShaderValidationOperand*
            {
                return &reader;
            },
            [](const void*) -> u32
            {
                return 0;
            }};
        std::array<ShaderValidationPassNode, 4> pass_nodes{};
        std::size_t pass_used = 0;
        const std::function<void*(std::size_t, u32)> allocate_pass_node = [&](std::size_t, u32) -> void*
        {
            return &pass_nodes[pass_used++];
        };
        std::vector<u32> freed;
        const std::function<void(ShaderValidationPassNode&, u32)> destroy_pass_node = [&](ShaderValidationPassNode& node, u32 flags)
        {
            freed.push_back(node.value * 10 + flags);
        };
        ShaderValidationPass registers_pass{};
        construct_shader_validation_registered_pass_tree(registers_pass.registers);
        require(registers_pass.registers.tree.vtable_address == 0x00203800 && registers_pass.registers.registered.vtable_address == 0x002037fc,
            "registered pass tree constructor stores the derived vtables");
        ShaderValidationPassNode* inserted;
        set_shader_validation_pass_value(registers_pass.registers.tree.nodes, allocate_pass_node, &inserted, 3, &first);
        ShaderValidationBalancedTree empty{nullptr, 0};
        require(retire_shader_validation_register(registers_pass, 13, empty, destroy_pass_node, allocate_pass_node, sets) &&
                retire_shader_validation_register(registers_pass, 4, empty, destroy_pass_node, allocate_pass_node, sets) &&
                registers_pass.registers.tree.nodes.count == 1,
            "out-of-range and absent register kinds retire trivially");
        require(!retire_shader_validation_register(registers_pass, 3, empty, destroy_pass_node, allocate_pass_node, sets) &&
                freed == std::vector<u32>{31} && registers_pass.registers.tree.nodes.count == 1 &&
                registers_pass.registers.tree.nodes.root->data == &second && second.kind == 1 && second.source == 3,
            "an unready channel owner takes over the register kind");
        ShaderValidationBalancedNode registered{};
        registered.value = 0x3000;
        const ShaderValidationBalancedTree registry{&registered, 1};
        require(retire_shader_validation_register(registers_pass, 3, registry, destroy_pass_node, allocate_pass_node, sets) &&
                registers_pass.registers.tree.nodes.count == 0 && freed == std::vector<u32>{31, 31},
            "registered readers retire the register kind");
        set_shader_validation_pass_value(registers_pass.registers.tree.nodes, allocate_pass_node, &inserted, 5, &second);
        require(!retire_shader_validation_register(registers_pass, 5, empty, destroy_pass_node, allocate_pass_node, sets) &&
                registers_pass.registers.tree.nodes.count == 1 && freed.size() == 2,
            "unregistered readers keep the register kind");
    }
}
