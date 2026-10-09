#pragma once

#include "gbtg/types.h"
#include <functional>

namespace gbtg
{
struct RuntimeLocalTextState
{
    const u8* chunk;
    const u8* entries;
    const char16_t* strings;
    u32 disabled;
    u32 retained;
};
struct RuntimeLocalTextKey
{
    const char* name;
    u16 hash;
    u16 id;
};

bool text_key_present(const char* key); // Xbox 0x000FBF90
void sanitize_localtext_key(char* output, const char* key); // Xbox 0x000FF3C0
u32 runtime_localtext_state_size(); // Xbox 0x00162910
void initialize_runtime_localtext(std::span<const u8> file,
    RuntimeLocalTextState& state); // Xbox 0x00162920
u32 runtime_localtext_hash(const char* key); // Xbox 0x00162A80
i32 compare_localtext_id(std::uintptr_t key, const u8* record); // Xbox 0x00162A20
const u8* runtime_binary_search(std::uintptr_t key, const u8* base, u32 count,
    u32 stride, const std::function<i32(std::uintptr_t, const u8*)>& compare); // Xbox 0x00167F86
const char16_t* runtime_localtext_by_id(const RuntimeLocalTextState* state,
    u32 id); // Xbox 0x00162A30
u32 find_runtime_localtext(std::span<const RuntimeLocalTextKey> keys,
    const RuntimeLocalTextState& state, const char* key,
    const char16_t*& output); // Xbox 0x00162950
}
