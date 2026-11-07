#include "bg_break_key.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../runtime/rare_replay.h"
#endif
#include "actor_lookup.h"
#include "actor_render_registration.h"
#include "actor_sound_pose.h"
#include "actor_trail_requests.h"
#include "actor_transform.h"
#include "entity_collision.h"
#include "../audio/event_voices.h"
#include "../camera/camera_body_slots.h"
#include "../camera/display_view.h"
#include "../camera/entity_camera.h"
#include "../camera/view_initialization.h"
#include <cmath>
#include <cstring>

namespace gbtg
{
namespace
{
BgBreakKeyState& fields_of(std::span<u8> bytes)
{
    return actor_record_view<BgBreakKeyState>(bytes);
}

const BgBreakKeyState& fields_of(std::span<const u8> bytes)
{
    return actor_record_view<BgBreakKeyState>(bytes);
}

namespace parameter = bg_break_key_parameter;

constexpr const char* kCamera = "breakkeycam"; // 0x00323E14.
}

// Xbox 0x0006CC50
void bg_break_key_object_sizes(ObjectSizes& sizes)
{
    sizes.instance = sizeof(BgBreakKeyState);
    sizes.parameters = parameter::kSize;
}

// Xbox 0x0006CCB0
bool initialize_runtime_bg_break_key(RuntimeBgBreakKey& context, std::span<const u8> parameters)
{
    const auto bytes = context.bytes;
    std::strcpy(fields_of(bytes).unk_934.data(), field_data(parameters, parameter::unk_334));
    fields_of(bytes).background = 0;
    fields_of(bytes).part = -1;
    fields_of(bytes).armed = 0;
    fields_of(bytes).timer = 0.f;
    fields_of(bytes).music_delay = 0.f;
    fields_of(bytes).music_started = 1;
    return true;
}

// Xbox 0x0006CD10
void destroy_runtime_bg_break_key(RuntimeBgBreakKey& context)
{
    if (fields_of(context.bytes).camera_linked != 0)
    {
        camera_unlink_view(&context.camera); // 0x00048C70
        fields_of(context.bytes).camera_linked = 0;
    }
}

// Xbox 0x0006CD40
void dispatch_runtime_bg_break_key_message(RuntimeBgBreakKey& context, std::span<u32> message)
{
    const auto bytes = context.bytes;
    if (message[0] == 0x7D)
    {
        const Vec3 position = actor_world_position(context.actor); // 0x000412A0
        u32 background = fields_of(bytes).background;
        i32 part = fields_of(bytes).part;
        const auto backgrounds = context.backgrounds();
        if (find_background_part_near(position, backgrounds, background, part)) // 0x0013B980
        {
            fields_of(bytes).background = background;
            fields_of(bytes).part = part;
        }
    }
    else if (message[0] == 0x81)
    {
        const i32 count = std::bit_cast<i32>(message[2]);
        if (count > 0)
        {
            const auto steps = context.memory_at(message[3]);
            for (i32 index = 0; index < count; ++index)
            {
                if (read_u32(steps, size_t(index) * 0x88) == 1)
                {
                    fields_of(bytes).armed = 1;
                    break;
                }
            }
        }
    }
    else if (message[0] == 0xAB && message[1] == fields_of(bytes).background
        && std::bit_cast<i32>(message[2]) == fields_of(bytes).part && fields_of(bytes).armed != 0)
    {
        change_runtime_actor_pause(u8(0xFFFF), 0, context.pause()); // 0x00117400
        const u32 source = find_runtime_actor_by_class(0x47, context.latch_owner, context.bytes_at); // 0x00129EA0
        if (source == 0)
        {
            throw Error("0x0006CD40 reads the class 0x47 actor through address zero");
        }
        const Actor& launcher = context.actor_at(source);
        fields_of(bytes).launch_euler = launcher.euler;
        fields_of(bytes).position = launcher.position;
        Vec3 velocity;
        rotate_euler_vector(fields_of(bytes).launch_euler, Vec3{0.f, 0.f, 12.f}, velocity); // 0x00106770
        velocity.y = velocity.y + 108.f; // DAT_0032B688.
        fields_of(bytes).velocity = velocity;
        fields_of(bytes).timer = 2.f; // DAT_0032B110.
        fields_of(bytes).music_delay = 0.4f; // DAT_0032B11C.
        fields_of(bytes).music_started = 0;
        if (context.view == nullptr)
        {
            throw Error("0x0006CD40 reads the display view through address zero");
        }
        fields_of(bytes).eye = context.view->position;
        initialize_camera_view(context.camera, kCamera, 3); // 0x000496C0, CL 3
        link_camera_view(0, context.camera); // 0x00048C20, ESI 0
        camera_bind_named_view(0, kCamera); // 0x00048CF0
        context.camera.position = fields_of(bytes).eye;
        fields_of(bytes).camera_linked = 1;
        fields_of(bytes).spin = 0.1f; // DAT_0032B254.
        std::array<u8, 0x334> parameters{};
        initialize_actor_effect_parameters(parameters.data(), context.empty_name); // 0x000411A0
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x28), "aid_model_ghoulies_misc_eventkey");
        const Vec3 position = fields_of(bytes).position;
        std::memcpy(parameters.data() + 8, &position, 12);
        std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x1A8), "aid_callout_ghoulies_misc_bgbreakkey");
        context.key = context.create(parameters); // 0x00107520
        fields_of(bytes).key = context.key == nullptr ? 0 : context.address_of(context.key->bytes.get());
        if (context.key != nullptr)
        {
            context.callout(*context.key, 0x4A); // 0x0003C040, EDX 0x4A
        }
        fields_of(bytes).active = 1;
        request_audio_event_voice("XACT_SOUNDBANK_GPICKUPDITTIES_KEYAPPEAR", 1, context.voices); // 0x000395E0
    }
    context.base_message(context.base, message); // 0x0003EF70
}

// Xbox 0x0006CFD0 after 0x000420D0
void update_runtime_bg_break_key(RuntimeBgBreakKey& context)
{
    const auto bytes = context.bytes;
    if (fields_of(bytes).active == 0)
    {
        return;
    }
    const u32 key_address = fields_of(bytes).key;
    context.key = key_address == 0 ? nullptr : context.object_at(key_address);
    if (!(fields_of(bytes).timer > 0.f))
    {
        activate_actor_bind_tree(context.actor); // 0x00112CC0, ESI this
        if (context.key == nullptr)
        {
            throw Error("0x0006CFD0 activates the key through address zero");
        }
        activate_actor_bind_tree(context.actor_of(*context.key)); // 0x00112CC0, ESI +0x9F8
        return;
    }
    fields_of(bytes).timer = fields_of(bytes).timer - runtime_actor_step(context.actor, context.clock);
    if (fields_of(bytes).music_started == 0)
    {
        if (fields_of(bytes).music_delay > 0.f)
        {
            fields_of(bytes).music_delay = fields_of(bytes).music_delay
                - runtime_actor_step(context.actor, context.clock); // 0x00042320
        }
        else
        {
            request_audio_event_voice("XACT_SOUNDBANK_GSTREAMSDITTIES_FINDKEYEVENT", 1, context.voices); // 0x000395E0
            fields_of(bytes).music_started = 1;
        }
    }
    if (!(fields_of(bytes).timer > 0.f))
    {
        // Retail sets only the type; its other words are the caller's stack.
        const std::array<u32, 1> event{0xAE};
        context.queue_event({reinterpret_cast<const u8*>(event.data()), sizeof(event)}); // 0x00116810
        change_runtime_actor_pause(u8(0xFFFF), 1, context.pause()); // 0x00117400
        return;
    }
    if (context.key == nullptr)
    {
        throw Error("0x0006CFD0 moves the key through address zero");
    }
    Actor& key = context.actor_of(*context.key);
    if (fields_of(bytes).bounces < 3)
    {
        Vec3 previous = fields_of(bytes).position;
        Vec3 position = previous;
        Vec3 velocity = fields_of(bytes).velocity;
        const float step = runtime_actor_step(context.actor, context.clock); // 0x00042320
        position = Vec3{position.x + velocity.x * step, position.y + velocity.y * step, position.z + velocity.z * step};
        const bool hit = entity_record_sweep_probe(position, previous, 0xC, 0) == 1; // 0x00122480, EBX position
        fields_of(bytes).position = position;
        if (hit)
        {
            const u8* record = entity_detail::g_segment_slots[0].pool.begin; // DAT_00511A20 + 4.
            const float nx = read_f32({record, 0x54}, 0xC);
            const float ny = read_f32({record, 0x54}, 0x10);
            const float nz = read_f32({record, 0x54}, 0x14);
            const float scale = 0.9f; // DAT_0032B174.
            const float along = ((velocity.z * nz + velocity.y * ny + velocity.x * nx) * 1.3f) // DAT_0032B674.
                / (nz * nz + ny * ny + nx * nx);
            velocity = Vec3{velocity.x * scale - along * nx, velocity.y * scale - ny * along,
                velocity.z * scale - nz * along};
            context.callout(*context.key, 0xC); // 0x0003C040, EDX 0xC
            const i32 bounces = fields_of(bytes).bounces + 1;
            fields_of(bytes).bounces = bounces;
            if (bounces == 3)
            {
                velocity = Vec3{0.f, 0.f, 0.f};
            }
        }
        else
        {
            velocity.y = velocity.y - runtime_actor_step(context.actor, context.clock) * 210.f; // DAT_0032B334.
        }
        fields_of(bytes).velocity = velocity;
        actor_set_world_position(key, position); // 0x00041730, EDI the key
    }
    float spin = fields_of(bytes).spin;
    float decay = 0.992f; // DAT_0032B670.
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // Rare Replay: the spin and its decay are per retail tick; a shorter tick turns and decays that part of one.
    spin *= rare_replay_tick_fraction();
    decay = std::pow(decay, rare_replay_tick_fraction());
#endif
    const Vec3 turned{key.euler.x, spin + key.euler.y, key.euler.z};
    wrap_runtime_euler(turned, key.euler); // 0x00105AA0
    const u32 stale = 0;
    std::memcpy(context.key->bytes.get() + 0x2A8, &stale, 4); // Key +0x2A8, the actor's transform.
    key.xform_valid = 0;
    fields_of(bytes).spin = fields_of(bytes).spin * decay;
    const Vec3 eye = fields_of(bytes).eye;
    context.camera.position = eye;
    const Vec3 position = fields_of(bytes).position;
    float yaw = 0.f;
    float pitch = 0.f;
    direction_to_yaw_pitch(position.x - eye.x, position.y - eye.y, position.z - eye.z, yaw, pitch); // 0x00105E20
    context.camera.rotation.x = 0.f - pitch;
    context.camera.rotation.y = yaw + std::bit_cast<float>(0x40490FDBu); // DAT_0032B148.
    context.camera.rotation.z = 0.f;
}
}
