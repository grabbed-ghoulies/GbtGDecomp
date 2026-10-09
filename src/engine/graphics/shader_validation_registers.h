#pragma once
#include "shader_validation_containers.h"
#include "shader_validation_registration.h"
#include <functional>

namespace gbtg
{
// Pass-layout tree object with a registered sub-object at +0x10 (0x14 retail bytes).
// 0x001934A5 unregisters the sub-object's address from the DAT_001a4d48 registry.
struct ShaderValidationRegisteredPassTree
{
    ShaderValidationPassTreeObject tree; // Retail +0x00..+0x0c.
    ShaderValidationRegisteredObject registered; // Retail +0x10.
};

// Validator pass object (vtable 0x00203818; 0x00196739 builds the 0x98-byte derived object).
// Only the fields used by translated functions are declared.
struct ShaderValidationPass
{
    ShaderValidationRegisteredObject base; // Retail +0x00.
    ShaderValidationList instructions; // Retail +0x04..+0x10.
    ShaderValidationRegisteredPassTree values; // Retail +0x14.
    ShaderValidationRegisteredPassTree registers; // Retail +0x28; register kinds 0..12.
    ShaderValidationRegisteredPassTree special; // Retail +0x3c; key 0x100 is kind 12.
    ShaderValidationOwner* owner; // Retail +0x50; parent passed by 0x001935E6.
};

void update_shader_validation_pass_ancestor_heights(ShaderValidationPassNode* node); // Xbox 0x00190554
void rotate_shader_validation_pass_tree_left(ShaderValidationPassNode& node); // Xbox 0x0019060B
void rotate_shader_validation_pass_tree_right(ShaderValidationPassNode& node); // Xbox 0x00190656
ShaderValidationOwner* find_shader_validation_register_owner(const ShaderValidationPass& pass, u32 kind); // Xbox 0x00191E8F
i32 find_shader_validation_owner_register_kind(const ShaderValidationPass& pass,
    const ShaderValidationOwner* owner); // Xbox 0x00191F00
void balance_shader_validation_pass_ancestors(ShaderValidationPassNode& node); // Xbox 0x00190BDD
bool shader_validation_owner_chain_has_kind_one(const ShaderValidationPass& pass,
    const ShaderValidationOwner* owner); // Xbox 0x00192629
void rebalance_shader_validation_pass_tree(ShaderValidationPassTree& tree, ShaderValidationPassNode& node); // Xbox 0x00191990
ShaderValidationPassNode* join_shader_validation_pass_subtrees(ShaderValidationPassNode* first,
    ShaderValidationPassNode* second); // Xbox 0x001919BD
ShaderValidationPassNode* erase_shader_validation_pass_node(ShaderValidationPassTree& tree, ShaderValidationPassNode* node,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x00192469
ShaderValidationPassNode* erase_shader_validation_pass_key(ShaderValidationPassTree& tree, u32 key,
    const std::function<void(ShaderValidationPassNode&, u32)>& deleting_destructor); // Xbox 0x001936B5
bool shader_validation_owners_equivalent(const ShaderValidationOwner* owner, const ShaderValidationOwner* other,
    const ShaderValidationOwner* active_owner); // Xbox 0x0018F4A9
}
