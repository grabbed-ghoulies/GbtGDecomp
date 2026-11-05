#include "player_overlay.h"
#include "player_mind_setup.h"
#include "actor_transform.h"
#include "entity.h"
#include "../audio/listener_settings.h"
#include "gbtg/anim.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
// 0x0001E3E0 keeps its digits, widths, direction and sprite position in overlapping stack
// words; this mirrors its frame from ESP+0 to ESP+0x44 after the two register saves.
struct DigitFrame
{
    std::array<u32, 0x12> words{};

    float get_float(size_t offset) const
    {
        return std::bit_cast<float>(words[offset / 4]);
    }
    void set_float(size_t offset, float value)
    {
        words[offset / 4] = std::bit_cast<u32>(value);
    }
    i32 get_int(size_t offset) const
    {
        return std::bit_cast<i32>(words[offset / 4]);
    }
    void set_int(size_t offset, i32 value)
    {
        words[offset / 4] = std::bit_cast<u32>(value);
    }
};

float digit_width(const u8* const* digits, i32 digit)
{
    if (digits[digit] == nullptr)
    {
        // DAT_00545A80 is filled by 0x0012EE60 under the scoreboard set-up 0x00132040; retail reads +8 of null.
        throw Error("scoreboard energy digit DAT_00545A80 is not loaded");
    }
    u16 width;
    std::memcpy(&width, digits[digit] + 8, 2);
    return float(i32(width)) * std::bit_cast<float>(0x3CA3D70Au); // DAT_0032B3E4.
}

i32 truncate_float(float value)
{
    return _mm_cvttss_si32(_mm_set_ss(value));
}
}

// Xbox 0x0002AC70
void normalize_vector_in_place(float* vector)
{
    const float length = vector[1] * vector[1] + vector[0] * vector[0] + vector[2] * vector[2];
    if (length != 0.0f)
    {
        const long double scale = retail_reciprocal_sqrt(length);
        vector[0] = float(scale * vector[0]);
        vector[1] = float(scale * vector[1]);
        vector[2] = float(scale * vector[2]);
    }
}

// Xbox 0x0003DD50
void runtime_actor_overlay_anchor(const Actor& actor, Vec3& position, float& height, float& width)
{
    position = actor_world_position(actor);
    position.y = actor.radius_y * actor.model_size_z + position.y;
    height = engine_absolute(float((static_cast<long double>(actor.model_size_y) - actor.model_size_z) * actor.radius_y));
    width = engine_absolute(float(static_cast<long double>(actor.radius_xz) * actor.model_size_x));
}

// Xbox 0x0001E2A0
u32 runtime_body_health_color(std::span<const u8> body)
{
    const u16 maximum = body_record(body).max_energy;
    if (maximum != 0)
    {
        const float ratio = float(i32(body_record(body).energy)) / float(i32(maximum));
        if (0.25f > ratio) // DAT_0032B104.
        {
            return 0xFF2020FF; // DAT_00217D60.
        }
        if (0.5f > ratio) // DAT_0032B0F4.
        {
            return 0x000080FF; // DAT_00217D64.
        }
        if (0.75f > ratio) // DAT_0032B2AC.
        {
            return 0xFF00FFFF; // DAT_00217D68.
        }
    }
    return 0xFF00FF00; // DAT_00217D6C.
}

// Xbox 0x0001E770
u32 runtime_player_uses_alternate_overlay(std::span<const u8> actor, std::span<const u8> attributes)
{
    if (get(actor, player_field::goal_volume) != 0)
    {
        return 0;
    }
    if (!(get(actor, player_field::countdown_56) > 0.0f) && get(actor, player_field::provoked) == 0)
    {
        return 0;
    }
    if (attributes.data() == nullptr || read_u32(attributes, 0xB8) == 0)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x0001E3E0
void queue_runtime_overlay_digits(i32 value, const Vec3& position, u32 color, const u8* const* digits,
    const DisplayView& view, BillboardQueue& queue)
{
    DigitFrame frame;
    i32 count;
    if (value == 0)
    {
        frame.set_int(0x20, 0);
        count = 1;
    }
    else
    {
        i32 index = 0;
        i32 remaining = value;
        while (index <= 3)
        {
            const i32 quotient = remaining / 10;
            const i32 digit = remaining % 10;
            ++index;
            frame.set_int(0x1C + size_t(index) * 4, digit);
            remaining = quotient;
            if (quotient == 0)
            {
                break;
            }
        }
        count = index;
    }
    float total = 0.0f;
    i32 index = 0;
    if (count >= 4)
    {
        do
        {
            const size_t base = size_t(index) * 4;
            const float first = digit_width(digits, frame.get_int(0x20 + base));
            const float second = digit_width(digits, frame.get_int(0x24 + base));
            const float third = digit_width(digits, frame.get_int(0x28 + base));
            const float fourth = digit_width(digits, frame.get_int(0x2C + base));
            frame.set_float(0x2C + base, first);
            frame.set_float(0x30 + base, second);
            frame.set_float(0x34 + base, third);
            frame.set_float(0x38 + base, fourth);
            total = first + total + second + third + fourth;
            index += 4;
        } while (index < count - 3);
    }
    for (; index < count; ++index)
    {
        const float width = digit_width(digits, frame.get_int(0x20 + size_t(index) * 4));
        frame.set_float(0x2C + size_t(index) * 4, width);
        total = width + total;
    }
    const float side = view.forward.y * 0.0f;
    frame.set_float(0x38, side - view.forward.z);
    frame.set_float(0x3C, view.forward.z * 0.0f - view.forward.x * 0.0f);
    frame.set_float(0x40, view.forward.x - side);
    i32 digit = count - 1;
    const float spread = float(digit) * 0.2f + total; // DAT_0032B1AC.
    frame.set_float(0x18, frame.get_float(0x3C));
    frame.set_float(0x1C, frame.get_float(0x40));
    frame.set_float(0x14, frame.get_float(0x38));
    float x = frame.get_float(0x14);
    float y = frame.get_float(0x18);
    const float z = frame.get_float(0x1C);
    const float length = z * z + y * y + x * x;
    frame.set_float(0xC, spread);
    frame.set_float(0x8, length);
    if (length != 0.0f)
    {
        const long double scale = retail_reciprocal_sqrt(length);
        frame.set_float(0x14, float(frame.get_float(0x14) * scale));
        x = frame.get_float(0x14);
        frame.set_float(0x18, float(frame.get_float(0x18) * scale));
        y = frame.get_float(0x18);
        frame.set_float(0x1C, float(frame.get_float(0x1C) * scale));
    }
    frame.set_float(0xC, 1.0f);
    frame.set_float(0x10, 1.0f);
    float offset;
    if (count > 1)
    {
        offset = frame.get_float(0x2C) * 0.5f + spread * -0.5f; // DAT_0032B0F4, DAT_0032B1C4.
    }
    else
    {
        offset = 0.0f;
    }
    for (; digit >= 0; --digit)
    {
        const u8* sprite = digits[frame.get_int(0x20 + size_t(digit) * 4)];
        const float sprite_x = position.x + x * offset;
        frame.set_float(0x3C, position.y + y * offset);
        frame.set_float(0x40, position.z + frame.get_float(0x1C) * offset);
        frame.set_float(0x38, sprite_x);
        const Vec3 at{frame.get_float(0x38), frame.get_float(0x3C), frame.get_float(0x40)};
        const float size[2]{frame.get_float(0xC), frame.get_float(0x10)};
        queue_billboard(queue, sprite, color, 0xFF, at, size, 0.0f);
        offset = frame.get_float(0x2C + size_t(digit) * 4) + offset + 0.2f;
    }
}

// Xbox 0x0001DF70
void queue_runtime_body_overlay(i32 kind, RuntimeBodyOverlay& context)
{
    if (kind != 2)
    {
        return;
    }
    const auto body = context.body;
    const u8* sprite;
    u32 color;
    i32 value;
    float rise;
    float lateral;
    float digit_rise;
    float size;
    if (body_record(body).unk_E0 != 0)
    {
        sprite = context.counter_sprite;
        color = 0xFFFFFFFF; // DAT_00314F48.
        value = truncate_float(body_record(body).unk_DC);
        rise = 3.0f; // DAT_0032B260.
        lateral = 3.0f;
        digit_rise = 3.0f;
        size = 3.0f;
    }
    else
    {
        if (body_record(body).unk_A4 == 0 || body_record(body).unk_A8 == 0)
        {
            return;
        }
        rise = 5.0f; // DAT_0032B184.
        if (runtime_player_uses_alternate_overlay(context.owner_bytes, context.attributes) == 0)
        {
            sprite = context.health_sprite;
        }
        else if (body_record(body).unk_A9 != 0)
        {
            sprite = context.active_sprite;
        }
        else
        {
            sprite = context.alternate_sprite;
        }
        color = (runtime_body_health_color(body) & 0x00FFFFFFu) | (u32(body_record(body).unk_A5) << 24);
        lateral = 0.0f;
        digit_rise = 2.0f; // DAT_0032B110.
        value = i32(body_record(body).energy);
        size = body_record(body).unk_BC;
    }
    float sprite_size[2]{size, size};
    Vec3 position;
    float height;
    float width;
    runtime_actor_overlay_anchor(context.owner, position, height, width);
    float y = position.y + height + rise;
    position.y = y;
    u32 state = 0;
    if (context.attached != nullptr)
    {
        state = context.attached->bytes[0x93D];
    }
    if (state == 1)
    {
        if (context.attached != nullptr
            && runtime_actor_case_class(context.attached->bytes, read_u16(context.owner_bytes, 6)) == 3)
        {
            y = y + 6.0f; // DAT_0032B218.
            position.y = y;
        }
    }
    else if (state == 3)
    {
        y = y + 6.0f;
        position.y = y;
    }
    const u32 frames = sprite[0x10];
    u32 frame = 0;
    if (float(i32(frames)) != 0.0f)
    {
        const u32 ticks = u32(truncate_runtime_float(static_cast<long double>(float(i32(frames))) * read_f32(context.owner_bytes, 0x924)));
        frame = u8(ticks % sprite[0x11]);
    }
    queue_billboard(context.queue, sprite, color, frame, position, sprite_size, 0.0f);
    position.y = digit_rise - rise + y;
    if (lateral != 0.0f)
    {
        const float side = context.view.forward.y * 0.0f;
        float direction[3]{side - context.view.forward.z,
            context.view.forward.z * 0.0f - context.view.forward.x * 0.0f, context.view.forward.x - side};
        normalize_vector_in_place(direction);
        position.x = direction[0] * lateral + position.x;
        position.y = direction[1] * lateral + position.y;
        position.z = direction[2] * lateral + position.z;
    }
    queue_runtime_overlay_digits(value, position, color, context.digits, context.view, context.queue);
}

// Xbox 0x00028050
u32 draw_runtime_player(std::span<const u8> actor, i32 kind, RuntimePlayerDraw& context)
{
    if (get(actor, player_field::live) != 0 &&
        (i16(get(actor, player_field::mode)) == 2 || get(actor, player_field::case_link) != 0) &&
        (context.runtime_flags & 4) != 0)
    {
        return 0;
    }
    const u32 result = draw_runtime_actor(context.state, kind, context.attachments, context.scene);
    queue_runtime_body_overlay(kind, context.overlay);
    if (context.body_effect != nullptr)
    {
        context.draw_body_effect(context.body_effect, kind);
    }
    return result;
}
}
