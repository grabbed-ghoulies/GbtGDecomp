#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "../../engine/cutscenes/quaternion_math.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace gbtg
{
// Xbox 0x0002DFC0.
bool dispatch_animation_event(std::span<const u8> event, u32 expected_size, u32 type_flags,
                              const AnimationEventClock& clock,
                              const std::function<void(int phase)>& callback)
{
    if (read_u16(event, 0) != expected_size)
    {
        return false;
    }
    const float begin = read_f32(event, 4);
    const float end = read_f32(event, 8);
    bool began = false;
    if (clock.position != clock.previous)
    {
        float high;
        float low;
        if (clock.speed < 0.f)
        {
            high = clock.previous;
            low = clock.position;
        }
        else
        {
            high = clock.position;
            low = clock.previous;
        }
        if (high <= low)
        {
            if (low <= begin || begin < high)
            {
                began = true;
                if (callback)
                {
                    callback(0);
                }
            }
        }
        else if (low <= begin && begin < high)
        {
            began = true;
            if (callback)
            {
                callback(0);
            }
        }
    }
    if (((event[14] | type_flags) & 4) != 0 && begin <= clock.position && clock.position < end && callback)
    {
        callback(2);
    }
    bool ended = false;
    if (end != 0.f && clock.position != clock.previous)
    {
        float high;
        float low;
        if (clock.speed < 0.f)
        {
            high = clock.previous;
            low = clock.position;
        }
        else
        {
            low = clock.previous;
            high = clock.position;
        }
        if (high <= low)
        {
            if (low <= end || end < high)
            {
                ended = true;
                if (callback)
                {
                    callback(1);
                }
            }
        }
        else if (low <= end && end < high)
        {
            ended = true;
            if (callback)
            {
                callback(1);
            }
        }
    }
    return began || ended;
}

#include "anim_events.inl"

namespace
{

u8 u8b(int v)
{
    return u8(v & 0xFF);
}

// 0x2EC40 / 0x2ED60. Cursor advance depends only on the bit count and the shift.
int extract_bits(std::span<const u8> buf, int nbits, int cursor, int& shift, int& out)
{
    out = 0;
    if (nbits == 0)
    {
        return cursor;
    }
    int acc = 0;
    int bl = nbits & 0xFF;
    while (true)
    {
        const int edx = shift & 0xFF;
        int al = u8b(1 << (bl & 31));
        al = u8b(u8b(al - 1) << (edx & 31));
        const int mask = al;
        unsigned top = (unsigned(mask) >> (edx & 31)) << 24;
        int found = -1;
        for (int index = 0; index < 32; ++index)
        {
            if ((top >> (31 - index)) & 1u)
            {
                found = index;
                break;
            }
        }
        const int bit_index = found < 0 ? 0 : found;
        const int take = u8b(8 - (bit_index & 0xFF));
        bl = u8b(bl - take);
        if (cursor >= 0 && size_t(cursor) < buf.size())
        {
            unsigned bits = unsigned(buf[size_t(cursor)]) & unsigned(mask);
            bits = (bits >> (edx & 31));
            bits = bits << (acc & 31);
            out |= int(bits);
        }
        const unsigned span = unsigned(take + edx - 8);
        const int step = (span == 0) ? 1 : 0;
        cursor += step;
        acc = u8b(acc + take);
        shift = u8b(take + shift) & 7;
        if (bl == 0)
        {
            break;
        }
    }
    return cursor;
}

struct Streams
{
    std::span<const u8> mina;
    int word = 0;
    int nibble = 0;
    int frames[4]{};
    int cursor = 0;
    int shift = 0;
    int mask = 0x0F;
    int nibble_hi = 0;

    i16 read_i16()
    {
        const i16 v = i16(read_u16(mina, size_t(word)));
        word += 2;
        return v;
    }

    int read_nibble()
    {
        const int b = mina[size_t(nibble)];
        const int val = (b & mask) >> (nibble_hi ? 4 : 0);
        nibble_hi ^= 1;
        if (nibble_hi == 0)
        {
            ++nibble;
        }
        mask = nibble_hi ? 0xF0 : 0x0F;
        return val + 1;
    }

    void delta(int& d0, int& d1)
    {
        const int nbits = read_nibble();
        const auto a = mina.subspan(size_t(frames[1]));
        const auto b = mina.subspan(size_t(frames[2]));
        const int cursor0 = cursor;
        int shift_a = shift;
        int shift_b = shift;
        cursor = extract_bits(a, nbits, cursor0, shift_a, d0);
        extract_bits(b, nbits, cursor0, shift_b, d1);
        shift = shift_a;
    }

    // Xbox 0x0002ED60; all neighbours use the same cursor and bit shift.
    void delta_four(int (&values)[4])
    {
        const int nbits = read_nibble();
        const int initial_cursor = cursor;
        const int initial_shift = shift;
        for (int frame = 0; frame < 4; ++frame)
        {
            int frame_shift = initial_shift;
            const int frame_cursor = extract_bits(mina.subspan(size_t(frames[frame])), nbits,
                                                   initial_cursor, frame_shift, values[frame]);
            cursor = frame_cursor;
            shift = frame_shift;
        }
    }
};

#include "anim_math.inl"

} // namespace

// Xbox 0x001534C0
long double retail_reciprocal_sqrt(float value)
{
    return animation_rsqrt(value);
}

// Xbox 0x00152B30
long double retail_square_root(float value)
{
    return animation_sqrt(value);
}

// Xbox 0x00154030
Quat slerp(Quat a, Quat b, float t)
{
    float dot = b.y * a.y + a.x * b.x + b.w * a.w + b.z * a.z;
    float first = 1.f - t;
    if (dot < 0.f)
    {
        dot = -dot;
        t = -t;
    }
    if (dot < animation_float(0x3f7fff58u))
    {
        const long double angle = animation_acos(dot);
        const float reciprocal = float(animation_rsqrt(1.f - dot * dot));
        first = float(animation_sin(float(angle) * first) * reciprocal);
        t = float(animation_sin(float(angle) * t) * reciprocal);
    }
    return Quat{a.x * first + b.x * t, b.y * t + a.y * first,
                b.z * t + a.z * first, b.w * t + a.w * first};
}

namespace
{
// Xbox 0x0002E3A0.
void finish_mina_quaternion(Quat& current, Quat& next, float fraction, u8 flags)
{
    if ((flags & 0xE0) == 0)
    {
        current.w = 1.f;
        return;
    }
    const float square = 1.f - (current.x * current.x + current.y * current.y + current.z * current.z);
    if ((flags & 0x1C) == 0x1C)
    {
        float w = 0.f;
        if (0.f < square)
        {
            w = float(animation_sqrt(square));
        }
        current.w = w;
        return;
    }
    if (square <= 0.f)
    {
        current.w = 0.f;
    }
    else
    {
        current.w = float(animation_sqrt(square));
    }
    const float next_square = 1.f - (next.x * next.x + next.y * next.y + next.z * next.z);
    float next_w = 0.f;
    if (0.f < next_square)
    {
        next_w = float(animation_sqrt(next_square));
    }
    next.w = next_w;
    current = slerp(current, next, fraction);
}

struct Mat
{
    float m[16]{};
};

Mat ident()
{
    Mat o;
    o.m[0] = o.m[5] = o.m[10] = o.m[15] = 1.f;
    return o;
}

// Row-major 3x4 of the same affine transform the palette stores column-major.
void to_affine(float m[12], const Mat& src)
{
    m[0] = src.m[0];
    m[1] = src.m[4];
    m[2] = src.m[8];
    m[3] = src.m[12];
    m[4] = src.m[1];
    m[5] = src.m[5];
    m[6] = src.m[9];
    m[7] = src.m[13];
    m[8] = src.m[2];
    m[9] = src.m[6];
    m[10] = src.m[10];
    m[11] = src.m[14];
}

Mat from_affine(const float m[12])
{
    Mat o = ident();
    o.m[0] = m[0];
    o.m[1] = m[4];
    o.m[2] = m[8];
    o.m[4] = m[1];
    o.m[5] = m[5];
    o.m[6] = m[9];
    o.m[8] = m[2];
    o.m[9] = m[6];
    o.m[10] = m[10];
    o.m[12] = m[3];
    o.m[13] = m[7];
    o.m[14] = m[11];
    return o;
}

} // namespace

#include "anim_scalar.inl"

// Xbox 0x001041B0
// out = a * b. Apply b first. A destination that aliases either input is written
// through a temporary, then copied back as twelve floats.
// Xbox 0x001041B0
void multiply_affine_matrices(float out[12], const float a[12], const float b[12])
{
    float tmp[12];
    float* dest = out;
    if (out == a || out == b)
    {
        dest = tmp;
    }
    dest[0] = (a[0] * b[0] + a[2] * b[8]) + b[4] * a[1];
    dest[1] = (a[0] * b[1] + a[2] * b[9]) + a[1] * b[5];
    dest[2] = (a[0] * b[2] + a[2] * b[10]) + a[1] * b[6];
    dest[3] = ((a[0] * b[3] + a[2] * b[11]) + a[1] * b[7]) + a[3];
    dest[4] = (a[4] * b[0] + b[8] * a[6]) + b[4] * a[5];
    dest[5] = (a[4] * b[1] + a[6] * b[9]) + a[5] * b[5];
    dest[6] = (a[4] * b[2] + a[6] * b[10]) + a[5] * b[6];
    dest[7] = ((a[4] * b[3] + b[11] * a[6]) + a[5] * b[7]) + a[7];
    dest[8] = (b[8] * a[10] + b[4] * a[9]) + a[8] * b[0];
    dest[9] = (a[10] * b[9] + a[9] * b[5]) + a[8] * b[1];
    dest[10] = (a[10] * b[10] + a[9] * b[6]) + a[8] * b[2];
    dest[11] = ((a[10] * b[11] + a[9] * b[7]) + a[8] * b[3]) + a[11];
    if (dest != out)
    {
        std::memcpy(out, tmp, sizeof(tmp));
    }
}

namespace
{

Mat mul(const Mat& a, const Mat& b)
{
    float ar[12];
    float br[12];
    float out[12];
    to_affine(ar, a);
    to_affine(br, b);
    multiply_affine_matrices(out, ar, br);
    return from_affine(out);
}

Mat from_pose(Quat q, Vec3 t, Vec3 s)
{
    const float x = q.x, y = q.y, z = q.z, w = q.w;
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;
    Mat o = ident();
    o.m[0] = s.x * (1.f - 2.f * (yy + zz));
    o.m[1] = s.x * (2.f * (xy + wz));
    o.m[2] = s.x * (2.f * (xz - wy));
    o.m[4] = s.y * (2.f * (xy - wz));
    o.m[5] = s.y * (1.f - 2.f * (xx + zz));
    o.m[6] = s.y * (2.f * (yz + wx));
    o.m[8] = s.z * (2.f * (xz + wy));
    o.m[9] = s.z * (2.f * (yz - wx));
    o.m[10] = s.z * (1.f - 2.f * (xx + yy));
    o.m[12] = t.x;
    o.m[13] = t.y;
    o.m[14] = t.z;
    return o;
}

// Xbox 0x0002E130, the player's linear interpolation callback.
long double mina_linear_value(float fraction, int current, int next)
{
    return static_cast<long double>(next - current) * fraction + current;
}

// Xbox 0x0002E270, linear callback (+0x2B == 0).
void decode_vec(Streams& st, int flag, float scale, float def, float frac, Vec3& out)
{
    float components[3];
    int mask = 0x80;
    for (int c = 0; c < 3; ++c)
    {
        if (flag & mask)
        {
            const int raw = st.read_i16();
            if (flag & (mask >> 3))
            {
                components[c] = float(raw) * scale;
            }
            else
            {
                int deltas[4]{};
                st.delta_four(deltas);
                components[c] = float(mina_linear_value(frac, raw + deltas[1], raw + deltas[2]) * scale);
            }
        }
        else
        {
            components[c] = def;
        }
        mask >>= 1;
    }
    out.x = components[0];
    out.y = components[1];
    out.z = components[2];
}

} // namespace

namespace
{

std::vector<JointPose> sample_mina_channel(std::span<const u8> mina, float time_norm, bool loop, bool root)
{
    std::vector<JointPose> poses;
    const u32 bits = read_u32(mina, 0x1C);
    const int trans_exp = int((bits >> 18) & 31);
    const int scale_exp = int((bits >> 8) & 31);
    const int rot_exp = int((bits >> 23) & 31);
    const float trans_scale = 1.f / float(std::bit_cast<i32>(1u << trans_exp));
    const float scale_scale = 1.f / float(std::bit_cast<i32>(1u << scale_exp));
    const float rot_scale = 1.f / float(std::bit_cast<i32>((1u << ((rot_exp - 1) & 31)) - 1));
    const int frame_count = int(read_u16(mina, root ? 0x1A : 0x18));
    const int bones = root ? 1 : int(read_u16(mina, 0x14));
    // 0x2EB50. Looping (+0x2A == 0) scales by the frame count and wraps.
    // Cutscene playback (+0x2A == 1) scales by count-1 and clamps the neighbours.
    const int span = loop ? frame_count : frame_count - 1;
    const float time = float(span) * time_norm;
    int frame = u16(int(time));
    int prev = 0;
    int next = 0;
    int next2 = 0;
    if (loop)
    {
        frame %= frame_count;
        prev = (frame + frame_count - 1) % frame_count;
        next = (frame + 1) % frame_count;
        next2 = (frame + 2) % frame_count;
    }
    else
    {
        prev = u16(frame - (frame != 0));
        next = u16(frame + (frame != span));
        next2 = u16(next + (next != span));
    }
    const float frac = time - float(frame);
    const int directory = root ? int(read_u32(mina, 0xC)) : 0x40;
    const int base_off = read_u16(mina, size_t(directory));
    const int off2 = read_u16(mina, size_t(directory + 2));
    const int off4 = read_u16(mina, size_t(directory + 4));
    const int stride = read_u16(mina, size_t(directory + 6));
    const int base = directory + base_off;
    const int ids[4] = {prev, frame, next, next2};
    Streams st;
    st.mina = mina;
    st.word = base;
    st.nibble = base + off2;
    for (int i = 0; i < 4; ++i)
    {
        st.frames[i] = base + off2 + off4 + ids[i] * stride;
    }
    poses.resize(size_t(bones));
    int flag_p = directory + 0xC;
    for (int i = 0; i < bones; ++i)
    {
        const int flag = mina[size_t(flag_p++)];
        float cur[3]{};
        float nxt[3]{};
        int mask = 0x80;
        for (int c = 0; c < 3; ++c)
        {
            if (flag & mask)
            {
                const int raw = st.read_i16();
                if (flag & (mask >> 3))
                {
                    cur[c] = nxt[c] = float(raw) * rot_scale;
                }
                else
                {
                    int d0 = 0, d1 = 0;
                    st.delta(d0, d1);
                    cur[c] = float(raw + d0) * rot_scale;
                    nxt[c] = float(raw + d1) * rot_scale;
                }
            }
            mask >>= 1;
        }
        Quat a{cur[0], cur[1], cur[2], 0.f};
        Quat b{nxt[0], nxt[1], nxt[2], 0.f};
        finish_mina_quaternion(a, b, frac, u8(flag));
        poses[size_t(i)].rotation = a;
        if (flag & 2)
        {
            decode_vec(st, mina[size_t(flag_p)], trans_scale, 0.f, frac, poses[size_t(i)].translation);
            ++flag_p;
        }
        if (flag & 1)
        {
            decode_vec(st, mina[size_t(flag_p)], scale_scale, 1.f, frac, poses[size_t(i)].scale);
            ++flag_p;
        }
    }
    return poses;
}

} // namespace

// Xbox 0x00152BE0. CVTTSS2SI preserves the retail signed integer conversion,
// including its indefinite result for an out-of-range quotient.
long double animation_remainder(float value, float divisor)
{
    float absolute_divisor = divisor;
    if (divisor < 0.f)
    {
        absolute_divisor = 0.f - divisor;
    }
    float absolute_value = value;
    if (value < 0.f)
    {
        absolute_value = 0.f - value;
    }
    if (absolute_divisor <= absolute_value)
    {
        const float quotient = value / divisor;
        const int integral = _mm_cvtt_ss2si(_mm_set_ss(quotient));
        return static_cast<long double>(value) - static_cast<long double>(integral) * divisor;
    }
    return value;
}

std::vector<JointPose> sample_mina(std::span<const u8> mina, float time_norm, bool loop)
{
    return sample_mina_channel(mina, time_norm, loop, false);
}

JointPose sample_mina_root(std::span<const u8> mina, float time_norm)
{
    return sample_mina_channel(mina, time_norm, false, true).at(0);
}

// Xbox 0x0002E750 / 0x0002E650.
Vec3 mina_root_cycle_translation(std::span<const u8> mina)
{
    const size_t directory = read_u32(mina, 0xC);
    Vec3 displacement{};
    if ((mina[directory + 0xC] & 2) != 0)
    {
        Streams stream;
        stream.mina = mina;
        stream.word = int(directory + read_u16(mina, directory));
        stream.nibble = stream.word + read_u16(mina, directory + 2);
        const int frame_base = stream.nibble + read_u16(mina, directory + 4);
        stream.frames[1] = frame_base;
        stream.frames[2] = frame_base + (read_u16(mina, 0x1A) - 1) * read_u16(mina, directory + 6);
        const int exponent = (read_u32(mina, 0x1C) >> 18) & 31;
        const float scale = 1.f / float(i32(u32(1) << exponent));
        const u8 flag = mina[directory + 0xD];
        float components[3]{};
        float* component = components;
        for (int mask = 0x80; mask != 0x10; mask >>= 1)
        {
            if ((flag & mask) == 0)
            {
                *component = 0.f;
            }
            else if ((flag & (mask >> 3)) == 0)
            {
                stream.word += 2;
                int first = 0;
                int last = 0;
                stream.delta(first, last);
                *component = float(last - first) * scale;
            }
            else
            {
                *component = 0.f;
                stream.word += 2;
            }
            ++component;
        }
        displacement = Vec3{components[0], components[1], components[2]};
    }
    return displacement;
}

#include "anim_playback.inl"
#include "anim_resources.inl"
#include "anim_resource_providers.inl"
#include "anim_mix.inl"
#include "anim_controller.inl"
#include "anim_skeleton.inl"
#include "anim_table.inl"

// Xbox 0x0002FAC0 builds each slot as parent * local (0x001041B0). Xbox 0x0002F720
// then subtracts the joint's bind point through that rotation. Rigid vertices read
// one slot; the draw remaps ndRigidSkinIdx onto these joints.
void skin_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, float* out, int max_joints)
{
    const int n = int(std::min(joints.size(), pose.size()));
    const int count = std::min(n, max_joints);
    if (!out || count <= 0)
    {
        return;
    }
    std::vector<Mat> palette(size_t(count), ident());
    for (int i = 0; i < count; ++i)
    {
        const JointPose& jp = pose[size_t(i)];
        const Vec3 t{joints[size_t(i)].local.x + jp.translation.x, joints[size_t(i)].local.y + jp.translation.y,
                     joints[size_t(i)].local.z + jp.translation.z};
        Mat local = ident();
        if (jp.rotation.w == 1.f)
        {
            local.m[0] = jp.scale.x;
            local.m[5] = jp.scale.y;
            local.m[10] = jp.scale.z;
            local.m[12] = t.x;
            local.m[13] = t.y;
            local.m[14] = t.z;
        }
        else
        {
            local = from_pose(jp.rotation, t, jp.scale);
        }
        const int p = joints[size_t(i)].parent;
        palette[size_t(i)] = (p >= 0 && p < i) ? mul(palette[size_t(p)], local) : local;
    }
    for (int i = 0; i < count; ++i)
    {
        Mat skin = palette[size_t(i)];
        const float bx = joints[size_t(i)].world.x;
        const float by = joints[size_t(i)].world.y;
        const float bz = joints[size_t(i)].world.z;
        skin.m[12] -= skin.m[0] * bx + skin.m[4] * by + skin.m[8] * bz;
        skin.m[13] -= skin.m[1] * bx + skin.m[5] * by + skin.m[9] * bz;
        skin.m[14] -= skin.m[2] * bx + skin.m[6] * by + skin.m[10] * bz;
        std::memcpy(out + size_t(i) * 16, skin.m, sizeof(skin.m));
    }
}

namespace
{

bool invert_affine(const float* m, float* out)
{
    const float a = m[0], b = m[4], c = m[8];
    const float d = m[1], e = m[5], f = m[9];
    const float g = m[2], h = m[6], i = m[10];
    const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    if (std::fabs(det) < 1e-8f)
    {
        return false;
    }
    const float inv = 1.f / det;
    out[0] = (e * i - f * h) * inv;
    out[1] = (f * g - d * i) * inv;
    out[2] = (d * h - e * g) * inv;
    out[4] = (c * h - b * i) * inv;
    out[5] = (a * i - c * g) * inv;
    out[6] = (b * g - a * h) * inv;
    out[8] = (b * f - c * e) * inv;
    out[9] = (c * d - a * f) * inv;
    out[10] = (a * e - b * d) * inv;
    out[3] = out[7] = out[11] = 0.f;
    out[15] = 1.f;
    const float tx = m[12], ty = m[13], tz = m[14];
    out[12] = -(out[0] * tx + out[4] * ty + out[8] * tz);
    out[13] = -(out[1] * tx + out[5] * ty + out[9] * tz);
    out[14] = -(out[2] * tx + out[6] * ty + out[10] * tz);
    return true;
}

void bake_pivot(float* m, Vec3 delta)
{
    const float rx = m[0] * delta.x + m[4] * delta.y + m[8] * delta.z;
    const float ry = m[1] * delta.x + m[5] * delta.y + m[9] * delta.z;
    const float rz = m[2] * delta.x + m[6] * delta.y + m[10] * delta.z;
    m[12] += delta.x - rx;
    m[13] += delta.y - ry;
    m[14] += delta.z - rz;
}

} // namespace

void skin_draw_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& rest,
                        const std::vector<JointPose>& pose, Vec3 delta, float* out, int max_joints)
{
    const int n = int(std::min(joints.size(), std::min(rest.size(), pose.size())));
    const int count = std::min(n, max_joints);
    if (!out || count <= 0)
    {
        return;
    }
    std::vector<float> rest_m(size_t(count) * 16);
    std::vector<float> pose_m(size_t(count) * 16);
    skin_matrices(joints, rest, rest_m.data(), count);
    skin_matrices(joints, pose, pose_m.data(), count);
    for (int i = 0; i < count; ++i)
    {
        float* dest = out + size_t(i) * 16;
        float inv[16];
        if (!invert_affine(rest_m.data() + size_t(i) * 16, inv))
        {
            std::memcpy(dest, ident().m, sizeof(float) * 16);
            continue;
        }
        Mat anim;
        Mat base;
        std::memcpy(anim.m, pose_m.data() + size_t(i) * 16, sizeof(anim.m));
        std::memcpy(base.m, inv, sizeof(base.m));
        const Mat skin = mul(anim, base);
        std::memcpy(dest, skin.m, sizeof(skin.m));
        bake_pivot(dest, delta);
    }
}

} // namespace gbtg
