#include "shader_validation_owner_sets.h"
#include "shader_validation_cleanup.h"
#if defined(_WIN32)
#include <excpt.h>
#else
#error Shader validator constructor unwind requires the approved Windows SEH backend.
#endif

namespace gbtg
{
namespace
{
bool readers_registered_body(const ShaderValidationOwner& owner, const ShaderValidationBalancedTree& registry, ShaderValidationChannelMask& mask,
    const ShaderValidationInstruction* instruction, u8 climb_derived, u8 climb_channel_owner, const ShaderValidationSetContext& context)
{
    if (pack_shader_validation_channel_mask(mask) == 0)
    {
        return true;
    }
    bool registered = true;
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
    while (node != nullptr)
    {
        const ShaderValidationChannelMask argument{0x00203790, mask.channels};
        if (shader_validation_operand_reads_masked_channel(*context.operand_at(node->value), argument))
        {
            const u32 operand = node->value;
            if (find_shader_validation_pass_node(registry, operand) == nullptr)
            {
                registered = false;
            }
            if (!registered)
            {
                if (instruction == nullptr)
                {
                    return false;
                }
                for (const ShaderValidationOperand* slot : instruction->operands)
                {
                    if (operand == context.address_of(slot))
                    {
                        registered = true;
                    }
                    // The slot's parent is dereferenced without a null test, as in retail.
                    if (operand == context.address_of(slot->parent->operand))
                    {
                        registered = true;
                    }
                }
                if (!registered)
                {
                    return false;
                }
            }
        }
        advance_shader_validation_balanced_iterator(&node);
    }
    if (climb_derived != 0 && owner.derived_owner != nullptr)
    {
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            if (owner.channels[channel] != 0)
            {
                mask.channels[channel] = 0;
            }
        }
        if (!shader_validation_owner_readers_registered(*owner.derived_owner, registry, ShaderValidationChannelMask{0x00203790, mask.channels},
                instruction, 1, 0, context))
        {
            return false;
        }
    }
    if (climb_channel_owner != 0 && owner.channel_owner != nullptr)
    {
        for (std::size_t channel = 0; channel < 4; ++channel)
        {
            if (owner.channel_owner->channels[channel] != 0)
            {
                mask.channels[channel] = 0;
            }
        }
        if (!shader_validation_owner_readers_registered(*owner.channel_owner, registry, ShaderValidationChannelMask{0x00203790, mask.channels},
                instruction, 0, 1, context))
        {
            return false;
        }
    }
    return true;
}

bool negate_writers_body(ShaderValidationOwner& owner, ShaderValidationBalancedChildTree& writers, const ShaderValidationSetContext& context,
    i32& state)
{
    writers.vtable_address = 0x002037d0;
    writers.nodes.root = nullptr;
    writers.nodes.count = 0;
    state = 0;
    ShaderValidationInsertContext insert{writers.nodes, context.allocate};
    for (const ShaderValidationChannelWriter writer : owner.channels)
    {
        if (writer == shader_validation_unwritten_channel)
        {
            return false;
        }
        if (writer != 0)
        {
            register_shader_validation_instruction(insert, context.address_of(reinterpret_cast<const void*>(writer)), context.instruction_at);
        }
    }
    ShaderValidationBalancedNode* node;
    find_shader_validation_tree_last(writers.nodes, &node);
    while (node != nullptr)
    {
        auto& writer = const_cast<ShaderValidationInstruction&>(*context.instruction_at(node->value));
        if (!negate_shader_validation_move_source(writer, &owner))
        {
            // Undo every earlier negation, walking back to the first set member.
            ShaderValidationBalancedNode* first;
            while (node != *find_shader_validation_tree_last(writers.nodes, &first))
            {
                retreat_shader_validation_node_iterator(&node);
                negate_shader_validation_move_source(const_cast<ShaderValidationInstruction&>(*context.instruction_at(node->value)), &owner);
            }
            return false;
        }
        advance_shader_validation_arena_iterator(node);
    }
    ShaderValidationBalancedNode* reader;
    find_shader_validation_tree_last(owner.diagnostics.nodes, &reader);
    while (reader != nullptr)
    {
        ShaderValidationOperand& operand = *context.operand_at(reader->value);
        operand.marker = operand.marker == 0 ? 1 : 0;
        advance_shader_validation_arena_iterator(reader);
    }
    return true;
}
}

// Xbox 0x00190E27
bool shader_validation_owner_readers_registered(const ShaderValidationOwner& owner, const ShaderValidationBalancedTree& registry,
    ShaderValidationChannelMask mask, const ShaderValidationInstruction* instruction, u8 climb_derived, u8 climb_channel_owner,
    const ShaderValidationSetContext& context)
{
    // registry is a balanced tree of retail operand addresses; owner registries map through context.operand_at.
    bool result = false;
    __try
    {
        result = readers_registered_body(owner, registry, mask, instruction, climb_derived, climb_channel_owner, context);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x00170A5F: FuncInfo 0x0032BAA8 destroys the by-value mask argument.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    return result;
}

// Xbox 0x00190F87
ShaderValidationChannelMask* shader_validation_unregistered_reader_channels(const ShaderValidationOwner& owner,
    ShaderValidationChannelMask& output, const ShaderValidationBalancedTree& registry,
    const ShaderValidationInstruction* instruction, const ShaderValidationSetContext& context)
{
    ShaderValidationChannelMask mask;
    construct_shader_validation_channel_mask(mask, 0);
    __try
    {
        for (u32 channel = 0; channel < 4; ++channel)
        {
            ShaderValidationChannelMask argument;
            construct_shader_validation_single_channel_mask(argument, channel);
            mask.channels[channel] = !shader_validation_owner_readers_registered(owner, registry, argument, instruction, 1, 1, context);
        }
        output.vtable_address = 0x00203790;
        output.channels = mask.channels;
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x001707EF: FuncInfo 0x0032B7F0 destroys the local mask.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    return &output;
}

// Xbox 0x00191417
bool move_shader_validation_masked_channels(ShaderValidationOwner& owner, ShaderValidationChannelMask mask,
    u8 climb, u8 propagate, const ShaderValidationSetContext& context)
{
    bool result = true;
    __try
    {
        for (u32 channel = 0; channel < 4; ++channel)
        {
            if (mask.channels[channel] != 0 && find_shader_validation_owner_channel(&owner, channel) != 0)
            {
                bool moved = false;
                for (u32 destination = 0; destination < 4 && !moved; ++destination)
                {
                    if (mask.channels[destination] == 0 && find_shader_validation_owner_channel(&owner, destination) == 0)
                    {
                        moved = swap_shader_validation_owner_channels(owner, channel, destination, climb, propagate, context.operand_at);
                    }
                }
                if (!moved)
                {
                    result = false;
                    break;
                }
            }
        }
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x00170823: FuncInfo 0x0032B848 destroys the by-value mask.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    return result;
}

namespace
{
bool link_owners_body(ShaderValidationOwner* owner, ShaderValidationOwner* other, u8 query_only,
    const ShaderValidationChannelMask& owner_mask, const ShaderValidationChannelMask& other_mask,
    const ShaderValidationSetContext& context)
{
    if (other == nullptr)
    {
        return true;
    }
    if (!move_shader_validation_masked_channels(*owner, ShaderValidationChannelMask{0x00203790, owner_mask.channels}, 1, 1, context))
    {
        return false;
    }
    if (!move_shader_validation_masked_channels(*other, ShaderValidationChannelMask{0x00203790, other_mask.channels}, 1, 1, context))
    {
        return false;
    }
    if (owner->derived_owner != nullptr)
    {
        return false;
    }
    if (owner->channel_owner != nullptr && other->channel_owner != nullptr)
    {
        return false;
    }
    if (query_only == 0)
    {
        if (owner->channel_owner == nullptr)
        {
            owner->channel_owner = other->channel_owner;
        }
        other->channel_owner = owner;
        owner->derived_owner = other;
    }
    return true;
}

bool reorder_owner_channels_body(ShaderValidationOwner& owner, ShaderValidationChannelOrder& order,
    ShaderValidationChannelOrder& retained_inverse, ShaderValidationChannelOrder& remaining, i32& state,
    const ShaderValidationSetContext& context)
{
    if (owner.kind != 1 && owner.kind != 6)
    {
        return false;
    }
    clear_shader_validation_repeated_channels(order);
    initialize_shader_validation_channel_order(retained_inverse);
    while (!shader_validation_channel_order_covers_all(order))
    {
        const u32 uncovered = find_shader_validation_uncovered_channel(order);
        const u32 missing = find_shader_validation_channel_order_index(order, 4);
        retained_inverse.channels[uncovered] = missing == 4 ? 4 : order.channels[missing];
        // After duplicate removal, an uncovered channel always has a missing position.
        order.channels[missing] = uncovered;
    }
    invert_shader_validation_channel_order(order, remaining);
    state = 2;
    while (!shader_validation_channel_order_keeps_all(remaining))
    {
        const u32 first = find_shader_validation_moved_channel(remaining);
        const u32 second = find_shader_validation_channel_order_index(remaining, first);
        bool swapped = false;
        if (owner.derived_owner != nullptr)
        {
            swapped = swap_shader_validation_owner_channels(*owner.derived_owner, first, second, 1, 1, context.operand_at);
        }
        else
        {
            const ShaderValidationChannelWriter first_writer = owner.channels[first];
            const ShaderValidationChannelWriter second_writer = owner.channels[second];
            if (first_writer == shader_validation_unwritten_channel || second_writer == shader_validation_unwritten_channel)
            {
                return false;
            }
            if (owner.channel_owner != nullptr &&
                !swap_shader_validation_owner_channels(*owner.channel_owner, first, second, 0, 1, context.operand_at))
            {
                return false;
            }
            if (first_writer == second_writer)
            {
                if (second_writer == 0)
                {
                    swapped = true;
                }
                else
                {
                    swapped = swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(second_writer),
                        &owner, first, second, 0);
                }
            }
            else if (second_writer == 0)
            {
                swapped = swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(first_writer),
                    &owner, first, second, 0);
                if (swapped)
                {
                    owner.channels[second] = first_writer;
                    owner.channels[first] = 0;
                }
            }
            else if (first_writer == 0)
            {
                swapped = swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(second_writer),
                    &owner, first, second, 0);
                if (swapped)
                {
                    owner.channels[first] = second_writer;
                    owner.channels[second] = 0;
                }
            }
            else if (swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(second_writer),
                &owner, first, second, 0))
            {
                swapped = swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(first_writer),
                    &owner, first, second, 0);
                if (swapped)
                {
                    owner.channels[first] = second_writer;
                    owner.channels[second] = first_writer;
                }
                else
                {
                    swap_shader_validation_destination_channels(*reinterpret_cast<ShaderValidationInstruction*>(second_writer),
                        &owner, first, second, 0);
                }
            }
            if (swapped)
            {
                ShaderValidationBalancedNode* node;
                find_shader_validation_tree_last(owner.diagnostics.nodes, &node);
                while (node != nullptr)
                {
                    ShaderValidationOperand& operand = *context.operand_at(node->value);
                    for (u32 index = 0; index < 4; ++index)
                    {
                        if (operand.channel_order.channels[index] == first)
                        {
                            operand.channel_order.channels[index] = second;
                        }
                        else if (operand.channel_order.channels[index] == second)
                        {
                            operand.channel_order.channels[index] = first;
                        }
                    }
                    advance_shader_validation_arena_iterator(node);
                }
            }
            else if (owner.channel_owner != nullptr)
            {
                swap_shader_validation_owner_channels(*owner.channel_owner, first, second, 0, 1, context.operand_at);
            }
        }
        if (!swapped)
        {
            return false;
        }
        const u32 temporary = remaining.channels[first];
        remaining.channels[first] = remaining.channels[second];
        remaining.channels[second] = temporary;
    }
    return true;
}

bool owner_dependencies_registered_body(const ShaderValidationOwner& owner, const ShaderValidationPass& pass,
    const ShaderValidationBalancedTree& writers, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, const ShaderValidationChannelMask& mask,
    const ShaderValidationSetContext& context)
{
    const ShaderValidationOwner* derived = owner.derived_owner;
    if (derived != nullptr)
    {
        if (!shader_validation_owner_readers_registered(*derived, readers, ShaderValidationChannelMask{0x00203790, mask.channels},
                instruction, 1, 1, context))
        {
            return false;
        }
        for (u32 channel = 0; channel < 4; ++channel)
        {
            const ShaderValidationChannelWriter writer = derived->channels[channel];
            if (mask.channels[channel] != 0 && writer != 0 && writer != shader_validation_unwritten_channel)
            {
                if (find_shader_validation_instruction_node(writers, context.address_of(reinterpret_cast<const void*>(writer)),
                        context.instruction_at) == nullptr)
                {
                    return false;
                }
            }
        }
    }
    if (owner.kind == 5 && pass.owner != nullptr)
    {
        if (!shader_validation_owner_readers_registered(*pass.owner, readers, ShaderValidationChannelMask{0x00203790, mask.channels},
                instruction, 1, 1, context))
        {
            return false;
        }
    }
    return true;
}
}

// Xbox 0x00191DF3
bool link_shader_validation_owners(ShaderValidationOwner* owner, ShaderValidationOwner* other, u8 query_only,
    ShaderValidationChannelMask owner_mask, ShaderValidationChannelMask other_mask,
    const ShaderValidationSetContext& context)
{
    bool result = false;
    __try
    {
        result = link_owners_body(owner, other, query_only, owner_mask, other_mask, context);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // FuncInfo 0x0032BC64: state 1 (+0x10 mask) -> state 0 (+0x18 mask).
            // Xbox 0x0017084F
            destroy_shader_validation_channel_mask(owner_mask);
            // Xbox 0x00170847
            destroy_shader_validation_channel_mask(other_mask);
        }
    }
    return result;
}

// Xbox 0x001911A8
bool reorder_shader_validation_owner_channels(ShaderValidationOwner& owner, ShaderValidationChannelOrder order,
    const ShaderValidationSetContext& context)
{
    ShaderValidationChannelOrder retained_inverse;
    ShaderValidationChannelOrder remaining;
    i32 state = 0;
    bool result = false;
    __try
    {
        result = reorder_owner_channels_body(owner, order, retained_inverse, remaining, state, context);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // FuncInfo 0x0032B824: states 2 -> 1 -> 0 -> -1.
            switch (state)
            {
            case 2:
                // Xbox 0x00170811
                destroy_shader_validation_channel_order(remaining);
                [[fallthrough]];
            case 1:
                // Xbox 0x00170809
                destroy_shader_validation_channel_order(retained_inverse);
                [[fallthrough]];
            case 0:
                // Xbox 0x00170801
                destroy_shader_validation_channel_order(order);
                break;
            }
        }
    }
    return result;
}

// Xbox 0x00191D2D
bool shader_validation_owner_dependencies_registered(const ShaderValidationOwner& owner, const ShaderValidationPass& pass,
    const ShaderValidationBalancedTree& writers, const ShaderValidationBalancedTree& readers,
    const ShaderValidationInstruction* instruction, ShaderValidationChannelMask mask,
    const ShaderValidationSetContext& context)
{
    bool result = false;
    __try
    {
        result = owner_dependencies_registered_body(owner, pass, writers, readers, instruction, mask, context);
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x00170835: FuncInfo 0x0032B86C destroys the by-value mask.
            destroy_shader_validation_channel_mask(mask);
        }
    }
    return result;
}

// Xbox 0x00192B84
bool negate_shader_validation_owner_writers(ShaderValidationOwner& owner, const ShaderValidationSetContext& context)
{
    if (owner.kind == 2 || owner.kind == 3 || owner.channel_owner != nullptr || owner.derived_owner != nullptr)
    {
        return false;
    }
    ShaderValidationBalancedChildTree writers{};
    i32 state = -1;
    bool result = false;
    __try
    {
        result = negate_writers_body(owner, writers, context, state);
        state = -1;
        writers.vtable_address = 0x002037d0;
        destroy_shader_validation_balanced_nodes_18f2aa(writers.nodes.root, context.deleting_destructor);
    }
    __finally
    {
        if (AbnormalTermination() && state == 0)
        {
            // Xbox 0x00170890 -> 0x001909CE: the local set's destructor.
            writers.vtable_address = 0x002037d0;
            clear_shader_validation_balanced_tree_190322(writers, context.deleting_destructor);
        }
    }
    return result;
}

// Xbox 0x00193C2A
bool shader_validation_instruction_accepts_order(const ShaderValidationInstruction& instruction, u32 slot, ShaderValidationChannelOrder order)
{
    // Retail sets up an EH frame and returns false (RET 0x18); nothing in the body can raise.
    (void)instruction;
    (void)slot;
    bool result = false;
    __try
    {
        result = false;
    }
    __finally
    {
        if (AbnormalTermination())
        {
            // Xbox 0x00170A29: FuncInfo 0x0032BB2C destroys the by-value order.
            destroy_shader_validation_channel_order(order);
        }
    }
    return result;
}

// Xbox 0x00194311
bool shader_validation_operands_equivalent(const ShaderValidationOperand& operand, const ShaderValidationOperand& other,
    const ShaderValidationOwner* active_owner)
{
    if (!shader_validation_owners_equivalent(operand.parent, other.parent, active_owner))
    {
        return false;
    }
    if (operand.marker != other.marker)
    {
        return false;
    }
    if (shader_validation_channel_orders_compatible(operand.channel_order, ShaderValidationChannelOrder{0x00203794, other.channel_order.channels}))
    {
        return true;
    }
    if (other.instruction != nullptr &&
        shader_validation_instruction_accepts_order(*other.instruction, operand.kind,
            ShaderValidationChannelOrder{0x00203794, operand.channel_order.channels}))
    {
        return true;
    }
    // Retail passes this operand's kind with the other operand's order here as well.
    if (operand.instruction != nullptr &&
        shader_validation_instruction_accepts_order(*operand.instruction, operand.kind,
            ShaderValidationChannelOrder{0x00203794, other.channel_order.channels}))
    {
        return true;
    }
    return false;
}

// Xbox 0x00193EE2
bool retire_shader_validation_register(ShaderValidationPass& pass, i32 kind, const ShaderValidationBalancedTree& registry,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor,
    const std::function<void*(std::size_t, u32)>& allocate, const ShaderValidationSetContext& context)
{
    if (kind < 0 || kind > 12)
    {
        return true;
    }
    const ShaderValidationPassNode* node = find_shader_validation_pass_node(pass.registers.tree.nodes, static_cast<u32>(kind));
    if (node == nullptr)
    {
        return true;
    }
    auto* owner = static_cast<ShaderValidationOwner*>(node->data);
    ShaderValidationChannelMask mask;
    construct_shader_validation_channel_mask(mask, 0xF);
    if (!shader_validation_owner_readers_registered(*owner, registry, mask, nullptr, 1, 1, context))
    {
        return false;
    }
    erase_shader_validation_pass_key(pass.registers.tree.nodes, static_cast<u32>(kind), deleting_destructor);
    while (true)
    {
        owner = owner->channel_owner;
        if (owner == nullptr)
        {
            return true;
        }
        construct_shader_validation_channel_mask(mask, 0xF);
        if (!shader_validation_owner_readers_registered(*owner, registry, mask, nullptr, 1, 1, context))
        {
            break;
        }
    }
    owner->kind = 1;
    owner->source = static_cast<u32>(kind);
    ShaderValidationPassNode* result;
    set_shader_validation_pass_value(pass.registers.tree.nodes, allocate, &result, static_cast<u32>(kind), owner);
    return false;
}
}
