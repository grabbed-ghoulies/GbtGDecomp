#pragma once
#include "gbtg/resource_error.h"

namespace gbtg
{
struct MovieErrorMessage
{
    i32 code;
    const char* title;
    const char* message;
};
struct MovieErrorContext
{
    const MovieErrorMessage* messages; // DAT_0042A180, zero-code terminated.
    char* fallback; // DAT_00506608, formatted with capacity 15.
    ResourceErrorState& error;
};
void lookup_movie_error_message(i32 code, const char*& title, const char*& message,
    MovieErrorContext& context); // Xbox 0x000E96A0
i32 report_movie_error(i32 code, MovieErrorContext& context); // Xbox 0x001074A0
}
