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
    bool accept = false; // A. Empty game slot opens the virtual keyboard.
    bool back = false;  // B. Game-select returns to the attract reel.
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
    // Widget mode from the table at 0x0030A000 (stride 0x18).
    // Boot 0x000E5A80 stores 1 in DAT_00429BC0. 0x000E5B27 uses that when the
    // attract stream is idle, which is slot select. 4 is the naming keyboard.
    // 3 is the occupied-slot contents screen (0x000E02D0).
    // 6 is the play row (0x000D5880). A on contents state 1 enters it (0x000D94FD).
    int frontend_mode = 0;
    // Contents rows. 0 is Play, the row whose accept switches to mode 6.
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
