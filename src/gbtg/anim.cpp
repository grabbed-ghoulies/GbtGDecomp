#include "gbtg/anim.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace gbtg
{
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
};

Quat slerp(Quat a, Quat b, float t)
{
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.f)
    {
        b.x = -b.x;
        b.y = -b.y;
        b.z = -b.z;
        b.w = -b.w;
    }
    Quat q{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
    const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len > 1e-8f)
    {
        q.x /= len;
        q.y /= len;
        q.z /= len;
        q.w /= len;
    }
    return q;
}

float quat_w(float x, float y, float z)
{
    const float d = 1.f - x * x - y * y - z * z;
    return d > 0.f ? std::sqrt(d) : 0.f;
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

// Xbox 0x001041B0
// out = a * b. Apply b first. A destination that aliases either input is written
// through a temporary, then copied back as twelve floats.
void fn_1041b0(float out[12], const float a[12], const float b[12])
{
    float tmp[12];
    float* dest = out;
    if (out == a || out == b)
    {
        dest = tmp;
    }
    for (int row = 0; row < 3; ++row)
    {
        const float ax = a[row * 4 + 0];
        const float ay = a[row * 4 + 1];
        const float az = a[row * 4 + 2];
        const float aw = a[row * 4 + 3];
        dest[row * 4 + 0] = ax * b[0] + ay * b[4] + az * b[8];
        dest[row * 4 + 1] = ax * b[1] + ay * b[5] + az * b[9];
        dest[row * 4 + 2] = ax * b[2] + ay * b[6] + az * b[10];
        dest[row * 4 + 3] = ax * b[3] + ay * b[7] + az * b[11] + aw;
    }
    if (dest != out)
    {
        std::memcpy(out, tmp, sizeof(tmp));
    }
}

Mat mul(const Mat& a, const Mat& b)
{
    float ar[12];
    float br[12];
    float out[12];
    to_affine(ar, a);
    to_affine(br, b);
    fn_1041b0(out, ar, br);
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

void decode_vec(Streams& st, int flag, float scale, float def, float frac, Vec3& out)
{
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
                cur[c] = nxt[c] = float(raw) * scale;
            }
            else
            {
                int d0 = 0, d1 = 0;
                st.delta(d0, d1);
                cur[c] = float(raw + d0) * scale;
                nxt[c] = float(raw + d1) * scale;
            }
        }
        else
        {
            cur[c] = nxt[c] = def;
        }
        mask >>= 1;
    }
    out.x = cur[0] + (nxt[0] - cur[0]) * frac;
    out.y = cur[1] + (nxt[1] - cur[1]) * frac;
    out.z = cur[2] + (nxt[2] - cur[2]) * frac;
}

} // namespace

std::vector<JointPose> sample_mina(std::span<const u8> mina, float time_norm, bool loop)
{
    std::vector<JointPose> poses;
    if (mina.size() < 0x40 || mina[0] != 'M' || mina[1] != 'I' || mina[2] != 'N' || mina[3] != 'A')
    {
        return poses;
    }
    const u32 bits = read_u32(mina, 0x1C);
    const int trans_exp = int((bits >> 18) & 31);
    const int scale_exp = int((bits >> 8) & 31);
    const int rot_exp = int((bits >> 23) & 31);
    const float trans_scale = trans_exp < 31 ? 1.f / float(1 << trans_exp) : 0.f;
    const float scale_scale = scale_exp < 31 ? 1.f / float(1 << scale_exp) : 0.f;
    const float rot_scale = 1.f / float((1 << (rot_exp - 1)) - 1);
    const int frame_count = int(read_u16(mina, 0x18));
    const int bones = int(read_u16(mina, 0x14));
    if (frame_count <= 0 || bones <= 0 || bones > 512)
    {
        return poses;
    }
    // 0x2EB50. Looping (+0x2A == 0) scales by the frame count and wraps.
    // Cutscene playback (+0x2A == 1) scales by count-1 and clamps the neighbours.
    const int span = loop ? frame_count : std::max(frame_count - 1, 1);
    const float time = float(span) * time_norm;
    int frame = time >= 0.f ? int(std::floor(time)) : int(time);
    const float frac = time - float(frame);
    int prev = 0;
    int next = 0;
    int next2 = 0;
    if (loop)
    {
        frame %= frame_count;
        if (frame < 0)
        {
            frame += frame_count;
        }
        prev = (frame + frame_count - 1) % frame_count;
        next = (frame + 1) % frame_count;
        next2 = (frame + 2) % frame_count;
    }
    else
    {
        if (frame < 0)
        {
            frame = 0;
        }
        if (frame > span)
        {
            frame = span;
        }
        prev = frame > 0 ? frame - 1 : 0;
        next = frame != span ? frame + 1 : frame;
        next2 = next != span ? next + 1 : next;
    }
    const int directory = 0x40;
    if (size_t(directory + 8) > mina.size())
    {
        return poses;
    }
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
        if (size_t(flag_p) >= mina.size())
        {
            break;
        }
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
        const float w0 = ((flag & 0xE0) == 0) ? 1.f : quat_w(cur[0], cur[1], cur[2]);
        const float w1 = ((flag & 0xE0) == 0) ? 1.f : quat_w(nxt[0], nxt[1], nxt[2]);
        Quat a{cur[0], cur[1], cur[2], w0};
        Quat b{nxt[0], nxt[1], nxt[2], w1};
        if ((flag & 0x1C) == 0x1C)
        {
            poses[size_t(i)].rotation = a;
        }
        else
        {
            poses[size_t(i)].rotation = slerp(a, b, frac);
        }
        if (flag & 2)
        {
            if (size_t(flag_p) < mina.size())
            {
                decode_vec(st, mina[size_t(flag_p)], trans_scale, 0.f, frac, poses[size_t(i)].translation);
                ++flag_p;
            }
        }
        if (flag & 1)
        {
            if (size_t(flag_p) < mina.size())
            {
                decode_vec(st, mina[size_t(flag_p)], scale_scale, 1.f, frac, poses[size_t(i)].scale);
                ++flag_p;
            }
        }
    }
    return poses;
}

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
