#pragma once

#include "gbtg/aid.hpp"
#include "gbtg/clock.hpp"
#include "gbtg/cutscene.hpp"
#include "gbtg/entity.hpp"
#include "gbtg/model.hpp"
#include "gbtg/script.hpp"

#include <string_view>

namespace gbtg
{

struct InputState
{
    bool quit = false;
    bool up = false;    // D-pad up. PC fills this at the platform edge.
    bool down = false;  // D-pad down.
    bool left = false;  // D-pad left. Virtual keyboard column.
    bool right = false; // D-pad right.
    bool accept = false; // A (0x0100). Empty game slot opens the virtual keyboard.
    bool back = false;   // B (0x0200). Game-select returns to the attract reel.
    bool start = false;  // Start (0x0010). Attract banner accepts this or A, not both.
    bool page = false;   // X (0x0400). Keyboard code 5 cycles the page.
    bool caret_left = false;  // Left trigger, bit 0x4000. Keyboard code 9.
    bool caret_right = false; // Right trigger, bit 0x8000. Keyboard code 10.
};

// Xbox WinMain 0x00032390: gfx_init, fs_init, game_init, then forever frame_tick 0x00033B50.
struct Game
{
    AidStore aids;
    Clock clock;
    std::vector<std::string> scene_order;
    size_t scene_index = 0; // Slot chosen by 0x0012C2D0. Boot starts at 0.
    std::string scene_name; // DAT_00512494; 0x001287F0 defaults to "There is no script for this scene!"
    const Bundle* scene_bundle = nullptr;
    SceneScript scene_script;
    std::vector<std::string> script_aidlists;
    CutsceneQueue cutscenes; // attract / scenecontrol reel of type-7 clips
    std::vector<Mesh> models; // scene models submitted at present
    // Cutscene actor tracks (title books, ghoulies). actor indexes Cutscene::actors.
    struct CutsceneProp
    {
        Mesh mesh;
        size_t actor = 0;
    };
    std::vector<CutsceneProp> cutscene_props;
    size_t cutscene_props_index = static_cast<size_t>(-1);
    struct UiImage
    {
        std::string name;
        RgbaImage image;
    };
    std::vector<UiImage> ui; // screen sprites from the scene bundle (menu plates, icons)
    std::vector<Actor> actors;
    std::vector<Entity> entities;
    Entity* camera = nullptr;
    bool running = true;
    float time = 0.f;
    u32 frame = 0;
    // Widget vtable from 0x0030A000, stride 0x18. -1 is no frontend widget.
    // 0 is the attract banner (entitySceneControlAttract). 1 is slot select,
    // installed by entitySceneControlGameSelect. 4 is the naming keyboard.
    // 3 is occupied-slot contents (0x000E02D0). Its own mode byte is separate
    // and is switched by 0x000DA580.
    int frontend_mode = -1;
    // DAT_00429BC0. Boot 0x000E5A80 stores 1. 0x000E5AE0 installs modes[this]
    // once stream 2 is idle. 0x000DD740 writes 1 again.
    int frontend_pending = 1;
    // Attract banner widget+0x294. 0x00080C10 calls 0x00080CB0 only while this is clear.
    bool attract_banner_latched = false;
    // Scene record at DAT_00510E58. +0x80 is the script 0x0011EB30 copies in.
    // +0x104 is the phase. +0x110 blocks 0x0011E4C0 until the load finishes.
    char requested_script[0x80]{};
    int script_phase = 0;
    bool script_hold = false;
    // Contents plates. 0x000DB450 wraps this. 0 Play, 1 Features, 2 Challenges.
    int contents_row = 0;
    // frontendGameSlotSelectBasic (0x000DF9A0). Index is menu+0xB2C. Empty slots until a save exists.
    int slot = 0;
    int slot_status[3] = {}; // 0 empty. 4 after a name is committed (0x000DD6C0).
    bool slot_up = false;
    bool slot_down = false;
    bool slot_left = false;
    bool slot_right = false;
    bool slot_accept = false;
    bool slot_back = false;
    bool slot_start = false;
    bool slot_page = false;
    bool slot_caret_left = false;
    bool slot_caret_right = false;
    // Virtual keyboard (0x00011870). Empty slots have no name, so A opens this instead of loading.
    bool naming = false;
    int kb_page = 0; // +0xBC. 0 alphabet, 1 symbols, 2 accents.
    int kb_row = 0;  // +0xC0
    int kb_col = 0;  // +0xC4
    int kb_saved = 1; // +0xC8. Column to restore when a wide key is left.
    bool kb_shift = false;
    bool kb_caps = false;
    char16_t slot_name[3][64] = {};
    int slot_name_len[3] = {};
    // Save blob +0x102. 0x00010E9D0 copies the 0x00035960 result here when the name is committed.
    char slot_script[3][0x80] = {};
    char16_t kb_text[64] = {};
    int kb_len = 0;
    int kb_caret = 0;

    void init(const std::filesystem::path& game_dir, std::string_view scene_select = {});
    void tick(float dt, const InputState& in);
};

void script_set_default_name(Game& g);                 // 0x001287F0
void script_set_name(Game& g, std::string_view name);  // 0x00128AB0
void cutscene_queue_start(Game& g);
void cutscene_queue_tick(Game& g, float dt);
void frame_tick(Game& g, float dt, const InputState& in); // 0x00033B50

} // namespace gbtg
