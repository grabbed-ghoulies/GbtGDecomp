#include "ghoul_intro.h"
#include "../entities/mind_fields.h"
#include "../../gbtg/text/localtext_resources.h"
#include "../audio/bank_streams.h"
#include "../camera/entity_camera.h"
#include "../camera/view_initialization.h"
#include "../entities/actor_transform.h"
#include "../entities/player_mind_events.h"
#include "../frontend/dialogue_text_metrics.h"
#include "../runtime/name_lookup.h"
#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{
constexpr const char* kIntroView = "ghouly_intro"; // 0x00321808.
constexpr const char* kIntroDitty = "XACT_SOUNDBANK_GSTREAMSDITTIES_GHOULINTRODITTY"; // 0x0030C790.

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// The scene step: 0 while DAT_004312DC is above 0, else DAT_005096C0 * DAT_004312C4.
float frame_step(const RuntimeGhoulIntroContext& context)
{
    return runtime_frame_step(context.clock);
}

// The retail countdown: a positive timer drops by the step and reports true when it reaches 0 (stored as 0).
bool count_down(float& timer, float step)
{
    if (!(0.f < timer))
    {
        return false;
    }
    timer -= step;
    if (0.f < timer)
    {
        return false;
    }
    timer = 0.f;
    return true;
}
}

// Xbox 0x000F4440
void load_runtime_ghoul_intro_rows(RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    const auto rows = acquire_animation_resource(context.registry, "aid_misc_ghoulies_actor_introdata"); // 0x0030C890
    if (rows.data() == nullptr)
    {
        resource_error(context.error, 2); // 0x00032700
    }
    intro.rows = rows; // 0x00035D90's size / 0x44 rows.
    context.notices.first = nullptr;
    context.notices.first_id = 0;
    context.notices.pending = 0;
    intro.shot = 0;
}

// Xbox 0x000F44B0
void unload_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    stop_runtime_ghoul_intro(context);
    if (intro.caption_state != 0)
    {
        release_dialogue_text_pool(intro.caption, *context.text_pools); // 0x0005F460, EBX DAT_00506E54
        intro.caption = nullptr;
        intro.caption_tag = 0;
        intro.caption_time = 0.f;
        intro.caption_state = 0;
    }
    if (intro.camera_bound != 0)
    {
        camera_unlink_view(&intro.camera); // 0x00048C70
        intro.camera_bound = 0;
    }
    if (intro.rows.data() != nullptr)
    {
        release_animation_resource(context.registry, intro.rows); // 0x00035C00
        intro.rows = {};
    }
    for (auto& queued : context.notices.queued)
    {
        queued = {};
    }
    context.notices.count = 0;
}

// Xbox 0x000F4560
void update_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    auto& notices = context.notices;
    const auto& tunables = context.tunables;
    if (notices.pending == 1)
    {
        start_runtime_ghoul_intro(context);
    }
    else if (notices.pending == 2)
    {
        if (count_down(intro.card_time, frame_step(context)))
        {
            find_runtime_ghoul_intro_name(notices.first->class_key, intro.text.data(), context); // AX actor +6
            float dimensions[2];
            const i32 lines = measure_runtime_text_block(context.metrics, 0, intro.text.data(), 1.5f, -1.f,
                context.viewport_width, dimensions); // 0x00060A40, EBX 0
            const float line = float(context.metrics.fonts[0]->cell) * tunables.line_scale; // DAT_00504444 +0x10
            const float y = (float(context.render_height) - line) * tunables.height - float(lines - 1) * line;
            const float x = float(context.render_width) * tunables.half;
            create_dialogue_text_record(*intro.card, intro.text.data(), x, y, 1.5f, 1.5f, 5.5f, 1, 0, 0,
                context.metrics, context.viewport_width, context.next_text_id, context.random); // 0x0005F5C0, EAX 0
        }
        // The intro's actor looks at the camera: mind +0x3C, gaze point +0x40 (DAT_00506DA0).
        if (std::span<u8> mind = context.mind(*notices.first); !mind.empty())
        {
            set(mind, mind_field::gaze_point_set, 1);
            set(mind, mind_field::gaze_point, intro.camera.position);
        }
        const Actor& actor = *notices.first;
        if (count_down(intro.shot_time, frame_step(context)) && -1 < intro.shot)
        {
            if (intro.shot < 3)
            {
                ++intro.shot;
                const float height = runtime_ghoul_intro_height(actor, context);
                place_runtime_ghoul_intro_camera(intro.shot, actor, height, context);
                intro.shot_time = tunables.shot_time;
            }
            else if (intro.shot == 3)
            {
                finish_runtime_ghoul_intro(context);
            }
        }
    }
    if (intro.caption_state == 1 && count_down(intro.caption_time, frame_step(context)))
    {
        find_runtime_ghoul_intro_name(intro.caption_tag, intro.text.data(), context);
        allocate_dialogue_text_pool(intro.caption, 0, 1, 0xE0, context.text_pools); // 0x0005F390, ESI DAT_00506E54
        const float line = float(context.metrics.fonts[0]->cell) * tunables.line_scale;
        const float y = (float(context.render_height) - line) * tunables.height;
        const float x = float(context.render_width) * tunables.half;
        create_dialogue_text_record(*intro.caption, intro.text.data(), x, y, 1.5f, 1.5f, intro.caption_lifetime, 1,
            0, 0, context.metrics, context.viewport_width, context.next_text_id, context.random);
        intro.caption_state = 2;
    }
}

// Xbox 0x000F4A00
void start_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    auto& notices = context.notices;
    const auto& tunables = context.tunables;
    const auto& clock = context.clock;
    if ((context.scene_flags & 0x4000) != 0 || clock.block_decay != 0 || clock.decay_enabled[clock.decay_index] == 0
        || (context.scene_flags & 1) != 0)
    {
        return;
    }
    context.scene_flags |= 4;
    notices.pending = 2;
    // The actor's goal 9 (the "ghouly_intro" goal), through vtable +0xC.
    std::array<u32, 0x42> message{0x4D, 9};
    context.message(*notices.first, message);
    context.pause(0xFF, 0); // 0x00117400(0xFFFF, 0)
    const Actor& actor = *notices.first;
    const u8* row = find_runtime_ghoul_intro_row(actor.class_key, context);
    const float height = row != nullptr ? read_f32({row, 0x44}, 0x40) : tunables.height;
    place_runtime_ghoul_intro_camera(0, actor, height, context);
    intro.shot_time = tunables.shot_time;
    intro.shot = 0;
    intro.card_time = tunables.half;
    allocate_dialogue_text_pool(intro.card, 0, 1, 0xE0, context.text_pools); // 0x0005F390, ESI DAT_00506E3C
    context.random = context.random * 0x19660Du + 0x3C6EF35Fu;
    intro.card->origin[0] = tunables.card_origin_x; // +0x14
    intro.card->origin[2] = std::bit_cast<float>(context.random); // +0x1C
    intro.card->origin[1] = tunables.card_origin_y; // +0x18
    acquire_tracked_audio_bank(kIntroDitty, 0x12, context.streams); // 0x00038910
    ++context.active_strategies;
    ++context.camera_hold;
    intro.showing = 1;
}

// Xbox 0x000F4B90
void finish_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& notices = context.notices;
    if (notices.first != nullptr)
    {
        std::array<u32, 0x42> message{0x3A};
        context.message(*notices.first, message);
    }
    --context.active_strategies;
    --context.camera_hold;
    context.pause(0xFF, 1); // 0x00117400(0xFFFF, 1)
    notices.seen[notices.first_id >> 3] = u8(notices.seen[notices.first_id >> 3] | (1u << (notices.first_id & 7)));
    stop_runtime_ghoul_intro(context);
    start_next_runtime_ghoul_intro(context);
}

// Xbox 0x000F4930, XMM0 delay
void queue_runtime_ghoul_intro_caption(u16 tag, float delay, float lifetime, RuntimeGhoulIntro& intro)
{
    if (intro.caption_state != 0)
    {
        return;
    }
    intro.caption_time = delay;
    intro.caption_tag = tag;
    intro.caption_lifetime = lifetime;
    intro.caption_state = 1;
}

// Xbox 0x000F4C10
void stop_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    auto& notices = context.notices;
    if (intro.showing != 0)
    {
        context.scene_flags &= ~4u;
        if (intro.camera_bound != 0)
        {
            camera_unlink_view(&intro.camera); // 0x00048C70
            intro.camera_bound = 0;
        }
        release_dialogue_text_pool(intro.card, *context.text_pools); // 0x0005F460, EBX DAT_00506E3C
        intro.card = nullptr;
        AudioTrackedBank* bank = find_tracked_audio_bank(kIntroDitty, context.streams); // 0x00038460
        if (bank == nullptr)
        {
            throw Error("ghoul intro ditty is not tracked; 0x000F4C10 reads its +0x88");
        }
        bank->references = bank->references - 1;
        if (bank->references < 1)
        {
            if (bank->id != 0x12)
            {
                if (AudioTrackedBank* resumed = select_resume_audio_bank(*bank, context.streams); resumed != nullptr)
                {
                    resume_tracked_audio_bank(*resumed, context.streams); // 0x00038590, 0x00038360
                }
            }
            detach_tracked_audio_bank(*bank, context.streams); // 0x00038260
            bank->id = 0;
        }
        intro.showing = 0;
    }
    notices.pending = 0;
    intro.shot = 0;
    notices.first = nullptr;
    notices.first_id = 0;
}

// Xbox 0x000F4CC0
u32 start_next_runtime_ghoul_intro(RuntimeGhoulIntroContext& context)
{
    auto& notices = context.notices;
    if (notices.count == 0)
    {
        return 0;
    }
    notices.first = notices.queued[0].actor;
    notices.first_id = notices.queued[0].id;
    notices.pending = 1;
    if (notices.count != 1 && -1 < notices.count - 1)
    {
        for (i32 index = 0; index < notices.count - 1; ++index)
        {
            notices.queued[size_t(index)] = notices.queued[size_t(index) + 1];
        }
    }
    --notices.count;
    start_runtime_ghoul_intro(context);
    return 1;
}

// Xbox 0x000F4220
void place_runtime_ghoul_intro_camera(i32 shot, const Actor& actor, float height, RuntimeGhoulIntroContext& context)
{
    auto& intro = context.intro;
    auto& camera = intro.camera;
    if (intro.camera_bound == 0)
    {
        initialize_camera_view(camera, kIntroView, 1); // 0x000496C0, CL 1
        link_camera_view(0, camera); // 0x00048C20, ESI 0
        camera_bind_named_view(0, kIntroView); // 0x00048CF0
        intro.camera_bound = 1;
    }
    const std::span<const u8> row = context.tunables.shots.subspan(size_t(shot) * 0xC, 0x10);
    const float turn = read_f32(row, 4);
    const float distance = read_f32(row, 0xC);
    float heading;
    const std::span<u8> mind = context.mind(actor);
    const u32 target = mind.empty() ? 0 : get(mind, mind_field::target_record);
    if (target != 0)
    {
        // Toward the mind's target; 0x00105F90 keeps the word it was given (the record address) for a zero vector.
        const Vec3 position = actor_world_position(actor); // 0x000412A0
        const Vec3 aim = context.target_position(target);
        heading = std::bit_cast<float>(target);
        runtime_direction_angle({aim.x - position.x, aim.y - position.y, aim.z - position.z}, heading);
    }
    else
    {
        heading = actor.euler.y; // Actor +0x18.
    }
    const std::span<const u8> body = context.body(actor);
    const float reach = actor.radius_xz * read_f32(body, 0x888) * distance; // Body +8 +0x38.
    const float rise = actor.radius_y * read_f32(body, 0x884) * height; // Body +8 +0x34.
    const Vec3 position = actor_world_position(actor); // 0x000412A0
    const float angle = float(static_cast<long double>(heading) + turn);
    const float x = float(retail_sin(angle)) * reach + position.x; // 0x00153320
    const long double cosine = retail_cos(angle); // 0x00153230
    const float y = position.y + rise;
    const float dx = position.x - x;
    const float z = float(cosine * reach + position.z);
    const float dy = y - y;
    const float dz = float(static_cast<long double>(position.z) - z);
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(dx, dy, dz, yaw, pitch); // 0x00105E20
    camera.position = {x, y, z};
    camera.rotation.x = 0.f - pitch;
    camera.rotation.y = yaw + context.tunables.pi;
    camera.rotation.z = read_f32(row, 8);
}

// Xbox 0x000F4D40
const u8* find_runtime_ghoul_intro_row(u16 tag, const RuntimeGhoulIntroContext& context)
{
    const auto& rows = context.intro.rows;
    for (size_t offset = 0; offset + 0x44 <= rows.size(); offset += 0x44)
    {
        const char* name = reinterpret_cast<const char*>(rows.data() + offset);
        const i32 index = binary_search_names(0xE9, context.actor_tags, name); // 0x000FBFB0
        const u16 row_tag = index == -1 ? 0 : u16(index + 1);
        if (row_tag == tag)
        {
            return rows.data() + offset;
        }
    }
    return nullptr;
}

// Xbox 0x000F4DA0
void find_runtime_ghoul_intro_name(u16 tag, char16_t* output, RuntimeGhoulIntroContext& context)
{
    if (find_runtime_ghoul_intro_row(tag, context) == nullptr)
    {
        return;
    }
    // The key is the object tag's name after its last '_' (ESI stays 0 without one).
    const char* name = context.object_tags[tag];
    i32 last = 0;
    for (i32 index = 0; name[index] != 0; ++index)
    {
        if (name[index] == '_')
        {
            last = index;
        }
    }
    lookup_borrowed_localtext(context.names, "ghoulyintro", name + last + 1, output, context.localtext); // 0x000FF120
}

// Xbox 0x000F4DF0
float runtime_ghoul_intro_height(const Actor& actor, const RuntimeGhoulIntroContext& context)
{
    const u8* row = find_runtime_ghoul_intro_row(actor.class_key, context);
    if (row == nullptr)
    {
        return context.tunables.height;
    }
    return read_f32({row, 0x44}, 0x40);
}
}
