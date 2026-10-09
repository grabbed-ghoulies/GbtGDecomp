#include "../src/engine/graphics/shader_rewrite_program.h"
#include "../src/engine/graphics/shader_schedule_init.h"
#include "../src/engine/graphics/shader_validation_setup.h"
#include "../src/engine/graphics/shader_rewrite_storage.h"
#include "../src/engine/graphics/shader_rewrite_writes.h"
#include "../src/engine/graphics/shader_rewrite_outputs.h"
#include "../src/engine/graphics/shader_reorder.h"
#include "../src/engine/graphics/shader_validation_channels.h"
#include "../src/engine/graphics/shader_validation_node_init.h"
#include "../src/engine/graphics/shader_rewrite_context.h"
#include "../src/engine/graphics/shader_validation_balance.h"
#include <limits>
#include "../src/engine/graphics/shader_register_rewrite.h"
#include "../src/engine/graphics/shader_validation_insert.h"
#include <cstdlib>
#include "../src/engine/graphics/shader_validation_registration.h"
#include "../src/engine/graphics/shader_validation_pass_traversal.h"
#include "../src/engine/graphics/shader_validation_cleanup.h"
#include "../src/engine/graphics/shader_validation_erase.h"
#include "../src/engine/graphics/shader_validation_operand_tree.h"
#include "../src/engine/graphics/shader_validation_operand_order.h"
#include "../src/engine/graphics/shader_validation_reserved_register.h"
#include <cstring>
#include <stdexcept>
#include <vector>

void check_runtime_cpp_exception_dispatch();
void check_shader_validation_constructors();
void check_shader_validation_serializer();
void check_shader_validation_lifecycle();

namespace
{
void require(bool value, const char* message)
{
    if (!value)
    {
        throw std::runtime_error(message);
    }
}
}

void check_shader_rewrite_dependencies()
{
    using namespace gbtg;
    ShaderValidationChannelMask packed_channels{0, {0, 0x80, 0, 0xff}};
    require(pack_shader_validation_channel_mask(packed_channels) == 5,
        "channel mask treats every nonzero byte as enabled");
    ShaderValidationChannelOrder missing_channels{0, {4, 4, 2, 4}};
    require(fill_shader_validation_missing_channels(missing_channels, missing_channels) == &missing_channels
        && missing_channels.channels == std::array<u32, 4>{2, 2, 2, 2}
        && missing_channels.vtable_address == 0x203794,
        "channel completion supports aliased output and forward fallback");
    missing_channels.channels = {1, 4, 4, 3};
    fill_shader_validation_missing_channels(missing_channels, missing_channels);
    require(missing_channels.channels == std::array<u32, 4>{1, 1, 1, 3},
        "channel completion prefers earlier present channels");
    missing_channels.channels = {4, 4, 4, 4};
    fill_shader_validation_missing_channels(missing_channels, missing_channels);
    require(missing_channels.channels == std::array<u32, 4>{0, 0, 0, 0},
        "channel completion maps entirely missing channels to zero");
    std::array<u8, 0x2c> channel_operand{};
    const std::array<u32, 4> source_channels{3, 2, 1, 0};
    std::memcpy(channel_operand.data() + 0x1c, source_channels.data(), 16);
    require(copy_shader_validation_operand_order(missing_channels, channel_operand.data()) == &missing_channels
        && missing_channels.channels == source_channels,
        "operand order copies the four retail words from offset 1c");
    ShaderValidationBalancedTree operand_tree{nullptr, 0};
    std::vector<ShaderValidationBalancedNode*> operand_allocations;
    bool operand_allocation_fails = false;
    ShaderValidationInsertContext operand_context
    {
        operand_tree,
        [&](std::size_t size, u32 tag) -> void*
        {
            require(size == sizeof(ShaderValidationBalancedNode) && tag == 0x24870000,
                "operand tree requests genuine node allocation");
            if (operand_allocation_fails)
            {
                return nullptr;
            }
            auto* node = new ShaderValidationBalancedNode{};
            operand_allocations.push_back(node);
            return node;
        }
    };
    register_shader_validation_operand(operand_context, 30);
    register_shader_validation_operand(operand_context, 20);
    register_shader_validation_operand(operand_context, 10);
    require(operand_tree.count == 3 && operand_tree.root->value == 20
        && operand_tree.root->first->value == 10 && operand_tree.root->second->value == 30
        && operand_tree.root->vtable_address == 0x2037e4,
        "operand registration performs genuine balancing with operand vtable");
    register_shader_validation_operand(operand_context, 20);
    require(operand_tree.count == 3 && operand_allocations.size() == 3,
        "duplicate operand registration retains count and allocation");
    operand_allocation_fails = true;
    register_shader_validation_operand(operand_context, 0xffffffff);
    require(operand_tree.count == 4 && operand_tree.root->second->second == nullptr,
        "failed operand insertion preserves retail count increment");
    for (auto* node : operand_allocations)
    {
        delete node;
    }
    operand_tree = {nullptr, 0};
    register_shader_validation_operand(operand_context, 1);
    require(operand_tree.root == nullptr && operand_tree.count == 1,
        "failed first operand allocation retains count one");
    ShaderValidationBalancedNode erase_left{0x2037e0, 10, nullptr, nullptr, nullptr, 1};
    ShaderValidationBalancedNode erase_right{0x2037e0, 30, nullptr, nullptr, nullptr, 1};
    auto* erase_root = new ShaderValidationBalancedNode{0x2037e0, 20, nullptr, &erase_left, &erase_right, 2};
    erase_left.parent = erase_right.parent = erase_root;
    ShaderValidationBalancedTree erase_tree{erase_root, 3};
    ShaderValidationRegisteredObject erased_object{0};
    u32 erase_calls = 0;
    ShaderValidationEraseContext erase_context
    {
        erase_tree,
        [&](ShaderValidationBalancedNode& node, u32 flags)
        {
            require(&node == erase_root && flags == 1 && erase_tree.count == 3,
                "erase invokes actual deletion before decrementing count");
            require(erase_tree.root == &erase_left && erase_left.parent == nullptr
                && erase_left.second == &erase_right && erase_right.parent == &erase_left,
                "erase publishes joined replacement before deletion");
            require(erased_object.vtable_address == 0x203808,
                "registered destructor installs base vtable before unregistering");
            ++erase_calls;
            delete &node;
        }
    };
    unregister_shader_validation_object(erase_context, 99);
    require(erase_calls == 0 && erase_tree.count == 3,
        "missing registration does not delete or decrement");
    destroy_shader_validation_registered_object(erase_context, erased_object, 20);
    require(erase_calls == 1 && erase_tree.count == 2 && erase_left.height == 2,
        "registered destructor joins and balances remaining tree");
    auto* erase_iterator = &erase_left;
    require(advance_shader_validation_balanced_iterator(&erase_iterator) == &erase_iterator
        && erase_iterator == &erase_right, "balanced iterator descends right subtree");
    advance_shader_validation_balanced_iterator(&erase_iterator);
    require(erase_iterator == nullptr, "balanced iterator climbs to end");
    require(erase_shader_validation_balanced_node(erase_context, nullptr) == nullptr
        && erase_calls == 1 && erase_tree.count == 2, "null erase preserves tree");
    check_runtime_cpp_exception_dispatch();
    check_shader_validation_constructors();
    check_shader_validation_serializer();
    check_shader_validation_lifecycle();
    std::array<u8, runtime_shader_write_storage_size> writes;
    writes.fill(0xA5);
    initialize_runtime_shader_writes(writes.data());
    for (u32 index = 0; index < 0xDF; ++index)
    {
        require(writes[index] == 0, "write accumulator clears native bytes");
    }
    require(writes[0xDF] == 0xA5, "write accumulator retains final padding byte");
    std::array<u32, 3> writing{0x03145028, 0, 0x3A000000};
    require(accumulate_runtime_shader_writes(writes.data(), writing.data()) == 0
        && writes[1] == 7 && writes[0x12] == 5 && writes[0xDE] == 5 && writes[0xDD] == 1,
        "combined instruction accumulates scalar register one and constant writes");
    writing = {0x0000B800, 0, 0};
    accumulate_runtime_shader_writes(writes.data(), writing.data());
    writing[0] = 0x00002800;
    accumulate_runtime_shader_writes(writes.data(), writing.data());
    require(writes[0xCD] == 2 && writes[0xC] == 11,
        "output writes replace individual masks while output zero accumulates");
    writing = {0x01F02000, 0, 0};
    require(accumulate_runtime_shader_writes(writes.data(), writing.data()) == 0x80004005u
        && writes[0xD] == 2 && writes[0xDE] == 7,
        "invalid vector register retains subsequent constant writes and failure");
    writing = {0x00001E00, 0, 0};
    require(accumulate_runtime_shader_writes(writes.data(), writing.data()) == 0x80004005u,
        "constant destination bounds return native failure");
    std::array<u8, runtime_shader_write_storage_size> other{};
    initialize_runtime_shader_writes(writes.data());
    writes[0xDF] = other[0xDF] = 0xFF;
    require(!runtime_shader_writes_conflict(writes.data(), other.data()),
        "write overlap ignores retained padding");
    for (const u32 offset : {0u, 12u, 13u, 0xCCu, 0xCDu, 0xDCu, 0xDDu})
    {
        writes[offset] = other[offset] = 1;
        require(runtime_shader_writes_conflict(writes.data(), other.data()),
            "write overlap includes each native bank boundary and address marker");
        other[offset] = 2;
        require(runtime_shader_writes_conflict(writes.data(), other.data()) == (offset == 0xDD),
            "write overlap compares channel bits but address marker uses nonzero booleans");
        writes[offset] = other[offset] = 0;
    }
    writes[2] = 8;
    const std::array<u32, 3> operand{0, 0x10000000, 0x020001B2};
    for (u32 channel = 0; channel < 4; ++channel)
    {
        require(runtime_shader_first_operand_reads_writes(writes.data(), operand.data(),
            static_cast<u8>(8u >> channel)) == (channel == 0),
            "operand reads only active swizzled channels");
    }
    auto reading = operand;
    for (u32 opcode = 0; opcode < 16; ++opcode)
    {
        reading[0] = 0x08000000;
        reading[2] = (opcode << 25) | 0x1B2;
        u8 conflict = 0xA5;
        const u32 status = runtime_shader_vector_reads_writes(writes.data(), reading.data(), &conflict);
        require(status == (opcode > 13 ? 0x80004005u : 0), "vector conflict opcode status matches native range");
        const u8 expected = (opcode == 0 || opcode > 13) ? 0xA5 : (opcode == 8 ? 0 : 1);
        require(conflict == expected, "vector conflict fixed masks and untouched outputs match native switch");
    }
    initialize_runtime_shader_writes(writes.data());
    writes[0xD + 5] = 4;
    reading = {0, 0x30000000, (5u << 17) | 0x1B2};
    require(runtime_shader_first_operand_reads_writes(writes.data(), reading.data(), 4),
        "constant input reads indexed write bank");
    writes[0xD + 5] = 0;
    writes[0xDE] = 4;
    require(!runtime_shader_first_operand_reads_writes(writes.data(), reading.data(), 4),
        "absolute constant ignores aggregate bank");
    reading[0] = 2;
    require(runtime_shader_first_operand_reads_writes(writes.data(), reading.data(), 4),
        "relative constant also reads aggregate bank");
    writes[0xDD] = 1;
    reading = {2, 0, 0};
    require(runtime_shader_instruction_reads_writes(writes.data(), reading.data()),
        "address marker blocks relative instruction without opcodes");
    reading[2] = 14u << 25;
    require(!runtime_shader_instruction_reads_writes(writes.data(), reading.data()),
        "invalid vector opcode suppresses relative conflict result");
    std::array<u32, 3> identity{0x1F2F0000, 0x100001B1, 0x220001B2};
    require(runtime_shader_instruction_outputs_are_redundant(identity.data()),
        "dual identity instruction reports redundant outputs");
    remove_runtime_shader_identity_writes(identity.data());
    require(identity[0] == 0x10200000, "identity removal preserves native register and operand fields");
    identity = {0x0000F018, 0x30000000, 0x020601B0};
    require(runtime_shader_vector_constant_write_is_identity(identity.data()),
        "matching constant and active swizzle channels form identity write");
    identity[0] |= 4;
    require(!runtime_shader_vector_constant_write_is_identity(identity.data()),
        "relative destination rejects identity elimination");
    std::array<u32, 12> merge_words{};
    RuntimeShaderMergeScratch merge_scratch{merge_words.data()};
    std::array<u32, 7> reorder_words{2, 0, 0, 0, 0, 0, 0};
    u32* reorder_program = reorder_words.data();
    std::array<u8, runtime_shader_schedule_storage_size> reorder_schedule{};
    configure_runtime_shader_schedule(reorder_schedule.data(), 1, 2);
    i32 candidate = -99;
    require(find_runtime_shader_ready_candidate(reorder_program, reorder_schedule.data(), 0, &candidate)
        && candidate == 1, "ready search finds independent zero-cost instruction");
    std::array<u32, 3> merge_output{};
    candidate = -99;
    require(find_runtime_shader_merge_candidate(reorder_program, 0, &candidate, merge_output.data(), merge_scratch)
        && candidate == 1, "merge search reaches genuine dependent-pair merger");
    require(reorder_runtime_shader_program(reorder_program, reorder_schedule.data(), 1, merge_scratch) == 0
        && reorder_program[0] == 1, "reorder pass merges independent instructions and updates count");
    candidate = -99;
    require(!find_runtime_shader_ready_candidate(reorder_program, reorder_schedule.data(), 0, &candidate)
        && candidate == -99, "empty ready search retains caller selection");
    reorder_words = {2, 0x08000000, 0, 0x02000000, 0x08000000, 0, 0x02000000};
    initialize_runtime_shader_writes(writes.data());
    accumulate_runtime_shader_writes(writes.data(), reorder_program + 1);
    require(!runtime_shader_reorder_is_safe(reorder_program, writes.data(), 0, 1),
        "reorder rejects overlapping destination channels");
    reorder_words = {2, 0x08000000, 0x10000000, 0x020001B2, 0x08200000, 0, 0x02000000};
    initialize_runtime_shader_writes(writes.data());
    accumulate_runtime_shader_writes(writes.data(), reorder_program + 1);
    require(!runtime_shader_reorder_is_safe(reorder_program, writes.data(), 0, 1),
        "reorder rejects candidate writes read by intervening instruction");
    reorder_words = {2, 0x01C00000, 0, 0x02000000, 0, 0, 0x04000000};
    require(reorder_runtime_shader_program(reorder_program, reorder_schedule.data(), 0, merge_scratch)
        == 0x80004005u, "reorder parent propagates genuine instruction recording failure");
    reorder_words = {0, 0, 0, 0, 0, 0, 0};
    const auto empty_reorder = reorder_words;
    require(reorder_runtime_shader_program(reorder_program, reorder_schedule.data(), 1, merge_scratch) == 0
        && reorder_words == empty_reorder, "empty reorder configures scheduler without reading instructions");
    ShaderValidationBalancedNode balance_root{}, balance_child{}, balance_leaf{};
    balance_root.height = balance_child.height = balance_leaf.height = 1;
    require(calculate_shader_validation_node_height(balance_root) == 1,
        "empty validation node retains native height one");
    balance_root.second = &balance_child;
    balance_child.parent = &balance_root;
    balance_child.second = &balance_leaf;
    balance_leaf.parent = &balance_child;
    ShaderValidationBalancedTree balanced_tree{&balance_root, 3};
    restore_shader_validation_tree_root(balanced_tree, balance_leaf);
    // The native updater stops at an already-correct starting node; refresh the changed owner first.
    restore_shader_validation_tree_root(balanced_tree, balance_child);
    require(balanced_tree.root == &balance_child && balance_child.parent == nullptr
        && balance_child.first == &balance_root && balance_child.second == &balance_leaf
        && balance_root.parent == &balance_child && balance_leaf.parent == &balance_child
        && balance_child.height == 2 && balance_root.height == 1 && balanced_tree.count == 3,
        "validator insertion balancing rotates root and preserves tree count");
    balance_root = {};
    balance_child = {};
    balance_leaf = {};
    balance_root.height = 3;
    balance_child.height = 2;
    balance_leaf.height = 1;
    balance_root.first = &balance_child;
    balance_child.parent = &balance_root;
    balance_child.second = &balance_leaf;
    balance_leaf.parent = &balance_child;
    balanced_tree.root = &balance_root;
    restore_shader_validation_tree_root(balanced_tree, balance_child);
    require(balanced_tree.root == &balance_leaf && balance_leaf.first == &balance_child
        && balance_leaf.second == &balance_root && balance_leaf.height == 2
        && balance_child.height == 1 && balance_root.height == 1,
        "validator performs native double rotation for inner-heavy subtree");
    balance_child.height = std::numeric_limits<i32>::max();
    balance_root.first = &balance_child;
    balance_root.second = nullptr;
    require(calculate_shader_validation_node_height(balance_root) == std::numeric_limits<i32>::min(),
        "validator height calculation preserves native signed comparison and wrapping increment");
    std::array<u8, 0x1C> register_references{};
    std::array<u8, 30> register_values{};
    std::array<u8, 54> register_lifetimes{};
    std::array<u32, 4> register_program{1, 0x0F200000, 0, 0x02000000};
    ShaderRegisterRewriteState register_state{register_references.data(), register_values.data(), 2,
        register_lifetimes.data(), 2, 0, register_program.data()};
    u8* lifetime = register_lifetimes.data() + 0x12;
    lifetime[13] = 3;
    lifetime[14] = 2;
    lifetime[15] = 1;
    lifetime[16] = 0;
    require(remap_runtime_shader_register_mask(3, lifetime) == 3,
        "register remap preserves reverse identity channel map");
    lifetime[13] = 0;
    lifetime[14] = 1;
    lifetime[15] = 2;
    lifetime[16] = 3;
    require(remap_runtime_shader_register_mask(3, lifetime) == 12,
        "register remap follows native low-bit-to-channel mapping");
    std::array<u8, 128> occupied{};
    const u16 occupied_value = 42;
    std::memcpy(occupied.data(), &occupied_value, 2);
    std::memcpy(occupied.data() + 2, &occupied_value, 2);
    lifetime[6] = 3;
    require(runtime_shader_register_has_space(register_state, occupied.data(), 0, 1),
        "flexible register lifetime counts available channels independently of their positions");
    lifetime[11] = 1;
    require(!runtime_shader_register_has_space(register_state, occupied.data(), 0, 1),
        "fixed register lifetime requires its actual channels to be free");
    register_program[1] = 0x00010000;
    register_program[3] = 0x22000000;
    mark_runtime_shader_register_lifetime(register_state, 1, 0, 0, 0);
    require(lifetime[8] == 1 && lifetime[9] == 1 && lifetime[11] == 1,
        "register lifetime tracks combined scalar register-one use");
    register_program = {1, 0x0F200000, 0, 0x02000000};
    const u16 value_one = 1;
    std::memcpy(register_references.data(), &value_one, 2);
    std::memcpy(register_references.data() + 0x18, &value_one, 2);
    std::memcpy(register_values.data() + 16, &value_one, 2);
    lifetime[12] = 7;
    lifetime[13] = 3;
    lifetime[14] = 2;
    lifetime[15] = 1;
    lifetime[16] = 0;
    require(apply_runtime_shader_register_mapping(register_state) == 0
        && register_program == std::array<u32, 4>{1, 0x0F700000, 0, 0x02000007},
        "register mapping updates genuine operand and output fields through value lifetime tables");
    const u16 value_two = 2;
    std::memcpy(register_references.data() + 6, &value_two, 2);
    std::memcpy(register_values.data() + 26, &value_two, 2);
    u8* last_lifetime = register_lifetimes.data() + 36;
    last_lifetime[6] = 1;
    last_lifetime[12] = 6;
    last_lifetime[13] = 3;
    std::memcpy(occupied.data() + 6 * 8, &occupied_value, 2);
    std::memcpy(occupied.data() + 7 * 8, &occupied_value, 2);
    require(release_runtime_shader_register_channels(register_state, register_references.data(), occupied.data(), 0) == 0
        && occupied[6 * 8] == 0 && occupied[7 * 8] == 42,
        "channel release uses the last nonzero reference lifetime and actual expiry instruction");
    ShaderValidationBalancedNode inserted_root{};
    initialize_shader_validation_object_node(inserted_root, 20, nullptr);
    ShaderValidationBalancedTree inserted_tree{&inserted_root, 1};
    std::vector<void*> allocations;
    ShaderValidationInsertContext insertion{inserted_tree, [&allocations](std::size_t bytes, u32 tag) -> void*
    {
        require(bytes == sizeof(ShaderValidationBalancedNode) && tag == 0x24870000,
            "validator insertion uses genuine node heap boundary and native tag");
        void* result = std::malloc(bytes);
        require(result != nullptr, "validator fixture real heap allocation succeeds");
        allocations.push_back(result);
        return result;
    }};
    auto* inserted_left = insert_shader_validation_object_node(insertion, 10, inserted_tree.root);
    auto* inserted_low = insert_shader_validation_object_node(insertion, 5, inserted_tree.root);
    require(inserted_tree.root == inserted_left && inserted_left->parent == nullptr
        && inserted_left->first == inserted_low && inserted_left->second == &inserted_root
        && inserted_tree.count == 3 && inserted_left->vtable_address == 0x2037E0,
        "validator recursive insertion restores rotated root and actual parent links");
    require(insert_shader_validation_object_node(insertion, 10, inserted_tree.root) == inserted_left
        && allocations.size() == 2 && inserted_tree.count == 3,
        "duplicate validation value retains existing node without allocation or count change");
    auto* inserted_high = insert_shader_validation_diagnostic_node(insertion, 0xFFFFFFFF, inserted_tree.root);
    require(inserted_high->value == 0xFFFFFFFF && inserted_high->vtable_address == 0x2037E8,
        "diagnostic insertion compares unsigned values and uses its native constructor");
    ShaderValidationBalancedNode failed_root{};
    initialize_shader_validation_object_node(failed_root, 20, nullptr);
    ShaderValidationBalancedTree failed_tree{&failed_root, 0xFFFFFFFF};
    ShaderValidationInsertContext failed_insert{failed_tree, [](std::size_t, u32) -> void*
    {
        return nullptr;
    }};
    require(insert_shader_validation_object_node(failed_insert, 10, &failed_root) == nullptr
        && failed_root.first == nullptr && failed_tree.count == 0 && failed_tree.root == &failed_root,
        "failed validator allocation still increments wrapping native count without fake node");
    for (void* allocation : allocations)
    {
        std::free(allocation);
    }
    ShaderValidationBalancedTree registered_tree{nullptr, 99};
    ShaderValidationRegisteredObject registered_object{0xFFFFFFFF};
    ShaderValidationBalancedNode registered_node{};
    u32 register_allocations = 0;
    ShaderValidationInsertContext registration{registered_tree, [&](std::size_t bytes, u32 tag) -> void*
    {
        require(bytes == sizeof(registered_node) && tag == 0x24870000 && registered_tree.count == 1
            && registered_object.vtable_address == 0x203808,
            "base validator constructor sets vtable and registration count before allocation");
        ++register_allocations;
        return &registered_node;
    }};
    require(initialize_shader_validation_registered_object(registration, registered_object, 0x12345678)
        == &registered_object && registered_tree.root == &registered_node && registered_tree.count == 1
        && registered_node.value == 0x12345678 && registered_node.parent == nullptr
        && registered_node.vtable_address == 0x2037E0 && registered_node.height == 1,
        "validator base constructor registers actual caller identity and returns object");
    register_shader_validation_object(registration, 0x12345678);
    require(register_allocations == 1 && registered_tree.count == 1,
        "duplicate object registration reuses existing root");
    ShaderValidationBalancedTree failed_registration_tree{nullptr, 17};
    ShaderValidationInsertContext failed_registration{failed_registration_tree, [](std::size_t, u32) -> void*
    {
        return nullptr;
    }};
    register_shader_validation_diagnostic(failed_registration, 0xFEDCBA98);
    require(failed_registration_tree.root == nullptr && failed_registration_tree.count == 1,
        "failed first diagnostic registration retains native count one and null root");
    lifetime[6] = 3;
    lifetime[11] = 1;
    occupied.fill(0);
    std::memcpy(occupied.data() + 2, &occupied_value, 2);
    require(assign_runtime_shader_register_channels(register_state, occupied.data(), 0, 1) == 0x80004005u
        && occupied[0] == 1 && occupied[2] == 42 && lifetime[13] == 3,
        "fixed channel assignment preserves earlier native writes on conflict");
    lifetime[6] = 1;
    occupied.fill(0);
    register_state.cursor = 0;
    require(allocate_runtime_shader_register(register_state, occupied.data(), 1, 1) == 0
        && lifetime[12] == 2 && occupied[2 * 8] == 1 && register_state.cursor == 1,
        "register allocator follows exact retail candidate order and retained cursor");
    std::array<u32, 7> allocated_program{2, 0x0F100000, 0x20000000, 0x020001B0,
        0x0F200000, 0x10000000, 0x020001B1};
    ShaderRegisterRewriteState allocated_state{};
    require(rewrite_runtime_shader_program_registers(allocated_state, allocated_program.data()) == 0
        && ((allocated_program[1] >> 20) & 15u) == 2 && ((allocated_program[4] >> 20) & 15u) == 3
        && (allocated_program[6] & 15u) == 2
        && allocated_state.references == nullptr && allocated_state.values == nullptr && allocated_state.lifetimes == nullptr,
        "complete register rewrite remaps linked instructions and frees all genuine tables");
    std::array<u32, 40> exhausted_program{};
    exhausted_program[0] = 13;
    for (u32 instruction = 0; instruction < 13; ++instruction)
    {
        exhausted_program[1 + instruction * 3] = 0x0F000000 | (instruction << 20);
        exhausted_program[2 + instruction * 3] = 0x20000000;
        exhausted_program[3 + instruction * 3] = 0x020001B0;
    }
    const auto exhausted_before = exhausted_program;
    require(rewrite_runtime_shader_program_registers(allocated_state, exhausted_program.data()) == 1
        && exhausted_program == exhausted_before && allocated_state.references == nullptr
        && allocated_state.values == nullptr && allocated_state.lifetimes == nullptr,
        "register exhaustion retains native collection result and skips remapping while freeing tables");
    std::array<u32, 4> skipped_program{1, 0, 0, 0};
    allocated_state.cursor = 7;
    require(rewrite_runtime_shader_register_one(allocated_state, 1, skipped_program.data()) == 0
        && allocated_state.cursor == 7, "register-one wrapper skips owner allocation when native scan is clear");
    std::array<u8, 50> merge_values{};
    std::array<u8, 90> merge_lifetimes{};
    std::array<u8, 128> merge_current{};
    std::array<u8, 8> source_references{};
    ShaderRegisterRewriteState merging_state{nullptr, merge_values.data(), 5, merge_lifetimes.data(), 5, 0, nullptr};
    auto store_u16 = [](u8* address, u16 value)
    {
        std::memcpy(address, &value, 2);
    };
    auto load_u16 = [](const u8* address)
    {
        u16 value;
        std::memcpy(&value, address, 2);
        return value;
    };
    store_u16(merge_current.data() + 16, 2);
    store_u16(merge_current.data() + 18, 4);
    store_u16(merge_values.data() + 26, 2);
    store_u16(merge_values.data() + 46, 4);
    u8* target_lifetime = merge_lifetimes.data() + 36;
    u8* source_lifetime = merge_lifetimes.data() + 72;
    store_u16(target_lifetime, 2);
    store_u16(target_lifetime + 2, 2);
    store_u16(target_lifetime + 4, 2);
    target_lifetime[6] = 1;
    store_u16(source_lifetime, 4);
    store_u16(source_lifetime + 2, 4);
    store_u16(source_lifetime + 4, 4);
    source_lifetime[6] = 2;
    source_lifetime[10] = 1;
    collect_runtime_shader_register_sources(merging_state, 3, 1, 2, 0, 1, 2, 3,
        source_references.data(), merge_current.data(), 5);
    require(load_u16(source_references.data()) == 2 && load_u16(source_references.data() + 2) == 4
        && load_u16(merge_values.data() + 26) == 2 && load_u16(merge_values.data() + 46) == 2
        && load_u16(merge_values.data() + 28) == 4 && load_u16(merge_values.data() + 48) == 0
        && load_u16(target_lifetime) == 2 && load_u16(target_lifetime + 2) == 5
        && target_lifetime[6] == 3 && target_lifetime[10] == 1,
        "source collection merges sorted lifetime value lists into lowest lifetime and retains aggregate flags");
    for (u32 byte = 0; byte < 18; ++byte)
    {
        require(source_lifetime[byte] == 0, "source collection clears merged-away lifetime record");
    }
    ShaderValidationPassNode pass_root{}, pass_left{}, pass_right{}, pass_middle{};
    pass_root.value = 0x80000000;
    pass_left.value = 0x7FFFFFFF;
    pass_right.value = 0xFFFFFFFF;
    pass_root.first = &pass_left;
    pass_root.second = &pass_right;
    pass_left.parent = pass_right.parent = &pass_root;
    ShaderValidationPassTree pass_tree{&pass_root, 3};
    ShaderValidationBalancedNode lookup_root{}, lookup_left{}, lookup_right{};
    lookup_root.value = 0x80000000;
    lookup_left.value = 0x7FFFFFFF;
    lookup_right.value = 0xFFFFFFFF;
    lookup_root.first = &lookup_left;
    lookup_root.second = &lookup_right;
    ShaderValidationBalancedTree lookup_tree{&lookup_root, 3};
    require(find_shader_validation_pass_node(lookup_tree, 0x7FFFFFFF) == &lookup_left
        && find_shader_validation_pass_node(lookup_tree, 0x80000000) == &lookup_root
        && find_shader_validation_pass_node(lookup_tree, 0xFFFFFFFF) == &lookup_right
        && find_shader_validation_pass_node(lookup_tree, 0) == nullptr,
        "validator graph traversal compares actual unsigned keys and returns null for missing node");
    ShaderValidationPassNode* pass_iterator = nullptr;
    require(first_shader_validation_pass_node(pass_tree, &pass_iterator) == &pass_iterator
        && pass_iterator == &pass_left, "validator graph first iterator preserves native returned argument pointer");
    require(advance_shader_validation_pass_iterator(&pass_iterator) == &pass_iterator && pass_iterator == &pass_root,
        "validator graph iterator climbs left-child parent link");
    advance_shader_validation_pass_iterator(&pass_iterator);
    require(pass_iterator == &pass_right, "validator graph iterator follows right subtree");
    advance_shader_validation_pass_iterator(&pass_iterator);
    require(pass_iterator == nullptr, "validator graph iterator reaches null after multiple ancestor climbs");
    pass_right.first = &pass_middle;
    pass_middle.parent = &pass_right;
    pass_iterator = &pass_root;
    advance_shader_validation_pass_iterator(&pass_iterator);
    require(pass_iterator == &pass_middle, "validator graph iterator descends left within right subtree");
    pass_tree.root = nullptr;
    lookup_tree.root = nullptr;
    require(first_shader_validation_pass_node(pass_tree, &pass_iterator) == &pass_iterator && pass_iterator == nullptr
        && find_shader_validation_pass_node(lookup_tree, 0) == nullptr,
        "empty validator graph preserves native null results");
    ShaderValidationChannelOrder channel_order{};
    initialize_shader_validation_channel_order(channel_order);
    require(channel_order.vtable_address == 0x203794
        && channel_order.channels == std::array<u32, 4>{0, 1, 2, 3},
        "validator initializes identity channel order and retail vtable");
    ShaderValidationChannelMask source_mask{0x1234, {0xFF, 0x80, 7, 0}}, target_mask{0xABCD, {1, 2, 3, 4}};
    copy_shader_validation_channel_mask(target_mask, source_mask);
    copy_shader_validation_channel_mask(target_mask, target_mask);
    require(target_mask.vtable_address == 0xABCD && target_mask.channels == source_mask.channels,
        "validator forward channel copy retains destination vtable and supports self copy");
    ShaderValidationNodeWords node_words;
    node_words.fill(0xFFFFFFFF);
    require(initialize_shader_validation_object_node(node_words, 0x11223344, 0x55667788) == &node_words
        && node_words == ShaderValidationNodeWords{0x2037E0, 0x11223344, 0x55667788, 0, 0, 1},
        "validator object node constructor retains genuine retail pointer words");
    node_words.fill(0xFFFFFFFF);
    require(initialize_shader_validation_diagnostic_node(node_words, 0x89ABCDEF, 0x12345678) == &node_words
        && node_words == ShaderValidationNodeWords{0x2037E8, 0x89ABCDEF, 0x12345678, 0, 0, 1},
        "validator diagnostic node constructor returns native initialized node");
    std::array<u8, runtime_shader_rewrite_storage_size> rewrite_storage;
    rewrite_storage.fill(0xA5);
    require(initialize_runtime_shader_rewrite_storage(rewrite_storage.data(), 0x201) == 0,
        "rewrite storage initialization returns native success");
    for (u32 offset = 0; offset < 0x200; ++offset)
    {
        require(rewrite_storage[offset] == ((offset >= 0xE0 && offset < 0xEC) ? 0x0F : 0),
            "mode one initialization retains exact enabled output bank");
    }
    u32 rewrite_options = 0;
    std::memcpy(&rewrite_options, rewrite_storage.data() + 0x204, sizeof(rewrite_options));
    require(rewrite_storage[0x200] == 0 && rewrite_storage[0x201] == 0xA5
        && rewrite_storage[0x202] == 0xA5 && rewrite_storage[0x203] == 0xA5 && rewrite_options == 0x201,
        "rewrite storage preserves retained gap and records full options");
    initialize_runtime_shader_rewrite_storage(rewrite_storage.data(), 0);
    for (u32 offset = 0; offset < 0x200; ++offset)
    {
        require(rewrite_storage[offset] == ((offset >= 0x20 && offset < 0xE0) ? 0x0F : 0),
            "disabled rewrite mode initializes only native register bank");
    }
    std::array<u32, 3> encoded{0x03215018, 0, 0x22000000};
    std::array<u8, 7> destinations{};
    decode_runtime_shader_rewrite_destinations(encoded.data(), destinations.data());
    require(destinations == std::array<u8, 7>{1, 0x23, 5, 0xE2, 3, 0xE1, 1},
        "rewrite destinations retain vector scalar and constant encoding");
    apply_runtime_shader_rewrite_destinations(encoded.data(), destinations.data());
    require(encoded == std::array<u32, 3>{0x03215018, 0, 0x22000000},
        "decoded destinations round trip through native field updates");
    destinations.fill(0);
    apply_runtime_shader_rewrite_destinations(encoded.data(), destinations.data());
    require(encoded == std::array<u32, 3>{0x00200FF8, 0, 0},
        "empty destinations remove both opcodes and retain native output sentinel");
    encoded = {0, 0, 13u << 25};
    decode_runtime_shader_rewrite_destinations(encoded.data(), destinations.data());
    require(destinations == std::array<u8, 7>{0, 0, 0, 0xF0, 8, 0, 0},
        "opcode thirteen selects special destination masks");
    std::array<u32, 3> rewrite_input{0x01200000, 0x20000000, 0x020041B0};
    std::array<u32, 6> rewrite_output;
    rewrite_output.fill(0xDEADBEEF);
    u32* rewrite_cursor = rewrite_output.data();
    initialize_runtime_shader_rewrite_storage(rewrite_storage.data(), 0x201);
    require(rewrite_runtime_shader_instruction(rewrite_storage.data(), rewrite_input.data(), rewrite_cursor) == 0
        && rewrite_cursor == rewrite_output.data() + 3
        && std::array<u32, 3>{rewrite_output[0], rewrite_output[1], rewrite_output[2]} == rewrite_input
        && rewrite_storage[0x1E2] == 1 && rewrite_storage[2] == 1,
        "rewrite instruction emits live destination and retains genuine source masks");
    initialize_runtime_shader_rewrite_storage(rewrite_storage.data(), 1);
    rewrite_cursor = rewrite_output.data();
    require(rewrite_runtime_shader_instruction(rewrite_storage.data(), rewrite_input.data(), rewrite_cursor) == 0
        && rewrite_cursor == rewrite_output.data(), "rewrite instruction suppresses unused destination in mode one");
    std::array<u32, 7> dead_writes{2, 0x01200000, 0x20000000, 0x020041B0,
        0x01200000, 0x20000000, 0x020041B0};
    require(remove_runtime_shader_dead_writes(rewrite_storage.data(), 0x201, dead_writes.data()) == 0
        && dead_writes[0] == 1 && dead_writes[1] == 0x01200000,
        "backward rewrite drops overwritten earlier write and preserves surviving instruction");
    dead_writes = {2, 0x01200000, 0x20000000, 0x020041B0, 0x01200000, 0x20000000, 14u << 25};
    const auto dead_writes_before = dead_writes;
    require(remove_runtime_shader_dead_writes(rewrite_storage.data(), 0x201, dead_writes.data()) == 0x80004005u
        && dead_writes == dead_writes_before, "failed backward rewrite retains original program");
    std::array<u8, 8> rewritten_sources;
    rewritten_sources.fill(0xA5);
    std::array<u8, 7> rewritten_destinations{};
    rewrite_input[2] = 14u << 25;
    require(decode_runtime_shader_rewrite_sources(rewrite_input.data(), rewritten_destinations.data(),
        rewritten_sources.data()) == static_cast<i32>(0x80004005u) && rewritten_sources[6] == 0xF1,
        "unsupported rewrite opcode retains native source initialization and error");
    ShaderValidationChannelOrder destroyed_order{0, {7, 8, 9, 10}};
    destroy_shader_validation_channel_order(destroyed_order);
    require(destroyed_order.vtable_address == 0x203794
        && destroyed_order.channels == std::array<u32, 4>{7, 8, 9, 10},
        "validation order destructor preserves channels and restores native vtable");
    ShaderValidationChannelMask destroyed_mask{0, {1, 7, 0xFF, 0x80}};
    destroy_shader_validation_channel_mask(destroyed_mask);
    require(destroyed_mask.vtable_address == 0x203790
        && destroyed_mask.channels == std::array<u8, 4>{1, 7, 0xFF, 0x80},
        "validation mask destructor preserves channel bytes");
    struct CleanupNode : ShaderValidationArenaNode
    {
        ShaderValidationChildTree* tree;
        CleanupNode* root_node;
        CleanupNode* replacement;
        std::vector<u32>* visits;
        u32 id;
    };
    std::vector<u32> cleanup_visits;
    CleanupNode cleanup_root{}, cleanup_first{}, cleanup_old_second{}, cleanup_replacement{};
    ShaderValidationChildTree cleanup_tree{0, 0xDEADBEEF, &cleanup_root, 3};
    const ShaderValidationArenaVtable cleanup_vtable
    {
        [](ShaderValidationArenaNode& base, u32 release)
        {
            auto& node = static_cast<CleanupNode&>(base);
            require(release == 1 && node.parent == nullptr && node.tree->vtable_address == 0x2037B8
                && node.tree->root == node.root_node && node.tree->count == 3,
                "child destruction clears parent before callback but retains owner root count until completion");
            node.visits->push_back(node.id);
            if (node.id == 1)
            {
                node.root_node->second = node.replacement;
            }
        }
    };
    for (CleanupNode* node : {&cleanup_root, &cleanup_first, &cleanup_old_second, &cleanup_replacement})
    {
        node->vtable = &cleanup_vtable;
        node->tree = &cleanup_tree;
        node->root_node = &cleanup_root;
        node->replacement = &cleanup_replacement;
        node->visits = &cleanup_visits;
        node->parent = &cleanup_root;
    }
    cleanup_first.id = 1;
    cleanup_old_second.id = 2;
    cleanup_replacement.id = 3;
    cleanup_root.first = &cleanup_first;
    cleanup_root.second = &cleanup_old_second;
    destroy_shader_validation_child_tree(cleanup_tree);
    require(cleanup_visits == std::vector<u32>{1, 3, 0} && cleanup_tree.root == nullptr
        && cleanup_tree.count == 0 && cleanup_tree.retained == 0xDEADBEEF,
        "child destruction reloads callback-mutated second child and clears owner after postorder callbacks");
    clear_shader_validation_child_tree(cleanup_tree);
    require(cleanup_visits.size() == 3, "empty child tree reset performs no virtual destruction");
    u32 heap_releases = 0;
    const std::function<void(void*, u32)> release_allocation = [&heap_releases](void* allocation, u32 tag)
    {
        require(tag == 0x24870000, "validation cleanup uses native heap tag");
        ++heap_releases;
        std::free(allocation);
    };
    void* allocated_cleanup = std::malloc(48);
    require(allocated_cleanup != nullptr, "validation cleanup fixture allocates genuine heap block");
    release_shader_validation_allocation(nullptr, release_allocation);
    release_shader_validation_allocation(allocated_cleanup, release_allocation);
    require(heap_releases == 1, "validation cleanup releases only actual nonnull heap allocation");
    std::array<u32, 4> reserved_program{1, 0x00F00000, 0xDEADBEEF, 0x12345678};
    resolve_shader_validation_reserved_register(reserved_program.data());
    require(reserved_program == std::array<u32, 4>{1, 0x00B00000, 0xDEADBEEF, 0x12345678},
        "reserved register resolver selects highest free register without modifying operands");
    std::array<u32, 10> reserved_lookahead{3, 0x00F00000, 0, 0,
        0x00B00000, (9u << 15) | 8u, 10, 0x00700000, (5u << 15) | 4u, 6};
    const auto reserved_before = reserved_lookahead;
    resolve_shader_validation_reserved_register(reserved_lookahead.data());
    require(reserved_lookahead[1] == 0x00300000,
        "reserved register resolver considers destinations and every raw operand in the next two instructions");
    for (u32 word = 2; word < reserved_lookahead.size(); ++word)
    {
        require(reserved_lookahead[word] == reserved_before[word], "reserved register resolution retains unrelated raw words");
    }
    reserved_program = {0xFFFFFFFF, 0x00F00000, 1, 2};
    const auto negative_reserved = reserved_program;
    resolve_shader_validation_reserved_register(reserved_program.data());
    require(reserved_program == negative_reserved, "reserved register resolver uses native signed program count");
    const std::array<u32, 10> original{3, 11, 22, 33, 44, 55, 66, 77, 88, 99};
    auto words = original;
    u32* program = words.data();
    require(move_runtime_shader_instruction(program, 2, 0) == 0
        && words == std::array<u32, 10>{3, 44, 55, 66, 77, 88, 99, 11, 22, 33},
        "instruction movement preserves forward shift ordering and count");
    move_runtime_shader_instruction(program, 0, 2);
    require(words == original, "instruction movement preserves backward shift ordering");
    move_runtime_shader_instruction(program, 1, 1);
    require(words == original, "equal instruction indices retain all raw words");
    const std::array<u32, 3> replacement{100, 200, 300};
    require(replace_runtime_shader_instruction_pair(program, 0, 1, replacement.data()) == 0
        && words == std::array<u32, 10>{2, 100, 200, 300, 77, 88, 99, 77, 88, 99},
        "instruction replacement shifts following records and retains stale tail");
    words = original;
    replace_runtime_shader_instruction_pair(program, 1, 2, program + 3);
    require(words[0] == 2 && words[4] == 33 && words[5] == 33 && words[6] == 33,
        "instruction replacement preserves forward copy alias rereads");
    std::array<u32, 4> scan{1, 0x01100000, 0, 0x02000000};
    u8 required = 99;
    require(runtime_shader_needs_register_one_rewrite(scan.data(), &required) == 0 && required == 1,
        "vector-only write to register one requests rewrite");
    scan = {1, 0x00210000, 0, 0x20000000};
    runtime_shader_needs_register_one_rewrite(scan.data(), &required);
    require(required == 1, "scalar-only write outside register one requests rewrite");
    scan[1] = 0x00110000;
    runtime_shader_needs_register_one_rewrite(scan.data(), &required);
    require(required == 0, "scalar-only register one preserves selection");
    scan[0] = 0xFFFFFFFF;
    runtime_shader_needs_register_one_rewrite(scan.data(), &required);
    require(required == 0, "rewrite scan uses signed count and clears output for empty input");
    std::array<u8, runtime_shader_schedule_storage_size> scheduler{};
    configure_runtime_shader_schedule(scheduler.data(), 1, 1);
    const std::array<u32, 3> no_op{};
    const std::array<u32, 3> register_read{0, 0x10000000, 0x02000000};
    require(!runtime_shader_instruction_has_cost(scheduler.data(), no_op.data())
        && runtime_shader_instruction_has_cost(scheduler.data(), register_read.data()),
        "rewrite cost gate uses actual native hazard calculation");

    std::vector<u32> visits;
    struct Arena : ShaderValidationArenaNode
    {
        std::vector<u32>* visits;
        u32 id;
    };
    const ShaderValidationArenaVtable arena_vtable
    {
        [](ShaderValidationArenaNode& base, u32 release)
        {
            auto& arena = static_cast<Arena&>(base);
            require(release == 1 && arena.parent == nullptr, "cleanup arena callback receives native cleared parent");
            arena.visits->push_back(arena.id);
        }
    };
    Arena expression{}, instruction{};
    expression.vtable = instruction.vtable = &arena_vtable;
    expression.visits = instruction.visits = &visits;
    expression.id = 1;
    instruction.id = 2;
    ShaderValidationArenaStorage expressions{&expression, 1}, instructions{&instruction, 1};
    ShaderValidationTreeNode root{}, child{};
    root.first = &child;
    ShaderValidationTreeStorage objects{&root, 2};
    struct Owned : ShaderValidationOwnedObject
    {
        ShaderValidationTreeStorage* storage;
        ShaderValidationTreeNode* node;
        ShaderValidationArenaStorage* expressions;
        ShaderValidationArenaStorage* instructions;
        std::vector<u32>* visits;
        u32 id;
    };
    const ShaderValidationOwnedObjectVtable object_vtable
    {
        [](ShaderValidationOwnedObject& base, u32 release)
        {
            auto& object = static_cast<Owned&>(base);
            require(release == 1 && object.expressions->root == nullptr && object.instructions->root == nullptr,
                "owned object cleanup follows both arena resets");
            object.visits->push_back(object.id);
            if (object.node == object.storage->root)
            {
                object.storage->root = nullptr;
            }
            else
            {
                object.storage->root->first = nullptr;
            }
            --object.storage->count;
        }
    };
    Owned first{}, last{};
    first.vtable = last.vtable = &object_vtable;
    first.storage = last.storage = &objects;
    first.expressions = last.expressions = &expressions;
    first.instructions = last.instructions = &instructions;
    first.visits = last.visits = &visits;
    first.node = &child;
    last.node = &root;
    first.id = 3;
    last.id = 4;
    child.value = &first;
    root.value = &last;
    ShaderValidationCleanupContext context{objects, expressions, instructions};
    release_shader_validation_storage(context);
    require(visits == std::vector<u32>{1, 2, 3, 4} && objects.root == nullptr && objects.count == 0,
        "validator cleanup reloads root after actual owned-object deletion callback");
    release_shader_validation_storage(context);
    require(visits.size() == 4, "empty validator cleanup makes no virtual deletion call");
}
