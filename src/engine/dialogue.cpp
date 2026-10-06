#include "gbtg/dialogue.hpp"
#include "gbtg/anim.hpp"
#include "gbtg/gfx.hpp"

#include <algorithm>
#include <cmath>

namespace gbtg
{
namespace
{

const BundleEntry& dialogue_asset(const Game& g, std::string_view name)
{
    const BundleEntry* entry = find_entry(g.aids.common, name);
    if (entry == nullptr)
    {
        throw Error("missing dialogue asset: " + std::string(name));
    }
    return *entry;
}

void dialogue_skin(const Mesh& mesh, std::span<const u8> mina, float norm, bool loop)
{
    const auto pose = sample_mina(mina, norm, loop);
    const int count = int(std::min(pose.size(), std::min(mesh.joints.size(), size_t(127))));
    float bones[128 * 16]{};
    skin_matrices(mesh.joints, pose, bones, count);
    gfx_set_bones(bones, count);
}

} // namespace

// Xbox 0x000584D0: 0x0004D680 loads dialogpage1 with detached actor flags 6;
// 0x00142860 / 0x0013B580 create the separately owned button-bat widget.
void DialogueScreen::open(const Game& g)
{
    close();
    page = parse_cutscene(dialogue_asset(g, "aid_cutscene_ghoulies_dialogs_dialogpage1").cpu,
                          "aid_cutscene_ghoulies_dialogs_dialogpage1");
    meshes.reserve(page.actors.size());
    for (const auto& actor : page.actors)
    {
        meshes.push_back(parse_model(dialogue_asset(g, actor.model)));
    }
    button_bat = parse_model(dialogue_asset(g, "aid_model_ghoulies_actor_buttonbat"));
    const auto stand_cpu = dialogue_asset(g, "aid_anim_ghoulies_props_buttonbat_stand").cpu;
    const auto hop_cpu = dialogue_asset(g, "aid_anim_ghoulies_props_buttonbat_hop").cpu;
    stand.assign(stand_cpu.begin(), stand_cpu.end());
    hop.assign(hop_cpu.begin(), hop_cpu.end());
    // Xbox 0x0006D4F0: stand, state 1, timer DAT_0032B3B0.
    time = 0.f;
    bat_time = 0.f;
    bat_timer = 4.5f;
    bat_state = 1;
    active = true;
}

// Xbox 0x000584D0 mode 0: release the detached cutscene and its owned widget.
void DialogueScreen::close()
{
    for (const auto& mesh : meshes)
    {
        gfx_release_mesh(mesh);
    }
    gfx_release_mesh(button_bat);
    meshes.clear();
    button_bat = Mesh{};
    stand.clear();
    hop.clear();
    active = false;
    gfx_release_dialog_texture();
}

// Xbox 0x00057E60: test duration before advancing; 0x0004E4A0 restarts at zero.
// Xbox 0x0006D5C0: the single-row widget alternates stand and hop. Flybetween
// and doh are selection/error messages, which dialogMenu_basic does not send.
void DialogueScreen::tick(float dt)
{
    if (page.duration <= time)
    {
        time = 0.f;
    }
    time += dt;
    bat_time += dt;
    switch (bat_state)
    {
    case 1:
        if (bat_timer > 0.f)
        {
            bat_timer -= dt;
            if (bat_timer <= 0.f)
            {
                bat_time = 0.f;
                bat_state = 3;
            }
        }
        break;
    case 3:
        if (bat_time >= read_f32(hop, 8))
        {
            // Xbox 0x0006D8D0, state 1.
            bat_time = 0.f;
            bat_timer = 4.5f;
            bat_state = 1;
        }
        break;
    }
}

// Xbox 0x00058150: push the dialogue camera, render its cutscene, then the
// widget; the caller submits the head and glyph quads through the existing HUD.
void DialogueScreen::draw(bool background, bool button, float widget_scale)
{
    Vec3 position;
    Quat orientation;
    float fov;
    if (!sample_cutscene_camera(page, time, position, orientation, fov))
    {
        throw Error("dialogue cutscene has no camera");
    }
    if (background)
    {
        gfx_clear_dialog_background();
    }
    gfx_set_dialog_camera(position, orientation, fov);
    for (size_t i = 0; i < page.actors.size(); ++i)
    {
        const auto& actor = page.actors[i];
        const Vec3 origin{sample_curve(actor.curves.at(0), time, page.duration),
                          sample_curve(actor.curves.at(1), time, page.duration),
                          sample_curve(actor.curves.at(2), time, page.duration)};
        const Vec3 scale{sample_curve(actor.curves.at(3), time, page.duration),
                         sample_curve(actor.curves.at(4), time, page.duration),
                         sample_curve(actor.curves.at(5), time, page.duration)};
        dialogue_skin(meshes[i], actor.mina, time / page.duration, false);
        gfx_draw_mesh(meshes[i], origin, scale);
    }
    // Xbox 0x00142720: bat scale = widget scale * 2; 0x0013B580 subtracts
    // 2 * 1.5 and 21 * 1.5 from the single button's screen anchor.
    if (button)
    {
        draw_button(widget_scale);
    }
    gfx_set_bones(nullptr, 0);
}

// Xbox 0x00142720 / 0x0006D6C0, after the background texture is restored.
void DialogueScreen::draw_button(float widget_scale)
{
    if (widget_scale >= 0.001f)
    {
        const auto& mina = bat_state == 3 ? hop : stand;
        dialogue_skin(button_bat, mina, bat_time / read_f32(mina, 8), bat_state == 1);
        gfx_draw_screen_mesh(button_bat, 320.f - 1.5f * 2.f,
                             dialogue_button_y - 1.5f * 21.f, widget_scale * 2.f);
    }
    gfx_set_bones(nullptr, 0);
}

} // namespace gbtg
