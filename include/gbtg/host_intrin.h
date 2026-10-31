#pragma once

// Compiler intrinsics retail translations use. MSVC supplies them through <intrin.h>;
// GCC and Clang get the x86 header and a __debugbreak that traps the same way (int 3).

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#if defined(__clang__)
#define __debugbreak() __builtin_debugtrap()
#else
#define __debugbreak() __asm__ volatile("int3")
#endif
#endif
