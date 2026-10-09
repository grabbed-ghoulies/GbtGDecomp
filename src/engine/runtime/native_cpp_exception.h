#pragma once
#include "gbtg/types.h"
#include <array>
#include <span>

namespace gbtg
{
inline constexpr std::array<u32, 8> retail_cpp_exception_template
{
    0xe06d7363, 1, 0, 0, 3, 0x19930520, 0, 0
}; // DAT_0020910c, eight words copied by 0x001679c7.

inline constexpr std::array<u32, 4> retail_shader_validation_throw_info
{
    0, 0, 0, 0x0032b79c
}; // Raw ThrowInfo at 0x0032b7a4; its final word remains a retail address.

struct RuntimeCppExceptionContext
{
    std::span<const u32, 8> record_template;
    const void* shader_validation_type_info; // Host mapping of retail throw-info 0x0032b7a4.
};

void dispatch_runtime_cpp_exception(std::span<const u32, 8> record_template, const void* object, const void* type_info); // Xbox 0x001679C7
void raise_invalid_shader_validation_operand(const RuntimeCppExceptionContext& context); // Xbox 0x0018E935
}
