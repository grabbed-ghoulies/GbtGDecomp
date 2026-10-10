#include "../src/engine/graphics/shader_validation_pass_lifecycle.h"
#include "../src/engine/graphics/shader_validation_owner_sets.h"
#include "../src/engine/graphics/shader_validation_destinations.h"
#include <array>
#include <functional>
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

std::vector<int>* destroyed_list_nodes = nullptr;

void record_list_node(gbtg::ShaderValidationListNode&, gbtg::u32 flags)
{
    destroyed_list_nodes->push_back(static_cast<int>(flags));
}
}

void check_shader_validation_lifecycle()
{
    using namespace gbtg;
    {
        alignas(16) std::array<std::array<unsigned char, 256>, 128> split_pool{};
        std::size_t split_used = 0;
        const auto split_allocate = [&](std::size_t, u32) -> void*
        {
            return split_pool[split_used++].data();
        };
        ShaderValidationBalancedTree split_objects{nullptr, 0};
        ShaderValidationInsertContext split_insert{split_objects, split_allocate};
        ShaderValidationEraseContext split_erase{split_objects, [](ShaderValidationBalancedNode&, u32)
            {
            }};
        RuntimeCppExceptionContext split_exceptions{retail_cpp_exception_template, nullptr};
        ShaderValidationOwner split_active{};
        ShaderValidationOwner* split_active_owner = &split_active;
        std::map<ShaderValidationOwner*, ShaderValidationInsertContext> split_registries;
        ShaderValidationConstructorContext split_context{split_insert, split_erase, split_exceptions, split_active_owner, split_allocate,
            [](void*, u32)
            {
            },
            [&](const void* pointer) -> u32
            {
                if (pointer == &split_active)
                {
                    return 0x10009000;
                }
                return 0x10040000 + static_cast<u32>((static_cast<const unsigned char*>(pointer) - split_pool[0].data()) / 256) * 0x80;
            },
            [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
            {
                return split_registries.try_emplace(&owner, ShaderValidationInsertContext{owner.diagnostics.nodes, split_allocate}).first->second;
            },
            [](ShaderValidationBalancedNode&, u32)
            {
            }};
        auto* source = create_shader_validation_instruction(split_context);
        source->scalar_opcode = 7;
        const std::size_t before = split_used;
        require(split_shader_validation_scalar_instruction(split_context, *source) == nullptr && split_used == before && source->scalar_opcode == 7,
            "a scalar-only instruction does not split or allocate");
        source->vector_opcode = 3;
        source->sequence = 91;
        source->operands[2]->marker = 255;
        source->operands[2]->channel_order.channels = {3, 1, 4, 0};
        source->output_flag = 2;
        source->output_mask.channels = {0, 255, 0, 2};
        source->scalar_mask.channels = {1, 0, 0, 0};
        ShaderValidationOwner output{}, scalar{};
        source->output_register = &output;
        source->scalar_destination = &scalar;
        output.channels[1] = output.channels[3] = reinterpret_cast<ShaderValidationChannelWriter>(source);
        scalar.channels[0] = reinterpret_cast<ShaderValidationChannelWriter>(source);
        auto* separated = split_shader_validation_scalar_instruction(split_context, *source);
        require(separated->vector_opcode == 0 && separated->scalar_opcode == 7 && source->scalar_opcode == 0 &&
                source->vector_opcode == 3 && separated->sequence == 91 && separated->instruction_set_count == 0xFFFFFFFF,
            "splitting moves only the scalar opcode and copies the sequence");
        require(separated->operands[2]->parent == &split_active && source->operands[2]->parent == &split_active &&
                separated->operands[2]->marker == 255 && separated->operands[2]->channel_order.channels == std::array<u32, 4>{3, 1, 4, 0},
            "add retains the original scalar parent while copying order and marker");
        require(separated->output_flag == 2 && separated->output_register == &output && separated->scalar_destination == &scalar &&
                source->output_register == nullptr && source->scalar_destination == nullptr &&
                separated->output_mask.channels == std::array<u8, 4>{0, 255, 0, 2} &&
                output.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(separated) &&
                scalar.channels[0] == reinterpret_cast<ShaderValidationChannelWriter>(separated),
            "noncanonical output flags transfer destinations and writer ownership");
        source->vector_opcode = 1;
        source->scalar_opcode = 3;
        source->output_flag = 0;
        auto* detached = split_shader_validation_scalar_instruction(split_context, *source);
        require(detached->operands[2]->parent == &split_active && source->operands[2]->parent != &split_active &&
                source->operands[2]->parent->kind == 0 && source->operands[2]->parent->operand->kind == 3 &&
                detached->operands[0]->parent->kind == 0 && detached->operands[1]->parent->kind == 0,
            "other vector opcodes receive a fresh scalar parent and both new unused operands own fresh parents");
        ShaderValidationProgramPass split_pass{};
        const auto split_instruction_at = [&](u32 value) -> ShaderValidationInstruction*
        {
            return reinterpret_cast<ShaderValidationInstruction*>(split_pool[(value - 0x10040000) / 0x80].data());
        };
        const std::size_t before_empty = split_used;
        split_shader_validation_program_scalar_instructions(split_context, split_pass, split_instruction_at);
        require(split_used == before_empty && split_pass.instruction_set.nodes.count == 0,
            "an empty program split traversal does not allocate");
        ShaderValidationInsertContext split_pass_insert{split_pass.instruction_set.nodes, split_allocate};
        for (auto* original : {source, separated, detached})
        {
            register_shader_validation_instruction(split_pass_insert, split_context.retail_address(original), split_instruction_at);
        }
        source->scalar_opcode = 3;
        split_and_register_shader_validation_scalar_instruction(split_context, split_pass, *source, split_instruction_at);
        require(source->scalar_opcode == 0 && split_pass.instruction_set.nodes.count == 4,
            "the single-instruction wrapper registers its new scalar instruction");
        detached->vector_opcode = 4;
        split_shader_validation_program_scalar_instructions(split_context, split_pass, split_instruction_at);
        require(detached->scalar_opcode == 0 && split_pass.instruction_set.nodes.count == 5,
            "the program traversal visits existing instructions while inserting scalar instructions into the same tree");
        const std::size_t after_pass = split_used;
        split_shader_validation_program_scalar_instructions(split_context, split_pass, split_instruction_at);
        require(split_used == after_pass && split_pass.instruction_set.nodes.count == 5,
            "a second traversal does not split scalar-only or vector-only instructions");
        auto* dependency_source = create_shader_validation_instruction(split_context);
        auto* dependency_reader = create_shader_validation_instruction(split_context);
        ShaderValidationOwner dependency_owner{}, dependency_ancestor{};
        ShaderValidationBalancedNode dependency_reader_node{};
        dependency_reader_node.value = split_context.retail_address(dependency_reader->operands[0]);
        dependency_owner.diagnostics.nodes = {&dependency_reader_node, 1};
        dependency_owner.channel_owner = &dependency_ancestor;
        dependency_ancestor.channels[1] = reinterpret_cast<ShaderValidationChannelWriter>(dependency_reader);
        dependency_source->output_register = &dependency_owner;
        dependency_source->output_mask.channels = {1, 1, 0, 0};
        dependency_reader->vector_opcode = 1;
        dependency_reader->vector_mask.channels = {1, 0, 0, 0};
        ShaderValidationBalancedTree dependency_registry{nullptr, 0};
        ShaderValidationInsertContext dependency_insert{dependency_registry, split_allocate};
        ShaderValidationSetContext dependency_sets{split_allocate, split_context.deleting_destructor, split_instruction_at,
            [&](u32 value) -> ShaderValidationOperand*
            {
                return reinterpret_cast<ShaderValidationOperand*>(split_pool[(value - 0x10040000) / 0x80].data());
            }, split_context.retail_address};
        ShaderValidationDependencyContext dependency_context{split_context, dependency_insert, dependency_sets, 0};
        build_shader_validation_instruction_dependencies(dependency_context, *dependency_source);
        require(dependency_source->instruction_set_count == 1 && dependency_registry.count == 2 &&
                shader_validation_instruction_has_dependency(dependency_context, *dependency_source, split_context.retail_address(dependency_reader)),
            "a diagnostic reader and ancestor writer register the same dependency only once");
        const std::size_t after_dependencies = split_used;
        build_shader_validation_instruction_dependencies(dependency_context, *dependency_source);
        require(split_used == after_dependencies && dependency_source->instruction_set_count == 1,
            "dependency construction reuses the cached instruction set");
        require(shader_validation_instruction_priority_precedes(dependency_context, split_context.retail_address(dependency_source),
                split_context.retail_address(dependency_reader)),
            "instructions with more dependencies sort before instructions with fewer dependencies");
        ShaderValidationPass dependency_pass{};
        ShaderValidationBalancedTree dependency_readers{nullptr, 0};
        ShaderValidationOwner register_owner{};
        ShaderValidationChannelMask full;
        construct_shader_validation_channel_mask(full, 0xF);
        require(assign_shader_validation_owner_register(dependency_context, dependency_pass, 7, register_owner,
                dependency_readers, dependency_source, full, 1) && dependency_pass.registers.tree.nodes.count == 0 && register_owner.kind == 0,
            "a register assignment query leaves the pass and owner unchanged");
        require(assign_shader_validation_owner_register(dependency_context, dependency_pass, 7, register_owner,
                dependency_readers, dependency_source, full, 0) && dependency_pass.registers.tree.nodes.count == 1 &&
                register_owner.kind == 1 && register_owner.source == 7,
            "register assignment installs an available register kind");
        require(!assign_shader_validation_owner_register(dependency_context, dependency_pass, 8, register_owner,
                dependency_readers, dependency_source, full, 0),
            "an already registered owner cannot claim another register kind");
    }
    alignas(16) std::array<std::array<unsigned char, 256>, 64> pool{};
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

    // 0x00193A40 restarts a mutable diagnostic registry and refuses split-mask readers.
    {
        ShaderValidationOwner source{};
        ShaderValidationOwner derived{};
        source.kind = 1;
        derived.kind = 1;
        source.derived_owner = &derived;
        source.channels[0] = shader_validation_unwritten_channel;
        std::array<ShaderValidationOperand, 3> readers{};
        for (u32 index = 0; index < readers.size(); ++index)
        {
            readers[index].kind = 3;
            readers[index].parent = &source;
            readers[index].channel_order.channels = {index == 1 ? 0u : 1u, 4, 4, 4};
        }
        ShaderValidationInsertContext source_registry{source.diagnostics.nodes, allocate};
        ShaderValidationInsertContext derived_registry{derived.diagnostics.nodes, allocate};
        for (u32 index = 0; index < readers.size(); ++index)
        {
            register_shader_validation_diagnostic(source_registry, 10 + index);
        }
        auto moving_context = context;
        moving_context.retail_address = [&](const void* pointer) -> u32
        {
            for (u32 index = 0; index < readers.size(); ++index)
            {
                if (pointer == &readers[index])
                {
                    return 10 + index;
                }
            }
            throw std::runtime_error("unexpected reader reparent address");
        };
        moving_context.diagnostic_registry = [&](ShaderValidationOwner& parent) -> ShaderValidationInsertContext&
        {
            require(&parent == &derived, "unwritten readers register on the derived owner");
            return derived_registry;
        };
        std::vector<u32> erased;
        moving_context.deleting_destructor = [&](ShaderValidationBalancedNode& node, u32 flags)
        {
            require(flags == 1, "reader move uses the deleting node destructor");
            erased.push_back(node.value);
        };
        const std::function<ShaderValidationOperand*(u32)> reader_at = [&](u32 address) -> ShaderValidationOperand*
        {
            return &readers.at(address - 10);
        };
        move_shader_validation_unwritten_readers_to_derived(moving_context, source, reader_at);
        require(readers[0].parent == &derived && readers[1].parent == &source && readers[2].parent == &derived &&
                source.diagnostics.nodes.count == 1 && derived.diagnostics.nodes.count == 2 && erased == std::vector<u32>{10, 12},
            "reader migration restarts after each erase and retains readers of written channels");

        moving_context.diagnostic_registry = [&](ShaderValidationOwner& parent) -> ShaderValidationInsertContext&
        {
            require(&parent == &derived || &parent == &source, "reparenting uses an actual owner registry");
            return &parent == &source ? source_registry : derived_registry;
        };
        reparent_shader_validation_operand(moving_context, readers[0], &source);
        erased.clear();

        ShaderValidationInstruction mixed_instruction{};
        mixed_instruction.vector_opcode = 1;
        mixed_instruction.vector_mask.channels = {1, 1, 0, 0};
        readers[1].kind = 0;
        readers[1].instruction = &mixed_instruction;
        readers[1].channel_order.channels = {0, 1, 4, 4};
        const auto before = used;
        move_shader_validation_unwritten_readers_to_derived(moving_context, source, reader_at);
        require(readers[0].parent == &source && readers[1].parent == &source && used == before &&
                source.diagnostics.nodes.count == 2 && erased.empty(),
            "a reader spanning written and unwritten channels rejects the migration before any move");
        source.derived_owner = nullptr;
        move_shader_validation_unwritten_readers_to_derived(moving_context, source, reader_at);
        require(readers[0].parent == &source && used == before && erased.empty(),
            "an owner without a derived destination performs no reader migration");
    }

    // Writer replacement clears the first matching destination before assigning the slot.
    {
        ShaderValidationOwner destination{};
        ShaderValidationInstruction previous{};
        ShaderValidationInstruction replacement{};
        previous.output_register = &destination;
        previous.vector_destination = &destination;
        previous.output_mask.channels = {2, 0, 0, 0};
        previous.vector_mask.channels = {1, 0, 0, 0};
        destination.channels[0] = reinterpret_cast<ShaderValidationChannelWriter>(&previous);
        set_shader_validation_channel_writer(destination, 0, reinterpret_cast<ShaderValidationChannelWriter>(&replacement));
        require(previous.output_register == nullptr && previous.output_mask.channels == std::array<u8, 4>{} &&
                previous.vector_destination == &destination && previous.vector_mask.channels[0] == 1 &&
                destination.channels[0] == reinterpret_cast<ShaderValidationChannelWriter>(&replacement),
            "writer removal uses output/vector/scalar precedence when destinations alias");

        ShaderValidationOwner next{};
        replacement.scalar_destination = &destination;
        replacement.scalar_mask.channels = {1, 0, 0, 0};
        const ShaderValidationChannelMask requested{0xDEADBEEF, {0, 2, 0, 255}};
        require(replace_shader_validation_scalar_destination(replacement, &next, requested) == &next &&
                replacement.scalar_destination == &next && replacement.scalar_mask.channels == requested.channels &&
                destination.channels[0] == 0 && next.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&replacement) &&
                next.channels[3] == reinterpret_cast<ShaderValidationChannelWriter>(&replacement),
            "destination replacement clears old writers and installs every nonzero requested channel");
        set_shader_validation_scalar_destination(replacement, nullptr);
        require(replacement.scalar_destination == nullptr && replacement.scalar_mask.channels == std::array<u8, 4>{} &&
                next.channels[1] == 0 && next.channels[3] == 0,
            "null destination replacement clears the mask and the previous owner writers");

        ShaderValidationOwner ancestor{};
        ShaderValidationOperand reader{};
        ShaderValidationBalancedNode diagnostic{};
        ShaderValidationInstruction duplicate{};
        duplicate.vector_destination = &destination;
        duplicate.vector_opcode = 0; // The retail predicate accepts both channels for opcode zero.
        duplicate.vector_mask.channels = {1, 1, 0, 0};
        destination.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&duplicate),
            reinterpret_cast<ShaderValidationChannelWriter>(&duplicate), 0, 0};
        ancestor.channels[0] = shader_validation_unwritten_channel;
        destination.channel_owner = &ancestor;
        diagnostic.value = 7;
        destination.diagnostics.nodes = {&diagnostic, 1};
        reader.channel_order.channels = {0, 1, 0, 4};
        require(merge_shader_validation_duplicate_channel(destination, 0, 1, [&](u32 address) -> ShaderValidationOperand*
                {
                    require(address == 7, "duplicate merge resolves its registered reader");
                    return &reader;
                }) && destination.channels[0] == 0 && destination.channels[1] != 0 &&
                reader.channel_order.channels == std::array<u32, 4>{1, 1, 1, 4} && ancestor.channels[0] == shader_validation_unwritten_channel,
            "ancestor occupancy reverses the surviving channel and stops reader propagation at the occupied ancestor");
    }

    // 0x0019373D/0x0019379C preserve changed-byte semantics and process all operands.
    {
        ShaderValidationOperand kind_three{};
        kind_three.kind = 3;
        kind_three.parent = active_owner;
        kind_three.channel_order.channels = {0, 4, 4, 4};
        require(clear_unused_shader_validation_operand_channels(context, kind_three) &&
                kind_three.channel_order.channels == std::array<u32, 4>{0, 4, 4, 4},
            "already missing unread channels still report a cleanup change");

        ShaderValidationInstruction unused{};
        std::array<ShaderValidationOperand, 3> slots{};
        for (u32 slot = 0; slot < 3; ++slot)
        {
            unused.operands[slot] = &slots[slot];
            slots[slot].instruction = &unused;
            slots[slot].kind = slot;
            slots[slot].parent = nullptr;
            slots[slot].channel_order.channels = {0, 1, 2, 3};
        }
        require(clear_unused_shader_validation_instruction_channels(context, unused),
            "unused instruction cleanup reports changed operands");
        for (const auto& slot : slots)
        {
            require(slot.channel_order.channels == std::array<u32, 4>{4, 4, 4, 4} && slot.parent == active_owner,
                "cleanup visits all three operands and detaches their empty orders to the active owner");
        }
    }

    // 0x001935E6 creates a constant owner, optionally parented to the pass owner.
    ShaderValidationPass pass{};
    pass.owner = active_owner;
    ShaderValidationOwner* owner = create_shader_validation_constant_owner(context, pass, 9, 1);
    require(owner != nullptr && owner->kind == 3 && owner->source == 9 && owner->operand != nullptr && owner->operand->kind == 3 &&
            owner->operand->owner == owner && owner->operand->parent == active_owner && objects.count == 6,
        "constant owners are registered with a kind-three operand");

    // 0x00193566 caches input owners by the exact source key, including failed allocations.
    {
        ShaderValidationPass inputs{};
        auto* input = get_shader_validation_input_owner(context, inputs, 0xFFFFFFFF);
        require(input != nullptr && input->kind == 2 && input->source == 0xFFFFFFFF &&
                input->operand->parent == active_owner && input->channels == std::array<ShaderValidationChannelWriter, 4>{},
            "input owners have kind two, empty writers and the active parent");
        const auto before = used;
        require(get_shader_validation_input_owner(context, inputs, 0xFFFFFFFF) == input && used == before,
            "cached input lookup performs no allocation");
        const auto saved_allocate = context.allocate;
        bool fail_first = true;
        context.allocate = [&](std::size_t size, u32 flags) -> void*
        {
            if (fail_first)
            {
                fail_first = false;
                return nullptr;
            }
            return saved_allocate(size, flags);
        };
        require(get_shader_validation_input_owner(context, inputs, 0) == nullptr && inputs.values.tree.nodes.count == 2,
            "failed input allocation still inserts a null table value");
        context.allocate = saved_allocate;
        const auto after_failure = used;
        require(get_shader_validation_input_owner(context, inputs, 0) == nullptr && used == after_failure,
            "cached null input values do not retry allocation");
    }

    // 0x00193DF4 chooses the appropriate source argument and register table.
    {
        ShaderValidationPass selection{};
        selection.owner = active_owner;
        const ShaderValidationChannelMask mask{0xDEADBEEF, {0xFF, 2, 0, 8}};
        const auto before = used;
        require(select_shader_validation_owner(context, selection, 0xFFFFFFFF, 17, 29, 4, 1, mask) == nullptr && used == before,
            "unknown owner kinds do not allocate or select a value");
        auto* input = select_shader_validation_owner(context, selection, 2, 17, 29, 4, 0, mask);
        require(input != nullptr && input->kind == 2 && input->source == 17, "input selection uses the input-source argument");
        const auto after_input = used;
        require(select_shader_validation_owner(context, selection, 2, 17, 99, 12, 0xFF, mask) == input && used == after_input,
            "selected inputs reuse their cached owner independently of the other arguments");
        auto* constant = select_shader_validation_owner(context, selection, 3, 17, 29, 4, 0xFF, mask);
        require(constant != nullptr && constant->kind == 3 && constant->source == 29 && constant->operand->parent == active_owner,
            "constant selection uses its separate source argument and parent flag");
        ShaderValidationPassNode regular{};
        regular.value = 4;
        regular.data = input;
        selection.registers.tree.nodes = {&regular, 1};
        ShaderValidationPassNode special{};
        special.value = 0x100;
        special.data = constant;
        selection.special.tree.nodes = {&special, 1};
        const auto after_constant = used;
        require(select_shader_validation_owner(context, selection, 1, 0, 0, 4, 0, mask) == input &&
                select_shader_validation_owner(context, selection, 1, 0, 0, 12, 0, mask) == constant && used == after_constant,
            "register selection uses the special table for kind twelve and ignores mask contents");
        require(select_shader_validation_owner(context, selection, 1, 0, 0, 13, 0, mask) == nullptr,
            "absent register values are returned as null");
    }

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

    // 0x00193649 clears the derived instruction set before invoking the full base destructor.
    {
        ShaderValidationProgramPass program{};
        ShaderValidationBalancedNode root{};
        ShaderValidationBalancedNode child{};
        root.first = &child;
        child.parent = &root;
        program.instruction_set.nodes = {&root, 2};
        program.instruction_set.retained = 0xA5A5A5A5;
        int destroyed = 0;
        destroy_shader_validation_program_pass(destroy, program, [&](ShaderValidationBalancedNode& node, u32 flags)
            {
                require(program.base.base.vtable_address == 0x00203820 && program.instruction_set.vtable_address == 0x002037D0,
                    "derived destructor installs its own vtables before deleting nodes");
                require(node.parent == nullptr && flags == 1, "derived set deletes detached nodes with flag one");
                ++destroyed;
            });
        require(destroyed == 2 && program.instruction_set.nodes.root == nullptr && program.instruction_set.nodes.count == 0 &&
                program.instruction_set.retained == 0xA5A5A5A5 && program.base.base.vtable_address == 0x00203808,
            "derived destructor clears its set, preserves the retained word and completes base destruction");
        int freed = 0;
        const std::function<void(void*, u32)> release_pass = [&](void* allocation, u32 flags)
        {
            require(allocation == &program && flags == 0x24870000 && program.base.base.vtable_address == 0x00203808,
                "deleting destructor releases the pass only after base destruction");
            ++freed;
        };
        const std::function<void(ShaderValidationBalancedNode&, u32)> delete_node = [](ShaderValidationBalancedNode&, u32)
        {
        };
        require(delete_shader_validation_program_pass(destroy, &program, 2, delete_node, release_pass) == &program && freed == 0,
            "deleting destructor returns its pointer without freeing when bit zero is clear");
        require(delete_shader_validation_program_pass(destroy, &program, 0xFF, delete_node, release_pass) == &program && freed == 1,
            "deleting destructor frees once when bit zero is set");
    }

    // 0x00191417 keeps successful early relocations when a later writer cannot move.
    // 0x001911A8 rolls back the current pair but does not undo an earlier pair.
    {
        ShaderValidationOwner target{};
        target.kind = 1;
        std::array<ShaderValidationInstruction, 2> writers{};
        for (auto& writer : writers)
        {
            writer.vector_opcode = 5; // DP3 swaps the mask without reading its operands.
            writer.vector_destination = &target;
        }
        writers[0].vector_mask.channels = {1, 0, 0, 0};
        writers[1].vector_mask.channels = {0, 1, 0, 0};
        target.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]),
            reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]), 0, 0};
        writers[1].output_register = &target; // Rejects a vector swap with output_flag == 0.
        ShaderValidationSetContext sets{};
        require(!move_shader_validation_masked_channels(target, ShaderValidationChannelMask{0x00203790, {2, 0xFF, 0, 0}}, 0, 0, sets) &&
                target.channels[0] == 0 && target.channels[2] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]) &&
                writers[0].vector_mask.channels == std::array<u8, 4>{0, 0, 1, 0} &&
                target.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]),
            "later relocation failure retains an earlier successful move");
        writers[1].output_register = nullptr;
        require(move_shader_validation_masked_channels(target, ShaderValidationChannelMask{0x00203790, {1, 1, 0, 0}}, 0, 0, sets) &&
                target.channels[1] == 0 && target.channels[3] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]),
            "relocation searches destinations in ascending channel order");
        target.channels[0] = shader_validation_unwritten_channel;
        require(!move_shader_validation_masked_channels(target, ShaderValidationChannelMask{0x00203790, {1, 0, 0, 0}}, 0, 0, sets),
            "unwritten markers count as occupied but cannot be moved");

        target.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]),
            reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]), 0, 0};
        writers[0].vector_mask.channels = {1, 0, 0, 0};
        writers[1].vector_mask.channels = {0, 1, 0, 0};
        writers[0].output_register = &target;
        const ShaderValidationChannelOrder swap{0x00203794, {1, 0, 2, 3}};
        require(!reorder_shader_validation_owner_channels(target, swap, sets) &&
                writers[1].vector_mask.channels == std::array<u8, 4>{0, 1, 0, 0} &&
                target.channels[0] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]),
            "reorder failure undoes the second writer when the first writer refuses");
        writers[0].output_register = nullptr;
        require(reorder_shader_validation_owner_channels(target, swap, sets) &&
                target.channels[0] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[1]) &&
                target.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&writers[0]) &&
                writers[0].vector_mask.channels == std::array<u8, 4>{0, 1, 0, 0},
            "owner reordering exchanges writers and their masks");
        target.kind = 5;
        require(!reorder_shader_validation_owner_channels(target, swap, sets), "only owner kinds one and six can be reordered");
        ShaderValidationOwner empty{};
        empty.kind = 6;
        require(reorder_shader_validation_owner_channels(empty, ShaderValidationChannelOrder{0x00203794, {4, 4, 4, 4}}, sets),
            "reordering fills missing positions before processing swaps");
    }

    // 0x00191DF3 relocates before linking, even in query mode or when later link checks reject.
    {
        ShaderValidationOwner first{};
        ShaderValidationOwner second{};
        ShaderValidationOwner tail{};
        ShaderValidationOwner other_tail{};
        ShaderValidationInstruction writer{};
        writer.vector_opcode = 5;
        writer.vector_destination = &first;
        writer.vector_mask.channels = {1, 0, 0, 0};
        first.channels[0] = reinterpret_cast<ShaderValidationChannelWriter>(&writer);
        ShaderValidationSetContext sets{};
        const ShaderValidationChannelMask empty{0x00203790, {}};
        const ShaderValidationChannelMask first_channel{0x00203790, {0xFF, 0, 0, 0}};
        require(link_shader_validation_owners(nullptr, nullptr, 0, first_channel, first_channel, sets),
            "a null other owner returns before dereferencing the first owner");
        require(link_shader_validation_owners(&first, &second, 2, first_channel, empty, sets) && first.channels[0] == 0 &&
                first.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&writer) && first.derived_owner == nullptr &&
                second.channel_owner == nullptr,
            "query mode relocates channels while leaving the owner links untouched");
        second.channel_owner = &tail;
        require(link_shader_validation_owners(&first, &second, 0, empty, empty, sets) && first.derived_owner == &second &&
                first.channel_owner == &tail && second.channel_owner == &first,
            "committing a link inherits the old tail and places the new owner in the chain");
        first.derived_owner = nullptr;
        first.channel_owner = &tail;
        second.channel_owner = &other_tail;
        first.channels = {reinterpret_cast<ShaderValidationChannelWriter>(&writer), 0, 0, 0};
        writer.vector_mask.channels = {1, 0, 0, 0};
        require(!link_shader_validation_owners(&first, &second, 0, first_channel, empty, sets) && first.channels[0] == 0 &&
                first.channels[1] == reinterpret_cast<ShaderValidationChannelWriter>(&writer) && first.channel_owner == &tail &&
                second.channel_owner == &other_tail,
            "incompatible tails reject the link after retaining successful relocation");
    }

    // 0x00191EC3 and 0x00193E52 replace the stored owner even when linking refuses.
    {
        ShaderValidationSetContext sets{};
        ShaderValidationPass installation{};
        require(set_shader_validation_pass_owner(installation, nullptr, sets) == nullptr && installation.owner == nullptr,
            "installing null into an empty pass preserves the retail null-other shortcut");
        ShaderValidationOwner replacement{};
        ShaderValidationOwner previous{};
        ShaderValidationOwner existing_derived{};
        replacement.derived_owner = &existing_derived;
        installation.owner = &previous;
        require(set_shader_validation_pass_owner(installation, &replacement, sets) == &replacement && installation.owner == &replacement &&
                replacement.derived_owner == &existing_derived && previous.channel_owner == nullptr,
            "pass owner installation ignores a refused link");
        replacement.source = 5;
        ShaderValidationPassNode old_node{};
        old_node.value = 5;
        old_node.data = &previous;
        installation.registers.tree.nodes = {&old_node, 1};
        std::array<ShaderValidationPassNode, 4> allocated_nodes{};
        std::size_t allocated = 0;
        sets.allocate = [&](std::size_t, u32) -> void*
        {
            return &allocated_nodes[allocated++];
        };
        std::vector<u32> freed_keys;
        const std::function<void(ShaderValidationPassNode&, u32)> delete_node = [&](ShaderValidationPassNode& node, u32 flags)
        {
            require(flags == 1, "register replacement deletes the old node with flag one");
            freed_keys.push_back(node.value);
        };
        require(replace_shader_validation_register_owner(installation, replacement, delete_node, sets) == &replacement &&
                installation.registers.tree.nodes.count == 1 && installation.registers.tree.nodes.root->data == &replacement &&
                freed_keys == std::vector<u32>{5} && replacement.derived_owner == &existing_derived,
            "register owner replacement ignores a refused link and replaces its table node");
        sets.allocate = [](std::size_t, u32) -> void*
        {
            return nullptr;
        };
        require(replace_shader_validation_register_owner(installation, replacement, delete_node, sets) == &replacement &&
                installation.registers.tree.nodes.root == nullptr && installation.registers.tree.nodes.count == 1,
            "failed root allocation retains the retail count of one and still returns the owner");
        sets.allocate = [&](std::size_t, u32) -> void*
        {
            return &allocated_nodes[allocated++];
        };
        ShaderValidationOwner special_owner{};
        ShaderValidationPassNode special{};
        special.value = 0x100;
        special.data = &special_owner;
        installation.special.tree.nodes = {&special, 1};
        old_node = {};
        old_node.value = 12;
        old_node.data = &previous;
        installation.registers.tree.nodes = {&old_node, 1};
        replacement.source = 12;
        replacement.derived_owner = nullptr;
        require(replace_shader_validation_register_owner(installation, replacement, delete_node, sets) == &replacement &&
                replacement.derived_owner == &special_owner && installation.registers.tree.nodes.root->value == 12 &&
                installation.registers.tree.nodes.root->data == &replacement && installation.special.tree.nodes.root == &special &&
                special.data == &special_owner,
            "source twelve looks up the special owner but replaces only the regular register table");
    }

    // 0x00191BE1 and 0x0018F513 stop at a reader, a kind-four root, or an occupied intervening channel.
    {
        ShaderValidationOwner owner{};
        ShaderValidationOwner root{};
        root.kind = 4;
        owner.channel_owner = &root;
        const std::function<const ShaderValidationOperand*(u32)> operand_at{};
        require(shader_validation_channel_owner_has_reader(root, 3, operand_at) &&
                shader_validation_owner_channel_has_reader(owner, 3, operand_at),
            "kind-four roots count as readers through empty channels");
        root.channels[3] = shader_validation_unwritten_channel;
        require(!shader_validation_owner_channel_has_reader(owner, 3, operand_at),
            "an occupied channel prevents climbing to the reader root");
        root.channels[3] = 0;
        root.kind = 1;
        require(!shader_validation_owner_channel_has_reader(owner, 3, operand_at),
            "an ordinary empty root has no reader");
        ShaderValidationOperand reader{};
        reader.kind = 3;
        ShaderValidationBalancedNode diagnostic{};
        diagnostic.value = 0x3000;
        owner.diagnostics.nodes = {&diagnostic, 1};
        const std::function<const ShaderValidationOperand*(u32)> direct_reader = [&](u32) -> const ShaderValidationOperand*
        {
            return &reader;
        };
        require(shader_validation_owner_channel_has_reader(owner, 0, direct_reader) &&
                !shader_validation_channel_owner_has_reader(owner, 0, direct_reader),
            "the combined reader query checks this owner's own registry before climbing");
    }

    // 0x00191D2D checks derived readers and real writers, plus the pass owner for kind five.
    {
        ShaderValidationOwner target{};
        ShaderValidationOwner derived{};
        ShaderValidationOwner pass_owner{};
        target.derived_owner = &derived;
        ShaderValidationPass pass{};
        pass.owner = &pass_owner;
        ShaderValidationInstruction writer{};
        writer.sequence = 3;
        ShaderValidationOperand reader{};
        reader.kind = 3;
        ShaderValidationBalancedNode diagnostic{};
        diagnostic.value = 0x3000;
        ShaderValidationBalancedTree readers{nullptr, 0};
        ShaderValidationBalancedTree writers{nullptr, 0};
        ShaderValidationBalancedNode writer_node{};
        ShaderValidationSetContext sets{[&](std::size_t, u32) -> void*
            {
                return &writer_node;
            }, {}, [&](u32) -> const ShaderValidationInstruction*
            {
                return &writer;
            }, [&](u32) -> ShaderValidationOperand*
            {
                return &reader;
            }, [](const void*) -> u32
            {
                return 0x2000;
            }};
        const ShaderValidationChannelMask mask{0x00203790, {0xFF, 0, 0, 0}};
        derived.channels[0] = shader_validation_unwritten_channel;
        require(shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "unwritten markers are exempt from writer-set membership");
        derived.channels[0] = reinterpret_cast<ShaderValidationChannelWriter>(&writer);
        require(!shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "real derived writers must belong to the instruction set");
        ShaderValidationInsertContext writer_insert{writers, sets.allocate};
        register_shader_validation_instruction(writer_insert, 0x2000, sets.instruction_at);
        require(shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "registered derived writers satisfy the writer gate");
        derived.diagnostics.nodes = {&diagnostic, 1};
        require(!shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "unregistered derived readers fail before writer membership");
        readers = {&diagnostic, 1};
        require(shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "registered readers and writers satisfy derived dependencies");
        derived.diagnostics.nodes = {nullptr, 0};
        readers = {nullptr, 0};
        pass_owner.diagnostics.nodes = {&diagnostic, 1};
        target.kind = 5;
        require(!shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "kind five also checks the pass owner's readers");
        target.kind = 1;
        require(shader_validation_owner_dependencies_registered(target, pass, writers, readers, nullptr, mask, sets),
            "other kinds do not check pass-owner readers");
    }

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

    // 0x00190F87 reports channels read outside the supplied registry, climbing both owner links.
    {
        ShaderValidationOwner target{};
        ShaderValidationOwner derived{};
        ShaderValidationOwner channel_owner{};
        target.derived_owner = &derived;
        target.channel_owner = &channel_owner;
        ShaderValidationOperand reader{};
        reader.kind = 3; // Reads channel zero without an instruction.
        ShaderValidationBalancedNode diagnostic{};
        diagnostic.value = 0x3000;
        derived.diagnostics.nodes = {&diagnostic, 1};
        ShaderValidationSetContext sets{{}, {}, {}, [&](u32) -> ShaderValidationOperand*
            {
                return &reader;
            }, {}};
        ShaderValidationBalancedTree registry{nullptr, 0};
        ShaderValidationChannelMask output{0xDEADBEEF, {9, 9, 9, 9}};
        require(shader_validation_unregistered_reader_channels(target, output, registry, nullptr, sets) == &output &&
                output.vtable_address == 0x00203790 && output.channels == std::array<u8, 4>{1, 0, 0, 0},
            "reader mask reports the missing derived reader and initializes every output byte");
        target.channels[0] = shader_validation_unwritten_channel;
        shader_validation_unregistered_reader_channels(target, output, registry, nullptr, sets);
        require(output.channels == std::array<u8, 4>{}, "a local writer masks the derived reader");
        target.channels[0] = 0;
        derived.diagnostics.nodes = {nullptr, 0};
        channel_owner.diagnostics.nodes = {&diagnostic, 1};
        shader_validation_unregistered_reader_channels(target, output, registry, nullptr, sets);
        require(output.channels == std::array<u8, 4>{1, 0, 0, 0}, "reader mask also climbs channel owners");
        registry = {&diagnostic, 1};
        shader_validation_unregistered_reader_channels(target, output, registry, nullptr, sets);
        require(output.channels == std::array<u8, 4>{}, "registered readers clear the missing-channel mask");
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
