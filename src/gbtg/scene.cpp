#include "gbtg/scene.hpp"
#include "gbtg/loctext.hpp"
#include "gbtg/texture.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace gbtg {
namespace {

bool keep_slice_mesh(std::string_view name) {
    if (name.find("aid_model_ghoulies_background_centralhallway") != std::string_view::npos) {
        return true;
    }
    return name == "aid_model_ghoulies_actor_boy";
}

bool keep_hud_texture(std::string_view name) {
    return name.find("scoreboard_energy_heart") != std::string_view::npos ||
           name.find("scoreboard_energy_numbers_") != std::string_view::npos ||
           name.find("joypad_buttons_a") != std::string_view::npos ||
           name.find("dialogheads_baron") != std::string_view::npos;
}

Vec3 add(Vec3 a, Vec3 b) { return Vec3{a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 sub(Vec3 a, Vec3 b) { return Vec3{a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 mul_s(Vec3 a, float s) { return Vec3{a.x * s, a.y * s, a.z * s}; }
float dotv(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) {
    return Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
Vec3 norm(Vec3 a) {
    const float l = std::sqrt(dotv(a, a));
    return l > 1e-8f ? mul_s(a, 1.f / l) : a;
}

struct Mat4 {
    float m[16]{};
};

Mat4 mul(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int rrow = 0; rrow < 4; ++rrow) {
        for (int c = 0; c < 4; ++c) {
            r.m[c * 4 + rrow] = a.m[0 * 4 + rrow] * b.m[c * 4 + 0] + a.m[1 * 4 + rrow] * b.m[c * 4 + 1] +
                                a.m[2 * 4 + rrow] * b.m[c * 4 + 2] + a.m[3 * 4 + rrow] * b.m[c * 4 + 3];
        }
    }
    return r;
}

Mat4 perspective(float fov_y, float aspect, float zn, float zf) {
    Mat4 r{};
    const float f = 1.f / std::tan(fov_y * 0.5f);
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = zf / (zn - zf);
    r.m[11] = -1.f;
    r.m[14] = (zn * zf) / (zn - zf);
    return r;
}

Mat4 look_at(Vec3 eye, Vec3 at, Vec3 up) {
    const Vec3 z = norm(sub(eye, at));
    const Vec3 x = norm(cross(up, z));
    const Vec3 y = cross(z, x);
    Mat4 r{};
    r.m[0] = x.x;
    r.m[4] = x.y;
    r.m[8] = x.z;
    r.m[1] = y.x;
    r.m[5] = y.y;
    r.m[9] = y.z;
    r.m[2] = z.x;
    r.m[6] = z.y;
    r.m[10] = z.z;
    r.m[12] = -dotv(x, eye);
    r.m[13] = -dotv(y, eye);
    r.m[14] = -dotv(z, eye);
    r.m[15] = 1.f;
    return r;
}

struct ClipV {
    float x, y, z, w;
    float u, v;
};

ClipV transform(const Mat4& m, Vec3 p, Vec2 uv) {
    ClipV o;
    o.x = m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12];
    o.y = m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13];
    o.z = m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14];
    o.w = m.m[3] * p.x + m.m[7] * p.y + m.m[11] * p.z + m.m[15];
    o.u = uv.u;
    o.v = uv.v;
    return o;
}

void fill_tri(Framebuffer& fb, std::vector<float>& zbuf, ClipV a, ClipV b, ClipV c, u8 col[4],
              const RgbaImage* tex) {
    if (a.w <= 0.01f || b.w <= 0.01f || c.w <= 0.01f) {
        return;
    }
    auto to_screen = [&](ClipV v) {
        const float iw = 1.f / v.w;
        return Vec3{(v.x * iw * 0.5f + 0.5f) * float(fb.width), (1.f - (v.y * iw * 0.5f + 0.5f)) * float(fb.height),
                    v.z * iw};
    };
    const Vec3 A = to_screen(a), B = to_screen(b), C = to_screen(c);
    const int minx = std::max(0, int(std::floor(std::min({A.x, B.x, C.x}))));
    const int maxx = std::min(fb.width - 1, int(std::ceil(std::max({A.x, B.x, C.x}))));
    const int miny = std::max(0, int(std::floor(std::min({A.y, B.y, C.y}))));
    const int maxy = std::min(fb.height - 1, int(std::ceil(std::max({A.y, B.y, C.y}))));
    const float area = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);
    if (std::fabs(area) < 1e-6f) {
        return;
    }
    const float ia = 1.f / area;
    for (int y = miny; y <= maxy; ++y) {
        for (int x = minx; x <= maxx; ++x) {
            const float px = float(x) + 0.5f;
            const float py = float(y) + 0.5f;
            const float w0 = ((B.x - px) * (C.y - py) - (B.y - py) * (C.x - px)) * ia;
            const float w1 = ((C.x - px) * (A.y - py) - (C.y - py) * (A.x - px)) * ia;
            const float w2 = 1.f - w0 - w1;
            if (w0 < 0 || w1 < 0 || w2 < 0) {
                continue;
            }
            const float z = w0 * A.z + w1 * B.z + w2 * C.z;
            const size_t idx = size_t(y) * fb.width + x;
            if (z >= zbuf[idx]) {
                continue;
            }
            zbuf[idx] = z;
            u8* d = fb.bgra.data() + idx * 4;
            if (tex && !tex->bgra.empty()) {
                const float u = w0 * a.u + w1 * b.u + w2 * c.u;
                const float v = w0 * a.v + w1 * b.v + w2 * c.v;
                int tx = int(u * tex->width) % tex->width;
                int ty = int(v * tex->height) % tex->height;
                if (tx < 0) {
                    tx += tex->width;
                }
                if (ty < 0) {
                    ty += tex->height;
                }
                const u8* s = tex->bgra.data() + (size_t(ty) * tex->width + tx) * 4;
                d[0] = u8((int(s[0]) * col[0]) / 255);
                d[1] = u8((int(s[1]) * col[1]) / 255);
                d[2] = u8((int(s[2]) * col[2]) / 255);
                d[3] = 255;
            } else {
                d[0] = col[0];
                d[1] = col[1];
                d[2] = col[2];
                d[3] = 255;
            }
        }
    }
}

void mesh_bounds(const Mesh& mesh, Vec3& bmin, Vec3& bmax) {
    bmin = Vec3{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                std::numeric_limits<float>::max()};
    bmax = Vec3{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                std::numeric_limits<float>::lowest()};
    for (const auto& p : mesh.positions) {
        bmin.x = std::min(bmin.x, p.x);
        bmin.y = std::min(bmin.y, p.y);
        bmin.z = std::min(bmin.z, p.z);
        bmax.x = std::max(bmax.x, p.x);
        bmax.y = std::max(bmax.y, p.y);
        bmax.z = std::max(bmax.z, p.z);
    }
}

}  // namespace

GameScene load_vertical_slice(const std::filesystem::path& game_dir) {
    GameScene s;
    s.common = load_bundle(game_dir / "bundles" / "common.bnl");
    s.playcam = load_bundle(game_dir / "bundles" / "aid_script" / "ghoulies_chapter1_scene1_2playcam.bnl");
    s.player = load_bundle(game_dir / "bundles" / "aid_objparams" / "ghoulies_actor_player_boy.bnl");

    auto take_models = [&](const Bundle& b) {
        for (auto& m : extract_models(b)) {
            if (keep_slice_mesh(m.name)) {
                s.meshes.push_back(std::move(m));
            }
        }
    };
    take_models(s.playcam);
    take_models(s.player);

    const Mesh* hall = nullptr;
    Mesh* boy = nullptr;
    for (auto& m : s.meshes) {
        if (m.name.find("centralhallway") != std::string::npos) {
            hall = &m;
        }
        if (m.name == "aid_model_ghoulies_actor_boy") {
            boy = &m;
        }
    }
    if (hall && boy && !hall->positions.empty() && !boy->positions.empty()) {
        Vec3 hmin, hmax, bmin, bmax;
        mesh_bounds(*hall, hmin, hmax);
        mesh_bounds(*boy, bmin, bmax);
        const Vec3 hcenter{(hmin.x + hmax.x) * 0.5f, hmin.y, (hmin.z + hmax.z) * 0.5f};
        const Vec3 bcenter{(bmin.x + bmax.x) * 0.5f, bmin.y, (bmin.z + bmax.z) * 0.5f};
        const Vec3 delta = sub(hcenter, bcenter);
        for (auto& p : boy->positions) {
            p = add(p, delta);
        }
    }

    for (const auto* e : entries_of_type(s.common, ResourceType::Texture)) {
        if (!keep_hud_texture(e->name)) {
            continue;
        }
        TextureHeader hdr;
        if (e->cpu.size() < 28 || !parse_texture_header(e->cpu, hdr)) {
            continue;
        }
        try {
            std::span<const u8> gpu = e->gpu;
            if (!e->gpu_chunks.empty()) {
                const auto& g = e->gpu_chunks[0];
                if (size_t(g.offset) + g.size <= e->gpu.size()) {
                    gpu = e->gpu.subspan(g.offset, g.size);
                }
            }
            s.hud_textures.push_back(decode_xbox_texture(hdr, gpu));
        } catch (...) {
        }
    }

    const auto loc_dir = game_dir / "loctext" / "english";
    if (std::filesystem::exists(loc_dir)) {
        for (auto& p : std::filesystem::directory_iterator(loc_dir)) {
            if (!p.is_regular_file()) {
                continue;
            }
            try {
                auto bytes = read_file(p.path());
                s.loc.push_back(parse_loctext(bytes, p.path().filename().string()));
            } catch (...) {
            }
        }
    }
    return s;
}

void init_fly_camera(const GameScene& scene, Camera& cam) {
    const Mesh* hall = nullptr;
    const Mesh* boy = nullptr;
    for (const auto& m : scene.meshes) {
        if (m.name.find("centralhallway") != std::string::npos) {
            hall = &m;
        }
        if (m.name.find("actor_boy") != std::string::npos) {
            boy = &m;
        }
    }
    Vec3 target{0, 10, 0};
    if (boy && !boy->positions.empty()) {
        Vec3 bmin, bmax;
        mesh_bounds(*boy, bmin, bmax);
        target = Vec3{(bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.55f, (bmin.z + bmax.z) * 0.5f};
    } else if (hall && !hall->positions.empty()) {
        Vec3 hmin, hmax;
        mesh_bounds(*hall, hmin, hmax);
        target = Vec3{(hmin.x + hmax.x) * 0.5f, hmin.y + 14.f, (hmin.z + hmax.z) * 0.5f};
    }
    cam.eye = Vec3{target.x, target.y + 4.f, target.z + 28.f};
    const Vec3 to = sub(target, cam.eye);
    const float len = std::sqrt(dotv(to, to));
    cam.yaw = std::atan2(to.x, to.z);
    cam.pitch = (len > 1e-4f) ? std::asin(std::clamp(to.y / len, -0.99f, 0.99f)) : -0.12f;
}

Framebuffer rasterize_scene(const GameScene& scene, const Camera& cam, int w, int h) {
    Framebuffer fb;
    fb.width = w;
    fb.height = h;
    fb.bgra.assign(size_t(w) * h * 4, 0);
    for (int i = 0; i < w * h; ++i) {
        fb.bgra[i * 4 + 0] = 36;
        fb.bgra[i * 4 + 1] = 22;
        fb.bgra[i * 4 + 2] = 18;
        fb.bgra[i * 4 + 3] = 255;
    }
    std::vector<float> zbuf(size_t(w) * h, 1.0f);

    Vec3 bmin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
    Vec3 bmax{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
              std::numeric_limits<float>::lowest()};
    size_t nverts = 0;
    for (const auto& mesh : scene.meshes) {
        if (mesh.name.find("centralhallway") == std::string::npos) {
            continue;
        }
        for (const auto& p : mesh.positions) {
            bmin.x = std::min(bmin.x, p.x);
            bmin.y = std::min(bmin.y, p.y);
            bmin.z = std::min(bmin.z, p.z);
            bmax.x = std::max(bmax.x, p.x);
            bmax.y = std::max(bmax.y, p.y);
            bmax.z = std::max(bmax.z, p.z);
            ++nverts;
        }
    }
    if (!nverts) {
        for (const auto& mesh : scene.meshes) {
            for (const auto& p : mesh.positions) {
                bmin.x = std::min(bmin.x, p.x);
                bmin.y = std::min(bmin.y, p.y);
                bmin.z = std::min(bmin.z, p.z);
                bmax.x = std::max(bmax.x, p.x);
                bmax.y = std::max(bmax.y, p.y);
                bmax.z = std::max(bmax.z, p.z);
                ++nverts;
            }
        }
    }
    if (!nverts) {
        return fb;
    }
    const float extent = std::max({bmax.x - bmin.x, bmax.y - bmin.y, bmax.z - bmin.z, 1.f});
    const float cp = std::cos(cam.pitch);
    const Vec3 forward{std::sin(cam.yaw) * cp, std::sin(cam.pitch), std::cos(cam.yaw) * cp};
    const Vec3 at = add(cam.eye, forward);
    const Mat4 vp = mul(perspective(0.85f, float(w) / float(h), 0.4f, extent * 8.f),
                        look_at(cam.eye, at, Vec3{0, 1, 0}));
    const Vec3 light = norm(Vec3{0.35f, 0.8f, 0.45f});

    for (const auto& mesh : scene.meshes) {
        const bool is_boy = mesh.name.find("actor_boy") != std::string::npos;
        const bool is_door = mesh.name.find("door_") != std::string::npos;
        // Boy's first embedded texture is a tiny mouth/gums map, not a body atlas.
        const RgbaImage* tex = (!is_boy && is_door && !mesh.textures.empty()) ? &mesh.textures[0] : nullptr;
        const size_t nidx = mesh.indices.size() / 3 * 3;
        for (size_t i = 0; i + 2 < nidx; i += 3) {
            const u16 ia = mesh.indices[i], ib = mesh.indices[i + 1], ic = mesh.indices[i + 2];
            if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
                continue;
            }
            const Vec3 pa = mesh.positions[ia], pb = mesh.positions[ib], pc = mesh.positions[ic];
            const Vec3 n = norm(cross(sub(pb, pa), sub(pc, pa)));
            const float ndl = std::max(0.22f, std::abs(dotv(n, light)));
            u8 col[4];
            if (is_boy) {
                col[0] = u8(55 * ndl);
                col[1] = u8(125 * ndl);
                col[2] = u8(210 * ndl);
            } else if (is_door) {
                col[0] = u8(70 * ndl);
                col[1] = u8(115 * ndl);
                col[2] = u8(170 * ndl);
            } else {
                col[0] = u8(95 * ndl);
                col[1] = u8(108 * ndl);
                col[2] = u8(125 * ndl);
            }
            col[3] = 255;
            Vec2 ua{}, ub{}, uc{};
            if (ia < mesh.uvs.size()) {
                ua = mesh.uvs[ia];
            }
            if (ib < mesh.uvs.size()) {
                ub = mesh.uvs[ib];
            }
            if (ic < mesh.uvs.size()) {
                uc = mesh.uvs[ic];
            }
            fill_tri(fb, zbuf, transform(vp, pa, ua), transform(vp, pb, ub), transform(vp, pc, uc), col, tex);
        }
    }

    int x = 8;
    for (const auto& t : scene.hud_textures) {
        if (t.bgra.empty()) {
            continue;
        }
        const int tw = std::min(t.width, 64);
        const int th = std::min(t.height, 64);
        for (int py = 0; py < th; ++py) {
            for (int px = 0; px < tw; ++px) {
                const int dx = x + px;
                const int dy = 8 + py;
                if (dx >= w || dy >= h) {
                    continue;
                }
                const u8* s = t.bgra.data() + (size_t(py) * t.width + px) * 4;
                u8* d = fb.bgra.data() + (size_t(dy) * w + dx) * 4;
                d[0] = s[0];
                d[1] = s[1];
                d[2] = s[2];
                d[3] = 255;
            }
        }
        x += tw + 6;
        if (x > w - 70) {
            break;
        }
    }
    return fb;
}

}  // namespace gbtg
