#pragma once
#include <array>

#include "gbtg/aid.h"
#include "gbtg/clock.h"
#include "gbtg/cutscene.h"
#include "gbtg/entity.h"
#include "gbtg/model.h"
#include "gbtg/script.h"

#include <string_view>

namespace gbtg
{

struct RuntimeScriptDialogueContext;
struct RuntimeDialogueOuterDrawContext;
class SceneExecutableResources;
class SceneFactoryRuntime;
class SceneDialogueResourceOwner;
struct NativeAudioRuntime;
struct NativeMovieRuntime;
struct NativeInputStack;

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
    // The XInput packets 0x001555B0 polls, per port: raw thumbsticks, trigger bytes and the Xbox button word
    // (D-pad 0x1..0x8, Start 0x10, Back 0x20, thumbs 0x40/0x80, A..White 0x100..0x2000, triggers 0x4000/0x8000).
    // The booleans above are OR'd into port 0's button word.
    struct PadPacket
    {
        i16 lx = 0;
        i16 ly = 0;
        i16 rx = 0;
        i16 ry = 0;
        u8 trigger_l = 0;
        u8 trigger_r = 0;
        u16 buttons = 0;
    };
    std::array<PadPacket, 4> pads{};
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
    std::vector<Actor> actors;
    std::vector<Entity> entities;
    Entity* camera = nullptr;
    // Borrowed binding supplied by the scene's native dialogue owner. The
    // owner and all referenced resources must outlive script execution.
    RuntimeScriptDialogueContext* script_dialogue = nullptr;
    RuntimeDialogueOuterDrawContext* dialogue_draw = nullptr;
    std::shared_ptr<SceneExecutableResources> executable_resources;
    // The resource registry (DAT_004DF250 / DAT_004DF09C) and the relocated resource memory, for the whole run as
    // retail keeps them: a scene teardown unloads only what nothing references (0x0011C5A0's purge), so a
    // transition's resources outlive the scene it opened in. Declared before the scene so it outlives it.
    std::shared_ptr<SceneDialogueResourceOwner> native_resources;
    // The audio core (event voices, bank streams) from boot; declared before the scene so it outlives it.
    std::shared_ptr<NativeAudioRuntime> native_audio;
    // The movie and file lists and the movie event slots from boot; also declared before the scene.
    std::shared_ptr<NativeMovieRuntime> native_movies;
    std::shared_ptr<NativeInputStack> native_input; // DAT_0050E0E8.., boot-owned.
    std::shared_ptr<SceneFactoryRuntime> factory_scene;
    bool running = true;
    float time = 0.f;
    u32 frame = 0;
    // Scene record at DAT_00510E58. +0x80 is the script 0x0011EB30 copies in.
    // +0x104 is the phase. +0x110 blocks 0x0011E4C0 until the load finishes.
    char requested_script[0x80]{};
    int script_phase = 0;
    bool script_hold = false;

    void init(const std::filesystem::path& game_dir, std::string_view scene_select = {});
    void tick(float dt, const InputState& in);
};

void script_set_default_name(Game& g);                 // 0x001287F0
void script_set_name(Game& g, std::string_view name);  // 0x00128AB0
void cutscene_queue_start(Game& g);
void cutscene_queue_tick(Game& g, float dt);
void frame_tick(Game& g, float dt, const InputState& in); // 0x00033B50

} // namespace gbtg
