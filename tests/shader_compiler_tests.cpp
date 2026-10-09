#include "../src/engine/graphics/shader_validation_storage.h"
#include "../src/engine/graphics/shader_pair_optimizer.h"
#include "../src/engine/graphics/shader_merge_optimizer.h"
#include "../src/engine/graphics/shader_swizzle.h"
#include "../src/engine/graphics/shader_merge_pass.h"
#include "../src/engine/graphics/shader_merge_patterns.h"
#include <stdexcept>
#include <vector>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}
}
void check_shader_schedule_dependencies();
void check_shader_cost_dependencies();

void check_shader_compiler_leaves()
{
    using namespace gbtg;
    check_shader_schedule_dependencies();
    check_shader_cost_dependencies();
    const std::array<u32, 3> multiply{0x01200000, 0, 0x04000000};
    std::array<u32, 3> addition{0x01200000, 0x10000000, 0x06000032};
    std::array<u32, 3> fusion{7, 8, 9};
    require(combine_runtime_shader_multiply_add(fusion.data(), multiply.data(), addition.data())
        && fusion == std::array<u32, 3>{0x01200000, 0, 0x08000000},
        "multiply/add fusion substitutes the operand referencing prior destination");
    addition = {0x11200000, 0x32, 0x060001B0};
    require(combine_runtime_shader_multiply_add(fusion.data(), multiply.data(), addition.data())
        && fusion == std::array<u32, 3>{0x01200000, 0x1B0, 0x08000000},
        "multiply/add fusion accepts alternate referencing operand orientation");
    addition = {0x01200000, 0x10000000, 0x06000002};
    const auto before_fusion_rejection = fusion;
    require(!combine_runtime_shader_multiply_add(fusion.data(), multiply.data(), addition.data())
        && fusion == before_fusion_rejection,
        "multiply/add fusion rejects nonidentity selected channel without output writes");
    const std::array<u32, 3> matching_first{0x11000000, 0x10002003, 0x020001B2};
    std::array<u32, 3> matching_second{0x12000000, 0x10002003, 0x020001B2};
    require(combine_runtime_shader_matching_operations(fusion.data(), matching_first.data(), matching_second.data())
        && fusion == std::array<u32, 3>{0x13000000, 0x10D821B3, 0x020001B2},
        "matching operations combine complementary output masks and inactive defaults");
    matching_second[0] |= 0x100000;
    const auto before_destination_rejection = fusion;
    require(!combine_runtime_shader_matching_operations(fusion.data(), matching_first.data(), matching_second.data())
        && fusion == before_destination_rejection,
        "matching operations reject conflicting register destinations before copying output");
    matching_second = matching_first;
    matching_second[2] ^= 0x10;
    require(!combine_runtime_shader_matching_operations(fusion.data(), matching_first.data(), matching_second.data())
        && fusion == matching_first,
        "matching operation swizzle rejection retains the retail initial copy");
    std::array<u32, 3> retry_words{};
    RuntimeShaderMergeScratch retry_scratch{retry_words.data()};
    addition = {0x01200000, 0x10000000, 0x06000032};
    require(combine_runtime_shader_all_patterns(fusion.data(), multiply.data(), addition.data(), retry_scratch)
        && fusion == std::array<u32, 3>{0x01200000, 0, 0x08000000},
        "full shader dispatcher reaches multiply/add fusion after prior pattern rejection");
    std::array<u32, 10> all_pattern_run{3, 0x01200000, 0, 0x04000000,
        0x01200000, 0x10000000, 0x06000032, 0, 0, 0x02000000};
    compact_runtime_shader_all_merge_runs(all_pattern_run.data(), retry_scratch);
    require(all_pattern_run[0] == 1 && (all_pattern_run[3] & 0x1E000000) == 0x08000000,
        "full shader compaction chains a fusion with the following scalar retry");
    const std::array<u32, 3> retry_move{0, 0, 0x02000000};
    const std::array<u32, 3> retry_vector{0, 0, 0x04000000};
    std::array<u32, 3> retry_output{7, 8, 9};
    u32 retry_error = 0xDEADBEEF;
    require(retry_runtime_shader_instruction_merge(retry_output.data(), retry_move.data(), retry_vector.data(),
        &retry_error, retry_scratch) && retry_error == 0x7B
        && (retry_output[2] & 0xFE000000) == 0x24000000,
        "shader retry succeeds through scalar conversion while retaining original caller error");
    const auto successful_scratch = retry_words;
    const std::array<u32, 3> retry_conflict{0, 0, 0xE4000000};
    retry_error = 0xDEADBEEF;
    require(!retry_runtime_shader_instruction_merge(retry_output.data(), retry_move.data(), retry_conflict.data(),
        &retry_error, retry_scratch) && retry_error == 0x7B && retry_words == successful_scratch,
        "failed shader retry retains conversion storage instead of resetting it");
    std::array<u32, 16> merge_run{5};
    require(compact_runtime_shader_merge_runs(merge_run.data(), retry_scratch) == 0 && merge_run[0] == 1,
        "shader merge-run compactor combines a full compatible run in one pass");
    std::array<u32, 10> split_run{3, 0, 0, 0x04000000, 0, 0, 0x08000000, 0, 0, 0x08000000};
    compact_runtime_shader_merge_runs(split_run.data(), retry_scratch);
    require(split_run[0] == 2 && (split_run[3] & 0x1E000000) == 0x04000000
        && (split_run[6] & 0x1E000000) == 0x08000000,
        "shader merge-run compactor retains rejected pair then merges following run");
    merge_run[0] = 0xFFFFFFFF;
    compact_runtime_shader_merge_runs(merge_run.data(), retry_scratch);
    require(merge_run[0] == 0, "shader merge-run compactor preserves signed empty count branch");
    const std::array<u32, 3> dependent_move{0x0F000000, 0x10000000, 0x020001B0};
    retry_output = {7, 8, 9};
    const auto scratch_before_dependency = retry_words;
    require(!combine_runtime_shader_dependent_pair(retry_output.data(), dependent_move.data(), dependent_move.data(), retry_scratch)
        && retry_output == std::array<u32, 3>{7, 8, 9} && retry_words == scratch_before_dependency,
        "shader dependency gate prevents merger and scratch writes for register read-after-write");
    const std::array<u32, 3> vector_move{0x356ABCDA, 0x61234567, 0x03ABCDE9};
    std::array<u32, 3> scalar_move{};
    require(convert_runtime_shader_move_to_scalar(scalar_move.data(), vector_move.data())
        && scalar_move == std::array<u32, 3>{0x2065BCDE, 0x10D82DE9, 0x21ABC1B0},
        "move conversion transfers native scalar fields into caller storage");
    scalar_move.fill(0xFFFFFFFF);
    require(convert_runtime_shader_move_to_scalar(scalar_move.data(), vector_move.data())
        && scalar_move == std::array<u32, 3>{0xE065BCDE, 0xD0D82DE9, 0x21ABC1B0},
        "move conversion retains unwritten output bits");
    scalar_move = vector_move;
    require(convert_runtime_shader_move_to_scalar(scalar_move.data(), scalar_move.data())
        && scalar_move == std::array<u32, 3>{0x1060BCDE, 0x50D821B0, 0x21ABC1B0},
        "move conversion preserves in-place read-after-write behavior");
    const auto converted = scalar_move;
    require(!convert_runtime_shader_move_to_scalar(scalar_move.data(), converted.data()) && scalar_move == converted,
        "move conversion rejects scalar opcode without output writes");
    for (u32 bits = 0; bits < 256; ++bits)
    {
        const std::array<u32, 3> instruction{0x0F000000, 0x10000000, 0x02000000 | (bits << 4)};
        std::array<u8, 12> channels{};
        decode_runtime_shader_swizzles(instruction.data(), channels.data());
        require(channels[0] == (bits & 3) && channels[1] == ((bits >> 2) & 3)
            && channels[2] == ((bits >> 4) & 3) && channels[3] == ((bits >> 6) & 3)
            && channels[4] == 0xFF && channels[11] == 0xFF,
            "shader swizzle decoder handles all 256 active operand permutations");
        auto encoded = instruction;
        encode_runtime_shader_swizzles(encoded.data(), channels.data());
        require(encoded[0] == instruction[0] && encoded[2] == instruction[2]
            && encoded[1] == 0x10D801B0,
            "shader swizzle encoder preserves active permutation and fills inactive operands with defaults");
    }
    const std::array<u8, 4> default_channels{3, 0xFF, 1, 0};
    const std::array<u8, 4> replicated_channels{0xFF, 1, 0xFF, 0xFF};
    require(runtime_shader_swizzle_fallback(default_channels.data()) == 0xFF
        && runtime_shader_swizzle_fallback(replicated_channels.data()) == 1
        && resolve_runtime_shader_swizzle_channel(0xFF, 0xFF, 3) == 3
        && resolve_runtime_shader_swizzle_channel(0xFE, 0, 0) == -2,
        "shader swizzle fallback distinguishes default channels, replication and signed byte values");
    const std::array<u32, 3> swizzle_first{0x0F000000, 0x10000000, 0x020001B0};
    std::array<u32, 3> swizzle_second = swizzle_first;
    swizzle_second[2] ^= 0x10;
    std::array<u32, 3> merge_output{7, 8, 9};
    const auto untouched = merge_output;
    require(!merge_runtime_shader_swizzles(merge_output.data(), swizzle_first.data(), swizzle_second.data())
        && merge_output == untouched, "shader swizzle conflict leaves destination untouched");
    u32 merge_error = 0xDEADBEEF;
    require(!merge_runtime_shader_instruction(merge_output.data(), swizzle_first.data(), swizzle_second.data(), &merge_error)
        && merge_error == 0x7D && merge_output == swizzle_first,
        "instruction merger retains initial writes before reporting swizzle conflict");
    swizzle_second = swizzle_first;
    swizzle_second[2] = 0x040001B0;
    require(!merge_runtime_shader_instruction(merge_output.data(), swizzle_first.data(), swizzle_second.data(), &merge_error)
        && merge_error == 0x7B, "instruction merger reports vector opcode conflict");
    swizzle_second = swizzle_first;
    swizzle_second[1] = 0x20000000;
    require(!merge_runtime_shader_instruction(merge_output.data(), swizzle_first.data(), swizzle_second.data(), &merge_error)
        && merge_error == 0x7C, "instruction merger reports operand type conflict");
    merge_error = 0xDEADBEEF;
    require(merge_runtime_shader_instruction(merge_output.data(), swizzle_first.data(), swizzle_first.data(), &merge_error)
        && merge_error == 0xDEADBEEF && merge_output == std::array<u32, 3>{0x0F000000, 0x10D801B0, 0x020001B0},
        "successful instruction merge retains caller error and canonicalizes inactive swizzles");
    const std::array<u32, 3> destination_first{0x0F100000, 0x10000000, 0x020001B0};
    const std::array<u32, 3> destination_second{0x0F200000, 0x10000000, 0x020001B0};
    require(!merge_runtime_shader_instruction(merge_output.data(), destination_first.data(), destination_second.data(), &merge_error)
        && merge_error == 0x7E && merge_output[1] == 0x10D801B0,
        "destination conflict retains earlier swizzle writes and reports native code");
    for (u32 selected = 0; selected < 16; ++selected)
    {
        std::array<u8, 4> masked{};
        decode_runtime_shader_masked_swizzle(static_cast<u8>(selected), 0, 1, 2, 3, masked.data());
        for (u32 lane = 0; lane < 4; ++lane)
        {
            require(masked[lane] == ((selected & (1u << lane)) != 0 ? 3 - lane : 0xFF),
                "masked swizzle decoder preserves reversed lane order and unused channel marker");
        }
    }
    std::array<u32, 3> operands{0, 0x30000000, 0x02000000};
    require(runtime_shader_uses_constant(operands.data())
        && !runtime_shader_uses_vertex_input(operands.data()),
        "shader operand predicates decode vector register type");
    require(equal_runtime_shader_operand(1, 7, 0, operands.data(), 1, 7, 0, operands.data())
        && !equal_runtime_shader_operand(1, 7, 0, operands.data(), 1, 8, 0, operands.data())
        && !equal_runtime_shader_operand(0, 0, 0, operands.data(), 0, 0, 0, operands.data()),
        "shader operand equality preserves register index and unknown type rejection");
    std::array<u32, 3> packed{0xFFFFFFFF, 2, 3};
    const auto before_pack = packed;
    const std::array<u8, 6> conflicting{2, 1, 1, 3, 2, 1};
    require(!pack_runtime_shader_destinations(packed.data(), conflicting.data()) && packed == before_pack,
        "conflicting shader destinations leave instruction unchanged");
    const std::array<u8, 6> destinations{2, 4, 1, 3, 1, 1};
    require(pack_runtime_shader_destinations(packed.data(), destinations.data())
        && packed[0] == 0xF243FFFF && packed[1] == 2 && packed[2] == 3,
        "shader destination packing retains unrelated instruction bits");
    std::array<u32, 7> program{2, 0, 0, 0, 0, 0, 0};
    require(compact_runtime_shader_instruction_pairs(program.data()) == 0 && program[0] == 1,
        "shader pair compaction merges compatible instructions in place");
    program = {2, 0, 0, 0x02000000, 0, 0, 0x04000000};
    const auto rejected = program;
    require(compact_runtime_shader_instruction_pairs(program.data()) == 0 && program == rejected,
        "different shader opcodes preserve instruction count and raw words");
    std::array<u32, 16> odd{5};
    odd[15] = 0x02000000;
    compact_runtime_shader_instruction_pairs(odd.data());
    require(odd[0] == 3 && odd[9] == 0x02000000,
        "shader compaction makes one pair pass and retains unmatched final instruction");
    program[0] = 0xFFFFFFFF;
    compact_runtime_shader_instruction_pairs(program.data());
    require(program[0] == 0, "negative shader count takes native empty compaction branch");
    require(runtime_shader_swizzle_is_identity(0xF, 0, 0, 1, 2, 3)
        && !runtime_shader_swizzle_is_identity(0xF, 1, 0, 1, 2, 3)
        && runtime_shader_swizzle_is_identity(0, 0, 3, 2, 1, 0),
        "shader identity swizzle respects negation and selected channel mask");
    const std::array<u32, 3> move_first{0x0F000000, 0, 0x02000000};
    std::array<u32, 3> move_second{0x0000F000, 0x10000000, 0x020001B0};
    std::array<u32, 3> output{1, 2, 3};
    require(combine_runtime_shader_move_instruction(output.data(), move_first.data(), move_second.data())
        && output == std::array<u32, 3>{0x0F00F000, 0, 0x02000000},
        "shader move merger preserves registers and incorporates output write mask");
    move_second[0] |= 4;
    const auto retained = output;
    require(!combine_runtime_shader_move_instruction(output.data(), move_first.data(), move_second.data())
        && output == retained, "rejected shader move merger retains output");
    ShaderValidationChannelMask mask{};
    for (u32 bits = 0; bits < 16; ++bits)
    {
        require(construct_shader_validation_channel_mask(mask, bits) == &mask
            && mask.vtable_address == 0x203790, "shader mask constructor retains actual vtable and self");
        require(mask.channels == std::array<u8, 4>
            {static_cast<u8>((bits >> 3) & 1), static_cast<u8>((bits >> 2) & 1),
                static_cast<u8>((bits >> 1) & 1), static_cast<u8>(bits & 1)},
            "shader mask decoder preserves channel order for all sixteen masks");
    }
    decode_shader_validation_channel_mask(mask, 0xFFFFFFF0);
    require(mask.channels == std::array<u8, 4>{0, 0, 0, 0}, "shader mask ignores upper bits");
    struct Node : ShaderValidationTreeNode
    {
        u32 id;
        std::vector<u32>* visits;
        ShaderValidationTreeNode* parent;
        ShaderValidationTreeNode* replacement;
    };
    const ShaderValidationTreeVtable vtable
    {
        [](ShaderValidationTreeNode& base, u32 release)
        {
            auto& node = static_cast<Node&>(base);
            require(release == 1 && node.retained == 0, "validation deleting destructor receives release and cleared field");
            node.visits->push_back(node.id);
            if (node.parent != nullptr)
            {
                node.parent->second = node.replacement;
            }
        }
    };
    std::vector<u32> visits;
    Node root{}, first{}, old_second{}, replacement{};
    root.vtable = first.vtable = old_second.vtable = replacement.vtable = &vtable;
    root.visits = first.visits = old_second.visits = replacement.visits = &visits;
    root.id = 0;
    first.id = 1;
    old_second.id = 2;
    replacement.id = 3;
    root.retained = first.retained = old_second.retained = replacement.retained = 99;
    root.first = &first;
    root.second = &old_second;
    first.parent = &root;
    first.replacement = &replacement;
    ShaderValidationTreeStorage storage{&root, 4};
    ShaderValidationTreeNode* result = nullptr;
    require(find_shader_validation_tree_last(storage, &result) == &result && result == &first,
        "validation tree lookup follows actual first-child chain and returns output pointer");
    destroy_shader_validation_tree(&root);
    require(visits == std::vector<u32>{1, 3, 0} && old_second.retained == 99,
        "validation deletion reads second child after first-child destructor mutation");
    reset_shader_validation_tree_storage(storage);
    require(storage.root == nullptr && storage.count == 0 && visits.size() == 3,
        "native validation reset clears storage then passes null to destruction");
    result = &root;
    find_shader_validation_tree_last(storage, &result);
    require(result == nullptr, "empty validation tree lookup writes null");
    destroy_shader_validation_tree(nullptr);
    require(visits.size() == 3, "null validation destruction makes no indirect call");
}
