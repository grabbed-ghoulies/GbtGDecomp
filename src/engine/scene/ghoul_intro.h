#pragma once
#include "../entities/actor_clock.h"
#include "../entities/strategy_target_selection.h"
#include "../frontend/dialogue_text_pool.h"
#include "gbtg/entity.h"
#include <array>
#include <functional>

namespace gbtg
{
struct AnimationResourceRegistry;
struct AudioBankStreams;
struct GhoulySpawnPool;
struct LocalTextResourceContext;
struct ResourceErrorState;
struct RuntimeTextMetrics;

// The first-sighting ghoul intro: an imp's notice (0x000F4870) queues it, and the scene tick shows it with its
// own camera, the name card and the intro ditty. DAT_00506D90..DAT_00506E54 and the card text DAT_00507E78.
struct RuntimeGhoulIntro
{
    std::span<const u8> rows; // DAT_00506D90, 0x44-byte rows (objTag name, +0x40 camera height); DAT_00506D94 count.
    EntityCamNode camera{}; // DAT_00506D98, the "ghouly_intro" view.
    u32 camera_bound = 0; // DAT_00506DF8 (camera +0x60).
    i32 shot = 0; // DAT_00506E2C.
    float shot_time = 0.f; // DAT_00506E30.
    float card_time = 0.f; // DAT_00506E34.
    u32 showing = 0; // DAT_00506E38.
    RuntimeDialogueTextPool* card = nullptr; // DAT_00506E3C.
    i32 caption_state = 0; // DAT_00506E44, set by 0x000F4930 (script type 0x5E).
    u16 caption_tag = 0; // DAT_00506E48.
    float caption_time = 0.f; // DAT_00506E4C.
    float caption_lifetime = 0.f; // DAT_00506E50.
    RuntimeDialogueTextPool* caption = nullptr; // DAT_00506E54.
    std::array<char16_t, 0x100> text{}; // DAT_00507E78.
};

struct RuntimeGhoulIntroTunables
{
    std::span<const u8> shots; // LAB_0030C748: 0xC-byte rows, +4 turn, +8 roll, +0xC distance.
    float height; // DAT_0032B17C, also the card's screen fraction.
    float shot_time; // DAT_0032B174.
    float half; // DAT_0032B0F4.
    float card_origin_x; // DAT_0032B580.
    float card_origin_y; // DAT_0032B248.
    float line_scale; // DAT_0032B210.
    float pi; // DAT_0032B148.
};

struct RuntimeGhoulIntroContext
{
    RuntimeGhoulIntro& intro;
    RuntimeTutorialNotices& notices; // DAT_00506DFC, DAT_00506E00, DAT_00506E04..DAT_00506E28.
    const RuntimeGhoulIntroTunables& tunables;
    u32& scene_flags; // DAT_0050EA9C.
    const RuntimeActorClock& clock;
    u32& active_strategies; // DAT_00510CEC.
    u32& camera_hold; // DAT_004DEA24.
    u32& random; // DAT_0054DD7C.
    const char* const* actor_tags; // DAT_0042A854, 0xE9 names.
    const char* const* object_tags; // DAT_0042A850.
    std::span<const u8> names; // DAT_00506E40, aid_loctext_ghoulies_actor_ghoulyintro.
    LocalTextResourceContext& localtext;
    AnimationResourceRegistry& registry;
    ResourceErrorState& error;
    GhoulySpawnPool*& text_pools; // DAT_005043CC.
    const RuntimeTextMetrics& metrics;
    float viewport_width;
    i32& next_text_id;
    const i32& render_width; // DAT_0054DD84.
    const i32& render_height; // DAT_0054DD80.
    AudioBankStreams& streams;
    std::function<void(const Actor&, std::span<const u32>)> message; // Actor vtable +0xC.
    std::function<std::span<u8>(const Actor&)> mind; // Actor +0x958 object; empty when null.
    std::function<std::span<const u8>(const Actor&)> body; // Actor +0x95C object.
    std::function<Vec3(u32)> target_position; // A DAT_00545988 record's +0x20 position.
    std::function<void(u8, i32)> pause; // 0x00117400.
};

void load_runtime_ghoul_intro_rows(RuntimeGhoulIntroContext& context); // Xbox 0x000F4440
void unload_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F44B0
void update_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F4560
void start_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F4A00
// Unless a caption is already queued: the tag's caption after `delay`, shown for `lifetime`.
void queue_runtime_ghoul_intro_caption(u16 tag, float delay, float lifetime,
    RuntimeGhoulIntro& intro); // Xbox 0x000F4930, XMM0 delay
void finish_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F4B90
void stop_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F4C10
u32 start_next_runtime_ghoul_intro(RuntimeGhoulIntroContext& context); // Xbox 0x000F4CC0
void place_runtime_ghoul_intro_camera(i32 shot, const Actor& actor, float height,
    RuntimeGhoulIntroContext& context); // Xbox 0x000F4220, EAX shot
const u8* find_runtime_ghoul_intro_row(u16 tag, const RuntimeGhoulIntroContext& context); // Xbox 0x000F4D40, BX tag
void find_runtime_ghoul_intro_name(u16 tag, char16_t* output,
    RuntimeGhoulIntroContext& context); // Xbox 0x000F4DA0, EAX tag
float runtime_ghoul_intro_height(const Actor& actor,
    const RuntimeGhoulIntroContext& context); // Xbox 0x000F4DF0, EAX actor
}
