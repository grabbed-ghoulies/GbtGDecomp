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
}
