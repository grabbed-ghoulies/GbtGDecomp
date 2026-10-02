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

Mat mul(const Mat& a, const Mat& b)
{
    Mat o;
    for (int c = 0; c < 4; ++c)
    {
        for (int r = 0; r < 4; ++r)
        {
            o.m[c * 4 + r] = a.m[0 * 4 + r] * b.m[c * 4 + 0] + a.m[1 * 4 + r] * b.m[c * 4 + 1] +
                             a.m[2 * 4 + r] * b.m[c * 4 + 2] + a.m[3 * 4 + r] * b.m[c * 4 + 3];
        }
    }
    return o;
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

bool invert4(const Mat& m, Mat& out)
{
    const float* a = m.m;
    float inv[16];
    inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
             a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
    inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
             a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
    inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
             a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
    inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
              a[12] * a[5] * a[10] + a[12] * a[6] * a[9];
    inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
             a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
    inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
             a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
    inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
             a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
    inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
              a[12] * a[1] * a[10] - a[12] * a[2] * a[9];
    inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
             a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
    inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
             a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
    inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
              a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
    inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
              a[12] * a[1] * a[6] + a[12] * a[2] * a[5];
    inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
             a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
    inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
             a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
    inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
              a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
    inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10] + a[4] * a[2] * a[9] +
              a[8] * a[1] * a[6] - a[8] * a[2] * a[5];
    float det = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
    if (det > -1e-8f && det < 1e-8f)
    {
        return false;
    }
    det = 1.f / det;
    for (int i = 0; i < 16; ++i)
    {
        out.m[i] = inv[i] * det;
    }
    return true;
}

std::vector<Mat> pose_world(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, int n,
                            const std::vector<int>& joint_order)
{
    std::vector<Mat> world(size_t(n), ident());
    for (int i : joint_order)
    {
        const JointPose& jp = pose[size_t(i)];
        Vec3 t = joints[size_t(i)].local;
        t.x += jp.translation.x;
        t.y += jp.translation.y;
        t.z += jp.translation.z;
        Mat local = from_pose(jp.rotation, t, jp.scale);
        const int p = joints[size_t(i)].parent;
        world[size_t(i)] = (p >= 0 && p < n) ? mul(world[size_t(p)], local) : local;
    }
    return world;
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

void skin_matrices(const std::vector<Joint>& joints, const std::vector<JointPose>& pose, float* out, int max_joints,
                   const std::vector<JointPose>* bind)
{
    const int n = int(std::min(joints.size(), pose.size()));
    const int count = std::min(n, max_joints);
    if (!out || count <= 0)
    {
        return;
    }
    std::vector<int> depth(size_t(n), 0);
    for (int i = 0; i < n; ++i)
    {
        int d = 0;
        int p = joints[size_t(i)].parent;
        int guard = 0;
        while (p >= 0 && p < n && guard++ < n)
        {
            ++d;
            p = joints[size_t(p)].parent;
        }
        depth[size_t(i)] = d;
    }
    std::vector<int> joint_order(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
    {
        joint_order[size_t(i)] = i;
    }
    std::sort(joint_order.begin(), joint_order.end(), [&](int a, int b) { return depth[size_t(a)] < depth[size_t(b)]; });

    const std::vector<Mat> world = pose_world(joints, pose, n, joint_order);
    const bool use_bind = bind && int(bind->size()) >= n;
    std::vector<Mat> bind_world;
    if (use_bind)
    {
        bind_world = pose_world(joints, *bind, n, joint_order);
    }
    for (int i = 0; i < count; ++i)
    {
        Mat skin = world[size_t(i)];
        if (use_bind)
        {
            Mat inv;
            if (invert4(bind_world[size_t(i)], inv))
            {
                skin = mul(world[size_t(i)], inv);
            }
        }
        else
        {
            Mat bind_t = ident();
            bind_t.m[12] = -joints[size_t(i)].world.x;
            bind_t.m[13] = -joints[size_t(i)].world.y;
            bind_t.m[14] = -joints[size_t(i)].world.z;
            skin = mul(world[size_t(i)], bind_t);
        }
        std::memcpy(out + size_t(i) * 16, skin.m, sizeof(skin.m));
    }
}

} // namespace gbtg
