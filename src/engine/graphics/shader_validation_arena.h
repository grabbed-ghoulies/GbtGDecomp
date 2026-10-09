#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct ShaderValidationArenaNode;
struct ShaderValidationArenaVtable
{
    void (*destroy)(ShaderValidationArenaNode&, u32);
};

struct ShaderValidationArenaNode
{
    const ShaderValidationArenaVtable* vtable; // Retail +0x00.
    void* value; // Retail +0x04.
    ShaderValidationArenaNode* parent; // Retail +0x08.
    ShaderValidationArenaNode* first; // Retail +0x0c.
    ShaderValidationArenaNode* second; // Retail +0x10.
};

struct ShaderValidationArenaStorage
{
    ShaderValidationArenaNode* root; // DAT_001a4d50 or DAT_001a4d60.
    u32 count; // DAT_001a4d54 or DAT_001a4d64.
};

struct ShaderValidationListNode;
struct ShaderValidationListVtable
{
    void (*destroy)(ShaderValidationListNode&, u32);
};

struct ShaderValidationListNode
{
    const ShaderValidationListVtable* vtable; // Retail +0x00.
    void* value; // Retail +0x04.
    ShaderValidationListNode* previous; // Retail +0x08.
    ShaderValidationListNode* next; // Retail +0x0c.
};

struct ShaderValidationListStorage
{
    ShaderValidationListNode* first; // Retail storage +0x04.
    ShaderValidationListNode* last; // Retail storage +0x08.
    u32 count; // Retail storage +0x0c.
};

void destroy_shader_validation_expression_arena(ShaderValidationArenaNode* node); // Xbox 0x0018F0C2
void destroy_shader_validation_instruction_arena(ShaderValidationArenaNode* node); // Xbox 0x0018F119
void reset_shader_validation_expression_arena(ShaderValidationArenaStorage& storage); // Xbox 0x001906A1
void reset_shader_validation_instruction_arena(ShaderValidationArenaStorage& storage); // Xbox 0x001906F1
void advance_shader_validation_arena_iterator(ShaderValidationArenaNode*& node); // Xbox 0x0018F0EB
void erase_shader_validation_list_node(ShaderValidationListStorage& storage, ShaderValidationListNode* node); // Xbox 0x0018F142
}
