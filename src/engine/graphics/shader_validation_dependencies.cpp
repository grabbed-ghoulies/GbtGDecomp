#include "shader_validation_dependencies.h"
#include "gbtg/host_seh.h"
#include "shader_validation_cleanup.h"
#include "shader_validation_pass_lifecycle.h"
#include "shader_validation_destinations.h"
#include <bit>

namespace gbtg
{
// Xbox 0x00196DD1
bool shader_validation_instruction_ready(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, const ShaderValidationInstruction& instruction,
    const ShaderValidationBalancedTree& writers, const ShaderValidationBalancedTree& readers)
{
    for (u32 slot = 0; slot < 3; ++slot)
    {
        ShaderValidationChannelMask mask;
        shader_validation_instruction_read_mask(instruction, mask, slot);
        if (!shader_validation_operand_writers_in_set(*instruction.operands[slot],
                context.constructors.retail_address(&pass), mask, writers,
                context.constructors.active_owner, context.sets.address_of, context.sets.instruction_at))
        {
            return false;
        }
    }
    if (instruction.output_register != nullptr)
    {
        ShaderValidationChannelMask mask = instruction.output_mask;
        mask.vtable_address = 0x00203790;
        if (!shader_validation_owner_dependencies_registered(*instruction.output_register, pass,
                writers, readers, &instruction, mask, context.sets))
        {
            return false;
        }
    }
    if (instruction.vector_destination != nullptr)
    {
        ShaderValidationChannelMask mask = instruction.vector_mask;
        mask.vtable_address = 0x00203790;
        if (!shader_validation_owner_dependencies_registered(*instruction.vector_destination, pass,
                writers, readers, &instruction, mask, context.sets))
        {
            return false;
        }
        if (instruction.scalar_opcode != 0 && choose_shader_validation_owner_register(context, pass,
                *instruction.vector_destination, readers, instruction, mask, 1) == 1)
        {
            return false;
        }
    }
    if (instruction.scalar_destination != nullptr)
    {
        ShaderValidationChannelMask mask = instruction.scalar_mask;
        mask.vtable_address = 0x00203790;
        if (!shader_validation_owner_dependencies_registered(*instruction.scalar_destination, pass,
                writers, readers, &instruction, mask, context.sets))
        {
            return false;
        }
        if (instruction.vector_opcode != 0 && !assign_shader_validation_owner_scalar_register(context, pass,
                *instruction.scalar_destination, readers, &instruction, mask, 0))
        {
            return false;
        }
    }
    if (instruction.vector_opcode == 13)
    {
        set_shader_validation_pass_owner(pass, instruction.vector_destination, context.sets);
    }
    return true;
}

namespace
{
bool rewrite_move_body(ShaderValidationDependencyContext& context, ShaderValidationInstruction& instruction,
    const ShaderValidationMoveFrame& frame, i32& state, ShaderValidationChannelOrder& order,
    ShaderValidationChannelMask& occupied, ShaderValidationChannelMask& selected)
{
    if (shader_validation_instruction_is_single_move(instruction) == 0)
    {
        return false;
    }
    convert_shader_validation_scalar_move(instruction);
    if (instruction.output_register != nullptr && instruction.output_flag == 0)
    {
        if (instruction.operands[0]->marker != 0 &&
            !negate_shader_validation_owner_writers(*instruction.operands[0]->parent, context.sets))
        {
            return false;
        }
        ShaderValidationChannelOrder argument;
        copy_shader_validation_operand_order(argument, *instruction.operands[0]);
        if (!reorder_shader_validation_owner_channels(*instruction.operands[0]->parent, argument, context.sets))
        {
            return false;
        }
        copy_shader_validation_operand_order(order, *instruction.operands[0]);
        if (!shader_validation_channel_order_keeps_all(order) || instruction.vector_destination != nullptr)
        {
            return false;
        }
    }
    if (instruction.vector_destination == nullptr)
    {
        return true;
    }
    if (instruction.operands[0]->marker != 0 &&
        !negate_shader_validation_owner_writers(*instruction.vector_destination, context.sets))
    {
        return false;
    }
    if (instruction.operands[0]->parent != instruction.vector_destination &&
        instruction.operands[0]->parent != instruction.vector_destination->derived_owner)
    {
        copy_shader_validation_operand_order(order, *instruction.operands[0]);
        state = 2;
        ShaderValidationChannelOrder inverted;
        invert_shader_validation_channel_order(order, inverted);
        if (!reorder_shader_validation_owner_channels(*instruction.vector_destination, inverted, context.sets))
        {
            return false;
        }
        copy_shader_validation_operand_order(order, *instruction.operands[0]);
        return shader_validation_channel_order_keeps_all(order);
    }
    occupied.vtable_address = 0x00203790;
    decode_shader_validation_channel_mask(occupied, 0);
    state = 0;
    std::array<u8, 5> occupied_bytes{0, 0, 0, 0, frame.retained_iterator_byte};
    for (auto* owner = instruction.vector_destination; owner != nullptr; owner = owner->channel_owner)
    {
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (owner->channels[channel] != 0 &&
                owner->channels[channel] != reinterpret_cast<ShaderValidationChannelWriter>(&instruction))
            {
                occupied.channels[channel] = 1;
                occupied_bytes[channel] = 1;
            }
        }
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (instruction.vector_mask.channels[channel] != 0 && occupied.channels[channel] == 0)
            {
                if (shader_validation_owner_diagnostic_reads_channel(*owner, channel, context.sets.operand_at))
                {
                    if (occupied_bytes[instruction.operands[0]->channel_order.channels[channel]] != 0)
                    {
                        return false;
                    }
                }
            }
        }
    }
    selected = {0x00203790, instruction.vector_mask.channels};
    state = 1;
    auto* owner = instruction.vector_destination;
    for (u32 channel = 0; channel < 4; ++channel)
    {
        if (instruction.vector_mask.channels[channel] != 0)
        {
            remove_shader_validation_destination_channel(instruction, channel, instruction.vector_destination);
        }
    }
    if (owner != nullptr)
    {
        while (pack_shader_validation_channel_mask(selected) != 0)
        {
            ShaderValidationBalancedNode* node;
            find_shader_validation_tree_last(owner->diagnostics.nodes, &node);
            while (node != nullptr)
            {
                auto& operand = *context.sets.operand_at(node->value);
                // Channel 4 addresses the adjacent occupied-mask vtable's low byte.
                const std::array<u8, 5> selected_bytes{selected.channels[0], selected.channels[1],
                    selected.channels[2], selected.channels[3], 0x90};
                for (u32 slot = 0; slot < 4; ++slot)
                {
                    const u32 source = operand.channel_order.channels[slot];
                    if (selected_bytes[source] != 0)
                    {
                        u32 replacement = 4;
                        if (source != 4)
                        {
                            replacement = instruction.operands[0]->channel_order.channels[source];
                        }
                        operand.channel_order.channels[slot] = replacement;
                    }
                }
                advance_shader_validation_arena_iterator(node);
            }
            owner = owner->channel_owner;
            if (owner == nullptr)
            {
                break;
            }
            ShaderValidationChannelMask owner_mask;
            ShaderValidationChannelMask inverted;
            ShaderValidationChannelMask remaining;
            shader_validation_owner_channel_mask(*owner, owner_mask);
            invert_shader_validation_channel_mask(owner_mask, inverted);
            intersect_shader_validation_channel_masks(selected, inverted, remaining);
            copy_shader_validation_channel_mask(selected, remaining);
            destroy_shader_validation_channel_mask(remaining);
            destroy_shader_validation_channel_mask(owner_mask);
        }
    }
    return true;
}

bool swap_merge_owner_channels(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, u32 first, u32 second)
{
    if (owner.derived_owner != nullptr)
    {
        return swap_shader_validation_owner_channels(owner, first, second, 1, 1, context.sets.operand_at);
    }
    const auto first_writer = owner.channels[first];
    const auto second_writer = owner.channels[second];
    if (first_writer == ~ShaderValidationChannelWriter{0} || second_writer == ~ShaderValidationChannelWriter{0})
    {
        return false;
    }
    if (owner.channel_owner != nullptr && !swap_shader_validation_owner_channels(
            *owner.channel_owner, first, second, 0, 1, context.sets.operand_at))
    {
        return false;
    }
    bool changed = false;
    if (first_writer == second_writer)
    {
        if (second_writer == 0)
        {
            changed = true;
        }
        else
        {
            changed = swap_shader_validation_destination_channels(
                *reinterpret_cast<ShaderValidationInstruction*>(second_writer), &owner, first, second, 0);
        }
    }
    else if (second_writer == 0)
    {
        changed = swap_shader_validation_destination_channels(
            *reinterpret_cast<ShaderValidationInstruction*>(first_writer), &owner, first, second, 0);
        if (changed)
        {
            owner.channels[second] = first_writer;
            owner.channels[first] = 0;
        }
    }
    else
    {
        const bool second_changed = swap_shader_validation_destination_channels(
            *reinterpret_cast<ShaderValidationInstruction*>(second_writer), &owner, first, second, 0);
        if (first_writer == 0)
        {
            changed = second_changed;
            if (changed)
            {
                owner.channels[first] = second_writer;
                owner.channels[second] = 0;
            }
        }
        else if (second_changed)
        {
            changed = swap_shader_validation_destination_channels(
                *reinterpret_cast<ShaderValidationInstruction*>(first_writer), &owner, first, second, 0);
            if (!changed)
            {
                swap_shader_validation_destination_channels(
                    *reinterpret_cast<ShaderValidationInstruction*>(second_writer), &owner, first, second, 0);
            }
            else
            {
                owner.channels[first] = second_writer;
                owner.channels[second] = first_writer;
            }
        }
    }
    if (changed)
    {
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
        while (node != nullptr)
        {
            auto& operand = *context.sets.operand_at(node->value);
            for (u32 slot = 0; slot < 4; ++slot)
            {
                if (operand.channel_order.channels[slot] == first)
                {
                    operand.channel_order.channels[slot] = second;
                }
                else if (operand.channel_order.channels[slot] == second)
                {
                    operand.channel_order.channels[slot] = first;
                }
            }
            advance_shader_validation_arena_iterator(node);
        }
    }
    else if (owner.channel_owner != nullptr)
    {
        swap_shader_validation_owner_channels(*owner.channel_owner, first, second, 0, 1, context.sets.operand_at);
    }
    return changed;
}

bool merge_owners_body(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner,
    ShaderValidationOwner* other, u8 query_only, ShaderValidationChannelMask& owner_mask,
    ShaderValidationChannelMask& other_mask, i32& state,
    ShaderValidationBalancedChildTree& dependencies, ShaderValidationChannelOrder& order)
{
    if (other == nullptr || &owner == other)
    {
        return true;
    }
    const bool adjacent = owner.derived_owner == other || owner.channel_owner == other;
    if (adjacent)
    {
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (owner.channels[channel] != 0 && other->channels[channel] != 0)
            {
                return false;
            }
        }
    }
    else
    {
        if (other->derived_owner != nullptr &&
            (owner.derived_owner != nullptr || pack_shader_validation_channel_mask(owner_mask) != 0))
        {
            return false;
        }
        ShaderValidationOwner* channel_chain;
        if (other->channel_owner == nullptr)
        {
            channel_chain = owner.channel_owner != nullptr ? &owner : other;
        }
        else
        {
            if (owner.channel_owner != nullptr)
            {
                return false;
            }
            channel_chain = other;
        }
        ShaderValidationOwner* derived_chain = owner.derived_owner != nullptr ? &owner : other;
        if (derived_chain != channel_chain)
        {
            dependencies.vtable_address = 0x002037D0;
            dependencies.nodes = {nullptr, 0};
            state = 2;
            for (auto* current = channel_chain; current != nullptr; current = current->channel_owner)
            {
                ShaderValidationChannelMask mask;
                shader_validation_owner_channel_mask(*current, mask);
                collect_shader_validation_owner_dependencies(context, *current, dependencies, mask);
            }
            bool collision = false;
            for (auto* current = derived_chain; current != nullptr && !collision; current = current->derived_owner)
            {
                for (u32 channel = 0; channel < 4; ++channel)
                {
                    const auto writer = current->channels[channel];
                    if (writer != 0 && writer != ~ShaderValidationChannelWriter{0})
                    {
                        if (find_shader_validation_instruction_node(dependencies.nodes,
                                context.sets.address_of(reinterpret_cast<void*>(writer)), context.sets.instruction_at) != nullptr)
                        {
                            collision = true;
                            break;
                        }
                    }
                }
            }
            state = 1;
            dependencies.vtable_address = 0x002037D0;
            destroy_shader_validation_balanced_nodes_18f2aa(dependencies.nodes.root, context.sets.deleting_destructor);
            if (collision)
            {
                return false;
            }
        }
        for (u32 first = 0; first < 4; ++first)
        {
            if (owner.channels[first] == 0 && !shader_validation_owner_channel_has_reader(owner, first, context.sets.operand_at)
                && owner_mask.channels[first] == 0)
            {
                continue;
            }
            if (find_shader_validation_owner_channel(other, first) == 0 &&
                !shader_validation_owner_channel_has_reader(*other, first, context.sets.operand_at) && other_mask.channels[first] == 0)
            {
                continue;
            }
            bool changed = false;
            for (u32 second = 0; second < 4 && !changed; ++second)
            {
                if (owner.channels[second] != 0 || shader_validation_owner_channel_has_reader(owner, second, context.sets.operand_at)
                    || owner_mask.channels[second] != 0 || find_shader_validation_owner_channel(other, second) != 0
                    || shader_validation_owner_channel_has_reader(*other, second, context.sets.operand_at)
                    || other_mask.channels[first] != 0)
                {
                    continue;
                }
                changed = swap_merge_owner_channels(context, *other, first, second);
                if (!changed && owner_mask.channels[second] == 0)
                {
                    changed = swap_merge_owner_channels(context, owner, first, second);
                }
            }
            if (!changed)
            {
                return false;
            }
        }
    }
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(other->diagnostics.nodes, &node);
    while (node != nullptr)
    {
        auto& operand = *context.sets.operand_at(node->value);
        copy_shader_validation_operand_order(order, operand);
        state = 3;
        ShaderValidationChannelMask used;
        shader_validation_channel_order_used_mask(order, used);
        const bool accepted = shader_validation_operand_accepts_parent(context, operand, owner, used);
        state = 1;
        destroy_shader_validation_channel_order(order);
        if (!accepted)
        {
            return false;
        }
        advance_shader_validation_arena_iterator(node);
    }
    if (query_only == 0)
    {
        if (adjacent)
        {
            if (owner.derived_owner == other)
            {
                owner.derived_owner = nullptr;
                other->channel_owner = nullptr;
            }
            if (owner.channel_owner == other)
            {
                owner.channel_owner = nullptr;
                other->derived_owner = nullptr;
            }
        }
        if (owner.derived_owner == nullptr)
        {
            set_shader_validation_owner_derived(owner, other->derived_owner);
        }
        if (owner.channel_owner == nullptr)
        {
            set_shader_validation_owner_channel_owner(owner, other->channel_owner);
        }
        for (u32 channel = 0; channel < 4; ++channel)
        {
            const auto writer = other->channels[channel];
            if (writer != 0 && writer != ~ShaderValidationChannelWriter{0})
            {
                auto& instruction = *reinterpret_cast<ShaderValidationInstruction*>(writer);
                if (instruction.output_register == other)
                {
                    set_shader_validation_output_destination(instruction, &owner);
                }
                if (instruction.vector_destination == other)
                {
                    set_shader_validation_vector_destination(instruction, &owner);
                }
                if (instruction.scalar_destination == other)
                {
                    set_shader_validation_scalar_destination(instruction, &owner);
                }
            }
        }
        u32 operand;
        while ((operand = last_shader_validation_tree_value(other->diagnostics.nodes)) != 0)
        {
            reparent_shader_validation_operand(context.constructors, *context.sets.operand_at(operand), &owner);
        }
    }
    return true;
}

bool instruction_registers_available_body(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    const ShaderValidationInstruction& instruction, const ShaderValidationBalancedTree& readers,
    i32* available, i32& state, ShaderValidationChannelMask& selected)
{
    i32 local_available;
    if (available == nullptr)
    {
        available = &local_available;
    }
    *available = std::bit_cast<i32>(12u - pass.registers.tree.nodes.count);
    if (find_shader_validation_pass_node_signed(pass.registers.tree.nodes, 12) != nullptr)
    {
        *available = std::bit_cast<i32>(std::bit_cast<u32>(*available) + 1u);
    }
    const bool combined = instruction.vector_opcode != 0 && instruction.scalar_opcode != 0;
    if (combined && find_shader_validation_pass_node_signed(pass.registers.tree.nodes, 1) == nullptr)
    {
        *available = std::bit_cast<i32>(std::bit_cast<u32>(*available) - 1u);
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        auto* owner = instruction.operands[slot]->parent;
        if (owner != nullptr && (owner->kind == 1 || owner->kind == 6))
        {
            do
            {
                ShaderValidationChannelMask full;
                construct_shader_validation_channel_mask(full, 0xF);
                if (!shader_validation_owner_readers_registered(*owner, readers, full, &instruction, 1, 1, context.sets) ||
                    (combined && owner->kind == 1 && owner->source == 1))
                {
                    break;
                }
                owner = owner->channel_owner;
            } while (owner != nullptr);
            if (owner == nullptr)
            {
                *available = std::bit_cast<i32>(std::bit_cast<u32>(*available) + 1u);
            }
        }
    }
    auto* destination = instruction.vector_opcode == 0 ? instruction.scalar_destination : instruction.vector_destination;
    if (destination == nullptr || destination->kind == 1)
    {
        return true;
    }
    const i32 kind = find_shader_validation_owner_register_kind(pass, destination);
    if (kind < 0)
    {
        *available = std::bit_cast<i32>(std::bit_cast<u32>(*available) - 1u);
        return *available >= 0;
    }
    if (instruction.vector_opcode == 0)
    {
        copy_shader_validation_scalar_mask(instruction, selected);
    }
    else
    {
        copy_shader_validation_vector_mask(instruction, selected);
    }
    selected.vtable_address = 0x00203790;
    state = 0;
    return assign_shader_validation_owner_register(context, pass, static_cast<u32>(kind), *destination, readers, &instruction,
        ShaderValidationChannelMask{0x00203790, selected.channels}, 1);
}

bool assign_owner_register_body(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    u32 kind, ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask& mask, u8 query_only,
    i32& state, ShaderValidationChannelMask& unregistered)
{
    const i32 registered = find_shader_validation_owner_register_kind(pass, &owner);
    if (static_cast<u32>(registered) == kind)
    {
        if (owner.kind == 1)
        {
            return true;
        }
    }
    else if (registered >= 0)
    {
        return false;
    }
    if (owner.derived_owner != nullptr)
    {
        ShaderValidationChannelMask occupied;
        shader_validation_owner_channel_mask(*owner.derived_owner, occupied);
        if (!assign_shader_validation_owner_register(context, pass, kind, *owner.derived_owner, readers, instruction, occupied, query_only))
        {
            return false;
        }
        if (!shader_validation_owner_readers_registered(*owner.derived_owner, readers, mask, instruction, 1, 1, context.sets))
        {
            ShaderValidationChannelMask missing;
            shader_validation_unregistered_reader_channels(*owner.derived_owner, missing, readers, instruction, context.sets);
            if (!move_shader_validation_masked_channels(owner, missing, 1, 1, context.sets))
            {
                return false;
            }
        }
    }
    else
    {
        auto* found = find_shader_validation_pass_node(pass.registers.tree.nodes, kind);
        if (found != nullptr)
        {
            found = find_shader_validation_pass_node(pass.registers.tree.nodes, kind);
            auto* ancestor = static_cast<ShaderValidationOwner*>(found->data);
            while (ancestor->channel_owner != nullptr)
            {
                ancestor = ancestor->channel_owner;
            }
            shader_validation_unregistered_reader_channels(*ancestor, unregistered, readers, instruction, context.sets);
            state = 1;
            ShaderValidationChannelMask empty;
            construct_shader_validation_channel_mask(empty, 0);
            // Retail puts the pass address into this by-value mask's bytes at 0x00196966.
            ShaderValidationChannelMask pass_mask{0x00203790,
                std::bit_cast<std::array<u8, 4>>(context.constructors.retail_address(&pass))};
            if (!link_shader_validation_owners(&owner, ancestor, query_only, pass_mask, empty, context.sets))
            {
                return false;
            }
            if (query_only == 0)
            {
                while (rotate_shader_validation_kind_six_owner(context, owner))
                {
                }
                owner.kind = 1;
                owner.source = kind;
                ShaderValidationPassNode* inserted;
                set_shader_validation_pass_value(pass.registers.tree.nodes, context.constructors.allocate, &inserted, kind, &owner);
            }
            return true;
        }
    }
    if (query_only == 0)
    {
        ShaderValidationPassNode* inserted;
        set_shader_validation_pass_value(pass.registers.tree.nodes, context.constructors.allocate, &inserted, kind, &owner);
        owner.kind = 1;
        owner.source = kind;
    }
    return true;
}

bool operand_accepts_parent_body(ShaderValidationDependencyContext& context,
    ShaderValidationOperand& operand, ShaderValidationOwner& parent, ShaderValidationChannelMask& mask)
{
    if (operand.instruction != nullptr)
    {
        auto& instruction = *operand.instruction;
        const u32 kind = parent.kind;
        if (kind == 2 || kind == 3)
        {
            for (u32 slot = 0; slot < 3; ++slot)
            {
                if (slot != operand.kind)
                {
                    auto* other = instruction.operands[slot]->parent;
                    if (other != nullptr)
                    {
                        if (!shader_validation_owners_equivalent(other, &parent, context.constructors.active_owner) && other->kind == kind)
                        {
                            return false;
                        }
                    }
                }
            }
        }
        if (kind == 1 || kind == 6)
        {
            for (u32 slot = 0; slot < 3; ++slot)
            {
                if (slot != operand.kind)
                {
                    auto* other = instruction.operands[slot]->parent;
                    if (other != nullptr && !shader_validation_owners_equivalent(other, &parent, context.constructors.active_owner))
                    {
                        for (auto* current = other; current != nullptr; current = current->derived_owner)
                        {
                            if (!shader_validation_owner_derives_from(*current, &parent) ||
                                shader_validation_owners_equivalent(&parent, current, context.constructors.active_owner))
                            {
                                break;
                            }
                            ShaderValidationChannelMask occupied, overlap;
                            shader_validation_owner_channel_mask(*current, occupied);
                            intersect_shader_validation_channel_masks(occupied, mask, overlap);
                            const bool conflicts = pack_shader_validation_channel_mask(overlap) != 0;
                            destroy_shader_validation_channel_mask(overlap);
                            destroy_shader_validation_channel_mask(occupied);
                            if (conflicts)
                            {
                                return false;
                            }
                            for (auto writer : current->channels)
                            {
                                if (writer != 0)
                                {
                                    auto& writer_instruction = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(
                                        context.sets.address_of(reinterpret_cast<const void*>(writer))));
                                    if (shader_validation_instruction_has_dependency(context, writer_instruction, context.sets.address_of(&instruction)))
                                    {
                                        return false;
                                    }
                                }
                            }
                        }
                        for (auto* current = &parent; current != nullptr; current = current->derived_owner)
                        {
                            if (!shader_validation_owner_derives_from(*current, other) ||
                                shader_validation_owners_equivalent(other, current, context.constructors.active_owner))
                            {
                                break;
                            }
                            ShaderValidationChannelMask occupied, overlap;
                            shader_validation_owner_channel_mask(*current, occupied);
                            intersect_shader_validation_channel_masks(occupied, mask, overlap);
                            const bool conflicts = pack_shader_validation_channel_mask(overlap) != 0;
                            destroy_shader_validation_channel_mask(overlap);
                            destroy_shader_validation_channel_mask(occupied);
                            if (conflicts)
                            {
                                return false;
                            }
                            for (auto writer : current->channels)
                            {
                                if (writer != 0)
                                {
                                    auto& writer_instruction = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(
                                        context.sets.address_of(reinterpret_cast<const void*>(writer))));
                                    if (shader_validation_instruction_has_dependency(context, writer_instruction, context.sets.address_of(&instruction)))
                                    {
                                        return false;
                                    }
                                }
                            }
                        }
                    }
                }
            }
            auto* ancestor = parent.channel_owner;
            while (ancestor != nullptr)
            {
                for (u32 channel = 0; channel < 4; ++channel)
                {
                    if (mask.channels[channel] != 0 && ancestor->channels[channel] != 0)
                    {
                        auto& writer = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(
                            context.sets.address_of(reinterpret_cast<const void*>(ancestor->channels[channel]))));
                        if (shader_validation_instruction_has_dependency(context, writer, context.sets.address_of(&instruction)))
                        {
                            return false;
                        }
                    }
                }
                ancestor = ancestor->channel_owner;
            }
            if (shader_validation_owner_derives_from(parent, instruction.output_register) ||
                shader_validation_owner_derives_from(parent, instruction.vector_destination) ||
                shader_validation_owner_derives_from(parent, instruction.scalar_destination))
            {
                return false;
            }
        }
    }
    return true;
}

void move_readers_body(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner, u8 climb,
    i32& state, ShaderValidationChannelOrder& order)
{
    if (owner.channel_owner != nullptr && climb != 0)
    {
        move_shader_validation_readers_to_derived(context, *owner.channel_owner, 1);
    }
    if (owner.derived_owner != nullptr)
    {
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
        while (node != nullptr)
        {
            auto& operand = *context.sets.operand_at(node->value);
            bool allowed = true;
            for (u32 channel = 0; channel < 4; ++channel)
            {
                if (owner.channels[channel] != 0 && shader_validation_operand_reads_order_channel(operand, channel))
                {
                    allowed = false;
                }
            }
            if (allowed)
            {
                copy_shader_validation_operand_order(order, operand);
                state = 0;
                ShaderValidationChannelMask used;
                shader_validation_channel_order_used_mask(order, used);
                allowed = shader_validation_operand_accepts_parent(context, operand, *owner.derived_owner, used);
            }
            state = -1;
            order.vtable_address = 0x00203794;
            if (allowed && operand.instruction != nullptr)
            {
                for (auto writer : owner.channels)
                {
                    if (writer != 0)
                    {
                        auto& writer_instruction = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(
                            context.sets.address_of(reinterpret_cast<const void*>(writer))));
                        if (shader_validation_instruction_has_dependency(context, writer_instruction, context.sets.address_of(operand.instruction)))
                        {
                            allowed = false;
                        }
                    }
                }
            }
            if (allowed)
            {
                reparent_shader_validation_operand(context.constructors, operand, owner.derived_owner);
                find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
            }
            else
            {
                advance_shader_validation_arena_iterator(node);
            }
        }
    }
}

void build_dependencies_body(ShaderValidationDependencyContext& context, ShaderValidationInstruction& instruction,
    i32& state, ShaderValidationChannelOrder& order, ShaderValidationChannelMask& used)
{
    if (instruction.instruction_set == nullptr)
    {
        auto* allocation = static_cast<ShaderValidationBalancedChildTree*>(
            context.constructors.allocate(sizeof(ShaderValidationBalancedChildTree), 0x24870000));
        if (allocation != nullptr)
        {
            allocation->vtable_address = 0x002037D0;
            allocation->nodes.root = nullptr;
            allocation->nodes.count = 0;
        }
        instruction.instruction_set = allocation;
        register_shader_validation_value_2037ec(context.instruction_sets, context.constructors.retail_address(allocation));
        const std::array<ShaderValidationOwner**, 3> destinations{&instruction.output_register, &instruction.vector_destination, &instruction.scalar_destination};
        const std::array<ShaderValidationChannelMask*, 3> masks{&instruction.output_mask, &instruction.vector_mask, &instruction.scalar_mask};
        for (u32 slot = 0; slot < 3; ++slot)
        {
            if (*destinations[slot] != nullptr)
            {
                collect_shader_validation_owner_dependencies(context, **destinations[slot], *instruction.instruction_set,
                    ShaderValidationChannelMask{0x00203790, masks[slot]->channels});
            }
        }
        if (context.include_operand_writers != 0)
        {
            for (u32 slot = 0; slot < 3; ++slot)
            {
                auto& operand = *instruction.operands[slot];
                copy_shader_validation_operand_order(order, operand);
                shader_validation_channel_order_used_mask(order, used);
                ShaderValidationOwner* owner = operand.parent->channel_owner;
                state = 0;
                order.vtable_address = 0x00203794;
                if (owner != nullptr)
                {
                    while (pack_shader_validation_channel_mask(used) != 0)
                    {
                        for (u32 channel = 0; channel < 4; ++channel)
                        {
                            if (used.channels[channel] != 0 && owner->channels[channel] != 0)
                            {
                                const u32 address = context.sets.address_of(reinterpret_cast<const void*>(owner->channels[channel]));
                                ShaderValidationInsertContext insert{instruction.instruction_set->nodes, context.constructors.allocate};
                                register_shader_validation_instruction(insert, address, context.sets.instruction_at);
                                auto* writer = const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(address));
                                merge_shader_validation_instruction_dependencies(context, *writer, *instruction.instruction_set);
                                used.channels[channel] = 0;
                            }
                        }
                        owner = owner->channel_owner;
                        if (owner == nullptr)
                        {
                            break;
                        }
                    }
                }
                state = -1;
                destroy_shader_validation_channel_mask(used);
            }
        }
    }
    instruction.instruction_set_count = instruction.instruction_set->nodes.count;
}
}

// Xbox 0x00197075
bool shader_validation_instruction_registers_available(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, const ShaderValidationInstruction& instruction,
    const ShaderValidationBalancedTree& readers, i32* available)
{
    i32 state = -1;
    ShaderValidationChannelMask selected;
    bool result = false;
    GBTG_SEH_TRY_FINALLY
    {
        result = instruction_registers_available_body(context, pass, instruction, readers, available, state, selected);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 0)
        {
            destroy_shader_validation_channel_mask(selected);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x00196814
bool assign_shader_validation_owner_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    u32 kind, ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask, u8 query_only)
{
    i32 state = 0;
    ShaderValidationChannelMask unregistered;
    bool result = false;
    GBTG_SEH_TRY_FINALLY
    {
        result = assign_owner_register_body(context, pass, kind, owner, readers, instruction, mask, query_only, state, unregistered);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            if (state == 1)
            {
                destroy_shader_validation_channel_mask(unregistered);
            }
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x001969C0
bool assign_shader_validation_owner_scalar_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask, u8 query_only)
{
    bool result = false;
    GBTG_SEH_TRY_FINALLY
    {
        while (rotate_shader_validation_kind_six_owner(context, owner))
        {
        }
        result = assign_shader_validation_owner_register(context, pass, 1, owner, readers, instruction,
            ShaderValidationChannelMask{0x00203790, mask.channels}, query_only);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x00196A14
i32 choose_shader_validation_owner_register(ShaderValidationDependencyContext& context, ShaderValidationPass& pass,
    ShaderValidationOwner& owner, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction& instruction, ShaderValidationChannelMask mask, u8 query_only)
{
    i32 result = -1;
    GBTG_SEH_TRY_FINALLY
    {
        while (rotate_shader_validation_kind_six_owner(context, owner))
        {
        }
        if (owner.source != 1 && assign_shader_validation_owner_register(context, pass, owner.source, owner, readers, &instruction,
                ShaderValidationChannelMask{0x00203790, mask.channels}, query_only))
        {
            result = static_cast<i32>(owner.source);
        }
        else
        {
            i32 kind = 11;
            for (; kind > 1; --kind)
            {
                if (assign_shader_validation_owner_register(context, pass, static_cast<u32>(kind), owner, readers, &instruction,
                        ShaderValidationChannelMask{0x00203790, mask.channels}, query_only))
                {
                    result = kind;
                    break;
                }
            }
            if (kind <= 1)
            {
                if (assign_shader_validation_owner_register(context, pass, 0, owner, readers, &instruction,
                        ShaderValidationChannelMask{0x00203790, mask.channels}, query_only))
                {
                    result = 0;
                }
                else if (instruction.vector_opcode == 0 || instruction.scalar_opcode == 0)
                {
                    if (assign_shader_validation_owner_register(context, pass, 1, owner, readers, &instruction,
                            ShaderValidationChannelMask{0x00203790, mask.channels}, query_only))
                    {
                        result = 1;
                    }
                }
            }
        }
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x00196C74
bool shader_validation_instructions_merge_on_pass(ShaderValidationDependencyContext& context,
    ShaderValidationPass& pass, ShaderValidationInstruction& instruction, ShaderValidationInstruction& other,
    const ShaderValidationBalancedTree& readers)
{
    if ((instruction.output_register != nullptr && other.output_register != nullptr && instruction.output_register != other.output_register) ||
        (instruction.vector_destination != nullptr && other.vector_destination != nullptr && instruction.vector_destination != other.vector_destination) ||
        (instruction.scalar_destination != nullptr && other.scalar_destination != nullptr && instruction.scalar_destination != other.scalar_destination))
    {
        return false;
    }
    if (!shader_validation_instructions_merge_compatible(context, instruction, other))
    {
        return false;
    }
    if (instruction.scalar_opcode != 0 && other.vector_destination != nullptr &&
        shader_validation_owner_chain_has_kind_one(pass, other.vector_destination))
    {
        return false;
    }
    if (other.scalar_opcode != 0 && instruction.vector_destination != nullptr &&
        shader_validation_owner_chain_has_kind_one(pass, instruction.vector_destination))
    {
        return false;
    }
    if (instruction.vector_opcode != 0 && other.scalar_destination != nullptr)
    {
        ShaderValidationChannelMask mask;
        copy_shader_validation_scalar_mask(other, mask);
        if (!assign_shader_validation_owner_scalar_register(context, pass, *other.scalar_destination,
                readers, &other, mask, 0))
        {
            return false;
        }
    }
    if (other.vector_opcode != 0 && instruction.scalar_destination != nullptr)
    {
        ShaderValidationChannelMask mask;
        copy_shader_validation_scalar_mask(instruction, mask);
        if (!assign_shader_validation_owner_scalar_register(context, pass, *instruction.scalar_destination,
                readers, &instruction, mask, 0))
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x00193222
bool rewrite_shader_validation_move(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, const ShaderValidationMoveFrame& frame)
{
    i32 state = -1;
    bool result = false;
    ShaderValidationChannelOrder order;
    ShaderValidationChannelMask occupied;
    ShaderValidationChannelMask selected;
    GBTG_SEH_TRY_FINALLY
    {
        result = rewrite_move_body(context, instruction, frame, state, order, occupied, selected);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            if (state == 2)
            {
                destroy_shader_validation_channel_order(order);
            }
            else
            {
                if (state == 1)
                {
                    destroy_shader_validation_channel_mask(selected);
                }
                if (state == 0 || state == 1)
                {
                    destroy_shader_validation_channel_mask(occupied);
                }
            }
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x001954F3
bool shader_validation_instructions_merge_compatible(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationInstruction& other)
{
    if (&instruction == &other ||
        (instruction.vector_opcode != 0 && other.vector_opcode != 0 && instruction.vector_opcode != other.vector_opcode) ||
        (instruction.scalar_opcode != 0 && other.scalar_opcode != 0 && instruction.scalar_opcode != other.scalar_opcode))
    {
        return false;
    }
    build_shader_validation_instruction_dependencies(context, instruction);
    build_shader_validation_instruction_dependencies(context, instruction);
    if (find_shader_validation_instruction_node(instruction.instruction_set->nodes,
            context.sets.address_of(&other), context.sets.instruction_at) != nullptr)
    {
        return false;
    }
    build_shader_validation_instruction_dependencies(context, other);
    build_shader_validation_instruction_dependencies(context, other);
    if (find_shader_validation_instruction_node(other.instruction_set->nodes,
            context.sets.address_of(&instruction), context.sets.instruction_at) != nullptr)
    {
        return false;
    }
    for (u32 slot = 0; slot < 3; ++slot)
    {
        if (shader_validation_instruction_uses_slot(other, slot))
        {
            if (shader_validation_instruction_uses_slot(instruction, slot))
            {
                if (!shader_validation_operands_equivalent(*instruction.operands[slot], *other.operands[slot],
                        context.constructors.active_owner))
                {
                    return false;
                }
            }
            else
            {
                ShaderValidationChannelMask full;
                construct_shader_validation_channel_mask(full, 0xF);
                if (!shader_validation_operand_accepts_parent(context, *instruction.operands[slot],
                        *other.operands[slot]->parent, full))
                {
                    return false;
                }
            }
        }
    }
    if (instruction.output_register != nullptr && other.output_register != nullptr)
    {
        if (instruction.output_register != other.output_register || instruction.output_flag != other.output_flag)
        {
            return false;
        }
    }
    if (instruction.vector_destination != nullptr)
    {
        if (shader_validation_owner_derives_from(*instruction.vector_destination, other.scalar_destination))
        {
            return false;
        }
        if (other.scalar_destination != nullptr &&
            shader_validation_owner_derives_from(*other.scalar_destination, instruction.vector_destination))
        {
            return false;
        }
        if (other.vector_destination != nullptr && instruction.vector_destination != other.vector_destination)
        {
            ShaderValidationChannelMask first;
            ShaderValidationChannelMask second;
            shader_validation_owner_channel_mask(*other.vector_destination, second);
            shader_validation_owner_channel_mask(*instruction.vector_destination, first);
            if (!merge_shader_validation_owners(context, *instruction.vector_destination, other.vector_destination, 1, first, second))
            {
                return false;
            }
        }
    }
    if (instruction.scalar_destination != nullptr)
    {
        if (shader_validation_owner_derives_from(*instruction.scalar_destination, other.vector_destination))
        {
            return false;
        }
        if (other.vector_destination != nullptr &&
            shader_validation_owner_derives_from(*other.vector_destination, instruction.scalar_destination))
        {
            return false;
        }
        if (other.scalar_destination != nullptr && instruction.scalar_destination != other.scalar_destination)
        {
            ShaderValidationChannelMask first;
            ShaderValidationChannelMask second;
            shader_validation_owner_channel_mask(*other.scalar_destination, second);
            shader_validation_owner_channel_mask(*instruction.scalar_destination, first);
            if (!merge_shader_validation_owners(context, *instruction.scalar_destination, other.scalar_destination, 1, first, second))
            {
                return false;
            }
        }
    }
    return true;
}

// Xbox 0x00195C1A
bool merge_or_link_shader_validation_owners(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner,
    ShaderValidationOwner* other, u8 query_only, ShaderValidationChannelMask owner_mask,
    ShaderValidationChannelMask other_mask)
{
    bool result = false;
    GBTG_SEH_TRY_FINALLY
    {
        ShaderValidationChannelMask first{0x00203790, owner_mask.channels};
        ShaderValidationChannelMask second{0x00203790, other_mask.channels};
        result = merge_shader_validation_owners(context, owner, other, query_only, first, second);
        if (!result)
        {
            first = {0x00203790, owner_mask.channels};
            second = {0x00203790, other_mask.channels};
            result = link_shader_validation_owners(&owner, other, query_only, first, second, context.sets);
        }
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            destroy_shader_validation_channel_mask(owner_mask);
            destroy_shader_validation_channel_mask(other_mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x00194E5E
bool merge_shader_validation_owners(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner,
    ShaderValidationOwner* other, u8 query_only, ShaderValidationChannelMask owner_mask,
    ShaderValidationChannelMask other_mask)
{
    i32 state = 1;
    bool result = false;
    ShaderValidationBalancedChildTree dependencies;
    ShaderValidationChannelOrder order;
    GBTG_SEH_TRY_FINALLY
    {
        result = merge_owners_body(context, owner, other, query_only, owner_mask, other_mask, state, dependencies, order);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            if (state == 3)
            {
                destroy_shader_validation_channel_order(order);
            }
            else if (state == 2)
            {
                dependencies.vtable_address = 0x002037D0;
                clear_shader_validation_balanced_tree_190322(dependencies, context.sets.deleting_destructor);
            }
            destroy_shader_validation_channel_mask(owner_mask);
            destroy_shader_validation_channel_mask(other_mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x001940AC
bool shader_validation_operand_accepts_parent(ShaderValidationDependencyContext& context,
    ShaderValidationOperand& operand, ShaderValidationOwner& parent, ShaderValidationChannelMask mask)
{
    bool result = false;
    GBTG_SEH_TRY_FINALLY
    {
        result = operand_accepts_parent_body(context, operand, parent, mask);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return result;
}

// Xbox 0x00194D12
void move_shader_validation_readers_to_derived(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, u8 climb)
{
    i32 state = -1;
    ShaderValidationChannelOrder order;
    GBTG_SEH_TRY_FINALLY
    {
        move_readers_body(context, owner, climb, state, order);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 0)
        {
            destroy_shader_validation_channel_order(order);
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x00195BE1
bool detach_unused_shader_validation_derived_owner(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner)
{
    if (owner.kind != 5)
    {
        move_shader_validation_readers_to_derived(context, owner, 1);
        move_shader_validation_unwritten_readers_to_derived(context.constructors, owner, context.sets.operand_at);
        if (!shader_validation_owner_has_unwritten_dependency(owner, context.sets.operand_at))
        {
            if (owner.derived_owner != nullptr)
            {
                owner.derived_owner->channel_owner = nullptr;
            }
            owner.derived_owner = nullptr;
            return true;
        }
    }
    return false;
}

// Xbox 0x00195CA0
bool rotate_shader_validation_kind_six_owner(ShaderValidationDependencyContext& context, ShaderValidationOwner& owner)
{
    auto* derived = owner.derived_owner;
    if (derived == nullptr || derived->kind != 6)
    {
        return false;
    }
    for (u32 channel = 0; channel < 4; ++channel)
    {
        if (owner.channels[channel] != 0 && (derived->channels[channel] != 0 ||
                shader_validation_owner_diagnostic_reads_channel(*derived, channel, context.sets.operand_at)))
        {
            return false;
        }
    }
    u32 reader = last_shader_validation_tree_value(owner.diagnostics.nodes);
    while (reader != 0)
    {
        reparent_shader_validation_operand(context.constructors, *context.sets.operand_at(reader), owner.derived_owner);
        reader = last_shader_validation_tree_value(owner.diagnostics.nodes);
    }
    derived = owner.derived_owner;
    derived->channel_owner = owner.channel_owner;
    owner.derived_owner = derived->derived_owner;
    owner.channel_owner = derived;
    derived->derived_owner = &owner;
    if (owner.derived_owner != nullptr)
    {
        owner.derived_owner->channel_owner = &owner;
    }
    if (derived->channel_owner != nullptr)
    {
        derived->channel_owner->derived_owner = derived;
    }
    move_shader_validation_readers_to_derived(context, *derived, 0);
    return true;
}

static ShaderValidationBalancedChildTree& collect_owner_dependencies_body(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, ShaderValidationBalancedChildTree& dependencies, ShaderValidationChannelMask mask)
{
    {
        ShaderValidationBalancedNode* node;
        find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
        while (node != nullptr)
        {
            auto& operand = *context.sets.operand_at(node->value);
            if (operand.instruction != nullptr)
            {
                const u32 address = context.sets.address_of(operand.instruction);
                if (find_shader_validation_instruction_node(dependencies.nodes, address, context.sets.instruction_at) == nullptr)
                {
                    if (shader_validation_operand_reads_masked_channel(operand, ShaderValidationChannelMask{0x00203790, mask.channels}))
                    {
                        ShaderValidationInsertContext insert{dependencies.nodes, context.constructors.allocate};
                        register_shader_validation_instruction(insert, address, context.sets.instruction_at);
                        merge_shader_validation_instruction_dependencies(context, *operand.instruction, dependencies);
                    }
                }
            }
            if (operand.owner != nullptr)
            {
                ShaderValidationChannelMask full;
                construct_shader_validation_channel_mask(full, 0xF);
                collect_shader_validation_owner_dependencies(context, *operand.owner, dependencies, full);
            }
            advance_shader_validation_balanced_iterator(&node);
        }
        if (owner.channel_owner != nullptr)
        {
            for (u32 channel = 0; channel < 4; ++channel)
            {
                if (mask.channels[channel] != 0 && owner.channel_owner->channels[channel] != 0)
                {
                    const u32 address = context.sets.address_of(reinterpret_cast<const void*>(owner.channel_owner->channels[channel]));
                    if (find_shader_validation_instruction_node(dependencies.nodes, address, context.sets.instruction_at) == nullptr)
                    {
                        ShaderValidationInsertContext insert{dependencies.nodes, context.constructors.allocate};
                        register_shader_validation_instruction(insert, address, context.sets.instruction_at);
                        auto* writer = const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(address));
                        merge_shader_validation_instruction_dependencies(context, *writer, dependencies);
                    }
                    mask.channels[channel] = 0;
                }
            }
            if (pack_shader_validation_channel_mask(mask) != 0)
            {
                collect_shader_validation_owner_dependencies(context, *owner.channel_owner, dependencies,
                    ShaderValidationChannelMask{0x00203790, mask.channels});
            }
        }
    }
    return dependencies;
}

// Xbox 0x0019390D
ShaderValidationBalancedChildTree& collect_shader_validation_owner_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationOwner& owner, ShaderValidationBalancedChildTree& dependencies, ShaderValidationChannelMask mask)
{
    GBTG_SEH_TRY_FINALLY
    {
        collect_owner_dependencies_body(context, owner, dependencies, mask);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL())
        {
            destroy_shader_validation_channel_mask(mask);
        }
    }
    GBTG_SEH_FINALLY_END
    return dependencies;
}

// Xbox 0x001931E4
ShaderValidationBalancedChildTree& merge_shader_validation_instruction_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, ShaderValidationBalancedChildTree& dependencies)
{
    build_shader_validation_instruction_dependencies(context, instruction);
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(instruction.instruction_set->nodes, &node);
    while (node != nullptr)
    {
        ShaderValidationInsertContext insert{dependencies.nodes, context.constructors.allocate};
        register_shader_validation_instruction(insert, node->value, context.sets.instruction_at);
        advance_shader_validation_arena_iterator(node);
    }
    return dependencies;
}

// Xbox 0x00193C44
void build_shader_validation_instruction_dependencies(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction)
{
    i32 state = -1;
    ShaderValidationChannelOrder order;
    ShaderValidationChannelMask used;
    GBTG_SEH_TRY_FINALLY
    {
        build_dependencies_body(context, instruction, state, order, used);
    }
    GBTG_SEH_FINALLY
    {
        if (GBTG_SEH_ABNORMAL() && state == 0)
        {
            destroy_shader_validation_channel_mask(used);
        }
    }
    GBTG_SEH_FINALLY_END
}

// Xbox 0x00193FE4
bool shader_validation_instruction_has_dependency(ShaderValidationDependencyContext& context,
    ShaderValidationInstruction& instruction, u32 other)
{
    build_shader_validation_instruction_dependencies(context, instruction);
    return find_shader_validation_instruction_node(instruction.instruction_set->nodes, other, context.sets.instruction_at) != nullptr;
}

// Xbox 0x00194002
bool shader_validation_instruction_priority_precedes(ShaderValidationDependencyContext& context, u32 first, u32 second)
{
    auto& first_instruction = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(first));
    auto& second_instruction = *const_cast<ShaderValidationInstruction*>(context.sets.instruction_at(second));
    build_shader_validation_instruction_dependencies(context, first_instruction);
    const i32 first_count = std::bit_cast<i32>(first_instruction.instruction_set_count);
    build_shader_validation_instruction_dependencies(context, second_instruction);
    const i32 second_count = std::bit_cast<i32>(second_instruction.instruction_set_count);
    if (first_count > second_count)
    {
        return true;
    }
    if (first_count < second_count)
    {
        return false;
    }
    const i32 first_sequence = std::bit_cast<i32>(first_instruction.sequence);
    const i32 second_sequence = std::bit_cast<i32>(second_instruction.sequence);
    if (first_sequence < second_sequence)
    {
        return true;
    }
    if (first_sequence > second_sequence)
    {
        return false;
    }
    return first < second;
}

// Xbox 0x00194047
void* select_shader_validation_list_priority(ShaderValidationDependencyContext& context,
    ShaderValidationListNode& first, ShaderValidationListNode* end)
{
    if (&first == end)
    {
        return nullptr;
    }
    for (ShaderValidationListNode* node = first.next; node != end; node = node->next)
    {
        if (!shader_validation_instruction_priority_precedes(context,
                context.sets.address_of(first.value), context.sets.address_of(node->value)))
        {
            void* value = first.value;
            first.value = node->value;
            node->value = value;
        }
    }
    return first.value;
}


}
