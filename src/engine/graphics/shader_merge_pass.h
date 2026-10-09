#pragma once
#include "gbtg/types.h"

namespace gbtg
{
// Borrowed words from the retail retry frame at EBP-0xC..EBP-0x4.
// The caller supplies retained storage; these functions never initialize it.
struct RuntimeShaderMergeScratch
{
    u32* conversion_words;
};
bool retry_runtime_shader_instruction_merge(u32* output, const u32* first, const u32* second,
    u32* error, RuntimeShaderMergeScratch& scratch); // Xbox 0x00188348
bool combine_runtime_shader_dependent_pair(u32* output, const u32* first, const u32* second,
    RuntimeShaderMergeScratch& scratch); // Xbox 0x0018A86A
u32 compact_runtime_shader_merge_runs(u32* program, RuntimeShaderMergeScratch& scratch); // Xbox 0x0018ADFB
bool combine_runtime_shader_all_patterns(u32* output, const u32* first, const u32* second,
    RuntimeShaderMergeScratch& scratch); // Xbox 0x0018ADAF
u32 compact_runtime_shader_all_merge_runs(u32* program, RuntimeShaderMergeScratch& scratch); // Xbox 0x0018AE8A
}
