#pragma once
#include "gbtg/anim.h"
#include "gbtg/types.h"
#include "../entities/actor_clock.h"

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace gbtg
{
class SceneExecutableResources;
struct RuntimeDialogueActivationContext;
struct AudioBankStreams;
struct GhoulySpawnPool;
struct RuntimeHudLayout;
struct RuntimeActiveActorLink;
struct RuntimeDialogueActivationContext;
struct RuntimeDialogueWidget;
struct RuntimeProfile;
struct RuntimeScoreboardHud;
struct RuntimeScoreboardResources;
struct SaveGameContext;
// One of a challenge's requirements (0x14 bytes, DAT_004E17D8's pool, stride +0). Its op indexes the tests at
// 0x002FDDC8 and the names at 0x004231C8 (challengeReqId_*); 0x0004ADD0 copies the op's size (0x004231CC) from
// the script's record and adds the scoreboard message (0x0012FCA0).
struct RuntimeChallengeRecord
{
    u32 op = 0; // +0
    u32 message = 0; // +4, the scoreboard message, or 0
    u32 value = 0; // +8: a count, a seconds float, or an actor tag in its low word
    u32 word_c = 0; // +0xC
    u32 word_10 = 0; // +0x10
};
// The restart menu's actors (DAT_004E19B0, stride 8): a timer and a player, who cheers or sulks when it runs out.
struct RuntimeChallengeReaction
{
    float timer = 0.f; // +0
    u32 actor = 0; // +4, the player's address
};
// The running challenge's BSS state (DAT_004E17C8 .. DAT_004E1A34). It outlives the scene: game select reads the
// run's result through the saved words.
struct RuntimeChallengeRun
{
    // DAT_004E17C8: 1 none, 2 starting, 3 running, 4 finished (the ditty), 5 the results page, 6 the restart menu.
    i32 state = 0;
    u32 kind = 0; // DAT_004E17CC, the challengeId_* index (0x0004A100)
    i32 mode = 0; // DAT_004E17D0: 0 none, 1 running, 2 failed, 3 passed
    std::unique_ptr<std::vector<RuntimeChallengeRecord>> records; // DAT_004E17D8, 0x000F72D0(0x38)
    float timer = 0.f; // DAT_004E18EC, the wait before the results
    i32 score = 0; // DAT_004E18F0, the run's score, which the results page and its trophy read
    i32 tier = 0; // DAT_004E18F4, 0x0004A400's trophy: 1 none .. 6 platinum
    RuntimeDialogueWidget* widget = nullptr; // DAT_004E19A0, "challengeMenu_restart"
    std::span<u8> input; // DAT_004E19A4, the restart menu's device (0x00107D60)
    u32 input_address = 0; // The device's mapped address.
    i32 choice = 0; // DAT_004E19A8: 1 restart, 2 quit
    u32 hold = 0; // DAT_004E19AC, the menu holds the pause count
    std::unique_ptr<std::vector<RuntimeChallengeReaction>> reactions; // DAT_004E19B0, 0x000F72D0(0x60)
    u32 reset_notice = 0; // DAT_004E1A34, set when a challenge first sets the progress bit 7
};
extern RuntimeChallengeRun g_challenge_run;

// What the challenge runtime reaches in the scene. SceneActorServices binds it while a native scene runs.
struct ChallengeRuntimeServices
{
    u32& flags; // DAT_0050EA9C
    RuntimeProfile& profile; // DAT_00506C54 .. DAT_00506C90
    u8& scene_flags; // **(DAT_00506C74+8)
    const RuntimeActorClock& clock;
    const float& scene_time; // DAT_004312D0
    i32& presses; // DAT_00505680, the Super Scary presses made
    u32& new_button_row; // DAT_00505684
    const u32& score_total; // DAT_0054597C
    const RuntimeActiveActorLink* const& active; // DAT_00511B48
    std::function<i32()> player_energy; // DAT_00511B40's body +0x94, or -1 with no player
    std::function<bool()> player_flag_10; // DAT_00511B40's body +0x15C byte 3 bit 0x10 (no player: false)
    std::function<i32(u16 tag)> count_by_class; // 0x00129240, DX the tag
    std::function<bool()> player_without_energy; // 0x001292D0's player has a body with no energy
    RuntimeScoreboardHud& hud; // DAT_00545AB0, DAT_00433B4C
    RuntimeScoreboardResources& board_resources;
    std::span<const u8> preset; // 0x00314170, 0x001318A0's digits
    RuntimeDialogueActivationContext& dialogue; // 0x000582E0, the input stack and the widget update
    AudioBankStreams& streams; // 0x00038720
    std::function<void(u8 mask, i32 release)> pause; // 0x00117400
    std::function<void(const char* name, i32 mode)> request_script; // 0x0011EB30
    std::function<void(const char* name)> hold_order; // 0x001234B0
    std::function<const char*()> held_order; // DAT_00511B64's first name
    const char* scene_script; // DAT_00510E58
    i32& next_row; // DAT_00429BC0
    u16* selected_game; // DAT_00429BC4
    const u16* challenge_game; // DAT_004E19B4
    std::function<std::vector<u32>()> reacting_players; // DAT_00511B38 players 0x0004A4D0 state 6 takes
    std::function<bool(u32 actor)> player_goal_held; // actor +0x980's current goal kind set while +0x984 is 0
    std::function<void(u32 actor)> drain_player_goals; // 0x00028240 until +0x980 is empty
    std::function<void(u32 actor, std::span<const u32> message)> message_player; // vtable +0xC
    SaveGameContext& saves; // 0x0010DA80
    AnimationResourceRegistry& resources; // 0x00141DA0's text, 0x00142070
    GhoulySpawnPool*& text_pools; // DAT_005043CC, 0x0005F460
    const u8* restart_rows; // 0x002FE2E0, "challengeMenu_restart"
    const RuntimeHudLayout& layout; // DAT_00509740 / DAT_0050974C
};
// The scene binds it while it runs; null outside a native scene.
extern ChallengeRuntimeServices* g_challenge_runtime;
ChallengeRuntimeServices& challenge_runtime(const char* reader);

// Challenge 21's scene order (DAT_004E1748) held once per game (scene flag 8), unless DAT_0050EA9C bit 0x200000.
void hold_runtime_challenge_order(const char* order, u8& scene_flags, u32 flags,
    const std::function<void(const char*)>& hold); // Xbox 0x00049B60, EAX order
void create_runtime_challenge(u16 kind, ChallengeRuntimeServices& context); // Xbox 0x0004A100
void add_runtime_challenge_record(const RuntimeChallengeRecord& record,
    ChallengeRuntimeServices& context); // Xbox 0x0004ADD0, EAX the record
void start_runtime_challenge(ChallengeRuntimeServices& context); // Xbox 0x0004A240
void start_runtime_scene_challenge(ChallengeRuntimeServices& context); // Xbox 0x00049BA0
void set_runtime_challenge_state(i32 state, ChallengeRuntimeServices& context); // Xbox 0x0004A4D0
void tick_runtime_challenge(ChallengeRuntimeServices& context); // Xbox 0x00049DE0
void end_runtime_challenge(ChallengeRuntimeServices& context); // Xbox 0x00049FF0
// The challenge again (the restart menu and the pause's restart, 0x0011CFB0 choice 3).
void restart_run(ChallengeRuntimeServices& context); // Xbox 0x0004A310
// The pause's Challenge Rules in a challenge (DAT_004E17C8 3): "challengereminder_" and the chosen challenge's key
// (DAT_004E17EC) into DAT_00508EB8, its page queued in the scene dialogue; false outside a challenge.
bool show_runtime_challenge_reminder(i32 frontend_state, const SceneExecutableResources& image,
    RuntimeDialogueActivationContext& dialogue); // Xbox 0x0004A270
// The restart menu's draw in the view present (0x0011C240): 0x00142720(0) while the state is 6.
// "challengeMenu_restart" A and B (0x002FE2E0).
void choose_runtime_challenge_restart(); // Xbox 0x0004B350
void choose_runtime_challenge_quit(); // Xbox 0x0004B360
}
