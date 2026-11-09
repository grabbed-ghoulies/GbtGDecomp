// The MINA keyframe sampler (0x0002E8A0) and its stream readers. Included by anim.cpp after slerp (0x00154030) and
// 0x0002E3A0's quaternion finish.

namespace
{
using MinaInterpolation = long double (*)(float fraction, int previous, int current, int next, int after);

// The decoder state 0x0002E810 (and 0x0002E750) build on the stack, 0x2C bytes. Offsets index the MINA.
struct MinaStream
{
    std::span<const u8> mina;
    size_t word = 0; // +0x00, the i16 base values
    size_t nibble = 0; // +0x04, the 4-bit delta widths
    std::array<size_t, 4> frames{}; // +0x08..+0x14: the previous, current, next and following frames' delta bits
    i32 cursor = 0; // +0x18, the byte cursor shared by the frames' bit streams
    float fraction = 0.f; // +0x1C
    MinaInterpolation interpolate = nullptr; // +0x20
    // +0x24 is always 0x0002E3A0 (finish_mina_quaternion).
    u8 nibble_mask = 0x0F; // +0x28
    u8 nibble_high = 0; // +0x29
    u8 shift = 0; // +0x2A, the bit shift within the cursor's byte

    i16 next_word()
    {
        const i16 value = i16(read_u16(mina, word));
        word += 2;
        return value;
    }

    // The width of the next delta: the nibble under the mask, plus one; the stream moves to the other nibble.
    u8 next_width()
    {
        const u8 high = nibble_high;
        const u8 bits = mina[nibble];
        nibble += high;
        nibble_high = high == 0 ? 1 : 0;
        const u8 width = u8(((bits & nibble_mask) >> ((high & 7) << 2)) + 1);
        nibble_mask = u8(0x0F << (nibble_high << 2));
        return width;
    }
};

// One step of 0x0002EC40 / 0x0002ED60's loop: the byte mask for `width` bits at `shift`, and how many bits the
// cursor's byte gives (the first set bit of (mask >> shift) << 24, counted from the top).
u8 mina_bits_taken(u8 width, u8 shift, u8& mask)
{
    mask = u8(u8(u8(1u << (width & 31)) - 1) << (shift & 31));
    const u32 top = (u32(mask) >> (shift & 31)) << 24;
    u32 index = 0;
    for (; index < 0x20; ++index)
    {
        if (((top >> (0x1F - index)) & 1) != 0)
        {
            break;
        }
    }
    if (index == 0x20)
    {
        index = 0;
    }
    return u8(8 - index);
}

// Xbox 0x0002EC40
void read_mina_bits_2(std::span<const u8> mina, size_t first, size_t second, i32& cursor, u8& shift, u32& a,
    u32& b, u8 width)
{
    u8 bit = shift;
    i32 at = cursor;
    u8 placed = 0;
    while (width != 0)
    {
        u8 mask = 0;
        const u8 taken = mina_bits_taken(width, bit, mask);
        width = u8(width - taken);
        a |= u32(u8((mina[first + size_t(at)] & mask) >> (bit & 31))) << (placed & 31);
        b |= u32(u8((mina[second + size_t(at)] & mask) >> (bit & 31))) << (placed & 31);
        at += u8(taken + bit) == 8 ? 1 : 0;
        bit = u8((taken + bit) & 7);
        placed = u8(placed + taken);
    }
    shift = bit;
    cursor = at;
}

// Xbox 0x0002ED60
void read_mina_bits_4(std::span<const u8> mina, const std::array<size_t, 4>& frames, i32& cursor, u8& shift,
    std::array<u32, 4>& values, u8 width)
{
    u8 bit = shift;
    i32 at = cursor;
    u8 placed = 0;
    while (width != 0)
    {
        u8 mask = 0;
        const u8 taken = mina_bits_taken(width, bit, mask);
        width = u8(width - taken);
        for (size_t frame = 0; frame < 4; ++frame)
        {
            values[frame] |= u32(u8((mina[frames[frame] + size_t(at)] & mask) >> (bit & 31))) << (placed & 31);
        }
        at += u8(taken + bit) == 8 ? 1 : 0;
        bit = u8((taken + bit) & 7);
        placed = u8(placed + taken);
    }
    shift = bit;
    cursor = at;
}

// Xbox 0x0002E130
long double interpolate_mina_linear(float fraction, int, int current, int next, int)
{
    return static_cast<long double>(next - current) * fraction + current;
}

// Xbox 0x0002E150
long double interpolate_mina_cubic(float fraction, int previous, int current, int next, int after)
{
    const long double c = current;
    const long double n = next;
    const float half_previous = float(previous) * 0.5f; // DAT_0032B0F4
    const long double t = fraction;
    return ((n * 0.5L - half_previous)
        + (((n + n + (static_cast<long double>(float(previous)) - c * 2.5L)) - static_cast<long double>(after) * 0.5L)
            + (((c * 1.5L - n * 1.5L) + static_cast<long double>(after) * 0.5L) - half_previous) * t)
            * t)
        * t + c;
}

// Xbox 0x0002E1D0
long double interpolate_mina_spline(float fraction, int previous, int current, int next, int after)
{
    const long double c = current;
    const float half_next = float(next) * 0.5f; // DAT_0032B0F4
    const float sixth_previous = float(previous) * 0.16666667f; // DAT_0032B208
    const float half_previous = float(previous) * 0.5f;
    const long double t = fraction;
    return c * static_cast<long double>(0.66666669f) + static_cast<long double>(next) * 0.16666667f
        + sixth_previous
        + (((static_cast<long double>(half_next) - half_previous)
               + ((static_cast<long double>(half_previous) - c) + half_next
                     + ((static_cast<long double>(after) * 0.16666667f + (0.5L * c - half_next)) - sixth_previous)
                         * t)
                   * t)
            * t);
}

// Xbox 0x0002E270
void decode_mina_vector(MinaStream& stream, float* out, float scale, float fallback, u8 flags)
{
    for (u16 mask = 0x80; mask != 0x10; mask = u16(mask >> 1), ++out)
    {
        if ((flags & mask) == 0)
        {
            *out = fallback;
        }
        else if (((mask >> 3) & flags) != 0)
        {
            *out = float(stream.next_word()) * scale;
        }
        else
        {
            const i32 base = stream.next_word();
            const u8 width = stream.next_width();
            std::array<u32, 4> values{};
            read_mina_bits_4(stream.mina, stream.frames, stream.cursor, stream.shift, values, width); // 0x0002ED60
            *out = float(stream.interpolate(stream.fraction, i32(values[0]) + base, i32(values[1]) + base,
                i32(values[2]) + base, i32(values[3]) + base) * scale);
        }
    }
}

// Xbox 0x0002E520, XMM1 the scale and XMM2 the value of an absent component
void decode_mina_rotation(MinaStream& stream, Quat& out, float scale, float absent, u8 flags)
{
    float current[4]{};
    float next[4]{};
    size_t component = 0;
    for (u32 mask = 0x80; mask != 0x10; mask >>= 1, ++component)
    {
        if ((mask & flags) == 0)
        {
            next[component] = absent;
            current[component] = absent;
        }
        else if ((flags & (mask >> 3)) != 0)
        {
            const i16 value = stream.next_word();
            next[component] = float(i32(value)) * scale;
            current[component] = float(i32(value)) * scale;
        }
        else
        {
            const i32 base = stream.next_word();
            const u8 width = stream.next_width();
            u32 first = 0;
            u32 second = 0;
            read_mina_bits_2(stream.mina, stream.frames[1], stream.frames[2], stream.cursor, stream.shift, first,
                second, width); // 0x0002EC40
            current[component] = float(i32(first) + base) * scale;
            next[component] = float(i32(second) + base) * scale;
        }
    }
    out = Quat{current[0], current[1], current[2], current[3]};
    Quat following{next[0], next[1], next[2], next[3]};
    finish_mina_quaternion(out, following, stream.fraction, flags); // +0x24, 0x0002E3A0
}

// Xbox 0x0002E810, EAX the stream and EDX the directory
void open_mina_stream(MinaStream& stream, std::span<const u8> mina, size_t directory, u16 previous, u16 current,
    u16 next, u16 after)
{
    const u16 stride = read_u16(mina, directory + 6);
    const u16 widths = read_u16(mina, directory + 2);
    const size_t words = read_u16(mina, directory) + directory;
    const u16 deltas = read_u16(mina, directory + 4);
    stream.mina = mina;
    stream.nibble_high = 0;
    stream.shift = 0;
    stream.cursor = 0;
    stream.nibble = widths + words;
    stream.frames[0] = size_t(previous) * stride + deltas + widths + words;
    stream.frames[1] = size_t(current) * stride + deltas + widths + words;
    stream.frames[2] = size_t(next) * stride + deltas + widths + words;
    stream.frames[3] = size_t(after) * stride + deltas + widths + words;
    stream.nibble_mask = 0x0F;
    stream.word = words;
}

// Xbox 0x0002EB50: EAX the loop override (null: player +0x2A == 0 loops), EDX the time (null: the position times
// the span), EDI the frame, EBX the frame after next; returns the fraction past the frame (XMM0).
float select_mina_frames(const MinaPlayback& player, i32 count, const i32* loop_override, const float* time,
    u16& previous, u16& frame, u16& next, u16& after)
{
    const i32 loop = loop_override != nullptr ? *loop_override : (player.mode == 0 ? 1 : 0);
    i32 span = count;
    if (loop == 0)
    {
        --span;
    }
    const float at = time != nullptr ? *time : float(span) * player.position;
    if (loop != 0)
    {
        frame = u16(i32(u16(static_cast<std::int64_t>(at))) % span); // 0x001673D8
        next = u16((i32(frame) + 1) % span);
        after = u16((i32(frame) + 2) % span);
        previous = u16((i32(frame) + span - 1) % span);
        return at - float(frame);
    }
    const u16 truncated = u16(static_cast<std::int64_t>(at)); // 0x001673D8
    frame = truncated;
    previous = u16(truncated - (truncated != 0 ? 1 : 0));
    next = u16(frame + (i32(frame) != span ? 1 : 0));
    after = u16(next + (i32(next) != span ? 1 : 0));
    return at - float(frame);
}
} // namespace

// Xbox 0x0002E8A0
void sample_mina_pose(JointPose* out, const MinaPlayback& player, const float* time, const i32* loop_override,
    bool root, float weight)
{
    const std::span<const u8> mina = player.mina;
    size_t directory = 0;
    u16 count = 0;
    if (root)
    {
        // Player +8: the root's directory, null when the MINA has no root frames (0x00031AC0).
        if (read_u16(mina, 0x1A) == 0)
        {
            throw Error("MINA root sample without root frames");
        }
        directory = read_u32(mina, 0xC);
        count = read_u16(mina, 0x1A);
    }
    else
    {
        directory = 0x40; // Player +4.
        count = read_u16(mina, 0x18);
    }
    u16 previous = 0;
    u16 frame = 0;
    u16 next = 0;
    u16 after = 0;
    MinaStream stream;
    stream.fraction = select_mina_frames(player, count, loop_override, time, previous, frame, next, after);
    open_mina_stream(stream, mina, directory, previous, frame, next, after);
    size_t flags = directory + 0xC;
    switch (player.sampling_mode)
    {
    case 0:
        stream.interpolate = interpolate_mina_linear;
        break;
    case 1:
        stream.interpolate = interpolate_mina_cubic;
        break;
    case 2:
        stream.interpolate = interpolate_mina_spline;
        break;
    }
    const u32 exponents = read_u32(mina, 0x1C);
    const float translation_scale = 1.f / float(i32(1u << ((exponents >> 0x12) & 0x1F)));
    const float scale_scale = 1.f / float(i32(1u << ((exponents >> 8) & 0x1F)));
    const float rotation_scale = 1.f / (float(i32(1u << ((((exponents >> 0x17) & 0x1F) - 1) & 0x1F))) - 1.f);
    const u16 bones = root ? u16(1) : read_u16(mina, 0x14);
    for (u16 bone = 0; bone < bones; ++bone)
    {
        JointPose& pose = out[bone];
        const u8 bits = mina[flags++];
        decode_mina_rotation(stream, pose.rotation, rotation_scale, 0.f, bits); // 0x0002E520
        if ((bits & 2) != 0)
        {
            decode_mina_vector(stream, &pose.translation.x, translation_scale, 0.f, mina[flags]); // 0x0002E270
            ++flags;
        }
        else
        {
            pose.translation = Vec3{};
        }
        if ((bits & 1) != 0)
        {
            decode_mina_vector(stream, &pose.scale.x, scale_scale, 1.f, mina[flags]);
            ++flags;
        }
        else
        {
            pose.scale = Vec3{1.f, 1.f, 1.f};
        }
    }
    if (weight != 1.f && bones != 0)
    {
        for (u16 bone = 0; bone < bones; ++bone)
        {
            JointPose& pose = out[bone];
            pose.rotation = slerp(Quat{0.f, 0.f, 0.f, 1.f}, pose.rotation, weight); // DAT_0042A830
            pose.translation.x *= weight;
            pose.translation.y *= weight;
            pose.translation.z *= weight;
            pose.scale.x *= weight;
            pose.scale.y *= weight;
            pose.scale.z *= weight;
        }
    }
}

namespace
{
// Xbox 0x0002E650, EAX the output, ESI the stream and XMM1 the scale
void decode_mina_cycle(MinaStream& stream, float* out, float scale, u8 flags)
{
    for (u16 mask = 0x80; mask != 0x10; mask = u16(mask >> 1), ++out)
    {
        if ((flags & mask) == 0)
        {
            *out = 0.f;
        }
        else if (((mask >> 3) & flags) == 0)
        {
            stream.word += 2;
            const u8 width = stream.next_width();
            u32 first = 0;
            u32 last = 0;
            read_mina_bits_2(stream.mina, stream.frames[1], stream.frames[2], stream.cursor, stream.shift, first, last,
                width); // 0x0002EC40
            *out = float(i32(last) - i32(first)) * scale;
        }
        else
        {
            *out = 0.f;
            stream.word += 2;
        }
    }
}
} // namespace

// Xbox 0x0002E750
Vec3 mina_root_cycle_translation(const MinaPlayback& player)
{
    const std::span<const u8> mina = player.mina;
    const size_t directory = read_u32(mina, 0xC); // Player +8.
    MinaStream stream;
    stream.mina = mina;
    stream.nibble_high = 0;
    stream.shift = 0;
    stream.cursor = 0;
    const size_t words = read_u16(mina, directory) + directory;
    const u16 widths = read_u16(mina, directory + 2);
    const u16 deltas = read_u16(mina, directory + 4);
    stream.word = words;
    stream.nibble = widths + words;
    stream.frames[1] = size_t(deltas) + widths + words; // The first root frame.
    stream.frames[2] = size_t(read_u16(mina, 0x1A) - 1) * read_u16(mina, directory + 6) + deltas + widths + words;
    stream.nibble_mask = 0x0F;
    Vec3 displacement{};
    if ((mina[directory + 0xC] & 2) != 0)
    {
        const float scale = 1.f / float(i32(1u << ((read_u32(mina, 0x1C) >> 0x12) & 0x1F)));
        decode_mina_cycle(stream, &displacement.x, scale, mina[directory + 0xD]); // 0x0002E650
        return displacement;
    }
    return Vec3{};
}
