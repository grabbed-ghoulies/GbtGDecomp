#pragma once

#include "gbtg/model.hpp"
#include "gbtg/bundle.hpp"
#include "gbtg/loctext.hpp"

namespace gbtg {

struct GameScene {
    Bundle common;
    Bundle playcam;
    Bundle player;
    std::vector<Mesh> meshes;
    std::vector<RgbaImage> hud_textures;
    std::vector<LocTable> loc;
};

GameScene load_vertical_slice(const std::filesystem::path& game_dir);

struct Framebuffer {
    int width = 0;
    int height = 0;
    std::vector<u8> bgra;
};

struct Camera {
    Vec3 eye{0, 12, 20};
    float yaw = 0.0f;
    float pitch = -0.12f;
};

void init_fly_camera(const GameScene& scene, Camera& cam);
Framebuffer rasterize_scene(const GameScene& scene, const Camera& cam, int w, int h);

}  // namespace gbtg
