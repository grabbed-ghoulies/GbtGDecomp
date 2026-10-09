#include "dialogue_time.h"
#include "../runtime/crt_format.h"
#include "../audio/listener_settings.h"
#include <bit>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x000FBB20
void append_dialogue_minutes(char16_t* output, float seconds, i32 omit_days,
    LocalTextResourceContext& context)
{
    const auto resource = acquire_animation_resource(context.registry, "aid_loctext_ghoulies_time");
    if (resource.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    char16_t minute_plural[16];
    char16_t minute_singular[16];
    char16_t hour_singular[16];
    char16_t hour_plural[16];
    char16_t day_singular[16];
    char16_t day_plural[16];
    char16_t text[64];
    lookup_borrowed_localtext(resource, "time", "daySingular", day_singular, context);
    lookup_borrowed_localtext(resource, "time", "dayPlural", day_plural, context);
    lookup_borrowed_localtext(resource, "time", "hourSingular", hour_singular, context);
    lookup_borrowed_localtext(resource, "time", "hourPlural", hour_plural, context);
    lookup_borrowed_localtext(resource, "time", "minuteSingular", minute_singular, context);
    lookup_borrowed_localtext(resource, "time", "minutePlural", minute_plural, context);
    release_animation_resource(context.registry, resource);
    animation_remainder(seconds, 60.f);
    const float minutes = seconds * std::bit_cast<float>(0x3C888889u);
    const i32 minute = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(animation_remainder(minutes, 60.f))));
    i32 hour = _mm_cvtt_ss2si(_mm_set_ss(minutes * std::bit_cast<float>(0x3C888889u)));
    if (omit_days == 0)
    {
        const i32 day = hour / 24;
        hour %= 24;
        if (day != 0)
        {
            const char16_t* minute_unit = minute_singular;
            if (minute != 1)
            {
                minute_unit = minute_plural;
            }
            const char16_t* hour_unit = hour_singular;
            if (hour % 24 != 1)
            {
                hour_unit = hour_plural;
            }
            const char16_t* day_unit = day_singular;
            if (day != 1)
            {
                day_unit = day_plural;
            }
            scene_detail::xapi_format_wide_string(text, u"%d %s %d %s %d %s", day, day_unit, hour % 24, hour_unit, minute, minute_unit);
            append_xapi_wide_text(output, text);
            return;
        }
    }
    if (hour == 0)
    {
        if (minute == 0)
        {
            scene_detail::xapi_format_wide_string(text, u"0 %s", minute_plural);
        }
        else
        {
            const char16_t* unit = minute_singular;
            if (minute != 1)
            {
                unit = minute_plural;
            }
            scene_detail::xapi_format_wide_string(text, u"%d %s", minute, unit);
        }
    }
    else
    {
        const char16_t* minute_unit = minute_singular;
        if (minute != 1)
        {
            minute_unit = minute_plural;
        }
        const char16_t* hour_unit = hour_singular;
        if (hour != 1)
        {
            hour_unit = hour_plural;
        }
        scene_detail::xapi_format_wide_string(text, u"%d %s %d %s", hour, hour_unit, minute, minute_unit);
    }
    append_xapi_wide_text(output, text);
}

// Xbox 0x000FB860
void append_dialogue_seconds(char16_t* output, float seconds, i32 omit_days,
    LocalTextResourceContext& context)
{
    const auto resource = acquire_animation_resource(context.registry, "aid_loctext_ghoulies_time");
    if (resource.data() == nullptr)
    {
        resource_error(context.error, 2);
    }
    char16_t second_plural[16];
    char16_t second_singular[16];
    char16_t minute_plural[16];
    char16_t minute_singular[16];
    char16_t hour_plural[16];
    char16_t hour_singular[16];
    char16_t day_singular[16];
    char16_t day_plural[16];
    char16_t text[64];
    lookup_borrowed_localtext(resource, "time", "daySingular", day_singular, context);
    lookup_borrowed_localtext(resource, "time", "dayPlural", day_plural, context);
    lookup_borrowed_localtext(resource, "time", "hourSingular", hour_singular, context);
    lookup_borrowed_localtext(resource, "time", "hourPlural", hour_plural, context);
    lookup_borrowed_localtext(resource, "time", "minuteSingular", minute_singular, context);
    lookup_borrowed_localtext(resource, "time", "minutePlural", minute_plural, context);
    lookup_borrowed_localtext(resource, "time", "secondSingular", second_singular, context);
    lookup_borrowed_localtext(resource, "time", "secondPlural", second_plural, context);
    release_animation_resource(context.registry, resource);
    const i32 second = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(animation_remainder(seconds, 60.f))));
    const float minutes = seconds * std::bit_cast<float>(0x3C888889u);
    const i32 minute = std::bit_cast<i32>(static_cast<u32>(truncate_runtime_float(animation_remainder(minutes, 60.f))));
    i32 hour = _mm_cvtt_ss2si(_mm_set_ss(minutes * std::bit_cast<float>(0x3C888889u)));
    if (omit_days == 0)
    {
        const i32 day = hour / 24;
        hour %= 24;
        if (day != 0)
        {
            const char16_t* second_unit;
            const char16_t* minute_unit;
            if (minute == 1)
            {
                second_unit = second_singular;
                minute_unit = minute_singular;
            }
            else
            {
                second_unit = second_plural;
                minute_unit = minute_plural;
            }
            const char16_t* hour_unit = hour_singular;
            if (hour % 24 != 1)
            {
                hour_unit = hour_plural;
            }
            const char16_t* day_unit = day_singular;
            if (day != 1)
            {
                day_unit = day_plural;
            }
            scene_detail::xapi_format_wide_string(text, u"%d %s %d %s %d %s %d %s",
                day, day_unit, hour % 24, hour_unit, minute, minute_unit, second, second_unit);
            append_xapi_wide_text(output, text);
            return;
        }
    }
    if (hour == 0)
    {
        if (minute == 0)
        {
            if (second == 0)
            {
                scene_detail::xapi_format_wide_string(text, u"0 %s", second_plural);
            }
            else
            {
                const char16_t* unit = second_singular;
                if (static_cast<float>(second) != 1.f)
                {
                    unit = second_plural;
                }
                scene_detail::xapi_format_wide_string(text, u"%d %s", second, unit);
            }
        }
        else
        {
            const char16_t* second_unit = second_singular;
            if (second != 1)
            {
                second_unit = second_plural;
            }
            const char16_t* minute_unit = minute_singular;
            if (minute != 1)
            {
                minute_unit = minute_plural;
            }
            scene_detail::xapi_format_wide_string(text, u"%d %s %d %s", minute, minute_unit, second, second_unit);
        }
    }
    else
    {
        const char16_t* second_unit = second_singular;
        if (second != 1)
        {
            second_unit = second_plural;
        }
        const char16_t* minute_unit = minute_singular;
        if (minute != 1)
        {
            minute_unit = minute_plural;
        }
        const char16_t* hour_unit = hour_singular;
        if (hour != 1)
        {
            hour_unit = hour_plural;
        }
        scene_detail::xapi_format_wide_string(text, u"%d %s %d %s %d %s", hour, hour_unit, minute, minute_unit, second, second_unit);
    }
    append_xapi_wide_text(output, text);
}
}
