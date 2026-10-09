#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "../src/engine/runtime/native_cpp_exception.h"
#include <stdexcept>
#include <cstdio>

namespace
{
int inspect_exception(EXCEPTION_POINTERS* pointers, const void* expected_object, const void* expected_type,
    bool inspect_byte, const gbtg::u32* expected_record, bool* result)
{
    const auto& record = *pointers->ExceptionRecord;
    *result = record.ExceptionCode == expected_record[0]
        && (record.ExceptionFlags & ~EXCEPTION_SOFTWARE_ORIGINATE) == expected_record[1]
        && record.NumberParameters == expected_record[4] && record.ExceptionInformation[0] == expected_record[5]
        && record.ExceptionInformation[2] == reinterpret_cast<ULONG_PTR>(expected_type);
    if (inspect_byte)
    {
        *result = *result && *reinterpret_cast<const gbtg::u8*>(record.ExceptionInformation[1]) == 0x21;
    }
    else
    {
        *result = *result && record.ExceptionInformation[1] == reinterpret_cast<ULONG_PTR>(expected_object);
    }
    if (!*result)
    {
        std::fprintf(stderr, "exception record: code=%08lx flags=%08lx count=%lu magic=%llx object=%llx type=%llx expected=%llx byte=%u\n",
            record.ExceptionCode, record.ExceptionFlags, record.NumberParameters,
            static_cast<unsigned long long>(record.ExceptionInformation[0]),
            static_cast<unsigned long long>(record.ExceptionInformation[1]),
            static_cast<unsigned long long>(record.ExceptionInformation[2]),
            static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(expected_type)),
            inspect_byte ? *reinterpret_cast<const gbtg::u8*>(record.ExceptionInformation[1]) : 0);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

bool observe_exception(const gbtg::RuntimeCppExceptionContext& context, bool invalid_operand)
{
    bool result = false;
    gbtg::u32 object = 0x12345678;
    __try
    {
        if (invalid_operand)
        {
            gbtg::raise_invalid_shader_validation_operand(context);
        }
        else
        {
            gbtg::dispatch_runtime_cpp_exception(context.record_template, &object, context.shader_validation_type_info);
        }
    }
    __except(inspect_exception(GetExceptionInformation(), &object, context.shader_validation_type_info,
        invalid_operand, context.record_template.data(), &result))
    {
    }
    return result;
}
}

void check_runtime_cpp_exception_dispatch()
{
    const gbtg::RuntimeCppExceptionContext context{gbtg::retail_cpp_exception_template,
        gbtg::retail_shader_validation_throw_info.data()};
    auto copied_record = gbtg::retail_cpp_exception_template;
    copied_record[0] = 0xE0424242;
    copied_record[5] = 0x12345678;
    const gbtg::RuntimeCppExceptionContext changed_context{copied_record,
        gbtg::retail_shader_validation_throw_info.data()};
    if (!observe_exception(context, false) || !observe_exception(context, true)
        || !observe_exception(changed_context, false))
    {
        throw std::runtime_error("real exception dispatch must preserve native code, flags, arguments and live operand byte");
    }
}
