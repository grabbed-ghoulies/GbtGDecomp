#pragma once
#include "scene_control_fields.h"
#include "cutscene_scene_runtime.h"
#include "../entities/candle_weapon.h"
#include "../camera/display_view.h"
#include "gbtg/object_factory.h"
#include "gbtg/retail_field.h"

#include <functional>

namespace gbtg
{
struct SceneControlInstance;
struct SceneControlLists;
// entitySceneControlChapter1Scene10 (factory type 0x94, getter 0x00081010, vtable 0x00505578) over the scene control
// (type 3): the relics store's Super Scary mummy. Message 0x7D (state 2) plays the walk cutscene with the scene's
// pause counters held down and lights the relic candle the cutscene carries (the candle weapon's flame, 0x000C6FB0).
// Each of the cutscene's 0x13 events (0x00081630) counts toward a random total; at it, state 4 sends the
// "sssmummy" ghoulybox to one of six markers, and once a mummy (class 0x22) stands, state 3 puts the candle out, ends
// the cutscene, spawns the player holding the candle at the view and starts the mummy's attack challenge at the
// marker's partner. Message 0x8E 1 makes the mummy cheer; 7 and 8 end the challenge.
struct RelicsStoreControlState
{
    RuntimeCutsceneInstance* cutscene = nullptr; // +0x24, the walk cutscene
    u32 phase = 0; // +0x28, 1 waiting, 2 walking, 3 the challenge, 4 the mummy coming
    i32 pulses = 0; // +0x2C, the cutscene's 0x13 events so far
    i32 marker = 0; // +0x30, 0..5, the DAT_00427600 row the mummy comes to
    u32 mummy = 0; // +0x34, its address
    u32 player = 0; // +0x38, its address
    Actor* candle = nullptr; // +0x3C, the relic candle's actor
    // +0x40: the relic candle's flame block (candle_flame_field). Its pointer words are the four members after it.
    std::array<u8, 0x18> flame_block{};
    const u8* binding = nullptr; // +0x44
    std::unique_ptr<std::array<u8, 0x98>> light; // The +0x48 slot's record.
    std::span<const u8> flame; // +0x50
    std::span<const u8> glow; // +0x54
    u32 pause_held = 0; // +0x58, the control holds DAT_00510CEC and DAT_004DEA24 down
};
struct RelicsStoreControlServices
{
    SceneCutsceneRuntime& cutscenes; // DAT_004E1C34 and the instances' track views.
    RuntimeCandleServices& candle;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    RuntimeActorSoundQueues& sounds;
    u32& random; // DAT_0054DD7C
    i32& pause_count; // DAT_00510CEC
    i32& transition_pause_count; // DAT_004DEA24
    u32& runtime_flags; // DAT_0050EA9C
    const i32& pal; // DAT_005096E0
    const i32& pal_override; // DAT_005096E4
    float& gameplay_time; // DAT_004312D0
    i32& gameplay_mark; // DAT_004312D4
    const i32& gameplay_tick; // DAT_004312D8
    const float& freeze; // DAT_004312DC
    const float& dt; // DAT_005096C0
    const float& scene_scale; // DAT_004312C4
    entity_detail::DisplayView* const& view; // DAT_005077A8
    std::span<const u8> markers; // DAT_00427600: six rows of 4 bytes, a marker's +0x1FC id in the low word.
    std::function<const float&(Actor&)> elapsed; // Actor +0x924.
    std::function<u32(u16)> find_class; // 0x00129EA0, DX class
    std::function<u32(u16)> find_id; // 0x00129ED0, DX id
    std::function<Actor*(u32)> actor_at;
    std::function<std::span<const u8>(u32)> bytes_at; // An actor's bytes by address (+0x1FE).
    std::function<u32(const void*)> address_of; // The control's own address for the cutscene's +0x154.
    std::function<void(u32, std::span<u32>)> message; // Vtable +0xC of the object at an address.
    std::function<u32(const char*, const Vec3&)> spawn_player; // 0x00134920, a zero euler; the scene keeps it.
    std::function<void(std::span<const u8>)> create_object; // 0x00107520; the scene keeps the object.
    std::function<void(std::span<const u8>)> queue_event; // 0x00116810
};
void relics_store_control_object_sizes(ObjectSizes& output); // Xbox 0x00080FF0
bool construct_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081060
void destroy_runtime_relics_store_control(RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081150
// `reference` is the native pointer a 0x7F message carries in its second word.
void message_runtime_relics_store_control(SceneControlBase& base, std::span<const u32> message, const void* reference,
    RelicsStoreControlState& control, SceneControlInstance& instance, SceneControlLists& lists,
    RelicsStoreControlServices& services); // Xbox 0x000811A0
void tick_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081080
void draw_runtime_relics_store_control(RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081310
void set_runtime_relics_store_state(SceneControlBase& base, u8 next, RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081330, ESI state
// The cutscene's 0x13 event: one more pulse, or the mummy's arrival (state 4) at the drawn total.
void pulse_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services); // Xbox 0x00081630
// The position of the DAT_00511AA4 actor whose +0x1FC id is `actor`'s +0x1FE.
Vec3 runtime_relics_store_partner(u32 actor, RelicsStoreControlServices& services); // Xbox 0x00081650, EAX actor
// The mummy's attack challenge (type 0x98) at `position`, its clips "..._attack1_<id>" and "..._attack2_<id>".
void start_runtime_relics_store_attack(i32 id, u32 mummy, const Vec3& position,
    RelicsStoreControlServices& services); // Xbox 0x000815A0, EAX id
}
