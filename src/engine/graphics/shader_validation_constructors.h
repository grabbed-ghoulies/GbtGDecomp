#pragma once
#include <cstdint>
#include "shader_validation_cleanup.h"
#include "shader_validation_erase.h"
#include "../runtime/native_cpp_exception.h"

namespace gbtg
{
struct ShaderValidationOwner;
struct ShaderValidationInstruction;

// An owner channel holds 0, the retail 0xFFFFFFFF marker, or the instruction that writes it.
using ShaderValidationChannelWriter = std::uintptr_t;
inline constexpr ShaderValidationChannelWriter shader_validation_unwritten_channel = ~ShaderValidationChannelWriter{0};

struct ShaderValidationOperand
{
    ShaderValidationRegisteredObject base; // Retail +0x00.
    ShaderValidationInstruction* instruction; // Retail +0x04; null only for an owner's kind-3 operand.
    ShaderValidationOwner* owner; // Retail +0x08.
    u32 kind; // Retail +0x0c; operand slot 0..2, or 3 for an owner's operand.
    ShaderValidationOwner* parent; // Retail +0x10.
    u8 marker; // Retail +0x14.
    ShaderValidationChannelOrder channel_order; // Retail +0x18.
};

struct ShaderValidationBalancedChildTree
{
    u32 vtable_address; // Retail +0x00.
    u32 retained; // Retail +0x04.
    ShaderValidationBalancedTree nodes; // Retail root +0x08, count +0x0c.
};

struct ShaderValidationOwner
{
    ShaderValidationRegisteredObject base; // Retail +0x00.
    u32 kind; // Retail +0x04.
    u32 source; // Retail +0x08.
    ShaderValidationOperand* operand; // Retail +0x0c.
    ShaderValidationBalancedChildTree diagnostics; // Retail +0x10.
    std::array<ShaderValidationChannelWriter, 4> channels; // Retail +0x20.
    ShaderValidationOwner* derived_owner; // Retail +0x30; set by 0x0018E76F/0x0018F4F7.
    ShaderValidationOwner* channel_owner; // Retail +0x34; 0x0018E7AF follows this chain.
};

struct ShaderValidationConstructorContext
{
    ShaderValidationInsertContext& objects;
    ShaderValidationEraseContext& object_erase;
    const RuntimeCppExceptionContext& exceptions;
    ShaderValidationOwner*& active_owner; // DAT_001a4d34.
    std::function<void*(std::size_t, u32)> allocate;
    std::function<void(void*, u32)> release;
    std::function<u32(const void*)> retail_address; // Actual owning address map, never truncated host pointers.
    std::function<ShaderValidationInsertContext&(ShaderValidationOwner&)> diagnostic_registry; // Must alias that owner's diagnostics.nodes.
    std::function<void(ShaderValidationBalancedNode&, u32)> deleting_destructor; // Genuine node vtable slot +0x00.
};

void destroy_shader_validation_child_nodes(ShaderValidationBalancedNode* node, const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x0018F281
void clear_shader_validation_child_tree(ShaderValidationBalancedChildTree& tree, const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x001901D6
void destroy_shader_validation_child_tree(ShaderValidationBalancedChildTree& tree, const std::function<void(ShaderValidationBalancedNode&, u32)>& deleting_destructor); // Xbox 0x001909A6
ShaderValidationOperand* construct_shader_validation_operand(ShaderValidationConstructorContext& context, ShaderValidationOperand& operand,
    u32 object_address, ShaderValidationInstruction* instruction, ShaderValidationOwner* owner, u32 kind, ShaderValidationOwner* parent,
    u8 marker); // Xbox 0x00192934
ShaderValidationOwner* construct_shader_validation_owner(ShaderValidationConstructorContext& context, ShaderValidationOwner& owner,
    u32 object_address, u32 kind, u32 source, ShaderValidationOwner* parent, ShaderValidationChannelWriter channel_value,
    ShaderValidationChannelMask mask); // Xbox 0x00192A54
}
