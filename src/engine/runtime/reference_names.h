#pragma once
#include "gbtg/types.h"

namespace gbtg
{
struct RuntimeReferenceName
{
    char* name; // Retail row +0, stride 0xC.
    u16 references; // +4.
    u16 length; // +6, includes NUL.
    u32 hash; // +8.
};
struct RuntimeReferenceNames
{
    RuntimeReferenceName* begin;
    RuntimeReferenceName* end;
    char* strings;
    char* cursor;
    char* limit;
};
RuntimeReferenceNames* allocate_runtime_reference_names(u32 bytes); // Xbox 0x00066430
RuntimeReferenceName* find_runtime_reference_name(RuntimeReferenceNames& pool,
    const char* name); // Xbox 0x000666F0
i32 retain_runtime_reference_name(RuntimeReferenceNames*& pool,
    const char* name); // Xbox 0x00066530
void release_runtime_reference_names(RuntimeReferenceNames* pool); // Xbox 0x000664A0
void release_runtime_reference_name_owner(RuntimeReferenceNames** owner); // Xbox 0x00035760
}
