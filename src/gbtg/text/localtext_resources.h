#pragma once

#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/runtime_localtext.h"
#include <unordered_map>

namespace gbtg
{
struct LocalTextResourceView
{
    std::span<const u8> file; // relocated resource +0
    std::span<const RuntimeLocalTextKey> keys; // +4, count +8
};
struct LocalTextResourceContext
{
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    const std::unordered_map<const u8*, LocalTextResourceView>& views;
    std::span<const u8> acquired; // DAT_0042A714
    RuntimeLocalTextState* state = nullptr; // DAT_0042A718
};
char16_t* copy_runtime_wide_text(char16_t* output, const char16_t* input); // Xbox 0x001660E9
char16_t* copy_xapi_wide_text(char16_t* output, const char16_t* input); // Xbox 0x0014A961
char16_t* append_runtime_wide_text(char16_t* output, const char16_t* input); // Xbox 0x001660BF
void append_xapi_wide_text(char16_t* output, const char16_t* input); // Xbox 0x0014AA05
u32 acquire_localtext_key(const char* resource, const char* key, char16_t* output,
    LocalTextResourceContext& context); // Xbox 0x000FF410
void request_namespaced_localtext(const char* resource, const char* group,
    const char* key, char16_t* output, LocalTextResourceContext& context); // Xbox 0x000FF0A0
u32 lookup_borrowed_localtext(std::span<const u8> resource, const char* group,
    const char* key, char16_t* output, LocalTextResourceContext& context); // Xbox 0x000FF120
}
