#pragma once
namespace gbtg
{
// Native mapping of the link at object+8 in DAT_0050E438.
struct SceneTickNode
{
    void (*tick)(SceneTickNode* self) = nullptr; // Object vtable+10.
    SceneTickNode* next = nullptr;
    void (*message)(SceneTickNode* self, const void* value) = nullptr; // Object vtable+C.
};
extern SceneTickNode* g_50e438;
}
