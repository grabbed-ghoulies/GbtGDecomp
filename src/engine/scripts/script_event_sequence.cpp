#include "script_event_sequence.h"
#include "script_actions.h"
#include "script_conditions.h"
#include "script_dialogue_actions.h"
#include "script_vm.h"
#include "../cutscenes/door_registration.h"
#include "../entities/actor_live_attachments.h"
#include "../entities/aid_spawn_queue.h"
#include "../entities/aid_spawn_resources.h"
#include "../entities/entity.h"
#include "../entities/entity_heap.h"
#include "../audio/bank_streams.h"
#include "../scene/actor_loading.h"
#include "../scene/cutscene_scene_runtime.h"
#include "../scene/dialogue_scene_runtime.h"
#include "../frontend/dialogue_activation.h"
#include "../camera/camera.h"
#include "../camera/display_view.h"
#include "../camera/view_initialization.h"
#include "../entities/actor_transform.h"
#include "../navigation/spawn_candidates.h"
#include "../scene/scene_markers.h"
#include "gbtg/engine_internal.h"

#include <bit>
#include <cstdio>
#include <cstring>

namespace gbtg
{
i32 g_event_sequence_state = 0;
ScriptEventSequence g_event_sequence;
ScriptEventSequenceContext* g_event_sequence_context = nullptr;

extern u32 g_flags_ea9c; // DAT_0050EA9C.
using scene_detail::g_block_decay; // DAT_0050E768.
using scene_detail::g_decay_arm; // DAT_0030F144[DAT_0051248C].
using scene_detail::g_sim_dt; // DAT_005096C0.
using scene_detail::g_sim_freeze; // DAT_004312DC.
using scene_detail::g_sim_scale; // DAT_004312C4.

namespace
{
constexpr u32 kStepStride = 0x88;

ScriptEventSequenceContext& context()
{
    if (g_event_sequence_context == nullptr)
    {
        throw Error("script event sequence without the scene's services (doors, scoreboard, voices)");
    }
    return *g_event_sequence_context;
}

std::span<u8> step_at(u8* row)
{
    return {row, kStepStride};
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float frame_step()
{
    return g_sim_freeze <= 0.f ? g_sim_dt * g_sim_scale : 0.f;
}

// Retail stack message {id, value} sent to the script queue (0x00116810) and every actor (0x00122C70).
void notify_event_end(Game& g)
{
    const u32 message[]{0x83, 1};
    script_queue_event(g, {reinterpret_cast<const u8*>(message), sizeof(message)}); // 0x00116810
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message)); // 0x00122C70
}

void broadcast_word(u32 id)
{
    // A one-word stack message; the receivers read its id.
    const u32 message[]{id};
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(message)); // 0x00122C70
}

RuntimeScoreboardMessage* board_row(u32 id)
{
    auto* board = context().board;
    if (board == nullptr)
    {
        throw Error("script event icons without the scoreboard DAT_00545AB0");
    }
    for (auto& row : board->messages)
    {
        if (row.id == id)
        {
            return &row;
        }
    }
    return nullptr;
}

bool door_exists(u16 door)
{
    u16 state = 0;
    return runtime_door_state(door, state, context().doors); // 0x0005B510's walk
}

void remove_step(u8*& row, u8*& end)
{
    auto* pool = g_event_sequence.steps;
    if (row < pool->end - pool->stride)
    {
        std::memmove(row, row + pool->stride, size_t(pool->end - (row + pool->stride))); // 0x000F71F0
    }
    pool->end -= pool->stride;
    end -= kStepStride;
    row -= kStepStride;
}

// Xbox 0x000CFD80, ESI step: a linked type 0xF view (+0x84) is unlinked.
void release_step_camera(std::span<u8> step)
{
    if (read_u32(step, 0) == 0xF && read_u32(step, 0x84) != 0)
    {
        camera_unlink_view(&g_event_sequence.key_camera); // 0x00048C70, EAX step +0x24
        put_word(step, 0x84, 0);
    }
}

// From the head, +0 is followed while it is set; the count walks +4 from that node's +4.
template<class Record, auto Back, auto Next>
i32 count_list_tail(Record& head)
{
    Record* node = &head;
    while (node->*Back != nullptr)
    {
        node = node->*Back;
    }
    i32 count = 0;
    for (Record* chain = node->*Next; chain != nullptr; chain = chain->*Next)
    {
        ++count;
    }
    return count;
}
}

// Xbox 0x000D0490
void release_script_event_steps()
{
    auto* pool = g_event_sequence.steps;
    if (pool == nullptr)
    {
        return;
    }
    for (u8* row = pool->begin; row < pool->end; row += kStepStride)
    {
        release_step_camera(step_at(row));
    }
    engine_heap_free(pool); // 0x000F7100 heap, then HeapFree
    g_event_sequence.steps = nullptr;
}

// Xbox 0x000D0870
bool complete_script_event_sequence(Game& g)
{
    ScriptEventSequence& sequence = g_event_sequence;
    if (sequence.steps == nullptr)
    {
        return false;
    }
    release_script_event_steps(); // 0x000D0490
    request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_EVENTCOMPLETE", 10, scene_music_streams(g)); // 0x00038720
    sequence.waiting = 0;
    if (door_exists(sequence.door)) // The inlined walk of 0x0005B510.
    {
        if (g_event_sequence_state == 4)
        {
            notify_event_end(g); // {0x83, 1}: 0x00116810 and 0x00122C70.
        }
        if (door_exists(sequence.door)) // 0x0005B510, SI door
        {
            set_runtime_door_state(sequence.door, 0, 0, 0, context().doors); // 0x0005B540
        }
        g_event_sequence_state = 4;
        script_update_decay_latches(0xFFFF, 0); // 0x00117400
    }
    else
    {
        const bool was_complete = g_event_sequence_state == 4;
        if (was_complete)
        {
            notify_event_end(g); // {0x83, 1}: 0x00116810 and 0x00122C70.
        }
        sequence.door = 0;
        g_event_sequence_state = 0;
        if (was_complete)
        {
            script_update_decay_latches(0xFFFF, 1); // 0x00117400
        }
    }
    g_flags_ea9c |= 0x800000;
    return true;
}

// Xbox 0x000D0530
void reset_script_event_sequence()
{
    release_script_event_steps();
    g_event_sequence.door = 0;
    g_event_sequence_state = 0;
    g_flags_ea9c &= 0xFF7FFFFFu;
}

// Xbox 0x000D0550
void begin_script_event_sequence(u16 door, u32 hold, const char* name)
{
    auto& sequence = g_event_sequence;
    if (sequence.door == 0)
    {
        auto* pool = static_cast<entity_detail::NavigationCellRegistration*>(engine_heap_allocate(
            i32(sizeof(entity_detail::NavigationCellRegistration) + 2 * kStepStride))); // 0x000F72D0(0x120)
        u8* rows = reinterpret_cast<u8*>(pool + 1);
        pool->begin = rows;
        pool->end = rows;
        pool->capacity = rows + 2 * kStepStride;
        pool->stride = u16(kStepStride);
        pool->growth = 2;
        sequence.steps = pool;
    }
    else
    {
        g_event_sequence_state = 0;
    }
    sequence.door = door;
    sequence.hold = hold;
    sequence.page_shown = 0;
    size_t index = 0;
    char character;
    do
    {
        character = name[index];
        sequence.name.at(index) = character;
        ++index;
    } while (character != '\0');
}

// Xbox 0x000D05D0
void queue_script_event_step(std::span<const u8> step)
{
    const u32 type = read_u32(step, 0);
    if ((g_flags_ea9c & 0x10u) != 0)
    {
        switch (type)
        {
        case 4:
        case 7:
        case 8:
        case 9:
        case 10:
        case 0xB:
        case 0xC:
        case 0x10:
        case 0x11:
            return;
        default:
            break;
        }
    }
    auto*& pool = g_event_sequence.steps;
    if (pool == nullptr)
    {
        throw Error("script event step queued before 0x000D0550 allocated DAT_005064E0");
    }
    if (pool->end == pool->capacity)
    {
        pool = entity_detail::grow_navigation_cell_registration(pool); // 0x000662A0
    }
    u8* row = pool->end;
    pool->end += pool->stride;
    const u32 size = read_u32(context().step_sizes, size_t(type) * 0x14);
    if (size > kStepStride || size > step.size())
    {
        throw Error("script event step type " + std::to_string(type) + " size outside its record");
    }
    std::memcpy(row, step.data(), size); // 0x000F71F0
    put_word(step_at(row), 0x10, 0);
    put_word(step_at(row), 0x14, 0);
}

// Xbox 0x000D0680
void play_script_event_sequence(Game& g)
{
    set_script_event_sequence_state(g, 1);
    tick_script_event_door(g);
}

// Xbox 0x000D09A0
void set_script_event_sequence_state(Game& g, i32 state)
{
    auto& sequence = g_event_sequence;
    bool notify = g_event_sequence_state == 4;
    u32 latch = 1;
    if (notify)
    {
        notify_event_end(g);
    }
    switch (state)
    {
    case 0:
        sequence.door = 0;
        break;
    case 1:
    {
        prepare_script_event_steps();
        auto* pool = sequence.steps;
        const u32 count = u32(pool->end - pool->begin) / pool->stride;
        // {0x81, door word, step count, steps}: the steps travel by address.
        const ScriptEventStartMessage message{0x81, sequence.door, 0, count, pool->begin};
        broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message)); // 0x00122C70
        if (door_exists(sequence.door)) // 0x0005B510, SI door
        {
            set_runtime_door_state(sequence.door, 5, sequence.hold, 1, context().doors); // 0x0005B540
            if (sequence.hold == 0)
            {
                request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_EVENTBEGIN", 9, scene_music_streams(g));
            }
        }
        latch = 0;
        notify = true;
        sequence.door_delay = 1.f; // DAT_0032B0F8.
        break;
    }
    case 3:
        latch = 1;
        notify = true;
        break;
    case 4:
        if (door_exists(sequence.door)) // 0x0005B510
        {
            set_runtime_door_state(sequence.door, 0, 0, 0, context().doors); // 0x0005B540
        }
        latch = 0;
        notify = true;
        break;
    default:
        break;
    }
    g_event_sequence_state = state;
    if (notify)
    {
        script_update_decay_latches(0xFFFF, i32(latch)); // 0x00117400
    }
}

// Xbox 0x000D0200: step type 1 keeps one of the class 0x47 key candidates at random.
void start_script_event_key_step()
{
    auto& services = context();
    if (services.random == nullptr)
    {
        throw Error("0x000D0200 needs DAT_0054DD7C");
    }
    const auto actors = services.latch_actors();
    i32 count = 0;
    for (const auto& actor : actors)
    {
        if (u16(actor[1]) == 0x47)
        {
            ++count;
        }
    }
    u32& random = *services.random;
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8.
    const i32 kept = i32(fraction * float(count));
    i32 remaining = count;
    for (const auto& actor : actors)
    {
        if (u16(actor[1]) != 0x47)
        {
            continue;
        }
        --remaining;
        if (remaining == kept)
        {
            continue;
        }
        services.drop_key_candidate(actor[0]);
    }
}

// Xbox 0x000CFAE0
i32 run_script_event_key_carrier_step(std::span<u8> step)
{
    auto& services = context();
    if (read_u32(step, 0x18) == 0)
    {
        for (const auto* node = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
             node != nullptr; node = static_cast<const entity_detail::ListHead*>(node->next))
        {
            const u32 actor = services.active_actor_of(node); // DAT_00511B48 node - 0x944.
            const auto bytes = services.bytes_at(actor, 0xCE8);
            if (bytes[0x96D] == 0) // Node +0x29.
            {
                continue;
            }
            put_word(step, 0x18, 1);
            put_word(bytes, 0xCE4, 1);
            services.create_from_template("aid_objparams_ghoulies_fx_keycarrier", // 0x003038C8
                [actor](std::span<u8> clone)
                {
                    put_word(clone, 8, actor);
                });
            return 0;
        }
        return 0;
    }
    const LiveScript* event = g_live_scripts; // DAT_0050E4DC.
    while (event != nullptr && event->type != 0xB5)
    {
        event = event->next;
    }
    if (event == nullptr)
    {
        return 0;
    }
    const std::span<const u8> message{event->message.data(), event->message.size()}; // Node +8.
    const Vec3 point{read_f32(message, 8), read_f32(message, 0xC), read_f32(message, 0x10)};
    if (read_u32(step, 0x1C) == 0)
    {
        script_update_decay_latches(0xFFFF, 0); // 0x00117400
        put_word(step, 0x1C, 1);
        const u32 key = services.create_from_template("aid_objparams_ghoulies_misc_ghoulykey", // 0x00303948
            [&point](std::span<u8> clone)
            {
                put_float(clone, 8, point.x);
                put_float(clone, 0xC, point.y);
                put_float(clone, 0x10, point.z);
                put_float(clone, 0x14, 0.f);
                put_float(clone, 0x18, 0.f);
                put_float(clone, 0x1C, 0.f);
            });
        put_word(step, 0x20, key);
        if (entity_detail::g_display_view == nullptr)
        {
            throw Error("0x000CFAE0 reads the display view (DAT_005077A8) through address zero");
        }
        const Vec3 eye = entity_detail::g_display_view->position;
        EntityCamNode& camera = g_event_sequence.key_camera;
        if (camera.prev != nullptr || camera.next != nullptr)
        {
            throw Error("0x000CFAE0 links a second step 0xF view while one is linked");
        }
        initialize_camera_view(camera, "ghoulykeycam", 1); // 0x000496C0, CL 1, EDX step +0x24
        link_camera_view(0, camera); // 0x00048C20, ESI 0
        put_word(step, 0x84, 1);
        camera_bind_named_view(0, "ghoulykeycam"); // 0x00048CF0
        camera.position = eye; // Step +0x2C.
        float yaw = 0.f;
        float pitch = 0.f;
        direction_to_yaw_pitch(point.x - eye.x, point.y - eye.y, point.z - eye.z, yaw, pitch); // 0x00105E20
        camera.rotation.x = 0.f - pitch; // Step +0x38.
        camera.rotation.y = yaw + std::bit_cast<float>(0x40490FDBu); // Step +0x3C, DAT_0032B148.
        camera.rotation.z = 0.f; // Step +0x40.
        return 5;
    }
    const float left = read_f32(step, 4) - frame_step(); // DAT_004312DC, DAT_005096C0, DAT_004312C4.
    put_float(step, 4, left);
    if (0.f < left)
    {
        return 5;
    }
    script_update_decay_latches(0xFFFF, 1); // 0x00117400
    release_runtime_script_events(0xB5); // 0x001168C0, EDI 0xB5
    const u32 key = read_u32(step, 0x20);
    if (key != 0)
    {
        services.activate_object(key); // 0x00112CC0, ESI the key
        put_word(step, 0x20, 0);
    }
    release_step_camera(step); // 0x000CFD80
    return 1;
}

// Xbox 0x000D02E0: step type 0xF starts with no camera (+0x84) and +0x18 / +0x1C clear.
void start_script_event_camera_step(std::span<u8> step)
{
    put_word(step, 0x18, 0);
    put_word(step, 0x1C, 0);
    put_word(step, 0x84, 0);
}

// Xbox 0x000D0B10
void prepare_script_event_steps()
{
    auto* pool = g_event_sequence.steps;
    for (u8* row = pool->begin; row < pool->end; row += kStepStride)
    {
        // The table at 0x003039C8: types 1 and 0xF have 0x000D0200 / 0x000D02E0, the others the empty 0x001628B0.
        const u32 type = read_u32(step_at(row), 0);
        if (type == 1)
        {
            start_script_event_key_step(); // 0x000D0200
        }
        if (type == 0xF)
        {
            start_script_event_camera_step(step_at(row)); // 0x000D02E0
        }
    }
}

// Xbox 0x000D12E0
void tick_script_event_door(Game& g)
{
    auto& sequence = g_event_sequence;
    auto& services = context();
    auto& doors = services.doors;
    bool held = false;
    if (sequence.hold != 0)
    {
        for (u32 index = 0; index < u32(doors.rows.size()); ++index)
        {
            const auto& door = doors.rows[index];
            if (read_u16(door.fields, 4) == sequence.door)
            {
                held = read_u32(door.fields, 0x14C) != 0;
                break;
            }
        }
        if (!held)
        {
            if (door_exists(sequence.door)) // 0x0005B510
            {
                request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_EVENTBEGIN", 9, scene_music_streams(g));
            }
            sequence.hold = 0;
        }
    }
    if (door_exists(sequence.door) && !runtime_door_at_event(sequence.door, doors)) // 0x0005B7C0
    {
        return;
    }
    bool ended = false;
    if (!script_event_icons_ready()) // 0x000D1260
    {
        if (sequence.steps != nullptr)
        {
            if (0.f < sequence.icon_delay)
            {
                sequence.icon_delay -= frame_step();
            }
            show_script_event_icons(); // 0x000D0F20
            return;
        }
        ended = g_event_sequence_state == 4;
        if (ended)
        {
            notify_event_end(g);
        }
        sequence.door = 0;
        g_event_sequence_state = 0;
    }
    else
    {
        const float step = frame_step();
        if (sequence.door_delay <= 0.f)
        {
            return;
        }
        if (0.f < sequence.door_delay - step)
        {
            sequence.door_delay -= step;
            return;
        }
        sequence.door_delay = 0.f;
        if (door_exists(sequence.door) && *doors.camera_registered != 0)
        {
            camera_unlink_view(doors.camera); // 0x00048C70
            *doors.camera_registered = 0;
        }
        if (show_script_event_page(g)) // 0x000D1190
        {
            if (g_event_sequence_state == 4)
            {
                notify_event_end(g);
            }
            g_event_sequence_state = 3;
            script_update_decay_latches(0xFFFF, 1); // 0x00117400
            return;
        }
        ended = g_event_sequence_state == 4;
        if (ended)
        {
            notify_event_end(g);
        }
        g_event_sequence_state = 2;
    }
    if (!ended)
    {
        return;
    }
    script_update_decay_latches(0xFFFF, 1); // 0x00117400
}

// Xbox 0x000D0300
void tick_script_event_sequence(Game& g)
{
    if (0 < g_script_e794)
    {
        pulse_script_event_icons(); // 0x000D1200
        return;
    }
    auto& sequence = g_event_sequence;
    switch (g_event_sequence_state)
    {
    case 1:
        pulse_script_event_icons();
        tick_script_event_door(g);
        return;
    case 2:
        if (!show_script_event_page(g))
        {
            return;
        }
        if (g_event_sequence_state == 4)
        {
            notify_event_end(g);
        }
        g_event_sequence_state = 3;
        script_update_decay_latches(0xFFFF, 1);
        return;
    case 3:
        if (!run_script_event_steps(g)) // 0x000D0B40
        {
            return;
        }
        g_flags_ea9c |= 0x00800000u;
        if (door_exists(sequence.door))
        {
            if (g_event_sequence_state == 4)
            {
                notify_event_end(g);
            }
            if (door_exists(sequence.door))
            {
                set_runtime_door_state(sequence.door, 0, 0, 0, context().doors); // 0x0005B540
            }
            g_event_sequence_state = 4;
            script_update_decay_latches(0xFFFF, 0);
            return;
        }
        break;
    case 4:
        if (door_exists(sequence.door))
        {
            u16 state = 0;
            if (!runtime_door_state(sequence.door, state, context().doors)) // 0x0005B780
            {
                return;
            }
            if (state != 0)
            {
                return;
            }
        }
        break;
    default:
        return;
    }
    set_script_event_sequence_state(g, 0);
}

// Xbox 0x000D1200
void pulse_script_event_icons()
{
    auto* pool = g_event_sequence.steps;
    if (pool == nullptr)
    {
        return;
    }
    for (u8* row = pool->begin; row < pool->end; row += kStepStride)
    {
        const u32 id = read_u32(step_at(row), 0x10);
        if (id != 0)
        {
            if (auto* message = board_row(id); message != nullptr)
            {
                message->float_c = context().icon_pulse;
            }
        }
    }
}

// Xbox 0x000D1260
bool script_event_icons_ready()
{
    auto* pool = g_event_sequence.steps;
    if (pool == nullptr)
    {
        return false;
    }
    bool ready = true;
    for (u8* row = pool->begin; row < pool->end; row += kStepStride)
    {
        const u32 id = read_u32(step_at(row), 0x10);
        if (ready && id != 0)
        {
            const auto* message = board_row(id);
            if (message != nullptr && message->word_8 == 2)
            {
                ready = true;
                continue;
            }
        }
        ready = false;
    }
    return ready;
}

// Xbox 0x000CF950
void place_runtime_grim_reaper(const Vec3& point, Vec3& position, Vec3& euler)
{
    auto& services = context();
    const u8* records = services.markers();
    const u16 id = farthest_runtime_marker(0x12, point, records); // 0x0011F0B0, EAX 0x12
    const u8* row = find_runtime_marker(records, i16(id));
    if (row == nullptr)
    {
        throw Error("0x000CF950 finds no row for the Grim Reaper's kind 0x12 marker");
    }
    Vec3 marker;
    std::memcpy(&marker, row + 0xC, sizeof(marker));
    std::memcpy(&euler, row + 0x18, sizeof(euler));
    const i32 floor = entity_detail::navigation_floor_index(marker, -1, 0); // 0x00110BF0
    if (services.random == nullptr || services.occupancy_initial == nullptr)
    {
        throw Error("0x00111420 has no DAT_0054DD7C or occupancy start for the Grim Reaper");
    }
    if (!entity_detail::select_navigation_spawn_position(floor, nullptr, marker, 0.f, 15.f, nullptr, nullptr, 0, 0,
            position, *services.random, *services.occupancy_initial)) // 0x00111420, ESI position
    {
        throw Error("0x00111420 found no spot for the Grim Reaper; retail leaves its position unwritten");
    }
}

// Xbox 0x000CF7B0
u32 summon_runtime_grim_reaper(float first, float second)
{
    auto& services = context();
    const Actor* player = services.first_player(); // DAT_00511B40 minus 0x93C
    if (player == nullptr)
    {
        throw Error("0x000CF7B0 reads the position of a missing player (address zero)");
    }
    const Vec3 point = actor_world_position(*player); // 0x000412A0, ESI player
    Vec3 position{};
    Vec3 euler{};
    place_runtime_grim_reaper(point, position, euler); // 0x000CF950
    const u32 reaper = services.create_from_template("aid_objparams_ghoulies_actor_grimreaper",
        [&position, &euler](std::span<u8> clone)
        {
            std::memcpy(clone.data() + 8, &position, sizeof(position));
            std::memcpy(clone.data() + 0x14, &euler, sizeof(euler));
            const float scale = 1.f; // DAT_0032B0F8.
            std::memcpy(clone.data() + 0x20, &scale, 4);
            const u16 zero = 0;
            std::memcpy(clone.data() + 0x2AA, &zero, 2);
            std::memcpy(clone.data() + 0x2AC, &zero, 2);
        }); // 0x00107720, 0x00107520, 0x000663E0
    if (reaper == 0)
    {
        throw Error("0x000CF7B0 reads +0x954 of a Grim Reaper that was not created (address zero)");
    }
    const u16 cleared = 0;
    std::memcpy(services.bytes_at(reaper, 0x992).data() + 0x990, &cleared, 2);
    std::array<u32, 0x42> appear{};
    appear[0] = 0x4D;
    appear[1] = 4;
    std::memcpy(&appear[2], &position, sizeof(position));
    std::memcpy(&appear[5], &euler, sizeof(euler));
    services.message(reaper, appear); // Vtable +0xC.
    const std::span<u8> target = services.strategy_at(reaper); // +0x954.
    if (first != 0.f)
    {
        put_float(target, 0x40, first);
    }
    if (second != 0.f)
    {
        put_float(target, 0x3C, second);
    }
    return reaper;
}

// Xbox 0x000D0F20
void show_script_event_icons()
{
    auto* pool = g_event_sequence.steps;
    if (pool == nullptr)
    {
        return;
    }
    auto& services = context();
    for (u8* row = pool->begin; row < pool->end; row += kStepStride)
    {
        const auto step = step_at(row);
        const u32 type = read_u32(step, 0);
        u16 kind = 0;
        u32 count = 0xFFFFFFFFu;
        switch (type)
        {
        case 2:
        case 7:
        case 0xD:
            kind = read_u16(step, 0x18); // The step's tag; no count.
            break;
        case 3:
        {
            // The tag's kills still owed, never below zero.
            kind = read_u16(step, 0x1C);
            const i32 left = std::bit_cast<i32>(read_u32(step, 0x18))
                - script_statistic_at_address(0x00512B78u + u32(kind) * 4u);
            count = left < 0 ? 0u : u32(left);
            break;
        }
        case 4:
        case 5:
            // The seconds left, truncated; none once they have run out (DAT_0032B100, 0).
            count = read_f32(step, 0x18) <= 0.f ? 0u : u32(read_f32(step, 0x18));
            break;
        case 6:
            count = 0;
            break;
        case 8:
            // The step's tag once one is set; no count.
            if (read_u16(step, 0x18) != 0)
            {
                kind = read_u16(step, 0x18);
            }
            break;
        case 0xE:
        {
            // Xbox 0x000D01A0: the target (+0x18) less 0x0012AE40 plus 0x0012AE90 (both ECX 0), never below zero.
            const i32 left = std::bit_cast<i32>(read_u32(step, 0x18)) - script_sum_statistics(0)
                + script_sum_paired_statistics(0);
            count = left < 0 ? 0u : u32(left);
            break;
        }
        case 0x12:
        {
            // The tag's (+0x18) kills still owed against +0x1C, never below zero.
            kind = read_u16(step, 0x18);
            const i32 left = std::bit_cast<i32>(read_u32(step, 0x1C))
                - script_statistic_at_address(0x00512B78u + u32(kind) * 4u);
            count = left < 0 ? 0u : u32(left);
            break;
        }
        case 0x10:
        {
            // The target (+0x18) less the attacks (DAT_00525B84 and DAT_005251FC), none once it is reached.
            const i32 left = std::bit_cast<i32>(read_u32(step, 0x18)) - script_statistic_at_address(0x00525B84u)
                - script_statistic_at_address(0x005251FCu);
            count = left < 0 ? 0u : u32(left);
            break;
        }
        default:
            // Every other type shows its icon with no tag and no count.
            break;
        }
        if (read_u32(step, 0x10) == 0 && read_u32(step, 0x14) == 0 && g_event_sequence.icon_delay <= 0.f)
        {
            g_event_sequence.icon_delay = services.icon_delay;
            put_word(step, 0x10, add_script_event_icon(type, kind)); // 0x0012FB80, EDI type, ESI the tag
            put_word(step, 0x14, 1);
            const i16 voice = allocate_audio_event_voice(0, services.voices); // 0x00039830, EBX 0
            if (voice != -1)
            {
                services.voices.voices.at(size_t(voice)).stop = 1; // DAT_004E0CEC
                prepare_audio_event_voice(voice, type == 4 ? "XACT_SOUNDBANK_GPICKUPDITTIES_REAPERICON"
                    : "XACT_SOUNDBANK_GPICKUPDITTIES_EVENTREQICON", services.voices); // 0x00039A20
            }
        }
        const u32 id = read_u32(step, 0x10);
        if (id != 0)
        {
            if (auto* message = board_row(id); message != nullptr)
            {
                message->float_c = services.icon_pulse;
            }
            if (count != 0xFFFFFFFFu)
            {
                set_runtime_scoreboard_count(id, i32(count), *services.board,
                    services.board_resources.counter_preset); // 0x001318A0, ECX id, EDX count
            }
            if (kind != 0)
            {
                set_runtime_scoreboard_icon(id, runtime_scoreboard_tag_icon(kind), *services.board,
                    services.board_resources); // 0x00131A30, then 0x00131920
            }
        }
    }
}

// Xbox 0x000D1190
bool show_script_event_page(Game& g)
{
    auto& sequence = g_event_sequence;
    if ((g_flags_ea9c & 0x4000u) != 0 || sequence.name[0] == '\0')
    {
        return true;
    }
    auto& services = context();
    if (sequence.page_shown != 0)
    {
        return services.prompt_owner == 0;
    }
    if (services.prompt_owner == 0)
    {
        if (services.restart_requested == 1)
        {
            return true;
        }
        if (enqueue_runtime_dialogue_key(sequence.name.data(), 0, scene_dialogue_context(g).dialogue) != 0)
        {
            sequence.page_shown = 1;
        }
    }
    return false;
}

// Xbox 0x000D0B40
bool run_script_event_steps(Game& g)
{
    auto& sequence = g_event_sequence;
    auto& services = context();
    bool succeeded = false;
    bool removed = false;
    auto* pool = sequence.steps;
    if (pool == nullptr)
    {
        return true;
    }
    if (0.f < sequence.icon_delay)
    {
        sequence.icon_delay -= frame_step();
    }
    u32 waiting = 0;
    i32 pending = 0;
    show_script_event_icons(); // 0x000D0F20
    u8* end = pool->end;
    for (u8* row = pool->begin; row < end; row += kStepStride)
    {
        const auto step = step_at(row);
        const u32 type = read_u32(step, 0);
        bool skipped = false;
        if ((g_flags_ea9c & 0x10u) != 0)
        {
            switch (type)
            {
            case 4:
            case 7:
            case 8:
            case 9:
            case 10:
            case 0xB:
            case 0xC:
            case 0x10:
            case 0x11:
                skipped = true;
                break;
            default:
                break;
            }
        }
        if (skipped)
        {
            release_step_camera(step);
            remove_step(row, end);
            continue;
        }
        i32 result = 0;
        if (!services.steps_held())
        {
            if (type == 0)
            {
                result = run_script_event_kill_step(step); // 0x000CFA10
            }
            else if (type == 1)
            {
                result = run_script_event_key_step(step); // 0x000CFA90
            }
            else if (type == 2 || type == 0xD)
            {
                result = run_script_event_tag_step(step); // 0x000CFDA0 (the table holds it for both types)
            }
            else if (type == 3)
            {
                result = run_script_event_kill_count_step(step); // 0x000CFE20
            }
            else if (type == 4)
            {
                result = run_script_event_time_limit_step(step); // 0x000CFE50
            }
            else if (type == 5)
            {
                result = run_script_event_survive_step(step); // 0x000CFEE0
            }
            else if (type == 0xF)
            {
                result = run_script_event_key_carrier_step(step); // 0x000CFAE0
            }
            else if (type == 0xC)
            {
                result = run_script_event_no_points_step(); // 0x000D0140
            }
            else if (type == 8)
            {
                result = run_script_event_next_different_step(step); // 0x000CFFE0
            }
            else if (type == 0xE)
            {
                result = run_script_event_number_step(step); // 0x000D0160
            }
            else if (type == 6)
            {
                result = run_script_event_done_step(); // 0x000CFFD0
            }
            else if (type == 7)
            {
                result = run_script_event_spare_tag_step(step); // 0x000CFF60
            }
            else if (type == 9)
            {
                result = run_script_event_unhit_step(); // 0x000D0090
            }
            else if (type == 10)
            {
                result = run_script_event_unhit_since_step(step); // 0x000D00C0
            }
            else if (type == 0x12)
            {
                result = run_script_event_tag_kill_count_step(step); // 0x000CFDF0
            }
            else if (type == 0xB)
            {
                result = run_script_event_unpaired_step(); // 0x000D0110
            }
            else if (type == 0x10)
            {
                result = run_script_event_attack_limit_step(step); // 0x000D01D0
            }
            else if (type == 0x11)
            {
                result = run_script_event_untouched_step(); // 0x000CFFB0
            }
            else
            {
                throw Error("script event step type " + std::to_string(type)
                    + " lies outside table 0x003039CC (types 0 to 0x12)");
            }
        }
        switch (result)
        {
        case 1:
            broadcast_word(0x82);
            fade_script_event_icon(read_u32(step, 0x10)); // 0x0012FDC0, EDX id
            put_word(step, 0x10, 0);
            release_step_camera(step);
            remove_step(row, end);
            succeeded = true;
            removed = true;
            break;
        case 2:
            ++pending;
            break;
        case 3:
            broadcast_word(0x84);
            fade_script_event_icon(read_u32(step, 0x10));
            put_word(step, 0x10, 0);
            if ((g_flags_ea9c & 0x10u) == 0)
            {
                summon_runtime_grim_reaper(g_script_6540, g_script_6544); // 0x000CF7B0
            }
            release_step_camera(step);
            remove_step(row, end);
            removed = true;
            break;
        case 4:
            if ((g_flags_ea9c & 0x10u) == 0)
            {
                g_event_sequence.unk_52c = 0; // DAT_0050652C
                summon_runtime_grim_reaper(g_script_6540, g_script_6544); // 0x000CF7B0
            }
            break;
        case 5:
            waiting = 1;
            sequence.waiting = 1;
            break;
        default:
            break;
        }
    }
    const u32 remaining = u32(pool->end - pool->begin) / pool->stride;
    if (removed && (remaining == 0 || remaining == u32(pending)))
    {
        for (u8* row = pool->begin; row < pool->end; row += kStepStride)
        {
            const auto step = step_at(row);
            if (auto* message = board_row(read_u32(step, 0x10)); message != nullptr)
            {
                if (1 < message->word_8 && message->word_8 < 4)
                {
                    message->float_c = 0.f;
                }
                message->word_8 = 5;
                message->float_10 = services.icon_fade;
            }
            put_word(step, 0x10, 0);
        }
        release_script_event_steps(); // 0x000D0490
        if (succeeded)
        {
            request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_EVENTCOMPLETE", 10, scene_music_streams(g)); // 0x00303C60
        }
    }
    else if (succeeded)
    {
        request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_EVENTCOMPLETEHALF", 0xB, scene_music_streams(g)); // 0x00303AE0
    }
    sequence.waiting = waiting;
    return sequence.steps == nullptr;
}

// Xbox 0x000D0700
bool script_event_step_delay_done(std::span<u8> timer)
{
    const i32 cutscene = g_event_sequence_context == nullptr ? -1 : context().cutscene_index();
    if (read_u32(timer, 4) == 0)
    {
        put_word(timer, 4, 1);
        put_word(timer, 8, cutscene > -1 ? 1u : 0u);
    }
    if (read_u32(timer, 8) != 0)
    {
        return cutscene <= -1;
    }
    if (read_f32(timer, 0) > 0.f)
    {
        // 0x000D06B0: no step while bit 4 of DAT_0050EA9C is set or the decay latches hold.
        float step = 0.f;
        if ((g_flags_ea9c & 4u) == 0 && g_block_decay == 0 && g_decay_arm != 0)
        {
            step = frame_step();
        }
        const float left = read_f32(timer, 0) - step;
        put_float(timer, 0, left);
        if (!(0.f < left))
        {
            put_float(timer, 0, 0.f);
            return true;
        }
    }
    return read_f32(timer, 0) == 0.f;
}

// Xbox 0x000CFA10
i32 run_script_event_kill_step(std::span<u8> step)
{
    i32 alive = 0;
    for (const auto* node = static_cast<const entity_detail::ListHead*>(entity_detail::g_world.alt.next);
         node != nullptr; node = static_cast<const entity_detail::ListHead*>(node->next))
    {
        if (!g_alt_node_body)
        {
            throw Error("DAT_00511B48 node has no body view");
        }
        if (read_u16(g_alt_node_body(node), 0x94) != 0)
        {
            ++alive;
        }
    }
    if (alive != 0)
    {
        return 0;
    }
    if (count_list_tail<ActorLoadRecord, &ActorLoadRecord::prev, &ActorLoadRecord::next>(g_side_list) != 0) // DAT_00510568
    {
        return 0;
    }
    if (count_list_tail<AidSpawnQueueRecord, &AidSpawnQueueRecord::previous, &AidSpawnQueueRecord::next>(
            g_aid_spawn_queue) != 0) // 0x0011B770, DAT_00510570
    {
        return 0;
    }
    return script_event_step_delay_done(step.subspan(4)) ? 1 : 5; // 0x000D0700, ECX step+4
}

// Xbox 0x000CFDA0: the tag step holds while an actor of its +0x18 tag lives (0x00129240, DX the tag), marking +0x1C
// once one has been seen or the DAT_00512B78 tally for the tag is nonzero; once marked and none lives, it waits out
// its delay.
i32 run_script_event_tag_step(std::span<u8> step)
{
    const u16 tag = read_u16(step, 0x18);
    if (script_count_actors_by_class(tag) == 0) // 0x00129240, DX kept for the tally below
    {
        if (read_u32(step, 0x1C) != 0)
        {
            return script_event_step_delay_done(step.subspan(4)) ? 1 : 5; // 0x000D0700, ECX step+4
        }
        if (script_statistic_at_address(0x00512B78u + u32(tag) * 4u) == 0)
        {
            return 0;
        }
    }
    put_word(step, 0x1C, 1);
    return 0;
}

// Xbox 0x000CFE20: the kill-count step holds while DAT_00512B78[+0x1C] kills fall short of +0x18, then waits out
// its delay.
i32 run_script_event_kill_count_step(std::span<u8> step)
{
    const i32 required = std::bit_cast<i32>(read_u32(step, 0x18));
    const i32 killed = script_statistic_at_address(0x00512B78u + u32(read_u16(step, 0x1C)) * 4u);
    const i32 left = required - killed;
    if (left > 0) // JS / JG
    {
        return 0;
    }
    return script_event_step_delay_done(step.subspan(4)) ? 1 : 5; // 0x000D0700, ECX step+4
}

// Xbox 0x000CFDF0
i32 run_script_event_tag_kill_count_step(std::span<u8> step)
{
    const i32 killed = script_statistic_at_address(0x00512B78u + u32(read_u16(step, 0x18)) * 4u);
    if (killed < std::bit_cast<i32>(read_u32(step, 0x1C))) // JL
    {
        return 0;
    }
    return script_event_step_delay_done(step.subspan(4)) ? 1 : 5; // 0x000D0700, ECX step+4
}

// Xbox 0x000CFFE0: "next one different". Each named tag (+0x1A, count in the byte at +0x4C) keeps its last kill total
// (DAT_005138D0 + DAT_00512F48 by tag) at +0x2C; a new kill of the current tag (+0x18) fails the step (3) unless the
// steps wait (DAT_0050653C); a new kill of another tag makes it the current one.
i32 run_script_event_next_different_step(std::span<u8> step)
{
    u16 latest = 0;
    const u8 count = step[0x4C];
    for (u8 index = 0; index < count; index = u8(index + 1))
    {
        const u16 current = read_u16(step, 0x18);
        const u16 tag = read_u16(step, 0x1A + size_t(index) * 2);
        const size_t seen_at = 0x2C + size_t(index) * 4;
        const i32 seen = std::bit_cast<i32>(read_u32(step, seen_at));
        const i32 total = script_statistic_at_address(0x005138D0u + u32(tag) * 4u)
            + script_statistic_at_address(0x00512F48u + u32(tag) * 4u);
        if (tag == current)
        {
            if (seen < total && g_event_sequence.waiting == 0) // JLE skips; DAT_0050653C
            {
                return 3;
            }
        }
        else if (seen < total)
        {
            latest = tag;
            put_word(step, seen_at, u32(total));
        }
    }
    if (latest != 0)
    {
        const u16 value = latest;
        std::memcpy(step.data() + 0x18, &value, 2);
    }
    return 2;
}

// Xbox 0x000D0160: the number step: its target (+0x18) less 0x0012AE40 plus 0x0012AE90 (both ECX 0) still to go.
i32 run_script_event_number_step(std::span<u8> step)
{
    const i32 left = std::bit_cast<i32>(read_u32(step, 0x18)) - script_sum_statistics(0)
        + script_sum_paired_statistics(0);
    if (left > 0) // JS / JG
    {
        return 0;
    }
    return script_event_step_delay_done(step.subspan(4)) ? 1 : 5; // 0x000D0700, ECX step+4
}

namespace
{
// The time steps' clock: none while bit 4 of DAT_0050EA9C is set or the decay latches hold (DAT_0050E768, the
// byte at [DAT_0051248C]+0x30F144), and none while the simulation is frozen (DAT_004312DC).
float script_event_step_time()
{
    if ((g_flags_ea9c & 4u) != 0 || g_block_decay != 0 || g_decay_arm == 0)
    {
        return 0.f;
    }
    return frame_step();
}
}

// Xbox 0x000CFE50: the time-limit step counts its +0x18 seconds down unless the steps wait (DAT_0050653C); it
// fails once they run out.
i32 run_script_event_time_limit_step(std::span<u8> step)
{
    if (g_event_sequence.waiting != 0)
    {
        return 2;
    }
    const float left = read_f32(step, 0x18) - script_event_step_time();
    put_float(step, 0x18, left);
    if (left <= 0.f)
    {
        return 3;
    }
    return 2;
}

// Xbox 0x000D0110: the step fails (3) once either paired tally (0x0012AE90, ECX 2 then 1) is nonzero while the steps
// do not wait (DAT_0050653C).
i32 run_script_event_unpaired_step()
{
    if (script_sum_paired_statistics(2) == 0 && script_sum_paired_statistics(1) == 0)
    {
        return 2;
    }
    return g_event_sequence.waiting == 0 ? 3 : 2;
}

// Xbox 0x000D01D0: the step fails (3) once the attacks (DAT_00525B84 and DAT_005251FC) pass its +0x18 while the steps
// do not wait (DAT_0050653C).
i32 run_script_event_attack_limit_step(std::span<u8> step)
{
    if (std::bit_cast<i32>(read_u32(step, 0x18)) - script_statistic_at_address(0x00525B84u)
            - script_statistic_at_address(0x005251FCu)
        >= 0) // JNS
    {
        return 2;
    }
    return g_event_sequence.waiting == 0 ? 3 : 2;
}

// Xbox 0x000CFFB0: the step fails (3) once DAT_00512F48 and DAT_005138D0 sum to anything but zero.
i32 run_script_event_untouched_step()
{
    return script_statistic_at_address(0x00512F48u) + script_statistic_at_address(0x005138D0u) != 0 ? 3 : 2;
}

// Xbox 0x000D0140: the no-points step fails once the running score (DAT_00545980) is nonzero while the steps do not
// wait (DAT_0050653C); it never succeeds on its own.
i32 run_script_event_no_points_step()
{
    if (g_545980 == 0 || g_event_sequence.waiting != 0)
    {
        return 2;
    }
    return 3;
}

// Xbox 0x000CFFD0: step type 6 is done at once.
i32 run_script_event_done_step()
{
    return 1;
}

// Xbox 0x000CFF60: the spare step keeps the player's kills of its tag (+0x18; DAT_005138D0 + DAT_00512F48 by tag)
// from its first run at +0x1C, and fails (3) once that total has grown.
i32 run_script_event_spare_tag_step(std::span<u8> step)
{
    const u16 tag = read_u16(step, 0x18);
    const auto total = [tag]()
    {
        return script_statistic_at_address(0x005138D0u + u32(tag) * 4u)
            + script_statistic_at_address(0x00512F48u + u32(tag) * 4u);
    };
    if (std::bit_cast<i32>(read_u32(step, 0x1C)) < 0)
    {
        put_word(step, 0x1C, std::bit_cast<u32>(total()));
    }
    return total() - std::bit_cast<i32>(read_u32(step, 0x1C)) > 0 ? 3 : 2;
}

// Xbox 0x000D0090: the unhit step fails (3) once the player has taken a kind-4 hit (DAT_00522160 + DAT_005217D8)
// while the steps do not wait (DAT_0050653C).
i32 run_script_event_unhit_step()
{
    if (script_statistic_at_address(0x00522160u) + script_statistic_at_address(0x005217D8u) == 0
        || g_event_sequence.waiting != 0)
    {
        return 2;
    }
    return 3;
}

// Xbox 0x000D00C0: as step type 9 for the player's kind-3 hits (DAT_0051E73C + DAT_0051DDB4), counted from the total
// the step keeps at +0x18 on its first run.
i32 run_script_event_unhit_since_step(std::span<u8> step)
{
    if (std::bit_cast<i32>(read_u32(step, 0x18)) < 0)
    {
        put_word(step, 0x18,
            std::bit_cast<u32>(script_statistic_at_address(0x0051E73Cu) + script_statistic_at_address(0x0051DDB4u)));
    }
    if (script_statistic_at_address(0x0051E73Cu) - std::bit_cast<i32>(read_u32(step, 0x18))
                + script_statistic_at_address(0x0051DDB4u)
            < 1
        || g_event_sequence.waiting != 0)
    {
        return 2;
    }
    return 3;
}

// Xbox 0x000CFEE0: the survive step counts its +0x18 seconds down and succeeds once they run out.
i32 run_script_event_survive_step(std::span<u8> step)
{
    const float left = read_f32(step, 0x18) - script_event_step_time();
    put_float(step, 0x18, left);
    if (left <= 0.f)
    {
        return 1;
    }
    return 0;
}

// Xbox 0x000CFA90: the key step waits for event 0xAE (the bg-break key's show has ended), then its delay.
i32 run_script_event_key_step(std::span<u8> step)
{
    if (find_runtime_script_event_type(0xAE) == nullptr) // DAT_0050E4DC walk
    {
        return 0;
    }
    if (!script_event_step_delay_done(step.subspan(4))) // 0x000D0700, ECX step+4
    {
        return 5;
    }
    release_runtime_script_events(0xAE); // 0x001168C0, EDI 0xAE
    return 1;
}

// Xbox 0x0012FB80, EDI kind, ESI tag
u32 add_script_event_icon(u32 kind, u16 tag)
{
    u32 icon = 0;
    u32 word_70 = 1;
    u32 word_78 = 0;
    switch (kind)
    {
    case 0:
        icon = 7;
        break;
    case 1:
        icon = 5;
        break;
    case 2:
    case 0xD:
        icon = runtime_scoreboard_tag_icon(tag); // 0x00131A30, EAX tag
        break;
    case 3:
    case 0x12:
        word_70 = 2;
        icon = runtime_scoreboard_tag_icon(tag); // 0x00131A30, EAX tag
        break;
    case 7:
        icon = runtime_scoreboard_tag_icon(tag); // 0x00131A30, EAX tag
        word_78 = 1;
        break;
    case 4:
        icon = 0xB;
        word_70 = 4;
        break;
    case 5:
        icon = 0xA;
        word_70 = 4;
        break;
    case 6:
        icon = 8;
        word_70 = 3;
        break;
    case 8:
        icon = 0x11;
        word_78 = 1;
        break;
    case 9:
        icon = 0xD;
        break;
    case 0xA:
        icon = 0xC;
        break;
    case 0xB:
        icon = 4;
        break;
    case 0xC:
        icon = 9;
        break;
    case 0xE:
        icon = 0xF;
        word_70 = 2;
        break;
    case 0xF:
        icon = 6;
        break;
    case 0x10:
        icon = 0xE;
        word_70 = 2;
        break;
    case 0x11:
        icon = 0x10;
        word_78 = 1;
        break;
    default:
        throw Error("script event icon kind " + std::to_string(kind) + " needs 0x00131A30");
    }
    auto& services = context();
    if (services.board == nullptr)
    {
        throw Error("script event icons without the scoreboard DAT_00545AB0");
    }
    return add_runtime_scoreboard_message(1, icon, word_70, 0, word_78, 1, 1, *services.board,
        services.board_resources); // 0x001316F0, EBX DAT_00545AB0
}

// Xbox 0x0012FDC0
bool fade_script_event_icon(u32 id)
{
    auto* message = board_row(id);
    if (message == nullptr)
    {
        return false;
    }
    if (1 < message->word_8 && message->word_8 < 4)
    {
        message->float_c = 0.f;
    }
    message->word_8 = 5;
    message->float_10 = context().icon_fade;
    return true;
}
}
