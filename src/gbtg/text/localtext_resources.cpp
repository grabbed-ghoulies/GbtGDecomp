#include "localtext_resources.h"
#include "gbtg/entity.h"
#include "../../engine/runtime/crt_format.h"

namespace gbtg
{
// Xbox 0x001660BF
char16_t* append_runtime_wide_text(char16_t* output, const char16_t* input)
{
    char16_t* result = output;
    while (*output != 0)
    {
        ++output;
    }
    char16_t unit;
    do
    {
        unit = *input;
        *output = unit;
        ++output;
        ++input;
    }
    while (unit != 0);
    return result;
}

// Xbox 0x0014AA05
void append_xapi_wide_text(char16_t* output, const char16_t* input)
{
    append_runtime_wide_text(output, input);
}

// Xbox 0x000FF120
u32 lookup_borrowed_localtext(std::span<const u8> resource, const char* group,
    const char* key, char16_t* output, LocalTextResourceContext& context)
{
    char sanitized[256];
    char combined[256];
    runtime_localtext_state_size();
    context.state = static_cast<RuntimeLocalTextState*>(engine_heap_allocate(sizeof(RuntimeLocalTextState)));
    const auto& view = context.views.at(resource.data());
    initialize_runtime_localtext(view.file, *context.state);
    sanitize_localtext_key(sanitized, key);
    scene_detail::crt_format_string(combined, "%s__%s", group, sanitized);
    const char16_t* text;
    const u32 id = find_runtime_localtext(view.keys, *context.state, combined, text);
    if (id == 0xFFFFFFFFu)
    {
        *output = 0;
        engine_heap_free(context.state);
        return 0;
    }
    copy_xapi_wide_text(output, text);
    engine_heap_free(context.state);
    return 1;
}

// Xbox 0x000FF0A0
void request_namespaced_localtext(const char* resource, const char* group,
    const char* key, char16_t* output, LocalTextResourceContext& context)
{
    char resource_name[128];
    char sanitized[256];
    char combined[256];
    if (resource != nullptr)
    {
        char* destination = resource_name;
        char value;
        do
        {
            value = *resource;
            *destination = value;
            ++resource;
            ++destination;
        }
        while (value != '\0');
    }
    sanitize_localtext_key(sanitized, key);
    scene_detail::crt_format_string(combined, "%s__%s", group, sanitized);
    acquire_localtext_key(resource_name, combined, output, context);
}

// Xbox 0x001660E9
char16_t* copy_runtime_wide_text(char16_t* output, const char16_t* input)
{
    char16_t* cursor = output;
    char16_t value;
    do
    {
        value = *input;
        *cursor = value;
        ++cursor;
        ++input;
    }
    while (value != 0);
    return output;
}

// Xbox 0x0014A961
char16_t* copy_xapi_wide_text(char16_t* output, const char16_t* input)
{
    return copy_runtime_wide_text(output, input);
}

// Xbox 0x000FF410
u32 acquire_localtext_key(const char* resource, const char* key, char16_t* output,
    LocalTextResourceContext& context)
{
    std::span<const u8> acquired;
    if (read_u32({reinterpret_cast<const u8*>(resource), 4}, 0) == 0x5F646961u)
    {
        acquired = acquire_animation_resource(context.registry, resource);
        if (acquired.data() == nullptr)
        {
            resource_error(context.error, 2);
        }
    }
    context.acquired = acquired;
    runtime_localtext_state_size();
    context.state = static_cast<RuntimeLocalTextState*>(
        engine_heap_allocate(sizeof(RuntimeLocalTextState)));
    // Native view of the actual relocated resource. Missing views are missing
    // resources; they cannot provide an empty table or simulated lookup result.
    const auto& view = context.views.at(acquired.data());
    initialize_runtime_localtext(view.file, *context.state);
    const char16_t* text;
    const u32 id = find_runtime_localtext(view.keys, *context.state, key, text);
    if (id != 0xFFFFFFFFu)
    {
        copy_xapi_wide_text(output, text);
        engine_heap_free(context.state);
        release_animation_resource(context.registry, context.acquired);
        context.acquired = {};
        return 1;
    }
    *output = 0;
    engine_heap_free(context.state);
    release_animation_resource(context.registry, context.acquired);
    return 0;
}
}
