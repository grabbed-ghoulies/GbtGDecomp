#include "shader_validation_arena.h"

namespace gbtg
{
// Xbox 0x0018F0C2
void destroy_shader_validation_expression_arena(ShaderValidationArenaNode* node)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_expression_arena(node->first);
        destroy_shader_validation_expression_arena(node->second);
        node->vtable->destroy(*node, 1);
    }
}

// Xbox 0x0018F119
void destroy_shader_validation_instruction_arena(ShaderValidationArenaNode* node)
{
    if (node != nullptr)
    {
        node->parent = nullptr;
        destroy_shader_validation_instruction_arena(node->first);
        destroy_shader_validation_instruction_arena(node->second);
        node->vtable->destroy(*node, 1);
    }
}

// Xbox 0x001906A1
void reset_shader_validation_expression_arena(ShaderValidationArenaStorage& storage)
{
    destroy_shader_validation_expression_arena(storage.root);
    storage.root = nullptr;
    storage.count = 0;
}

// Xbox 0x001906F1
void reset_shader_validation_instruction_arena(ShaderValidationArenaStorage& storage)
{
    destroy_shader_validation_instruction_arena(storage.root);
    storage.root = nullptr;
    storage.count = 0;
}

// Xbox 0x0018F0EB
void advance_shader_validation_arena_iterator(ShaderValidationArenaNode*& node)
{
    ShaderValidationArenaNode* next = node->second;
    ShaderValidationArenaNode* current = node;
    if (next != nullptr)
    {
        do
        {
            current = next;
            next = current->first;
        } while (next != nullptr);
        node = current;
        return;
    }
    ShaderValidationArenaNode* previous;
    do
    {
        previous = current;
        current = previous->parent;
        if (current == nullptr)
        {
            break;
        }
    } while (current->second == previous);
    node = previous->parent;
}

// Xbox 0x0018F142
void erase_shader_validation_list_node(ShaderValidationListStorage& storage, ShaderValidationListNode* node)
{
    if (node == storage.first)
    {
        storage.first = storage.first->next;
    }
    if (node == storage.last)
    {
        storage.last = storage.last->previous;
    }
    if (node != nullptr)
    {
        node->vtable->destroy(*node, 1);
    }
    --storage.count;
}
}
