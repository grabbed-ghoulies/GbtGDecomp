#include "gbtg/model_runtime.h"
#include "gbtg/model_render.h"
#include "gbtg/resource_error.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <bit>
#include <cmath>

using namespace gbtg;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        ModelPoseBuffer pose_fixture;
        pose_fixture.count = 1;
        pose_fixture.dirty = true;
        pose_fixture.matrices.push_back(AnimationMatrix{2.f, 0.f, 0.f, 3.f,
            0.f, 3.f, 0.f, 4.f, 0.f, 0.f, 4.f, 5.f});
        require(&model_pose_matrix(pose_fixture, 0) == &pose_fixture.matrices[0],
            "CPU pose lookup must return the original matrix");
        pose_fixture.packed_matrices.push_back(std::array<float, 16>{2.f, 0.f, 0.f, 0.f,
            0.f, 3.f, 0.f, 0.f, 0.f, 0.f, 4.f, 0.f, 3.f, 4.f, 5.f, 1.f});
        pose_fixture.dirty = false;
        require(model_pose_matrix(pose_fixture, 0) == pose_fixture.matrices[0],
            "packed pose lookup changed matrix ordering");
        const AnimationMatrix world_fixture{1.f, 0.f, 0.f, 10.f,
            0.f, 1.f, 0.f, 20.f, 0.f, 0.f, 1.f, 30.f};
        Joint joint_fixture{};
        joint_fixture.world = Vec3{1.f, 2.f, 3.f};
        const auto joint_world = model_joint_world_matrix(pose_fixture, 0, joint_fixture, world_fixture);
        require(joint_world[3] == 15.f && joint_world[7] == 30.f && joint_world[11] == 47.f,
            "joint world transform lost bind translation or multiplication order");
        ModelBoneBinding bone_fixture;
        bone_fixture.matrix = AnimationMatrix{1.f, 0.f, 0.f, 1.f,
            0.f, 1.f, 0.f, 2.f, 0.f, 0.f, 1.f, 3.f};
        AnimationMatrix bone_world = world_fixture;
        model_bone_world_matrix(bone_fixture, nullptr, bone_world, bone_world);
        require(bone_world[3] == 11.f && bone_world[7] == 22.f && bone_world[11] == 33.f,
            "unbound bone transform changed alias handling");
        bone_fixture.joint = 0;
        model_bone_world_matrix(bone_fixture, &pose_fixture, world_fixture, bone_world);
        require(bone_world == joint_world, "bound bone transform changed joint/local composition");
        Vec3 bone_point;
        model_bone_world_point(bone_fixture, &pose_fixture, &world_fixture, bone_point);
        require(bone_point.x == 15.f && bone_point.y == 30.f && bone_point.z == 47.f,
            "bone point changed joint and world transform order");
        bone_fixture.joint = -2;
        model_bone_world_point(bone_fixture, nullptr, nullptr, bone_point);
        require(bone_point.x == 1.f && bone_point.y == 2.f && bone_point.z == 3.f,
            "negative bone index must skip pose lookup without a world matrix");
        std::vector<int> copies;
        auto copy = [&](const RuntimeModelNode& node, RuntimeModelInstanceStorage& instance)
        {
            copies.push_back(node.type);
            instance.bytes[node.instance_offset] = u8(node.type + 10);
        };
        const std::array<RuntimeModelNodeType, 3> types
        {
            RuntimeModelNodeType{"ndRoot", copy}, RuntimeModelNodeType{"ndChild", copy},
            RuntimeModelNodeType{"ndSkeleton", copy}
        };
        RuntimeModelNode sibling{12, 2, &types[2], nullptr, nullptr};
        RuntimeModelNode grandchild{8, 2, &types[2], nullptr, nullptr};
        RuntimeModelNode child{4, 1, &types[1], &grandchild, nullptr};
        RuntimeModelNode root{0, 0, &types[0], &child, &sibling};
        std::array<RuntimeModelNode*, 1> roots{&root};
        const std::array<RuntimeModelNamedEntry, 2> entries
        {
            RuntimeModelNamedEntry{"first", &child}, RuntimeModelNamedEntry{"last", &sibling}
        };
        const RuntimeModelNameTable names{entries};
        RuntimeModelTree tree{16, roots, &names};
        require(find_runtime_model_node(&root, 2) == &grandchild &&
                find_runtime_model_node(&root, 1) == &child &&
                find_runtime_model_node(&root, 9) == nullptr,
                "node lookup changed depth-first ordering or miss behavior");
        const RuntimeModelNodeType* matched = nullptr;
        require(find_runtime_model_type(types, "ndSkeleton", matched) == 2 && matched == &types[2],
                "model type name lookup did not return the exact index/record");
        require(find_runtime_model_type(types, "ndskeleton", matched) == 0 && matched == nullptr,
                "model type name lookup became case-insensitive");
        require(find_runtime_model_node_by_name(tree, 0, "ndSkeleton", types) == &grandchild &&
                find_runtime_model_node_by_name(tree, 0, "missing", types) == &root,
                "name-to-node lookup changed the original zero-index miss behavior");
        std::array<u8, 16> instance{};
        RuntimeModelInstanceStorage model_storage{instance};
        copy_runtime_model_tree(tree, model_storage);
        require(runtime_model_instance_size(tree) == 16 &&
                copies == std::vector<int>{0, 1, 2, 2} &&
                instance[0] == 10 && instance[4] == 11 && instance[8] == 12 && instance[12] == 12,
                "model copy dispatch lost parent/child/sibling order or destination offsets");
        RuntimeModelNode* named = &root;
        require(find_runtime_model_named_node(tree, "last", named) && named == &sibling,
                "named node lookup returned the wrong node");
        require(!find_runtime_model_named_node(tree, "LAST", named) && named == &sibling,
                "named miss changed output pointer or matched a different case");
        const char* entry_name = nullptr;
        runtime_model_named_entry(tree, 0, entry_name, named);
        require(std::strcmp(entry_name, "first") == 0 && named == &child &&
                runtime_model_named_count(tree) == 2,
                "named entry/count access differs");
        std::array<const char*, 48> auxiliary_names;
        auxiliary_names.fill("absent");
        auxiliary_names[0] = "first";
        auxiliary_names[47] = "last";
        std::array<float*, 48> targets;
        targets.fill(reinterpret_cast<float*>(instance.data()));
        bind_model_auxiliary_targets(targets, tree, instance, auxiliary_names);
        require(targets[0] == reinterpret_cast<float*>(instance.data() + 4) &&
                targets[1] == nullptr && targets[47] == reinterpret_cast<float*>(instance.data() + 12),
                "auxiliary binding lost first/final offsets or absent-node clearing");
        tree.names = nullptr;
        named = &root;
        require(runtime_model_named_count(tree) == 0 &&
                !find_runtime_model_named_node(tree, "first", named) && named == &root,
                "absent name table changed count or lookup result/output");
        std::array<u8, 0x64> node_bytes{};
        for (size_t byte = 0x24; byte < node_bytes.size(); ++byte)
        {
            node_bytes[byte] = u8(byte);
        }
        RuntimeModelNode copied_node{};
        copied_node.instance_offset = 4;
        copied_node.copy_offset = 16;
        copied_node.bytes = node_bytes;
        std::array<u8, 256> copied_instance;
        copied_instance.fill(0xCD);
        copy_runtime_node_transform(copied_node, copied_instance);
        require(copied_instance[4] == 0xCD && copied_instance[16] == 0 &&
                copied_instance[20] == 0 && copied_instance[21] == 0xCD &&
                std::memcmp(copied_instance.data() + 24, node_bytes.data() + 0x24, 64) == 0,
                "transform copy used the auxiliary offset or changed byte/word copy boundaries");
        copied_instance.fill(0xCD);
        copy_runtime_node_visibility(copied_node, copied_instance);
        require(copied_instance[16] == 0 && copied_instance[24] == 1 && copied_instance[25] == 0xCD,
                "visibility copy changed its flag or adjacent bytes");
        copied_instance.fill(0xCD);
        copy_runtime_node_bg_push_buffer(copied_node, copied_instance);
        u32 copied_word;
        std::memcpy(&copied_word, copied_instance.data() + 20, 4);
        require(copied_word == 0xFFFFFFFF && copied_instance[16] == 0 && copied_instance[24] == 0xCD,
                "background push-buffer copy changed its unset value or base word");
        const i32 matrix_count = 2;
        std::memcpy(node_bytes.data() + 0x30, &matrix_count, 4);
        copied_instance.fill(0xCD);
        copy_runtime_node_matrix_array(copied_node, copied_instance);
        require(copied_instance[16] == 0 && copied_instance[148] == 0 &&
                copied_instance[149] == 0 && copied_instance[147] == 0xCD && copied_instance[150] == 0xCD,
                "matrix-array copy changed the post-matrix flags or their count");
        std::array<std::array<u8, 0x48>, 2> initial_parameters;
        initial_parameters[0].fill(0xAB);
        initial_parameters[1].fill(0xEF);
        RuntimeShaderCpu initial_shader;
        initial_shader.initial_parameters = initial_parameters;
        initial_shader.zero_word_count = 3;
        initial_shader.instance_size = 8 + 12 + 2 * 0x44;
        std::array<u8, 8 + 12 + 2 * 0x44> initialized_shader_storage;
        initialized_shader_storage.fill(0xCD);
        RuntimeShaderInstance initialized_shader;
        initialize_runtime_shader_instance(initial_shader, initialized_shader, initialized_shader_storage);
        require(runtime_shader_instance_size(initial_shader) == initialized_shader_storage.size() &&
                initialized_shader.zero_words.data() == reinterpret_cast<u32*>(initialized_shader_storage.data() + 8) &&
                initialized_shader.zero_words[0] == 0 && initialized_shader.zero_words[2] == 0 &&
                initialized_shader.parameters[0].bytes[0] == 0xAB &&
                initialized_shader.parameters[1].bytes[0x3F] == 0xEF &&
                initialized_shader.parameters[1].bytes[0x40] == 0 &&
                initialized_shader.parameters[1].bytes[0x41] == 0xCD,
                "shader instance initialization lost header offsets, strides, flag or retained tail bytes");
        std::array<u8, 512> shader_node_storage;
        shader_node_storage.fill(0xCD);
        copied_node.shader_cpu = &initial_shader;
        copied_node.secondary_shader_cpu = &initial_shader;
        RuntimeShaderNodeInstance shader_node_instance;
        copy_runtime_node_shader_parameters(copied_node, shader_node_instance, shader_node_storage);
        require(shader_node_instance.primary == &shader_node_instance.primary_instance &&
                shader_node_instance.secondary == &shader_node_instance.secondary_instance &&
                shader_node_instance.primary_storage == shader_node_storage.data() + 16 + 12 &&
                shader_node_instance.primary->parameters.data() == reinterpret_cast<RuntimeShaderParameter*>(
                    shader_node_storage.data() + 16 + 12 + 8 + 12) &&
                shader_node_instance.secondary->parameters.data() == reinterpret_cast<RuntimeShaderParameter*>(
                    shader_node_storage.data() + 16 + 12 + initial_shader.instance_size + 8 + 12),
                "shader node copy changed primary/secondary storage offsets or initialization");
        copied_node.secondary_shader_cpu = nullptr;
        RuntimeShaderInstance retained_secondary;
        shader_node_instance.secondary = &retained_secondary;
        copy_runtime_node_shader_parameters(copied_node, shader_node_instance, shader_node_storage);
        require(shader_node_instance.secondary == &retained_secondary,
                "absent secondary shader CPU changed the retained runtime pointer");
        RuntimeShaderCpu empty_shader_cpu;
        copied_node.secondary_shader_cpu = &empty_shader_cpu;
        copy_runtime_node_shader_parameters(copied_node, shader_node_instance, shader_node_storage);
        require(shader_node_instance.secondary == nullptr,
                "zero-size secondary shader CPU did not clear its runtime pointer");
        copied_node.shader_cpu = &empty_shader_cpu;
        copied_node.secondary_shader_cpu = nullptr;
        copy_runtime_node_shader_parameters(copied_node, shader_node_instance, shader_node_storage);
        require(shader_node_instance.primary == nullptr && shader_node_instance.primary_storage == nullptr,
                "zero-size primary shader CPU did not clear its runtime pointer");
        const std::array<u8, 5> morph_data{1, 2, 3, 4, 5};
        const std::array<RuntimeBlendshapeVertexBuffer, 3> morph_buffers
        {
            RuntimeBlendshapeVertexBuffer{{0x900, 0x1234, 2, 3, 4, 5}, morph_data.data()},
            RuntimeBlendshapeVertexBuffer{{0xB00, 0, 0, 0, 0, 0}, nullptr},
            RuntimeBlendshapeVertexBuffer{{0xA00, 0x5678, 6, 7, 8, 4}, morph_data.data()}
        };
        const RuntimeBlendshapeCpu morph_cpu{morph_buffers};
        copied_node.blendshape_cpu = &morph_cpu;
        const u16 morph_count = 3;
        std::memcpy(node_bytes.data() + 0x24, &morph_count, 2);
        node_bytes[0x26] = 1;
        std::array<u8, 1024> morph_storage;
        morph_storage.fill(0xCD);
        std::vector<std::unique_ptr<u8[]>> morph_allocations;
        std::vector<const u8*> morph_registrations;
        RuntimeBlendshapeBoundaries morph_boundaries;
        morph_boundaries.allocator = [&](u32 size, u32 alignment, const char* source, u32 line)
        {
            require((size == 5 || size == 4) && alignment == 0x80 && line == 0x1EA &&
                    std::strcmp(source, ".\\nodes\\geometry\\blendshape.c") == 0,
                    "blendshape allocator arguments changed");
            morph_allocations.push_back(std::make_unique<u8[]>(size));
            return morph_allocations.back().get();
        };
        morph_boundaries.register_resource = [&](RuntimeBlendshapeVertexBuffer& buffer, const u8* data)
        {
            morph_registrations.push_back(data);
            require(data == morph_data.data() && (buffer.words[1] == 0x1234 || buffer.words[1] == 0x5678),
                    "blendshape registration did not receive the overwritten source record/data");
        };
        RuntimeBlendshapeNodeInstance morph_instance;
        copy_runtime_node_blendshape(copied_node, morph_instance, morph_storage, morph_boundaries);
        require(morph_allocations.size() == 2 && morph_registrations.size() == 2 &&
                std::memcmp(morph_allocations[0].get(), morph_data.data(), 5) == 0 &&
                std::memcmp(morph_allocations[1].get(), morph_data.data(), 4) == 0 &&
                morph_instance.buffers[1].words == std::array<u32, 6>{0, 0, 0, 0, 0, 0} &&
                morph_instance.buffers[1].data == nullptr &&
                morph_instance.weights == reinterpret_cast<float*>(morph_storage.data() + 16 + 0x1C8) &&
                morph_instance.weights[0] == 0 && morph_instance.weights[1] == 0 &&
                morph_storage[16 + 0x1C8 + 8] == 0xCD &&
                morph_instance.attribute_targets[0] == morph_storage.data() + 16 + 0x1C8 + 8 &&
                morph_instance.attribute_targets[1] == nullptr &&
                morph_instance.attribute_targets[2] == morph_storage.data() + 16 + 0x1C8 + 8 + 5 &&
                morph_storage[16] == 0,
                "blendshape copy changed heap copies, zero buffers, type 9/10 targets or weight count");
        RuntimeModelInstanceStorage cleanup_storage;
        cleanup_storage.blendshape_nodes[copied_node.copy_offset] = morph_instance;
        copied_node.type_name = "ndBlendShape";
        u32 busy_calls = 0;
        u32 physical_frees = 0;
        morph_boundaries.is_busy = [&](RuntimeBlendshapeVertexBuffer&)
        {
            ++busy_calls;
            return busy_calls == 1 ? 1 : 0;
        };
        morph_boundaries.free_physical = [&](const u8* data, const char* source, u32 line)
        {
            require(data == morph_data.data() && line == 0x18E &&
                std::strcmp(source, ".\\nodes\\geometry\\blendshape.c") == 0,
                "blendshape release must receive overwritten source data and original allocator site");
            ++physical_frees;
        };
        release_runtime_model_nodes(&copied_node, cleanup_storage, morph_boundaries);
        const auto& cleaned_morph = cleanup_storage.blendshape_nodes.at(copied_node.copy_offset);
        require(busy_calls == 3 && physical_frees == 2 &&
            cleaned_morph.buffers[0].data == nullptr && cleaned_morph.buffers[0].words[4] == 0 &&
            cleaned_morph.buffers[0].words[5] == 5 && cleaned_morph.buffers[2].data == nullptr,
            "blendshape release waits for GPU, clears data and retains buffer length");
        release_runtime_model_nodes(&copied_node, cleanup_storage, morph_boundaries);
        require(physical_frees == 2, "blendshape cleanup repeated a physical free");
        node_bytes[0x26] = 0;
        const u16 single_morph = 1;
        std::memcpy(node_bytes.data() + 0x24, &single_morph, 2);
        morph_storage.fill(0xCD);
        morph_instance.attribute_targets[0] = morph_storage.data();
        copy_runtime_node_blendshape(copied_node, morph_instance, morph_storage, morph_boundaries);
        require(morph_allocations.size() == 2 && morph_registrations.size() == 2 &&
                morph_instance.attribute_targets[0] == morph_storage.data() &&
                morph_storage[16 + 0x1C8] == 0xCD,
                "disabled GPU copy or single morph changed retained attributes/weights");
        std::array<u8, 4 + 2 * 0x98> shader_storage{};
        const u32 shader_count = 2;
        std::memcpy(shader_storage.data(), &shader_count, 4);
        const std::array<RuntimeModelResourceBlock, 2> shader_blocks
        {
            RuntimeModelResourceBlock{1, nullptr}, RuntimeModelResourceBlock{3, shader_storage.data()}
        };
        require(runtime_model_shader_storage_size(shader_blocks) == shader_storage.size() &&
                runtime_model_shader_storage_size({}) == 0,
                "model shader storage sizing changed count/stride or absent-directory behavior");
        const std::array<RuntimeModelResourceBlock, 2> null_shader_blocks
        {
            RuntimeModelResourceBlock{3, nullptr}, RuntimeModelResourceBlock{3, shader_storage.data()}
        };
        require(runtime_model_shader_storage_size(null_shader_blocks) == 0,
                "shader storage sizing skipped a null first matching block");
        RuntimeShaderRegistrationState shader_registry;
        shader_registry.records[0] = instance.data();
        shader_registry.records[2] = instance.data() + 4;
        shader_registry.count = 2;
        shader_storage[4 + 0x66] = 0x80;
        register_runtime_model_shaders(shader_storage.data(), shader_registry);
        require(shader_registry.count == 4 && shader_registry.records[1] == shader_storage.data() + 4 &&
                shader_registry.records[3] == shader_storage.data() + 4 + 0x98 &&
                shader_storage[4 + 0x66] == 0x81 && shader_storage[4 + 0x98 + 0x66] == 1,
                "shader registration changed first-free order, record stride or preserved flag bits");
        register_runtime_model_shaders(nullptr, shader_registry);
        std::array<u8, 4> no_shader_storage{};
        register_runtime_model_shaders(no_shader_storage.data(), shader_registry);
        require(shader_registry.count == 4, "null/empty shader storage changed registration state");
        const std::array<RuntimeShaderCpuEntry, 8> shader_entries
        {
            RuntimeShaderCpuEntry{"xf_uv", 1}, RuntimeShaderCpuEntry{"xf_uv_du", -2500},
            RuntimeShaderCpuEntry{"xf_uv_dv", 1250}, RuntimeShaderCpuEntry{"xf_uv_drotate", 500},
            RuntimeShaderCpuEntry{"xf_uv_pu", -100}, RuntimeShaderCpuEntry{"xf_uv_pv", 200},
            RuntimeShaderCpuEntry{"xf_static", 0}, RuntimeShaderCpuEntry{"color", 0}
        };
        const RuntimeShaderCpu shader{shader_entries};
        std::array<RuntimeShaderParameter, 2> parameters{};
        parameters[0].bytes[0x40] = 7;
        RuntimeShaderInstance shader_instance{parameters};
        const RuntimeShaderCpuEntry* shader_entry = &shader_entries[0];
        require(!find_runtime_shader_entry(shader, "XF_uv", shader_entry) &&
                shader_entry == &shader_entries[0],
                "shader entry miss changed output or case sensitivity");
        RuntimeShaderParameter* parameter = &parameters[0];
        require(find_runtime_shader_parameter(shader, shader_instance, "xf_uv", parameter) &&
                parameter == &parameters[1], "shader runtime lookup lost its 0x44 index stride");
        require(!find_runtime_shader_parameter(shader, shader_instance, "absent", parameter) &&
                parameter == &parameters[1], "shader runtime miss changed the output");
        char shader_animation_name[100];
        runtime_shader_animation_name("plate", "xf_uv", shader_animation_name);
        require(std::strcmp(shader_animation_name, "plate_xf_uv") == 0,
                "shader animation name lost the prefix/underscore/parameter order");
        RuntimeShaderAnimationScratch shader_scratch;
        shader_scratch.animations[0].retained = {0x12345678, 0x90ABCDEF};
        auto shader_list = make_runtime_shader_animation_list("plate", shader, shader_instance,
            shader_scratch);
        require(shader_list != nullptr && shader_list->count == 1 &&
                shader_list->entries[0].parameter == &parameters[1] &&
                shader_list->entries[0].position == std::array<float, 3>{0, 0, 0} &&
                shader_list->entries[0].rates[0] == float(-2500.0L * static_cast<long double>(0.0001f)) &&
                shader_list->entries[0].rates[4] == float(200.0L * static_cast<long double>(0.0001f)) &&
                shader_list->entries[0].retained == std::array<u32, 2>{0x12345678, 0x90ABCDEF} &&
                parameters[0].bytes[0x40] == 7 && parameters[1].bytes[0x40] == 1,
                "shader setup changed signed rates, scratch tail, selection or flag writes");
        root.shader_cpu = &shader;
        root.shader_instance = &shader_instance;
        const std::array<RuntimeModelNamedEntry, 3> shader_names
        {
            RuntimeModelNamedEntry{"Shader_ignored", &root},
            RuntimeModelNamedEntry{"shader_plate", &root},
            RuntimeModelNamedEntry{"other", &root}
        };
        const RuntimeModelNameTable shader_name_table{shader_names};
        tree.names = &shader_name_table;
        RuntimeModelInstanceStorage shader_test_storage;
        shader_test_storage.shader_nodes[root.copy_offset].primary = &shader_instance;
        auto model_shader_lists = make_runtime_model_shader_animations(tree, shader_test_storage, shader_scratch);
        require(model_shader_lists != nullptr && model_shader_lists->count == 1 &&
                model_shader_lists->entries[0]->count == 1 &&
                model_shader_lists->entries[0]->entries[0].parameter == &parameters[1],
                "model shader collection failed its recovered nonzero-count allocation branch");
        tree.names = nullptr;
        require(make_runtime_model_shader_animations(tree, shader_test_storage, shader_scratch) == nullptr,
                "absent named nodes allocated a shader list");
        const std::array<RuntimeShaderCpuEntry, 1> static_shader_entries
        {
            RuntimeShaderCpuEntry{"xf_static", 0}
        };
        const RuntimeShaderCpu static_shader{static_shader_entries};
        require(make_runtime_shader_animation_list("static", static_shader, shader_instance,
                    shader_scratch) == nullptr && parameters[0].bytes[0x40] == 7,
                "zero shader rates allocated a list or changed the parameter flag");
        std::vector<char> matrix_calls;
        RuntimeShaderMatrixBoundaries matrix_boundaries;
        matrix_boundaries.translation = [&](RuntimeShaderMatrix& output, float x, float y, float z)
        {
            matrix_calls.push_back('t');
            output = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x, y, z, 1};
        };
        matrix_boundaries.rotation_z = [&](RuntimeShaderMatrix& output, float radians)
        {
            matrix_calls.push_back('z');
            const float sine = std::sin(radians);
            const float cosine = std::cos(radians);
            output = {cosine, sine, 0, 0, -sine, cosine, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        };
        matrix_boundaries.rotation_x = [&](RuntimeShaderMatrix& output, float radians)
        {
            matrix_calls.push_back('x');
            const float sine = std::sin(radians);
            const float cosine = std::cos(radians);
            output = {1, 0, 0, 0, 0, cosine, sine, 0, 0, -sine, cosine, 0, 0, 0, 0, 1};
        };
        matrix_boundaries.rotation_y = [&](RuntimeShaderMatrix& output, float radians)
        {
            matrix_calls.push_back('y');
            const float sine = std::sin(radians);
            const float cosine = std::cos(radians);
            output = {cosine, 0, -sine, 0, 0, 1, 0, 0, sine, 0, cosine, 0, 0, 0, 0, 1};
        };
        matrix_boundaries.multiply = [&](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& a,
                                         const RuntimeShaderMatrix& b)
        {
            matrix_calls.push_back('m');
            RuntimeShaderMatrix product{};
            for (size_t row = 0; row < 4; ++row)
            {
                for (size_t column = 0; column < 4; ++column)
                {
                    for (size_t k = 0; k < 4; ++k)
                    {
                        product[row * 4 + column] += a[row * 4 + k] * b[k * 4 + column];
                    }
                }
            }
            output = product;
        };
        matrix_boundaries.transpose = [&](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
        {
            matrix_calls.push_back('s');
            const RuntimeShaderMatrix original = input;
            for (size_t row = 0; row < 4; ++row)
            {
                for (size_t column = 0; column < 4; ++column)
                {
                    output[row * 4 + column] = original[column * 4 + row];
                }
            }
        };
        RuntimeShaderMatrix rotation_output;
        rotation_output.fill(9);
        runtime_shader_rotation(rotation_output, '?', 1, matrix_boundaries);
        require(matrix_calls.empty() && rotation_output[0] == 9,
                "unknown shader rotation axis changed output or called a D3D boundary");
        runtime_shader_rotation(rotation_output, 'X', 0, matrix_boundaries);
        runtime_shader_rotation(rotation_output, 'y', 0, matrix_boundaries);
        runtime_shader_rotation(rotation_output, 'Z', 0, matrix_boundaries);
        require(matrix_calls == std::vector<char>{'x', 'y', 'z'},
                "shader rotation axis dispatch lost upper/lower-case branches");
        matrix_calls.clear();
        RuntimeShaderAnimation moving_shader{};
        moving_shader.parameter = &parameters[0];
        moving_shader.rates = {0.5f, -0.25f, 0, 0, 0};
        tick_runtime_shader_animation(moving_shader, 2, matrix_boundaries);
        RuntimeShaderMatrix parameter_matrix;
        std::memcpy(parameter_matrix.data(), parameters[0].bytes.data(), 64);
        require(moving_shader.position == std::array<float, 3>{1, -0.5f, 0} &&
                std::bit_cast<float>(moving_shader.retained[0]) == 2 &&
                matrix_calls == std::vector<char>{'t', 's'} &&
                parameter_matrix[3] == 1 && parameter_matrix[7] == -0.5f &&
                parameters[0].bytes[0x40] == 7,
                "translation-only shader tick changed motion, phase, matrix transpose or flag tail");
        moving_shader.position = {0, 0, 0};
        moving_shader.rates = {0, 0, 90, 0, 0};
        moving_shader.retained[0] = 0;
        matrix_calls.clear();
        tick_runtime_shader_animation(moving_shader, 1, matrix_boundaries);
        std::memcpy(parameter_matrix.data(), parameters[0].bytes.data(), 64);
        require(matrix_calls == std::vector<char>{'t', 'z', 'm', 't', 'm', 't', 'm', 's'} &&
                moving_shader.position[2] == 90 &&
                std::abs(parameter_matrix[1] + 1) < 0.000001f &&
                std::abs(parameter_matrix[4] - 1) < 0.000001f &&
                std::abs(parameter_matrix[3] - 1) < 0.000001f &&
                std::abs(parameter_matrix[7]) < 0.000001f,
                "rotating shader tick changed D3D call order or center-rotation matrix");
        moving_shader.position = {0, 0, 0};
        moving_shader.rates = {0, 0, 0, 2, -3};
        moving_shader.retained[0] = 0;
        matrix_calls.clear();
        tick_runtime_shader_animation(moving_shader, 1, matrix_boundaries);
        require(std::abs(moving_shader.position[0] - float(std::sin(static_cast<long double>(0.04f)) * 2)) < 0.000001f &&
                std::abs(moving_shader.position[1] - float(std::sin(static_cast<long double>(0.04f)) * -3)) < 0.000001f,
                "shader periodic rates lost the original phase multiplier or signed amplitudes");
        RuntimeModelShaderAnimations empty_animations{};
        matrix_calls.clear();
        tick_runtime_model_shader_animations(empty_animations, 1, matrix_boundaries);
        require(matrix_calls.empty(), "empty model shader list invoked D3D matrix boundaries");
        model_shader_lists->entries[0]->entries[0] = moving_shader;
        matrix_calls.clear();
        tick_runtime_model_shader_animations(*model_shader_lists, 1, matrix_boundaries);
        require(matrix_calls == std::vector<char>{'t', 's'} &&
                std::bit_cast<float>(model_shader_lists->entries[0]->entries[0].retained[0]) == 2,
                "model shader tick did not visit the populated animation list");
        auto retail_types = runtime_model_copy_types(morph_boundaries);
        require(retail_types.size() == 17 && std::strcmp(retail_types[7].name, "ndSkeleton") == 0 &&
                std::strcmp(retail_types[11].name, "ndShaderParam2") == 0 &&
                std::strcmp(retail_types[16].name, "ndBlendShape") == 0,
                "retail copy table changed its registration order");
        RuntimeModelNode constructed_shader{0, 11, &retail_types[11], nullptr, nullptr};
        constructed_shader.type_name = "ndShaderParam2";
        constructed_shader.copy_offset = 128;
        RuntimeShaderCpu constructed_shader_cpu = initial_shader;
        constructed_shader_cpu.entries = shader_entries;
        constructed_shader.shader_cpu = &constructed_shader_cpu;
        const std::array<Joint, 2> constructed_joints
        {
            Joint{{0, 0, 0}, {0, 0, 0}, -1}, Joint{{1, 2, 3}, {1, 2, 3}, 0}
        };
        RuntimeModelNode constructed_skeleton{0, 7, &retail_types[7], nullptr, &constructed_shader};
        constructed_skeleton.type_name = "ndSkeleton";
        constructed_skeleton.copy_offset = 4;
        constructed_skeleton.joints = constructed_joints;
        RuntimeModelNode constructed_root{0, 0, &retail_types[0], &constructed_skeleton, nullptr};
        constructed_root.type_name = "ndGroup";
        std::array<RuntimeModelNode*, 1> constructed_roots{&constructed_root};
        RuntimeModelNode auxiliary_node{};
        auxiliary_node.instance_offset = 12;
        const std::array<RuntimeModelNamedEntry, 2> constructed_names
        {
            RuntimeModelNamedEntry{"shader_plate", &constructed_shader},
            RuntimeModelNamedEntry{"MORPH_TARGET_BLINK", &auxiliary_node}
        };
        const RuntimeModelNameTable constructed_name_table{constructed_names};
        const RuntimeModelTree constructed_tree{2048, constructed_roots, &constructed_name_table};
        const std::array<u8, 16> constructed_cpu{};
        const RuntimeModelAsset constructed_asset{shader_blocks, &constructed_tree, constructed_cpu};
        RuntimeShaderRegistrationState construction_registry;
        RuntimeShaderAnimationScratch construction_scratch;
        auto constructed_model = make_runtime_model_instance(constructed_asset, 1, 1, retail_types,
            construction_registry, construction_scratch);
        require(constructed_model->source == &constructed_asset &&
                constructed_model->allocation_size == 0x2A0 + 2048 + shader_storage.size() &&
                constructed_model->shader_storage == constructed_model->allocation.get() + 0x2A0 + 2048 &&
                construction_registry.count == 2 &&
                construction_registry.records[0] == constructed_model->shader_storage + 4 &&
                constructed_model->skeleton_node == &constructed_skeleton &&
                constructed_model->primary == &constructed_model->primary_pose &&
                constructed_model->primary->count == 2 && constructed_model->primary->dirty &&
                constructed_model->primary->matrices[1] == AnimationMatrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0} &&
                constructed_model->alternate == nullptr &&
                constructed_model->auxiliary_targets[0] == reinterpret_cast<float*>(
                    constructed_model->tree_storage.bytes.data() + 12) &&
                constructed_model->auxiliary_targets[47] == nullptr &&
                constructed_model->shader_indices[0] == 0xFFFFFFFF &&
                constructed_model->shader_indices[19] == 0xFFFFFFFF &&
                constructed_model->node_flags == std::array<u8, 8>{0, 0, 0, 0, 0, 0, 0, 0} &&
                constructed_model->state_1c4 == 0xFFFFFFFF && constructed_model->state_1c0 == 0 &&
                constructed_model->state_290 == 0 && constructed_model->shader_animations != nullptr &&
                constructed_model->shader_animations->count == 1,
                "model instance construction changed allocation, shader registration, pose or header fields");
        RuntimeShaderInstance* constructed_primary = constructed_model->tree_storage.shader_nodes.at(128).primary;
        require(constructed_model->shader_animations->entries[0]->entries[0].parameter ==
                &constructed_primary->parameters[1] && constructed_primary->parameters[1].bytes[0x40] == 1,
                "model constructor did not bind shader animation to its own copied runtime instance");
        auto second_model = make_runtime_model_instance(constructed_asset, 0, 0, retail_types,
            construction_registry, construction_scratch);
        RuntimeShaderInstance* second_primary = second_model->tree_storage.shader_nodes.at(128).primary;
        require(second_model->allocation_size == 0x2A0 + 2048 && second_model->shader_storage == nullptr &&
                second_model->primary == nullptr && second_model->skeleton_node == &constructed_skeleton &&
                construction_registry.count == 2 && second_primary != constructed_primary &&
                second_model->shader_animations->entries[0]->entries[0].parameter == &second_primary->parameters[1] &&
                second_primary->parameters.data() != constructed_primary->parameters.data(),
                "second model instance shared shader pointers or ignored pose/shader-copy gates");
        const std::array<RuntimeModelResourceBlock, 2> constructor_null_blocks
        {
            RuntimeModelResourceBlock{3, nullptr}, RuntimeModelResourceBlock{3, shader_storage.data()}
        };
        const RuntimeModelAsset null_shader_asset{constructor_null_blocks, &constructed_tree};
        auto null_shader_model = make_runtime_model_instance(null_shader_asset, 0, 1, retail_types,
            construction_registry, construction_scratch);
        require(null_shader_model->shader_storage == nullptr && construction_registry.count == 2,
                "model constructor skipped the first null shader block");
        RuntimeModelNode no_skeleton_root{0, 0, &retail_types[0], nullptr, nullptr};
        no_skeleton_root.type_name = "ndGroup";
        std::array<RuntimeModelNode*, 1> no_skeleton_roots{&no_skeleton_root};
        const RuntimeModelTree no_skeleton_tree{16, no_skeleton_roots, nullptr};
        const RuntimeModelAsset no_skeleton_asset{{}, &no_skeleton_tree};
        auto no_skeleton_model = make_runtime_model_instance(no_skeleton_asset, 1, 1, retail_types,
            construction_registry, construction_scratch);
        require(no_skeleton_model->skeleton_node == nullptr && no_skeleton_model->primary == nullptr &&
                no_skeleton_model->shader_animations == nullptr && no_skeleton_model->shader_storage == nullptr,
                "model without skeleton/named shaders allocated their runtime outputs");
        AnimationResourceRegistry model_resource_registry;
        AnimationResourceRecord model_resource_record;
        model_resource_record.name = "aid_model_fixture";
        model_resource_record.resource_slot = 1;
        model_resource_record.references = 2;
        model_resource_registry.records.push_back(model_resource_record);
        AnimationLoadedResource model_loaded_resource;
        model_loaded_resource.cpu = constructed_cpu;
        model_resource_registry.loaded.push_back(model_loaded_resource);
        RuntimeModelDeferredPool deferred_models{{}, 1, 0};
        std::vector<u8*> freed_shader_records;
        const auto free_shader_record = [&](u8* record)
        {
            freed_shader_records.push_back(record);
        };
        const RuntimeModelAsset* released_asset = defer_runtime_model_release(*constructed_model,
            deferred_models, 123, model_resource_registry, construction_registry, free_shader_record);
        require(released_asset == &constructed_asset && deferred_models.records.size() == 1 &&
                deferred_models.limit == 1 && deferred_models.records[0].instance == constructed_model.get() &&
                deferred_models.records[0].fence == 123 && constructed_model->shader_animations == nullptr &&
                construction_registry.count == 0 && construction_registry.records[0] == nullptr &&
                construction_registry.records[1] == nullptr && freed_shader_records.size() == 2 &&
                freed_shader_records[0] == constructed_model->shader_storage + 4 &&
                freed_shader_records[1] == constructed_model->shader_storage + 4 + 0x98 &&
                model_resource_registry.records[0].references == 3 &&
                constructed_model->allocation != nullptr,
                "deferred model release changed fence/pool state, shader ownership or resource retain");
        release_animation_resource(model_resource_registry, constructed_cpu);
        defer_runtime_model_release(*second_model, deferred_models, 456, model_resource_registry,
            construction_registry, free_shader_record);
        require(deferred_models.records.size() == 2 && deferred_models.limit == 2 &&
                deferred_models.records[0].fence == 123 && deferred_models.records[1].fence == 456 &&
                second_model->shader_animations == nullptr && freed_shader_records.size() == 2 &&
                model_resource_registry.records[0].references == 3,
                "deferred pool growth lost existing records or freed absent shader storage");
        shader_storage[4 + 0x66] |= 0x10;
        shader_storage[4 + 0x98 + 0x66] |= 0x10;
        RuntimeShaderRegistrationState retained_shader_registry;
        register_runtime_model_shaders(shader_storage.data(), retained_shader_registry);
        unregister_runtime_model_shaders(shader_storage.data(), retained_shader_registry, free_shader_record);
        require(retained_shader_registry.count == 0 && freed_shader_records.size() == 2,
                "retained shader flag 0x10 did not skip heap release");
        unregister_runtime_model_shaders(shader_storage.data(), retained_shader_registry, free_shader_record);
        unregister_runtime_model_shaders(nullptr, retained_shader_registry, free_shader_record);
        require(retained_shader_registry.count == 0 && freed_shader_records.size() == 2,
                "unregistered/null shader records changed count or invoked heap release");
        auto empty_shader_list = std::make_unique<RuntimeModelShaderAnimations>();
        empty_shader_list->count = 0;
        release_runtime_model_shader_animations(empty_shader_list);
        require(empty_shader_list == nullptr, "zero-count shader animation header was not released");
        const std::array<u8, 16> second_cpu{};
        const RuntimeModelAsset startup_second_asset{{}, &no_skeleton_tree, second_cpu};
        std::array<AnimationBundleRecord, 2> startup_resources;
        startup_resources[0].name = "aid_model_first";
        startup_resources[0].cpu = constructed_cpu;
        startup_resources[1].name = "aid_model_second";
        startup_resources[1].cpu = second_cpu;
        std::array<AnimationBundlePool, 1> startup_pools
        {
            AnimationBundlePool{startup_resources, true}
        };
        AnimationResourceRegistry startup_registry;
        register_animation_resource_provider(startup_registry,
            [&](u32 handle, const AnimationResourceRecord& record, const void*& resolved)
            {
                return find_bundle_resource_provider(startup_pools, record.name.c_str(), resolved);
            },
            [&](u32 handle, const void* resolved, AnimationLoadedResource& resource)
            {
                return load_bundle_resource_provider(resolved, resource);
            },
            [&](u32 handle, const char* name, const AnimationLoadedResource& resource)
            {
                unload_bundle_resource_provider(startup_pools, name);
            }, 3, nullptr);
        RuntimeModelResourceViews startup_views;
        startup_views.assets[constructed_cpu.data()] = &constructed_asset;
        startup_views.assets[second_cpu.data()] = &startup_second_asset;
        RuntimeActorModelState startup_actor;
        RuntimeModelDeferredPool startup_deferred{{}, 1, 0};
        RuntimeShaderRegistrationState startup_shaders;
        RuntimeShaderAnimationScratch startup_scratch;
        ResourceErrorState startup_error;
        startup_error.suppressed = 1;
        require(set_runtime_actor_model(startup_actor, "aid_model_first", startup_registry, startup_error,
                    startup_views, startup_deferred, 11, retail_types, startup_shaders, startup_scratch,
                    free_shader_record) && startup_actor.model->source == &constructed_asset &&
                std::strcmp(startup_actor.name.data(), "aid_model_first") == 0 &&
                startup_registry.records[0].references == 1 && startup_resources[0].references == 1 &&
                startup_shaders.count == 2,
                "actor model startup did not acquire, name or construct the selected resource");
        RuntimeModelInstance* first_actor_model = startup_actor.model.get();
        require(set_runtime_actor_model(startup_actor, "aid_model_second", startup_registry, startup_error,
                    startup_views, startup_deferred, 22, retail_types, startup_shaders, startup_scratch,
                    free_shader_record) && startup_actor.model->source == &startup_second_asset &&
                std::strcmp(startup_actor.name.data(), "aid_model_second") == 0 &&
                startup_deferred.records.size() == 1 && startup_deferred.records[0].instance == first_actor_model &&
                startup_deferred.records[0].owner.get() == first_actor_model &&
                startup_deferred.records[0].fence == 22 && startup_registry.records[0].references == 1 &&
                startup_registry.records[1].references == 1 && startup_shaders.count == 0,
                "actor model replacement freed the queued instance or lost resource/fence ownership");
        require(set_runtime_actor_model(startup_actor, "not_an_aid", startup_registry, startup_error,
                    startup_views, startup_deferred, 33, retail_types, startup_shaders, startup_scratch,
                    free_shader_record) && startup_actor.model == nullptr && startup_actor.name[0] == 0 &&
                startup_deferred.records.size() == 2 && startup_registry.records[1].references == 1,
                "non-AID actor model replacement failed to release and clear the previous model");
        require(!set_runtime_actor_model(startup_actor, "aid_model_missing", startup_registry, startup_error,
                    startup_views, startup_deferred, 44, retail_types, startup_shaders, startup_scratch,
                    free_shader_record) && startup_actor.model == nullptr && startup_actor.name[0] == 0 &&
                startup_deferred.records.size() == 2,
                "suppressed missing model resource fabricated successful startup");
        require(set_runtime_actor_model(startup_actor, nullptr, startup_registry, startup_error,
                    startup_views, startup_deferred, 55, retail_types, startup_shaders, startup_scratch,
                    free_shader_record) && startup_actor.model == nullptr,
                "null actor model name changed the original success/empty behavior");
        const RuntimeModelHeapBoundaries cleanup_heap;
        const RuntimeNodeRelocationBoundaries cleanup_d3d;
        deferred_models.records[0].fence = 0xFFFFFFFE;
        deferred_models.records[1].fence = 0;
        u32 cleanup_fence = 0xFFFFFFFF;
        collect_runtime_model_releases(deferred_models, cleanup_fence, model_resource_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(cleanup_fence == 0 && deferred_models.records.size() == 1 &&
            deferred_models.records[0].instance == second_model.get() &&
            constructed_model->allocation == nullptr && constructed_model->primary == nullptr &&
            second_model->allocation != nullptr && model_resource_registry.records[0].references == 2,
            "wrapped fence or swap-last removal failed");
        collect_runtime_model_releases(deferred_models, cleanup_fence, model_resource_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(cleanup_fence == 1 && deferred_models.records.size() == 1 && second_model->allocation != nullptr,
            "model released before its two-frame fence delay");
        collect_runtime_model_releases(deferred_models, cleanup_fence, model_resource_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(cleanup_fence == 2 && deferred_models.records.empty() && second_model->allocation == nullptr &&
            model_resource_registry.records[0].references == 1, "deferred collector did not finish model/resource release");
        flush_runtime_model_releases(startup_deferred, cleanup_fence, startup_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(cleanup_fence == 0 && startup_deferred.records.empty() &&
            startup_registry.records[0].references == 0 && startup_registry.records[1].references == 0 &&
            startup_resources[0].references == 1 && startup_resources[1].references == 1,
            "forced flush lost model ownership or changed the separately deferred provider unload");
        flush_runtime_model_releases(startup_deferred, cleanup_fence, startup_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(cleanup_fence == 0 && startup_deferred.records.empty(), "empty flush changed fence/reset behavior");
        RuntimeActorModelState secondary_actor;
        require(set_runtime_actor_model(secondary_actor, "aid_model_first", startup_registry, startup_error,
            startup_views, startup_deferred, 60, retail_types, startup_shaders, startup_scratch,
            free_shader_record), "secondary-model fixture main model startup");
        set_runtime_actor_secondary_model(secondary_actor, "aid_model_first", startup_registry, startup_error,
            startup_views, startup_deferred, 61, retail_types, startup_shaders, startup_scratch, free_shader_record);
        require(secondary_actor.secondary->primary == nullptr && secondary_actor.secondary->shader_storage == nullptr &&
            secondary_actor.secondary->alternate == secondary_actor.model->primary &&
            secondary_actor.secondary->state_290 == 1 && startup_shaders.count == 2,
            "secondary model did not borrow the primary pose or preserve allocation gates");
        ModelPoseBuffer alternate_fixture;
        alternate_fixture.count = 1;
        alternate_fixture.matrices.resize(1);
        alternate_fixture.matrices[0][0] = 7.f;
        secondary_actor.model->alternate = &alternate_fixture;
        set_runtime_actor_secondary_model(secondary_actor, "aid_model_first", startup_registry, startup_error,
            startup_views, startup_deferred, 62, retail_types, startup_shaders, startup_scratch, free_shader_record);
        require(secondary_actor.secondary->alternate == &alternate_fixture && startup_deferred.records.size() == 1 &&
            startup_registry.records[0].references == 3, "secondary replacement lost alternate-pose priority or retained ownership");
        set_runtime_actor_secondary_model(secondary_actor, "none", startup_registry, startup_error,
            startup_views, startup_deferred, 63, retail_types, startup_shaders, startup_scratch, free_shader_record);
        require(secondary_actor.secondary == nullptr && startup_deferred.records.size() == 2,
            "non-AID secondary name did not clear and defer the old model");
        set_runtime_actor_secondary_model(secondary_actor, "aid_model_missing", startup_registry, startup_error,
            startup_views, startup_deferred, 64, retail_types, startup_shaders, startup_scratch, free_shader_record);
        require(secondary_actor.secondary == nullptr && startup_deferred.records.size() == 2,
            "suppressed missing secondary resource fabricated a model");
        require(set_runtime_actor_model(secondary_actor, "none", startup_registry, startup_error,
            startup_views, startup_deferred, 65, retail_types, startup_shaders, startup_scratch,
            free_shader_record), "secondary-model fixture main model release");
        flush_runtime_model_releases(startup_deferred, cleanup_fence, startup_registry,
            morph_boundaries, cleanup_heap, cleanup_d3d);
        require(startup_deferred.records.empty() && startup_registry.records[0].references == 0 &&
            alternate_fixture.matrices.size() == 1 && alternate_fixture.matrices[0][0] == 7.f,
            "secondary cleanup released borrowed pose or lost resource ownership");
        RuntimeActorResourceReference actor_reference;
        set_runtime_actor_resource(actor_reference, "aid_model_first", startup_registry, startup_error);
        require(actor_reference.resource.data() == startup_resources[0].cpu.data() &&
            std::strcmp(actor_reference.name.data(), "aid_model_first") == 0 &&
            startup_registry.records[0].references == 1, "actor resource acquisition lost name or ownership");
        set_runtime_actor_resource(actor_reference, "none", startup_registry, startup_error);
        require(actor_reference.resource.data() == nullptr && startup_registry.records[0].references == 0 &&
            std::strcmp(actor_reference.name.data(), "none") == 0, "non-AID actor resource retained ownership");
        set_runtime_actor_resource(actor_reference, nullptr, startup_registry, startup_error);
        require(actor_reference.name[0] == '\0', "null actor resource name did not clear");
        ActorAnimationState actor_table_fixture;
        AnimationTableResourceViews table_views;
        table_views.tables.emplace(startup_resources[0].cpu.data(), &button_bat_animation_table());
        set_actor_animation_table(actor_table_fixture, "aid_model_first", startup_registry, startup_error, table_views);
        require(actor_table_fixture.table == &button_bat_animation_table() && actor_table_fixture.table_owned == 1 &&
            startup_registry.records[0].references == 1, "actor table did not retain acquired resource ownership");
        set_actor_animation_table(actor_table_fixture, "none", startup_registry, startup_error, table_views);
        require(actor_table_fixture.table == nullptr && actor_table_fixture.table_owned == 1 &&
            startup_registry.records[0].references == 0, "actor table replacement lost release or owned flag");
        actor_table_fixture.table = &button_bat_animation_table();
        actor_table_fixture.table_owned = 0;
        set_actor_animation_table(actor_table_fixture, "none", startup_registry, startup_error, table_views);
        require(startup_registry.records[0].references == 0, "borrowed actor table was released");
        set_actor_animation_table(actor_table_fixture, "aid_model_missing", startup_registry, startup_error, table_views);
        require(actor_table_fixture.table == nullptr && actor_table_fixture.table_owned == 1,
            "suppressed missing table resource created an invented table");
        RuntimeModelInstance render_pose_model{};
        RuntimeModelNode render_pose_skeleton{};
        render_pose_model.skeleton_node = &render_pose_skeleton;
        ModelPoseBuffer render_pose_fixture;
        render_pose_fixture.count = 5;
        render_pose_fixture.dirty = true;
        render_pose_fixture.matrices.resize(5);
        for (i32 joint = 0; joint < 5; ++joint)
        {
            render_pose_fixture.matrices[joint] =
            {
                static_cast<float>(joint + 1), 2, 3, 4,
                5, 6, 7, 8, 9, 10, 11, 12
            };
        }
        std::vector<float> packed_pose_order;
        RuntimeShaderMatrixBoundaries packing_boundaries;
        packing_boundaries.transpose = [&](RuntimeShaderMatrix& output, const RuntimeShaderMatrix& input)
        {
            packed_pose_order.push_back(input[0]);
            for (size_t row = 0; row < 4; ++row)
            {
                for (size_t column = 0; column < 4; ++column)
                {
                    output[row * 4 + column] = input[column * 4 + row];
                }
            }
        };
        bind_runtime_model_pose(render_pose_model, &render_pose_fixture, packing_boundaries);
        require(render_pose_model.render_pose == &render_pose_fixture && !render_pose_fixture.dirty &&
            packed_pose_order == std::vector<float>{5.f, 4.f} &&
            render_pose_fixture.packed_matrices[0] == RuntimeShaderMatrix{1, 5, 9, 0, 2, 6, 10, 0, 3, 7, 11, 0, 4, 8, 12, 1} &&
            model_pose_matrix(render_pose_fixture, 4) == render_pose_fixture.matrices[4],
            "render pose packing lost reverse transpose boundary, affine values or query storage switch");
        bind_runtime_model_pose(render_pose_model, &render_pose_fixture, packing_boundaries);
        require(packed_pose_order.size() == 2, "clean render pose was packed twice");
        bind_runtime_model_pose(render_pose_model, nullptr, packing_boundaries);
        require(render_pose_model.render_pose == nullptr, "null render pose failed to clear skeleton binding");
        render_pose_model.skeleton_node = nullptr;
        render_pose_model.render_pose = &render_pose_fixture;
        bind_runtime_model_pose(render_pose_model, nullptr, packing_boundaries);
        require(render_pose_model.render_pose == &render_pose_fixture,
            "missing skeleton changed retained render binding");
        std::array<u8, 0x200> render_bytes{};
        RuntimeRelocationImage render_image{render_bytes, 0x1000};
        render_image.set_word(0x1000, 0, 0x1040);
        render_image.set_word(0x1000, 4, 3);
        render_image.set_word(0x1000, 0x10, 0x10A0);
        render_image.set_word(0x1040, 0, 9);
        render_image.set_word(0x1048, 0, 7);
        render_image.set_word(0x1048, 4, 0x1080);
        render_image.set_word(0x1050, 0, 7);
        render_image.set_word(0x1050, 4, 0x1084);
        render_image.set_word(0x1080, 0, 0xABC123);
        render_image.set_word(0x1084, 0, 0x123ABC);
        render_image.set_word(0x10A0, 0x28, 0x10E0);
        render_image.set_word(0x10E0, 4, 0xCDEF);
        render_image.set_word(0x10A0, 0x24, 0x1120);
        render_image.set_word(0x1110, 0, 0xFEED);
        std::array<RuntimeModelRenderFrame, 2> render_frames{};
        RuntimeModelRenderContext render_context;
        RuntimeRenderLight second_light, first_light, equal_light;
        set_runtime_light_type(second_light, 2);
        set_runtime_light_type(first_light, 1);
        set_runtime_light_type(equal_light, 2);
        set_runtime_lighting_enabled(render_context, 1);
        add_runtime_light(render_context, second_light);
        add_runtime_light(render_context, first_light);
        add_runtime_light(render_context, equal_light);
        require(render_context.lights.next == &first_light && first_light.next == &second_light &&
            second_light.next == &equal_light && equal_light.previous == &second_light && equal_light.next == nullptr &&
            render_context.lighting_flags == 0x80000221,
            "light list lost unsigned stable type ordering or nibble flags");
        set_runtime_light_color(first_light, 0.1f, 0.2f, 0.3f, 0.4f);
        set_runtime_light_direction(first_light, Vec3{1, 2, 3});
        set_runtime_light_position(first_light, Vec3{4, 5, 6});
        first_light.reserved = 0xFFFFFFFF;
        set_runtime_light_range(first_light, 5.f, 9.f);
        require(first_light.color[2] == 0.3f && first_light.direction.y == 2.f &&
            first_light.position.z == 6.f && first_light.range == 9.f &&
            first_light.range_reciprocal == 0.25f && first_light.reserved == 0,
            "light range or parameter setters lost field values");
        set_runtime_light_range(first_light, 9.f, 9.f);
        require(std::isinf(first_light.range_reciprocal), "light range invented zero-width guard");
        clear_runtime_lights(render_context);
        require(render_context.lights.next == nullptr && render_context.lighting_flags == 0x80000000 &&
            second_light.next == &equal_light,
            "light clear lost enabled bit or invented per-node unlink");
        set_runtime_lighting_enabled(render_context, 0);
        require(render_context.lighting_flags == 0, "lighting disable changed low flags");
        render_context.changed_render_states = 0x100;
        render_context.fog_render_state = 1;
        set_runtime_render_fog_enabled(render_context, 1);
        require(render_context.changed_render_states == 0x100 && render_context.fog_render_state == 1,
            "fog enable invented immediate render-state update");
        set_runtime_render_fog_enabled(render_context, 0);
        set_runtime_render_fog(render_context, 5.f, 9.f, 0x12, 0x34, 0x56, 0x78);
        require(render_context.changed_render_states == 0x2100 && render_context.fog_render_state == 0 &&
            render_context.state.fog_reciprocal == 0.25f && render_context.state.fog_scaled_end == 2.25f &&
            render_context.state.fog_color == 0x78123456,
            "fog state lost disable cache, reciprocal or color byte order");
        set_runtime_render_ambient(render_context, {-1.f, 0.5f, 2.f, 2.f});
        require(render_context.state.ambient_color == std::array<float, 4>{0, 0.5f, 1, 1},
            "ambient color clipping lost channel bounds");
        const float color_nan = std::bit_cast<float>(0x7FC00001u);
        set_runtime_render_ambient(render_context, {color_nan, color_nan, color_nan, color_nan});
        require(std::isnan(render_context.state.ambient_color[0]) && std::isnan(render_context.state.ambient_color[3]),
            "ambient clipping changed COMISS unordered-value branch");
        render_context.frames = render_frames;
        render_context.frame_index = 1;
        apply_runtime_model_materials(render_image, 0x1000, render_context);
        require(render_context.state.material_values == 0xCDEF && render_context.state.material == 0xABC123,
            "render material lookup lost first matching block or values indirection");
        render_image.set_word(0x1080, 0, 0);
        apply_runtime_model_materials(render_image, 0x1000, render_context);
        require(render_context.state.material == 0xABC123,
            "empty first material block cleared retained state or used later duplicate");
        render_image.set_word(0x1080, 0, 0x456);
        render_image.set_word(0x10A0, 0x28, 0);
        apply_runtime_model_materials(render_image, 0x1000, render_context);
        require(render_context.state.material_values == 0 && render_context.state.material == 0x456,
            "render material without values retained old values pointer");
        set_runtime_render_color(render_context, 1, 2, 3, 4);
        require(render_context.state.color == std::array<u8, 4>{1, 2, 3, 4}, "render color channel order changed");
        set_runtime_render_world(render_context, nullptr);
        require(render_frames[1].world[0] == 1 && render_frames[1].world[15] == 1 && render_frames[0].world[0] == 0,
            "render world identity changed an inactive frame");
        render_pose_model.skeleton_node = &render_pose_skeleton;
        render_pose_model.alternate = &render_pose_fixture;
        ModelPoseBuffer unselected_primary;
        unselected_primary.dirty = true;
        render_pose_model.primary = &unselected_primary;
        render_context.rendering = 7;
        int tree_draws = 0;
        render_context.state.draw = [&](u32 node, u32 first, u32 second, void* third)
        {
            ++tree_draws;
            require(node == 0x1120 && first == 0 && second == 42 && third == &render_pose_model.tree_storage &&
                render_context.state.node_descriptor == 0xFEED && render_context.draw_parameter == 42 &&
                render_context.state.instance == &render_pose_model && render_context.active_model == &render_pose_model &&
                render_context.rendering == 7 && render_frames[1].world[12] == 7.f,
                "model tree draw lost arguments, active model or setup/clear ordering");
        };
        const AnimationMatrix render_world{1, 0, 0, 7, 0, 1, 0, 8, 0, 0, 1, 9};
        AnimationMatrix inverse_fixture = render_world;
        require(invert_model_matrix(inverse_fixture.data(), inverse_fixture.data()) &&
            inverse_fixture[3] == -7.f && inverse_fixture[7] == -8.f && inverse_fixture[11] == -9.f,
            "retail matrix inverse lost alias branch or translation");
        AnimationMatrix singular_fixture{};
        const AnimationMatrix retained_inverse = inverse_fixture;
        require(!invert_model_matrix(inverse_fixture.data(), singular_fixture.data()) && inverse_fixture == retained_inverse,
            "singular matrix inverse lost failure return or changed output");
        RuntimeSceneFog scene_fog;
        scene_fog.enabled = 1;
        scene_fog.disabled = 1;
        scene_fog.start = 2.f;
        scene_fog.end = 4.f;
        scene_fog.color = {0x12, 0x34, 0x56, 0x78};
        configure_runtime_scene_fog(render_context, scene_fog);
        require(render_context.state.fog_enabled == 0 && render_context.state.fog_reciprocal == 0.5f &&
            render_context.state.fog_color == 0x78123456, "scene fog lost override gate or parameters");
        scene_fog.disabled = 0;
        configure_runtime_scene_fog(render_context, scene_fog);
        require(render_context.state.fog_enabled == 1, "scene fog enabled branch skipped");
        RuntimeSceneLightDirection light_direction_fixture;
        light_direction_fixture.camera = render_world;
        int normalizations = 0;
        const auto normalize_direction = [&](Vec3& output, const Vec3& input)
        {
            ++normalizations;
            const float length = std::sqrt(input.x * input.x + input.y * input.y + input.z * input.z);
            output = Vec3{input.x / length, input.y / length, input.z / length};
        };
        Vec3& first_direction = runtime_scene_light_direction(light_direction_fixture, normalize_direction);
        require(first_direction.z < -0.999f && normalizations == 0,
            "scene light lost reversed yaw or called camera normalization without gate");
        light_direction_fixture.camera_relative = 1;
        light_direction_fixture.override_mode = 1;
        light_direction_fixture.override_yaw_degrees = 90.f;
        Vec3& second_direction = runtime_scene_light_direction(light_direction_fixture, normalize_direction);
        require(&first_direction == &second_direction && second_direction.x < -0.999f && normalizations == 1,
            "scene light lost override degrees, shared scratch or camera-normalization branch");
        draw_runtime_model_instance(render_pose_model, render_image, 0x1000, render_world, 42,
            render_context, packing_boundaries);
        require(tree_draws == 1 && render_context.rendering == 0 && render_pose_model.render_pose == &render_pose_fixture &&
            unselected_primary.dirty && render_frames[1].world[13] == 8.f && render_frames[1].world[14] == 9.f,
            "model draw lost alternate pose priority, world conversion or final gate clear");
        RuntimeShaderMatrix left_matrix{2, 0, 0, 0, 0, 3, 0, 0, 0, 0, 4, 0, 5, 6, 7, 1};
        RuntimeShaderMatrix right_matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 8, 9, 10, 1};
        multiply_runtime_matrix(right_matrix, left_matrix, right_matrix);
        require(right_matrix[0] == 2 && right_matrix[5] == 3 && right_matrix[10] == 4 &&
            right_matrix[12] == 13 && right_matrix[13] == 15 && right_matrix[14] == 17,
            "runtime matrix multiply lost right operand alias or row translation order");
        multiply_runtime_matrix(left_matrix, left_matrix, right_matrix);
        require(left_matrix[0] == 4 && left_matrix[5] == 9 && left_matrix[10] == 16 &&
            left_matrix[12] == 23 && left_matrix[13] == 33 && left_matrix[14] == 45,
            "runtime matrix multiply lost left operand alias");
        std::array<u8, 0x34> matrix_node{};
        const u32 matrix_offset = 8;
        const u32 stored_matrix_count = 2;
        std::memcpy(matrix_node.data() + 0xC, &matrix_offset, 4);
        std::memcpy(matrix_node.data() + 0x30, &stored_matrix_count, 4);
        std::array<u8, 0x90> matrix_instance{};
        write_runtime_model_matrix(matrix_node, matrix_instance, 1, right_matrix);
        RuntimeShaderMatrix stored_matrix;
        std::memcpy(stored_matrix.data(), matrix_instance.data() + 0x4C, 0x40);
        require(stored_matrix == right_matrix && matrix_instance[0x8C] == 0 && matrix_instance[0x8D] == 1,
            "model matrix write lost runtime offset, matrix stride or per-matrix dirty byte");
        RuntimeModelInstance node_state_fixture{};
        node_state_fixture.node_flags[1] = 0xFF;
        std::array<u32, 32> shared_node_states{};
        set_runtime_model_node_state(node_state_fixture, 5, 0, shared_node_states);
        require(node_state_fixture.node_flags[1] == 0xF3 && shared_node_states[5] == 0,
            "model node state cleared adjacent two-bit fields");
        set_runtime_model_node_state(node_state_fixture, 5, 2, shared_node_states);
        require(node_state_fixture.node_flags[1] == 0xFB && shared_node_states[5] == 2,
            "model node state lost per-slot shared value or byte packing");
        set_runtime_model_node_state(node_state_fixture, 0, 0xFF, shared_node_states);
        require(node_state_fixture.node_flags[0] == 0xFF && shared_node_states[0] == 0xFF,
            "model node state invented two-bit value masking");
        std::cout << "model runtime construction, shader, deferred release and actor resource checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
