#pragma once

// Structured exception handling for the retail frames that use it.
//
// Retail code raises C++ exceptions through RaiseException (code 0xE06D7363) and
// cleans up with __try/__finally frames whose handlers test AbnormalTermination().
// MSVC keeps those keywords. Other compilers have no SEH, so the same frames
// become C++ try/catch: RaiseException throws HostRaisedException, a __finally
// handler runs on both paths and rethrows after an abnormal exit, and an
// __except on one code catches HostRaisedException with that code.
//
//     GBTG_SEH_TRY_FINALLY
//     {
//         body;
//     }
//     GBTG_SEH_FINALLY
//     {
//         if (GBTG_SEH_ABNORMAL()) { cleanup; }
//     }
//     GBTG_SEH_FINALLY_END
//
//     GBTG_SEH_TRY_EXCEPT
//     {
//         body;
//     }
//     GBTG_SEH_EXCEPT_CODE(0xE06D7363)
//     {
//         handler;
//     }
//     GBTG_SEH_EXCEPT_END
//
// A __try body must not leave by return, break, continue or goto: MSVC counts that
// as abnormal termination, and the C++ form would skip the __finally handler.

#include "gbtg/types.h"
#include <array>
#include <cstdint>

#if defined(_MSC_VER)
#include <excpt.h>
#else
#include <exception>
#endif

namespace gbtg
{
// The RaiseException arguments a frame's filter can inspect.
struct HostRaisedException
{
    u32 code;
    u32 flags;
    u32 parameter_count;
    std::array<std::uintptr_t, 15> parameters;
};
}

#if defined(_MSC_VER)

#define GBTG_SEH_TRY_FINALLY __try
#define GBTG_SEH_FINALLY __finally
#define GBTG_SEH_FINALLY_END
#define GBTG_SEH_ABNORMAL() AbnormalTermination()

#define GBTG_SEH_TRY_EXCEPT __try
#define GBTG_SEH_EXCEPT_CODE(raised_code) \
    __except (GetExceptionCode() == (raised_code) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
#define GBTG_SEH_EXCEPT_END

#else

#define GBTG_SEH_TRY_FINALLY \
    { \
        std::exception_ptr gbtg_seh_pending_; \
        try
#define GBTG_SEH_FINALLY \
        catch (...) \
        { \
            gbtg_seh_pending_ = std::current_exception(); \
        }
#define GBTG_SEH_FINALLY_END \
        if (gbtg_seh_pending_) \
        { \
            std::rethrow_exception(gbtg_seh_pending_); \
        } \
    }
#define GBTG_SEH_ABNORMAL() (gbtg_seh_pending_ != nullptr)

#define GBTG_SEH_TRY_EXCEPT try
#define GBTG_SEH_EXCEPT_CODE(raised_code) \
    catch (const ::gbtg::HostRaisedException& gbtg_seh_raised_) \
    { \
        if (gbtg_seh_raised_.code != static_cast<::gbtg::u32>(raised_code)) \
        { \
            throw; \
        }
#define GBTG_SEH_EXCEPT_END \
    }

#endif
