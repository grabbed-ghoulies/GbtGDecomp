#include "model_renderer_boot.h"

#include <cstring>

namespace gbtg
{
// Xbox 0x00158800
void initialize_runtime_node_registry(RuntimeNodeTypeRegistry& registry)
{
    registry.count = 0;
}

// Xbox 0x00158730
void register_runtime_node_type(RuntimeNodeTypeRegistry& registry, const char* name, u32 vtable)
{
    const size_t offset = size_t(registry.count) * 0x54;
    if (offset + 0x54 > registry.descriptors.size())
    {
        throw Error("node type registry is full");
    }
    std::memcpy(registry.descriptors.data() + offset + 0x50, &vtable, 4);
    size_t index = 0;
    char unit;
    do
    {
        unit = name[index];
        registry.descriptors[offset + index] = u8(unit);
        ++index;
    }
    while (unit != 0);
    ++registry.count;
}

// Xbox 0x00159A00
void link_runtime_shader_fragment(RuntimeRenderLight& list, RuntimeRenderLight& node)
{
    RuntimeRenderLight* head = &list;
    while (head->previous != nullptr)
    {
        head = head->previous;
    }
    RuntimeRenderLight* following = head->next;
    head->next = &node;
    node.previous = head;
    node.next = following;
    if (following != nullptr)
    {
        following->previous = &node;
    }
}

// Xbox 0x00158E00
void register_runtime_shader_fragment(u32 key, u32 first, u32 second, RuntimeShaderFragmentTable& table)
{
    const auto add = [&table](u32 stored, u32 program)
    {
        if (u32(table.count) >= table.nodes.size())
        {
            throw Error("shader fragment table is full");
        }
        RuntimeRenderLight& node = table.nodes[size_t(table.count)];
        table.programs[&node] = program;
        node.type = stored;
        link_runtime_shader_fragment(table.buckets[stored % 0xEF], node);
        ++table.count;
    };
    if (first != 0)
    {
        add(key | 0x80000000u, first);
    }
    if (second != 0)
    {
        add(key, second);
    }
}

// Xbox 0x00159720
void register_runtime_shader_fragments(RuntimeShaderFragmentTable& table)
{
    // Key, then the two vertex-shader program addresses, in the dump's call order.
    constexpr std::array<std::array<u32, 3>, 36> kFragments
    {{
        {0x0, 0x32E6C8, 0x32E6EC}, {0x1, 0x32E700, 0x32E768}, {0x21, 0x32E7B0, 0x32E898},
        {0x31, 0x32E960, 0x32EA88}, {0xA1, 0x32EB90, 0x32ECA8}, {0xB1, 0x32EDA0, 0x32EEF8},
        {0x221, 0x32F030, 0x32F198}, {0x331, 0x32F2E0, 0x32F4C8}, {0xAA1, 0x32F690, 0x32F858},
        {0xBB1, 0x32FA00, 0x32FC48}, {0x321, 0x32FE70, 0x330018}, {0xBA1, 0x3301A0, 0x3303A8},
        {0x2, 0x330590, 0x330638}, {0x3, 0x3306C0, 0x3307A8}, {0xA, 0x330870, 0x330948},
        {0xB, 0x330A00, 0x330B18}, {0x22, 0x330C10, 0x330D38}, {0x33, 0x330E40, 0x330FE8},
        {0xAA, 0x331170, 0x3312F8}, {0xBB, 0x331460, 0x331668}, {0x32, 0x331850, 0x3319B8},
        {0xBA, 0x331B00, 0x331CC8}, {0xE, 0x331E70, 0x331F78}, {0xEE, 0x332060, 0x332248},
        {0xEEE, 0x332410, 0x3326D8}, {0xF, 0x332980, 0x332AC8}, {0xFF, 0x332BF0, 0x332E58},
        {0xFFF, 0x3330A0, 0x333428}, {0xE1, 0x333790, 0x3338D8}, {0xEE1, 0x333A00, 0x333C28},
        {0xEEE1, 0x333E30, 0x334138}, {0xF1, 0x334420, 0x3345A8}, {0xFF1, 0x334710, 0x3349B8},
        {0xFFF1, 0x334C40, 0x335008}, {0x11, 0x3353B0, 0x335458}, {0x111, 0x3354E0, 0x3355C8},
    }};
    for (const auto& [key, first, second] : kFragments)
    {
        register_runtime_shader_fragment(key, first, second, table);
    }
}

// Xbox 0x00158CE0
void initialize_runtime_lighting(RuntimeModelRenderContext& renderer, RuntimeShaderFragmentTable& table)
{
    initialize_runtime_light(renderer.lights);
    for (auto& bucket : table.buckets)
    {
        initialize_runtime_light(bucket);
    }
    table.count = 0;
    renderer.lights_in_world_space = 1;
    renderer.lighting_flags = 0;
    register_runtime_shader_fragments(table);
}

// Xbox 0x00155A10
void initialize_runtime_model_renderer(RuntimeModelRenderContext& renderer, RuntimeNodeTypeRegistry& registry,
    RuntimeShaderFragmentTable& table)
{
    // Render state fields the host state models; +0x4C, +0x188, +0x18C, +0x198, +0x19C,
    // +0x1A8, +0x1AC, +0x1F0, +0x1FC and +0x200 have no native readers yet. +0x50 is the tree draw 0x00158A80,
    // which install_runtime_model_tree_dispatch binds.
    RuntimeModelRenderState& state = runtime_render_state(renderer);
    state.material_values = 0;
    state.material = 0;
    state.frustum = nullptr;
    state.texture_key = 0;
    state.material_vector = {0.f, 0.f, 0.f};
    state.directional_light_count = 0;
    state.color = {0, 0, 0, 0xFF};
    runtime_render_frame(renderer).world = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f,
        1.f};
    initialize_runtime_node_registry(registry);
    // Names DAT_00203B30.., vtables from DAT_0032E3B8.., in the dump's registration order.
    constexpr std::array<std::pair<const char*, u32>, 17> kTypes
    {{
        {"ndGroup", 0x32E4D8}, {"ndPixelShaderParam", 0x32E4CC}, {"ndPixelShader", 0x32E4AC},
        {"ndVertexShader", 0x32E4A0}, {"ndVertexBuffer", 0x32E494}, {"ndPushBuffer", 0x32E470},
        {"ndBGPushBuffer", 0x32E464}, {"ndSkeleton", 0x32E3D4}, {"ndVisibility", 0x32E458},
        {"ndTransform", 0x32E44C}, {"nd2D", 0x32E440}, {"ndShaderParam2", 0x32E3EC}, {"ndShader2", 0x32E434},
        {"ndMtxArray", 0x32E3B8}, {"ndRigidSkinIdx", 0x32E41C}, {"ndVtxDesc", 0x32E4A0},
        {"ndBlendShape", 0x32E410},
    }};
    for (const auto& [name, vtable] : kTypes)
    {
        register_runtime_node_type(registry, name, vtable);
    }
    initialize_runtime_lighting(renderer, table);
}
}
