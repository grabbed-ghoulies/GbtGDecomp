#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "../src/engine/graphics/shader_validation_constructors.h"
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <utility>

namespace
{
void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

bool observe_invalid_operand(gbtg::ShaderValidationConstructorContext& context,
    gbtg::ShaderValidationOperand& operand, gbtg::u32 address)
{
    __try
    {
        gbtg::construct_shader_validation_operand(context, operand, address, 0, nullptr, 2, nullptr, 7);
    }
    __except(GetExceptionCode() == 0xe06d7363 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return true;
    }
    return false;
}

bool observe_owner_failure(gbtg::ShaderValidationConstructorContext& context,
    gbtg::ShaderValidationOwner& owner, gbtg::u32 address, gbtg::ShaderValidationOwner* parent)
{
    __try
    {
        gbtg::construct_shader_validation_owner(context, owner, address, 2, 1,
            parent, 9, gbtg::ShaderValidationChannelMask{0, {1, 1, 1, 1}});
    }
    __except(GetExceptionCode() == 0xe06d7363 ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
    {
        return true;
    }
    return false;
}
}

void check_shader_validation_constructors()
{
    using namespace gbtg;
    ShaderValidationBalancedTree objects_tree{nullptr, 0};
    std::vector<std::pair<const void*, u32>> addresses;
    const auto address_of = [&](const void* pointer) -> u32
    {
        for (const auto& entry : addresses)
        {
            if (entry.first == pointer)
            {
                return entry.second;
            }
        }
        const u32 address = 0x1000 + static_cast<u32>(addresses.size()) * 0x100;
        addresses.emplace_back(pointer, address);
        return address;
    };
    const auto allocate_node = [](std::size_t size, u32 tag) -> void*
    {
        require(size == sizeof(ShaderValidationBalancedNode) && tag == 0x24870000,
            "constructor registration uses genuine node heap allocation");
        return std::malloc(size);
    };
    ShaderValidationInsertContext objects{objects_tree, allocate_node};
    u32 deletions = 0;
    const auto delete_node = [&](ShaderValidationBalancedNode& node, u32 flags)
    {
        require(flags == 1, "constructor cleanup requests deleting destructor");
        ++deletions;
        std::free(&node);
    };
    ShaderValidationEraseContext erase{objects_tree, delete_node};
    const RuntimeCppExceptionContext exceptions{retail_cpp_exception_template,
        retail_shader_validation_throw_info.data()};
    ShaderValidationOwner active{};
    ShaderValidationOwner* active_owner = &active;
    ShaderValidationInsertContext diagnostics{active.diagnostics.nodes, allocate_node};
    bool fail_operand_allocation = false;
    bool raise_during_allocation = false;
    bool fail_diagnostic_registration = false;
    u32 allocation_releases = 0;
    ShaderValidationConstructorContext context
    {
        objects, erase, exceptions, active_owner,
        [&](std::size_t size, u32 tag) -> void*
        {
            require(size == sizeof(ShaderValidationOperand) && tag == 0x24870000,
                "owner constructor requests genuine operand heap allocation");
            if (raise_during_allocation)
            {
                raise_invalid_shader_validation_operand(exceptions);
            }
            if (fail_operand_allocation)
            {
                return nullptr;
            }
            return std::malloc(size);
        },
        [&](void* allocation, u32 tag)
        {
            require(tag == 0x24870000, "constructor unwind preserves release tag");
            require(objects_tree.count == 1,
                "nested operand unregister precedes allocation release and owner unregister");
            ++allocation_releases;
            std::free(allocation);
        },
        address_of,
        [&](ShaderValidationOwner& owner) -> ShaderValidationInsertContext&
        {
            require(&owner.diagnostics.nodes == &diagnostics.tree,
                "diagnostic registry aliases the actual owner tree");
            if (fail_diagnostic_registration)
            {
                raise_invalid_shader_validation_operand(exceptions);
            }
            return diagnostics;
        },
        delete_node
    };
    ShaderValidationOperand invalid{};
    const u32 invalid_address = address_of(&invalid);
    require(observe_invalid_operand(context, invalid, invalid_address),
        "invalid operand raises the actual retail exception");
    require(objects_tree.root == nullptr && objects_tree.count == 0 && deletions == 1
        && invalid.base.vtable_address == 0x203808 && invalid.channel_order.vtable_address == 0x203794,
        "actual exception unwinds channel order then registered base");

    ShaderValidationOwner owner{};
    owner.diagnostics.retained = 0xabcdef01;
    const u32 owner_address = address_of(&owner);
    require(construct_shader_validation_owner(context, owner, owner_address, 5, 0x1234,
        nullptr, 0x55, ShaderValidationChannelMask{0, {1, 1, 1, 1}}) == &owner,
        "owner constructor returns its actual object");
    require(owner.base.vtable_address == 0x203814 && owner.kind == 5 && owner.source == 0x1234
        && owner.channels == std::array<u32, 4>{0x55, 0, 0, 0}
        && owner.diagnostics.retained == 0xabcdef01 && owner.diagnostics.nodes.count == 0
        && owner.derived_owner == nullptr && owner.channel_owner == nullptr && objects_tree.count == 2,
        "kind five owner decodes mask eight and preserves child retained word");
    require(owner.operand != nullptr && owner.operand->kind == 3 && owner.operand->owner == &owner
        && owner.operand->parent == active_owner && owner.operand->marker == 0,
        "owner operand preserves null-source kind-three and active-parent fallback");
    destroy_shader_validation_registered_object(erase, owner.operand->base, address_of(owner.operand));
    std::free(owner.operand);
    destroy_shader_validation_registered_object(erase, owner.base, owner_address);

    fail_operand_allocation = true;
    ShaderValidationOwner failed{};
    const u32 failed_address = address_of(&failed);
    construct_shader_validation_owner(context, failed, failed_address, 2, 3,
        nullptr, 9, ShaderValidationChannelMask{0, {0, 0x80, 0, 0xff}});
    require(failed.operand == nullptr && failed.channels == std::array<u32, 4>{0, 9, 0, 9}
        && objects_tree.count == 1, "failed operand allocation preserves owner and caller mask");
    destroy_shader_validation_registered_object(erase, failed.base, failed_address);
    require(objects_tree.count == 0 && objects_tree.root == nullptr,
        "constructor fixture releases every actual registration");

    fail_operand_allocation = false;
    fail_diagnostic_registration = true;
    active_owner = nullptr;
    ShaderValidationOwner unwound{};
    unwound.diagnostics.retained = 0x12345678;
    require(observe_owner_failure(context, unwound, address_of(&unwound), &active),
        "owner constructor propagates real nested registration exception");
    require(allocation_releases == 1 && objects_tree.root == nullptr && objects_tree.count == 0
        && unwound.base.vtable_address == 0x203808
        && unwound.diagnostics.vtable_address == 0x2037b8
        && unwound.diagnostics.nodes.root == nullptr && unwound.diagnostics.nodes.count == 0
        && unwound.diagnostics.retained == 0x12345678,
        "owner state-three unwind releases allocation then children and registered base");

    raise_during_allocation = true;
    fail_diagnostic_registration = false;
    ShaderValidationOwner early_unwound{};
    const u32 early_address = address_of(&early_unwound);
    require(observe_owner_failure(context, early_unwound, early_address, &active),
        "allocation exception propagates before owner reaches state three");
    require(allocation_releases == 1 && objects_tree.count == 1
        && early_unwound.base.vtable_address == 0x203814,
        "retail state zero unwinds only mask and retains base registration");
    destroy_shader_validation_registered_object(erase, early_unwound.base, early_address);
}
