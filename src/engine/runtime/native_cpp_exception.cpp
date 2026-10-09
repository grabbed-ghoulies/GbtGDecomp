#include "native_cpp_exception.h"
#include <cstdint>

#if defined(_WIN32)
extern "C"
{
    __declspec(dllimport) void __stdcall RaiseException(unsigned long code, unsigned long flags, unsigned long count,
        const std::uintptr_t* arguments);
}
#else
#error The approved native exception dispatch requires Windows RaiseException.
#endif

namespace gbtg
{
// Xbox 0x001679C7
void dispatch_runtime_cpp_exception(std::span<const u32, 8> record_template, const void* object, const void* type_info)
{
    std::array<u32, 8> record;
    u32* output = record.data();
    const u32* input = record_template.data();
    i32 remaining = 8;
    do
    {
        *output = *input;
        ++output;
        ++input;
        --remaining;
    } while (remaining != 0);
    std::array<std::uintptr_t, 3> arguments
    {
        record[5],
        reinterpret_cast<std::uintptr_t>(object),
        reinterpret_cast<std::uintptr_t>(type_info)
    };
    RaiseException(record[0], record[1], record[4], arguments.data());
}

// Xbox 0x0018E935
void raise_invalid_shader_validation_operand(const RuntimeCppExceptionContext& context)
{
    u8 object = 0x21;
    dispatch_runtime_cpp_exception(context.record_template, &object, context.shader_validation_type_info);
}
}
