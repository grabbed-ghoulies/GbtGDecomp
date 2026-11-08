#include <unordered_set>
#include "../src/engine/entities/player_fields.h"
#include <cstddef>
#include "../src/engine/entities/body_fields.h"
#include "../src/engine/entities/strategy_fields.h"
#include "gbtg/engine_internal.h"
#include "../src/engine/runtime/throw_trace.h"
#include "../src/engine/scene/cutscene_scene_runtime.h"
#include "../src/engine/scene/gameselect_control.h"
#include "../src/engine/scene/scene_controls.h"
#include "../src/engine/frontend/gameselect_slots.h"
#include "../src/engine/frontend/app_error_screen.h"
#include "../src/engine/frontend/gameselect_delete.h"
#include "../src/engine/frontend/gameselect_naming.h"
#include "../src/engine/frontend/gameselect_options.h"
#include "../src/engine/audio/native_audio.h"
#include "../src/engine/video/file_copy.h"
#include "../src/engine/frontend/gameselect_stream_wait.h"
#include "../src/engine/scene/gallery_control.h"
#include "../src/engine/frontend/button_bat_menu.h"
#include "../src/engine/frontend/gameselect_challenges.h"
#include "../src/engine/scene/challenge_runtime.h"
#include "../src/engine/saves/save_games.h"
#include "../src/engine/frontend/gameselect_features.h"
#include "../src/engine/frontend/gameselect_contents.h"
#include "../src/engine/frontend/gameselect_scene_replay.h"
#include "../src/engine/frontend/frontend_state.h"
#include "gbtg/texture.h"
#include "../src/engine/saves/save_root.h"
#include "../src/engine/saves/xdk_saves.h"
#include "../src/engine/scripts/script_conditions.h"
#include "../src/engine/scripts/script_event_sequence.h"
#include "../src/engine/cutscenes/door_registration.h"
#include "../src/engine/entities/bg_break_key.h"
#include "../src/engine/entities/ghouly_bg_break_setup.h"
#if !defined(GBTG_RETAIL_GAME_ONLY)
#include "../src/engine/plugins/plugins.h"
#include "../src/engine/camera/wide_aspect.h"
#endif
#include "gbtg/gfx.h"
#include "gbtg/anim.h"
#include "gbtg/loctext.h"
#include "../src/engine/cutscenes/curve_sampling.h"
#include "../src/engine/scene/dialogue_resource_owner.h"
#include "../src/engine/scene/dialogue_scene_runtime.h"
#include "../src/engine/scene/executable_resources.h"
#include "../src/engine/entities/player_spawn.h"
#include "../src/engine/entities/player_children.h"
#include "../src/engine/entities/player_mind_setup.h"
#include "player_parent_checks.h"
#include "player_mind_owner_checks.h"
#include "player_factory_owner_checks.h"
#include "player_message_checks.h"
#include "player_update_checks.h"
#include "../src/engine/entities/player_messages.h"
#include "../src/engine/scripts/script_table_lookup.h"
#include "../src/engine/entities/entity_heap.h"
#include "../src/engine/entities/aligned_heap.h"
#include "../src/engine/graphics/scene_d3d_device.h"
#include "../src/engine/graphics/effect_tick.h"
#include "../src/engine/scene/scene_actor_update.h"
#include "../src/engine/scene/scene_control_update.h"
#include "../src/engine/scene/scene_pause.h"
#include "../src/engine/frontend/input_stack.h"
#include "../src/engine/frontend/dialogue_widget_state.h"
#include "../src/engine/scene/scene.h"
#include "../src/engine/scene/scene_object_disposal.h"
#include "../src/engine/audio/actor_sound_tick.h"
#include "../src/engine/frontend/dialogue_page_setup.h"
#include "../src/engine/cutscenes/instance_construction.h"
#include "../src/engine/cutscenes/camera_sampling.h"
#include "../src/engine/cutscenes/quaternion_math.h"
#include "../src/engine/cutscenes/actor_tracks.h"
#include "../src/engine/video/movie_errors.h"
#include "../src/engine/video/movie_startup.h"
#include "../src/engine/cutscenes/event_pause.h"
#include "../src/engine/cutscenes/event_audio.h"
#include "../src/engine/cutscenes/event_timing.h"
#include "../src/engine/cutscenes/event_binding.h"
#include "../src/engine/camera/cutscene_impulse.h"
#include "../src/engine/runtime/interpolation.h"
#include "../src/engine/entities/ghouly_requests.h"
#include "../src/engine/entities/ghouly_swap_setup.h"
#include "../src/engine/entities/actor_spawn_eligibility.h"
#include "../src/engine/entities/actor_lookup.h"
#include "../src/engine/entities/actor_spawn_query.h"
#include "../src/engine/entities/shape_transform.h"
#include "../src/engine/entities/actor_world_lists.h"
#include "../src/engine/entities/actor_render_registration.h"
#include "../src/engine/entities/actor_model_anchors.h"
#include "../src/engine/entities/actor_bind_tree.h"
#include "../src/engine/entities/actor_pose_updates.h"
#include "../src/engine/entities/actor_live_attachments.h"
#include "../src/engine/entities/actor_sound_pose.h"
#include "../src/engine/camera/actor_camera_effects.h"
#include "../src/engine/camera/view_initialization.h"
#include "../src/engine/camera/display_view.h"
#include "../src/engine/graphics/actor_effect_nodes.h"
#include "../src/engine/graphics/actor_effect_stream.h"
#include "../src/engine/entities/actor_attachment_requests.h"
#include "../src/engine/entities/actor_factory_owner.h"
#include "../src/engine/frontend/dialogue_draw_outer.h"
#include "../src/engine/entities/button_bat_update.h"
#include "../src/engine/entities/actor_attachment_events.h"
#include "../src/engine/audio/resource_requests.h"
#include "../src/engine/navigation/navigation.h"
#include "../src/engine/entities/ghouly_spawn_record.h"
#include "../src/engine/entities/ghouly_box_decoder.h"
#include "../src/engine/entities/ghouly_spawn_scheduler.h"
#include "../src/engine/entities/ghouly_spawn_trigger.h"
#include "../src/engine/entities/spawn_pool_selection.h"
#include "../src/engine/entities/spawn_queue_gates.h"
#include "../src/engine/entities/actor_trail_requests.h"
#include "../src/engine/entities/actor_scare_requests.h"
#include "../src/engine/entities/aid_spawn_queue.h"
#include "../src/engine/entities/aid_spawn_resources.h"
#include "../src/engine/scene/live_spawn_tick.h"
#include "../src/engine/scene/factory_scene_runtime.h"
#include "../src/engine/scene/scene_actor_services.h"
#include "../src/engine/entities/camera_director.h"
#include "../src/engine/navigation/occupancy.h"
#include <cstdlib>
#include <process.h>
#include <fstream>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include "../src/engine/audio/bank_lookup.h"
#include "../src/engine/audio/core_source.h"
#include "../src/engine/audio/core_cue.h"
#include "../src/engine/audio/event_voices.h"
#include "../src/engine/audio/pcm_backend.h"
#include "../src/engine/entities/entity.h"
#include "gbtg/resource_error.h"
#include "gbtg/runtime_localtext.h"
#include "../src/gbtg/text/localtext_resources.h"
#include "../src/engine/frontend/dialogue_tags.h"
#include "../src/engine/frontend/dialogue_sound_tags.h"
#include "../src/engine/frontend/dialogue_tag_actions.h"
#include "../src/engine/frontend/dialogue_resources.h"
#include "../src/engine/frontend/dialogue_trophies.h"
#include "../src/engine/frontend/dialogue_widget_parameters.h"
#include "../src/engine/frontend/dialogue_text_pool.h"
#include "../src/engine/frontend/dialogue_text_metrics.h"
#include "../src/engine/frontend/dialogue_widget_state.h"
#include "../src/engine/frontend/dialogue_widget_construction.h"
#include "../src/engine/frontend/dialogue_widget_update.h"
#include "../src/engine/frontend/dialogue_widget_bat.h"
#include "../src/engine/frontend/dialogue_input.h"
#include "../src/engine/frontend/input_activation.h"
#include "../src/engine/audio/sound_names.h"
#include "../src/engine/audio/footstep_names.h"
#include "../src/engine/runtime/crt_format.h"
#include "../src/engine/runtime/crt_strings.h"
#include "../src/engine/frontend/dialogue_substitutions.h"
#include "../src/engine/frontend/dialogue_measurement.h"
#include "../src/engine/frontend/dialogue_page_layout.h"
#include "../src/engine/frontend/dialogue_page_setup.h"
#include "../src/engine/runtime/crt_mantissa.h"
#include "../src/engine/runtime/crt_decimal.h"
#include "../src/engine/runtime/crt_runtime.h"
#include "../src/engine/entities/entity.h"
#include "../src/engine/scene/actor_loading.h"
#include "../src/engine/scene/background_geometry.h"
#include "../src/engine/entities/physical_shape_events.h"
#include "../src/engine/entities/actor_matrix_setup.h"
#include "../src/engine/scene/background_volumes.h"
#include "../src/engine/entities/shape_copy.h"
#include "../src/engine/scene/background_model_nodes.h"
#include "../src/engine/camera/camera.h"
#include "../src/engine/scripts/script_actions.h"
#include "../src/engine/scripts/script_vm.h"
#include "../src/engine/transitions/scene_transition.h"
#include "../src/engine/transitions/scene_transitions.h"
#include "../src/engine/scripts/script_conditions.h"
#include "../src/engine/entities/bg_break_key.h"
#include "../src/engine/entities/ghouly_bg_break_setup.h"

#include <cstring>
#include <bit>
#include <limits>
#include <iostream>
#if defined(_WIN32)
#define NOMINMAX
#include <xaudio2.h>
#endif

namespace gbtg
{
extern i32& g_transition_row;
extern i32& g_transition_phase;
extern i32 g_flag_2474;
extern i32 g_draw_e1ec;
extern i32 g_script_30d0c;
}

void check_camera_display();
void check_script_actor_query();
void check_scene_render();

namespace
{
// The game-select control's widget while it is slot select (mode-table row 1), or null.
// Whether the game select screen on top takes input now: the states that hand its prompts the front-end device
// (0x00142460), with the prompts up. Between two screens game select isn't listening. Nothing holds outside its
// scene: a challenge's run keeps the challenges widget open underneath it.
bool gameselect_listening(const gbtg::Game& game)
{
    if (game.scene_script.name.find("gameselect") == std::string::npos)
    {
        return true;
    }
    if (gbtg::find_runtime_scene_control(0x9F) != nullptr) // The Gallery reads its own device.
    {
        return true;
    }
    if (game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0) // A closes a page.
    {
        return true;
    }
    const auto& frontend = gbtg::g_frontend;
    const auto prompts_up = [](const auto* widget)
    {
        return widget->prompts != nullptr && widget->prompts->mode == 2 && 0 < widget->prompts->count;
    };
    const auto idle = [&prompts_up](const auto* widget)
    {
        return widget->state == 2 && prompts_up(widget);
    };
    if (frontend.naming_widget != nullptr)
    {
        return frontend.naming_widget->state == 2 && frontend.naming_widget->keyboard != nullptr;
    }
    if (frontend.bat_menu != nullptr && frontend.bat_menu->state != 2)
    {
        return false;
    }
    if (frontend.deletion_widget != nullptr)
    {
        return idle(frontend.deletion_widget);
    }
    if (frontend.scene_replay_widget != nullptr)
    {
        return idle(frontend.scene_replay_widget);
    }
    if (frontend.chapter_replay_widget != nullptr)
    {
        return idle(frontend.chapter_replay_widget);
    }
    if (frontend.bonus_book_replay_widget != nullptr)
    {
        return idle(frontend.bonus_book_replay_widget);
    }
    if (frontend.story_book_widget != nullptr)
    {
        return idle(frontend.story_book_widget);
    }
    if (frontend.options_widget != nullptr)
    {
        return idle(frontend.options_widget);
    }
    if (frontend.challenges_widget != nullptr)
    {
        return idle(frontend.challenges_widget);
    }
    if (frontend.features_widget != nullptr)
    {
        return idle(frontend.features_widget);
    }
    if (frontend.contents_widget != nullptr)
    {
        const auto* contents = frontend.contents_widget;
        return 2 <= contents->state && contents->state <= 5 && prompts_up(contents);
    }
    if (frontend.slot_widget != nullptr)
    {
        return frontend.slot_widget->state == 5 && prompts_up(frontend.slot_widget);
    }
    return false;
}

// A scheduled press waits for the screen: the frame numbers below count from when game select opened, and a press
// that lands while the screen isn't listening is lost, which leaves the rest of the sequence one press behind. While
// it waits, `ready` moves up so the schedule holds at this press.
void hold_gameselect_press(const gbtg::Game& game, gbtg::InputState& input, int& ready)
{
    bool pressed = input.up || input.down || input.left || input.right || input.accept || input.back || input.page;
    for (const auto& pad : input.pads)
    {
        pressed = pressed || pad.buttons != 0;
    }
    if (ready < 0 || !pressed || gameselect_listening(game))
    {
        return;
    }
    input.up = input.down = input.left = input.right = input.accept = input.back = input.page = false;
    for (auto& pad : input.pads)
    {
        pad.buttons = 0;
    }
    ++ready;
}

gbtg::SlotSelectWidget* native_slot_select()
{
    gbtg::SceneControlInstance* control = gbtg::find_runtime_scene_control(0x9A);
    if (control == nullptr || control->gameselect == nullptr || control->gameselect->row == nullptr
        || gbtg::gameselect_widget_row_index(*control->gameselect->row) != 1 || control->gameselect->widget == nullptr)
    {
        return nullptr;
    }
    return static_cast<gbtg::SlotSelectWidget*>(control->gameselect->widget.get());
}

struct RequestProbe : gbtg::GhoulyController
{
    int calls = 0;
    gbtg::GhoulyRequest received;

    RequestProbe()
    {
        message = [](gbtg::GhoulyController& controller, void* message)
        {
            auto& request = *static_cast<gbtg::GhoulyRequest*>(message);
            auto& probe = static_cast<RequestProbe&>(controller);
            ++probe.calls;
            probe.received = request;
            request.result = 0xCAFE;
        };
    }
};

void check_ghoulybox_decoder(const char* game_directory)
{
    using namespace gbtg;
    auto* record = allocate_ghouly_spawn_record();
    std::memset(record->pool_8->begin, 0xA5, 2 * 0x288);
    append_ghouly_spawn_variant(*record, "variant");
    append_ghouly_spawn_alternate(*record, "alternate");
    auto* rows = reinterpret_cast<GhoulySpawnDefinition*>(record->pool_8->begin);
    if (rows[0].second_override[0] != char(0xA5) || rows[1].second_override[0] != char(0xA5) ||
        record->value_b0 != 0 || rows[0].weight != 1.f || rows[1].weight != 0.f)
    {
        throw Error("short spawn rows overwrote untouched fields");
    }
    std::vector<u8> stream;
    auto node = [&](u32 type, size_t size)
    {
        const size_t offset = stream.size();
        stream.resize(offset + size);
        std::memcpy(stream.data() + offset, &size, 4);
        std::memcpy(stream.data() + offset + 4, &type, 4);
        return offset;
    };
    auto word = [&](size_t offset, u32 value)
    {
        std::memcpy(stream.data() + offset, &value, 4);
    };
    size_t at = node(1, 0x210);
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 8), "actor");
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 0x8C), "skin");
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 0x10C), "first");
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 0x18C), "second");
    word(at + 0x88, u32(-25));
    word(at + 0x20C, 0x12345678);
    at = node(2, 0x88);
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 8), "variant2");
    at = node(8, 0x88);
    std::strcpy(reinterpret_cast<char*>(stream.data() + at + 8), "alternate2");
    at = node(3, 0xC);
    word(at + 8, 9);
    node(5, 8);
    at = node(4, 0x10);
    word(at + 8, u32(-3));
    word(at + 0xC, u32(-3));
    at = node(6, 0xC);
    word(at + 8, 0xDEADBEEF);
    at = node(7, 0x10);
    word(at + 8, 17);
    word(at + 0xC, 0x7FC01234);
    node(99, 8);
    node(0, 8);
    at = node(3, 0xC);
    word(at + 8, 999);
    u32 random = 0x12345678;
    decode_ghoulybox(*record, stream, random);
    rows = reinterpret_cast<GhoulySpawnDefinition*>(record->pool_8->begin);
    if (record->pool_8->cursor - record->pool_8->begin != 5 * 0x288 ||
        record->pool_8->limit - record->pool_8->begin != 6 * 0x288 ||
        std::strcmp(rows[0].variant, "variant") || std::strcmp(rows[1].alternate, "alternate") ||
        std::strcmp(rows[2].name, "actor") || std::strcmp(rows[2].second_override, "second") ||
        rows[2].weight != -0.25f || rows[2].flags != 0x12345678 || record->value_b0 != 1 ||
        std::strcmp(rows[3].variant, "variant2") || std::strcmp(rows[4].alternate, "alternate2") ||
        record->remaining_c != -2 || record->value_14 != 0xDEADBEEF || record->value_18 != 17 ||
        record->value_1c != 0x7FC01234 || std::bit_cast<u32>(record->timer_20) != 0x3A83126F ||
        random != 0x75432777)
    {
        throw Error("ghoulybox decoding or pool growth differs from retail");
    }
    // Individual count nodes must remain observable before the next node overwrites them.
    stream.clear();
    at = node(3, 0xC);
    word(at + 8, 9);
    node(0, 8);
    decode_ghoulybox(*record, stream, random);
    if (record->remaining_c != 9)
    {
        throw Error("fixed spawn count was not decoded");
    }
    stream.clear();
    node(5, 8);
    node(0, 8);
    decode_ghoulybox(*record, stream, random);
    if (record->remaining_c != -1 || random != 0x75432777)
    {
        throw Error("unlimited spawn count changed the random state");
    }
    release_ghouly_spawn_record(*record);
    AidStore aids;
    aids.open(game_directory);
    const Bundle& bundle = aids.load_bundle_rel("aid_script/ghoulies_chapter1_scene1_2playcam.bnl");
    std::vector<AnimationBundleRecord> resources;
    for (const BundleEntry& entry : bundle.entries)
    {
        if (entry.type == ResourceType::GhoulyBox)
        {
            AnimationBundleRecord resource;
            resource.name = entry.name;
            resource.cpu = entry.cpu;
            resource.gpu = entry.gpu;
            resources.push_back(resource);
        }
    }
    std::array<AnimationBundlePool, 1> pools{AnimationBundlePool{resources, true}};
    AnimationResourceRegistry registry;
    register_animation_resource_provider(registry,
        [&](u32, const AnimationResourceRecord& resource, const void*& resolved)
        {
            return find_bundle_resource_provider(pools, resource.name.c_str(), resolved);
        },
        [&](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            return load_bundle_resource_provider(resolved, loaded);
        },
        [&](u32, const char* name, const AnimationLoadedResource&)
        {
            unload_bundle_resource_provider(pools, name);
        }, 3, nullptr);
    ResourceErrorState error{};
    error.suppressed = 1; // Retail's existing suppression flag prevents the fatal error display loop.
    if (load_ghoulybox("plain_name", registry, error, random) != nullptr || !registry.records.empty())
    {
        throw Error("non-AID name entered the resource registry");
    }
    const u32 random_before_missing = random;
    if (load_ghoulybox("aid_ghoulybox_missing", registry, error, random) != nullptr ||
        random != random_before_missing)
    {
        throw Error("missing ghoulybox allocated or decoded a spawn record");
    }
    size_t decoded = 0;
    for (const BundleEntry& entry : bundle.entries)
    {
        if (entry.type == ResourceType::GhoulyBox)
        {
            record = load_ghoulybox(entry.name.c_str(), registry, error, random);
            if (record == nullptr)
            {
                throw Error("retail ghoulybox resource acquisition failed");
            }
            const auto* registered = find_animation_resource(registry.records, entry.name.c_str());
            if (registered == nullptr || registered->references != 0)
            {
                throw Error("ghoulybox wrapper did not release the acquired resource");
            }
            const size_t count = (record->pool_8->cursor - record->pool_8->begin) / 0x288;
            if (count == 0)
            {
                throw Error("retail scene ghoulybox produced no definitions");
            }
            std::cout << entry.name << ": " << count << " definitions, count=" << record->remaining_c << "\n";
            const i32 expected_count = entry.name.find("scene1_4playcamimps") != std::string::npos ? 10 : 1;
            if (count != 1 || record->remaining_c != expected_count)
            {
                throw Error("retail ghoulybox count differs from its node data");
            }
            auto* repeated = load_ghoulybox(entry.name.c_str(), registry, error, random);
            registered = find_animation_resource(registry.records, entry.name.c_str());
            if (repeated == nullptr || repeated == record || registered->references != 0 ||
                repeated->remaining_c != record->remaining_c)
            {
                throw Error("repeated ghoulybox request lost data or leaked a resource reference");
            }
            const auto& first_row = *reinterpret_cast<const GhoulySpawnDefinition*>(record->pool_8->begin);
            const auto& repeated_row = *reinterpret_cast<const GhoulySpawnDefinition*>(repeated->pool_8->begin);
            if (std::strcmp(first_row.name, repeated_row.name) ||
                std::strcmp(first_row.variant, repeated_row.variant) ||
                std::strcmp(first_row.alternate, repeated_row.alternate) ||
                std::strcmp(first_row.first_override, repeated_row.first_override) ||
                first_row.weight != repeated_row.weight || first_row.flags != repeated_row.flags)
            {
                throw Error("repeated resource load changed the decoded definition");
            }
            release_ghouly_spawn_record(*repeated);
            release_ghouly_spawn_record(*record);
            ++decoded;
        }
    }
    if (decoded != 3)
    {
        throw Error("expected three retail scene ghoulybox resources");
    }
}

void check_actor_spawn_eligibility()
{
    using namespace gbtg;
    Actor actor;
    Actor parent_actor;
    ActorLink link;
    ActorLink parent; // The parent actor's node (+0x918).
    ActorLink root; // DAT_00511B10: its first dword is null.
    actor.spawn_point_count = 2;
    actor.mask_flag = 1;
    if (!actor_can_supply_ghoulybox(actor))
    {
        throw Error("unbound eligible actor rejected as a ghoulybox source");
    }
    actor.link = &link;
    if (!actor_can_supply_ghoulybox(actor))
    {
        throw Error("empty actor link rejected as a ghoulybox source");
    }
    // 0x00146150 looks one node up: an actor under the root (every activated actor) stays eligible, one bound
    // under another actor's node does not.
    link.head = &root;
    if (!actor_can_supply_ghoulybox(actor))
    {
        throw Error("actor under the world bind root rejected as a ghoulybox source");
    }
    parent.head = &root;
    link.head = &parent;
    link.owner = &parent_actor;
    if (actor_can_supply_ghoulybox(actor))
    {
        throw Error("bound actor accepted as a ghoulybox source");
    }
    link.head = nullptr;
    link.owner = nullptr;
    actor.mask_flag = 0x100;
    if (actor_can_supply_ghoulybox(actor))
    {
        throw Error("eligibility checked more than the retail flag byte");
    }
    actor.mask_flag = 0x101;
    actor.spawn_blocked = -1;
    if (actor_can_supply_ghoulybox(actor))
    {
        throw Error("blocked actor accepted as a ghoulybox source");
    }
    actor.spawn_blocked = 0;
    actor.spawn_point_count = 0;
    if (actor_can_supply_ghoulybox(actor))
    {
        throw Error("actor without spawn data accepted as a ghoulybox source");
    }
    actor.spawn_point_count = 2;
    if (!actor_can_supply_ghoulybox(actor))
    {
        throw Error("eligible actor remained blocked after restoring spawn data");
    }
}

void check_actor_spawn_filters()
{
    using namespace gbtg;
    const auto saved_world = entity_detail::g_world;
    entity_detail::g_world.segment = nullptr;
    entity_detail::g_world.segment_prev = nullptr;
    entity_detail::SegmentProbe first_probe;
    entity_detail::SegmentProbe second_probe;
    Actor segment_actor;
    register_actor_segment_probe(segment_actor, first_probe);
    if (entity_detail::g_world.segment != nullptr)
    {
        throw Error("zero actor model flags registered a segment probe");
    }
    segment_actor.model_flags = 1;
    register_actor_segment_probe(segment_actor, first_probe);
    register_actor_segment_probe(segment_actor, second_probe);
    if (entity_detail::g_world.segment != &first_probe || first_probe.next != &second_probe ||
        second_probe.next != nullptr || second_probe.prev != &first_probe ||
        first_probe.prev != reinterpret_cast<entity_detail::SegmentProbe*>(&entity_detail::g_world.segment_prev))
    {
        throw Error("actor segment registration lost the retail tail or sentinel links");
    }
    entity_detail::g_world = saved_world;
    Actor candidate;
    Actor requester;
    requester.class_key = 7;
    std::array<ActorAcceptedClass, 2> accepted{};
    accepted[0].key = 3;
    accepted[1].key = 7;
    candidate.accepted_classes = accepted.data();
    candidate.accepted_class_count = 2;
    if (!actor_accepts_spawn_class(candidate, requester))
    {
        throw Error("spawn class filter skipped a later accepted key");
    }
    candidate.accepted_class_count = 1;
    if (actor_accepts_spawn_class(candidate, requester))
    {
        throw Error("spawn class filter walked past its signed count");
    }
    candidate.accepted_class_count = -1;
    if (actor_accepts_spawn_class(candidate, requester))
    {
        throw Error("negative accepted-class count was treated as unsigned");
    }
    candidate.accepted_class_count = 2;
    Actor owner;
    candidate.spawn_owner = &owner;
    for (u32 state = 0; state < 4; ++state)
    {
        candidate.mask_flag = i32(state << 24);
        if (actor_spawn_owner_unavailable(candidate) != (state == 1 || state == 2))
        {
            throw Error("spawn owner state predicate differs from retail");
        }
    }
    candidate.mask_flag = 0x02000000;
    owner.mask_mode = 2;
    if (actor_spawn_owner_unavailable(candidate))
    {
        throw Error("mode-two owner was treated as unavailable");
    }
    owner.mask_mode = 0;
    owner.mask_flag = 0x100;
    if (actor_spawn_owner_unavailable(candidate))
    {
        throw Error("spawn owner predicate ignored the full owner flag dword");
    }
    candidate.spawn_owner = nullptr;
    if (!actor_spawn_owner_unavailable(candidate))
    {
        throw Error("state-two actor without an owner passed availability");
    }
    std::array<u8, 0x40> cpu{};
    u32 metadata = 0x20;
    u32 flag = 0x38;
    std::memcpy(cpu.data() + 0x10, &metadata, 4);
    std::memcpy(cpu.data() + 0x30, &flag, 4);
    ActorModel model;
    model.cpu = cpu;
    candidate.model = &model;
    if (actor_has_spawn_model(candidate))
    {
        throw Error("zero model flag passed spawn filtering");
    }
    cpu[flag] = 0xFF;
    if (!actor_has_spawn_model(candidate))
    {
        throw Error("spawn model flag was interpreted as a signed byte");
    }
    candidate.spawn_model_blocked = 1;
    if (actor_has_spawn_model(candidate))
    {
        throw Error("blocked model passed spawn filtering");
    }
    candidate.spawn_model_blocked = 0;
    candidate.mask_flag = 1;
    candidate.spawn_point_count = 1;
    if (!actor_matches_spawn_filter(candidate, &requester, 0x77))
    {
        throw Error("eligible actor failed the combined retail spawn filter");
    }
    ActorLink link;
    int bound = 0;
    link.head = &bound;
    candidate.link = &link;
    if (actor_matches_spawn_filter(candidate, &requester, 2) ||
        !actor_matches_spawn_filter(candidate, nullptr, 0) ||
        !actor_matches_spawn_filter(candidate, nullptr, 1))
    {
        throw Error("spawn filter changed optional requester or flag gates");
    }
    candidate.link = nullptr;
    candidate.mask_flag = 0;
    if (actor_matches_spawn_filter(candidate, nullptr, 4) ||
        actor_matches_spawn_filter(candidate, nullptr, 0x10))
    {
        throw Error("spawn filter ignored ready or eligibility bits");
    }
    candidate.mask_flag = 0x01000001;
    if (actor_matches_spawn_filter(candidate, nullptr, 0x20))
    {
        throw Error("spawn filter ignored unavailable-owner bit");
    }
    candidate.model = nullptr;
    if (actor_matches_spawn_filter(candidate, nullptr, 0x40))
    {
        throw Error("spawn filter accepted an absent model");
    }
    requester.class_key = 9;
    if (actor_matches_spawn_filter(candidate, &requester, 1))
    {
        throw Error("spawn filter accepted a missing requester class");
    }
}

void check_actor_navigation()
{
    using namespace gbtg;
    using namespace gbtg::entity_detail;
    NavGrid grid;
    grid.width = 3;
    grid.depth = 3;
    grid.cell = 10;
    grid.cells.resize(9);
    for (size_t index = 0; index < grid.cells.size(); ++index)
    {
        grid.cells[index].x = i16((index % 3) * 10);
        grid.cells[index].bottom = i16(index);
        grid.cells[index].z = i16((index / 3) * 10);
    }
    const std::array<i32, 8> expected{3, 5, 7, 1, 6, 8, 0, 2};
    for (u32 direction = 0; direction < 8; ++direction)
    {
        grid.cells[4].connections = u8(1u << direction);
        for (i32 source = 0; source < 9; ++source)
        {
            if (navigation_cells_connected(grid, 4, source) != (source == expected[direction]))
            {
                throw Error("navigation connection bit order differs from the retail table");
            }
        }
        if (navigation_cell_is_isolated(grid, 4, true))
        {
            throw Error("connected open navigation neighbor was treated as isolated");
        }
    }
    grid.cells[4].connections = 0;
    if (navigation_cells_connected(grid, 4, 3) || !navigation_cell_is_isolated(grid, 4, true))
    {
        throw Error("empty navigation connection mask passed a query");
    }
    grid.cells[0].connections = 1;
    if (navigation_cells_connected(grid, 0, -1) || !navigation_cell_is_isolated(grid, 0, true))
    {
        throw Error("navigation neighbor query crossed the grid boundary");
    }
    grid.cells[0].connections = 0xFF;
    if (!navigation_cells_connected(grid, 0, -1))
    {
        throw Error("retail FF connection shortcut was removed");
    }
    grid.cells[4].connections = 1;
    grid.cells[4].region = 0x2007;
    grid.cells[3].region = 0x1007;
    grid.cells[3].flags_7 = 2;
    if (navigation_cell_is_isolated(grid, 4, true) || !navigation_cell_is_isolated(grid, 4, false))
    {
        throw Error("matching navigation region did not override blocked-neighbor flags");
    }
    grid.cells[3].flags_7 = 0;
    grid.cells[3].region = 0x1008;
    if (!navigation_cell_is_isolated(grid, 4, true))
    {
        throw Error("different closed navigation region passed isolation query");
    }
    grid.cells[3].region = 8;
    if (navigation_cell_is_isolated(grid, 4, true))
    {
        throw Error("open different navigation region was rejected");
    }
    grid.cells[3].region = 0x1000;
    if (navigation_cell_is_isolated(grid, 4, true))
    {
        throw Error("zero navigation region ID was treated as closed");
    }
    auto* saved_grid = g_nav_grids[0];
    const i32 saved_x = g_nav_origin_x;
    const i32 saved_z = g_nav_origin_z;
    g_nav_grids[0] = &grid;
    g_nav_origin_x = 5;
    g_nav_origin_z = -5;
    i32 x = 99;
    i32 z = 88;
    if (!navigation_cell_coordinates(0, Vec3{24.9f, 0.f, 15.9f}, x, z) || x != 1 || z != 2 ||
        !navigation_cell_coordinates(0, Vec3{4.9f, 0.f, -5.9f}, x, z) || x != 0 || z != 0)
    {
        throw Error("navigation coordinates changed truncation or origin arithmetic");
    }
    x = 99;
    z = 88;
    if (navigation_cell_coordinates(0, Vec3{35.f, 0.f, -5.f}, x, z) || x != 99 || z != 88)
    {
        throw Error("out-of-grid coordinates wrote the output pair");
    }
    g_nav_grids[0] = nullptr;
    if (navigation_cell_coordinates(0, Vec3{}, x, z))
    {
        throw Error("missing navigation grid produced coordinates");
    }
    g_nav_grids[0] = saved_grid;
    g_nav_origin_x = saved_x;
    g_nav_origin_z = saved_z;
}

void check_spawn_effect_allocation()
{
    using namespace gbtg;
    {
        std::array<u8, 12 * 0x14> events{};
        for (size_t index = 0; index < 12; ++index)
        {
            events[index * 0x14 + 0xC] = 4;
            events[index * 0x14 + 0x10] = 1;
        }
        events[0x10] = 0;
        std::unordered_map<const u8*, PhysicalShapeEventList> lists;
        GhoulySpawnPool* pool = nullptr;
        PhysicalShapeEventContext context{0, 0, 0,
            [&](const u8* event, void*)
            {
                return collect_background_shape_event(event, pool);
            }, nullptr, lists};
        if (visit_physical_shape_event_list({12, events.data()}, context) != 1 ||
            pool == nullptr || (pool->cursor - pool->begin) / pool->stride != 11)
        {
            throw Error("background shape collector lost filtered events or pool growth");
        }
        auto* stored = reinterpret_cast<BackgroundShapeEvent*>(pool->begin);
        if (stored[0].source != events.data() + 0x14 || stored[10].source != events.data() + 11 * 0x14 ||
            stored[10].flags != 4)
        {
            throw Error("background shape collector lost source pointers during growth");
        }
        engine_heap_free(pool);
        int calls = 0;
        context.callback = [&](const u8*, void*)
        {
            ++calls;
            return 0u;
        };
        context.category = 1;
        if (visit_physical_shape_event_list({12, events.data()}, context) != 0 || calls != 1)
        {
            throw Error("background shape event walk did not honor filtering and callback stop");
        }
    }
    const auto saved_effects = g_actor_camera_effects;
    g_actor_camera_effects = {};
    auto* first = allocate_actor_camera_effect(3);
    auto* second = create_positioned_camera_effect(4, 5.f, 6.f, Vec3{7.f, 8.f, 9.f});
    Vec3 position;
    std::memcpy(&position, second->payload, sizeof(position));
    if (g_actor_camera_effects.next != first || first->next != second ||
        second->previous != first || first->payload[0x79] != 3 ||
        second->payload[0x7A] != 1 || position.z != 9.f)
    {
        throw Error("camera effect allocation lost its list order or positioned fields");
    }
    engine_heap_free(first);
    engine_heap_free(second);
    g_actor_camera_effects = saved_effects;

    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    auto* effect = create_actor_effect_node(nullptr, 3, Vec3{2.f, 3.f, 4.f}, Vec3{}, registry, error);
    auto get = [&]<class T>(size_t offset)
    {
        T value;
        std::memcpy(&value, effect->bytes.data() + offset, sizeof(T));
        return value;
    };
    if (get.operator()<float>(8) != -3.f || get.operator()<float>(0xB8) != 1.f ||
        get.operator()<u16>(0x3EA) != 1 || get.operator()<u16>(0x53A) != 2 ||
        get.operator()<u16>(0x68A) != 0xFFFF || get.operator()<float>(0x24) != 4.f)
    {
        throw Error("actor effect constructor lost its defaults or particle free chain");
    }
    const i16 active = -2;
    const u16 count = 1;
    std::memcpy(effect->bytes.data(), &count, 2);
    std::memcpy(effect->bytes.data() + 0x53A, &active, 2);
    recycle_actor_effect_particles(*effect);
    if (get.operator()<u16>(0) != 0 || get.operator()<u16>(6) != 1 || get.operator()<u16>(0x53A) != 0)
    {
        throw Error("actor effect recycling lost its active count or free head");
    }
    const auto saved_slot = g_actor_effect_nodes[0];
    const i32 saved_last = g_last_actor_effect;
    g_actor_effect_nodes[0] = effect;
    g_last_actor_effect = 0;
    if (!retain_last_actor_effect(12.f) || g_last_actor_effect != -1 ||
        (effect->bytes[0x18] & 8) == 0 || get.operator()<float>(0x10) != 12.f)
    {
        throw Error("effect retention lost its duration or consumed slot");
    }
    g_last_actor_effect = 0xFFFF;
    if (retain_last_actor_effect(0.f) || g_last_actor_effect != -1)
    {
        throw Error("effect retention did not clear the signed missing-slot value");
    }
    g_actor_effect_nodes[0] = saved_slot;
    g_last_actor_effect = saved_last;
    effect->~ActorEffectNode();
    engine_heap_free(effect);

    ActorAnimationState animation;
    std::array<u8, 12> event{};
    const float begin = 0.25f;
    const float end = 0.75f;
    std::memcpy(event.data() + 4, &begin, 4);
    std::memcpy(event.data() + 8, &end, 4);
    animation.controller = make_animation_controller(1);
    animation.controller->slots[0].player = make_mina_player();
    animation.controller->slots[0].player->duration = 10.f;
    if (actor_effect_event_duration(0, event, animation) != -2.f ||
        actor_effect_event_duration(1, event, animation) != 5.f)
    {
        throw Error("animation effect duration lost its register return");
    }
}

void check_spawn_effect_stream()
{
    using namespace gbtg;
    std::vector<u8> script(0x110 + 0x18 + 0x10 + 8);
    auto put = [&](size_t offset, auto value)
    {
        std::memcpy(script.data() + offset, &value, sizeof(value));
    };
    put(4, u32{0x1F51});
    put(0xA, u16{3});
    std::strcpy(reinterpret_cast<char*>(script.data() + 0xC), "plain");
    put(0x10C, u32{1});
    put(0x114, u32{0x1F43});
    put(0x118, u32{1});
    put(0x11C, 8.f);
    put(0x120, 300.f);
    put(0x124, 20.f);
    put(0x12C, u32{0x1F52});
    put(0x130, 2.f);
    put(0x13C, u32{2});
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    RuntimeModelResourceViews views;
    RuntimeModelDeferredPool deferred{};
    u32 fence = 0;
    RuntimeShaderRegistrationState shaders;
    RuntimeShaderAnimationScratch scratch;
    std::function<void(u8*)> free;
    ActorEffectModelContext models{registry, error, views, deferred, fence, {}, shaders, scratch, free};
    RuntimeActorSoundQueues sounds{};
    u32 random = 7;
    Vec3 origin{};
    Vec3 default_euler{};
    ActorEffectStreamContext context{models, sounds, random, origin, default_euler};
    const auto saved_nodes = g_actor_effect_nodes;
    const i32 saved_last = g_last_actor_effect;
    g_actor_effect_nodes = {};
    RuntimeActorCreationOptions options;
    options.flags = 0x45;
    options.position = Vec3{4.f, 5.f, 6.f};
    options.scale = 2.f;
    const u8* cursor = script.data();
    if (run_actor_effect_stream(cursor, &options, context) != 1 || g_last_actor_effect != 0 ||
        cursor != script.data() + 0x138 || random == 7)
    {
        throw Error("effect stream did not construct, advance or stop at the retail terminator");
    }
    ActorEffectNode* effect = g_actor_effect_nodes[0];
    auto get = [&]<class T>(size_t offset)
    {
        T value;
        std::memcpy(&value, effect->bytes.data() + offset, sizeof(T));
        return value;
    };
    constexpr float radians = std::bit_cast<float>(0x3C8EFA35u);
    if (get.operator()<float>(0x1C) != 4.f || get.operator()<float>(0xB8) != 2.f ||
        get.operator()<float>(0xE0) != 4.f || get.operator()<float>(0xFC) != 380.f * radians ||
        get.operator()<float>(8) != 0.5f || (effect->bytes[0x18] & 0x84) != 0x84)
    {
        throw Error("effect stream lost creation options, wrap angles or particle interval");
    }
    std::array<u8, 8> rejected{};
    const u32 type = 0x7D1;
    std::memcpy(rejected.data() + 4, &type, 4);
    cursor = rejected.data();
    if (run_actor_effect_stream(cursor, nullptr, context) != 0 || cursor != rejected.data())
    {
        throw Error("effect dispatcher invented success for the retail rejection handler");
    }
    Actor actor;
    ActorAttachmentList attachments;
    ActorAttachmentNode pending{};
    attachments.pending = &pending; // Actor +0x1B4, where 0x0003D1B0 links new nodes.
    auto* attachment = create_actor_joint_attachment(attachments, "aid_effect_test", 0, -1, &options);
    Vec3 position;
    std::memcpy(&position, attachment->bytes.data() + 0xA8, sizeof(position));
    if (attachment->bytes[0xA4] != 1 || attachment->bytes[0xA5] != 1 || position.z != 6.f)
    {
        throw Error("attachment constructor lost its retained pose overrides");
    }
    engine_heap_free(attachment);
    AnimationBundleRecord attachment_script;
    attachment_script.name = "aid_effect_attachment_test";
    attachment_script.cpu = script;
    std::array<AnimationBundleRecord, 1> attachment_records{attachment_script};
    std::array<AnimationBundlePool, 1> attachment_pools{AnimationBundlePool{attachment_records, true}};
    register_animation_resource_provider(registry,
        [&](u32, const AnimationResourceRecord& entry, const void*& resolved)
        {
            return find_bundle_resource_provider(attachment_pools, entry.name.c_str(), resolved);
        },
        [&](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            return load_bundle_resource_provider(resolved, loaded);
        },
        [&](u32, const char* name, const AnimationLoadedResource&)
        {
            unload_bundle_resource_provider(attachment_pools, name);
        }, 3, nullptr);
    ActorAttachmentList active_attachments{};
    ActorAttachmentList pending_attachments{};
    // Requests take the +0x1AC list and link into its pending view (+0x1B4).
    active_attachments.pending = &pending_attachments.sentinel;
    auto* retained = create_actor_joint_attachment(active_attachments,
        attachment_script.name.c_str(), 7, -1, &options);
    create_actor_joint_attachment(active_attachments, "plain_rejected", 0, -1, nullptr);
    create_actor_joint_attachment(active_attachments,
        attachment_script.name.c_str(), 8, -1, &options);
    actor.position = Vec3{10.f, 20.f, 30.f};
    actor.euler = Vec3{0.1f, 0.2f, 0.3f};
    update_actor_attachment_effects(actor, active_attachments, pending_attachments.sentinel, context);
    const i32 retained_slot = std::bit_cast<i32>(read_u32(retained->bytes, 8));
    ActorEffectNode* retained_effect = g_actor_effect_nodes[retained_slot];
    Vec3 followed_position;
    Vec3 followed_euler;
    std::memcpy(&followed_position, retained_effect->bytes.data() + 0x1C, sizeof(followed_position));
    std::memcpy(&followed_euler, retained_effect->bytes.data() + 0x50, sizeof(followed_euler));
    if (pending_attachments.sentinel.next != nullptr || active_attachments.sentinel.next != retained ||
        retained->previous != &active_attachments.sentinel || retained->next != nullptr ||
        followed_position.x != 10.f || followed_position.y != 20.f || followed_position.z != 30.f ||
        followed_euler.x != 0.1f || followed_euler.y != 0.2f || followed_euler.z != 0.3f ||
        retained_slot != 1 || g_actor_effect_nodes[2] == nullptr ||
        (g_actor_effect_nodes[2]->bytes[0x18] & 8) == 0 || registry.records[0].references != 0)
    {
        throw Error("attachment update lost pending unlinking, transient ownership or active pose following");
    }
    retained->bytes[0x20] = 0;
    actor.position = Vec3{40.f, 50.f, 60.f};
    update_actor_attachment_effects(actor, active_attachments, pending_attachments.sentinel, context);
    if (read_f32(retained_effect->bytes, 0x1C) != 10.f ||
        read_f32(retained_effect->bytes, 0x50) != 0.1f || g_actor_effect_nodes[3] != nullptr)
    {
        throw Error("attachment update changed disabled pose fields or restarted an active effect");
    }
    engine_heap_free(retained);
    for (i32 slot : {1, 2})
    {
        g_actor_effect_nodes[slot]->~ActorEffectNode();
        engine_heap_free(g_actor_effect_nodes[slot]);
    }
    active_attachments.sentinel.next = nullptr;
    ButtonBatScreen frame_bat{};
    ActorAnimationState frame_animation{};
    ModelAnimationState frame_model{};
    entity_detail::ListHead frame_anchors{};
    ModelNodeFlagCache frame_cache{};
    const AnimationMatrix frame_reset{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const RuntimeShaderMatrixBoundaries frame_shader_matrices{};
    static std::vector<u32> frame_notes;
    frame_notes.clear();
    ActorFrameAttachment second_update{nullptr, [](ActorFrameAttachment&, Actor&)
    {
        frame_notes.push_back(2);
    }};
    ActorFrameAttachment first_update{&second_update, [](ActorFrameAttachment& self, Actor&)
    {
        frame_notes.push_back(1);
        self.next = nullptr;
    }};
    ActorFrameState frame{frame_bat.actor, frame_animation, frame_model, active_attachments,
        pending_attachments.sentinel, &first_update, frame_anchors, 0,
        Vec3{0.f, 10.f, -10.f}, Vec3{10.f, 20.f, 0.f}, 1.f, 7};
    ActorFrameContext frame_context{context, nullptr, frame_cache, frame_reset,
        frame_shader_matrices, 0.f, 0.1f, 2.f, 1, 0, 1};
    frame_bat.actor.animation_rate = 2.f;
    frame_bat.actor.animation_pause = 0.05f;
    update_actor_frame(frame, frame_context);
    if (frame_notes != std::vector<u32>{1, 2} || frame_bat.actor.animation_pause != 0.f ||
        std::abs(frame.elapsed - 1.4f) > 0.00001f || frame.ticks != 8 ||
        frame.eased.x != 1.f || frame.eased.y != 11.f || frame.eased.z != -9.f)
    {
        throw Error("base actor update lost saved callback links, easing or post-decay clock advancement");
    }
    frame.updates = nullptr;
    frame_context.freeze = 1.f;
    frame_bat.actor.animation_pause = 0.05f;
    update_actor_frame(frame, frame_context);
    if (frame_bat.actor.animation_pause != 0.05f || frame.elapsed != 1.4f || frame.ticks != 9 ||
        std::abs(frame.eased.x - 1.9f) > 0.00001f)
    {
        throw Error("base actor freeze changed its clock or suppressed retail position easing");
    }
    frame_context.freeze = 0.f;
    frame_context.block_decay = 1;
    frame_context.ease = 0;
    update_actor_frame(frame, frame_context);
    if (frame_bat.actor.animation_pause != 0.05f || frame.elapsed != 1.4f || frame.ticks != 10 ||
        frame.eased.x != 10.f || frame.eased.y != 20.f || frame.eased.z != 0.f)
    {
        throw Error("base actor blocked decay or direct target copy differed from retail");
    }
    const auto saved_display = entity_detail::g_display_view;
    entity_detail::DisplayView bat_display{};
    bat_display.position = Vec3{2.f, 3.f, 4.f};
    bat_display.forward = Vec3{0.f, 0.f, -1.f};
    entity_detail::g_display_view = &bat_display;
    frame_bat.actor.animation_pause = 0.f;
    frame_bat.state = 1;
    frame_bat.idle_timer = 0.2f;
    update_button_bat(frame_bat, frame, frame_context);
    if (frame_bat.state != 3 || frame_bat.idle_timer != 0.f ||
        frame_bat.actor.position.x != 2.f || frame_bat.actor.position.y != 3.f ||
        frame_bat.actor.position.z != -1.f)
    {
        throw Error("button bat update lost its display-relative position or timed hop transition");
    }
    frame_bat.target = ButtonBatPoint{21.f, 34.f};
    update_button_bat(frame_bat, frame, frame_context);
    if (frame_bat.state != 1 || frame_bat.idle_timer != 4.5f ||
        frame_bat.current.x != 21.f || frame_bat.current.y != 34.f ||
        frame_bat.from.x != 21.f || frame_bat.from.y != 34.f)
    {
        throw Error("button bat animation completion did not return to its retail idle pose");
    }
    const std::unordered_map<const u8*, LocalTextResourceView> frame_text_views;
    LocalTextResourceContext frame_text{registry, error, frame_text_views};
    const RuntimeTextMetrics frame_metrics{};
    i32 frame_text_id = 1;
    RuntimeDialogueWidgetContext frame_widget_context{frame_text, frame_metrics, {}, nullptr,
        640.f, frame_text_id, random};
    RuntimeDialogueInputState frame_input{};
    AudioCoreSources frame_audio_sources{};
    AudioCoreCues frame_audio_cues{frame_audio_sources};
    AudioEventVoices frame_voices{frame_audio_cues, {}, {}, {}, 0, origin, default_euler};
    const std::unordered_map<Actor*, DialogueWidgetBatView> frame_bats
    {
        {&frame_bat.actor, DialogueWidgetBatView{frame_bat, frame, frame_context}}
    };
    const std::unordered_map<u32, std::function<void()>> frame_widget_callbacks;
    const std::function<DialogueWidgetBatView(Actor*)> frame_bat_views = [&frame_bats](Actor* bat)
    {
        return frame_bats.at(bat);
    };
    DialogueWidgetUpdateContext bat_widget_update{frame_widget_context, frame_input, frame_voices,
        frame_bat_views, frame_widget_callbacks, 0.f, 0.1f, 2.f};
    RuntimeDialogueWidget bat_widget{};
    bat_widget.bat = &frame_bat.actor;
    const u32 bat_ticks = frame.ticks;
    if (update_dialogue_widget(bat_widget, nullptr, bat_widget_update) != 0 ||
        frame.ticks != bat_ticks + 1 || std::abs(frame_bat.idle_timer - 4.1f) > 0.00001f)
    {
        throw Error("widget update did not dispatch the registered button bat frame routine");
    }
    entity_detail::g_display_view = saved_display;
    std::vector<u8> records(0xD0);
    const u16 record_size = 0xC8;
    const u16 kind = 10;
    std::memcpy(records.data(), &record_size, 2);
    std::memcpy(records.data() + 4, &kind, 2);
    std::strcpy(reinterpret_cast<char*>(records.data() + 8), "avatarCalloutId_Accept");
    std::strcpy(reinterpret_cast<char*>(records.data() + 0x48), "XACT_SOUNDBANK_TEST");
    const u8* terminal = records.data() + 0xC8;
    std::unordered_map<const u8*, const u8*> linked{{terminal, nullptr}};
    ObjectFactoryRegistry factory{};
    std::vector<FactoryObject> objects;
    ActorAttachmentEventContext events{actor, attachments, context, factory, {}, objects, 7, linked};
    const auto saved_requests = g_sound_resource_requests;
    g_sound_resource_requests = {};
    if (dispatch_actor_attachment_records(records.data(), 1, nullptr, nullptr, events, 0) != 1 ||
        std::strcmp(g_sound_resource_requests.names[0].data(), "XACT_SOUNDBANK_TEST") != 0 ||
        g_sound_resource_requests.reasons[0] != 0x12 ||
        dispatch_actor_attachment_records(records.data(), 999, nullptr, nullptr, events, 0) != 0)
    {
        throw Error("attachment callout dispatch lost the retail name ID or miss traversal");
    }
    g_sound_resource_requests = {};
    RuntimeActorResourceReference callouts;
    events.actor_address = 0xFFFFFF74u;
    dispatch_actor_resource_callout(callouts, 1, nullptr, events);
    if (g_sound_resource_requests.states[0] != 0)
    {
        throw Error("actor resource callout ignored its retail wrapped-address guard");
    }
    events.actor_address = 7;
    callouts.resource = records;
    dispatch_actor_resource_callout(callouts, 1, nullptr, events);
    if (std::strcmp(g_sound_resource_requests.names[0].data(), "XACT_SOUNDBANK_TEST") != 0)
    {
        throw Error("actor resource callout did not use its default event dispatcher");
    }
    g_sound_resource_requests = saved_requests;
    effect->~ActorEffectNode();
    engine_heap_free(effect);
    g_actor_effect_nodes = saved_nodes;
    g_last_actor_effect = saved_last;
}

void check_spawn_sound_pose()
{
    using namespace gbtg;
    Vec3 output;
    rotate_euler_vector(Vec3{}, Vec3{1.f, 2.f, 3.f}, output);
    if (output.x != 1.f || output.y != 2.f || output.z != 3.f)
    {
        throw Error("sound euler transform lost the identity vector");
    }
    Actor actor;
    actor.position = Vec3{4.f, 5.f, 6.f};
    actor.sound_slots[0].bone = 0xFFFF;
    RuntimeActorSound sound;
    sound.request_id = 0;
    sound.position = Vec3{11.f, 12.f, 13.f};
    sound.direction = Vec3{14.f, 15.f, 16.f};
    update_actor_sound_pose(actor, sound);
    if (sound.position.x != 11.f || sound.direction.x != 14.f)
    {
        throw Error("disabled sound pose update changed retained outputs");
    }
    actor.sound_slots[0].category = 1;
    actor.sound_slots[0].mode = 1;
    update_actor_sound_pose(actor, sound);
    if (sound.position.x != 4.f || sound.position.y != 5.f || sound.position.z != 6.f ||
        sound.direction.x != 0.f || sound.direction.y != 0.f || sound.direction.z != 1.f)
    {
        throw Error("sound pose update lost the actor position or forward vector");
    }
}

void check_spawn_bind_tree()
{
    using namespace gbtg;
    Actor root_actor;
    Actor parent_actor;
    Actor child_actor;
    Actor grandchild_actor;
    ActorLink root;
    ActorLink parent;
    ActorLink child;
    ActorLink grandchild;
    root.actor = &root_actor;
    parent.actor = &parent_actor;
    child.actor = &child_actor;
    grandchild.actor = &grandchild_actor;
    parent_actor.link = &parent;
    child_actor.link = &child;
    grandchild_actor.link = &grandchild;
    append_actor_bind_child(root, parent);
    append_actor_bind_child(parent, child);
    append_actor_bind_child(child, grandchild);
    unlink_actor_bind_node(child);
    if (child.head != nullptr || child.next != nullptr || child.child != nullptr ||
        parent.child != &grandchild || grandchild.head != &parent || grandchild.owner != &parent_actor)
    {
        throw Error("bind unlink did not promote nested nodes into the parent list");
    }
    append_actor_bind_child(parent, child);
    if (grandchild.next != &child || child.owner != &parent_actor)
    {
        throw Error("bind append did not preserve the parent or append after siblings");
    }
    child_actor.bind = static_cast<ActorBind*>(engine_heap_allocate(sizeof(ActorBind)));
    *child_actor.bind = ActorBind{};
    child_actor.bind->parent = &parent_actor;
    child_actor.scale_x = child_actor.radius_y = child_actor.radius_xz = 1.f;
    child_actor.scale_200 = 1.f;
    parent_actor.scale_200 = 1.f;
    child_actor.position = Vec3{3.f, 4.f, 5.f};
    if (actor_bound_parent(child_actor) != &parent_actor)
    {
        throw Error("explicit bind parent lookup changed");
    }
    detach_actor_bind(child_actor);
    if (child_actor.bind != nullptr || child_actor.position.x != 3.f || child_actor.radius_xz != 1.f)
    {
        throw Error("bind detach lost world pose or retained the heap allocation");
    }
    child_actor.bind = static_cast<ActorBind*>(engine_heap_allocate(sizeof(ActorBind)));
    *child_actor.bind = ActorBind{};
    child.head = &root;
    child.owner = &root_actor;
    if (actor_bound_parent(child_actor) != nullptr || child_actor.bind != nullptr)
    {
        throw Error("root bind lookup did not release an inactive bind");
    }
    const float matrix[12] = {3.f, 0.f, 0.f, 0.f, 4.f, 0.f, 0.f, 0.f, 0.f, 0.f, 2.f, 0.f};
    Vec3 scales;
    matrix_axis_scales(matrix, scales);
    // Retail 0x00152B30 returns 5 exactly but 0x3FFFFFF8 for the root of 4 (Unicorn run of 0x00103A80).
    if (scales.x != 5.f || scales.y != 0.f || std::bit_cast<u32>(scales.z) != 0x3FFFFFF8u)
    {
        throw Error("model matrix scale extraction changed column lengths");
    }
}

void check_spawn_live_attachment_release()
{
    using namespace gbtg;
    static i32 calls;
    calls = 0;
    const auto saved_broadcast = g_actor_broadcast;
    g_actor_broadcast = [](const i32* raw) // 0x00122C70 over DAT_00511AA4.
    {
        const auto& message = *reinterpret_cast<const ActorAttachmentReleaseMessage*>(raw);
        if (message.id != 0x8D || message.record == nullptr || message.value != 0)
        {
            throw Error("attachment release broadcast lost its record pointer");
        }
        ++calls;
    };
    const auto saved_slots = g_actor_attachment_slots;
    g_actor_attachment_slots = {};
    entity_detail::ListHead owner;
    entity_detail::ListHead global;
    auto* attachment = static_cast<ActorLiveAttachment*>(engine_heap_allocate(sizeof(ActorLiveAttachment)));
    *attachment = {};
    attachment->owner_link.prev = &owner;
    attachment->global_link.prev = &global;
    owner.next = attachment;
    global.next = &attachment->global_link;
    auto& slot = g_actor_attachment_slots[3];
    slot.owner = attachment;
    entity_detail::ListHead leased;
    leased.prev = &slot.records;
    slot.records.next = &leased;
    clear_actor_live_attachments(owner);
    if (owner.next != nullptr || global.next != nullptr || slot.owner != nullptr ||
        slot.records.next != nullptr || leased.prev != nullptr || calls != 1)
    {
        throw Error("attachment release retained a live link or a leased slot");
    }
    g_actor_attachment_slots = saved_slots;
    g_actor_broadcast = saved_broadcast;
}

void check_spawn_render_registration()
{
    using namespace gbtg;
    auto& world = entity_detail::g_world;
    const auto saved = world;
    world = {};
    Actor actor;
    actor.model_flags = 1;
    actor.model_mode = 1;
    entity_detail::SegmentProbe segment;
    entity_detail::ShapeProbe shape;
    RuntimeActorLink view;
    auto* view_sentinel = reinterpret_cast<RuntimeActorLink*>(&world.view);
    view.previous = view_sentinel;
    world.view.next = &view;
    ActorRenderRegistration state{actor, shape, segment, view};
    static std::vector<u32> notes;
    notes.clear();
    state.message = [](ActorRenderRegistration&, u32 id)
    {
        notes.push_back(id);
    };
    register_actor_render_bins(state);
    register_actor_render_bins(state);
    if (notes != std::vector<u32>{0x11})
    {
        throw Error("actor bin registration sent duplicate notifications");
    }
    for (size_t index = 0; index < 8; ++index)
    {
        if (world.bins[index].next != &state.bins[index] || state.bins[index].prev != &world.bins[index])
        {
            throw Error("actor bin registration lost an intrusive head");
        }
    }
    register_actor_segment_probe(actor, segment);
    register_actor_child_list(state);
    if (world.child.next != &state.child || world.segment != nullptr || world.view.next != nullptr ||
        actor.model_flags != 0 || actor.model_mode != 0 || notes != std::vector<u32>{0x11, 0x12})
    {
        throw Error("actor child registration retained rendering or collision registrations");
    }
    register_actor_child_list(state);
    if (state.child.next != nullptr || notes.size() != 2)
    {
        throw Error("actor child registration repeated an already complete transition");
    }
    world = saved;
}

void check_spawn_model_anchors()
{
    using namespace gbtg;
    std::array<u8, 160> cpu{};
    auto put = [&](size_t offset, u32 value)
    {
        std::memcpy(cpu.data() + offset, &value, 4);
    };
    put(0, 24);
    put(4, 1);
    put(24, 2);
    put(28, 32);
    put(32, 2);
    const u16 kind = 3;
    const i16 joint = -1;
    const AnimationMatrix matrix{1.f, 0.f, 0.f, 2.f, 0.f, 1.f, 0.f, 3.f, 0.f, 0.f, 1.f, 4.f};
    std::memcpy(cpu.data() + 36, &kind, 2);
    std::memcpy(cpu.data() + 38, &joint, 2);
    std::memcpy(cpu.data() + 40, matrix.data(), 48);
    RuntimeModelInstance runtime{};
    runtime.primary = nullptr;
    runtime.alternate = nullptr;
    ActorModel model;
    model.cpu = cpu;
    model.runtime = &runtime;
    Actor actor;
    actor.model = &model;
    actor.xform[3] = 10.f;
    entity_detail::ListHead owner;
    const auto saved = g_actor_model_anchors;
    g_actor_model_anchors = {};
    register_actor_model_anchors(actor, owner);
    auto* anchor = static_cast<ActorModelAnchor*>(owner.next);
    if (anchor == nullptr || anchor->owner != &actor || anchor->owner_link.next != nullptr ||
        g_actor_model_anchors.next != &anchor->global_link ||
        anchor->output.x != 12.f || anchor->output.y != 3.f || anchor->output.z != 4.f)
    {
        throw Error("model anchor registration lost its locator or world transform");
    }
    anchor->mode = 2;
    anchor->joint = 0xFFFF;
    anchor->local = Vec3{1.f, 2.f, 3.f};
    update_actor_model_anchor(*anchor);
    if (anchor->output.x != 11.f || anchor->output.y != 2.f || anchor->output.z != 3.f)
    {
        throw Error("model anchor local-point mode lost the actor matrix");
    }
    g_actor_model_anchors = saved;
    engine_heap_free(anchor);
}

void check_spawn_trigger_and_lifecycle()
{
    using namespace gbtg;
    Actor player;
    player.position = Vec3{1.f, 0.f, 0.f};
    GhoulySpawnPlayerLink link;
    link.actor = &player;
    link.previous_position = Vec3{3.f, 0.f, 0.f};
    const auto saved = g_spawn_trigger_players;
    g_spawn_trigger_players = [&link]()
    {
        return std::vector<GhoulySpawnPlayerLink>{link};
    };
    u32 random = 7;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, random) != 1 || random == 7)
    {
        throw Error("spawn trigger failed to detect a player entering its radius");
    }
    u32 advanced = random;
    link.previous_position = player.position;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, random) != 0 || random != advanced)
    {
        throw Error("spawn trigger fired again for a player already inside");
    }
    // A disabled mode-2 player is skipped only while DAT_00512218 is set, which no retail code does.
    const u32 invisibility = 0;
    const u32* const saved_invisibility = g_runtime_invisibility;
    g_runtime_invisibility = &invisibility;
    link.previous_position = Vec3{3.f, 0.f, 0.f};
    link.disabled_ce0 = 1;
    link.mode_cdc = 2;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, random) != 1 || random == advanced)
    {
        throw Error("spawn trigger skipped a disabled player with DAT_00512218 clear");
    }
    g_runtime_invisibility = saved_invisibility;
    link.disabled_ce0 = 0;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, -1.f, random) != -1)
    {
        throw Error("spawn trigger fabricated a successful probability test");
    }
    // The swap setup (type 0x26) over its own +0x934..+0xA54 words: 0x0006BCA0, 0x0006C0A0 and 0x0006BC70.
    RequestProbe controller;
    std::vector<u8> setup_bytes(sizeof(GhoulySwapState));
    GhoulySwapState& setup_state = actor_record_view<GhoulySwapState>(std::span<u8>(setup_bytes));
    Actor setup_actor;
    Actor source;
    source.position = Vec3{5.f, 6.f, 7.f};
    GhoulySpawnRecord* load = nullptr;
    std::vector<std::vector<u32>> requests;
    RuntimeGhoulySwapSetup setup{setup_bytes, setup_actor, controller, random, load,
        [](const void*) -> u32
        {
            return 0;
        },
        [](u32, std::span<u32>)
        {
            throw Error("swap setup without an anchor messaged one");
        },
        [&](std::span<u32> message)
        {
            requests.emplace_back(message.begin(), message.end());
        },
        [&](u32 address) -> Actor*
        {
            return address == 0x5000 ? &source : nullptr;
        }};
    setup_state.source = 0x5000u;
    Vec3 source_position;
    Vec3 source_euler;
    Vec3 position;
    Vec3 euler;
    if (!query_runtime_ghouly_swap_pose(setup, source_position, source_euler, position, euler) ||
        position.z != 7.f)
    {
        throw Error("swap setup pose query lost its actual spawn source");
    }
    setup_state.radius = 2.f;
    setup_state.chance = 1.f;
    std::strcpy(setup_state.box.data(), "aid_ghoulybox_trigger_test");
    std::strcpy(setup_state.attributes.data(), "attributes");
    update_runtime_ghouly_swap_setup(setup);
    if (requests.size() != 1 || requests[0][0] != 0xA8 ||
        std::strcmp(reinterpret_cast<const char*>(requests[0].data() + 1), "aid_ghoulybox_trigger_test") != 0 ||
        std::strcmp(reinterpret_cast<const char*>(requests[0].data() + 0x21), "attributes") != 0)
    {
        throw Error("swap setup update did not send its actual queued request");
    }
    setup_state.swapped = 1u;
    update_runtime_ghouly_swap_setup(setup);
    if (requests.size() != 1)
    {
        throw Error("swap setup update retriggered after its ghouly was made");
    }
    load = allocate_ghouly_spawn_record();
    setup_state.load = 1u;
    update_runtime_ghouly_swap_setup(setup);
    if (load != nullptr || setup_state.load != 0)
    {
        throw Error("swap setup update retained a completed spawn record");
    }
    const size_t count = g_ghouly_controllers.size();
    g_ghouly_controllers.push_back({123, &controller});
    load = allocate_ghouly_spawn_record();
    setup_state.load = 1u;
    destroy_runtime_ghouly_swap_setup(setup);
    if (load != nullptr || setup_state.load != 0 || g_ghouly_controllers.size() != count)
    {
        throw Error("swap setup shutdown retained its record or registration");
    }
    g_spawn_trigger_players = saved;
}

void check_spawn_shape_registration()
{
    using namespace gbtg;
    std::array<u8, 64> cpu{};
    auto put = [&](size_t offset, u32 value)
    {
        std::memcpy(cpu.data() + offset, &value, 4);
    };
    put(0, 24);
    put(4, 1);
    put(24, 4);
    put(28, 32);
    put(32, 1);
    cpu[41] = 0;
    put(44, 0x10);
    ActorModel model;
    model.cpu = cpu;
    Actor actor;
    actor.model = &model;
    actor.model_mode = 0x10;
    entity_detail::ShapeProbe first;
    entity_detail::ShapeProbe second;
    auto& world = entity_detail::g_world;
    auto* saved_first = world.shape;
    auto* saved_previous = world.shape_prev;
    world.shape = nullptr;
    world.shape_prev = nullptr;
    register_actor_shape_probe(actor, first);
    register_actor_shape_probe(actor, second);
    if (world.shape != &second || second.next != &first || first.prev != &second)
    {
        throw Error("shape registration lost head insertion or mask matching");
    }
    register_actor_shape_probe(actor, second);
    if (second.next != &first)
    {
        throw Error("shape registration inserted an already registered probe twice");
    }
    actor.model_mode = 0x20;
    register_actor_shape_probe(actor, second);
    if (world.shape != &first || second.prev != nullptr || second.next != nullptr)
    {
        throw Error("shape mask mismatch failed to unregister");
    }
    clear_actor_shape_probe(actor, first);
    if (world.shape != nullptr || first.prev != nullptr || actor.model_mode != 0)
    {
        throw Error("shape shutdown retained its mask or intrusive link");
    }
    world.shape = saved_first;
    world.shape_prev = saved_previous;
}

void check_spawn_scheduler()
{
    {
        using namespace gbtg;
        std::vector<void*> allocations;
        AudioCoreSources sources{};
        sources.memory.allocate = [&](size_t bytes) -> void*
        {
            void* value = std::malloc(bytes);
            allocations.push_back(value);
            return value;
        };
        sources.memory.release = [&](void* value)
        {
            const auto found = std::find(allocations.begin(), allocations.end(), value);
            if (found == allocations.end())
            {
                throw Error("audio core released an unowned allocation");
            }
            allocations.erase(found);
            std::free(value);
        };
        int created = 0;
        sources.boundaries.create = [&](u32 flags, void*& handle) -> i32
        {
            ++created;
            handle = &sources;
            return 0;
        };
        sources.boundaries.bad_pointer = [](const void* pointer)
        {
            return pointer == nullptr;
        };
        sources.boundaries.debug_string = [](const char*)
        {
        };
        sources.boundaries.mix_bins = [](void*, std::span<const AudioCoreMixBin>)
        {
        };
        sources.boundaries.mix_volumes = [](void*, std::span<const AudioCoreMixBin>)
        {
        };
        u32 status = 0;
        sources.boundaries.status = [&](void*, u32& output)
        {
            output = status;
        };
        int released = 0;
        sources.boundaries.release = [&](void*)
        {
            ++released;
        };
        AudioCoreSource* flat;
        create_audio_core_source(0, flat, sources);
        AudioCoreSource* spatial;
        create_audio_core_source(1, spatial, sources);
        constexpr u32 expected[]{6, 8, 7, 9, 2, 10, 3};
        if (created != 1 || flat->handle != nullptr || spatial->mix_count != 7 ||
            sources.sources.next != flat || flat->next != spatial || sources.tail != spatial)
        {
            throw Error("audio source creation changed the mode-zero boundary or list ownership");
        }
        for (size_t index = 0; index < std::size(expected); ++index)
        {
            if (spatial->mix[index].bin != expected[index] || spatial->mix_flags[index] != 0x7F)
            {
                throw Error("audio source mix bins lost the retail ordering");
            }
        }
        AudioCoreCues cues{sources};
        AudioCoreBank bank{};
        bank.runtime.handle = &bank;
        int played = 0;
        int paused = 0;
        int stopped = 0;
        cues.boundaries.prepare = [&](void* handle, u32 index, void*& output) -> i32
        {
            if (handle != &bank || index != 0)
            {
                throw Error("audio cue preparation changed the bank or cue index");
            }
            output = &bank;
            return 0;
        };
        cues.boundaries.play = [&](void*, void*, void*&) -> i32
        {
            ++played;
            return 0;
        };
        cues.sound.pause_cue = [&](void*, void*, bool pause) -> i32
        {
            paused += pause ? 1 : -1;
            return 0;
        };
        cues.sound.stop_cue = [&](void*, u32, u32, void*)
        {
            ++stopped;
        };
        cues.sound.last_error = []() -> u32
        {
            return 0;
        };
        AudioCueName cue_name{};
        std::strcpy(cue_name.name.data(), "TEST");
        AudioBankNames bank_name{};
        std::strcpy(bank_name.name.data(), "GMISC");
        bank_name.cues = &cue_name;
        const auto saved_names = g_audio_bank_names;
        g_audio_bank_names = &bank_name;
        AudioCoreBank* banks[]{&bank};
        const std::array<u32, 2> curves[]{std::array<u32, 2>{0, 0}};
        const Vec3 origin{};
        const Vec3 cone{1.f, 1.f, 1.f};
        AudioEventVoices events{cues, {}, banks, curves, 0, origin, cone};
        events.voices[0].state = 1;
        events.voices[0].source = flat;
        if (!prepare_audio_event_voice(0, "XACT_SOUNDBANK_GMISC_TEST", events) ||
            events.voices[0].state != 2 || audio_core_cue_user_data(*events.voices[0].cue) != &events.voices[0])
        {
            throw Error("audio event preparation failed to install cue ownership and state");
        }
        events.voices[0].state = 3;
        audio_event_cue_callback(&events, *events.voices[0].cue, 3);
        if (events.voices[0].state != 7 || played != 1 || paused != 1 || cues.started_count != 1)
        {
            throw Error("audio ready callback did not preserve a requested pause");
        }
        audio_event_cue_callback(&events, *events.voices[0].cue, 1);
        if (events.voices[0].state != 8 || events.voices[0].cue != nullptr || bank.cues.next != nullptr ||
            paused != 0 || stopped != 1 || cues.stopped_count != 1)
        {
            throw Error("audio completion callback retained its cue or changed stop bookkeeping");
        }
        DialogueSoundTagState page_sound{};
        std::strcpy(page_sound.cue.data(), "XACT_SOUNDBANK_GMISC_TEST");
        request_dialogue_page_voice(page_sound, events);
        auto& page_voice = events.voices[1];
        if (page_voice.state != 2 || page_voice.stop != 1 || page_voice.mode != 0 ||
            page_voice.cue == nullptr || audio_core_cue_user_data(*page_voice.cue) != &page_voice)
        {
            throw Error("dialogue page voice lost its flat source, stop flag or cue ownership");
        }
        audio_event_cue_callback(&events, *page_voice.cue, 1);
        release_audio_event_voice(page_voice, events);
        std::strcpy(page_sound.cue.data(), "XACT_SOUNDBANK_GMISC_MISSING");
        request_dialogue_page_voice(page_sound, events);
        if (page_voice.state != 1 || page_voice.stop != 1 || page_voice.cue != nullptr)
        {
            throw Error("dialogue page voice invented cue success for an unresolved sound name");
        }
        release_audio_event_voice(page_voice, events);
        const auto retained_voices = events.voices;
        for (auto& voice : events.voices)
        {
            voice.state = 1;
        }
        const auto retained_allocations = allocations.size();
        request_dialogue_page_voice(page_sound, events);
        if (allocations.size() != retained_allocations || bank.cues.next != nullptr)
        {
            throw Error("dialogue page voice bypassed exhaustion of the retail voice slots");
        }
        events.voices = retained_voices;
        g_audio_bank_names = saved_names;
        status = 1;
        destroy_audio_core_source(spatial, sources);
        if (spatial == nullptr || released != 0)
        {
            throw Error("audio source destruction released an in-use source");
        }
        status = 0;
        destroy_audio_core_source(spatial, sources);
        destroy_audio_core_source(flat, sources);
        if (!allocations.empty() || sources.sources.next != nullptr || sources.secondary.next != nullptr || released != 1)
        {
            throw Error("audio source destruction retained owned memory or links");
        }
        sources.boundaries.create = [](u32, void*&) -> i32
        {
            return static_cast<i32>(0x80004005u);
        };
        create_audio_core_source(5, flat, sources);
        if (flat != nullptr || !allocations.empty() || sources.sources.next != nullptr)
        {
            throw Error("audio source creation failure did not release the wrapper");
        }
    }
    {
        using namespace gbtg;
        AudioCueName first{};
        AudioCueName second{};
        std::strcpy(first.name.data(), "First");
        std::strcpy(second.name.data(), "Second_Cue");
        first.next = &second;
        AudioBankNames bank{};
        bank.cues = &first;
        std::strcpy(bank.name.data(), "GMISC");
        const auto saved_banks = g_audio_bank_names;
        g_audio_bank_names = &bank;
        i32 bank_index = -7;
        i32 cue_index = -8;
        if (!is_audio_bank_resource("XACT_SOUNDBANK_GMISC_Second_Cue") ||
            is_audio_bank_resource("xact_soundbank_GMISC_Second_Cue") ||
            !resolve_audio_bank_cue("XACT_SOUNDBANK_gmisc_second_cue", &bank_index, &cue_index) ||
            bank_index != 0 || cue_index != 1)
        {
            throw Error("audio bank/cue lookup lost prefix case, cue separators, or retail indices");
        }
        bank_index = -7;
        cue_index = -8;
        if (resolve_audio_bank_cue("XACT_SOUNDBANK_GMISC_missing", &bank_index, &cue_index) ||
            bank_index != -7 || cue_index != -8)
        {
            throw Error("failed audio cue lookup modified its output indices");
        }
        std::strcpy(bank.name.data(), "ABCDEFGHIJKLMN_first");
        if (find_audio_bank_name("abcdefghijklmn_second", bank_index) != &bank)
        {
            throw Error("audio bank lookup compared past the retail fourteen-character limit");
        }
        g_audio_bank_names = saved_banks;
    }
    {
        using namespace gbtg;
        std::vector<u8> stream;
        auto add = [&](u32 kind, u32 bytes)
        {
            const size_t offset = stream.size();
            stream.resize(offset + bytes);
            std::memcpy(stream.data() + offset, &bytes, 4);
            std::memcpy(stream.data() + offset + 4, &kind, 4);
            return offset;
        };
        auto word = [&](size_t offset, u32 value)
        {
            std::memcpy(stream.data() + offset, &value, 4);
        };
        size_t offset = add(1, 12);
        word(offset + 8, 4);
        for (const char* name : {"aid_first_box", "aid_second_box", "aid_third_box"})
        {
            offset = add(2, 136);
            std::strcpy(reinterpret_cast<char*>(stream.data() + offset + 8), name);
        }
        offset = add(3, 12);
        word(offset + 8, 5);
        for (u32 key : {7u, 7u, 9u})
        {
            offset = add(4, 12);
            word(offset + 8, key);
        }
        offset = add(7, 24);
        word(offset + 8, std::bit_cast<u32>(2.f));
        word(offset + 12, 0);
        word(offset + 16, std::bit_cast<u32>(0.5f));
        word(offset + 20, std::bit_cast<u32>(1.f));
        offset = add(14, 136);
        std::strcpy(reinterpret_cast<char*>(stream.data() + offset + 8), "aid_attributes_queue");
        offset = add(15, 12);
        word(offset + 8, 0x12345678);
        add(0, 8);
        AnimationBundleRecord resource;
        resource.name = "aid_aidlist_queue_test";
        resource.cpu = stream;
        std::array<AnimationBundlePool, 1> pools{AnimationBundlePool{{&resource, 1}, true}};
        AnimationResourceRegistry registry;
        register_animation_resource_provider(registry,
            [&](u32, const AnimationResourceRecord& request, const void*& resolved)
            {
                return find_bundle_resource_provider(pools, request.name.c_str(), resolved);
            },
            [&](u32, const void* resolved, AnimationLoadedResource& loaded)
            {
                return load_bundle_resource_provider(resolved, loaded);
            },
            [&](u32, const char* name, const AnimationLoadedResource&)
            {
                unload_bundle_resource_provider(pools, name);
            }, 3, nullptr);
        ResourceErrorState error{};
        const auto saved_head = g_aid_spawn_queue;
        g_aid_spawn_queue = {};
        auto* first = load_aid_spawn_queue(resource.name.c_str(), registry, error);
        auto* second = load_aid_spawn_queue(resource.name.c_str(), registry, error);
        if (first == nullptr || second == nullptr || g_aid_spawn_queue.next != first ||
            first->next != second || second->previous != first || first->remaining != -3 ||
            first->value_1c != 0x5678 || first->names->mode != 4 ||
            first->names->pool->cursor - first->names->pool->begin != 3 * 128 ||
            first->controllers->mode != 5 || first->controllers->distinct != 2 ||
            first->controllers->occurrences[7] != 2 || first->intervals->mode != 3 ||
            read_f32({first->intervals->pool->begin, 12}, 8) != -0.5f)
        {
            throw Error("aid spawn queue loading lost decoded modes, duplicate controllers, growth, or list order");
        }
        decode_aid_spawn_queue(*first, stream.data());
        release_aid_spawn_queue(first);
        if (g_aid_spawn_queue.next != second || second->previous != &g_aid_spawn_queue)
        {
            throw Error("aid spawn queue release broke the next record's owner link");
        }
        entity_release_aid_records();
        if (g_aid_spawn_queue.next != nullptr)
        {
            throw Error("scene release retained its aid spawn queue");
        }
        g_aid_spawn_queue = saved_head;
    }
    {
        using namespace gbtg;
        std::array<u8, 0x80> cpu{};
        auto store = [&]<class T>(size_t offset, T value)
        {
            std::memcpy(cpu.data() + offset, &value, sizeof(value));
        };
        store(0, u32(8));
        store(4, u32(1));
        store(8, u32(5));
        store(12, u32(0x20));
        store(0x40, -3.f);
        ActorModel model;
        model.cpu = cpu;
        Actor actor;
        actor.model = &model;
        actor.scale_200 = 2.f;
        actor.radius_xz = 4.f;
        if (actor_model_height(actor) != 24.L)
        {
            throw Error("scare fallback height did not use model type five and absolute scaled height");
        }
        store(8, u32(6));
        if (actor_model_height(actor) != 0.L)
        {
            throw Error("scare height accepted another model block type");
        }
        i32 calls = 0;
        const auto saved = g_marked_actor_message;
        g_marked_actor_message = [&calls](std::span<const u32> raw)
        {
            if (raw[0] == 0x37)
            {
                ++calls;
            }
        };
        const std::array<u32, 1> message{0x37};
        broadcast_actor_world_message(message);
        g_marked_actor_message = saved;
        if (calls != 1)
        {
            throw Error("world actor broadcast did not reach the DAT_00511B38 view");
        }
    }
    {
        using namespace gbtg;
        static i32 trail_calls;
        static std::array<u8, 0x358> received;
        static const ObjectFactoryVtable vtable
        {
            0x1234,
            [](ObjectSizes& sizes)
            {
                sizes = {16, 0x358};
            },
            [](FactoryObject&, std::span<const u8> parameters)
            {
                ++trail_calls;
                std::memcpy(received.data(), parameters.data(), received.size());
                return true;
            },
            nullptr
        };
        std::array<ObjectFactoryType, 17> types{};
        types[16].vtable = []()
        {
            return &vtable;
        };
        ObjectFactoryRegistry factory{types, 0};
        std::vector<FactoryObject> objects;
        float pause = 0.f;
        float delta = 0.2f;
        float time_scale = 1.f;
        i32 pal = 0;
        i32 override_pal = 0;
        ActorTrailContext trail{factory, objects, 0x11223344, pause, delta,
            time_scale, pal, override_pal, ""};
        Actor actor;
        actor.animation_rate = 1.f;
        actor.scale_200 = 2.f;
        actor.scale_x = 3.f;
        actor.radius_y = 4.f;
        actor.radius_xz = 5.f;
        float timer = 0.f;
        trail_calls = 0;
        // 0x0013B030 pushes the record address before its stores, so the source, life, alpha and scale land at
        // +0x334..+0x348 (the after-image 0x000690F0 reads them there) and +0x34C / +0x350 are zero.
        if (!request_actor_trail(actor, 6.f, timer, 30, trail) || trail_calls != 1 ||
            objects.size() != 1 || read_u16(received, 2) != 0x43 ||
            read_u16(received, 4) != 0x10 || read_u32(received, 0x334) != 0x11223344 ||
            read_f32(received, 0x24) != 2.f || read_f32(received, 0x338) != 6.f ||
            read_f32(received, 0x340) != 3.f || read_f32(received, 0x344) != 4.f ||
            read_f32(received, 0x348) != 5.f || received[0x33C] != 0x80 || read_u32(received, 0x34C) != 0 ||
            read_u32(received, 0x350) != 0)
        {
            throw Error("actor trail lost factory selection, its source pointer, or derived parameter offsets");
        }
        actor.animation_pause = 1.f;
        timer = 0.f;
        if (request_actor_trail(actor, 6.f, timer, 30, trail) || trail_calls != 1 || timer != 0.f)
        {
            throw Error("paused actor created a trail");
        }
        std::array<u8, 0x334> initialized;
        initialized.fill(0xCD);
        initialize_actor_effect_parameters(initialized.data(), "");
        if (initialized[6] != 0xCD || initialized[0xA9] != 0xCD ||
            read_u16(initialized, 0x2A8) != 0xFFFF || initialized[0x2B4] != 0)
        {
            throw Error("effect parameter initializer overwrote unspecified retail fields");
        }
    }
    {
        using namespace gbtg;
        std::array<u32, 6> rows{11, 1, 22, 1, 33, 1};
        auto* begin = reinterpret_cast<u8*>(rows.data());
        GhoulySpawnPool pool{8, 0, begin, begin + sizeof(rows), begin + sizeof(rows)};
        SpawnPoolSelection selection{&pool, 0, 0, -1};
        u32 state = 123;
        if (select_spawn_name(selection, state) != begin ||
            select_spawn_name(selection, state) != begin + 8 || state != 123)
        {
            throw Error("spawn name sequencing changed its row or random state");
        }
        const i32 index = select_unused_spawn_controller(selection, state);
        if (index < 0 || index >= 3 || rows[1] != 0 || rows[3] != 0 || rows[5] != 0)
        {
            throw Error("exhausted spawn controller selection did not reset its usage counters");
        }
        const auto saved_controllers = g_ghouly_controllers;
        GhoulyController first;
        GhoulyController second;
        Actor first_actor;
        Actor second_actor;
        first_actor.position = Vec3{10.f, 0.f, 0.f};
        second_actor.position = Vec3{2.f, 0.f, 0.f};
        g_ghouly_controllers = {{11, &first}, {22, &second}, {33, nullptr}};
        std::unordered_map<const GhoulyController*, Actor*> owners
        {
            {&first, &first_actor}, {&second, &second_actor}
        };
        selection.mode = 2;
        if (select_spawn_controller(selection, Vec3{}, state, owners) != 22 ||
            selection.previous != 1 || rows[3] != 1)
        {
            throw Error("nearest spawn controller selection lost its owner position or usage count");
        }
        first_actor.position = second_actor.position;
        if (select_nearest_spawn_controller(selection, Vec3{}, owners) != 0)
        {
            throw Error("nearest spawn controller did not retain the first equal-distance row");
        }
        g_ghouly_controllers = saved_controllers;
        pool.cursor = begin;
        const u32 sequence = selection.sequence;
        if (select_spawn_attributes(selection, state) != nullptr || selection.sequence != sequence)
        {
            throw Error("empty spawn attribute selection advanced its cursor");
        }
        std::array<float, 3> intervals{1.f, 2.f, 0.75f};
        pool.begin = reinterpret_cast<u8*>(intervals.data());
        pool.cursor = pool.begin + sizeof(intervals);
        pool.stride = 4;
        selection.mode = 3;
        if (select_spawn_interval(selection, state) != 1.f || intervals[0] != 1.75f ||
            select_spawn_interval(selection, state) != 1.75f || intervals[0] != 2.f || intervals[2] != 0.f)
        {
            throw Error("spawn interval ramp did not retain the current value and clamp the next value");
        }
        intervals = {2.f, 1.f, -0.75f};
        if (select_spawn_interval(selection, state) != 2.f ||
            select_spawn_interval(selection, state) != 1.25f || intervals[0] != 1.f || intervals[2] != 0.f)
        {
            throw Error("descending spawn interval did not stop at its endpoint");
        }
        std::array<u8, 256> attempts{};
        attempts[0] = 4;
        attempts[1] = 255;
        attempts[255] = 1;
        if (count_spawn_controller_attempts(attempts) != 3)
        {
            throw Error("spawn attempts counted retries instead of distinct controllers");
        }
    }
    using namespace gbtg;
    struct Probe : GhoulyController
    {
        i32 calls = 0;
        i32 result = 1;
        GhoulySpawnMessage received;
        Probe()
        {
            message = [](GhoulyController& base, void* raw)
            {
                auto& self = static_cast<Probe&>(base);
                ++self.calls;
                auto& message = *static_cast<GhoulySpawnMessage*>(raw);
                self.received = message;
                message.result = self.result;
            };
        }
    } probe;
    u32 random = 17;
    u32 flags = 0;
    i32 maximum = 0;
    i32 minimum = 0;
    i32 bodies = 0;
    std::array<u32, 8> limits{};
    std::array<u8, 8> cpu{};
    cpu[2] = 3;
    ExecutableResourceRecord resource;
    resource.name = "aid_actor_scheduler_test";
    resource.hash = animation_resource_hash(resource.name.c_str());
    resource.cpu = cpu;
    ExecutableResourcePool pool{{&resource, 1}, "actor", true};
    GhoulySpawnContext context{random, flags, maximum, minimum, bodies, limits.data(), {&pool, 1}};
    const auto saved_side = g_side_list;
    g_side_list = {};
    minimum = 1;
    if (!spawn_queue_below_minimum(context) || !spawn_queue_below_maximum(context))
    {
        throw Error("empty spawn queue did not permit a spawn below its population minimum");
    }
    ActorLoadRecord pending;
    g_side_list.next = &pending;
    if (spawn_queue_below_minimum(context) || spawn_queue_below_maximum(context))
    {
        throw Error("unfinished spawn request did not block another queue request");
    }
    pending.flags_2c = 1;
    maximum = 1;
    bodies = 1;
    if (spawn_queue_below_maximum(context))
    {
        throw Error("spawn queue exceeded its population maximum");
    }
    std::array<u8, 0x90> class_records{};
    const u32 row_stride = 0x88;
    const u32 row_kind = 1;
    std::memcpy(class_records.data(), &row_stride, 4);
    std::memcpy(class_records.data() + 4, &row_kind, 4);
    std::strcpy(reinterpret_cast<char*>(class_records.data() + 8), resource.name.c_str());
    if (!spawn_script_under_class_limits(class_records.data(), context))
    {
        throw Error("spawn class gate rejected an available unlimited resource");
    }
    std::strcpy(reinterpret_cast<char*>(class_records.data() + 8), "aid_missing_spawn_resource");
    if (spawn_script_under_class_limits(class_records.data(), context))
    {
        throw Error("spawn class gate accepted a missing actor resource");
    }
    g_side_list = saved_side;
    minimum = 0;
    maximum = 0;
    bodies = 0;
    {
        struct QueueProbe : GhoulyController
        {
            i32 calls = 0;
            u32 result = 1;
            QueueProbe()
            {
                message = [](GhoulyController& controller, void* raw)
                {
                    auto& self = static_cast<QueueProbe&>(controller);
                    ++self.calls;
                    static_cast<GhoulyRequest*>(raw)->result = self.result;
                };
            }
        } receiver;
        const auto saved_controllers = g_ghouly_controllers;
        const auto saved_side_queue = g_side_list;
        g_side_list = {};
        g_ghouly_controllers = {{1, &receiver}};
        std::array<u8, 128> names{};
        std::strcpy(reinterpret_cast<char*>(names.data()), "aid_ghoulybox_queue_test");
        GhoulySpawnPool name_pool{128, 2, names.data(), names.data() + 128, names.data() + 128};
        SpawnPoolSelection name_selection{&name_pool, 0, 0, -1};
        std::array<u32, 2> keys{1, 0};
        auto* key_begin = reinterpret_cast<u8*>(keys.data());
        GhoulySpawnPool key_pool{8, 2, key_begin, key_begin + 8, key_begin + 8};
        SpawnControllerSelection key_selection{{&key_pool, 0, 0, -1}, 1, {}};
        float interval = 0.f;
        auto* interval_begin = reinterpret_cast<u8*>(&interval);
        GhoulySpawnPool interval_pool{4, 2, interval_begin, interval_begin + 4, interval_begin + 4};
        SpawnPoolSelection interval_selection{&interval_pool, 0, 0, 0};
        AidSpawnQueueRecord queued{};
        queued.names = &name_selection;
        queued.controllers = &key_selection;
        queued.intervals = &interval_selection;
        queued.remaining = 1;
        AnimationResourceRegistry registry;
        ResourceErrorState error{};
        std::unordered_map<const GhoulyController*, Actor*> owners;
        i32 game_state = 3;
        i32 game_gate = 0;
        AidSpawnQueueContext queue_context{context, registry, error, owners, game_state, game_gate};
        minimum = 1;
        if (!tick_aid_spawn_queue(queued, Vec3{}, 0.1f, queue_context) ||
            receiver.calls != 1 || queued.remaining != 0 || queued.current != nullptr)
        {
            throw Error("aid spawn queue did not complete one successful controller request");
        }
        receiver.calls = 0;
        receiver.result = 0;
        queued.remaining = -2;
        if (!tick_aid_spawn_queue(queued, Vec3{}, 0.1f, queue_context) ||
            receiver.calls != 20 || queued.attempts[1] != 20)
        {
            throw Error("aid spawn queue lost its minimum retry count or all-controller completion rule");
        }
        queued.remaining = -3;
        if (tick_aid_spawn_queue(queued, Vec3{}, 0.1f, queue_context))
        {
            throw Error("aid spawn queue completed while its retail game gate was closed");
        }
        game_gate = 1;
        if (!tick_aid_spawn_queue(queued, Vec3{}, 0.1f, queue_context))
        {
            throw Error("aid spawn queue ignored its open retail game gate");
        }
        auto* finished = allocate_ghouly_spawn_record();
        link_actor_load_record(&g_side_list, *finished);
        float pause = 0.f;
        float frame_delta = 0.1f;
        float time_scale = 1.f;
        u32 player_address = 0x93C;
        std::unordered_map<u32, Actor*> frame_actors;
        LiveSpawnTickContext frame_context{queue_context, pause, frame_delta, time_scale,
            player_address, frame_actors};
        flags = 0x40;
        tick_live_spawn_queues(frame_context);
        if (g_side_list.next != finished)
        {
            throw Error("disabled live spawn worker consumed a pending queue record");
        }
        flags = 0;
        tick_live_spawn_queues(frame_context);
        if (g_side_list.next != nullptr || finished->prev != nullptr || finished->next != nullptr)
        {
            throw Error("live spawn worker did not unlink its completed controller record");
        }
        release_ghouly_spawn_record(*finished);
        g_ghouly_controllers = saved_controllers;
        g_side_list = saved_side_queue;
        minimum = 0;
    }
    auto* record = allocate_ghouly_spawn_record();
    record->controller_24 = &probe;
    append_ghouly_spawn_definition(*record, resource.name.c_str(), "variant", 1.f,
        "aid_attributes_row", "second", 0);
    std::strcpy(record->attributes_30, "aid_attributes_request");
    record->remaining_c = 2;
    record->timer_20 = 0.f;
    if (!tick_ghouly_spawn_record(*record, 1.f, context) || probe.calls != 2 ||
        record->remaining_c != 0 || record->value_10 != 2 ||
        std::strcmp(probe.received.attributes, "aid_attributes_request") != 0 ||
        std::strcmp(probe.received.variant, "variant") != 0)
    {
        throw Error("spawn scheduler lost request overrides or completion count");
    }
    record->remaining_c = 1;
    probe.calls = 0;
    probe.result = 0;
    if (tick_ghouly_spawn_record(*record, 1.f, context) || probe.calls != 10 || record->remaining_c != 1)
    {
        throw Error("spawn scheduler did not retain failed requests after ten retries");
    }
    record->timer_20 = 2.f;
    probe.calls = 0;
    if (tick_ghouly_spawn_record(*record, 0.5f, context) || record->timer_20 != 1.5f || probe.calls != 0)
    {
        throw Error("spawn scheduler ignored the retail delay");
    }
    record->timer_20 = 0.f;
    flags = 0x01000000;
    if (!tick_ghouly_spawn_record(*record, 1.f, context) || record->remaining_c != 0 || probe.calls != 0)
    {
        throw Error("spawn scheduler lost terminal global flag behavior");
    }
    flags = 0;
    auto* definition = reinterpret_cast<GhoulySpawnDefinition*>(record->pool_8->begin);
    definition->flags = 1;
    probe.result = 1;
    if (dispatch_next_ghouly_spawn(*record, 1, context) != 1 ||
        record->pool_8->cursor != record->pool_8->begin || record->index_28 != -1 || record->value_10 != 2)
    {
        throw Error("one-shot spawn row did not erase without advancing the sequence");
    }
    release_ghouly_spawn_record(*record);
}

void check_spawn_point_queries()
{
    using namespace gbtg;
    EntityBonePose pose;
    pose.mats = {1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f, 10.f, 20.f, 30.f, 1.f};
    const float* matrix = shape_bone_matrix(pose, 0);
    if (matrix[3] != 10.f || matrix[7] != 20.f || matrix[11] != 30.f)
    {
        throw Error("spawn bone matrix lost its unpacked translation");
    }
    EntityShapeNode first;
    first.bone = 0;
    first.along_x = 2.f;
    first.along_y = 1.f;
    first.point = Vec3{2.f, 3.f, 0.f};
    Vec3 center;
    if (shape_sphere_metrics(first, nullptr, &pose, center) != 2.f ||
        center.x != 11.f || center.y != 22.f || center.z != 33.f)
    {
        throw Error("spawn sphere lost its retail center or radius");
    }
    pose.packed = 1;
    pose.mats = {1.f, 0.f, 0.f, 10.f, 0.f, 1.f, 0.f, 20.f,
        0.f, 0.f, 1.f, 30.f};
    if (shape_bone_matrix(pose, 0) != pose.mats.data())
    {
        throw Error("packed spawn bone matrix was copied instead of borrowed");
    }
    Actor actor;
    ActorModel model;
    model.alternate_shape_pose = &pose;
    actor.model = &model;
    actor.xform[0] = actor.xform[5] = actor.xform[10] = 1.f;
    EntityShapeNode second = first;
    second.along_y = 9.f;
    const EntityShapeNode* points[] = {&first, &second};
    actor.spawn_points = points;
    actor.spawn_point_count = 2;
    float radius = -1.f;
    i32 index = -1;
    if (!select_actor_spawn_point(actor, Vec3{19.f, 22.f, 33.f}, center, radius, index) ||
        index != 1 || center.x != 19.f || radius != 2.f)
    {
        throw Error("spawn query did not select the closest transformed point");
    }
    actor.spawn_point_count = 0;
    index = 99;
    radius = 77.f;
    if (select_actor_spawn_point(actor, Vec3{}, center, radius, index) || index != 99 || radius != 77.f)
    {
        throw Error("empty spawn query changed its outputs");
    }
    Actor other;
    actor.position = Vec3{3.f, 4.f, 0.f};
    other.position = Vec3{0.f, 26.f, 0.f};
    ActorSpawnLink last{nullptr, nullptr, &other};
    ActorSpawnLink link{nullptr, &last, &actor};
    const auto saved = entity_detail::g_world.body_b;
    entity_detail::g_world.body_b.next = &link;
    float distance = 88.f;
    if (find_nearest_spawn_actor(Vec3{}, nullptr, &distance, 0) != &actor || distance != 5.f)
    {
        throw Error("nearest spawn query lost its vertical gate or distance output");
    }
    entity_detail::g_world.body_b.next = nullptr;
    distance = 88.f;
    if (find_nearest_spawn_actor(Vec3{}, nullptr, &distance, 0) != nullptr || distance != 88.f)
    {
        throw Error("empty spawn list changed its distance output");
    }
    entity_detail::g_world.body_b = saved;
}

void check_actor_occupancy_and_paths()
{
    using namespace gbtg;
    using namespace gbtg::entity_detail;
    OccupancyRecord sphere;
    sphere.extent_x = 2.f;
    float distance = 77.f;
    if (!occupancy_sphere_contains(sphere, Vec3{2.f, 0.f, 0.f}, distance) || distance != 77.f ||
        occupancy_sphere_contains(sphere, Vec3{5.f, 0.f, 0.f}, distance) || distance != 3.f)
    {
        throw Error("sphere occupancy changed boundary or distance-output behavior");
    }
    OccupancyRecord box;
    box.kind = 1;
    box.extent_x = 2.f;
    box.extent_z = 4.f;
    distance = 77.f;
    if (!occupancy_box_contains(box, Vec3{0.f, 3.f, 0.f}, distance) || distance != 77.f ||
        occupancy_box_contains(box, Vec3{0.f, 5.f, 0.f}, distance) || distance != 5.f ||
        occupancy_box_contains(box, Vec3{5.f, 0.f, 8.f}, distance) || distance != 5.f)
    {
        throw Error("box occupancy changed retail height gates or corner distance");
    }
    box.extent_x = 4.f;
    box.extent_z = 1.f;
    box.yaw = 1.57079632679f;
    if (!occupancy_box_contains(box, Vec3{0.f, 0.f, 3.f}, distance) ||
        occupancy_box_contains(box, Vec3{3.f, 0.f, 0.f}, distance))
    {
        throw Error("box occupancy rotation lost its retail orientation");
    }
    const float nan = std::numeric_limits<float>::quiet_NaN();
    distance = 77.f;
    box.yaw = 0.f;
    if (!occupancy_sphere_contains(sphere, Vec3{nan, 0.f, 0.f}, distance) ||
        !occupancy_box_contains(box, Vec3{nan, 0.f, 0.f}, distance) || distance != 77.f)
    {
        throw Error("occupancy changed unordered containment comparisons");
    }
    std::array<OccupancyRecord, 4> records{};
    records[0].kind = -1;
    records[0].mask = 4;
    records[1] = sphere;
    records[1].mask = 4;
    records[1].group = 7;
    records[2] = records[1];
    records[2].group = 0;
    records[2].kind = 1;
    records[2].extent_z = 2.f;
    records[3] = records[2];
    OccupancyPool pool{0x38, 4, records.data(), records.data() + 3, records.data() + 4};
    void* saved_pool = g_occupancy_pool;
    const i32 saved_mask = g_occupancy_mask;
    g_occupancy_pool = &pool;
    g_occupancy_mask = 4;
    if (find_occupancy_record(Vec3{}, 0.f, 4, nullptr, 7) != &records[1] ||
        find_occupancy_record(Vec3{}, 0.f, 4, &records[1], 7) != &records[2] ||
        find_occupancy_record(Vec3{}, 0.f, 4, nullptr, 9) != &records[2] ||
        find_occupancy_record(Vec3{}, 0.f, 8, nullptr, 0) != nullptr)
    {
        throw Error("occupancy query lost type, group, mask or first-match gates");
    }
    if (find_occupancy_record(Vec3{}, 0.f, 4, &records[2], 9) != nullptr)
    {
        throw Error("occupancy query walked past its used cursor into spare capacity");
    }
    NavGrid grid;
    grid.width = 5;
    grid.depth = 5;
    grid.cell = 10;
    grid.cells.resize(25);
    for (size_t index = 0; index < grid.cells.size(); ++index)
    {
        grid.cells[index].x = i16((index % 5) * 10);
        grid.cells[index].z = i16((index / 5) * 10);
        grid.cells[index].connections = 0xFF;
        grid.cells[index].flags_7 = 4;
    }
    if (navigation_cell_allowed(grid, 0, 0, 0, 0, 1, nullptr, 7) ||
        !navigation_cell_allowed(grid, 0, 0, 0, 0, 1, &records[1], 7) ||
        !navigation_cell_allowed(grid, 0, 0, 0, 0, 0, nullptr, 7))
    {
        throw Error("navigation occupancy check lost its allowed-record exception");
    }
    grid.cells[0].flags_11 = 0x80;
    if (navigation_cell_allowed(grid, 0, 1, 0, 0, 0, nullptr, 0))
    {
        throw Error("navigation marked-cell gate ignored the signed flag byte");
    }
    grid.cells[0].flags_11 = 1;
    grid.cells[0].flags_7 = 0;
    if (!navigation_cell_allowed(grid, 0, 0, 0, 1, 0, nullptr, 0) ||
        navigation_cell_allowed(grid, 0, 0, 0, 0, 0, nullptr, 0))
    {
        throw Error("navigation tagged-cell exception changed");
    }
    grid.cells[0].flags_11 = 0;
    grid.cells[0].region = 0x1001;
    if (navigation_cell_allowed(grid, 0, 0, 1, 0, 0, nullptr, 0))
    {
        throw Error("navigation region mode ignored closed-region bits");
    }
    grid.cells[0].region = 0;
    grid.cells[0].flags_7 = 4;
    auto* saved_grid0 = g_nav_grids[0];
    auto* saved_grid1 = g_nav_grids[1];
    const i32 saved_count = g_nav_count;
    const i32 saved_x = g_nav_origin_x;
    const i32 saved_z = g_nav_origin_z;
    const std::array<float, 2> saved_min{g_nav_min_y[0], g_nav_min_y[1]};
    const std::array<float, 2> saved_max{g_nav_max_y[0], g_nav_max_y[1]};
    g_nav_grids[0] = &grid;
    g_nav_origin_x = 0;
    g_nav_origin_z = 0;
    g_nav_count = 1;
    g_nav_min_y[0] = -1.f;
    g_nav_max_y[0] = 10.f;
    const Vec3 start{};
    const Vec3 end{40.f, 0.f, 0.f};
    i32 last = 99;
    grid.cells[4].flags_7 = 0; // Retail returns at the destination before checking cell eligibility.
    if (!navigation_path_clear(start, end, 0, 0, 0, 0, nullptr, &last, 0) || last != 99)
    {
        throw Error("navigation path checked target eligibility or wrote output on success");
    }
    grid.cells[2].flags_7 = 0;
    if (navigation_path_clear(start, end, 0, 0, 0, 0, nullptr, &last, 0) || last != 1)
    {
        throw Error("horizontal navigation failure lost its previous-cell output");
    }
    if (navigation_path_clear(end, start, 0, 0, 0, 0, nullptr, &last, 0) || last != 3)
    {
        throw Error("reverse horizontal navigation changed its rasterization");
    }
    grid.cells[2].flags_7 = 4;
    grid.cells[10].flags_7 = 0;
    if (navigation_path_clear(start, Vec3{0.f, 0.f, 40.f}, 0, 0, 0, 0, nullptr, &last, 0) || last != 5)
    {
        throw Error("vertical navigation changed its previous-cell output");
    }
    grid.cells[10].flags_7 = 4;
    grid.cells[12].flags_7 = 0;
    if (navigation_path_clear(start, Vec3{40.f, 0.f, 40.f}, 0, 0, 0, 0, nullptr, &last, 0) || last != 6)
    {
        throw Error("diagonal navigation changed its error accumulator");
    }
    grid.cells[12].flags_7 = 4;
    records[1].center.x = 20.f;
    last = 99;
    if (navigation_path_clear(start, end, 0, 0, 1, 0, nullptr, &last, 7) || last != 1 ||
        !navigation_path_clear(start, end, 0, 0, 1, 0, &records[1], &last, 7))
    {
        throw Error("path walker lost its occupancy flag or allowed-record argument");
    }
    grid.cells[2].flags_11 = 0x80;
    if (navigation_path_clear(start, end, 0, 0, 0, 1, nullptr, &last, 0) || last != 1)
    {
        throw Error("path walker lost its marked-cell flag argument");
    }
    grid.cells[2].flags_11 = 0;
    grid.cells[1].connections = 0;
    last = 99;
    if (navigation_path_clear(start, end, 0, 0, 0, 0, nullptr, &last, 0) || last != 99)
    {
        throw Error("navigation connection failure wrote a cell-eligibility output");
    }
    if (!navigation_path_clear(start, start, 0, 0, 1, 1, nullptr, &last, 0))
    {
        throw Error("same-cell path lost the retail early return");
    }
    g_nav_count = 2;
    g_nav_min_y[1] = -1.f;
    g_nav_max_y[1] = 20.f;
    if (navigation_floor_index(start, -1, 0) != 1 || navigation_floor_index(start, 0, 0) != 0)
    {
        throw Error("floor selection lost preferred-floor versus last-lower-minimum behavior");
    }
    NavGrid second_grid = grid;
    g_nav_grids[1] = &second_grid;
    g_nav_min_y[1] = 20.f;
    grid.cells[0].flags_11 = 0x40;
    second_grid.cells[0].flags_11 = 0x40;
    if (!navigation_path_clear(start, Vec3{0.f, 30.f, 0.f}, 0, 0, 0, 0, nullptr, &last, 0))
    {
        throw Error("cross-floor navigation lost the paired transfer flags");
    }
    second_grid.cells[0].flags_11 = 0;
    if (navigation_path_clear(start, Vec3{0.f, 30.f, 0.f}, 0, 0, 0, 0, nullptr, &last, 0))
    {
        throw Error("cross-floor navigation accepted an unmarked destination");
    }
    g_nav_grids[0] = saved_grid0;
    g_nav_grids[1] = saved_grid1;
    g_nav_count = saved_count;
    g_nav_origin_x = saved_x;
    g_nav_origin_z = saved_z;
    for (size_t index = 0; index < 2; ++index)
    {
        g_nav_min_y[index] = saved_min[index];
        g_nav_max_y[index] = saved_max[index];
    }
    g_occupancy_pool = saved_pool;
    g_occupancy_mask = saved_mask;
}

void check_native_audio_preparation(const char* directory)
{
    using namespace gbtg;
    Game game;
    game.aids.open(directory);
    u64 handle = 123;
    void* voice = &game;
    if (pcm_prepare_owned_cue(game, "XACT_SOUNDBANK_MISSING_MISSING", handle, voice) >= 0 ||
        handle != 0 || voice != nullptr)
    {
        throw Error("native audio preparation reported success for an absent cue");
    }
    const BundleEntry* list = find_entry(game.aids.common, "aid_xcuelist_ghoulies_default");
    if (list == nullptr)
    {
        throw Error("native audio preparation requires the retail cue list");
    }
    const std::string text(reinterpret_cast<const char*>(list->cpu.data()), list->cpu.size());
    size_t at = 0;
    i32 last_error = 0;
    while (at < text.size())
    {
        const size_t end = text.find('\n', at);
        std::string row = text.substr(at, end == std::string::npos ? text.size() - at : end - at);
        at = end == std::string::npos ? text.size() : end + 1;
        if (!row.empty() && row.back() == '\r')
        {
            row.pop_back();
        }
        const size_t tab = row.find('\t');
        if (tab == std::string::npos)
        {
            continue;
        }
        std::string cue = "XACT_SOUNDBANK_" + row.substr(0, tab) + "_" + row.substr(tab + 1);
        for (char& value : cue)
        {
            if (value >= 'a' && value <= 'z')
            {
                value -= 32;
            }
        }
        last_error = pcm_prepare_owned_cue(game, cue.c_str(), handle, voice);
        if (last_error < 0)
        {
            continue;
        }
#if defined(_WIN32)
        XAUDIO2_VOICE_STATE state{};
        static_cast<IXAudio2SourceVoice*>(voice)->GetState(&state);
        if (handle == 0 || state.BuffersQueued != 1 || state.SamplesPlayed != 0)
        {
            throw Error("native prepared cue was missing its buffer or started before PlayEx");
        }
#endif
        auto boundaries = native_audio_core_cue_boundaries(game,
            [&](void*, u32) -> const char*
            {
                return cue.c_str();
            });
        auto listener = native_audio_listener_settings();
        const std::array<u32, 12> parameters{
            std::bit_cast<u32>(i32(-1000)), std::bit_cast<u32>(i32(-100)),
            std::bit_cast<u32>(0.f), std::bit_cast<u32>(1.49f), std::bit_cast<u32>(0.83f),
            std::bit_cast<u32>(i32(-2602)), std::bit_cast<u32>(0.007f), u32(200),
            std::bit_cast<u32>(0.011f), std::bit_cast<u32>(100.f),
            std::bit_cast<u32>(100.f), std::bit_cast<u32>(5000.f)};
        const i32 reverb_result = listener.listener(parameters, 0);
        const i32 volume_result = listener.master_volume(0xFF, -301);
        if (reverb_result < 0 || volume_result < 0)
        {
            pcm_stop_owned_cue(handle);
            throw Error("native dialogue listener failed actual DSP or category volume setup: " +
                std::to_string(reverb_result) + ", " + std::to_string(volume_result));
        }
#if defined(_WIN32)
        float actual_volume;
        static_cast<IXAudio2SourceVoice*>(voice)->GetVolume(&actual_volume);
        if (std::abs(actual_volume - std::pow(10.f, -301.f / 2000.f)) > 0.00001f)
        {
            pcm_stop_owned_cue(handle);
            throw Error("native category volume did not reach the prepared source voice");
        }
#endif
        listener.master_volume(0xFF, 0);
        if (boundaries.play(nullptr, nullptr, voice) < 0)
        {
            pcm_stop_owned_cue(handle);
            throw Error("native prepared cue failed its actual PlayEx boundary");
        }
        pcm_stop_owned_cue(handle);
        if (pcm_owned_cue_playing(handle))
        {
            throw Error("native audio release retained a source voice");
        }
        std::cout << "PASS: retail cue prepared without starting, played and released: " << cue << '\n';
        return;
    }
    throw Error("native audio could not prepare a retail PCM cue, final HRESULT " + std::to_string(last_error));
}

void check_spawn_receiver_dependencies()
{
    using namespace gbtg;
    Actor actor;
    Actor source;
    ActorAnimationState animation;
    RuntimeActorStartupState state{actor, animation};
    ActorAttachmentNode pending{};
    state.attachments.pending = &pending; // Actor +0x1B4.
    ActorLink link;
    link.actor = &actor;
    actor.link = &link;
    AnimationResourceRegistry registry;
    const std::array<u32, 2> terminator{8, 0};
    AnimationBundleRecord resource;
    resource.name = "aid_ghoulybox_receiver_test";
    resource.cpu = std::span(reinterpret_cast<const u8*>(terminator.data()), sizeof(terminator));
    std::array<AnimationBundleRecord, 1> records{resource};
    std::array<AnimationBundlePool, 1> pools{AnimationBundlePool{records, true}};
    register_animation_resource_provider(registry,
        [&](u32, const AnimationResourceRecord& entry, const void*& resolved)
        {
            return find_bundle_resource_provider(pools, entry.name.c_str(), resolved);
        },
        [&](u32, const void* resolved, AnimationLoadedResource& loaded)
        {
            return load_bundle_resource_provider(resolved, loaded);
        },
        [&](u32, const char* name, const AnimationLoadedResource&)
        {
            unload_bundle_resource_provider(pools, name);
        }, 3, nullptr);
    ResourceErrorState error{};
    RuntimeModelResourceViews models;
    RuntimeModelDeferredPool deferred{};
    RuntimeShaderRegistrationState shaders{};
    RuntimeShaderAnimationScratch scratch{};
    const std::function<void(u8*)> heap_free = [](u8* bytes)
    {
        delete[] bytes;
    };
    u32 fence = 0;
    u32 random = 0;
    u32 flags = 0;
    u32 player = 0;
    ActorEffectModelContext model_context{registry, error, models, deferred, fence, {}, shaders, scratch, heap_free};
    RuntimeActorSoundQueues sounds;
    Vec3 origin{};
    Vec3 euler{};
    ActorEffectStreamContext effects{model_context, sounds, random, origin, euler};
    ObjectFactoryRegistry factory{};
    std::vector<FactoryObject> objects;
    float pause = 0.f;
    float delta = 0.f;
    float scale = 1.f;
    float timer = 99.f;
    i32 pal = 0;
    i32 pal_override = 0;
    ActorTrailContext trails{factory, objects, 0x10000, pause, delta, scale, pal, pal_override, ""};
    ActorScareContext scares{factory, {}, objects, flags, [&player]() { return player; }, euler};
    AudioCoreSources audio_sources{};
    AudioCoreCues audio_cues{audio_sources};
    AudioEventVoices voices{audio_cues, {}, {}, {}, 0, origin, euler};
    ActorEventContext events{state, effects, trails, scares, voices, {}, timer, scale, scale};
    entity_detail::ShapeProbe shape;
    entity_detail::SegmentProbe segment;
    ActorRenderRegistration render{actor, shape, segment, state.live};
    const std::unordered_map<const u8*, const u8*> relocated;
    ActorAttachmentEventContext callouts{actor, state.attachments, effects, factory, {}, objects, 0x10000, relocated};
    entity_detail::ListHead anchors;
    entity_detail::ListHead live_attachments;
    ActorLink world_bind;
    void* watched = &source;
    ModelAnimationState pose;
    ModelNodeFlagCache cache{};
    AnimationMatrix reset{};
    ActorBaseContext base{events, render, callouts, anchors, live_attachments, world_bind, watched, pose, nullptr, cache, reset};
    std::array<u8, 0x18> event{};
    u16 kind = 0x25;
    std::memcpy(event.data() + 0xC, &kind, 2);
    ActorAnimationMessage animation_message{7, 0.f, 0, event, 0};
    dispatch_base_actor_message(&animation_message, base);
    if (timer != 0.f)
    {
        throw Error("base actor receiver did not route its animation event");
    }
    ActorVectorMessage position{0xC, Vec3{7.f, 8.f, 9.f}};
    dispatch_base_actor_message(&position, base);
    if (actor.position.x != 7.f || actor.xform_valid != 0)
    {
        throw Error("base actor receiver did not invalidate its moved actor transform");
    }
    ActorReferenceMessage released{0x7C, &source};
    dispatch_base_actor_message(&released, base);
    if (watched != nullptr)
    {
        throw Error("base actor receiver did not release its watched actor");
    }
    {
        // Player dispatch 0x00028F60/0x00028FC0 through the same base receiver; ids 0xA9, 0xF,
        // 0x2D and 9 are ignored by 0x0003EF70.
        std::vector<u8> player_bytes(0xF00);
        std::vector<std::vector<u32>> self_messages;
        RuntimeBodyAttachedActor self{player_bytes, 0x4000, [&](std::span<const u32> message)
            {
                self_messages.emplace_back(message.begin(), message.end());
            }};
        Actor player_actor;
        std::span<const u8> attributes;
        std::array<u8, 1> bit_bytes{};
        EntityBitBlock bits{bit_bytes.data()};
        EntityBitBlock* body_bits = &bits;
        GhoulyController spawn_controller;
        RuntimePlayerSpawnController spawn{nullptr, 0, spawn_controller};
        std::array<entity_detail::ListHead, 4> links{};
        entity_detail::WorldLists world{};
        i32 excluded_count = 0;
        const OccupancyRecord* room = nullptr;
        const OccupancyRecord* allowed = nullptr;
        RuntimePlayerGoalRing* ring = nullptr;
        std::array<u8, 2> zero{};
        u32 off = 0;
        RuntimePlayerScriptOverrides overrides{off, off, off, off, off, off, off, off};
        RuntimePlayerActivation activation{self, player_actor, links, world, [](const void*) -> u32
            {
                return 0;
            },
            excluded_count, nullptr, room, allowed, ring, zero, overrides};
        FactoryObject strategy;
        FactoryObject mind;
        FactoryObject body;
        RuntimePlayerChildContext child{factory, {}, 0x4000};
        RuntimePlayerMindRequest* first_request = nullptr;
        RuntimePlayerMindRequests requests{first_request, [](RuntimePlayerMindRequest&)
            {
            }};
        RuntimePlayerMindRequests* request_view = &requests;
        RuntimePlayerStrategySetup setup{child, request_view, [](FactoryObject&, u16, u16)
            {
            },
            [](FactoryObject&, u16, u16)
            {
            }};
        RuntimeScriptActorQueryContext query{[] { return std::vector<RuntimeScriptActorQueryView>{}; },
            zero.data()}; // An empty DAT_00511B38.
        std::vector<u8> body_storage(0x950);
        std::span<u8> body_view = body_storage;
        entity_detail::LatchNode* latch = nullptr;
        RuntimePlayerGoalCleanup goals{self, ring, body_view, player_actor, latch, zero, overrides};
        std::array<u32, 2> references{};
        std::array<u8, 1> decay{1};
        u32 decay_index = 0;
        RuntimePlayerStrategyResume resume{references, decay, decay_index};
        RuntimePlayerPause pause{player_actor, strategy, setup, query, goals, resume};
        entity_detail::NavigationCellRegistration* records = nullptr;
        RuntimePlayerNavigationRegistration navigation{body_storage, player_actor, records};
        RuntimePlayerMessageContext player{self, base, nullptr, attributes, body_bits, spawn, activation, pause, navigation};
        std::vector<std::pair<FactoryObject*, u32>> forwarded;
        RuntimePlayerChildMessages children{strategy, mind, body, [&](FactoryObject& object, void* message)
            {
                forwarded.emplace_back(&object, *static_cast<const u32*>(message));
            }};
        strategy.bytes = std::make_unique<u8[]>(0x10);
        strategy.size = 0x10;
        body.bytes = std::make_unique<u8[]>(0x10);
        body.size = 0x10;
        GhoulyVariantMessage variant;
        std::strcpy(variant.name, "variant");
        dispatch_runtime_player_message(&variant, player, children);
        if (variant.result != 1 || gbtg::player_record(std::span<gbtg::u8>(player_bytes)).ghouly_kind != 1 || forwarded.size() != 2
            || forwarded[0].first != &strategy
            || forwarded[1].first != &body || forwarded[1].second != 0xA9)
        {
            throw Error("player dispatch must handle 0xA9, then forward to the present strategy and body only");
        }
        u32 bit_message = 0x2D;
        dispatch_runtime_player_message(&bit_message, player, children);
        u32 pause_message = 9;
        dispatch_runtime_player_message(&pause_message, player, children);
        if (bit_bytes[0] != 2 || gbtg::player_record(std::span<gbtg::u8>(player_bytes)).pause_count != 1
            || player_actor.parked_gate != 1 || forwarded.size() != 6)
        {
            throw Error("player dispatch must set the body bit, count pauses and forward each message");
        }
        RuntimeMessageReceiver sender{0x5000, [&](std::span<const u32> message)
            {
                self_messages.emplace_back(message.begin(), message.end());
            }};
        RuntimePlayerContactMessage contact{0xF, &sender, {}, 0};
        dispatch_runtime_player_message(&contact, player, children);
        if (self_messages.size() != 2 || self_messages[0] != std::vector<u32>{0x2D, 0x4000, 0x5000}
            || self_messages[1] != std::vector<u32>{0x2D, 0x5000, 0x4000})
        {
            throw Error("player contact acknowledgement must notify self then sender");
        }
    }
}

void check_actor_loading()
{
    check_camera_display();
    check_script_actor_query();
    check_scene_render();
    {
        using namespace gbtg;
        u32 calls = 0;
        const auto require = [](bool condition, const char* message)
        {
            if (!condition)
            {
                throw Error(message);
            }
        };
        RuntimeSpecialTextures hooks{};
        hooks[1].get_texture = [&calls, &require](u32 first, u32 second)
        {
            ++calls;
            require(first == 17 && second == 29, "dialogue target callback arguments");
            return u32{41};
        };
        hooks[1].model = 17;
        hooks[1].index = 29;
        require(invoke_dialogue_render_target(0, hooks) == 0 && calls == 0,
            "absent dialogue target hook");
        require(invoke_dialogue_render_target(1, hooks) == 41 && calls == 1,
            "dialogue target callback result");
        std::array<RuntimeDialogueQueuedSprite, 2> records{};
        i32 count = 0;
        RuntimeDialogueSpriteQueue queue{records, count};
        const float position[2]{12.f, -3.f};
        const float scale[2]{2.f, 4.f};
        const u8 texture[1]{};
        queue_runtime_dialogue_sprite(scale, position, texture, 0x12345678,
            255, -0.25f, queue);
        require(count == 1 && records[0].texture == texture &&
            records[0].position == std::array<float, 2>{12.f, -3.f} &&
            records[0].scale == std::array<float, 2>{2.f, 4.f} &&
            records[0].angle == -0.25f && records[0].color == 0x12345678 &&
            records[0].frame == 255, "dialogue sprite queue payload");
        const std::array<u32, 4> viewport{10, 20, 640, 480};
        std::array<float, 8> corners{};
        dialogue_viewport_corners(corners.data(), viewport);
        require(corners == std::array<float, 8>{10.f, 20.f, 10.f, 500.f,
            650.f, 500.f, 650.f, 20.f}, "dialogue viewport corner order");
    }
    {
        using namespace gbtg;
        AnimationResourceRegistry resources;
        ResourceErrorState error{};
        RuntimeModelResourceViews models;
        AnimationTableResourceViews tables;
        std::unordered_map<const u8*, ActorModel> model_views;
        RuntimeModelDeferredPool deferred{};
        u32 fence = 0;
        RuntimeShaderRegistrationState shaders{};
        RuntimeShaderAnimationScratch scratch{};
        const std::function<void(u8*)> free = [](u8* allocation)
        {
            engine_heap_free(allocation);
        };
        RuntimeActorLink live{};
        u32 random = 0;
        RuntimeActorStartupContext startup{resources, error, models, tables,
            model_views, deferred, fence, {}, shaders, scratch, free, live, random};
        std::array<ObjectFactoryType, 0x1B> types{};
        ObjectFactoryRegistry factory{types};
        RuntimeActorSoundQueues sounds{};
        std::unordered_map<ActorFrameAttachment*, const u8*> frames;
        const char empty[4]{};
        NativeActorFactoryOwner owner{NativeActorFactoryBindings{startup, factory,
            sounds, frames,
            [](Actor&, const void*)
            {
                throw Error("bare actor unexpectedly sent an attachment message");
            }, empty}};
        owner.install(types);
        std::array<u8, 0x340> parameters{};
        const u16 kind = 0x15;
        const u16 tag = 7;
        const float x = 12.f;
        const float y = -7.f;
        const float duration = 0.4f;
        std::memcpy(parameters.data() + 4, &kind, sizeof(kind));
        std::memcpy(parameters.data() + 2, &tag, sizeof(tag));
        std::memcpy(parameters.data() + 0x334, &x, sizeof(x));
        std::memcpy(parameters.data() + 0x338, &y, sizeof(y));
        std::memcpy(parameters.data() + 0x33C, &duration, sizeof(duration));
        auto object = create_factory_object(factory, parameters);
        auto& native = owner.view(object);
        if (factory.count != 1 || object.size != 0x958 ||
            owner.actors.at(object.bytes.get()) != &native.bat.actor ||
            native.bat.actor.class_key != tag || native.bat.state != 1 || native.bat.current.x != x ||
            native.bat.current.y != y || native.bat.flight_duration != duration ||
            native.bat.idle_timer != 4.5f || live.next != &native.startup.live)
        {
            throw Error("native button-bat factory lost its parent constructor or instance mapping");
        }
        release_factory_object(factory, object);
        if (factory.count != 0 || object.bytes != nullptr || live.next != nullptr ||
            !owner.actors.empty() || !owner.animations.empty() || !owner.render.empty())
        {
            throw Error("native actor factory release retained its list or pointer mappings");
        }
    }
    check_spawn_receiver_dependencies();
    using namespace gbtg;
    {
        const MovieErrorMessage messages[]{
            {2, "Resource", "Missing"}, {110, "Movie", "Open failed"}, {0, nullptr, nullptr}};
        char fallback[16];
        std::memset(fallback, 0xA5, sizeof(fallback));
        ResourceErrorState error{};
        error.suppressed = 1;
        MovieErrorContext context{messages, fallback, error};
        char path[256];
        std::vector<std::string> queried;
        std::vector<u32> attributes{0, 0};
        size_t probe = 0;
        u32 last_error_reads = 0;
        MoviePathContext paths{1, 0, "d:\\", path, context,
            [&](const char* name)
            {
                queried.emplace_back(name);
                return attributes.at(probe++);
            },
            [&]()
            {
                ++last_error_reads;
                return 2u;
            }};
        const char* name = "aid_movie_ghoulies_test";
        const std::string standard = "d:\\movie\\" + aid_hash_hex(animation_resource_hash(name));
        const std::string pal = "d:\\movie\\" + aid_hash_hex(animation_resource_hash("aid_movie_ghoulies_testpal"));
        if (resolve_runtime_movie_path(name, paths) != path || queried != std::vector<std::string>{pal, pal}
            || path != pal || last_error_reads != 0)
        {
            throw Error("PAL movie resolution lost the successful candidate's second validation");
        }
        probe = 0;
        queried.clear();
        attributes = {0xFFFFFFFF, 0};
        resolve_runtime_movie_path(name, paths);
        if (queried != std::vector<std::string>{pal, standard} || path != standard || last_error_reads != 1)
        {
            throw Error("missing PAL movie did not fall back to the standard path");
        }
        probe = 0;
        queried.clear();
        paths.pal_override = 1;
        attributes = {0x10};
        resolve_runtime_movie_path(name, paths);
        if (queried != std::vector<std::string>{standard} || std::strcmp(fallback, "267") != 0)
        {
            throw Error("movie directory or PAL override lost its retail error result");
        }
        u32 play = 0xFFFFFFFF;
        std::array<RuntimeMovieEventSlot, 4> slots{
            RuntimeMovieEventSlot{nullptr, 7, 2}, RuntimeMovieEventSlot{&play, 7, 1},
            RuntimeMovieEventSlot{nullptr, 8, 0}, RuntimeMovieEventSlot{nullptr, 9, 0}};
        start_runtime_movie_event(7, 2, slots);
        if (play != 0 || slots[1].state != 2 || slots[0].state != 2)
        {
            throw Error("movie event start did not skip inactive IDs or wrap its play count");
        }
        slots[1].state = 1;
        start_runtime_movie_event(7, 0, slots);
        start_runtime_movie_event(99, 2, slots);
        if (play != 0 || slots[1].state != 3)
        {
            throw Error("movie event retirement or missing-ID lookup changed playback");
        }
        const char* title = nullptr;
        const char* message = nullptr;
        std::memset(fallback, 0xA5, sizeof(fallback));
        lookup_movie_error_message(110, title, message, context);
        if (title != messages[1].title || message != messages[1].message
            || static_cast<u8>(fallback[0]) != 0xA5)
        {
            throw Error("movie error table lookup changed row pointers or fallback storage");
        }
        if (report_movie_error(-2147483647, context) != -2147483647
            || std::strcmp(fallback, "-2147483647") != 0
            || static_cast<u8>(fallback[15]) != 0xA5)
        {
            throw Error("movie failure reporting lost its code or bounded fallback message");
        }
        const MovieErrorMessage empty[]{MovieErrorMessage{0, nullptr, nullptr}};
        MovieErrorContext empty_context{empty, fallback, error};
        lookup_movie_error_message(0, title, message, empty_context);
        if (std::strcmp(title, "Error Code") != 0 || message != fallback
            || std::strcmp(message, "0") != 0)
        {
            throw Error("empty movie error table did not use its retail numeric fallback");
        }
        char truncated[5];
        std::memset(truncated, 0xA5, sizeof(truncated));
        if (scene_detail::crt_format_bounded_string(truncated, 4, "%s", "abcdef") != -1
            || std::memcmp(truncated, "abcd", 4) != 0 || static_cast<u8>(truncated[4]) != 0xA5)
        {
            throw Error("bounded variadic formatting changed retail truncation semantics");
        }
    }
    {
        entity_detail::ListHead sentinel{};
        RuntimeActorLink first{};
        RuntimeActorLink last{};
        first.previous = reinterpret_cast<RuntimeActorLink*>(&sentinel);
        first.next = &last;
        last.previous = &first;
        sentinel.next = &first;
        unlink_actor_view(first);
        if (sentinel.next != &last || last.previous != reinterpret_cast<RuntimeActorLink*>(&sentinel)
            || first.previous != nullptr || first.next != nullptr)
        {
            throw Error("actor view removal did not reconnect the native sentinel and successor");
        }
        unlink_actor_view(last);
        if (sentinel.next != nullptr || last.previous != nullptr || last.next != nullptr)
        {
            throw Error("last actor view removal did not empty the list");
        }
        unlink_actor_view(last);
    }
    {
        std::array<u8, 0xF00> owner{};
        char replacement_source[128];
        char replacement_target[128];
        std::memset(replacement_source, 0xA5, sizeof(replacement_source));
        std::memset(replacement_target, 0xB6, sizeof(replacement_target));
        set_cutscene_actor_replacement(replacement_source, replacement_target,
            "aid_model_ghoulies_actor_dialogbat", "aid_model_ghoulies_actor_dialogbatgold");
        if (std::strcmp(replacement_source, "aid_model_ghoulies_actor_dialogbat") != 0
            || std::strcmp(replacement_target, "aid_model_ghoulies_actor_dialogbatgold") != 0
            || static_cast<u8>(replacement_source[127]) != 0xA5
            || static_cast<u8>(replacement_target[127]) != 0xB6)
        {
            throw Error("dialogue actor replacement changed its names or cleared untouched bytes");
        }
        set_cutscene_actor_replacement(replacement_source, replacement_target, "", "");
        if (replacement_source[0] != 0 || replacement_target[0] != 0)
        {
            throw Error("empty cutscene actor replacement did not write both terminators");
        }
        std::array<u8, 24> pool{};
        std::array<u8, 24> unknown{};
        const auto allocate_device = [](u32 kind, size_t size)
        {
            std::span<u8> result{static_cast<u8*>(engine_heap_allocate(static_cast<i32>(size))), size};
            std::memset(result.data(), 0, size);
            const u32 active = 1;
            std::memcpy(result.data(), &kind, 4);
            std::memcpy(result.data() + 4, &active, 4);
            return result;
        };
        auto simple = allocate_device(1, 0x80);
        auto nested = allocate_device(4, 24);
        auto composite = allocate_device(4, 24);
        auto actor_input = allocate_device(5, 0xBC);
        auto detached = allocate_device(5, 0xBC);
        const auto store = [](std::span<u8> bytes, size_t offset, u32 value)
        {
            std::memcpy(bytes.data() + offset, &value, 4);
        };
        store(actor_input, 8, 2);
        store(actor_input, 0xC, 10);
        store(actor_input, 0xB8, 11);
        store(owner, 0x960, 12);
        store(pool, 16, 12);
        store(detached, 8, 0xFFFFFFFF);
        store(nested, 8, 1);
        store(composite, 8, 2);
        store(composite, 12, 12);
        store(composite, 16, 3);
        store(composite, 20, 4);
        store(unknown, 0, 77);
        store(unknown, 4, 0x12345678);
        const std::unordered_map<u32, std::span<u8>> addresses{
            {1, simple}, {2, nested}, {3, unknown}, {4, detached},
            {10, owner}, {11, pool}, {12, actor_input}};
        const RuntimeInputReleaseContext context{addresses};
        release_runtime_input_device(composite, context);
        if (gbtg::player_record(std::span<gbtg::u8>(owner)).input_device != 0 || read_u32(pool, 16) != 0
            || read_u32(unknown, 4) != 0x12345678)
        {
            throw Error("recursive input release lost actor ownership or changed unknown device kinds");
        }
        release_runtime_input_child(unknown, context);
        release_runtime_input_device(unknown, context);
        if (read_u32(unknown, 4) != 0x12345678)
        {
            throw Error("unknown input release changed its active word");
        }
    }
    {
        const auto* saved_banks = g_audio_bank_names;
        AudioBankNames bank{};
        AudioCueName cue{};
        std::strcpy(bank.name.data(), "GFOOTSTEPWOOD");
        std::strcpy(cue.name.data(), "RUNLEFT");
        bank.cues = &cue;
        g_audio_bank_names = &bank;
        std::array<u8, 256> remap{};
        remap[2] = 1;
        const char* surfaces[]{"none", "wood", "stone"};
        FootstepSoundNames context{remap, surfaces};
        char name[128];
        format_footstep_sound_name(name, 2, "left", "run", context);
        if (std::strcmp(name, "XACT_SOUNDBANK_GFOOTSTEPWOOD_RUNLEFT") != 0)
        {
            throw Error("footstep cue selection lost surface remapping or suffix order");
        }
        format_footstep_sound_name(name, 1, "right", "run", context);
        if (std::strcmp(name, "XACT_SOUNDBANK_GFOOTSTEPWOOD_DEFAULT") != 0)
        {
            throw Error("missing footstep cue did not use its retail default name");
        }
        g_audio_bank_names = const_cast<AudioBankNames*>(saved_banks);
        using namespace entity_detail;
        auto* saved_grid = g_nav_grids[0];
        const i32 saved_x = g_nav_origin_x;
        const i32 saved_z = g_nav_origin_z;
        NavGrid grid;
        grid.width = 1;
        grid.depth = 1;
        grid.cell = 10;
        grid.cells.resize(1);
        g_nav_grids[0] = &grid;
        g_nav_origin_x = 0;
        g_nav_origin_z = 0;
        grid.cells[0].value_8 = 0x02010000;
        if (navigation_surface_kind(0, Vec3{5.f, 0.f, 5.f}) != 2)
        {
            throw Error("navigation surface query lost its override byte");
        }
        grid.cells[0].value_8 = 0x00010000;
        if (navigation_surface_kind(0, Vec3{5.f, 0.f, 5.f}) != 1
            || navigation_surface_kind(0, Vec3{50.f, 0.f, 50.f}) != 0)
        {
            throw Error("navigation surface query lost fallback or off-grid result");
        }
        g_nav_grids[0] = nullptr;
        if (navigation_surface_kind(0, Vec3{}) != 0)
        {
            throw Error("missing navigation grid invented a surface");
        }
        g_nav_grids[0] = saved_grid;
        g_nav_origin_x = saved_x;
        g_nav_origin_z = saved_z;
    }
    {
        alignas(8) std::array<u8, 0x100> analog;
        analog.fill(0xA6);
        const u32 analog_kind = 1;
        std::memcpy(analog.data(), &analog_kind, 4);
        const auto original = analog;
        reset_runtime_analog_input(analog);
        if (std::memcmp(analog.data() + 0x44, original.data() + 8, 0x3C) != 0
            || read_u32(analog, 0) != 1 || analog[4] != 0xA6 || analog[0x42] != 0xA6
            || read_u32(analog, 8) != 0 || read_u32(analog, 0x3C) != 0 || read_u16(analog, 0x40) != 0)
        {
            throw Error("analog input reset lost its prior snapshot or retained device bytes");
        }
        std::array<u8, 0xA8 * 4> controllers;
        for (i32 port = 0; port < 4; ++port)
        {
            std::memset(controllers.data() + port * 0xA8, 0x20 + port, 0xA8);
        }
        std::array<u8, 0x3C> keyboard;
        keyboard.fill(0xC5);
        const u32 ports[]{2, 4, 1, 8, 2}; // first word supplies the retail [-1] view
        std::array<u8, 0x50 * 3> rows{};
        std::array<u8, 0x100> selected;
        selected.fill(0xB7);
        const u32 selected_kind = 2;
        std::memcpy(selected.data(), &selected_kind, 4);
        std::array<u8, 0x100> port;
        port.fill(0xD8);
        const u32 port_kind = 5;
        const u32 port_index = 1;
        std::memcpy(port.data(), &port_kind, 4);
        std::memcpy(port.data() + 8, &port_index, 4);
        const std::unordered_map<u32, std::span<u8>> devices
        {
            {101, analog}, {102, selected}, {103, port}
        };
        GhoulySpawnPool pool{0x50, 1, rows.data(), rows.data() + rows.size(), rows.data() + rows.size()};
        i32 current = 2;
        RuntimeInputActivationContext context{pool, current, controllers, keyboard, ports + 1, devices};
        if (runtime_controller_mask_index(8) != 3 || runtime_controller_mask_index(3) != -1
            || runtime_player_mask_index(0x40) != 2 || runtime_player_mask_index(4) != -1
            || runtime_player_controller_index(0x20, ports + 1) != 0
            || runtime_input_state_for_mask(0x40, context) != controllers.data() + 3 * 0xA8
            || runtime_input_state_for_mask(0xFFFFFFFFu, context) != keyboard.data()
            || runtime_input_state_for_mask(0, context) != nullptr)
        {
            throw Error("input activation mask conversion or controller lookup changed");
        }
        const auto previous_selected = selected;
        reset_runtime_selected_input(selected, context);
        if (std::memcmp(selected.data() + 8, controllers.data() + 2 * 0xA8, 0x3C) != 0
            || std::memcmp(selected.data() + 0x44, previous_selected.data() + 8, 0x3C) != 0)
        {
            throw Error("selected input reset lost physical state or previous snapshot");
        }
        const auto previous_port = port;
        reset_runtime_port_input(port, context);
        if (std::memcmp(port.data() + 0x10, controllers.data(), 0x3C) != 0
            || std::memcmp(port.data() + 0x4C, previous_port.data() + 0x10, 0x3C) != 0)
        {
            throw Error("port input reset lost port mapping or previous snapshot");
        }
        const u32 invalid_port = 8;
        std::memcpy(port.data() + 8, &invalid_port, 4);
        if (runtime_input_port_mask(port, context) != 2)
        {
            throw Error("invalid port changed the retail preceding-mask read");
        }
        std::memcpy(port.data() + 8, &port_index, 4);
        const u32 ids[]{101, 102, 103};
        const char* names[]{"base", "dialogue", "later"};
        for (i32 index = 0; index < 3; ++index)
        {
            std::memcpy(rows.data() + index * 0x50, &ids[index], 4);
            std::strcpy(reinterpret_cast<char*>(rows.data() + index * 0x50 + 4), names[index]);
        }
        if (find_runtime_input_entry("BASE", context) != rows.data()
            || find_runtime_input_entry("missing", context) != nullptr)
        {
            throw Error("input stack name lookup lost retail folding or missing result");
        }
        activate_runtime_input_entry("dialogue", context);
        std::array<u8, 0xF00> player_actor{};
        const u32 player_device = 0x1234;
        gbtg::player_record(std::span<gbtg::u8>(player_actor)).input_device = player_device;
        const auto retained_rows = rows;
        destroy_runtime_player_mind(player_actor, 0x800, context);
        if (current != 2 || pool.cursor != rows.data() + rows.size() || rows != retained_rows)
        {
            throw Error("disabled player mind destruction changed the input stack");
        }
        player_actor.fill(0);
        destroy_runtime_player_mind(player_actor, 0, context);
        if (current != 2 || pool.cursor != rows.data() + rows.size() || rows != retained_rows)
        {
            throw Error("player mind without input changed the input stack");
        }
        gbtg::player_record(std::span<gbtg::u8>(player_actor)).input_device = player_device;
        destroy_runtime_player_mind(player_actor, 0x1000, context);
        if (current != 1 || pool.cursor != rows.data() + 2 * 0x50
            || read_u32(rows, 0x50) != 103 || read_u32(rows, 0x48) != 1
            || read_u32(rows, 0x98) != 0 || read_u32(analog, 8) != 0)
        {
            throw Error("input stack pop did not shift rows and reactivate its previous device");
        }
        current = 2;
        pop_runtime_input_entry(context);
        pop_runtime_input_entry(context);
        if (current != 0 || pool.cursor != pool.begin)
        {
            throw Error("final input stack pop retained a selected entry");
        }
        for (i32 offset = 0; offset < 8; ++offset)
        {
            std::array<u8, 0x100> copied;
            copied.fill(0xE9);
            copy_runtime_input_state(copied.data() + offset, keyboard.data());
            if (std::memcmp(copied.data() + offset, keyboard.data(), 0x3C) != 0
                || copied[offset + 0x3C] != 0xE9)
            {
                throw Error("input state copy changed unaligned data or trailing bytes");
            }
        }
        char name[128];
        format_sound_name(name, "Xact_%s_%d", "MixedCase", 7);
        if (std::strcmp(name, "XACT_MIXEDCASE_7") != 0)
        {
            throw Error("sound name formatter changed native formatting or uppercase conversion");
        }
    }
    {
        std::array<u8, 0x94> event{};
        const auto timing = [&](u16 kind, float start, float end, i32 ending,
            float current, float previous, i32 restricted, bool accepted, u32 expected)
        {
            std::memcpy(event.data() + 0xC, &kind, 2);
            std::memcpy(event.data() + 4, &start, 4);
            std::memcpy(event.data() + 8, &end, 4);
            u32 phase = 91;
            if (select_cutscene_event_phase(event.data(), ending, current, previous, restricted, phase) != accepted
                || phase != (accepted ? expected : 91))
            {
                throw Error("cutscene event timing changed phase or failure output");
            }
        };
        timing(0xC, 1.f, 0.f, 0, 1.f, 0.f, 0, false, 0);
        timing(0xC, 1.f, 0.f, 0, 1.01f, 1.f, 0, true, 0);
        timing(0xC, 1.f, 0.f, 0, 2.f, 1.01f, 0, false, 0);
        timing(0xC, 1.f, 3.f, 0, 2.f, 0.f, 0, true, 0);
        timing(0xC, 1.f, 3.f, 0, 2.f, 1.1f, 0, true, 2);
        timing(0xC, 1.f, 3.f, 0, 3.f, 2.f, 0, false, 0);
        timing(0xC, 1.f, 3.f, 0, 3.01f, 3.f, 0, true, 1);
        timing(0xC, 1.f, 3.f, 0, 4.f, 3.1f, 0, false, 0);
        timing(0xF, 1.f, 3.f, 1, 0.f, 0.f, 0, true, 3);
        timing(0xF, 1.f, 3.f, 1, 4.f, 4.f, 0, true, 3);
        timing(0xC, 1.f, 3.f, 1, 2.f, 0.f, 0, false, 0);
        timing(0xE, 1.f, 3.f, 0, 2.f, 0.f, 1, true, 0);
        timing(0xF, 1.f, 3.f, 1, 2.f, 0.f, 1, false, 0);
        const float nan = std::numeric_limits<float>::quiet_NaN();
        timing(0xC, 1.f, 0.f, 0, nan, 0.f, 0, false, 0);
        timing(0xC, 1.f, 0.f, 0, 2.f, nan, 0, false, 0);
        if (interpolate_clamped_range(3.f, 2.f, 1.f, 10.f, 20.f) != 15.f
            || interpolate_clamped_range(3.f, 4.f, 1.f, 10.f, 20.f) != 20.f
            || interpolate_clamped_range(3.f, 0.f, 1.f, 20.f, 10.f) != 20.f
            || interpolate_clamped_range(1.f, 0.f, 1.f, 20.f, 10.f) != 10.f
            || !std::isnan(interpolate_clamped_range(3.f, nan, 1.f, 10.f, 20.f)))
        {
            throw Error("cutscene range interpolation changed clamps or unordered values");
        }
        Actor actor;
        actor.position = {1.f, 2.f, 3.f};
        actor.euler = {4.f, 5.f, 6.f};
        const auto saved = g_actor_creation_options;
        g_actor_creation_options.scale = 17.f;
        g_actor_creation_options.retained.fill(0xA7);
        auto& options = cutscene_actor_effect_options(actor, &actor);
        if (options.flags != 0xD || options.position.x != 1.f || options.euler.z != 6.f
            || options.user != &actor || options.scale != 17.f || options.retained[0] != 0xA7)
        {
            throw Error("cutscene effect binding changed pose, flags or retained fields");
        }
        g_actor_creation_options = saved;
        u32 random = 0;
        float progress = 0.f;
        float retained = 0.f;
        float amount_word = 0.f;
        float scale = 10.f;
        i32 active = 0;
        CutsceneCameraImpulse impulse{progress, retained, amount_word, scale, active};
        begin_cutscene_camera_impulse(impulse, random);
        const float amount = (std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f) * 10.f;
        if (random != 0x3C6EF35Fu || impulse.active != 1 || impulse.progress != 1.f
            || impulse.retained != 1.f || impulse.amount != std::min(amount, 1.f))
        {
            throw Error("cutscene camera impulse lost shared random advancement or clamp");
        }
        impulse.scale = -10.f;
        begin_cutscene_camera_impulse(impulse, random);
        if (impulse.amount != std::bit_cast<float>(0x3D888889u))
        {
            throw Error("cutscene camera impulse lost its lower clamp");
        }
    }
    {
        AudioCoreSources sources{};
        AudioCoreSource source{};
        std::array<u8, 0x40> attributes{};
        source.attributes = attributes.data();
        source.handle = &source;
        sources.sources.next = &source;
        sources.unit_scale = 0.5f;
        sources.apply = 7;
        sources.boundaries.bad_pointer = [](const void* pointer)
        {
            return pointer == nullptr;
        };
        i32 positioned = 0;
        sources.boundaries.position = [&](void* handle, const Vec3& position, u32 apply)
        {
            if (handle != &source || apply != 7 || position.x != 4.5f
                || position.y != -1.f || position.z != 2.5f)
            {
                throw Error("spatial voice positioning changed native boundary arguments");
            }
            ++positioned;
        };
        AudioCoreCues cues{sources};
        Vec3 origin{};
        AudioEventVoices voices{cues, {}, {}, {}, 0, origin, origin};
        const float listener[]{1.f, 2.f, 3.f, 0.f, 0.f, 0.f, 0.f, 2.f, -1.f, 0.5f};
        voices.listener = listener;
        voices.voices[0].source = &source;
        position_audio_event_voice_ahead(0, 4.f, voices);
        Vec3 stored;
        std::memcpy(&stored, attributes.data(), sizeof(stored));
        if (positioned != 1 || voices.voices[0].word_c != 1
            || std::bit_cast<float>(voices.voices[0].retained_14) != 4.f
            || stored.x != 9.f || stored.y != -2.f || stored.z != 5.f)
        {
            throw Error("spatial voice positioning lost listener-relative source state");
        }
        voices.voices[1].word_c = 9;
        position_audio_event_voice_ahead(1, 4.f, voices);
        if (positioned != 1 || voices.voices[1].word_c != 9)
        {
            throw Error("missing spatial source changed voice state");
        }
        for (auto& voice : voices.voices)
        {
            voice.state = 1;
        }
        if (request_spatial_audio_event_voice("missing", 1, 0.f, voices) != -1)
        {
            throw Error("spatial voice exhaustion invented a request result");
        }
        std::array<u8, 0x94> event{};
        const i32 slot = 1;
        std::memcpy(event.data() + 0x10, &slot, 4);
        std::array<u8, 264> state{};
        const auto saved = g_actor_creation_options;
        g_actor_creation_options.flags = 0xFF;
        dispatch_cutscene_voice_event(event.data(), state, 0, voices);
        if (read_u16(state, 2) != 0xFFFF || g_actor_creation_options.flags != 0)
        {
            throw Error("cutscene voice allocation failure lost slot retirement or flags");
        }
        AudioCoreCue cue{};
        voices.voices[0].cue = &cue;
        voices.voices[0].state = 2;
        const i16 index = 0;
        std::memcpy(state.data() + 2, &index, 2);
        dispatch_cutscene_voice_event(event.data(), state, 2, voices);
        if (voices.voices[0].state != 2 || read_u16(state, 2) != 0)
        {
            throw Error("intermediate cutscene phase stopped its voice");
        }
        dispatch_cutscene_voice_event(event.data(), state, 1, voices);
        if (voices.voices[0].state != 4 || read_u16(state, 2) != 0xFFFF)
        {
            throw Error("cutscene voice end did not retire its prepared cue");
        }
        g_actor_creation_options = saved;
    }
    {
        AudioCoreSources sources{};
        AudioCoreCues cues{sources};
        Vec3 origin{};
        AudioEventVoices voices{cues, {}, {}, {}, 0, origin, origin};
        AudioBankStreams audio{{}, {}, {}, voices};
        for (auto& channel : audio.channels)
        {
            channel.voice = -1;
        }
        AudioCoreCue cue{};
        voices.voices[0].cue = &cue;
        voices.voices[0].state = 2;
        audio.channels[0].voice = 0;
        audio.channels[0].mode = 2;
        std::strcpy(audio.channels[0].name.data(), "background");
        auto& background = audio.banks[2];
        initialize_tracked_audio_bank(background, 3, "background");
        background.channel = 0x11223344;
        auto& dialogue = audio.banks[6];
        initialize_tracked_audio_bank(dialogue, 7, "dialogue");
        audio.channels[1].voice = 1;
        audio.channels[1].mode = 3;
        std::strcpy(audio.channels[1].name.data(), "dialogue");
        voices.voices[1].cue = &cue;
        voices.voices[1].state = 3;
        if (!tracked_audio_bank_has_priority(dialogue, audio)
            || tracked_audio_bank_has_priority(background, audio))
        {
            throw Error("bank acquisition lost retail priority comparisons");
        }
        pause_lower_priority_audio_banks(dialogue, audio);
        start_tracked_audio_bank(dialogue, audio);
        if (background.pending != 1 || voices.voices[0].state != 3
            || audio.channels[0].mode != 3 || voices.voices[1].state != 2
            || dialogue.retained_90 != 1 || audio.channels[1].mode != 2)
        {
            throw Error("new bank did not pause background and resume its existing channel");
        }
        start_tracked_audio_bank(background, audio);
        if (background.channel != 0x11223300)
        {
            throw Error("bank channel assignment overwrote retained upper bytes");
        }
        acquire_tracked_audio_bank("dialogue", 99, audio);
        if (dialogue.references != 2 || dialogue.id != 7)
        {
            throw Error("repeated bank acquisition changed priority instead of sharing ownership");
        }
        std::array<u8, 264> state{};
        replace_cutscene_bank(state, "XACT_SILENCE", audio);
        auto* silent = find_tracked_audio_bank("XACT_SILENCE", audio);
        if (silent == nullptr || silent->silence != 1 || silent->references != 1
            || silent->id != 7 || silent->retained_90 != 1)
        {
            throw Error("silent dialogue bank did not use the retail acquisition path");
        }
        replace_cutscene_bank(state, "", audio);
        if (silent->id != 0 || state[6] != 0)
        {
            throw Error("empty bank replacement retained the previous bank reference");
        }
        audio.channels[2].voice = 2;
        audio.channels[2].mode = 3;
        std::strcpy(audio.channels[2].name.data(), "incoming");
        voices.voices[2].cue = &cue;
        voices.voices[2].state = 3;
        acquire_tracked_audio_bank("incoming", 9, audio);
        auto* incoming = find_tracked_audio_bank("incoming", audio);
        if (incoming == nullptr || incoming->references != 1 || incoming->pending != 0
            || audio.channels[2].mode != 2 || voices.voices[2].state != 2
            || background.pending != 1 || dialogue.pending != 1
            || voices.voices[1].state != 3)
        {
            throw Error("new bank acquisition did not apply its priority and start channel");
        }
        release_tracked_audio_bank("incoming", audio);
        if (incoming->id != 0 || voices.voices[2].state != 4
            || audio.channels[2].voice != -1 || dialogue.pending != 0
            || voices.voices[1].state != 2 || background.pending != 1)
        {
            throw Error("acquired bank release did not resume the highest remaining priority");
        }
        for (auto& voice : voices.voices)
        {
            voice.state = 1;
        }
        i32 existing = 71;
        if (acquire_tracked_audio_channel("unallocated", &existing, audio) != -1
            || existing != 71 || audio.channels[15].voice != -1
            || audio.channels[15].name[0] != 0)
        {
            throw Error("voice exhaustion invented a channel or wrote the output argument");
        }
        audio.banks[0].channel = 0x12345678;
        acquire_tracked_audio_bank("exhausted-bank", 0x12, audio);
        if (audio.banks[0].id != 0x12 || audio.banks[0].channel != 0x123456FF
            || audio.banks[0].retained_90 != 1)
        {
            throw Error("bank acquisition lost the actual channel allocation failure");
        }
        for (auto& bank : audio.banks)
        {
            bank.id = 1;
        }
        if (allocate_tracked_audio_bank(7, "full", audio) != nullptr)
        {
            throw Error("full bank tracker invented free storage");
        }
        for (auto& channel : audio.channels)
        {
            channel.voice = 0;
            std::strcpy(channel.name.data(), "occupied");
        }
        if (acquire_tracked_audio_channel("full", &existing, audio) != -1 || existing != 71)
        {
            throw Error("full channel tracker invented a channel");
        }
    }
    {
        AudioCoreSources sources{};
        AudioCoreCues cues{sources};
        AudioCoreBank core_bank{};
        AudioCoreCue cue{};
        cue.runtime.bank = &core_bank.runtime;
        cue.runtime.handle = &cue;
        cue.runtime.flags = 1;
        i32 stopped = 0;
        cues.sound.stop_cue = [&](void*, u32, u32 flags, void* handle)
        {
            if (flags != 0 || handle != &cue)
            {
                throw Error("bank release changed native cue stop arguments");
            }
            ++stopped;
        };
        Vec3 origin{};
        AudioEventVoices voices{cues, {}, {}, {}, 0, origin, origin};
        AudioBankStreams audio{{}, {}, {}, voices};
        for (auto& channel : audio.channels)
        {
            channel.voice = -1;
        }
        auto& background = audio.banks[2];
        background.id = 3;
        background.references = 1;
        background.pending = 1;
        std::strcpy(background.name.data(), "background");
        audio.channels[0].voice = 0;
        audio.channels[0].mode = 3;
        std::strcpy(audio.channels[0].name.data(), "background");
        voices.voices[0].cue = &cue;
        voices.voices[0].state = 3;
        auto& dialogue = audio.banks[6];
        dialogue.id = 7;
        dialogue.references = 2;
        std::strcpy(dialogue.name.data(), "dialogue");
        audio.channels[1].voice = 1;
        audio.channels[1].mode = 2;
        audio.channels[1].owned = 1;
        std::strcpy(audio.channels[1].name.data(), "dialogue");
        voices.voices[1].cue = &cue;
        voices.voices[1].state = 6;
        i32 found = -1;
        if (find_tracked_audio_bank("dialogue", audio) != &dialogue
            || next_tracked_audio_bank(&background, audio) != &dialogue
            || find_tracked_audio_channel("missing", found, audio) != &audio.channels[15]
            || found != 0 || select_resume_audio_bank(dialogue, audio) != &background)
        {
            throw Error("tracked bank/channel lookup lost active or last-free selection");
        }
        audio.banks[10].id = 8;
        if (select_resume_audio_bank(dialogue, audio) != nullptr)
        {
            throw Error("bank resume ignored a higher-priority active bank");
        }
        audio.banks[10].id = 0x12;
        if (select_resume_audio_bank(dialogue, audio) != &background)
        {
            throw Error("stream bank incorrectly blocked background resume");
        }
        audio.banks[10].id = 0;
        release_tracked_audio_bank("dialogue", audio);
        if (dialogue.references != 1 || dialogue.id != 7 || stopped != 0
            || voices.voices[0].state != 3)
        {
            throw Error("shared bank release retired a still-referenced bank");
        }
        const u8 resource[] = {0, 0, 0, 0};
        auto* instance = allocate_cutscene_instance(resource, 0, 1.f, 0);
        std::strcpy(reinterpret_cast<char*>(instance->event_state.data() + 6), "dialogue");
        std::strcpy(reinterpret_cast<char*>(instance->event_state.data() + 0x86), "old-stream");
        request_audio_stream("old-stream", 0x12, audio);
        audio.streams[0].time = 7.f;
        std::array<u8, 0x150> event{};
        std::strcpy(reinterpret_cast<char*>(event.data() + 0x50), "dialogue");
        std::strcpy(reinterpret_cast<char*>(event.data() + 0xD0), "next-stream");
        const std::unordered_map<Actor*, ActorAnimationState*> animations;
        const std::unordered_map<u32, CutsceneResumeCallback> callbacks
        {
            {0x4C4F0, [&](RuntimeCutsceneInstance& received, u32 payload, i32)
            {
                if (payload != 0x1234)
                {
                    throw Error("dialogue completion lost its event record address");
                }
                complete_cutscene_dialogue_event(received, event.data(), audio);
            }}
        };
        instance->pending[4] = 0x4C4F0;
        instance->pending[5] = 0x1234;
        CutsceneEventPauseContext pause{nullptr, animations, voices, callbacks};
        pause_cutscene_events(*instance, 1, pause);
        if (dialogue.references != 1)
        {
            throw Error("dialogue completion released its bank while paused");
        }
        pause_cutscene_events(*instance, 0, pause);
        if (dialogue.id != 0 || dialogue.references != 0 || stopped != 1
            || voices.voices[1].state != 9 || audio.channels[1].voice != -1
            || audio.channels[1].mode != 0 || audio.channels[1].owned != 0
            || voices.voices[0].state != 2 || background.pending != 0
            || audio.channels[0].mode != 2 || instance->event_state[6] != 0
            || std::strcmp(audio.streams[0].name.data(), "next-stream") != 0
            || audio.streams[0].state != 1 || audio.streams[0].id != 0x12
            || audio.streams[0].time != 7.f)
        {
            throw Error("dialogue continuation did not release, resume and replace audio");
        }
        std::strcpy(reinterpret_cast<char*>(event.data() + 0x50), "XACT_SOUNDBANK_GMISC_NOSOUND");
        std::strcpy(reinterpret_cast<char*>(event.data() + 0xD0), "XACT_SOUNDBANK_GMISC_NOSOUND");
        complete_cutscene_dialogue_event(*instance, event.data(), audio);
        release_cutscene_stream(instance->event_state, "other", audio);
        if (std::strcmp(audio.streams[0].name.data(), "next-stream") != 0 || stopped != 1)
        {
            throw Error("silent or mismatched event unexpectedly changed stream ownership");
        }
        release_cutscene_stream(instance->event_state, "next-stream", audio);
        if (audio.streams[0].name[0] != 0 || audio.streams[0].id != -1
            || audio.streams[0].state != 1 || instance->event_state[0x86] != 0)
        {
            throw Error("queued stream release changed the retail retained state");
        }
        background.held = 1;
        detach_tracked_audio_bank(background, audio);
        if (audio.channels[0].voice != 0)
        {
            throw Error("held bank release stopped its channel");
        }
        background.held = 0;
        background.silence = 1;
        detach_tracked_audio_bank(background, audio);
        if (audio.channels[0].voice != 0)
        {
            throw Error("silent bank release stopped its channel");
        }
        request_audio_stream("background", 0x12, audio);
        audio.streams[0].state = 2;
        release_audio_stream("background", audio);
        if (background.references != 0 || background.id != 0 || audio.streams[0].name[0] != 0)
        {
            throw Error("acquired stream release did not release its bank reference");
        }
        instance->~RuntimeCutsceneInstance();
        engine_heap_free(instance);
    }
    {
        AudioCoreSources sources{};
        AudioCoreCues cues{sources};
        AudioCoreBank bank{};
        AudioCoreCue cue{};
        cue.runtime.bank = &bank.runtime;
        cue.runtime.handle = &cue;
        i32 boundary_calls = 0;
        bool boundary_paused = false;
        cues.sound.pause_cue = [&](void*, void* handle, bool paused) -> i32
        {
            if (handle != &cue)
            {
                throw Error("cutscene audio pause changed its native cue handle");
            }
            ++boundary_calls;
            boundary_paused = paused;
            return 0;
        };
        cues.sound.last_error = []() -> u32
        {
            return 0;
        };
        Vec3 origin;
        AudioEventVoices voices{cues, {}, {}, {}, 0, origin, origin};
        voices.voices[0].cue = &cue;
        voices.voices[0].state = 2;
        pause_audio_event_voice(0, 1, voices);
        if (voices.voices[0].state != 3 || boundary_calls != 0)
        {
            throw Error("prepared cutscene audio paused the host before playback");
        }
        pause_audio_event_voice(0, 0, voices);
        if (voices.voices[0].state != 2 || boundary_calls != 0)
        {
            throw Error("prepared cutscene audio resume changed its state gate");
        }
        voices.voices[0].state = 6;
        Actor actor;
        ActorAnimationState animation;
        animation.controller = make_animation_controller(4);
        animation.controller->slots[0].player = make_mina_player();
        CutsceneActorRowView row{&actor, 0, 0, 0, 0, 0};
        const std::unordered_map<Actor*, ActorAnimationState*> animations{{&actor, &animation}};
        const u8 resource[] = {0, 0, 0, 1};
        auto* instance = allocate_cutscene_instance(resource, 1, 1.f, 0);
        const i16 indices[] = {0, -1, -1};
        std::memcpy(instance->event_state.data(), indices, sizeof(indices));
        i32 resumed = 0;
        const std::unordered_map<u32, CutsceneResumeCallback> callbacks
        {
            {0x4C4F0, [&](RuntimeCutsceneInstance& received, u32 payload, i32 paused)
            {
                if (&received != instance || payload != 7 || paused != 0)
                {
                    throw Error("cutscene completion callback changed its retail arguments");
                }
                ++resumed;
            }}
        };
        instance->pending[4] = 0x4C4F0;
        instance->pending[5] = 7;
        CutsceneEventPauseContext context{&row, animations, voices, callbacks};
        pause_cutscene_events(*instance, 1, context);
        pause_cutscene_events(*instance, 1, context);
        if (instance->pending[2] != 2 || animation.controller->slots[0].player->paused != 2
            || voices.voices[0].state != 7 || !boundary_paused || boundary_calls != 1 || resumed != 0)
        {
            throw Error("nested cutscene pause did not connect actors and audio");
        }
        pause_cutscene_events(*instance, 0, context);
        if (resumed != 0 || instance->pending[2] != 1 || boundary_paused || boundary_calls != 2)
        {
            throw Error("cutscene pause invoked continuation before its count reached zero");
        }
        pause_cutscene_events(*instance, 0, context);
        if (resumed != 1 || instance->pending[2] != 0
            || animation.controller->slots[0].player->paused != 0 || boundary_calls != 2)
        {
            throw Error("cutscene resume lost actor state or completion callback");
        }
        instance->flags = 0x10;
        pause_cutscene_events(*instance, 1, context);
        if (voices.voices[0].state != 6 || boundary_calls != 2)
        {
            throw Error("cutscene voice ownership flag failed to suppress audio pause");
        }
        pause_cutscene_events(*instance, 0, context);
        row.actor = nullptr;
        instance->pending[4] = 0;
        voices.voices[0].cue = nullptr;
        instance->flags = 0;
        pause_cutscene_events(*instance, 1, context);
        pause_cutscene_events(*instance, 0, context);
        if (resumed != 2 || boundary_calls != 2)
        {
            throw Error("cutscene resume invented missing callback or cue activity");
        }
        i32 stopped = 0;
        cues.sound.stop_cue = [&](void*, u32, u32 flags, void* handle)
        {
            if (flags != 0 || handle != &cue)
            {
                throw Error("cutscene event cleanup changed native stop arguments");
            }
            ++stopped;
        };
        voices.voices[0].cue = &cue;
        voices.voices[0].state = 6;
        cue.runtime.flags = 1;
        voices.voices[1].cue = &cue;
        voices.voices[1].state = 2;
        voices.voices[2].cue = nullptr;
        voices.voices[2].state = 1;
        const i16 stop_indices[] = {0, 1, 2};
        std::memcpy(instance->event_state.data(), stop_indices, sizeof(stop_indices));
        stop_cutscene_event_voices(instance->event_state, voices);
        if (voices.voices[0].state != 9 || voices.voices[1].state != 4
            || voices.voices[2].state != 10 || stopped != 1 || cues.stopped_count != 1
            || read_u16(instance->event_state, 0) != 0xFFFF
            || read_u16(instance->event_state, 2) != 0xFFFF
            || read_u16(instance->event_state, 4) != 0xFFFF)
        {
            throw Error("cutscene cleanup did not retire playing, prepared and missing cues");
        }
        stop_cutscene_event_voices(instance->event_state, voices);
        if (stopped != 1)
        {
            throw Error("cutscene cleanup repeated an already-cleared voice stop");
        }
        instance->~RuntimeCutsceneInstance();
        engine_heap_free(instance);
    }
    {
        float sine;
        float cosine;
        cutscene_sine_cosine(0.f, sine, cosine);
        if (sine != 0.f || cosine != 1.f)
        {
            throw Error("cutscene trigonometric origin changed");
        }
        cutscene_sine_cosine(1.57079637f, sine, cosine);
        if (std::abs(sine - 1.f) > .00001f || std::abs(cosine) > .00001f)
        {
            throw Error("cutscene trigonometric quadrant changed");
        }
        cutscene_sine_cosine(-3.14159274f, sine, cosine);
        if (std::abs(sine) > .00001f || cosine != -1.f)
        {
            throw Error("cutscene trigonometric sign changed");
        }
        Quat rotation;
        ordered_euler_quaternion(0, {0.f, 0.f, 1.57079637f}, rotation);
        if (std::abs(rotation.z - .70710677f) > .00002f
            || std::abs(rotation.w - .70710677f) > .00002f
            || rotation.x != 0.f || rotation.y != 0.f)
        {
            throw Error("cutscene ordered Euler quaternion changed");
        }
        const float positions[] = {2.f, 4.f, 8.f, 16.f};
        const float zero[] = {0.f};
        const float one[] = {1.f};
        const float changing_w[] = {1.f, .5f};
        CutsceneCameraView camera
        {
            {CutsceneScalarCurveView{4, 4.f, positions},
                CutsceneScalarCurveView{4, 8.f, positions},
                CutsceneScalarCurveView{4, 2.f, positions}},
            {CutsceneScalarCurveView{1, 1.f, zero},
                CutsceneScalarCurveView{1, 1.f, zero},
                CutsceneScalarCurveView{1, 1.f, zero},
                CutsceneScalarCurveView{2, 2.f, changing_w}},
            {1, 1.f, one}, {0, 0.f, nullptr}
        };
        sample_cutscene_camera_rotation(camera, .5f, rotation, 1);
        if (std::abs(rotation.w - 1.f) > .0001f || rotation.x != 0.f)
        {
            throw Error("cutscene camera quaternion did not retain the first channel's fraction");
        }
        Vec3 position;
        sample_cutscene_camera_position(camera, 1.25f, position, 1);
        if (position.x != 5.f || position.y != 3.25f || position.z != 12.f)
        {
            throw Error("cutscene camera position sampling changed");
        }
        const float aspect[] = {1.5f, 1.9f, 2.f};
        camera.aspect = {1, 1.f, aspect};
        if (sample_cutscene_camera_aspect(camera, 0.f) != 0)
        {
            throw Error("cutscene camera lower aspect threshold lost strict comparison");
        }
        camera.aspect.values = aspect + 1;
        if (sample_cutscene_camera_aspect(camera, 0.f) != 1)
        {
            throw Error("cutscene camera upper aspect threshold lost strict comparison");
        }
        camera.aspect.values = aspect + 2;
        if (sample_cutscene_camera_aspect(camera, 0.f) != 2)
        {
            throw Error("cutscene camera wide aspect changed");
        }
        const u8 resource[] = {0, 0, 1, 0};
        auto* instance = allocate_cutscene_instance(resource, 0, 1.f, 0x20);
        instance->scale = {2.f, 3.f, 4.f};
        instance->position = {10.f, 20.f, 30.f};
        transform_cutscene_camera_pose(*instance, &camera, &position, &rotation, 1.25f);
        if (position.x != 20.f || position.y != 29.75f || position.z != 78.f)
        {
            throw Error("cutscene camera pose did not apply instance scale and translation");
        }
        transform_cutscene_camera_pose(*instance, &camera, nullptr, &rotation, 0.f);
        transform_cutscene_camera_pose(*instance, &camera, &position, nullptr, 0.f);
        auto* saved_display = entity_detail::g_display_view;
        const i32 saved_preset = entity_detail::g_aspect_preset;
        entity_detail::DisplayView display;
        display.width = 640;
        display.height = 480;
        entity_detail::g_display_view = &display;
        entity_detail::g_aspect_preset = 0;
        EntityCamNode view;
        view.focal = 35.f;
        update_cutscene_camera_view(*instance, &camera, view);
        if (view.aspect_slot != 2 || view.focal != 1.f
            || view.position.x != 14.f || view.position.y != 26.f || view.position.z != 38.f
            || !std::isfinite(view.focal_pixels) || !std::isfinite(view.pixel_aspect))
        {
            throw Error("cutscene camera view did not select aspect and apply projection");
        }
        instance->blend_time = .5f;
        instance->blend_duration = 1.f;
        instance->blend_position = {0.f, 0.f, 0.f};
        instance->flags |= 8;
        view.aspect_slot = 0;
        update_cutscene_camera_view(*instance, &camera, view);
        const float expected_focal = std::bit_cast<float>(0x4178B780u)
            / std::bit_cast<float>(0x41C00625u);
        if (view.aspect_slot != 0 || view.focal != expected_focal
            || view.position.x != 7.f || view.position.y != 13.f || view.position.z != 19.f)
        {
            throw Error("cutscene camera view changed pose blending or retained-aspect focal scaling");
        }
        entity_detail::g_display_view = saved_display;
        entity_detail::g_aspect_preset = saved_preset;
        instance->~RuntimeCutsceneInstance();
        engine_heap_free(instance);
    }
    {
        static i32 allocated_size;
        const u8 resource[] = {0, 0, 3, 2};
        auto* instance = allocate_cutscene_instance(resource, 2, 12.f, 0x25,
            [](i32 size) -> void*
            {
                allocated_size = size;
                void* storage = std::malloc(size);
                std::memset(storage, 0xA5, size);
                return storage;
            });
        if (allocated_size != sizeof(RuntimeCutsceneInstance) + 0x60
            || instance->actor_rows != reinterpret_cast<u8*>(instance + 1)
            || instance->resource != resource || instance->actor_count != 2
            || instance->camera != 2 || instance->duration != 12.f
            || instance->flags != 0x25 || instance->events != nullptr
            || instance->time != 0.f || instance->previous_time != 0.f
            || instance->scale.x != 1.f || instance->scale.y != 1.f || instance->scale.z != 1.f
            || instance->blend_time != -1.f || instance->blend_duration != -1.f
            || instance->blend_rotation.w != 1.f
            || instance->untouched_38 != 0xA5A5A5A5u
            || read_u32(instance->event_state, 0) != 0xFFFFFFFFu
            || read_u16(instance->event_state, 4) != 0xFFFFu
            || instance->event_state[6] != 0xA5 || instance->event_state.back() != 0xA5
            || instance->actor_rows[0] != 0xA5 || instance->actor_rows[0x5F] != 0xA5)
        {
            throw Error("cutscene constructor changed initialized or untouched storage");
        }
        instance->~RuntimeCutsceneInstance();
        std::free(instance);
        const u8 no_camera[] = {0, 0, 0, 0};
        instance = allocate_cutscene_instance(no_camera, 0, 0.f, 0);
        if (instance->camera != -1 || instance->actor_count != 0)
        {
            throw Error("cutscene constructor changed empty camera selection");
        }
        instance->~RuntimeCutsceneInstance();
        engine_heap_free(instance);
    }
    {
        static std::array<u8, 0x358> received;
        static i32 calls;
        static std::unordered_map<const u8*, CutsceneActorBinding*> bindings;
        static CutsceneActorBinding* active_binding;
        static const ObjectFactoryVtable vtable
        {
            0x1234,
            [](ObjectSizes& sizes)
            {
                sizes = {16, 0x358};
            },
            [](FactoryObject& object, std::span<const u8> parameters)
            {
                ++calls;
                std::memcpy(received.data(), parameters.data(), received.size());
                if (active_binding != nullptr)
                {
                    bindings[object.bytes.get()] = active_binding;
                }
                return true;
            },
            nullptr
        };
        std::array<ObjectFactoryType, 2> types{};
        types[1].vtable = []()
        {
            return &vtable;
        };
        ObjectFactoryRegistry factory{types, 0};
        std::vector<u8> source(0x358, 0);
        write_u16(source, 0, 0x358);
        write_u16(source, 2, 1);
        write_u16(source, 4, 1);
        std::strcpy(reinterpret_cast<char*>(source.data() + 0x28), "aid_original");
        ExecutableResourceRecord record{"aid_objparams_actor", source, {},
            animation_resource_hash("aid_objparams_actor")};
        ExecutableResourcePool pool{{&record, 1}, "objparams", false};
        auto object = create_cutscene_actor(factory, {&pool, 1}, record.name.c_str(),
            "aid_model", {3.f, 4.f, 5.f}, 1);
        if (object.bytes == nullptr || calls != 1 || read_f32(received, 8) != 3.f
            || read_f32(received, 0xC) != 4.f || read_f32(received, 0x10) != 5.f
            || std::strcmp(reinterpret_cast<const char*>(received.data() + 0x28), "aid_model") != 0
            || read_u32(received, 0x2B0) != 1
            || std::strcmp(reinterpret_cast<const char*>(received.data() + 0x2B4), "aid_model") != 0
            || read_u32(source, 0x2B0) != 0 || read_f32(source, 8) != 0.f)
        {
            throw Error("cutscene actor factory did not clone and patch retail parameters");
        }
        object = create_cutscene_actor(factory, {&pool, 1}, record.name.c_str(),
            "plain_model", {0.f, 0.f, 0.f}, 0);
        if (object.bytes == nullptr || calls != 2 || read_u32(received, 0x2B0) != 0
            || std::strcmp(reinterpret_cast<const char*>(received.data() + 0x28), "aid_original") != 0)
        {
            throw Error("cutscene actor factory changed conditional model overrides");
        }
        object = create_cutscene_actor(factory, {&pool, 1}, "aid_objparams_missing",
            "aid_model", {0.f, 0.f, 0.f}, 1);
        if (object.bytes != nullptr || calls != 2)
        {
            throw Error("cutscene actor factory continued after a failed clone");
        }
        record.name = "aid_objparams_ghoulies_misc_prop"; // DAT_002FE6C0 used by 0x0004DEC0.
        record.hash = animation_resource_hash(record.name.c_str());
        const float x[] = {7.f};
        const float y[] = {8.f};
        const float z[] = {9.f};
        CutsceneActorTrackView track
        {
            "aid_original", {}, 1, 4, "auxiliary",
            {{CutsceneScalarCurveView{1, 1.f, x}, CutsceneScalarCurveView{1, 1.f, y},
                CutsceneScalarCurveView{1, 1.f, z}}}, {}
        };
        CutsceneActorFactoryContext context{factory, {&pool, 1}, "aid_original", "aid_replacement"};
        object = create_cutscene_track_actor(&track, 0, 0, context);
        if (object.bytes == nullptr || calls != 3 || read_f32(received, 8) != 7.f
            || read_f32(received, 0xC) != 8.f || read_f32(received, 0x10) != 9.f
            || read_u32(received, 0x2B0) != 1
            || std::strcmp(reinterpret_cast<const char*>(received.data() + 0x28), "aid_replacement") != 0)
        {
            throw Error("cutscene track actor substitution or initial position changed");
        }
        context.replacement_source = "aid_other";
        track.auxiliary = nullptr;
        object = create_cutscene_track_actor(&track, 0, 1, context);
        if (object.bytes == nullptr || calls != 4 || read_u32(received, 0x2B0) != 0
            || std::strcmp(reinterpret_cast<const char*>(received.data() + 0x28), "aid_original") != 0)
        {
            throw Error("cutscene track actor secondary suppression changed");
        }
        Actor actor;
        actor.scale_200 = 1.f;
        ActorAnimationState animation;
        ModelAnimationState model;
        ActorAttachmentList attachments;
        ActorAttachmentNode pending{};
        attachments.pending = &pending; // Actor +0x1B4.
        entity_detail::ListHead anchors;
        ActorFrameState frame{actor, animation, model, attachments, pending, nullptr, anchors,
            0, {}, {}, 0.f, 0};
        entity_detail::ShapeProbe shape;
        entity_detail::SegmentProbe segment;
        RuntimeActorLink link;
        ActorRenderRegistration render{actor, shape, segment, link};
        static u32 message;
        render.message = [](ActorRenderRegistration&, u32 id)
        {
            message = id;
        };
        CutsceneActorBinding binding{frame, render};
        active_binding = &binding;
        std::vector<u8> mina(0x40);
        std::memcpy(mina.data(), "MINA", 4);
        const float duration = 1.f;
        std::memcpy(mina.data() + 8, &duration, 4);
        track.animation = mina;
        track.scale = {{CutsceneScalarCurveView{1, 1.f, x}, CutsceneScalarCurveView{1, 1.f, y},
            CutsceneScalarCurveView{1, 1.f, z}}};
        AnimationResourceRegistry registry;
        ResourceErrorState error;
        u32 random = 1;
        ModelNodeFlagCache cache{};
        AnimationMatrix reset{};
        std::vector<FactoryObject> objects;
        CutsceneActorInitializationContext initialization{context, objects, bindings, registry,
            error, random, nullptr, cache, reset};
        const u8 resource[] = {0, 0, 0, 1};
        auto* instance = allocate_cutscene_instance(resource, 1, 1.f, 0);
        CutsceneActorRowView row{nullptr, 99, 99, 99, 99, 99};
        initialize_cutscene_actors(*instance, &track, &row, 0, 0, initialization);
        if (row.actor != &actor || row.elapsed != 0 || row.previous != 0 || row.flags != 0
            || row.listed != 1 || row.initialized != 1 || objects.size() != 1 || message != 9
            || actor.scale_x != 7.f || actor.radius_y != 8.f || actor.radius_xz != 9.f
            || animation.controller == nullptr || animation.controller->slot_count != 4
            || animation.controller->slots[0].player->paused != 1 || !animation.controller->dirty)
        {
            throw Error("cutscene actor initialization lost animation, scale or message setup");
        }
        RuntimeActorLink previous;
        RuntimeActorLink next;
        link.previous = &previous;
        link.next = &next;
        previous.next = &link;
        next.previous = &link;
        initialize_cutscene_actors(*instance, &track, &row, 1, 1, initialization);
        if (row.listed != 0 || link.previous != nullptr || link.next != nullptr
            || previous.next != &next || next.previous != &previous || objects.size() != 2
            || animation.controller->slots[0].player->paused != 2)
        {
            throw Error("cutscene actor initialization lost unlinking or byte pause count");
        }
        if (find_cutscene_actor_track(*instance, &track, 4) != 0
            || find_cutscene_actor_track(*instance, &track, 5) != -1
            || find_cutscene_actor(*instance, &track, &row, 4) != &actor
            || find_cutscene_actor(*instance, &track, &row, 5) != nullptr)
        {
            throw Error("cutscene actor lookup changed track IDs or missing sentinel");
        }
        set_cutscene_actor_track_state(*instance, &track, &row, 4, 7);
        set_cutscene_actor_track_state(*instance, &track, &row, 5, 9);
        if (row.previous != 7)
        {
            throw Error("cutscene actor state update changed missing-ID behavior");
        }
        const std::unordered_map<Actor*, ActorAnimationState*> actor_animations{{&actor, &animation}};
        row.flags = 1;
        animation.controller->slots[1].player = make_mina_player();
        animation.controller->slots[1].player->paused = 0xFF;
        pause_cutscene_actor(&row, 0, 1, actor_animations);
        if (animation.controller->slots[0].player->paused != 3
            || animation.controller->slots[1].player->paused != 0)
        {
            throw Error("cutscene pause did not increment both player bytes with wraparound");
        }
        pause_cutscene_actor(&row, 0, 0, actor_animations);
        if (animation.controller->slots[0].player->paused != 2
            || animation.controller->slots[1].player->paused != 0xFF)
        {
            throw Error("cutscene resume did not restore both player pause bytes");
        }
        row.flags = 0;
        pause_cutscene_actor(&row, 0, 1, actor_animations);
        if (animation.controller->slots[1].player->paused != 0xFF)
        {
            throw Error("cutscene pause changed inactive secondary animation");
        }
        record.name = "aid_objparams_missing";
        registry.loaded.push_back(AnimationLoadedResource{mina});
        registry.records.push_back(AnimationResourceRecord{
            1, 1, "aid_test_cutscene_events", animation_resource_hash("aid_test_cutscene_events")});
        std::span<const u8> event_resources[]{mina};
        const std::function<FactoryObject*(Actor*)> released_objects = [&](Actor* found) -> FactoryObject*
        {
            if (found != &actor)
            {
                throw Error("cutscene release looked up an unknown actor");
            }
            return &objects.back();
        };
        const std::unordered_map<Actor*, ActorRenderRegistration*> released_render{{&actor, &render}};
        CutsceneActorReleaseContext release_context{
            factory, released_objects, released_render, event_resources, registry};
        row.listed = 1;
        row.elapsed = 42;
        link.previous = &previous;
        link.next = &next;
        previous.next = &link;
        next.previous = &link;
        ActorLink detached_link{};
        actor.link = &detached_link; // Native view exists, but the retail +0x918 head is zero.
        const u32 count_before_release = factory.count;
        release_cutscene_actor_row(&row, 0, release_context);
        if (row.actor != nullptr || row.elapsed != 0 || event_resources[0].data() != nullptr
            || registry.records.back().references != 0 || objects.back().bytes != nullptr
            || factory.count != count_before_release - 1 || previous.next != &next
            || next.previous != &previous || row.listed != 1)
        {
            throw Error("cutscene actor release lost unlinking, ownership or retail row writes");
        }
        release_cutscene_actor_row(&row, 0, release_context);
        if (factory.count != count_before_release - 1 || registry.records.back().references != 0)
        {
            throw Error("cleared cutscene row released its actor or event resource twice");
        }
        event_resources[0] = mina;
        registry.records.back().references = 1;
        row.elapsed = 43;
        release_cutscene_actor_row(&row, 0, release_context);
        if (row.elapsed != 0 || event_resources[0].data() != nullptr
            || registry.records.back().references != 0)
        {
            throw Error("actorless cutscene row retained its event resource");
        }
        ActorLink bound_link{};
        ActorLink parent_link{};
        bound_link.head = &parent_link;
        actor.link = &bound_link;
        actor.render_registration = &render;
        render.child.prev = &render.child;
        row.actor = &actor;
        release_cutscene_actor_row(&row, 0, release_context);
        if (row.actor != nullptr || actor.link != &bound_link
            || factory.count != count_before_release - 1)
        {
            throw Error("bound cutscene actor was freed instead of reactivated");
        }
        actor.link = nullptr;
        render.child.prev = nullptr;
        record.hash = animation_resource_hash(record.name.c_str());
        row = {&actor, 99, 99, 99, 7, 8};
        initialize_cutscene_actors(*instance, &track, &row, 0, 0, initialization);
        if (row.actor != nullptr || row.elapsed != 0 || row.previous != 0 || row.flags != 0
            || row.listed != 7 || row.initialized != 8 || objects.size() != 2)
        {
            throw Error("cutscene actor initialization changed failed actor row writes");
        }
        instance->~RuntimeCutsceneInstance();
        engine_heap_free(instance);
        active_binding = nullptr;
        bindings.clear();
    }
    {
        const float keys[] = {2.f, 4.f, 8.f, 16.f};
        CutsceneScalarCurveView curve{4, 4.f, keys};
        float first = 0.f;
        float second = 0.f;
        if (sample_cutscene_curve_pair(curve, -.5f, first, second) != -.5f
            || first != 2.f || second != 4.f
            || sample_cutscene_curve_pair(curve, 1.25f, first, second) != .25f
            || first != 4.f || second != 8.f
            || sample_cutscene_curve_pair(curve, 4.f, first, second) != 1.f
            || first != 8.f || second != 16.f)
        {
            throw Error("retail cutscene interpolation boundaries changed");
        }
        curve.count = 1;
        if (sample_cutscene_curve_pair(curve, 9.f, first, second) != 0.f
            || first != 2.f || second != 2.f)
        {
            throw Error("single-key cutscene interpolation changed");
        }
        CutsceneActorScaleView actor{{CutsceneScalarCurveView{4, 4.f, keys},
            CutsceneScalarCurveView{4, 8.f, keys}, CutsceneScalarCurveView{4, 2.f, keys}}};
        Vec3 scale;
        sample_cutscene_actor_scale(&actor, 0, 2.f, scale);
        if (scale.x != 8.f || scale.y != 4.f || scale.z != 16.f)
        {
            throw Error("cutscene actor scale did not use each channel's duration");
        }
        sample_cutscene_actor_scale(&actor, 0, -1.f, scale);
        if (scale.x != 16.f || scale.y != 2.f || scale.z != 16.f)
        {
            throw Error("cutscene actor scale unsigned-index clamp changed");
        }
        const CutsceneActorPositionView position_actor{actor.scale};
        Vec3 position;
        sample_cutscene_actor_position(&position_actor, 0, 1.25f, position, 1);
        if (position.x != 5.f || position.y != 3.25f || position.z != 12.f)
        {
            throw Error("cutscene actor position did not interpolate independent channels");
        }
        sample_cutscene_actor_position(&position_actor, 0, 1.25f, position, 0);
        if (position.x != 4.f || position.y != 2.f || position.z != 8.f)
        {
            throw Error("cutscene actor position changed discrete sampling");
        }
    }
    {
        EntityCamNode previous;
        EntityCamNode next;
        EntityCamNode view;
        view.prev = &previous;
        view.next = &next;
        view.position = {1.f, 2.f, 3.f};
        view.rotation = {4.f, 5.f, 6.f, 7.f};
        std::memset(view.name, 0x7F, sizeof(view.name));
        initialize_camera_view(view, "dialog", 0);
        if (view.prev != nullptr || view.next != nullptr || view.position.x != 0.f
            || view.position.y != 0.f || view.position.z != 0.f
            || view.rotation.x != 0.f || view.rotation.y != 0.f || view.rotation.z != 0.f
            || view.rotation.w != 1.f || view.flags != 0 || view.focal != -1.f
            || std::bit_cast<u32>(view.clip_limit) != 0x4E6E6B28u || view.blend != 0.f
            || view.aspect_slot != 3 || std::strcmp(view.name, "dialog") != 0 || view.name[7] != 0x7F)
        {
            throw Error("camera view initialization lost retail pose, links, constants or exact name copy");
        }
        // This component fixture has the untouched zero-size display record.
        if (!std::isnan(view.pixel_aspect) || view.focal_pixels != 0.f)
        {
            throw Error("camera view initialization lost the zero-size display projection result");
        }
        view.rotation = {8.f, 9.f, 10.f, 11.f};
        initialize_camera_view(view, "01234567890123456789012345678901", 0xA3);
        if (view.flags != 0xA3 || view.rotation.x != 0.f || view.rotation.y != 0.f
            || view.rotation.z != 0.f || view.rotation.w != 11.f || view.name[32] != 0)
        {
            throw Error("Euler camera initialization overwrote its untouched fourth component or flag bits");
        }
    }
    {
        using namespace entity_detail;
        DisplayView display{0, 0, 0, 640, 480};
        display.position = {10.f, 20.f, 30.f};
        display.rotation = {1.f, 2.f, 3.f, 4.f};
        display.forward = {0.f, -1.f, 0.f};
        DisplayView* previous_display = g_display_view;
        const EntityCamNode previous_anchor = g_cam_anchor[0];
        EntityCamNode* previous_active = g_cam_active[0];
        std::array<char, 33> previous_name;
        std::memcpy(previous_name.data(), g_cam_saved_name[0], 33);
        g_display_view = &display;
        g_cam_anchor[0] = {};
        g_cam_active[0] = nullptr;
        g_cam_saved_name[0][0] = 0;
        EntityCamNode first;
        EntityCamNode second;
        EntityCamNode fixed;
        initialize_camera_view(first, "dialog", 2);
        initialize_camera_view(second, "euler", 3);
        initialize_camera_view(fixed, "fixed", 0);
        second.rotation.w = 42.f;
        link_camera_view(0, first);
        if (g_cam_active[0] != &first || g_cam_anchor[0].next != &first
            || first.prev != &g_cam_anchor[0] || first.next != nullptr
            || first.position.x != 0.f || first.position.y != 15.f || first.position.z != 30.f
            || first.rotation.x != 1.f || first.rotation.w != 4.f
            || first.pixel_aspect != 1.f || !std::isfinite(first.focal_pixels) || first.focal_pixels <= 0.f)
        {
            throw Error("camera insertion lost auto-binding, default pose or fitted projection");
        }
        link_camera_view(0, second);
        if (g_cam_active[0] != &first || g_cam_anchor[0].next != &second
            || second.next != &first || first.prev != &second)
        {
            throw Error("camera insertion lost list order or replaced an already active view");
        }
        if (camera_bind_named_view(0, "EULER") != 1 || g_cam_active[0] != &second
            || second.position.x != 10.f || second.position.y != 20.f || second.position.z != 30.f
            || std::abs(second.rotation.x + 1.57079632679f) > 0.000001f
            || second.rotation.y != 0.f || second.rotation.z != 0.f || second.rotation.w != 42.f
            || std::strcmp(g_cam_saved_name[0], "dialog") != 0)
        {
            throw Error("camera binding lost case folding, display direction or the prior view name");
        }
        camera_unlink_view(&second);
        if (g_cam_active[0] != &first || g_cam_anchor[0].next != &first
            || first.prev != &g_cam_anchor[0] || second.prev != nullptr || second.next != nullptr
            || first.position.x != 10.f || first.position.y != 20.f)
        {
            throw Error("camera removal did not restore and reseed the saved view");
        }
        link_camera_view(0, fixed);
        fixed.position = {7.f, 8.f, 9.f};
        fixed.rotation = {5.f, 6.f, 7.f, 8.f};
        if (camera_bind_named_view(0, "fixed") != 1 || fixed.position.x != 7.f || fixed.rotation.w != 8.f)
        {
            throw Error("camera binding overwrote an unseeded view's pose");
        }
        if (camera_bind_named_view(0, "missing") != 0 || g_cam_active[0] != nullptr)
        {
            throw Error("camera lookup miss did not clear the active view");
        }
        camera_unlink_view(&fixed);
        camera_unlink_view(&first);
        link_camera_view(0, second);
        if (second.position.y != 15.f || second.rotation.x != 0.f || second.rotation.y != 0.f
            || second.rotation.z != 0.f || second.rotation.w != 42.f)
        {
            throw Error("initial Euler binding lost its default forward direction or retained fourth word");
        }
        camera_unlink_view(&second);
        if (g_cam_active[0] != nullptr || g_cam_anchor[0].next != nullptr)
        {
            throw Error("last camera removal left an active or linked view");
        }
        g_display_view = previous_display;
        g_cam_anchor[0] = previous_anchor;
        g_cam_active[0] = previous_active;
        std::memcpy(g_cam_saved_name[0], previous_name.data(), 33);
    }
    {
        using namespace entity_detail;
        DisplayView* previous_display = g_display_view;
        std::array<DisplayView, 4> previous_records;
        for (size_t index = 0; index < 4; ++index)
        {
            previous_records[index] = g_display_records[index];
            g_display_records[index].flags = 0xA5;
            g_display_records[index].mark = 0xFF;
        }
        i32 render_width = -1;
        i32 render_height = -1;
        initialize_display_views(640, 480, render_width, render_height);
        if (render_width != 640 || render_height != 480 || g_display_view != &g_display_records[3])
        {
            throw Error("display initialization lost viewport globals or its final active record");
        }
        for (size_t index = 0; index < 4; ++index)
        {
            const auto& view = g_display_records[index];
            if (view.flags != (index == 0 ? 0xA5u : 0xA4u) || view.mark != 0
                || view.width != 640 || view.height != 480 || view.origin_x != 0 || view.origin_y != 0
                || view.focal != -1.f || view.aspect_mode != 3 || view.near_clip != 1.f || view.far_clip != 1000.f
                || view.position.x != 0.f || view.position.y != 0.f || view.position.z != 0.f
                || std::abs(view.rotation.x - 1.f) > 0.0001f || std::abs(view.rotation.w) > 0.0001f
                || std::abs(view.forward.x) > 0.0001f || std::abs(view.forward.y) > 0.0001f
                || std::abs(view.forward.z - 1.f) > 0.0001f || view.pixel_aspect != 1.f
                || view.fitted_width != 640 || view.fitted_height != 436 || view.fitted_x != 0 || view.fitted_y != 22
                || view.transform != std::array<float, 12>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0}
                || view.projection != std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1})
            {
                throw Error("display initialization lost flags, fitted viewport, clip planes or initial camera matrices");
            }
        }
        Quat quaternion;
        camera_euler_to_quaternion({0.f, 0.f, 0.f}, quaternion);
        if (quaternion.x != 0.f || quaternion.y != 0.f || quaternion.z != 0.f || quaternion.w != 1.f)
        {
            throw Error("retail Euler conversion did not produce the zero-angle identity");
        }
        camera_euler_to_quaternion({0.f, 1.57079632679f, 0.f}, quaternion);
        if (std::abs(quaternion.y - 0.70710678118f) > 0.0001f
            || std::abs(quaternion.w - 0.70710678118f) > 0.0001f || quaternion.x != 0.f || quaternion.z != 0.f)
        {
            throw Error("retail Euler conversion lost its yaw-axis half angle");
        }
        set_display_rotation(quaternion);
        if (std::abs(g_display_view->forward.x + 1.f) > 0.0001f
            || std::abs(g_display_view->forward.y) > 0.0001f || std::abs(g_display_view->forward.z) > 0.0001f)
        {
            throw Error("display rotation did not rotate the camera forward axis");
        }
        set_display_rotation({0.f, 2.f, 0.f, 0.f});
        if (std::abs(g_display_view->forward.z - 1.f) > 0.0001f)
        {
            throw Error("display rotation failed to normalize a non-unit quaternion's direction");
        }
        set_display_rotation({0.f, 0.f, 0.f, 0.f});
        if (g_display_view->forward.x != 0.f || g_display_view->forward.y != 0.f || g_display_view->forward.z != 0.f)
        {
            throw Error("display rotation normalized the zero direction instead of preserving it");
        }
        for (size_t index = 0; index < 4; ++index)
        {
            g_display_records[index] = previous_records[index];
        }
        g_display_view = previous_display;
    }
    {
        DialogueTagState state{};
        DialogueOwnedResources owned;
        DialogueSoundTagState sound{};
        const std::array<i32, 4> thresholds{1, 2, 3, 4};
        AnimationResourceRegistry registry;
        ResourceErrorState error{};
        u32 flags = 0;
        char name[128];
        DialogueTrophyContext trophies{thresholds, owned, flags, name,
            g_retail_dialogue_token_names, registry, error};
        DialogueTagContext context{state, trophies, sound, g_retail_dialogue_tokens,
            g_retail_dialogue_sound_swaps, {}, 0, -1, 0, 0};
        const char16_t* text = u"{settime 0.75}next";
        if (*process_dialogue_tag(text, context) != u'}' || state.time != 0.75f)
        {
            throw Error("dialogue tag processing lost its arguments or closing-brace return");
        }
        process_dialogue_tag(u"{scale 1.5}", context);
        process_dialogue_tag(u"{liner}", context);
        process_dialogue_tag(u"{page}", context);
        if (state.scale_x != 1.5f || state.scale_y != 1.5f || state.alignment != 2 || flags != 5)
        {
            throw Error("dialogue token dispatcher lost scale, alignment or page flags");
        }
        process_dialogue_tag(u"{sfx gmisc_example}", context);
        if (std::strcmp(sound.cue.data(), "XACT_SOUNDBANK_GMISC_EXAMPLE") != 0)
        {
            throw Error("dialogue tag processing did not execute its sound-name handler");
        }
        const auto saved_flags = flags;
        if (*process_dialogue_tag(u"{unknown no-close", context) != 0 || flags != saved_flags)
        {
            throw Error("unknown dialogue tag changed state or lost unterminated-tag scanning");
        }
        std::u16string long_number = u"2";
        long_number.append(70, u' ');
        if (dialogue_wide_to_double(long_number.c_str(), state.numeric_scratch) != 2.0 ||
            state.numeric_scratch[63] != 0)
        {
            throw Error("dialogue wide-number conversion lost its 63-byte scratch limit");
        }
    }
    {
        using namespace scene_detail;
        CrtDecimalResult result{};
        result.retained[0] = 0xAABBCCDD;
        crt_parse_decimal_result(result, " -12.5e+2tail");
        if (result.value != -1250.0 || result.flags != 0 || result.consumed != 9 || result.retained[0] != 0xAABBCCDD)
        {
            throw Error("retail decimal parser lost exponent, consumed length or retained result words");
        }
        crt_parse_decimal_result(result, "1e+oops");
        if (result.value != 1.0 || result.flags != 0 || result.consumed != 1)
        {
            throw Error("retail decimal parser did not roll back an incomplete exponent");
        }
        crt_parse_decimal_result(result, "+oops");
        if (result.flags != 0x200 || result.value != 0 || result.consumed != 0)
        {
            throw Error("retail decimal parser lost invalid-number flags or sign rollback");
        }
        crt_parse_decimal_result(result, "1e6000");
        if (result.flags != 0x80 || !std::isinf(result.value) || result.consumed != 6)
        {
            throw Error("retail decimal parser lost overflow status or exponent consumption");
        }
        crt_parse_decimal_result(result, "1e-6000");
        if (result.flags != 0x100 || result.value != 0 || result.consumed != 7)
        {
            throw Error("retail decimal parser lost underflow status");
        }
        if (crt_ascii_to_double("\t.125D2tail") != 12.5 ||
            std::bit_cast<std::uint64_t>(crt_ascii_to_double("-0")) != 0x8000000000000000ull)
        {
            throw Error("retail atof lost whitespace, alternate exponent or negative zero");
        }
    }
    {
        using namespace scene_detail;
        CrtMantissa arithmetic{0xFFFFFFFFu, 0xFFFFFFFFu, 0};
        crt_add_mantissa(arithmetic, CrtMantissa{1, 0, 0});
        if (arithmetic != CrtMantissa{0, 0, 1})
        {
            throw Error("extended mantissa addition lost carry propagation");
        }
        arithmetic = {0x80000000u, 0x80000000u, 1};
        crt_shift_mantissa_left(arithmetic);
        if (arithmetic != CrtMantissa{0, 1, 3})
        {
            throw Error("extended mantissa left shift lost word boundaries");
        }
        arithmetic = {0x80000000u, 0, 0};
        crt_shift_mantissa_right(arithmetic, 64);
        if (arithmetic != CrtMantissa{0, 0, 0x80000000u})
        {
            throw Error("extended mantissa right shift lost a whole-word shift");
        }
        CrtMantissa tie{0x80000000u, 0x400, 0};
        CrtMantissa above{0x80000000u, 0x400, 1};
        crt_round_mantissa(tie, 53);
        crt_round_mantissa(above, 53);
        if (tie != CrtMantissa{0x80000000u, 0x400, 0} ||
            above != CrtMantissa{0x80000000u, 0xC00, 0})
        {
            throw Error("extended mantissa rounding changed exact-half or sticky-tail behavior");
        }
        const signed char decimal_digits[]{1, 2, 5};
        CrtMantissa decimal;
        crt_decimal_digits_to_extended(decimal_digits, 3, decimal);
        double decimal_output;
        if (crt_extended_to_double({reinterpret_cast<const u8*>(decimal.data()), 12}, decimal_output) != 0 ||
            decimal_output != 125.0)
        {
            throw Error("decimal digit accumulation lost multiply-by-ten or exponent normalization");
        }
        crt_scale_extended_decimal(decimal, -1, 0);
        if (crt_extended_to_double({reinterpret_cast<const u8*>(decimal.data()), 12}, decimal_output) != 0 ||
            decimal_output != 12.5)
        {
            throw Error("extended decimal scaling lost its negative power table or multiplication");
        }
        crt_scale_extended_decimal(decimal, 2, 0);
        if (crt_extended_to_double({reinterpret_cast<const u8*>(decimal.data()), 12}, decimal_output) != 0 ||
            decimal_output != 1250.0)
        {
            throw Error("extended decimal scaling lost its positive power table");
        }
        std::array<u8, 12> extended{};
        const u32 high = 0x80000000u;
        u16 exponent = 0x3FFF;
        std::memcpy(extended.data() + 6, &high, 4);
        std::memcpy(extended.data() + 10, &exponent, 2);
        double output;
        if (crt_extended_to_double(extended, output) != 0 || output != 1.0)
        {
            throw Error("extended-to-double conversion lost its normal exponent or explicit mantissa bit");
        }
        exponent = 0x7FFF;
        std::memcpy(extended.data() + 10, &exponent, 2);
        if (crt_extended_to_double(extended, output) != 1 || !std::isinf(output))
        {
            throw Error("extended-to-double conversion lost overflow status");
        }
        exponent = 0x3FFF - 1074;
        std::memcpy(extended.data() + 10, &exponent, 2);
        if (crt_extended_to_double(extended, output) != 2 ||
            std::bit_cast<std::uint64_t>(output) != 1)
        {
            throw Error("extended-to-double conversion lost the smallest subnormal or underflow status");
        }
        extended.fill(0);
        extended[11] = 0x80;
        if (crt_extended_to_double(extended, output) != 0 ||
            std::bit_cast<std::uint64_t>(output) != 0x8000000000000000ull)
        {
            throw Error("extended-to-double conversion lost negative zero");
        }
    }
    {
        DialogueSoundTagState sound{};
        select_dialogue_sound_tag(u"  gmisc_test}ignored", 0, 0, sound, g_retail_dialogue_sound_swaps);
        if (std::strcmp(sound.name.data(), "gmisc_test") != 0 ||
            std::strcmp(sound.cue.data(), "XACT_SOUNDBANK_GMISC_TEST") != 0)
        {
            throw Error("dialogue sound tag lost whitespace, closing brace or cue capitalization");
        }
        select_dialogue_sound_tag(u"GDIALOGUE_BOY_BREATHLAUGH}", 8, 0, sound, g_retail_dialogue_sound_swaps);
        if (std::strcmp(sound.cue.data(), "XACT_SOUNDBANK_GDIALOGUE_GIRL_BREATHLAUGH") != 0)
        {
            throw Error("dialogue sound tag did not apply the retail boy-to-girl cue swap");
        }
        select_dialogue_sound_tag(u"GDIALOGUE_GIRL_BREATHLAUGH}", 0, 0x200000, sound, g_retail_dialogue_sound_swaps);
        if (std::strcmp(sound.cue.data(), "XACT_SOUNDBANK_GDIALOGUE_BOY_BREATHLAUGH") != 0)
        {
            throw Error("dialogue sound tag did not apply the retail girl-to-boy cue swap");
        }
        select_dialogue_sound_tag(u"GDIALOGUE_BOY_UNKNOWN}", 8, 0, sound, g_retail_dialogue_sound_swaps);
        if (std::strcmp(sound.cue.data(), "XACT_SOUNDBANK_GDIALOGUE_BOY_UNKNOWN") != 0)
        {
            throw Error("dialogue unmatched sound tag changed its requested cue");
        }
        const auto retained = sound.cue;
        select_dialogue_sound_tag(u"   ", 0, 0, sound, g_retail_dialogue_sound_swaps);
        if (sound.cue != retained)
        {
            throw Error("empty dialogue sound tag changed the previous cue");
        }
    }
    {
        std::array<char, 40> digits{};
        const std::array<i32, 7> values{0, 1, -1, 10, -1250, INT32_MIN, INT32_MAX};
        for (const i32 value : values)
        {
            if (scene_detail::crt_integer_to_string(value, digits.data(), 10) != digits.data()
                || std::string(digits.data()) != std::to_string(value))
            {
                throw Error("retail integer conversion lost sign, reversal or minimum magnitude");
            }
        }
        scene_detail::crt_integer_to_string(-1, digits.data(), 16);
        if (std::string(digits.data()) != "ffffffff")
        {
            throw Error("retail non-decimal integer conversion lost unsigned digits");
        }
        std::array<char16_t, 40> wide{};
        char16_t* result = nullptr;
        dialogue_total_first(result, wide.data(), digits.data(), INT32_MAX, 1);
        if (result != wide.data() || std::u16string(result) != u"-2147483648")
        {
            throw Error("dialogue total substitution lost wrapped addition or borrowed output");
        }
        dialogue_second_count(result, wide.data(), digits.data(), 0);
        if (std::u16string(result) != u"0")
        {
            throw Error("dialogue count substitution did not replace the previous text");
        }
        dialogue_total_third(result, wide.data(), digits.data(), 125, -25);
        if (std::u16string(result) != u"100")
        {
            throw Error("dialogue third total substitution lost its additional count");
        }
        const std::array<u8, 4> bits{0, 0xFF, 0x81, 0x55};
        dialogue_bit_count(result, wide.data(), digits.data(), {4, bits.data()});
        if (std::u16string(result) != u"14"
            || count_dialogue_bits({0, nullptr}) != 0
            || count_dialogue_bits({-1, nullptr}) != 0)
        {
            throw Error("dialogue bit count lost sparse bits or empty-range behavior");
        }
        const std::array<i32, 3> scores{-1, INT32_MIN, 1250};
        dialogue_final_score(result, wide.data(), scores.data(), 0);
        if (result != wide.data() || std::u16string(result) != u"?")
        {
            throw Error("dialogue unset final score lost its retail question mark");
        }
        dialogue_final_score(result, wide.data(), scores.data(), 1);
        if (std::u16string(result) != u"-2147483648")
        {
            throw Error("dialogue final score lost signed wide formatting");
        }
        dialogue_final_score(result, wide.data(), scores.data(), 2);
        if (std::u16string(result) != u"1250"
            || scene_detail::crt_format_wide_string(wide.data(), u"%04d %s", 7, u"x") != 6
            || std::u16string(wide.data()) != u"0007 x")
        {
            throw Error("retail wide formatter lost padding, string argument or termination");
        }
        alignas(4) std::array<char, 32> source{};
        alignas(4) std::array<char, 32> actual{};
        std::array<char, 32> expected{};
        for (u32 alignment = 0; alignment < 4; ++alignment)
        {
            for (u32 length = 0; length < 9; ++length)
            {
                source.fill(0);
                for (u32 index = 0; index < length; ++index)
                {
                    source[alignment + index] = static_cast<char>('a' + index);
                }
                for (u32 count = 0; count < 12; ++count)
                {
                    actual.fill('!');
                    expected.fill('!');
                    std::strncpy(expected.data() + alignment, source.data() + alignment, count);
                    if (scene_detail::crt_copy_string_count(actual.data() + alignment,
                            source.data() + alignment, count) != actual.data() + alignment || actual != expected)
                    {
                        throw Error("retail counted string copy lost alignment, truncation or zero padding");
                    }
                    actual.fill('!');
                    expected.fill('!');
                    std::memcpy(actual.data() + alignment, "pre", 4);
                    std::memcpy(expected.data() + alignment, "pre", 4);
                    std::strncat(expected.data() + alignment, source.data() + alignment, count);
                    if (scene_detail::crt_append_string_count(actual.data() + alignment,
                            source.data() + alignment, count) != actual.data() + alignment || actual != expected)
                    {
                        throw Error("retail counted string append lost alignment or termination");
                    }
                }
            }
        }
    }
    {
        const std::array<i32, 4> ascending{10, 20, 30, 40};
        const std::array<i32, 4> descending{40, 30, 20, 10};
        if (dialogue_score_tier(-1, ascending) != 1 || dialogue_score_tier(9, ascending) != 2 ||
            dialogue_score_tier(10, ascending) != 3 || dialogue_score_tier(20, ascending) != 4 ||
            dialogue_score_tier(30, ascending) != 5 || dialogue_score_tier(40, ascending) != 6 ||
            dialogue_score_tier(41, descending) != 2 || dialogue_score_tier(40, descending) != 3 ||
            dialogue_score_tier(30, descending) != 4 || dialogue_score_tier(20, descending) != 5 ||
            dialogue_score_tier(10, descending) != 6)
        {
            throw Error("dialogue score tiers lost sentinel, ordering or inclusive boundaries");
        }
        std::array<u8, 0x20> bronze{};
        const char* name = "aid_texture_ghoulies_scoreboard_challenge_bronzetrophy";
        AnimationResourceRegistry registry;
        registry.loaded.push_back(AnimationLoadedResource{bronze});
        registry.records.push_back(AnimationResourceRecord{1, 0, name, animation_resource_hash(name)});
        ResourceErrorState error{};
        DialogueOwnedResources state;
        u32 flags = 0x400;
        char scratch[128];
        DialogueTrophyContext context{ascending, state, flags, scratch, g_retail_dialogue_token_names, registry, error};
        const i32 scores[]{-1, 10};
        select_final_dialogue_trophy(scores, 1, context);
        if (state.icon != 82 || state.icon_resource.data() != bronze.data() ||
            registry.records[0].references != 1 || flags != 0x408)
        {
            throw Error("dialogue final trophy lost selected score, icon resource or tag flags");
        }
        select_high_dialogue_trophy(10, context);
        if (registry.records[0].references != 1)
        {
            throw Error("dialogue repeated icon selection acquired another reference");
        }
        select_high_dialogue_trophy(-1, context);
        if (state.icon != 82 || flags != 0x408)
        {
            throw Error("dialogue invalid score changed an existing trophy");
        }
        select_dialogue_icon(state, 0x36, 0, scratch, g_retail_dialogue_token_names, registry, error);
        if (state.icon != 0x36 || state.icon_resource.data() != nullptr || registry.records[0].references != 0)
        {
            throw Error("dialogue icon sentinel did not release the prior trophy texture");
        }
        mark_dialogue_pause_icon(0x4B, flags);
        mark_dialogue_pause_icon(0x4C, flags);
        mark_dialogue_pause_icon(0x4D, flags);
        mark_dialogue_pause_icon(0x4E, flags);
        mark_dialogue_pause_icon(0x4F, flags);
        if (flags != 0x7F8)
        {
            throw Error("dialogue pause icons lost substitution flag bits");
        }
    }
    {
        if (compare_dialogue_wide_prefix(nullptr, nullptr, 0) != 0 ||
            compare_dialogue_wide_prefix(u"boy suffix", u"boy", 3) != 0 ||
            compare_dialogue_wide_prefix(u"a", u"ab", 3) != -98 ||
            compare_dialogue_wide_prefix(u"\uFFFF", u"a", 1) != 65438)
        {
            throw Error("dialogue wide prefix comparison lost count, termination or unsigned character subtraction");
        }
        DialogueTokenTable owned_tokens;
        initialize_dialogue_tokens(owned_tokens, g_retail_dialogue_token_names);
        if (find_dialogue_token(u"boy}", owned_tokens) != 0 ||
            find_dialogue_token(u"SpeakerMAX}", owned_tokens) != 18 ||
            find_dialogue_token(u"sfx sound}", owned_tokens) != 91)
        {
            throw Error("owned dialogue token initialization lost prefix lookup or table order");
        }
        for (const auto* token : owned_tokens)
        {
            engine_heap_free(const_cast<char16_t*>(token));
        }
        char formatted[128];
        format_dialogue_resource_name(formatted, "AID_%s_%d", "Boy", 7);
        if (std::strcmp(formatted, "aid_boy_7") != 0)
        {
            throw Error("dialogue resource formatting lost varargs or ASCII lowercase conversion");
        }
        std::array<u8, 0x20> boy{};
        std::array<u8, 0x20> girl{};
        boy[0x11] = 19;
        girl[0x11] = 23;
        const char* boy_name = "aid_texture_ghoulies_scoreboard_dialogheads_boy";
        const char* girl_name = "aid_texture_ghoulies_scoreboard_dialogheads_girl";
        AnimationResourceRegistry registry;
        registry.loaded.push_back(AnimationLoadedResource{boy});
        registry.loaded.push_back(AnimationLoadedResource{girl});
        registry.records.push_back(AnimationResourceRecord{1, 0, boy_name, animation_resource_hash(boy_name)});
        registry.records.push_back(AnimationResourceRecord{2, 0, girl_name, animation_resource_hash(girl_name)});
        ResourceErrorState error{};
        DialogueOwnedResources state;
        state.previous_speaker = 12;
        state.next_speaker = 13;
        state.speaker_frame = 99;
        state.speaker_state = 99;
        select_dialogue_speaker(state, 0, 0, 0, g_retail_dialogue_token_names, registry, error);
        if (state.speaker != 0 || state.speaker_resource.data() != boy.data() ||
            state.speaker_frame != 0 || state.speaker_height != 19 || state.speaker_state != 0 ||
            registry.records[0].references != 1 || state.previous_speaker != 12 || state.next_speaker != 13)
        {
            throw Error("dialogue speaker selection lost texture dimensions, ownership or retained speaker fields");
        }
        select_dialogue_speaker(state, 0, 8, 0, g_retail_dialogue_token_names, registry, error);
        if (state.speaker != 1 || state.speaker_resource.data() != girl.data() || state.speaker_height != 23 ||
            registry.records[0].references != 0 || registry.records[1].references != 1)
        {
            throw Error("dialogue alternate mode did not swap speaker heads and release the prior texture");
        }
        select_dialogue_speaker(state, 1, 0, 0x200000, g_retail_dialogue_token_names, registry, error);
        if (state.speaker != 0 || state.speaker_resource.data() != boy.data() || registry.records[1].references != 0)
        {
            throw Error("dialogue scene swap flag did not select the opposite speaker head");
        }
        select_dialogue_speaker(state, -1, 0, 0, g_retail_dialogue_token_names, registry, error);
        if (state.speaker != -1 || state.speaker_resource.data() != nullptr ||
            state.speaker_state != 0 || state.speaker_height != 19 || registry.records[0].references != 0)
        {
            throw Error("dialogue no-speaker selection lost cleanup or changed retained dimensions");
        }
    }
    {
        std::array<u8, 0x88> pad{};
        std::array<u8, 0x80> keyboard{};
        std::array<u8, 8> masks{};
        const u32 kind = 5;
        const u32 active = 1;
        std::memcpy(pad.data(), &kind, 4);
        std::memcpy(pad.data() + 4, &active, 4);
        RuntimeDialogueInputDevice device{pad, {}};
        RuntimeDialogueInputState input{keyboard, masks, 0x10, 1, 0, nullptr, 1};
        pad[0x48] = 3;
        if (runtime_input_button_held(device, 1, input) != 1 ||
            runtime_input_button_edge(device, 1, input) != 1)
        {
            throw Error("dialogue device input lost held buttons or a new edge");
        }
        pad[0x84] = 2;
        if (runtime_input_button_edge(device, 3, input) != 0 ||
            runtime_input_button_edge(device, 1, input) != 1)
        {
            throw Error("dialogue edge did not suppress a multi-bit mask with a previous held bit");
        }
        std::array<u8, 0x88> disconnected = pad;
        disconnected[4] = 0;
        const u32 absent = 0xFFFFFFFFu;
        std::memcpy(disconnected.data() + 8, &absent, 4);
        RuntimeDialogueInputDevice missing{disconnected, {}};
        std::array<u8, 0x18> combined_record{};
        combined_record[0] = 4;
        RuntimeDialogueInputDevice combined{combined_record, {&missing, &device, &missing, &missing}};
        if (runtime_input_button_edge(combined, 1, input) != 1 ||
            runtime_input_button_held(combined, 2, input) != 1 ||
            runtime_input_device_presence(combined, input) != 0)
        {
            throw Error("dialogue aggregate input lost connected device queries");
        }
        masks[0] = 0x10;
        if (runtime_input_port_presence(device, input) != 0x10 ||
            runtime_input_device_presence(combined, input) != 1)
        {
            throw Error("dialogue presence did not preserve the retail device mask result");
        }
        const RuntimeDialogueInputDevice* selected = &device;
        GhoulySpawnPool devices{};
        devices.stride = sizeof(selected);
        devices.begin = reinterpret_cast<u8*>(&selected);
        devices.cursor = devices.begin + sizeof(selected);
        devices.limit = devices.cursor;
        input.devices = &devices;
        if (activate_dialogue_input(input) != 0 || input.activated != 0)
        {
            throw Error("dialogue activation ignored a present selected device");
        }
        masks[0] = 0;
        if (activate_dialogue_input(input) != 1 || input.activated != 1)
        {
            throw Error("dialogue activation lost the absent-device latch");
        }
        input.activated = 7;
        if (activate_dialogue_input(input) != 1 || input.activated != 1)
        {
            throw Error("dialogue activation did not normalize an existing latch");
        }
        pad[0x48] = 0;
        pad[0x84] = 0;
        const float analog[]{0.1f, -0.2f};
        std::memcpy(pad.data() + 0x10, analog, sizeof(analog));
        std::array<float, 2> direction;
        dialogue_input_direction(device, direction, input);
        if (direction != std::array<float, 2>{0.f, -0.2f})
        {
            throw Error("dialogue analog dead zone changed its strict boundary");
        }
        pad[0x48] = 0xF;
        dialogue_input_direction(device, direction, input);
        if (direction != std::array<float, 2>{-1.f, -1.f})
        {
            throw Error("dialogue digital direction lost left/down precedence");
        }
        const float raw_axes[]{40000.f, -40000.f};
        std::memcpy(pad.data() + 0x10, raw_axes, sizeof(raw_axes));
        runtime_input_axes(combined, direction, input);
        if (direction != std::array<float, 2>{0.f, 0.f})
        {
            throw Error("dialogue aggregate axes did not preserve last-child overwrite");
        }
        combined.children = {&missing, &missing, &missing, &device};
        runtime_input_axes(combined, direction, input);
        if (direction != std::array<float, 2>{1.f, -1.f})
        {
            throw Error("dialogue aggregate axes lost signed normalization limits");
        }
        pad[0] = 3;
        keyboard[0x38] = 1;
        if (runtime_input_button_edge(device, 1, input) != 1)
        {
            throw Error("dialogue keyboard input did not use its shared state block");
        }
        pad[0] = 1;
        pad[0x40] = 4;
        if (runtime_input_button_held(device, 4, input) != 1)
        {
            throw Error("dialogue controller input selected the wrong state offset");
        }
    }
    {
        auto* registry = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + sizeof(void*) * 2));
        registry->stride = sizeof(void*);
        registry->grow = 2;
        registry->begin = reinterpret_cast<u8*>(registry + 1);
        registry->cursor = registry->begin;
        registry->limit = registry->begin + sizeof(void*) * 2;
        RuntimeDialogueTextPool* pools[3];
        for (i32 index = 0; index < 3; ++index)
        {
            allocate_dialogue_text_pool(pools[index], index, 1, 0x80, registry);
            if (pools[index]->font != index || pools[index]->variant != 1 || pools[index]->flags != 0x80 ||
                pools[index]->end_time != 0.5f || pools[index]->enabled != 1 ||
                pools[index]->allocation->stride != 0x1238 || pools[index]->allocation->grow != 4 ||
                pools[index]->allocation->limit - pools[index]->allocation->begin != 0x48E0)
            {
                throw Error("dialogue text pool constructor lost its four-record capacity or defaults");
            }
        }
        release_dialogue_text_pool(pools[1], *registry);
        RuntimeDialogueTextPool* retained[2];
        std::memcpy(retained, registry->begin, sizeof(retained));
        if (registry->cursor - registry->begin != sizeof(retained) || retained[0] != pools[0] || retained[1] != pools[2])
        {
            throw Error("dialogue text pool release lost registry compaction after growth");
        }
        release_dialogue_text_pool(pools[2], *registry);
        release_dialogue_text_pool(pools[0], *registry);
        if (registry->cursor != registry->begin)
        {
            throw Error("dialogue text pool release retained a registry entry");
        }
        engine_heap_free(registry);
    }
    {
        RuntimeDialogueWidget widget{};
        widget.row_step = {2.f, 3.f};
        widget.column_step = {20.f, 30.f};
        position_dialogue_widget(widget, {5.f, 7.f});
        if (widget.positions[9] != std::array<float, 2>{23.f, 34.f})
        {
            throw Error("dialogue single-column positions lost sequential row steps");
        }
        widget.layout_mode = 1;
        position_dialogue_widget(widget, {5.f, 7.f});
        if (widget.positions[8] != std::array<float, 2>{13.f, 19.f} ||
            widget.positions[9] != std::array<float, 2>{33.f, 49.f})
        {
            throw Error("dialogue two-column positions lost alternating row steps");
        }
        const auto retained = widget.positions;
        widget.layout_mode = 2;
        position_dialogue_widget(widget, {1.f, 2.f});
        if (widget.positions[0] != std::array<float, 2>{1.f, 2.f} ||
            widget.positions[1] != retained[1] || widget.positions[9] != retained[9])
        {
            throw Error("dialogue unknown layout mode changed later positions");
        }
    }
    {
        const std::array<u32, 6> saved_converters{
            scene_detail::g_cfltcvt_tab[0], scene_detail::g_cfltcvt_tab[1],
            scene_detail::g_cfltcvt_tab[2], scene_detail::g_cfltcvt_tab[3],
            scene_detail::g_cfltcvt_tab[4], scene_detail::g_cfltcvt_tab[5]};
        scene_detail::crt_initialize_float_math(0);
        const char16_t text[] = u"\u00AC2 3 0.5\u00ACFight";
        float scale, x, y;
        if (parse_dialogue_text_transform(text, scale, x, y) != text + 1 ||
            scale != 0.5f || x != 2.f || y != 3.f)
        {
            throw Error("dialogue text prefix lost its retail transform or returned cursor");
        }
        const char16_t plain[] = u"Fight";
        if (parse_dialogue_text_transform(plain, scale, x, y) != plain ||
            scale != 1.f || x != 0.f || y != 0.f)
        {
            throw Error("plain dialogue text did not preserve the transform defaults");
        }
        for (size_t index = 0; index < 6; ++index)
        {
            scene_detail::g_cfltcvt_tab[index] = saved_converters[index];
        }
        std::array<u8, 0x20> texture{};
        std::array<RuntimeTextGlyph, 44> glyphs{};
        glyphs[0] = {texture.data(), 0xFFFF, 1, 11};
        glyphs[1] = {texture.data(), 0, 0, 8};
        glyphs[43] = {texture.data(), 0, 0, 3};
        RuntimeTextFont font{65, 108, glyphs, 32, 32};
        const RuntimeTextFont* fonts[]{&font};
        std::array<u8, 24> pairs{};
        const u16 codes[]{65, 66};
        const float horizontal = -2.f;
        const float vertical = 1.5f;
        std::memcpy(pairs.data(), codes, 4);
        std::memcpy(pairs.data() + 4, &horizontal, 4);
        std::memcpy(pairs.data() + 8, &vertical, 4);
        const u16 sentinel[]{0xFFFF, 0xFFFF};
        std::memcpy(pairs.data() + 12, sentinel, 4);
        const RuntimeTextKerning kerning[]{RuntimeTextKerning{1, pairs}};
        RuntimeTextMetrics metrics{fonts, kerning, 0.5f, 0.6f, 1.f};
        const float scales[]{2.f, 3.f};
        float kern_x = 10.f;
        float kern_y = 10.f;
        apply_runtime_text_kerning(kerning[0], 65, 66, scales, &kern_x, &kern_y);
        if (kern_x != 6.f || kern_y != 14.5f ||
            runtime_text_glyph_advance(font, 65) != 10 ||
            runtime_text_glyph_advance(font, 0) != 32 ||
            runtime_text_glyph_advance(font, 67) != 32 ||
            runtime_text_space_advance(font, 2.f, 0.5f) != 6.f ||
            runtime_text_space_advance(font, 0.f, 0.5f) != 1.5f ||
            runtime_text_character_advance(metrics, 0, 0, 0x20, 2.f) != 13.f ||
            runtime_text_word_advance(metrics, 0, u"AB ignored", 0, 2.f) != 34.f ||
            runtime_text_word_advance(metrics, 0, u"AB", u'B', 2.f) != 21.f)
        {
            throw Error("dialogue font advances, kerning or word terminators differ from retail");
        }
        if (runtime_text_line_width(metrics, 0, u"AB AB", 2.f) != 74.f ||
            runtime_text_line_width(metrics, 0, u"AB\nA", 2.f) != 34.f ||
            runtime_text_line_width(metrics, 0, u"AB\n", 2.f) != 34.f ||
            runtime_text_line_width(metrics, 0, u"", 2.f) != 0.f ||
            runtime_text_line_width(metrics, 0, u"A", std::numeric_limits<float>::quiet_NaN()) != 0.f)
        {
            throw Error("dialogue line measurement lost space scaling, line maxima or unordered comparisons");
        }
        float dimensions[2];
        if (measure_runtime_text_block(metrics, 0, u"AB AB", 2.f, 40.f, 640.f, dimensions) != 2 ||
            dimensions[0] != 34.f || dimensions[1] != 128.f ||
            measure_runtime_text_block(metrics, 0, u"", 2.f, 40.f, 640.f, dimensions) != 0 ||
            dimensions[0] != 0.f || dimensions[1] != 0.f)
        {
            throw Error("dialogue wrapping or empty scaled text dimensions differ from retail");
        }
        auto* allocation = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + 0x1238));
        allocation->stride = 0x1238;
        allocation->grow = 1;
        allocation->begin = reinterpret_cast<u8*>(allocation + 1);
        allocation->cursor = allocation->begin;
        allocation->limit = allocation->begin + 0x1238;
        std::memset(allocation->begin, 0xA5, 0x1238);
        RuntimeDialogueTextPool pool{{}, 0, 0.f, allocation, 40.f};
        i32 next_id = -1;
        u32 random = 123;
        if (create_dialogue_text_record(pool, u"AB", 100.f, 80.f, 2.f, 2.f, 3.f,
                1, 1, 1, metrics, 640.f, next_id, random) != 1 || next_id != 2 ||
            read_f32(pool.records, 0x604) != 60.f || read_f32(pool.records, 0x608) != 60.f ||
            read_u32(pool.records, 0x60C) != 0xA5A5A5A5u ||
            read_f32(pool.records, 0x1228) != 58.f || read_f32(pool.records, 0x122C) != 55.f ||
            read_f32(pool.records, 0x1230) != 84.f || read_f32(pool.records, 0x1234) != 114.f)
        {
            throw Error("dialogue text creation lost its sticky lifetime, retained arrays or panel bounds");
        }
        const auto saved_random = random;
        if (create_dialogue_text_record(pool, u"", 0.f, 0.f, 0.f, 0.f, 3.f,
                0, 0, 0, metrics, 640.f, next_id, random) != 2 ||
            pool.records.size() != 0x2470 || read_u32(pool.records, 0) != 1 ||
            read_f32(pool.records, 0x604) != 60.f || random != saved_random)
        {
            throw Error("dialogue text pool growth lost its earlier row or advanced random state for empty text");
        }
        engine_heap_free(pool.allocation);
    }
    {
        if (truncate_runtime_float(1.75L) != 1 || truncate_runtime_float(-1.75L) != -1 ||
            truncate_runtime_float(0.5L) != 0 || truncate_runtime_float(-0.5L) != 0 ||
            audio_volume_attenuation(0) != -10000 || audio_volume_attenuation(127) != 0 ||
            audio_volume_attenuation(63) != -304)
        {
            throw Error("retail dialogue attenuation or x87 integer conversion differs");
        }
        std::array<u8, 0x60> presets{};
        for (u32 index = 0; index < 12; ++index)
        {
            const u32 value = index * 17 + 1;
            std::memcpy(presets.data() + 0x30 + index * 4, &value, 4);
        }
        AnimationResourceRegistry registry;
        const char* name = "aid_misc_ghoulies_audio_reverbsettings";
        registry.loaded.push_back(AnimationLoadedResource{presets});
        registry.records.push_back(AnimationResourceRecord{1, 0, name, animation_resource_hash(name)});
        ResourceErrorState error{};
        AudioListenerSettings settings;
        settings.apply = 9;
        settings.listener = [&](const std::array<u32, 12>& values, u32 apply) -> i32
        {
            if (values[0] != 1 || values[11] != 188 || apply != 9 ||
                registry.records[0].references != 1)
            {
                throw Error("dialogue listener lost its preset fields, apply flag or resource lifetime");
            }
            return i32(0x80004005u);
        };
        settings.master_volume = [](u32 category, i32 attenuation) -> i32
        {
            if (category != 8 || attenuation != -304)
            {
                throw Error("dialogue volume lost its retail category or attenuation");
            }
            return i32(0x80004005u);
        };
        set_audio_category_volume(settings, 8, 63);
        set_audio_reverb_preset(settings, 1, registry, error);
        if (settings.preset != 1 || settings.parameters[11] != 188 ||
            registry.records[0].references != 0)
        {
            throw Error("dialogue reverb did not preserve its state and release the preset resource");
        }
        std::vector<std::pair<u32, i32>> volumes;
        settings.master_volume = [&](u32 category, i32 attenuation) -> i32
        {
            volumes.emplace_back(category, attenuation);
            return 0;
        };
        set_dialogue_audio_duck(settings, 1, 0, 2, 8);
        set_dialogue_audio_duck(settings, 0, 0, 2, 8);
        set_dialogue_audio_duck(settings, 1, 0x20, 2, 8);
        const std::vector<std::pair<u32, i32>> expected{{2, -10000}, {8, -304}, {2, 0}, {8, 0}};
        if (volumes != expected)
        {
            throw Error("dialogue audio duck did not honor its categories, restore or attract bypass");
        }
    }
    {
        std::array<u8, 0x2470> records{};
        const u32 id = 31;
        const u32 sticky = 1;
        const char16_t text[] = u"AB";
        std::memcpy(records.data() + 0x1238, &id, 4);
        std::memcpy(records.data() + 0x123C, text, sizeof(text));
        std::memcpy(records.data() + 0x1238 + 0x121C, &sticky, 4);
        std::array<u8, 0x20> first_texture{};
        std::array<u8, 0x20> second_texture{};
        first_texture[0x10] = 4;
        first_texture[0x11] = 8;
        second_texture[0x10] = 1;
        second_texture[0x11] = 20;
        const RuntimeTextGlyph glyphs[]{{first_texture.data(), 0xFFFF}, {second_texture.data(), 0}};
        const RuntimeTextFont font{65, 66, glyphs};
        const RuntimeTextFont* fonts[]{&font};
        for (u32 index = 0; index < 2; ++index)
        {
            const float age = 7.f;
            const float maximum = index == 0 ? 1.f : 5.f;
            std::memcpy(records.data() + 0x1238 + 0x604 + index * 4, &age, 4);
            std::memcpy(records.data() + 0x1238 + 0xA04 + index * 4, &maximum, 4);
        }
        RuntimeDialogueTextPool pool{records, 0, 0.f};
        u32 random = 123;
        if (end_dialogue_text_record(pool, 99, fonts, nullptr, random) != 0 || random != 123 ||
            end_dialogue_text_record(pool, id, fonts, nullptr, random) != 1 ||
            read_f32(records, 0x1238 + 0x604) != 1.f ||
            read_f32(records, 0x1238 + 0x608) < 2.f || read_f32(records, 0x1238 + 0x608) >= 2.25f ||
            read_u32(records, 0x1238 + 0x121C) != 0)
        {
            throw Error("dialogue text ending lost record lookup, first-glyph duration or age clamps");
        }
        const auto saved_random = random;
        records[0x123C] = 0;
        records[0x123D] = 0;
        std::memcpy(records.data() + 0x1238 + 0x121C, &sticky, 4);
        if (end_dialogue_text_record(pool, id, fonts, nullptr, random) != 1 ||
            random != saved_random || read_u32(records, 0x1238 + 0x121C) != 0)
        {
            throw Error("empty dialogue text did not clear its sticky flag without advancing random state");
        }
    }
    {
        const auto& tokens = g_retail_dialogue_tokens;
        std::array<u8, 0x310> source{};
        std::array<u8, 0x310> destination;
        destination.fill(0xA5);
        std::memcpy(source.data(), "dialogue", 9);
        for (u32 index = 0; index < 10; ++index)
        {
            const u32 icon = index + 10;
            const u32 option = index + 20;
            std::memcpy(source.data() + 0x40 + index * 0x48, &icon, 4);
            std::memcpy(source.data() + 0x44 + index * 0x48, "Accept", 7);
            std::memcpy(source.data() + 0x84 + index * 0x48, &option, 4);
        }
        copy_dialogue_widget_parameters(destination, source.data());
        if (std::strcmp(reinterpret_cast<const char*>(destination.data()), "dialogue") != 0 ||
            destination[9] != 0xA5)
        {
            throw Error("dialogue widget copy changed bytes after the name terminator");
        }
        for (u32 index = 0; index < 10; ++index)
        {
            if (read_u32(destination, 0x40 + index * 0x48) != index + 10 ||
                read_u32(destination, 0x84 + index * 0x48) != index + 20 ||
                std::strcmp(reinterpret_cast<const char*>(destination.data() + 0x44 + index * 0x48), "Accept") != 0 ||
                destination[0x4B + index * 0x48] != 0xA5)
            {
                throw Error("dialogue widget copy lost a row or overwrote its retained text tail");
            }
        }
        const auto retained = destination;
        source[0] = 0;
        copy_dialogue_widget_parameters(destination, source.data());
        copy_dialogue_widget_parameters(destination, nullptr);
        if (destination != retained)
        {
            throw Error("empty dialogue widget parameters changed the existing state");
        }
        if (find_dialogue_token(u"butler}", tokens) != 2 ||
            find_dialogue_token(u"unknown}", tokens) != 0x5C ||
            find_next_dialogue_speaker(u"{boy}Hello{page}{butler}Next", 0, tokens) != 2 ||
            find_next_dialogue_speaker(u"{boy}Hello{end}", 0, tokens) != -1 ||
            find_next_dialogue_speaker(u"{page}Next{page}", 2, tokens) != 2 ||
            find_next_dialogue_speaker(u"", 2, tokens) != -1)
        {
            throw Error("dialogue token prefix or next-page speaker gate differs from retail");
        }
        i32 enabled = 7;
        set_dialogue_button_enabled(1, 0, enabled);
        if (enabled != 0)
        {
            throw Error("dialogue button ignored the page's button permission");
        }
        set_dialogue_button_enabled(1, 1, enabled);
        if (enabled != 1 || xapi_wide_text_length(nullptr) != 0)
        {
            throw Error("dialogue button or null wide-text length differs from retail");
        }
    }
    {
        std::array<u8, 384> time_file{};
        const u32 time_chunk = 16;
        const u32 time_count = 8;
        std::memcpy(time_file.data() + 12, &time_chunk, 4);
        std::memcpy(time_file.data() + 20, &time_count, 4);
        const char* names[]{"time__secondSingular", "time__secondPlural",
            "time__minuteSingular", "time__minutePlural", "time__hourSingular",
            "time__hourPlural", "time__daySingular", "time__dayPlural"};
        const char16_t* labels[]{u"sec", u"secs", u"min", u"mins", u"hr", u"hrs", u"day", u"days"};
        std::array<RuntimeLocalTextKey, 8> time_keys{};
        u32 text_offset = 0;
        for (u16 index = 0; index < 8; ++index)
        {
            const u16 id = index + 1;
            std::memcpy(time_file.data() + 24 + index * 6, &id, 2);
            std::memcpy(time_file.data() + 26 + index * 6, &text_offset, 4);
            const size_t units = std::u16string_view(labels[index]).size() + 1;
            std::memcpy(time_file.data() + 78 + text_offset * 2, labels[index], units * 2);
            text_offset += static_cast<u32>(units);
            time_keys[index] = {names[index], u16(runtime_localtext_hash(names[index])), id};
        }
        AnimationResourceRegistry time_registry;
        const char* time_name = "aid_loctext_ghoulies_time";
        time_registry.loaded.push_back(AnimationLoadedResource{time_file});
        time_registry.records.push_back(AnimationResourceRecord{1, 0, time_name, animation_resource_hash(time_name)});
        ResourceErrorState time_error{};
        const std::unordered_map<const u8*, LocalTextResourceView> time_views{
            {time_file.data(), LocalTextResourceView{time_file, time_keys}}};
        LocalTextResourceContext time_context{time_registry, time_error, time_views};
        const std::array<float, 7> values{0.f, 1.f, 62.f, 3661.f, 86462.f, 172801.f, -62.f};
        const char16_t* expected[]{u"0 secs", u"1 sec", u"1 min 2 secs", u"1 hr 1 min 1 sec",
            u"1 day 0 hrs 1 min 2 sec", u"2 days 0 hrs 0 mins 1 secs", u"-1 mins -2 secs"};
        char16_t formatted_time[128];
        for (size_t index = 0; index < values.size(); ++index)
        {
            copy_xapi_wide_text(formatted_time, u"T: ");
            append_dialogue_seconds(formatted_time, values[index], 0, time_context);
            if (std::u16string(formatted_time) != std::u16string(u"T: ") + expected[index]
                || time_registry.records[0].references != 0)
            {
                throw Error("localized dialogue time lost its unit branch, append or resource release");
            }
        }
        const std::array<i32, 2> scores{-1, 86462};
        char16_t* result = nullptr;
        dialogue_final_time(result, formatted_time, scores.data(), 0, time_context);
        if (std::u16string(result) != u"?")
        {
            throw Error("unset dialogue time lost its retail fallback");
        }
        dialogue_final_time(result, formatted_time, scores.data(), 1, time_context);
        if (std::u16string(result) != u"24 hrs 1 min 2 secs")
        {
            throw Error("final dialogue time did not omit the day split");
        }
        dialogue_elapsed_time(result, formatted_time, 3600, 60.f, time_context);
        if (result != formatted_time || std::u16string(result) != u"1 hr 1 min")
        {
            throw Error("elapsed dialogue time lost minute precision or its accumulated seconds");
        }
        const std::array<float, 4> minute_values{0.f, 60.f, 3600.f, 90060.f};
        const char16_t* minute_expected[]{u"0 mins", u"1 min", u"1 hr 0 mins", u"1 day 1 hr 1 min"};
        for (size_t index = 0; index < minute_values.size(); ++index)
        {
            formatted_time[0] = 0;
            append_dialogue_minutes(formatted_time, minute_values[index], 0, time_context);
            if (std::u16string(formatted_time) != minute_expected[index]
                || time_registry.records[0].references != 0)
            {
                throw Error("minute-precision dialogue time lost its zero, singular or day branch");
            }
        }
        copy_xapi_wide_text(formatted_time, u"\u03A9");
        if (append_runtime_wide_text(formatted_time, u"\u4E2D") != formatted_time
            || std::u16string(formatted_time) != u"\u03A9\u4E2D")
        {
            throw Error("wide append lost a full UTF-16 unit or the original buffer pointer");
        }
        std::array<u8, 16> icon_texture{};
        const u16 icon_width = 20;
        std::memcpy(icon_texture.data() + 8, &icon_width, 2);
        const char* icon_names[]{"aid_texture_ghoulies_scoreboard_pauseicons_clock",
            "aid_texture_ghoulies_scoreboard_pauseicons_faints",
            "aid_texture_ghoulies_scoreboard_pauseicons_prisoners",
            "aid_texture_ghoulies_scoreboard_pauseicons_skull",
            "aid_texture_ghoulies_scoreboard_pauseicons_bonusbooks"};
        std::array<std::array<u8, 16>, 5> pause_textures{};
        for (size_t icon = 0; icon < std::size(icon_names); ++icon)
        {
            const char* name = icon_names[icon];
            std::memcpy(pause_textures[icon].data() + 8, &icon_width, 2);
            const u32 id = static_cast<u32>(time_registry.loaded.size()) + 1;
            time_registry.loaded.push_back(AnimationLoadedResource{pause_textures[icon]});
            time_registry.records.push_back(AnimationResourceRecord{id, 0, name, animation_resource_hash(name)});
        }
        std::array<RuntimeTextGlyph, 95> glyphs;
        for (auto& glyph : glyphs)
        {
            glyph = {icon_texture.data(), 0, 0, 10};
        }
        glyphs['l' - 32].right = 3;
        const RuntimeTextFont font{32, 126, glyphs, 10, 20};
        const RuntimeTextFont* fonts[]{&font, &font};
        const RuntimeTextKerning kerning[]{{0, {}}, {0, {}}};
        RuntimeTextMetrics metrics{fonts, kerning, 0.5f, 0.5f, 1.f};
        DialogueTagState tag_state{};
        DialogueOwnedResources owned;
        DialogueSoundTagState sound{};
        const std::array<i32, 4> thresholds{60, 62, 3661, 86462};
        u32 flags = 99;
        char resource_name[128];
        DialogueTrophyContext trophy_context{thresholds, owned, flags, resource_name,
            g_retail_dialogue_token_names, time_registry, time_error};
        DialogueTagContext tag_context{tag_state, trophy_context, sound, g_retail_dialogue_tokens,
            g_retail_dialogue_sound_swaps, scores, 1, 62, 0, 0};
        const u8 bits[]{0x81};
        char16_t wide_scratch[2049];
        char narrow_scratch[128];
        i32 first_count = 10;
        i32 second_count = 3;
        i32 third_count = 4;
        i32 elapsed = 3600;
        DialogueMeasurementContext measurement{tag_context, metrics, time_context, {1, bits},
            first_count, 2, second_count, third_count, 1, elapsed, 60.f, 2.f, 0.5f, 0, wide_scratch, narrow_scratch};
        if (measure_dialogue_segment(u"AB", false, 2.f, measurement) != 42.f
            || measure_dialogue_segment(u"AB", true, 2.f, measurement) != 62.f
            || measure_dialogue_segment(u"'AB", true, 2.f, measurement) != 103.f
            || measure_dialogue_segment(u"AB", true, 0.f, measurement) != 17.f
            || measure_dialogue_segment(u"A B", false, 2.f, measurement) != 48.f
            || measure_dialogue_segment(u"AB{page}C", false, 2.f, measurement) != 42.f
            || measure_dialogue_segment(u"AB{linek}C", false, 2.f, measurement) != 42.f
            || measure_dialogue_segment(u"AB{end}C", false, 2.f, measurement) != 63.f
            || measure_dialogue_segment(u"{tab}A", false, 2.f, measurement) != 121.f
            || flags != 0)
        {
            throw Error("dialogue segment width lost paragraph, apostrophe, space, stop or seed behavior");
        }
        measurement.paragraph_disabled = 1;
        if (measure_dialogue_segment(u"AB", true, 2.f, measurement) != 42.f)
        {
            throw Error("dialogue segment ignored the paragraph suppression state");
        }
        measurement.paragraph_disabled = 0;
        const char16_t* substitution_tags[]{u"{bronzereq}", u"{silverreq}", u"{goldreq}", u"{platinumreq}",
            u"{bronzetime}", u"{silvertime}", u"{goldtime}", u"{platinumtime}",
            u"{finalscore}", u"{finaltime}", u"{highscore}", u"{hightime}"};
        const char16_t* substitutions[]{u"60", u"62", u"3661", u"86462", u"1 min 0 secs",
            u"1 min 2 secs", u"1 hr 1 min 1 sec", u"24 hrs 1 min 2 secs", u"62",
            u"1 min 2 secs", u"86462", u"24 hrs 1 min 2 secs"};
        for (size_t index = 0; index < std::size(substitution_tags); ++index)
        {
            const float expected = measure_dialogue_segment(substitutions[index], false, 2.f, measurement);
            if (measure_dialogue_segment(substitution_tags[index], false, 2.f, measurement) != expected)
            {
                throw Error("dialogue segment did not recursively measure its score or time substitution");
            }
        }
        const char16_t* pause_tags[]{u"{clock}", u"{faints}", u"{prisoners}", u"{skull}", u"{bonusbooks}"};
        const char16_t* pause_values[]{u"1 hr 1 min", u"3", u"5", u"12", u"2"};
        for (size_t index = 0; index < std::size(pause_tags); ++index)
        {
            const float expected = measure_dialogue_segment(pause_values[index], false, 2.f, measurement) + 140.f;
            if (measure_dialogue_segment(pause_tags[index], false, 2.f, measurement) != expected
                || flags != 0 || time_registry.records[0].references != 0)
            {
                throw Error("dialogue segment lost icon/seed width or a pause-stat substitution");
            }
        }
        release_dialogue_icon(owned, time_registry);
        std::array<u8, 24> tag_kerning{};
        const u16 tag_pair[]{u'}', u'B'};
        const float adjustment = -2.f;
        std::memcpy(tag_kerning.data(), tag_pair, 4);
        std::memcpy(tag_kerning.data() + 4, &adjustment, 4);
        const RuntimeTextKerning tagged_kerning[]{{0, {}}, {1, tag_kerning}};
        metrics.kerning = tagged_kerning;
        if (measure_dialogue_segment(u"A{left}B", false, 2.f, measurement) != 38.f
            || !std::isnan(measure_dialogue_segment(u"A", false,
                std::numeric_limits<float>::quiet_NaN(), measurement)))
        {
            throw Error("dialogue segment lost closing-tag kerning or unordered scale propagation");
        }
        metrics.kerning = kerning;
        tag_state.scale_x = 2.f;
        DialoguePageLayout page{u"AB", 0xA5, -99.f, -99, {}};
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 64.f || page.lines != 1 || page.line_widths[0] != 64.f
            || page.line_widths[1] != -1.f || page.retained != 0xA5 || metrics.spacing != 2.f)
        {
            throw Error("dialogue page lost its measured width, spacing update or untouched record word");
        }
        page.text = u"AB{linek}CD";
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 64.f || page.lines != 1 || page.line_widths[0] != 64.f || page.line_widths[1] != 44.f)
        {
            throw Error("dialogue explicit line break lost its following segment measurement: "
                + std::to_string(page.width) + ", " + std::to_string(page.lines)
                + ", " + std::to_string(page.line_widths[0]) + ", " + std::to_string(page.line_widths[1]));
        }
        page.text = u"AB{para}CD";
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 64.f || page.lines != 1 || page.line_widths[1] != 64.f)
        {
            throw Error("dialogue paragraph break lost its next enlarged initial");
        }
        page.text = u"AB{linek}CD{left}";
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 64.f || page.lines != 2 || page.line_widths[1] != 44.f)
        {
            throw Error("dialogue page lost the final line increment after a later tag replaces its break flags");
        }
        page.text = u"AB CD";
        measure_dialogue_page(page, 5, 100.f, measurement);
        if (page.width != 64.f || page.lines != 2 || page.line_widths[0] != 114.f || page.line_widths[1] != 64.f)
        {
            throw Error("dialogue wrapping lost retail line slots or its separate maximum width");
        }
        measure_dialogue_page(page, 1, 100.f, measurement);
        if (page.width != 64.f || page.lines != 2 || page.line_widths[1] != -1.f)
        {
            throw Error("dialogue wrap limit lost its retail final line-count increment");
        }
        measure_dialogue_page(page, 5, std::numeric_limits<float>::quiet_NaN(), measurement);
        if (page.width != 114.f || page.lines != 1)
        {
            throw Error("dialogue wrapping treated an unordered width as an overflow");
        }
        page.text = u"AB{page}C";
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 64.f || page.lines != 1)
        {
            throw Error("dialogue page did not stop at the page tag");
        }
        page.text = u"AB{linek}C";
        measure_dialogue_page(page, 1, 640.f, measurement);
        if (page.width != 64.f || page.lines != 1 || page.line_widths[1] != -1.f)
        {
            throw Error("dialogue line break exceeded its permitted line slots");
        }
        page.text = u"";
        measure_dialogue_page(page, 0, 640.f, measurement);
        if (page.width != 0.f || page.lines != 1 || page.line_widths[0] != 0.f)
        {
            throw Error("empty dialogue page lost its retail single-line result");
        }
        tag_state.scale_x = 0.f;
        page.text = u"AB";
        measure_dialogue_page(page, 5, 640.f, measurement);
        if (page.width != 14.f || page.line_widths[0] != 19.f)
        {
            throw Error("default-scale dialogue page collapsed its distinct initial and layout measurements");
        }
        tag_state.scale_x = 2.f;
        measurement.paragraph_disabled = 1;
        for (size_t index = 0; index < std::size(substitution_tags); ++index)
        {
            const float expected = measure_dialogue_segment(substitutions[index], false, 2.f, measurement);
            page.text = substitution_tags[index];
            measure_dialogue_page(page, 5, 640.f, measurement);
            if (page.width != expected || page.lines != 1 || page.line_widths[0] != expected || flags != 0)
            {
                throw Error("dialogue page lost a recursively measured score/time substitution");
            }
        }
        for (size_t index = 0; index < std::size(pause_tags); ++index)
        {
            const float expected = measure_dialogue_segment(pause_values[index], false, 2.f, measurement) + 140.f;
            page.text = pause_tags[index];
            measure_dialogue_page(page, 5, 640.f, measurement);
            if (page.width != expected || page.line_widths[0] != expected || flags != 0)
            {
                throw Error("dialogue page lost pause icon, seed or stat widths");
            }
        }
        DialoguePageSettings page_settings{};
        const char16_t prepared_text[] = u" \t\r\n{nohead}{nobutton}~AB";
        owned.previous_speaker = 17;
        owned.next_speaker = 18;
        reset_dialogue_page(page, prepared_text, 0, 640.f, page_settings, measurement);
        if (page.text != prepared_text + std::size(prepared_text) - 3 ||
            page_settings.characters != 100 || page_settings.line_limit != 5 ||
            page_settings.initial_alignment != 0 || tag_state.buttons != 0 ||
            tag_state.heads != 0 || tag_state.time != 0.1f ||
            tag_state.scale_x != 0.6f || tag_state.scale_y != 0.6f ||
            owned.previous_speaker != 17 || owned.next_speaker != 18 ||
            std::strcmp(measurement.tags.sound.cue.data(), "XACT_SOUNDBANK_GMISC_UNSPECIFIED") != 0)
        {
            throw Error("dialogue page reset lost leading tags, tilde handling or disabled-head state");
        }
        const char16_t* retained_text = page.text;
        prepare_dialogue_page(page, u" \t\r\n", 5, 640.f, measurement);
        if (page.text != retained_text)
        {
            throw Error("whitespace-only dialogue setup replaced the retained page text");
        }
        reset_dialogue_page(page, u"AB{page}{butler}Next", 1, 640.f, page_settings, measurement);
        if (page_settings.line_limit != 4 || tag_state.buttons != 1 || tag_state.heads != 1 ||
            owned.previous_speaker != -1 || owned.next_speaker != 2)
        {
            throw Error("dialogue page reset lost entry limits or next-page speaker lookup");
        }
        release_dialogue_icon(owned, time_registry);
        for (const auto& record : time_registry.records)
        {
            if (record.references != 0)
            {
                throw Error("dialogue measurement left a released texture or text reference live");
            }
        }
    }
    {
        std::array<u8, 64> file{};
        const u32 chunk_offset = 16;
        const u32 count = 2;
        std::memcpy(file.data() + 12, &chunk_offset, 4);
        std::memcpy(file.data() + 20, &count, 4);
        const u16 ids[]{7, 19};
        const u32 offsets[]{0, 6};
        for (size_t index = 0; index < 2; ++index)
        {
            std::memcpy(file.data() + 24 + index * 6, &ids[index], 2);
            std::memcpy(file.data() + 26 + index * 6, &offsets[index], 4);
        }
        const char16_t text[] = u"Fight\0Door";
        std::memcpy(file.data() + 42, text, sizeof(text));
        RuntimeLocalTextState state{};
        initialize_runtime_localtext(file, state);
        const char* key = "dialogs__fight";
        const RuntimeLocalTextKey keys[]{
            {"dialogs__collision", u16(runtime_localtext_hash(key)), 19},
            {key, u16(runtime_localtext_hash(key)), 7}};
        const char16_t* output = nullptr;
        if (find_runtime_localtext(keys, state, key, output) != 7 ||
            std::u16string_view(output) != u"Fight" ||
            std::u16string_view(runtime_localtext_by_id(&state, 19)) != u"Door" ||
            *runtime_localtext_by_id(&state, 8) != 0)
        {
            throw Error("retail dialogue lookup lost its collision mapping or sorted text ID");
        }
        AnimationResourceRegistry registry;
        const char* resource_name = "aid_loctext_ghoulies_dialog_dialogs";
        registry.loaded.push_back(AnimationLoadedResource{file});
        registry.records.push_back(AnimationResourceRecord{
            1, 0, resource_name, animation_resource_hash(resource_name)});
        ResourceErrorState error{};
        const std::unordered_map<const u8*, LocalTextResourceView> views{
            {file.data(), LocalTextResourceView{file, keys}}};
        LocalTextResourceContext context{registry, error, views};
        char16_t copied[32]{};
        const auto borrowed = acquire_animation_resource(registry, resource_name);
        if (lookup_borrowed_localtext(borrowed, "dialogs", "fi-ght!", copied, context) != 1 ||
            std::u16string_view(copied) != u"Fight" || registry.records[0].references != 1 ||
            lookup_borrowed_localtext(borrowed, "dialogs", "missing", copied, context) != 0 ||
            copied[0] != 0 || registry.records[0].references != 1)
        {
            throw Error("borrowed widget text lookup changed resource ownership or its lookup result");
        }
        release_animation_resource(registry, borrowed);
        std::array<RuntimeTextGlyph, 1> empty_glyphs{};
        const RuntimeTextFont widget_font{0, 0, empty_glyphs, 32, 32};
        const RuntimeTextFont* widget_fonts[]{&widget_font};
        const RuntimeTextKerning widget_kerning[]{RuntimeTextKerning{0, {}}};
        const RuntimeTextMetrics metrics{widget_fonts, widget_kerning, 0.6f, 0.6f, 0.f};
        auto* widget_allocation = static_cast<GhoulySpawnPool*>(engine_heap_allocate(sizeof(GhoulySpawnPool) + 0x1238));
        widget_allocation->stride = 0x1238;
        widget_allocation->grow = 1;
        widget_allocation->begin = reinterpret_cast<u8*>(widget_allocation + 1);
        widget_allocation->cursor = widget_allocation->begin;
        widget_allocation->limit = widget_allocation->begin + 0x1238;
        RuntimeDialogueTextPool widget_pool{{}, 0, 0.f, widget_allocation, 640.f};
        RuntimeDialogueWidget widget{};
        widget.pool = &widget_pool;
        widget.duration = 0.3f;
        widget.scale = 0.6f;
        std::memcpy(widget.group.data(), "dialogs", 8);
        widget.text_resource = acquire_animation_resource(registry, resource_name);
        widget.icons[0] = acquire_animation_resource(registry, resource_name);
        std::array<u8, 0x310> prompts{};
        std::memcpy(prompts.data(), "prompt", 7);
        const u32 text_only = 9;
        const u32 terminator = 0xFFFFFFFFu;
        std::memcpy(prompts.data() + 0x40, &text_only, 4);
        std::memcpy(prompts.data() + 0x44, "fight", 6);
        std::memcpy(prompts.data() + 0x88, &terminator, 4);
        i32 widget_id = 1;
        u32 widget_random = 1;
        RuntimeDialogueWidgetContext widget_context{context, metrics, {}, nullptr, 640.f, widget_id, widget_random};
        char16_t width_scratch[256]{};
        const auto retained_references = registry.records[0].references;
        if (measure_dialogue_widget_width(resource_name, prompts.data(), 0.5f, 0.5f,
                "dialogs", width_scratch, widget_context) != 116.f ||
            registry.records[0].references != retained_references ||
            std::u16string_view(width_scratch) != u"Fight")
        {
            throw Error("dialogue widget width lost text measurement, padding or borrowed resource lifetime");
        }
        std::array<u8, 0x310> unterminated_prompts{};
        if (measure_dialogue_widget_width(resource_name, unterminated_prompts.data(), 0.5f, 0.5f,
                "dialogs", width_scratch, widget_context) != 36.f ||
            width_scratch[0] != 0 || registry.records[0].references != retained_references)
        {
            throw Error("dialogue widget width treated ten rows without a sentinel as active prompts");
        }
        set_dialogue_widget_prompt(widget, prompts.data(), widget_context);
        if (widget.mode != 1 || widget.count != 1 || widget.progress != 0.3f ||
            widget.requested_set != nullptr || widget.current_set != widget.new_parameters.data() ||
            std::u16string_view(widget.labels[0].text.data()) != u"Fight" ||
            widget.labels[0].id != 1 || registry.records[0].references != 1 ||
            widget.icons[0].data() != nullptr || read_u32(widget_pool.records, 0x121C) != 1)
        {
            throw Error("dialogue widget activation lost its prompt, label lookup, text record or icon ownership");
        }
        set_dialogue_widget_prompt(widget, nullptr, widget_context);
        if (widget.mode != 3 || read_u32(widget_pool.records, 0x121C) != 0)
        {
            throw Error("dialogue widget closing did not end the existing prompt text");
        }
        set_dialogue_widget_mode(widget, 1, widget_context);
        if (widget.current_set != nullptr || widget.old_set != nullptr || widget.count != 0 || widget.progress != 0.f)
        {
            throw Error("dialogue widget finishing did not clear its disabled prompt set");
        }
        release_animation_resource(registry, widget.text_resource);
        engine_heap_free(widget_pool.allocation);
        auto* widget_registry = static_cast<GhoulySpawnPool*>(
            engine_heap_allocate(sizeof(GhoulySpawnPool) + sizeof(void*)));
        widget_registry->stride = sizeof(void*);
        widget_registry->grow = 1;
        widget_registry->begin = reinterpret_cast<u8*>(widget_registry + 1);
        widget_registry->cursor = widget_registry->begin;
        widget_registry->limit = widget_registry->begin + sizeof(void*);
        auto& world = entity_detail::g_world;
        const auto saved_world = world;
        world = {};
        Actor bat{};
        ActorLink bat_link{};
        bat.link = &bat_link;
        entity_detail::ShapeProbe bat_shape{};
        entity_detail::SegmentProbe bat_segment{};
        RuntimeActorLink bat_view{};
        ActorRenderRegistration bat_registration{bat, bat_shape, bat_segment, bat_view};
        bat.render_registration = &bat_registration;
        Actor bat_child{};
        ActorLink child_link{};
        child_link.actor = &bat_child;
        bat_link.child = &child_link;
        entity_detail::ShapeProbe child_shape{};
        entity_detail::SegmentProbe child_segment{};
        RuntimeActorLink child_view{};
        ActorRenderRegistration child_registration{bat_child, child_shape, child_segment, child_view};
        bat_child.render_registration = &child_registration;
        static u32 bat_messages;
        bat_messages = 0;
        bat_registration.message = [](ActorRenderRegistration&, u32 id)
        {
            if (id == 0x80)
            {
                ++bat_messages;
            }
        };
        child_registration.message = bat_registration.message;
        for (u32 owns_text : {0u, 1u})
        {
            auto* closing = new (engine_heap_allocate(sizeof(RuntimeDialogueWidget))) RuntimeDialogueWidget{};
            closing->owns_text_resource = owns_text;
            closing->text_resource = acquire_animation_resource(registry, resource_name);
            closing->icons[0] = acquire_animation_resource(registry, resource_name);
            closing->icons[9] = acquire_animation_resource(registry, resource_name);
            closing->bat = &bat;
            allocate_dialogue_text_pool(closing->pool, 0, 1, 0x80, widget_registry);
            destroy_dialogue_widget(closing, registry, *widget_registry);
            if (registry.records[0].references != 1u - owns_text ||
                widget_registry->cursor != widget_registry->begin || bat_messages != 2 ||
                child_registration.child.prev != &world.child || world.child.next != &child_registration.child ||
                bat_registration.child.prev != &child_registration.child ||
                child_registration.child.next != &bat_registration.child)
            {
                throw Error("dialogue widget destruction lost resource ownership, text pool release or bat registration");
            }
            if (owns_text == 0)
            {
                release_animation_resource(registry, file);
            }
        }
        static i32 allocated_widget_mode;
        DialogueWidgetAllocationContext construction{widget_context, widget_registry,
            [](i32 size) -> void*
            {
                void* storage = engine_heap_allocate(size);
                std::memset(storage, 0x5A, size);
                std::memcpy(storage, &allocated_widget_mode, sizeof(allocated_widget_mode));
                const i32 retained_count = 42;
                std::memcpy(static_cast<u8*>(storage) + offsetof(RuntimeDialogueWidget, count),
                    &retained_count, sizeof(retained_count));
                return storage;
            }};
        allocated_widget_mode = 0;
        RuntimeDialogueWidget* created_widget;
        create_dialogue_widget(created_widget, 0, nullptr, {20.f, 30.f}, {2.f, 3.f},
            {4.f, 5.f}, nullptr, 1.5f, 0.75f, "dialogs", {}, 0x80, construction);
        if (created_widget->mode != 1 || created_widget->count != 0 || created_widget->duration != 0.f ||
            created_widget->column_step != std::array<float, 2>{0.f, 0.f} ||
            created_widget->positions[1] != std::array<float, 2>{22.f, 33.f} ||
            created_widget->origin != std::array<float, 2>{4.f, 5.f} ||
            created_widget->transition_flags != 0xFFFFFFFFu || created_widget->sound_enabled != 1 ||
            created_widget->pool->flags != 0x80 || created_widget->owns_text_resource != 0 ||
            std::strcmp(created_widget->sound_name.data(), "XACT_SOUNDBANK_GSUPERSCARYSTUF_RIGHTBUTTON") != 0)
        {
            throw Error("widget construction lost heap-state transition, font fallback or retail layout defaults");
        }
        destroy_dialogue_widget(created_widget, registry, *widget_registry);
        std::array<u8, 0x20> timing_texture{};
        timing_texture[0x10] = 4;
        timing_texture[0x11] = 8;
        const RuntimeTextGlyph timing_glyphs[]{{timing_texture.data(), 0xFFFF, 0, 32}};
        const RuntimeTextFont timing_font{65, 65, timing_glyphs, 32, 32};
        const RuntimeTextFont* timing_fonts[]{&timing_font};
        const RuntimeTextMetrics timing_metrics{timing_fonts, widget_kerning, 0.6f, 0.6f, 0.f};
        RuntimeDialogueWidgetContext timing_context{context, timing_metrics, {}, nullptr,
            640.f, widget_id, widget_random};
        DialogueWidgetAllocationContext timed_construction{timing_context, widget_registry, construction.allocate};
        const std::array<float, 2> column_step{10.f, 20.f};
        create_resource_dialogue_widget(created_widget, 1, prompts.data(), {20.f, 30.f},
            {2.f, 3.f}, {4.f, 5.f}, &column_step, 1.5f, 0.75f, "dialogs",
            resource_name, 0x80, timed_construction);
        if (created_widget->mode != 1 || created_widget->count != 1 || created_widget->duration != 2.f ||
            created_widget->progress != 2.f || created_widget->owns_text_resource != 1 ||
            registry.records[0].references != 1 ||
            created_widget->positions[1] != std::array<float, 2>{30.f, 50.f} ||
            created_widget->positions[2] != std::array<float, 2>{22.f, 33.f})
        {
            throw Error("owned widget construction lost the glyph timing, grid layout or initial prompt");
        }
        RuntimeDialogueInputState widget_input{};
        AudioCoreSources widget_sources{};
        AudioCoreCues widget_cues{widget_sources};
        const Vec3 widget_origin{};
        AudioEventVoices widget_voices{widget_cues, {}, {}, {}, 0, widget_origin, widget_origin};
        for (auto& voice : widget_voices.voices)
        {
            voice.state = 1;
        }
        const std::unordered_map<Actor*, DialogueWidgetBatView> widget_bats;
        static u32 widget_callback_count;
        widget_callback_count = 0;
        const std::unordered_map<u32, std::function<void()>> widget_callbacks{{0x101, []()
        {
            ++widget_callback_count;
        }}};
        const std::function<DialogueWidgetBatView(Actor*)> widget_bat_views = [&widget_bats](Actor* bat)
        {
            return widget_bats.at(bat);
        };
        DialogueWidgetUpdateContext update_context{timing_context, widget_input, widget_voices,
            widget_bat_views, widget_callbacks, 0.f, 0.5f, 1.f};
        update_dialogue_widget(*created_widget, nullptr, update_context);
        if (created_widget->mode != 1 || created_widget->progress != 1.5f || created_widget->timer != 0.375f)
        {
            throw Error("widget opening lost its duration-scaled progress");
        }
        update_context.freeze = 1.f;
        update_dialogue_widget(*created_widget, nullptr, update_context);
        if (created_widget->progress != 1.5f || created_widget->timer != 0.375f)
        {
            throw Error("widget opening ignored retail freeze");
        }
        update_context.freeze = 0.f;
        update_context.dt = 2.f;
        update_dialogue_widget(*created_widget, nullptr, update_context);
        if (created_widget->mode != 2 || created_widget->progress != 0.f || created_widget->timer != 1.5f)
        {
            throw Error("widget opening failed to clamp its timer and enter input mode");
        }
        std::array<u8, 0x88> widget_pad{};
        const u32 widget_device_kind = 5;
        std::memcpy(widget_pad.data(), &widget_device_kind, 4);
        RuntimeDialogueInputDevice widget_device{widget_pad, {}};
        const u32 button_icon = 0;
        const u32 callback_address = 0x101;
        std::memcpy(created_widget->new_parameters.data() + 0x40, &button_icon, 4);
        std::memcpy(created_widget->new_parameters.data() + 0x84, &callback_address, 4);
        const u16 button_mask = 0x0110;
        std::memcpy(widget_pad.data() + 0x48, &button_mask, 2);
        created_widget->sound_enabled = 0;
        if (update_dialogue_widget(*created_widget, nullptr, update_context) != 0 ||
            update_dialogue_widget(*created_widget, &widget_device, update_context) != 1 ||
            widget_callback_count != 1)
        {
            throw Error("widget input lost device gating, retail button masks or callback dispatch");
        }
        std::memcpy(widget_pad.data() + 0x84, &button_mask, 2);
        if (update_dialogue_widget(*created_widget, &widget_device, update_context) != 0 || widget_callback_count != 1)
        {
            throw Error("widget input treated a held button as a new edge");
        }
        const u32 axis_icon = 6;
        std::memcpy(created_widget->new_parameters.data() + 0x40, &axis_icon, 4);
        const std::array<float, 2> widget_axes{0.3f, 0.f};
        std::memcpy(widget_pad.data() + 0x10, widget_axes.data(), sizeof(widget_axes));
        if (update_dialogue_widget(*created_widget, &widget_device, update_context) != 1 || widget_callback_count != 2)
        {
            throw Error("widget input lost the directional prompt path");
        }
        set_dialogue_widget_prompt(*created_widget, nullptr, timing_context);
        update_dialogue_widget(*created_widget, nullptr, update_context);
        if (created_widget->mode != 3)
        {
            throw Error("widget closing completed before its text pool emptied");
        }
        created_widget->pool->allocation->cursor = created_widget->pool->allocation->begin;
        update_dialogue_widget(*created_widget, nullptr, update_context);
        if (created_widget->mode != 1 || created_widget->current_set != nullptr || created_widget->count != 0)
        {
            throw Error("widget closing did not finish after its text pool emptied");
        }
        ButtonBatScreen factory_bat{};
        ActorAnimationState factory_animation{};
        ActorLink factory_link{};
        factory_bat.actor.link = &factory_link;
        entity_detail::ShapeProbe factory_shape{};
        entity_detail::SegmentProbe factory_segment{};
        RuntimeActorLink factory_view{};
        ActorRenderRegistration factory_registration{factory_bat.actor, factory_shape, factory_segment, factory_view};
        factory_bat.actor.render_registration = &factory_registration;
        factory_registration.message = bat_registration.message;
        RuntimeActorLink next_factory_view{};
        auto* factory_sentinel = reinterpret_cast<RuntimeActorLink*>(&world.view);
        factory_view.previous = factory_sentinel;
        factory_view.next = &next_factory_view;
        next_factory_view.previous = &factory_view;
        world.view.next = &factory_view;
        std::unordered_map<const u8*, Actor*> factory_actors;
        struct BatConstructorFixture
        {
            ButtonBatScreen& bat;
            ActorAnimationState& animation;
            AnimationResourceRegistry& resources;
            ResourceErrorState& error;
            u32& random;
            std::unordered_map<const u8*, Actor*>& actors;
        };
        BatConstructorFixture bat_constructor{factory_bat, factory_animation, registry,
            error, widget_random, factory_actors};
        static BatConstructorFixture* constructor_fixture;
        constructor_fixture = &bat_constructor;
        static const ObjectFactoryVtable bat_vtable
        {
            0x00504C34, button_bat_object_sizes,
            [](FactoryObject& object, std::span<const u8> parameters) -> bool
            {
                auto& fixture = *constructor_fixture;
                button_bat_initialize(fixture.bat,
                    ButtonBatPoint{read_f32(parameters, 0x334), read_f32(parameters, 0x338)},
                    read_f32(parameters, 0x33C), fixture.animation, fixture.resources,
                    fixture.error, fixture.random);
                fixture.actors[object.bytes.get()] = &fixture.bat.actor;
                return true;
            }, nullptr
        };
        const ObjectFactoryType bat_types[]
        {
            {0, nullptr}, {0, []() -> const ObjectFactoryVtable*
            {
                return &bat_vtable;
            }}
        };
        ObjectFactoryRegistry bat_factory{bat_types};
        std::array<u8, 0x340> bat_parameters{};
        const u16 bat_parameter_size = 0x340;
        const u16 bat_type = 1;
        const float retained_bat_duration = 0.625f;
        std::memcpy(bat_parameters.data(), &bat_parameter_size, 2);
        std::memcpy(bat_parameters.data() + 4, &bat_type, 2);
        std::memcpy(bat_parameters.data() + 0x33C, &retained_bat_duration, 4);
        ExecutableResourceRecord bat_resource;
        bat_resource.name = "aid_objparams_ghoulies_misc_buttonbatgold";
        bat_resource.hash = animation_resource_hash(bat_resource.name.c_str());
        bat_resource.cpu = bat_parameters;
        ExecutableResourcePool bat_resources{{&bat_resource, 1}, "objparams", false};
        std::vector<FactoryObject> widget_bat_objects;
        DialogueWidgetBatFactoryContext bat_factory_context{bat_factory, {&bat_resources, 1},
            0x40, widget_bat_objects, factory_actors};
        created_widget->selected = 1;
        attach_dialogue_widget_bat(*created_widget, 2.f, 7, bat_factory_context);
        if (created_widget->bat != &factory_bat.actor || created_widget->bat_scale != 2.f ||
            created_widget->selected != 7 || widget_bat_objects.size() != 1 || bat_factory.count != 1 ||
            factory_bat.from.x != 27.f || factory_bat.from.y != 18.5f ||
            factory_bat.flight_duration != 0.625f || world.view.next != &next_factory_view ||
            next_factory_view.previous != factory_sentinel ||
            factory_view.previous != nullptr || factory_view.next != nullptr)
        {
            throw Error("widget bat attachment lost factory selection, prior row positioning or render-list unlinking");
        }
        attach_dialogue_widget_bat(*created_widget, 3.f, 4, bat_factory_context);
        if (created_widget->bat_scale != 3.f || created_widget->selected != 4 ||
            widget_bat_objects.size() != 1 || bat_factory.count != 1 || factory_bat.from.x != 27.f)
        {
            throw Error("widget bat attachment recreated an existing bat or lost its updated selection");
        }
        destroy_dialogue_widget(created_widget, registry, *widget_registry);
        constructor_fixture = nullptr;
        allocated_widget_mode = 3;
        create_resource_dialogue_widget(created_widget, 0, prompts.data(), {0.f, 0.f},
            {0.f, 0.f}, {0.f, 0.f}, nullptr, 1.f, 1.f, "dialogs", resource_name, 0, timed_construction);
        if (created_widget->mode != 3 || created_widget->count != 42 || created_widget->current_set != nullptr ||
            created_widget->requested_set != created_widget->requested_parameters.data())
        {
            throw Error("widget construction invented an initial mode or bypassed the retained-mode guard");
        }
        destroy_dialogue_widget(created_widget, registry, *widget_registry);
        if (registry.records[0].references != 0 || widget_registry->cursor != widget_registry->begin)
        {
            throw Error("constructed widgets retained resources or pool registry entries after destruction");
        }
        engine_heap_free(widget_registry);
        world = saved_world;
        request_namespaced_localtext(resource_name, "dialogs", "fi-ght!", copied, context);
        if (std::u16string_view(copied) != u"Fight" || registry.records[0].references != 0)
        {
            throw Error("namespaced dialogue request did not sanitize and resolve the retail key");
        }
        char formatted[32]{};
        if (scene_detail::crt_format_string(formatted, "%s__%s", "dialogs", "fight") != 14 ||
            std::strcmp(formatted, key) != 0 ||
            scene_detail::crt_format_string(nullptr, "%s__%s", "dialogs", "fight") != 14)
        {
            throw Error("retail string formatting lost its count or null-output count mode");
        }
        if (acquire_localtext_key(resource_name, key, copied, context) != 1 ||
            std::u16string_view(copied) != u"Fight" ||
            registry.records[0].references != 0 || context.acquired.data() != nullptr)
        {
            throw Error("dialogue text acquisition did not copy and release its owned resource");
        }
        copied[0] = u'X';
        if (acquire_localtext_key(resource_name, "dialogs__missing", copied, context) != 0 ||
            copied[0] != 0 || registry.records[0].references != 0 ||
            context.acquired.data() != file.data())
        {
            throw Error("failed dialogue lookup lost its empty result or retail release state");
        }
        DialogueOwnedResources owned;
        owned.speaker_resource = acquire_animation_resource(registry, resource_name);
        owned.icon_resource = acquire_animation_resource(registry, resource_name);
        owned.speaker = 2;
        owned.previous_speaker = 3;
        owned.next_speaker = 4;
        owned.speaker_state = 1;
        owned.icon = 7;
        if (release_dialogue_speaker(owned, registry, 0xABCDE, 8, 0x40) != 0xABCDE ||
            owned.speaker != -1 || owned.previous_speaker != -1 || owned.next_speaker != -1 ||
            owned.speaker_state != 0 || owned.speaker_resource.data() != nullptr ||
            registry.records[0].references != 1)
        {
            throw Error("dialogue speaker cleanup lost its resource ownership or mode return");
        }
        release_dialogue_icon(owned, registry);
        if (owned.icon != 0x36 || owned.icon_resource.data() != nullptr ||
            registry.records[0].references != 0)
        {
            throw Error("dialogue icon cleanup did not release and reset the owned icon");
        }
        owned.icon = 9;
        release_dialogue_icon(owned, registry);
        if (owned.icon != 9 || release_dialogue_speaker(owned, registry, 0xABCDE, 0, 0x40) != 0x40)
        {
            throw Error("empty dialogue cleanup changed the retained icon or scene-flag return");
        }
        state.disabled = 1;
        if (find_runtime_localtext(keys, state, key, output) != 0xFFFFFFFFu || *output != 0)
        {
            throw Error("disabled dialogue text did not preserve the retail failure result");
        }
        state.disabled = 0;
        const u32 empty_count = 0;
        std::memcpy(file.data() + 20, &empty_count, 4);
        if (*runtime_localtext_by_id(&state, 7) != 0)
        {
            throw Error("empty dialogue table returned an entry");
        }
        char sanitized[32]{};
        sanitize_localtext_key(sanitized, "Fight-1! _A");
        if (std::strcmp(sanitized, "Fight1_A") != 0 ||
            text_key_present(nullptr) || text_key_present("") || !text_key_present(key))
        {
            throw Error("dialogue key sanitation or presence gate differs from retail");
        }
    }
    {
        Game game;
        std::vector<u8> bytes(302 * 8);
        for (u32 index = 0; index < 302; ++index)
        {
            const u32 stride = 8;
            const u32 type = index < 300 ? 0xFFFFFFFFu : index == 300 ? 0x10u : 0u;
            std::memcpy(bytes.data() + index * 8, &stride, 4);
            std::memcpy(bytes.data() + index * 8 + 4, &type, 4);
        }
        game.scene_script = SceneScript{"vm-traversal-regression", bytes};
        u8 flags = 0;
        SceneFlagBlock scene_flags{0, 1, &flags};
        auto* saved_scene_flags = g_scene_flags;
        auto* saved_live = g_live_scripts;
        const auto saved_flags = g_flags_ea9c;
        const auto saved_end = g_script_30d0c;
        const auto saved_gate = g_gate_251c;
        g_scene_flags = &scene_flags;
        g_live_scripts = nullptr;
        g_flags_ea9c = 0;
        g_script_30d0c = 0;
        g_gate_251c = 0;
        u32 cursor = 0;
        if (script_execute_fired_action(game, cursor) != 0 || cursor != 300 * 8)
        {
            throw Error("script VM stopped on an unhandled node or imposed a host step limit");
        }
        g_gate_251c = 1;
        if (script_execute_fired_action(game, cursor) != 1 || cursor != 301 * 8 || g_gate_251c != 0)
        {
            throw Error("script VM did not resume its waiting cursor and reach the terminator");
        }
        g_scene_flags = saved_scene_flags;
        g_live_scripts = saved_live;
        g_flags_ea9c = saved_flags;
        g_script_30d0c = saved_end;
        g_gate_251c = saved_gate;
    }
    {
        std::array<u8, 0x60> inside{};
        std::array<u8, 0x60> outside{};
        std::array<u8, 0x60> shader_bytes{};
        std::array<u8, 0x60> push_bytes{};
        for (size_t offset = 0x48; offset < 0x60; offset += 4)
        {
            const float value = 1.5f;
            std::memcpy(inside.data() + offset, &value, 4);
            std::memcpy(outside.data() + offset, &value, 4);
        }
        inside[0x3F] = outside[0x3F] = push_bytes[0x3F] = 0xFF;
        shader_bytes[8] = 0xFF;
        RuntimeModelNode matrix{};
        RuntimeModelNode child{};
        RuntimeModelNode sibling{};
        RuntimeModelNode shader{};
        RuntimeModelNode push{};
        matrix.type_name = "ndMtxArray";
        matrix.child = &child;
        matrix.sibling = &sibling;
        child.type_name = sibling.type_name = "ndBGPushBuffer";
        child.sibling = &shader;
        shader.type_name = "ndShaderParam2";
        shader.child = &push;
        push.type_name = "ndPushBuffer";
        BackgroundNodeViews views{{&child, inside}, {&sibling, outside},
            {&shader, shader_bytes}, {&push, push_bytes}};
        i32 found = 0;
        initialize_background_model_nodes(matrix, 0, nullptr, found, views);
        if (found != 1 || read_f32(inside, 0x48) != -68.5f ||
            read_f32(inside, 0x5C) != 71.5f || read_f32(outside, 0x48) != 1.5f ||
            inside[0x3F] != 0 || outside[0x3F] != 0 || push_bytes[0x3F] != 0 ||
            read_u32(shader_bytes, 8) != 0)
        {
            throw Error("background node initialization lost matrix ancestry or sibling state");
        }
    }
    {
        std::array<u8, 0x100> cpu{};
        auto word = [&](size_t offset, u32 value)
        {
            std::memcpy(cpu.data() + offset, &value, sizeof(value));
        };
        word(0, 8);
        word(4, 1);
        word(8, 4);
        word(12, 0x20);
        for (size_t offset : {size_t(0x28), size_t(0x4C)})
        {
            cpu[offset] = 3;
            cpu[offset + 4] = 0x40;
            cpu[offset + 8] = 0xFF;
            cpu[offset + 9] = 0xFF;
        }
        cpu[0x29] = 0x24;
        word(0x38, std::bit_cast<u32>(1.f));
        word(0x5C, std::bit_cast<u32>(2.f));
        word(0x60, std::bit_cast<u32>(10.f));
        std::array<u8, 0x40> header{};
        u16 initial_tag = 3000;
        std::memcpy(header.data() + 0x1C, &initial_tag, 2);
        ActorModel model;
        model.cpu = cpu;
        Actor actor;
        actor.model = &model;
        std::array<u8, 0x20> shape;
        shape.fill(0xAA);
        u16 tag = 99;
        if (!build_background_animation_volume(actor, 0, header, shape, tag) ||
            tag != 3000 || shape[3] != 0xAA || read_f32(shape, 4) != 5.5f ||
            read_f32(shape, 8) != -2.f || read_f32(shape, 0x14) != 4.f ||
            read_f32(shape, 0x1C) != 6.5f)
        {
            throw Error("background volume did not merge matching spheres or preserve retained bytes");
        }
        const auto saved_shape = shape;
        tag = 99;
        if (build_background_animation_volume(actor, 1, header, shape, tag) || shape != saved_shape || tag != 99)
        {
            throw Error("missing background volume modified output state");
        }
        std::array<BackgroundAnimationVolume, 1> records{};
        entity_detail::ListHead owner;
        entity_detail::ListHead global;
        entity_detail::ListHead anchors;
        const auto saved_anchors = g_actor_model_anchors;
        g_actor_model_anchors = {};
        const Vec3 origin{1.f, 2.f, 3.f};
        initialize_background_animation_volume(actor, 0, header, records, owner, global, anchors, origin);
        const auto* attachment = records[0].attachment;
        const auto* anchor = records[0].anchor;
        if (attachment == nullptr || anchor == nullptr || attachment->actor != &actor ||
            attachment->mode != 1 || attachment->origin.y != 2.f ||
            attachment->shape != attachment->secondary_shape || anchor->joint != 0xFFFF ||
            anchor->local.x != 5.5f || anchor->radius != 6.5f ||
            read_u16(records[0].fields, 0x10) != 3000 || owner.next != attachment ||
            global.next != &attachment->global_link)
        {
            throw Error("background volume initialization lost live attachment or anchor ownership");
        }
        release_actor_live_attachment(*records[0].attachment);
        if (owner.next != nullptr || global.next != nullptr)
        {
            throw Error("background volume attachment remained linked after release");
        }
        engine_heap_free(records[0].anchor);
        g_actor_model_anchors = saved_anchors;
        alignas(8) std::array<u8, 40> overlapping{};
        for (size_t index = 0; index < overlapping.size(); ++index)
        {
            overlapping[index] = u8(index);
        }
        copy_actor_shape(overlapping.data() + 4, overlapping.data());
        if (overlapping[8] != 0 || overlapping[35] != 3)
        {
            throw Error("shape copy lost the retail forward dword overlap behavior");
        }
    }
    {
        std::array<u8, 0x38> bytes{};
        auto word = [&](size_t offset, u32 value)
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        };
        word(0x24, 3);
        word(0x30, 2);
        // 0x000400D0's gate: model CPU +0x10 -> +0x10 (here +0x14 + 0x10, the 3 at +0x24) must be set.
        word(0x10, 0x14);
        std::array<u8, 24> bindings{};
        bindings[0] = 9;
        bindings[5] = 1;
        bindings[8] = 10;
        bindings[16] = 7;
        bindings[22] = 1;
        RuntimeModelNode node{};
        node.bytes = bytes;
        std::array<RuntimeModelNode*, 1> roots{&node};
        RuntimeModelTree tree{0, roots, nullptr};
        std::array<RuntimeModelResourceBlock, 1> blocks{{{16, bytes.data()}}};
        RuntimeModelAsset asset{blocks, &tree, bytes};
        RuntimeModelInstance instance{};
        instance.source = &asset;
        ActorModel model;
        model.cpu = bytes;
        model.runtime = &instance;
        Actor actor;
        actor.model = &model;
        std::array<RuntimeModelNodeType, 1> types{{{"ndMtxArray", {}}}};
        ActorMatrixBindings views{{bytes.data(), bindings}};
        ActorMatrixSetup state;
        state.progress = 99.f;
        state.flags = 77;
        if (!initialize_actor_model_matrices(actor, state, types, views) ||
            state.matrices.count != 2 || state.matrices.node.data() != bytes.data() ||
            state.matrices.records[0].binding != 0xFFFFFFFFu ||
            state.matrices.records[1].binding != 7 ||
            state.matrices.records[0].scale.x != 1.f ||
            state.matrices.records[1].active != 0 ||
            state.progress != 0.f || state.flags != 0)
        {
            throw Error("actor matrix setup lost first-match binding rejection or matrix defaults");
        }
        engine_heap_free(state.allocation);
        word(0x30, 0);
        state.progress = 99.f;
        state.flags = 77;
        if (!initialize_actor_model_matrices(actor, state, types, views) ||
            state.matrices.count != 0 || state.progress != 99.f || state.flags != 77)
        {
            throw Error("empty matrix setup incorrectly performed per-record state resets");
        }
        engine_heap_free(state.allocation);
    }
    {
        std::vector<u8> cpu(0x80);
        auto word = [&](size_t offset, u32 value)
        {
            std::memcpy(cpu.data() + offset, &value, sizeof(value));
        };
        word(0, 8);
        word(4, 2);
        word(8, 5);
        word(12, 0x20);
        word(16, 5);
        word(20, 0x40);
        word(0x48, std::bit_cast<u32>(1e30f));
        word(0x4C, std::bit_cast<u32>(-2.f));
        word(0x50, std::bit_cast<u32>(3.f));
        word(0x54, std::bit_cast<u32>(4.f));
        word(0x58, std::bit_cast<u32>(5.f));
        word(0x5C, std::bit_cast<u32>(6.f));
        ActorModel model;
        model.cpu = cpu;
        Actor actor;
        actor.model = &model;
        actor.scale_x = 1e30f;
        actor.radius_y = -1e30f;
        actor.radius_xz = 1e30f;
        actor.scale_200 = 1e-30f;
        Vec3 low;
        Vec3 high;
        actor_model_bounds_low(actor, low);
        actor_model_bounds_high(actor, high);
        if (low.x != 1e30f || low.y != 2.f || low.z != 3.f ||
            high.x != 4.f || high.y != -5.f || high.z != 6.f)
        {
            throw Error("background bounds changed SSE scaling order or reflected endpoints");
        }
        word(12, 0);
        actor_model_bounds_low(actor, low);
        if (low.x != 0.f || low.y != 0.f || low.z != 0.f)
        {
            throw Error("background bounds incorrectly searched past a null first matching block");
        }
    }
    RequestProbe probe;
    std::vector<u8> parameters(0x448);
    auto parameter_word = [&](size_t offset, u32 value)
    {
        std::memcpy(parameters.data() + offset, &value, sizeof(value));
    };
    constexpr char initial_box[] = "aid_ghoulybox_initial";
    constexpr char initial_attributes[] = "aid_actorattribs_initial";
    std::memcpy(parameters.data() + 0x338, initial_box, sizeof(initial_box));
    std::memcpy(parameters.data() + 0x3B8, initial_attributes, sizeof(initial_attributes));
    parameter_word(0x334, 7);
    parameter_word(0x438, std::bit_cast<u32>(1.f));
    parameter_word(0x43C, 12);
    parameter_word(0x440, 34);
    parameter_word(0x444, 56);
    u32 random_state = 0;
    std::vector<u8> probe_bytes(sizeof(GhoulySwapState));
    GhoulySwapState& probe_state = actor_record_view<GhoulySwapState>(std::span<u8>(probe_bytes));
    Actor probe_actor;
    GhoulySpawnRecord* probe_load = nullptr;
    RuntimeGhoulySwapSetup probe_setup{probe_bytes, probe_actor, probe, random_state, probe_load, {}, {}, {}, {}};
    auto setup_word = [&](auto member)
    {
        return std::bit_cast<u32>(probe_state.*member);
    };
    probe_state.load = 0x1234u;
    probe_state.anchor = 0x1234u;
    probe_state.source = 0x1234u;
    g_ghouly_controllers.clear();
    if (!initialize_runtime_ghouly_swap_setup(probe_setup, parameters) ||
        g_ghouly_controllers.size() != 1 || g_ghouly_controllers[0].key != 7 ||
        g_ghouly_controllers[0].controller != &probe || random_state != 0x3C6EF35Fu ||
        probe_state.anchor != 0 || probe_state.source != 0 ||
        probe_state.load != 0x1234u ||
        std::strcmp(probe_state.box.data(), initial_box) != 0 ||
        std::strcmp(probe_state.attributes.data(), initial_attributes) != 0 ||
        setup_word(&GhoulySwapState::radius) != 12 || setup_word(&GhoulySwapState::chance) != 34 ||
        setup_word(&GhoulySwapState::unk_A50) != 56)
    {
        throw Error("swap setup initialization lost registry, random, or parameter fields");
    }
    // An unkeyed initialization still draws random and updates +0xA50.
    // A rejected chance must preserve the earlier names and selection fields.
    parameter_word(0x334, 0);
    parameter_word(0x438, std::bit_cast<u32>(-1.f));
    parameter_word(0x43C, 99);
    parameter_word(0x444, 78);
    random_state = 0;
    initialize_runtime_ghouly_swap_setup(probe_setup, parameters);
    if (g_ghouly_controllers.size() != 1 || random_state != 0x3C6EF35Fu ||
        setup_word(&GhoulySwapState::radius) != 12 || setup_word(&GhoulySwapState::unk_A50) != 78 ||
        std::strcmp(probe_state.box.data(), initial_box) != 0)
    {
        throw Error("unkeyed or rejected-chance initialization changed the wrong fields");
    }
    const float boundary = std::bit_cast<float>((0x3C6EF35Fu & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    parameter_word(0x438, std::bit_cast<u32>(boundary));
    random_state = 0;
    initialize_runtime_ghouly_swap_setup(probe_setup, parameters);
    if (setup_word(&GhoulySwapState::radius) != 99)
    {
        throw Error("swap setup chance equality was rejected");
    }
    parameter_word(0x438, std::bit_cast<u32>(std::numeric_limits<float>::quiet_NaN()));
    parameter_word(0x43C, 100);
    random_state = 0;
    initialize_runtime_ghouly_swap_setup(probe_setup, parameters);
    if (setup_word(&GhoulySwapState::radius) != 99)
    {
        throw Error("NaN swap setup chance selected a ghoulybox");
    }
    parameter_word(0x438, std::bit_cast<u32>(1.f));
    parameter_word(0x338, 0);
    random_state = 0;
    initialize_runtime_ghouly_swap_setup(probe_setup, parameters);
    if (setup_word(&GhoulySwapState::radius) != 99 || random_state != 0x3C6EF35Fu)
    {
        throw Error("non-AID box selected parameters or skipped its random draw");
    }
    g_ghouly_controllers.push_back({7, nullptr});
    if (request_ghoulybox("aid_ghoulybox_test", 7, nullptr) != 0xCAFE ||
        probe.received.id != 0xA8 || probe.received.result != 0 ||
        std::strcmp(probe.received.box, "aid_ghoulybox_test") != 0 || probe.received.attributes[0] != '\0')
    {
        throw Error("ghoulybox message layout or controller return was lost");
    }
    remove_ghouly_controller(&probe);
    if (request_ghoulybox("aid_ghoulybox_test", 7, nullptr) != 0 || probe.calls != 1)
    {
        throw Error("removed controller still received a request");
    }
    g_ghouly_controllers = {{7, nullptr}, {7, &probe}};
    if (request_ghoulybox("aid_ghoulybox_test", 7, nullptr) != 0 || probe.calls != 1)
    {
        throw Error("null first match incorrectly fell through to a duplicate key");
    }
    g_ghouly_controllers = {{7, &probe}};
    Game game;
    std::vector<u8> bytes(0x110);
    auto word = [&](size_t offset, u32 value)
    {
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    word(0, 0x110);
    word(4, 0x2A);
    word(0x88, 7);
    word(0x10C, 1);
    constexpr char box_name[] = "aid_ghoulybox_test";
    constexpr char attributes_name[] = "aid_actorattribs_test";
    std::memcpy(bytes.data() + 8, box_name, sizeof(box_name));
    std::memcpy(bytes.data() + 0x8C, attributes_name, sizeof(attributes_name));
    game.scene_script = SceneScript{"loading_test", bytes};
    const ScriptNode node = *script_node_at(game.scene_script, 0);
    GhoulySpawnRecord* pending = allocate_ghouly_spawn_record();
    if (pending->prev != nullptr || pending->next != nullptr || pending->remaining_c != 0 ||
        pending->value_10 != 0 || pending->value_14 != 0 || pending->value_18 != 0 ||
        pending->value_1c != 0 || pending->value_b0 != 0 || pending->attributes_30[0] != '\0' ||
        pending->index_28 != -1 || std::bit_cast<u32>(pending->timer_20) != 0x3A83126Fu ||
        pending->pool_8->stride != 0x288 || pending->pool_8->grow != 2 ||
        pending->pool_8->begin != pending->pool_8->cursor ||
        pending->pool_8->limit - pending->pool_8->begin != 2 * 0x288)
    {
        throw Error("spawn record or its initial definition pool was initialized incorrectly");
    }
    // 0x0006BDF0 writes this completion field before calling 0x000F3C30.
    pending->flags_2c = 0;
    link_actor_load_record(&g_side_list, *pending);
    if (script_handle_actor_actions(game, node) != 0 || probe.calls != 2 ||
        std::strcmp(probe.received.attributes, "aid_actorattribs_test") != 0)
    {
        throw Error("script did not dispatch and hold its actor request");
    }
    if (script_handle_actor_actions(game, node) != 0 || probe.calls != 2)
    {
        throw Error("unfinished load was released or requested twice");
    }
    pending->flags_2c = 1;
    // DAT_00511B0C: actors waiting on the scene's live list.
    bool live_pending = true;
    const auto saved_live_pending = g_live_actors_pending;
    g_live_actors_pending = [&live_pending]()
    {
        return live_pending;
    };
    if (script_handle_actor_actions(game, node) != 0)
    {
        throw Error("pending world-view registration did not hold loading");
    }
    live_pending = false;
    // One DAT_00511B38 actor: its goal-ring front kind and its +0x984 / +0x98C words.
    u16 actor_state = 2;
    u32 word_984 = 0;
    u32 word_98c = 0;
    bool actor_listed = true;
    const auto saved_loading = g_loading_actors;
    g_loading_actors = [&]()
    {
        std::vector<RuntimeLoadingActor> actors;
        if (actor_listed)
        {
            actors.push_back({0, actor_state, word_984, word_98c});
        }
        return actors;
    };
    if (script_handle_actor_actions(game, node) != 0)
    {
        throw Error("unready live actor did not hold loading");
    }
    actor_state = 7;
    if (script_handle_actor_actions(game, node) != 2 || probe.calls != 2)
    {
        throw Error("ready actors did not release the script without another request");
    }
    // After release, this same action must be able to issue a new request.
    if (script_handle_actor_actions(game, node) != 0 || probe.calls != 3)
    {
        throw Error("actor-loading gate did not reset after completion");
    }
    script_handle_actor_actions(game, node);
    actor_state = 2;
    word_984 = 1;
    if (!actor_loading_complete())
    {
        throw Error("retail actor state override was ignored");
    }
    word_984 = 0;
    word_98c = 1;
    if (!actor_loading_complete())
    {
        throw Error("retail secondary actor state override was ignored");
    }
    actor_listed = false;
    GhoulySpawnRecord* second = allocate_ghouly_spawn_record();
    second->flags_2c = 0;
    link_actor_load_record(&g_side_list, *second);
    if (g_side_list.next != pending || pending->next != second || second->prev != pending ||
        actor_loading_complete())
    {
        throw Error("spawn records were not appended in order or a later unfinished load was ignored");
    }
    ActorLoadList other;
    link_actor_load_record(&other, *pending);
    if (g_side_list.next != second || second->prev != &g_side_list ||
        other.next != pending || pending->prev != &other || pending->next != nullptr)
    {
        throw Error("moving a spawn record corrupted the load lists");
    }
    link_actor_load_record(nullptr, *pending);
    if (other.next != nullptr || pending->prev != nullptr || pending->next != nullptr)
    {
        throw Error("null destination did not detach the spawn record");
    }
    release_ghouly_spawn_record(*pending);
    release_ghouly_spawn_record(*second);
    if (g_side_list.next != nullptr || !actor_loading_complete())
    {
        throw Error("releasing the final spawn record left a stale load-list entry");
    }
    g_loading_actors = saved_loading;
    g_live_actors_pending = saved_live_pending;
    g_side_list = {};
    g_ghouly_controllers.clear();
}
}

void check_scene_model_resources(gbtg::AidStore& aids)
{
    using namespace gbtg;
    const auto fail = [](const char* message)
    {
        throw Error(std::string("scene model resources: ") + message);
    };
    const Bundle& bundle = aids.load_bundle_rel(*aid_to_bundle_rel("aid_script_ghoulies_chapter1_scene1_2playcam"));
    SceneExecutableResources executable(aids.game_dir);
    SceneDialogueResourceOwner owner(aids, executable.pools);
    // Every executable animtable resolves through the registry to its decoded, name-sorted view.
    size_t tables = 0;
    size_t animations = 0;
    for (const auto& pool : executable.pools)
    {
        if (pool.type != "animtable")
        {
            continue;
        }
        for (const auto& record : pool.records)
        {
            const auto cpu = acquire_animation_resource(owner.registry, record.name.c_str());
            if (cpu.data() != record.cpu.data())
            {
                fail("an animtable did not resolve through the executable provider");
            }
            const auto found = executable.animation_tables.tables.find(cpu.data());
            if (found == executable.animation_tables.tables.end() || found->second == nullptr)
            {
                fail("an animtable has no decoded view");
            }
            for (const auto& animation : found->second->records)
            {
                if (find_animation_record(*found->second, animation.name) != &animation
                    || !animation.animation.starts_with("aid_anim_"))
                {
                    fail("a decoded animation record is out of order or names no animation");
                }
                ++animations;
            }
            for (const auto& alias : found->second->aliases)
            {
                if (find_animation_alias(*found->second, alias.name) != &alias)
                {
                    fail("a decoded animation alias is out of order");
                }
            }
            release_animation_resource(owner.registry, cpu);
            ++tables;
        }
    }
    if (tables != 0x56)
    {
        fail("the animtable pool does not hold its 0x56 retail records");
    }
    std::cout << "executable animtables: " << tables << " animations " << animations << "\n";
    size_t loaded = 0;
    for (const BundleEntry* entry : entries_of_type(bundle, ResourceType::Model))
    {
        const char* name = entry->name.c_str();
        const auto cpu = acquire_animation_resource(owner.registry, name);
        if (cpu.data() == nullptr)
        {
            fail("an opening scene model did not load");
        }
        ++loaded;
        const auto found = owner.model_views.assets.find(cpu.data());
        if (found == owner.model_views.assets.end())
        {
            fail("the model post-load hook published no asset view");
        }
        const RuntimeModelAsset& asset = *found->second;
        if (asset.tree == nullptr || asset.tree->roots.empty() || asset.blocks.empty() || asset.cpu.data() != cpu.data())
        {
            fail("the relocated model has no node tree or directory blocks");
        }
        RuntimeShaderRegistrationState registration{};
        RuntimeShaderAnimationScratch scratch{};
        auto instance = make_runtime_model_instance(asset, 1, 1, owner.copy_types, registration, scratch);
        if (instance == nullptr || instance->source != &asset || instance->tree_storage.bytes.size() != asset.tree->instance_size)
        {
            fail("a model instance could not be built from the relocated asset");
        }
        std::cout << name << ": blocks " << asset.blocks.size() << " roots " << asset.tree->roots.size()
                  << " instance " << asset.tree->instance_size << " d3d " << owner.d3d.resources.size()
                  << " memory blocks " << owner.memory.block_count() << "\n";
        release_runtime_model_instance(*instance, owner.registry, owner.blendshapes, owner.model_heap,
            owner.d3d_boundaries);
        // The registry defers the unload of a released resource, so its view stays published.
        release_animation_resource(owner.registry, cpu);
    }
    if (loaded == 0 || owner.model_views.assets.size() != loaded)
    {
        fail("the opening scene bundle's models were not all viewed");
    }
    std::cout << "opening scene models: " << loaded << "\n";
}

// Scene startup's marker pass (0x00140020) over the opening scene, through the live service graph.
void check_aligned_engine_heap()
{
    using namespace gbtg;
    std::array<void*, 3> blocks;
    const std::array<u32, 3> alignment{16, 32, 64};
    for (size_t index = 0; index < blocks.size(); ++index)
    {
        blocks[index] = engine_heap_allocate_aligned(0x1F400, alignment[index]);
        if (blocks[index] == nullptr || (reinterpret_cast<uintptr_t>(blocks[index]) & (alignment[index] - 1)) != 0)
        {
            throw Error("native aligned heap did not honor the retail alignment request");
        }
        std::memset(blocks[index], int(index + 1), 0x1F400);
    }
    void* ordinary = engine_heap_allocate(0x80);
    std::memset(ordinary, 0xA5, 0x80);
    engine_heap_free(blocks[1]);
    engine_heap_free(ordinary);
    for (size_t index : {size_t(2), size_t(0)})
    {
        if (static_cast<const u8*>(blocks[index])[0x1F3FF] != u8(index + 1))
        {
            throw Error("native aligned heap allocations overlap or lost their payload");
        }
        engine_heap_free(blocks[index]);
    }
}

void check_scene_vertex_shader_ownership()
{
    using namespace gbtg;
    SceneD3DDevice device;
    u32 first = 0, second = 0;
    const std::array<u32, 9> expected{0x00022078, 0, 0x12345678, 0xABCDEF00, 0,
        0, 0x33333333, 0x44444444, 1};
    {
        std::array<i32, 32> declaration{};
        declaration[0] = 0x123;
        declaration[31] = -1;
        auto program = expected;
        device.create_vertex_shader(declaration.data(), program.data(), first, 0);
        declaration.fill(0);
        program.fill(0);
        auto other = expected;
        other[2] = 0xDEADBEEF;
        device.create_vertex_shader(declaration.data(), other.data(), second, 0);
    }
    const auto& retained = device.vertex_shaders.at(first);
    if (first == second || retained.declaration[0] != 0x123 || retained.declaration[31] != -1 ||
        retained.program != std::vector<u32>(expected.begin(), expected.end()) ||
        device.vertex_shaders.at(second).program[2] != 0xDEADBEEF)
    {
        throw Error("native D3D vertex shader lost its temporary creation data or aliased another shader");
    }
    device.set_vertex_stream(0, 0x10000404, 0x20);
    device.set_vertex_stream(1, 0x10000804, 0x10);
    device.set_vertex_stream(0, 0x10001004, 0x30);
    const auto& draw = device.draw_indexed(4, 3, 0x10002000);
    device.set_vertex_stream(0, 0x10003004, 0x40);
    if (draw.streams.at(0).resource != 0x10001004 || draw.streams.at(0).stride != 0x30 ||
        draw.streams.at(1).resource != 0x10000804 || draw.streams.at(1).stride != 0x10)
    {
        throw Error("native D3D vertex stream snapshot lost its binding or changed a different stream");
    }
}

void check_scene_object_disposal()
{
    using namespace gbtg;
    static std::vector<u32>* order;
    static RuntimeSceneObjectDisposalLink* mutated;
    std::vector<u32> destroyed;
    order = &destroyed;
    const ObjectFactoryVtable vtable{0, nullptr, nullptr, [](FactoryObject& object)
        {
            order->push_back(read_u32({object.bytes.get(), object.size}, 8));
            mutated->next = nullptr;
        }};
    static const ObjectFactoryVtable* active_vtable;
    active_vtable = &vtable;
    const std::array<ObjectFactoryType, 2> types{{{}, {0, []()
        {
            return active_vtable;
        }}}};
    ObjectFactoryRegistry registry{types, 3};
    std::array<FactoryObject, 3> objects;
    for (u32 index = 0; index < 3; ++index)
    {
        auto& object = objects[index];
        object.bytes = std::make_unique<u8[]>(0x20);
        object.size = 0x20;
        const u16 type = 1;
        std::memcpy(object.bytes.get() + 4, &type, 2);
        std::memcpy(object.bytes.get() + 8, &index, 4);
    }
    RuntimeSceneObjectDisposalLink third{nullptr, objects[2]};
    RuntimeSceneObjectDisposalLink second{&third, objects[1]};
    RuntimeSceneObjectDisposalLink first{&second, objects[0]};
    mutated = &first;
    dispose_runtime_scene_objects(&first, registry);
    if (destroyed != std::vector<u32>{0, 1, 2} || registry.count != 0 ||
        objects[0].bytes != nullptr || objects[1].bytes != nullptr || objects[2].bytes != nullptr)
    {
        throw Error("queued disposal lost its saved next link or factory ownership");
    }
    dispose_runtime_scene_objects(nullptr, registry);
}

void check_actor_sound_frame_pass()
{
    using namespace gbtg;
    AudioCoreSources sources{};
    sources.unit_scale = 2.f;
    sources.boundaries.bad_pointer = [](const void*)
    {
        return false;
    };
    Vec3 position{}, cone{}, origin{};
    sources.boundaries.position = [&](void*, const Vec3& value, u32)
    {
        position = value;
    };
    sources.boundaries.cone = [&](void*, const Vec3& value, u32)
    {
        cone = value;
    };
    sources.boundaries.velocity = [](void*, const Vec3&, u32)
    {
        throw Error("actor sound direction used velocity instead of cone");
    };
    std::array<u8, 0x60> first_attributes{}, second_attributes{};
    AudioCoreSource first_source{}, second_source{};
    first_source.attributes = first_attributes.data();
    second_source.attributes = second_attributes.data();
    sources.sources.next = &first_source;
    first_source.next = &second_source;
    AudioCoreCues core{sources};
    AudioEventVoices voices{core, {}, {}, {}, 0, origin, origin};
    for (auto& voice : voices.voices)
    {
        voice.state = 1; // Exhausted allocation boundary through the real allocator.
    }
    std::array<AudioCoreCue, 4> cues{};
    for (size_t index = 0; index < cues.size(); ++index)
    {
        voices.voices[index].cue = &cues[index];
    }
    voices.voices[0].state = 8;
    voices.voices[1].state = 5;
    voices.voices[2].state = voices.voices[3].state = 6;
    voices.voices[2].source = &first_source;
    voices.voices[3].source = &second_source;
    RuntimeActorSoundQueues queues{};
    auto& group = queues.groups[0];
    group.capacity = 4;
    Actor owner;
    RuntimeActorSound ended{}, live{};
    ended.owner = live.owner = &owner;
    ended.previous_bone = 0;
    live.previous_bone = 2;
    group.active.next = &ended;
    ended.previous = &group.active;
    ended.next = &live;
    live.previous = &ended;
    std::vector<u32> messages;
    const std::function<void(Actor&, u32, RuntimeActorSound&)> message = [&](Actor&, u32 id, RuntimeActorSound& sound)
    {
        messages.push_back(id);
        if (id == 0x8A)
        {
            sound.previous_bone = 1;
        }
        else if (id == 0x8B)
        {
            sound.position = {4.f, 5.f, 6.f};
        }
        else if (id == 0x8C)
        {
            sound.previous_bone = 3;
            sound.direction = {0.f, 1.f, 0.f};
        }
    };
    RuntimeActorSoundTick context{voices, origin, message};
    update_active_actor_sound_voices(group, context);
    if (messages != std::vector<u32>{0x8A, 0x8B, 0x8C} || ended.previous_bone != 0xFFFF
        || ended.enabled != 1 || group.active.next != &live || voices.voices[1].state != 9
        || core.stopped_count != 1 || position.x != 8.f || position.y != 10.f || position.z != 12.f
        || cone.x != 0.f || cone.y != 1.f || cone.z != 0.f || live.enabled != 0)
    {
        throw Error("actor sound frame lost callback index changes, unlinking or source updates");
    }
    std::array<RuntimeActorSound, 3> pending{};
    group.pending.next = &pending[0];
    for (size_t index = 0; index < pending.size(); ++index)
    {
        pending[index].previous = index == 0 ? &group.pending : &pending[index - 1];
        pending[index].next = index == 2 ? nullptr : &pending[index + 1];
        pending[index].position = {float(3 - index), 0.f, 0.f};
    }
    sort_pending_actor_sounds(group, origin);
    if (group.pending.next != &pending[2] || pending[2].next != &pending[1] || pending[1].next != &pending[0])
    {
        throw Error("pending sounds were not sorted nearest first");
    }
    tick_runtime_actor_sounds(queues, context);
    if (group.pending.next != nullptr)
    {
        throw Error("exhausted voice allocation left pending sound nodes linked");
    }
}

void check_scene_actor_update_queue()
{
    using namespace gbtg;
    RuntimeActorLink head, first, second, added;
    head.next = &first;
    first.previous = &head;
    first.next = &second;
    second.previous = &first;
    std::array<Actor, 3> actors;
    for (auto& actor : actors)
    {
        actor.xform_valid = 1;
    }
    actors[1].xform_valid = 0;
    actors[1].position = {3.f, 4.f, 5.f};
    actors[1].scale_200 = actors[1].scale_x = actors[1].radius_y = actors[1].radius_xz = 1.f;
    const std::function<Actor&(RuntimeActorLink&)> actor_at = [&](RuntimeActorLink& node) -> Actor&
    {
        if (&node == &first)
        {
            return actors[0];
        }
        return &node == &second ? actors[1] : actors[2];
    };
    std::vector<size_t> order;
    const std::function<void(Actor&)> update = [&](Actor& actor)
    {
        const size_t index = size_t(&actor - actors.data());
        RuntimeActorLink& node = index == 0 ? first : index == 1 ? second : added;
        if (node.previous != nullptr || node.next != nullptr)
        {
            throw Error("actor update called before unlinking its node");
        }
        order.push_back(index);
        if (index == 0)
        {
            second.next = &added;
            added.previous = &second;
        }
    };
    RuntimeSceneActorUpdates context{head.next, actor_at, update};
    update_runtime_scene_actors(context);
    if (order != std::vector<size_t>{0, 1, 2} || head.next != nullptr || actors[1].xform_valid != 1)
    {
        throw Error("actor update drain lost callback additions or transform refresh");
    }
    RuntimeActorLink pending_head, active_head, update_head, pending_first, pending_second;
    std::array<RuntimeActorLink, 3> control_updates;
    pending_head.next = &pending_first;
    pending_first.previous = &pending_head;
    pending_first.next = &pending_second;
    pending_second.previous = &pending_first;
    const std::function<RuntimeActorLink&(RuntimeActorLink&)> update_link = [&](RuntimeActorLink& node) -> RuntimeActorLink&
    {
        return control_updates[&node == &pending_first ? 0 : 1];
    };
    std::vector<size_t> notices, updates;
    const std::function<void(RuntimeActorLink&, std::span<const u32>)> notice = [&](RuntimeActorLink& node, std::span<const u32> message)
    {
        if (message.size() != 1 || message[0] != 0x7D)
        {
            throw Error("control notification did not retain its one-word message");
        }
        const size_t index = size_t(&node - control_updates.data());
        notices.push_back(index);
        if (index == 0)
        {
            control_updates[1].next = &control_updates[2];
            control_updates[2].previous = &control_updates[1];
        }
    };
    const std::function<void(RuntimeActorLink&)> control_update = [&](RuntimeActorLink& node)
    {
        if (node.previous != nullptr || node.next != nullptr)
        {
            throw Error("control update called before unlinking its node");
        }
        updates.push_back(size_t(&node - control_updates.data()));
    };
    RuntimeSceneControlUpdates controls{pending_head.next, active_head, update_head, update_link, notice, control_update};
    activate_runtime_scene_controls(controls);
    if (pending_head.next != nullptr || active_head.next != &pending_second
        || pending_second.next != &pending_first || update_head.next != &control_updates[0])
    {
        throw Error("control activation lost active insertion or update order");
    }
    notify_runtime_scene_controls(controls);
    update_runtime_scene_controls(controls);
    if (notices != std::vector<size_t>{0, 1, 2} || updates != notices || update_head.next != nullptr)
    {
        throw Error("control passes lost callback additions or update order");
    }
}

void check_scene_spawn(gbtg::Game& game, bool scene_start)
{
    using namespace gbtg;
    for (int frame = 0; frame < 600 && game.scene_script.name != "aid_script_ghoulies_chapter1_scene1_2playcam"; ++frame)
    {
        gfx_begin_frame();
        game.tick(1.f / 60.f, {});
        gfx_end_frame();
    }
    if (game.factory_scene == nullptr || game.scene_script.name != "aid_script_ghoulies_chapter1_scene1_2playcam")
    {
        throw Error("scene spawn check did not load the opening scene");
    }
    auto& services = game.factory_scene->start_actors(game);
    const auto& resource_owner = game.factory_scene->resources;
    for (size_t index = 0; index < resource_owner.relocation_types.size(); ++index)
    {
        const auto descriptor = resource_owner.node_types.bytes.subspan(index * 0x54, 0x54);
        if (std::strcmp(reinterpret_cast<const char*>(descriptor.data()),
                resource_owner.relocation_types[index].name.c_str()) != 0)
        {
            throw Error("model renderer registry is not the relocation registry");
        }
        const u32 vtable = read_u32(descriptor, 0x50);
        const u32 draw = read_u32(game.factory_scene->executable->memory(vtable, 4), 0);
        if (draw == 0)
        {
            throw Error("model renderer node has no actual executable draw slot");
        }
    }
    try
    {
        if (scene_start)
        {
            // 0x0011CBD0 creates and activates the requested background and builds navigation first.
            services.start_background();
            std::cout << "scene start: background and navigation built\n";
        }
        std::cout << "scene spawn: markers\n";
        services.start_markers();
        const auto spawned = services.actor_addresses();
        if (spawned.empty() || !services.activation_pending())
        {
            throw Error("opening markers created no pending actors");
        }
        std::cout << "scene spawn: activation\n";
        services.activate_actors();
        const auto active = services.active_actor_addresses();
        if (services.activation_pending() || active.size() != spawned.size())
        {
            throw Error("opening activation did not drain every spawned actor");
        }
        std::unordered_set<u32> seen;
        for (u32 address : active)
        {
            if (!spawned.contains(address) || !seen.insert(address).second)
            {
                throw Error("opening activation registered a duplicate or foreign actor");
            }
        }
        size_t listed = 0;
        for (auto* node = entity_detail::g_world.latch; node != nullptr; node = node->next)
        {
            if (++listed > active.size())
            {
                throw Error("opening active actor registry contains a cycle or extra actor");
            }
        }
        if (listed != active.size())
        {
            throw Error("opening activation did not publish the world active actor registry");
        }
        const size_t count = active.size();
        services.activate_actors();
        if (services.activation_pending() || services.active_actor_addresses().size() != count)
        {
            throw Error("empty activation pass changed the active actor list");
        }
        std::cout << "scene spawn: activated=" << count << " pending=0\n";
        if (scene_start)
        {
            // 0x0011CBD0 then sends 0x88 (0x001231F0) and 0x7D (0x001231A0) over the activated list.
            services.send_scene_start_notice();
            std::cout << "scene start: notice 0x88 delivered\n";
            services.send_scene_start();
            std::cout << "scene start: 0x7D delivered\n";
            std::cout << "scene start: script 0x6F player wait returns " << services.wait_script_player() << "\n";
            check_scene_actor_update_queue();
            check_actor_sound_frame_pass();
            check_scene_object_disposal();
            check_aligned_engine_heap();
            check_scene_vertex_shader_ownership();
            services.update_sounds();
            // Exercise the actual background +0x10 binding directly; the full
            // gameplay frame dispatcher is a separate reconstruction frontier.
            RuntimeEffectPassScratch effect_scratch{{17.f, -23.f, 31.f}, 0xA5A5A5A5u,
                std::bit_cast<float>(0x3F000001u)};
            services.update_effects(effect_scratch);
            u32 backgrounds_updated = 0;
            for (auto& object : game.factory_scene->objects)
            {
                if (object.type == 4)
                {
                    services.update_actor(game.factory_scene->address_of(object.bytes.get()), effect_scratch);
                    ++backgrounds_updated;
                }
            }
            if (backgrounds_updated == 0)
            {
                throw Error("scene-start fixture has no background update slot");
            }
            std::cout << "scene start: background update slot passed\n";
        }
    }
    catch (const std::exception& error)
    {
        // Reported before the scene's teardown runs during unwinding.
        std::cout << "scene spawn error: " << error.what() << "\n";
        throw;
    }
    std::cout << "scene spawn: objects=" << game.factory_scene->objects.size()
              << " players=" << services.player_total() << " factory=" << game.factory_scene->factory.count
              << " player link=0x" << std::hex << services.player_link() << std::dec << "\n";
    if (services.player_total() == 0)
    {
        throw Error("opening markers created no player actors");
    }
    const auto& actor_addresses = services.actor_addresses();
    const u32 player_link = services.player_link();
    if (player_link < 0x93C)
    {
        throw Error("opening push-message check has no Cooper player link");
    }
    const u32 cooper = player_link - 0x93C;
    u32 imp = 0;
    for (const auto& [address, actor] : actor_addresses)
    {
        if (actor->class_key == 23)
        {
            imp = address;
        }
    }
    if (!actor_addresses.contains(cooper) || read_u16(game.factory_scene->resolve_address(cooper, 6), 4) != 5
        || imp == 0 || find_runtime_script_event_type(0xB6) != nullptr)
    {
        throw Error("opening push-message check has no Cooper/imp pair or a preexisting contact event");
    }
    const std::array<u32, 9> contact_message{0xE, cooper, std::bit_cast<u32>(1.f), 0, 0,
        std::bit_cast<u32>(2.f), std::bit_cast<u32>(3.f), std::bit_cast<u32>(4.f), 0};
    services.send_actor_message(imp, contact_message);
    const u32* event = find_runtime_script_event_type(0xB6);
    if (event == nullptr)
    {
        throw Error("actor push contact round trip did not enqueue its real player-mind event");
    }
    services.send_actor_message(imp, contact_message);
    size_t contacts = 0;
    for (auto* node = g_live_scripts; node != nullptr; node = node->next)
    {
        if (node->type == 0xB6)
        {
            ++contacts;
            if (read_u32(node->message, 4) != cooper || read_u32(node->message, 8) != imp
                || read_u32(node->message, 12) != 23)
            {
                throw Error("actor push contact reply changed sender, receiver or class");
            }
        }
    }
    if (contacts != 1)
    {
        throw Error("repeated push contact was not deduplicated by the retail script filter");
    }
    std::cout << "scene spawn: push contact round trip and event deduplication passed\n";
    game.factory_scene->release_objects();
    game.factory_scene->actor_services.reset();
    if (game.factory_scene->factory.count != 0 || game.factory_scene->address_count() != 0
        || entity_detail::g_world.latch != nullptr)
    {
        throw Error("activated scene teardown retained factory=" + std::to_string(game.factory_scene->factory.count)
            + " addresses=" + std::to_string(game.factory_scene->address_count())
            + " latch=" + std::to_string(entity_detail::g_world.latch != nullptr));
    }
    std::cout << "scene spawn: teardown factory=0 addresses=0\n";
}

void check_player_dialogue_resources(gbtg::AidStore& aids)
{
    // The unit checks run with no scene: DAT_00511AA4 is empty, so 0x00122C70's broadcast reaches no actor. The
    // scene binds the real walk when it starts.
    gbtg::g_actor_broadcast = [](const gbtg::i32*)
    {
    };
    {
        using namespace gbtg;
        u32 output = 0xABCDEF01;
        if (find_runtime_script_table_row(nullptr, 0x2C, output) != nullptr || output != 0xABCDEF01)
        {
            throw Error("null script table changed the caller output");
        }
        std::array<u8, 0x108 * 5> rows{};
        const u16 keys[]{0, 0x2C, 0x7FFF, 0x8000, 0xFFFF};
        for (size_t index = 0; index < 5; ++index)
        {
            std::memcpy(rows.data() + index * 0x108, &keys[index], 2);
        }
        RuntimeScriptLookupTable table{5, rows.data()};
        for (size_t index = 0; index < 5; ++index)
        {
            output = 0xABCDEF01;
            if (find_runtime_script_table_row(&table, keys[index], output) != rows.data() + index * 0x108
                || output != 0)
            {
                throw Error("script table lookup changed its unsigned key or row stride");
            }
        }
        for (u16 missing : {u16{1}, u16{0x7FFE}, u16{0xFFFE}})
        {
            output = 0xABCDEF01;
            if (find_runtime_script_table_row(&table, missing, output) != nullptr || output != 0)
            {
                throw Error("script table miss did not clear caller output");
            }
        }
        for (i32 count : {0, -1})
        {
            table.count = count;
            table.rows = nullptr;
            output = 0xABCDEF01;
            if (find_runtime_script_table_row(&table, 0x2C, output) != nullptr || output != 0)
            {
                throw Error("empty script table changed signed count handling");
            }
        }
    }
    {
        using namespace gbtg;
        std::array<u8, 0x54> strategy{};
        const u32 active = 0x1234;
        std::memcpy(strategy.data() + 0x48, &active, 4);
        u32 active_count = 1;
        destroy_runtime_player_strategy(strategy, active_count);
        destroy_runtime_player_strategy(strategy, active_count);
        if (active_count != 0 || read_u32(strategy, 0x48) != 0)
        {
            throw Error("player strategy destructor must remove its active count once");
        }
        RuntimePlayerMindRequest anchor{};
        auto* first = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
        auto* second = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
        auto* third = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
        *first = {&anchor, second, 1, 1};
        *second = {first, third, 2, 0};
        *third = {second, nullptr, 3, 1};
        anchor.next = first;
        int releases = 0;
        RuntimePlayerMindRequests mind{anchor.next, [&](RuntimePlayerMindRequest& request)
        {
            ++releases;
            if (request.previous != nullptr || request.next != nullptr)
            {
                throw Error("bulk mind cleanup must detach before virtual release");
            }
        }};
        clear_runtime_player_mind_requests(mind);
        if (anchor.next != nullptr || releases != 2)
        {
            throw Error("mind cleanup must release active requests and unlink every node");
        }
        clear_runtime_player_mind_requests(mind);
        first = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
        *first = {&anchor, nullptr, 4, 1};
        anchor.next = first;
        mind.release_active = [&](RuntimePlayerMindRequest& request)
        {
            ++releases;
            if (request.previous != &anchor || anchor.next != &request)
            {
                throw Error("single mind cleanup must invoke virtual release before unlinking");
            }
        };
        release_runtime_player_mind_request(*first, mind);
        if (anchor.next != nullptr || releases != 3)
        {
            throw Error("single mind request release left a linked node");
        }
    }
    {
        gbtg::SceneExecutableResources executable(aids.game_dir);
        check_player_parent_lifecycle(executable);
        check_player_messages();
        check_player_update();
        check_player_mind_owner(executable);
        check_player_factory_owner();
        // The registrations of 0x00034C30 and 0x00034BC0, in order, with their active flags.
        const std::pair<const char*, bool> registered[]{{"animtable", true}, {"attackdata", true},
            {"hitreaction", true}, {"objparams", false}, {"scripttable", false}, {"objparams", false},
            {"statetable", true}};
        if (executable.pools.size() != std::size(registered))
        {
            throw gbtg::Error("executable resource pools differ from the retail registrations");
        }
        for (size_t index = 0; index < std::size(registered); ++index)
        {
            if (executable.pools[index].type != registered[index].first
                || executable.pools[index].active != registered[index].second)
            {
                throw gbtg::Error("executable resource pool order or active flag differs from retail");
            }
        }
        size_t checked = 0;
        for (const auto& pool : executable.pools)
        {
            if (pool.type != "objparams")
            {
                continue;
            }
            if (pool.active)
            {
                throw gbtg::Error("retail objparams pool became an active bundle provider");
            }
            for (const auto& record : pool.records)
            {
                // Lookup uses the actual pool priority; compare against the
                // selected record when a later pool repeats a template name.
                const auto* selected = gbtg::find_executable_resource(executable.pools,
                    record.name.c_str(), true);
                auto clone = gbtg::clone_executable_resource(executable.pools, record.name.c_str());
                if (selected == nullptr || clone.bytes == nullptr || clone.size != selected->cpu.size() ||
                    std::memcmp(clone.bytes.get(), selected->cpu.data(), clone.size) != 0)
                {
                    throw gbtg::Error("retail executable template clone disagrees: " + record.name);
                }
                ++checked;
            }
        }
        auto player = gbtg::clone_executable_resource(executable.pools, "aid_objparams_ghoulies_actor_player_boy");
        auto bat = gbtg::clone_executable_resource(executable.pools, "aid_objparams_ghoulies_misc_buttonbat");
        if (checked != 0x1F1 + 0x35C || player.size != 0x63C || bat.size != 0x340 ||
            gbtg::read_u32({player.bytes.get(), player.size}, 4) != 5 ||
            gbtg::read_u32({bat.bytes.get(), bat.size}, 4) != 0x15)
        {
            throw gbtg::Error("retail player or button-bat executable template has the wrong factory type");
        }
        std::cout << "player executable resources: " << checked << " template clones matched\n";
        const auto raw_overrides = executable.memory(0x00430DA0, 0x210);
        const std::span<const gbtg::u8, 0x210> overrides{raw_overrides.data(), 0x210};
        const auto selected = [&](const char* scene, gbtg::u8 flags, gbtg::u32 runtime)
        {
            return std::string_view(gbtg::select_player_parameter_name(scene, overrides, flags, runtime));
        };
        const char* opening = "aid_script_ghoulies_chapter1_scene1_2playcam";
        if (selected(opening, 0, 0) != "aid_objparams_ghoulies_actor_player_boy" ||
            selected(opening, 8, 0) != "aid_objparams_ghoulies_actor_player_girl" ||
            selected(opening, 0, 0x200000) != "aid_objparams_ghoulies_actor_player_girl" ||
            selected("aid_script_ghoulies_challenges_kickemquick", 0, 0) != "aid_objparams_ghoulies_actor_player_girl" ||
            selected("aid_script_ghoulies_chapter5_scene13_2playcam", 8, 0x200000) !=
                "aid_objparams_ghoulies_actor_cookplayable" ||
            gbtg::find_player_scene_override("aid_script_ghoulies_attractsequence_final1", overrides) != 0)
        {
            throw gbtg::Error("player selector lost retail scene overrides, fallback flags or the four-row bound");
        }
        // Parameter-capture factory fixture only: these callbacks validate the
        // translated wrappers; they are never installed in a live Game.
        struct CaptureFactory
        {
            static std::vector<gbtg::u8>& parameters()
            {
                static std::vector<gbtg::u8> bytes;
                return bytes;
            }
            static void sizes(gbtg::ObjectSizes& sizes)
            {
                sizes = {0x200, 0x820};
            }
            static bool construct(gbtg::FactoryObject& object, std::span<const gbtg::u8> parameters)
            {
                CaptureFactory::parameters().assign(parameters.begin(), parameters.end());
                if (object.type == 0x4F)
                {
                    const gbtg::u16 states[]{4, 7};
                    std::memcpy(object.bytes.get() + 0xC, states, 4);
                }
                return true;
            }
            static void destroy(gbtg::FactoryObject&)
            {
            }
            static const gbtg::ObjectFactoryVtable* table()
            {
                static const gbtg::ObjectFactoryVtable table{0xFFFF0005, sizes, construct, destroy};
                return &table;
            }
        };
        std::array<gbtg::ObjectFactoryType, 0x80> factory_types{};
        for (auto& type : factory_types)
        {
            type = {0, CaptureFactory::table};
        }
        gbtg::ObjectFactoryRegistry factory{factory_types};
        const gbtg::u8 scene_flags = 0;
        const gbtg::u32 runtime_flags = 0;
        gbtg::RuntimePlayerSpawnContext spawn{factory, executable.pools, opening, overrides,
            scene_flags, runtime_flags, ""};
        auto player_object = gbtg::create_runtime_player_actor("aid_actorattribs_test",
            {11.f, -22.f, 33.f}, {-1.f, 2.f, -3.f}, spawn);
        const auto captured = std::span<const gbtg::u8>(CaptureFactory::parameters());
        if (player_object.bytes == nullptr || factory.count != 1 || captured.size() != 0x63C ||
            gbtg::read_f32(captured, 8) != 11.f || gbtg::read_f32(captured, 0xC) != -22.f ||
            gbtg::read_f32(captured, 0x10) != 33.f || gbtg::read_f32(captured, 0x14) != -1.f ||
            gbtg::read_f32(captured, 0x18) != 2.f || gbtg::read_f32(captured, 0x1C) != -3.f ||
            gbtg::read_f32(captured, 0x20) != 1.f || gbtg::read_u16(captured, 0x2AA) != 0 ||
            gbtg::read_u16(captured, 0x2AC) != 0 ||
            std::string_view(reinterpret_cast<const char*>(captured.data() + 0x4BA)) != "aid_actorattribs_test")
        {
            throw gbtg::Error("player spawn wrapper lost native transform or override fields");
        }
        gbtg::release_factory_object(factory, player_object);
        gbtg::RuntimePlayerChildContext children{factory, executable.pools, 0x12345678};
        gbtg::FactoryObject body;
        gbtg::FactoryObject mind;
        gbtg::set_runtime_player_body(body, "aid_objparams_ghoulies_body_boy", children);
        if (body.bytes == nullptr || gbtg::read_u32(CaptureFactory::parameters(), 0x1B0) != children.actor_address)
        {
            throw gbtg::Error("player body wrapper lost its native parent token");
        }
        gbtg::set_runtime_player_mind(mind, "aid_objparams_ghoulies_mind_player", children);
        if (mind.bytes == nullptr || factory.count != 2 ||
            gbtg::read_u32(CaptureFactory::parameters(), 0x9C) != children.actor_address)
        {
            throw gbtg::Error("player mind wrapper lost its native parent token");
        }
        gbtg::set_runtime_player_body(body, nullptr, children);
        gbtg::set_runtime_player_mind(mind, "none", children);
        if (body.bytes != nullptr || mind.bytes != nullptr || factory.count != 0)
        {
            throw gbtg::Error("player child wrappers did not release their previous objects");
        }
        {
            using namespace gbtg;
            Actor actor;
            actor.position = {10.f, 20.f, 30.f};
            actor.radius_y = 3.f;
            actor.radius_xz = 4.f;
            actor.class_key = 42;
            std::array<u8, 0x950> body_bytes{};
            const float factors[]{2.f, 3.f};
            std::memcpy(body_bytes.data() + offsetof(gbtg::BodyRecord, height_scale), factors, 8);
            std::array<u8, 0x970> link{};
            const i16 count = 2;
            const u16 slots[]{11, 3, 42, 2};
            std::memcpy(link.data() + 0x944, &count, 2);
            std::memcpy(link.data() + 0x948, slots, 2);
            std::memcpy(link.data() + 0x94C, slots + 1, 2);
            std::memcpy(link.data() + 0x958, slots + 2, 2);
            std::memcpy(link.data() + 0x95C, slots + 3, 2);
            if (runtime_actor_case_class(link, 42) != 2 || runtime_actor_case_class(link, 99) != 3)
            {
                throw Error("native case class lookup lost matching or first-row fallback");
            }
            const i16 negative = -1;
            std::memcpy(link.data() + 0x944, &negative, 2);
            if (runtime_actor_case_class(link, 42) != 3)
            {
                throw Error("negative case count must skip search and retain first class");
            }
            const i16 zero = 0;
            std::memcpy(link.data() + 0x944, &zero, 2);
            if (runtime_actor_case_class(link, 42) != 0)
            {
                throw Error("zero case count must return zero");
            }
            std::memcpy(link.data() + 0x944, &count, 2);
            link[0x93D] = 1;
            u32 flags = 0;
            i32 population = 0;
            const auto camera_name = executable.memory(0x00314E50, 128);
            const auto offsets = executable.memory(0x00314EB0, 0x80);
            int messages = 0;
            RuntimePlayerCameraContext camera_context{factory, executable.pools,
                reinterpret_cast<const char*>(camera_name.data()), offsets, flags, population,
                [&](FactoryObject& camera, const std::array<u32, 3>& payload)
                {
                    if (camera.bytes == nullptr || payload != std::array<u32, 3>{0xBA, 0x12345678, 0})
                    {
                        throw Error("player camera message lost its created object or actor token");
                    }
                    ++messages;
                }};
            RuntimePlayerCameraSource source{actor, body_bytes, link};
            std::array<u8, 0x20> target;
            for (u8 state = 0; state <= 6; ++state)
            {
                link[0x93D] = state;
                for (u16 kind = 0; kind <= 5; ++kind)
                {
                    std::memcpy(link.data() + 0x95C, &kind, 2);
                    target.fill(0xA6);
                    build_runtime_player_camera_target(target, source, camera_context);
                    const bool valid = state > 0 && state < 5 && kind > 0 && kind < 4;
                    const size_t offset = (kind + state * 3) * 8;
                    if (target[0] != 4 || target[1] != 0x20 || target[2] != 1 || target[3] != 0xA6
                        || read_f32(target, 4) != 10.f || read_f32(target, 8) != 20.f
                        || read_f32(target, 0xC) != 30.f || read_f32(target, 0x10) != 0.f
                        || read_f32(target, 0x14) != 6.f + (valid ? read_f32(offsets, offset) : 0.f)
                        || read_f32(target, 0x18) != 0.f
                        || read_f32(target, 0x1C) != 12.f + (valid ? read_f32(offsets, offset + 4) : 0.f))
                    {
                        throw Error("player camera target changed state/class offsets or untouched byte");
                    }
                }
            }
            source.case_link = {};
            flags = 0x10;
            for (i32 players : {3, 4})
            {
                population = players;
                build_runtime_player_camera_target(target, source, camera_context);
                if (read_f32(target, 0x14) != (players < 4 ? 16.f : 6.f))
                {
                    throw Error("player camera population height gate changed");
                }
            }
            flags = 0;
            auto camera = create_runtime_player_camera(&source, 0x12345678, camera_context);
            const auto camera_parameters = CaptureFactory::parameters();
            if (messages != 1 || camera.bytes == nullptr
                || read_f32(camera_parameters, 8) != 10.f
                || read_f32(camera_parameters, 0xC) != 26.f + std::bit_cast<float>(0x4178A3D7u)
                || read_f32(camera_parameters, 0x10) != 30.f - std::bit_cast<float>(0x422AC28Fu)
                || read_f32(camera_parameters, 0x18) != std::bit_cast<float>(0x40490FDBu)
                || read_f32(camera_parameters, 0x20) != 1.f
                || read_u16(camera_parameters, 0x2AA) != 0 || read_u16(camera_parameters, 0x2AC) != 0)
            {
                throw Error("player camera clone lost pose, message or identity clearing");
            }
            release_factory_object(factory, camera);
            camera = create_runtime_player_camera(nullptr, 0, camera_context);
            if (messages != 1 || read_f32(CaptureFactory::parameters(), 8) != 0.f
                || read_f32(CaptureFactory::parameters(), 0x14) != 0.f
                || read_f32(CaptureFactory::parameters(), 0x18) != 0.f)
            {
                throw Error("null player camera source changed origin or sent an actor message");
            }
            release_factory_object(factory, camera);
            const char* saved_parameters = camera_context.parameters;
            camera_context.parameters = "none";
            camera = create_runtime_player_camera(nullptr, 0, camera_context);
            if (camera.bytes != nullptr || messages != 1)
            {
                throw Error("rejected camera parameters fabricated a factory object");
            }
            camera_context.parameters = saved_parameters;
            RuntimeActorSoundQueues sounds;
            std::array<RuntimeSoundVoice, 1> voices{};
            sounds.voices = voices;
            u32 intro_enabled = 0;
            float intro_scale = 0;
            i16 voice = -1;
            u8 configuration = 0;
            RuntimePlayerIntroSound intro{intro_enabled, intro_scale, voice, configuration, sounds};
            reset_runtime_player_intro_sound(intro);
            if (intro_enabled != 1 || intro_scale != 2.f || voice != -1)
            {
                throw Error("player intro reset changed the absent-voice path");
            }
            configuration = 0x40;
            voice = 0;
            reset_runtime_player_intro_sound(intro);
            if (intro_scale != 1.f || voice != -1 || voices[0].state != 10)
            {
                throw Error("player intro reset failed to stop the selected voice");
            }
            std::array<u8, 0xF00> actor_bytes{};
            std::array<u8, 0x198> mind_bytes;
            std::array<u8, 0xA4> mind_parameters{};
            const u32 setting = 0x89ABCDEF;
            std::memcpy(mind_parameters.data() + 0xA0, &setting, 4);
            std::array<u8, 0x108> animation_rows{};
            const u16 animation_key = 0x2C;
            std::memcpy(animation_rows.data(), &animation_key, 2);
            RuntimeScriptLookupTable animations{1, animation_rows.data()};
            u8 scene = 0;
            u32 story = 0;
            i32 health = 7;
            i32 chapter_mode = 0;
            std::vector<FactoryObject> cameras;
            RuntimePlayerMindSetup setup{actor, actor_bytes, body_bytes, &animations,
                0x12345678, scene, flags, story, health, chapter_mode, opening,
                intro, source, camera_context, cameras};
            for (int variant = 0; variant < 7; ++variant)
            {
                scene = variant == 1 ? 8 : 0;
                flags = variant == 2 ? 0x200000 : 0;
                health = variant == 3 ? 0 : 7;
                story = variant == 4 ? 0x40 : 0;
                chapter_mode = variant == 5 ? 1 : 0;
                setup.scene_name = variant == 6 ? "aid_script_ghoulies_chapter5_scene1" : opening;
                setup.animation_table = variant == 3 ? nullptr : &animations;
                mind_bytes.fill(0xA6);
                const u16 prior_health = 0xA6A6;
                gbtg::body_record(std::span<gbtg::u8>(body_bytes)).energy = prior_health;
                gbtg::body_record(std::span<gbtg::u8>(body_bytes)).max_energy = prior_health;
                intro_enabled = 0;
                if (!initialize_runtime_player_mind(mind_bytes, mind_parameters, setup))
                {
                    throw Error("translated player mind constructor changed its return");
                }
                const u16 expected_health = variant == 1 || variant == 2 ? 10 : variant == 3 ? prior_health : 7;
                if (gbtg::body_record(std::span<gbtg::u8>(body_bytes)).energy != expected_health
                    || gbtg::body_record(std::span<gbtg::u8>(body_bytes)).max_energy != expected_health
                    || intro_enabled != (variant == 0 ? 1u : 0u)
                    || gbtg::player_record(std::span<gbtg::u8>(actor_bytes)).live != 1 || actor_bytes[0x331] != 1
                    || read_u32(mind_bytes, 0x144) != (variant == 3 ? 0u : 1u)
                    || read_u32(mind_bytes, 0x80) != setting || read_f32(mind_bytes, 0x12C) != 1.f
                    || read_u32(mind_bytes, 0x174) != 1 || read_u32(mind_bytes, 0x18C) != 1
                    || read_u32(mind_bytes, 0x98) != 1 || read_u32(mind_bytes, 0xC8) != 0x12345678
                    || mind_bytes[0xCC] != 3 || read_u16(mind_bytes, 0xCE) != 0
                    || mind_bytes[0x81] != 0xCD || mind_bytes[0x84] != 0xA6
                    || mind_bytes[0x190] != 0xA6 || cameras.size() != 1)
                {
                    throw Error("player mind setup changed health gates, state words or preserved fields");
                }
                release_factory_object(factory, cameras.back());
                cameras.clear();
            }
            RuntimePlayerMindRequest request_anchor{};
            RuntimePlayerMindRequests requests{request_anchor.next, {}};
            RuntimePlayerMindRequests* request_view = &requests;
            FactoryObject strategy;
            std::vector<int> order;
            RuntimePlayerStrategySetup strategy_setup{children, request_view,
                [&](FactoryObject& object, u16 previous, u16 next)
                {
                    if (previous != 7 || next != 4 || read_u32(CaptureFactory::parameters(), 8) != 0x12345678)
                    {
                        throw Error("strategy leaving dispatch lost states or parent token");
                    }
                    order.push_back(1);
                    const u16 modified = 9;
                    std::memcpy(object.bytes.get() + 0xE, &modified, 2);
                    auto* request = static_cast<RuntimePlayerMindRequest*>(engine_heap_allocate(sizeof(RuntimePlayerMindRequest)));
                    *request = {&request_anchor, nullptr, 1, 1};
                    request_anchor.next = request;
                },
                [&](FactoryObject& object, u16 next, u16 previous)
                {
                    if (next != 4 || previous != 9 || read_u16({object.bytes.get(), object.size}, 0xE) != 4
                        || request_anchor.next != nullptr)
                    {
                        throw Error("strategy entering dispatch lost post-leaving state or request cleanup");
                    }
                    order.push_back(3);
                }};
            requests.release_active = [&](RuntimePlayerMindRequest& request)
            {
                if (request.previous != nullptr || request.next != nullptr)
                {
                    throw Error("strategy mind cleanup did not detach before virtual release");
                }
                order.push_back(2);
            };
            set_runtime_player_strategy(strategy, "aid_objparams_ghoulies_strategy_player", strategy_setup);
            if (order != std::vector<int>{1, 2, 3} || strategy.bytes == nullptr)
            {
                throw Error("strategy setup reordered virtual transitions and mind cleanup");
            }
            set_runtime_player_strategy(strategy, "none", strategy_setup);
            if (strategy.bytes != nullptr || factory.count != 0)
            {
                throw Error("strategy replacement retained its old factory object");
            }
            request_view = nullptr;
            set_runtime_player_strategy(strategy, nullptr, strategy_setup);
        }
    }
    {
        gbtg::Game unbound;
        gbtg::ScriptNode node;
        node.type = 0x14;
        // The retail gate must finish without any resource or scene binding.
        if (gbtg::script_handle_cutscene_actions(unbound, node, 1) != 2)
        {
            throw gbtg::Error("gated dialogue action required scene ownership");
        }
        std::vector<gbtg::u8> dialogue_script(0x98);
        unbound.scene_script.bytes = dialogue_script;
        bool missing_owner = false;
        try
        {
            gbtg::script_handle_cutscene_actions(unbound, node, 0);
        }
        catch (const gbtg::Error& error)
        {
            missing_owner = std::string_view(error.what()).find("scene dialogue runtime is not installed")
                != std::string_view::npos;
        }
        if (!missing_owner || unbound.script_dialogue != nullptr)
        {
            throw gbtg::Error("unbound dialogue action silently advanced the live script");
        }
    }
    // Exercise the real tutorial's translated lookup and page setup
    // before checking the live script's hold. This catches incorrect
    // native views of the retail LSBL and font resources.
    gbtg::SceneDialogueResourceOwner resources(aids);
    resources.load_fonts();
    {
        gbtg::Game input_game;
        const std::unordered_map<gbtg::u32, std::span<gbtg::u8>> addresses;
        gbtg::RuntimeInputReleaseContext release{addresses};
        auto base = gbtg::create_runtime_actor_base_input(release);
        const bool base_valid = base.size() == 0xB0 && gbtg::read_u32(base, 0) == 1 &&
            std::all_of(base.begin() + 4, base.end(), [](gbtg::u8 value)
            {
                return value == 0;
            });
        gbtg::release_runtime_input_device(base, release);
        if (!base_valid)
        {
            throw gbtg::Error("player base input allocation lost its kind-1 inactive state");
        }
        auto record = gbtg::create_runtime_keyboard_input(release);
        const gbtg::RuntimeDialogueInputDevice device{record, {nullptr, nullptr, nullptr, nullptr}};
        gbtg::pad_tick(input_game, {});
        const auto samples = gbtg::runtime_pad_state_views();
        gbtg::RuntimeDialogueInputState input{samples.merged, {}, 0, 0, 0, nullptr, 0};
        gbtg::InputState pressed;
        pressed.accept = true;
        gbtg::pad_tick(input_game, pressed);
        const bool first = gbtg::runtime_input_button_edge(device, 0x100, input) != 0;
        gbtg::pad_tick(input_game, pressed);
        const bool held = gbtg::runtime_input_button_held(device, 0x100, input) != 0;
        const bool repeated = gbtg::runtime_input_button_edge(device, 0x100, input) != 0;
        gbtg::pad_tick(input_game, {});
        const bool released = gbtg::runtime_input_button_held(device, 0x100, input) == 0;
        gbtg::release_runtime_input_device(record, release);
        if (!first || !held || repeated || !released || samples.controllers.size() != 4 * 0xA8)
        {
            throw gbtg::Error("live pad samples did not preserve the dialogue A press edge");
        }
    }
    const auto table = gbtg::parse_loctext(gbtg::read_file(
        aids.game_dir / "loctext" / "english" / "0c50e153"));
    if (table.keys_to_utf8.empty())
    {
        throw gbtg::Error("player dialogue reference table is empty");
    }
    for (const auto& [key, expected] : table.keys_to_utf8)
    {
        std::array<char16_t, 0x800> text{};
        if (gbtg::acquire_localtext_key("aid_loctext_ghoulies_dialog_dialogs",
            key.c_str(), text.data(), resources.text) == 0)
        {
            throw gbtg::Error("translated dialogue lookup missed retail key: " + key);
        }
        std::string actual;
        for (const char16_t unit : std::u16string_view(text.data()))
        {
            if (unit < 0x80)
            {
                actual.push_back(static_cast<char>(unit));
            }
            else if (unit < 0x800)
            {
                actual.push_back(static_cast<char>(0xC0 | (unit >> 6)));
                actual.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
            }
            else
            {
                actual.push_back(static_cast<char>(0xE0 | (unit >> 12)));
                actual.push_back(static_cast<char>(0x80 | ((unit >> 6) & 0x3F)));
                actual.push_back(static_cast<char>(0x80 | (unit & 0x3F)));
            }
        }
        if (actual != expected)
        {
            throw gbtg::Error("translated dialogue lookup returned the wrong string: " + key +
                " actual=" + actual.substr(0, 120) + " expected=" + expected.substr(0, 120));
        }
    }
    std::cout << "player dialogue lookup: " << table.keys_to_utf8.size()
        << " retail keys matched\n";
    std::array<char16_t, 0x800> tutorial{};
    gbtg::request_namespaced_localtext("aid_loctext_ghoulies_dialog_dialogs",
        "dialogs", "chapter1_scene1_fight1_howtofight", tutorial.data(), resources.text);
    if (tutorial[0] == 0)
    {
        throw gbtg::Error("player opening tutorial localization is empty");
    }
    gbtg::DialogueOwnedResources owned;
    const std::array<gbtg::i32, 4> thresholds{};
    gbtg::u32 flags = 0;
    std::array<char, 256> names{};
    gbtg::DialogueTrophyContext trophies{thresholds, owned, flags, names.data(),
        gbtg::g_retail_dialogue_token_names, resources.registry, resources.error};
    gbtg::DialogueTagState tag_state{};
    gbtg::DialogueSoundTagState sound{};
    gbtg::DialogueTagContext tags{tag_state, trophies, sound,
        gbtg::g_retail_dialogue_tokens, gbtg::g_retail_dialogue_sound_swaps, {}, 0, 0, 0, 0};
    std::array<char16_t, 0x800> wide{};
    std::array<char, 0x800> narrow{};
    gbtg::i32 first_count = 0;
    gbtg::i32 second_count = 0;
    gbtg::i32 third_count = 0;
    gbtg::i32 elapsed = 0;
    gbtg::DialogueMeasurementContext measurement{tags, resources.metrics, resources.text,
        {}, first_count, 0, second_count, third_count, 0, elapsed, 0.f, 1.f, 1.f, 0, wide.data(), narrow.data()};
    gbtg::DialoguePageLayout page{};
    gbtg::DialoguePageSettings settings{};
    gbtg::reset_dialogue_page(page, tutorial.data(), 0, 400.f, settings, measurement);
    if (page.text == nullptr || *page.text == 0 || page.lines == 0 ||
        page.width <= 0.f || tag_state.buttons == 0)
    {
        throw gbtg::Error("player opening tutorial page setup lost its text or A prompt");
    }
    std::cout << "player tutorial resources: speaker=" << owned.speaker
        << " lines=" << page.lines << " width=" << page.width
        << " buttons=" << tag_state.buttons << "\n";
    gbtg::release_dialogue_speaker(owned, resources.registry, 0, 0, 0);
    gbtg::release_dialogue_icon(owned, resources.registry);
}

void capture_readback(const std::string& path)
{
    gbtg::RgbaImage image;
    if (gbtg::gfx_readback(image))
    {
        gbtg::write_bmp(path, image);
    }
}

// The book transition (row 3, clip 2) turns its page and holds on the chapter title until A. A moves it to phase 2
// (the scene load), phase 3 turns the leaving page and the transition ends (phase 0) in the requested scene. A row
// without a title runs through its phases on its own; `title` says whether the title is required.
void pass_chapter_title(gbtg::Game& game, const std::string& label, const std::string& script, bool title = true)
{
    bool titled = false;
    bool started = gbtg::g_scene_transition.phase != 0;
    int ended = -1;
    int last_phase = -1;
    for (int frame = 0; frame < 3600 && ended < 0; ++frame)
    {
        gbtg::InputState input;
        input.accept = titled && gbtg::g_scene_transition.phase == 1 && frame % 30 < 5;
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const int phase = gbtg::g_scene_transition.phase;
        if (phase != last_phase)
        {
            std::cout << label << ": transition row " << gbtg::g_scene_transition.row << " phase " << phase
                      << " at frame " << frame << ", script " << game.scene_script.name << "\n";
            last_phase = phase;
        }
        started = started || phase != 0;
        if (!titled && phase == 1 && gbtg::g_scene_transition.title_widget != nullptr)
        {
            titled = true;
            std::cout << label << ": chapter title shows at frame " << frame << "\n";
            capture_readback("cache/" + label + "_title.bmp");
        }
        if (phase == 3 && frame % 10 == 0)
        {
            capture_readback("cache/" + label + "_page_turn.bmp");
        }
        // Ended: back at phase 0 in the requested scene, after a transition ran or (no transition) 60 frames in.
        if (phase == 0 && game.scene_script.name == script && (started || frame >= 60))
        {
            ended = frame;
            std::cout << label << ": the transition ends at frame " << frame << ", title " << game.scene_name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    if ((title && !titled) || ended < 0)
    {
        throw gbtg::Error(label + "'s transition " + (title && !titled ? "never showed the chapter title"
            : "never ended in " + script + " (script " + game.scene_script.name + ")"));
    }
    std::cout << "PASS: " << label << (titled ? "'s chapter title moves on after A and the transition ends\n"
        : "'s transition ends without a chapter title\n");
}

// The live rare books: FX anim emitters (type 0xD) holding powerup kind 1 (+0x940) past their start (+0x93C 1 or 2).
// 0x00109FC0 starts none when the save's challenge bit (+0x3C0 of its parameters) is set.
int count_rare_books(const gbtg::Game& game)
{
    int books = 0;
    for (const auto& object : game.factory_scene->objects)
    {
        if (object.type != 0xD || object.bytes == nullptr || object.size < 0x950)
        {
            continue;
        }
        const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
        const gbtg::u32 state = gbtg::read_u32(bytes, 0x93C);
        if (gbtg::read_u32(bytes, 0x940) == 1 && (state == 1 || state == 2))
        {
            std::cout << "rare book: challenge " << gbtg::read_u32(bytes, 0x94C) << ", state " << state
                      << ", profile challenges " << int(gbtg::g_runtime_profile.challenges.bytes[0]) << "\n";
            ++books;
        }
    }
    return books;
}

// A point a few cells along the shortest walk from `from` to `to` over the scene's navigation grid (the floor under
// `from`, cells joined by their connection bits, blocked cells skipped), or nothing when the grid has no such walk.
// With `closed_goal` the goal's own cell is entered even when it is closed (a door's point lies on the door's cell).
// A goal on another floor (scene 2a9's lantern room, up a spiral stair) is walked towards through the reachable cell
// of `from`'s floor that climbs (or drops) furthest towards it, the stair's end, until the floor under `from` changes.
// Test-side steering only: the game's own path search is not involved.
std::optional<gbtg::Vec3> nav_waypoint(const gbtg::Vec3& from, const gbtg::Vec3& to, int ahead = 3,
    const std::set<gbtg::i32>* avoid = nullptr, bool closed_goal = false)
{
    using namespace gbtg::entity_detail;
    if (g_nav_count <= 0)
    {
        return std::nullopt;
    }
    const gbtg::i32 floor = navigation_floor_index(from, -1, 0);
    const NavGrid* grid = g_nav_grids[floor];
    if (grid == nullptr)
    {
        return std::nullopt;
    }
    const gbtg::i32 start = navigation_cell_index(floor, from);
    const gbtg::i32 goal_floor = navigation_floor_index(to, -1, 0);
    gbtg::i32 goal = navigation_cell_index(floor, to);
    if (start < 0 || (goal < 0 && goal_floor == floor))
    {
        return std::nullopt;
    }
    static constexpr int dx[8] = {-1, 1, 0, 0, -1, 1, -1, 1};
    static constexpr int dz[8] = {0, 0, 1, -1, 1, 1, -1, -1};
    std::vector<gbtg::i32> parent(grid->cells.size(), -2);
    std::deque<gbtg::i32> open{start};
    parent[size_t(start)] = -1;
    const bool other_floor = goal_floor != floor;
    if (other_floor)
    {
        goal = -1;
    }
    while (!open.empty() && (goal < 0 || parent[size_t(goal)] == -2))
    {
        const gbtg::i32 cell = open.front();
        open.pop_front();
        const int x = cell % grid->width;
        const int z = cell / grid->width;
        for (int direction = 0; direction < 8; ++direction)
        {
            if ((grid->cells[size_t(cell)].connections & (1u << direction)) == 0)
            {
                continue;
            }
            const int nx = x + dx[direction];
            const int nz = z + dz[direction];
            if (nx < 0 || nx >= grid->width || nz < 0 || nz >= grid->depth)
            {
                continue;
            }
            const gbtg::i32 next = nz * grid->width + nx;
            // Cells in `avoid` (where the walk was found stuck on something the grid does not mark) are skipped.
            const auto closed = [&](gbtg::i32 index)
            {
                if (index == goal)
                {
                    return !closed_goal && (grid->cells[size_t(index)].flags_7 & 2) != 0;
                }
                return (grid->cells[size_t(index)].flags_7 & 2) != 0 || (avoid != nullptr && avoid->contains(index));
            };
            if (parent[size_t(next)] != -2 || closed(next))
            {
                continue;
            }
            // A step to a cell whose floor is more than 8 above or below is a railing or a drop (scene 2a11's store
            // keeps its balcony, its stair and the floor under them on one grid).
            if (std::abs(grid->cells[size_t(next)].bottom - grid->cells[size_t(cell)].bottom) > 8)
            {
                continue;
            }
            // A diagonal step needs both cells beside it open, so the walk does not cut a wall's corner (a doorway
            // entered at a slant catches its jamb).
            if (direction >= 4 && (closed(z * grid->width + nx) || closed(nz * grid->width + x)))
            {
                continue;
            }
            parent[size_t(next)] = cell;
            open.push_back(next);
        }
    }
    if (other_floor)
    {
        // The reached cell nearest the goal's floor in height, then nearest the goal across.
        const bool up = goal_floor > floor;
        float best_height = 0.f;
        float best_distance = 0.f;
        for (gbtg::i32 index = 0; index < gbtg::i32(grid->cells.size()); ++index)
        {
            if (parent[size_t(index)] == -2)
            {
                continue;
            }
            const float height = up ? float(grid->cells[size_t(index)].top) : -float(grid->cells[size_t(index)].bottom);
            const float cx = float(g_nav_origin_x + (index % grid->width) * grid->cell) - to.x;
            const float cz = float(g_nav_origin_z + (index / grid->width) * grid->cell) - to.z;
            const float distance = cx * cx + cz * cz;
            if (goal < 0 || height > best_height || (height == best_height && distance < best_distance))
            {
                goal = index;
                best_height = height;
                best_distance = distance;
            }
        }
    }
    if (goal < 0 || parent[size_t(goal)] == -2)
    {
        return std::nullopt;
    }
    if (other_floor && goal == start)
    {
        // Already at the stair's end of this floor's grid: the open cell of the goal's floor nearest across, its
        // floor within a step of `from`, where the stair goes on.
        const NavGrid* next_grid = g_nav_grids[goal_floor];
        const bool up = goal_floor > floor;
        std::optional<gbtg::Vec3> best;
        float best_distance = 0.f;
        for (gbtg::i32 index = 0; next_grid != nullptr && index < gbtg::i32(next_grid->cells.size()); ++index)
        {
            const NavCell& candidate = next_grid->cells[size_t(index)];
            const float height = float(candidate.bottom);
            if ((candidate.flags_7 & 2) != 0 || (up ? height < from.y - 2.f || height > from.y + 16.f
                                                     : height > from.y + 2.f || height < from.y - 16.f))
            {
                continue;
            }
            const float half = float(next_grid->cell) * 0.5f;
            const gbtg::Vec3 centre{float(g_nav_origin_x + (index % next_grid->width) * next_grid->cell) + half, height,
                float(g_nav_origin_z + (index / next_grid->width) * next_grid->cell) + half};
            const float distance = (centre.x - from.x) * (centre.x - from.x) + (centre.z - from.z) * (centre.z - from.z);
            if (!best || distance < best_distance)
            {
                best = centre;
                best_distance = distance;
            }
        }
        return best;
    }
    std::vector<gbtg::i32> path;
    for (gbtg::i32 cell = goal; cell != -1; cell = parent[size_t(cell)])
    {
        path.push_back(cell);
    }
    const gbtg::i32 cell = path[path.size() > size_t(ahead) ? path.size() - 1 - size_t(ahead) : 0];
    if (cell == goal && !other_floor)
    {
        return to;
    }
    const float half = float(grid->cell) * 0.5f;
    return gbtg::Vec3{float(g_nav_origin_x + (cell % grid->width) * grid->cell) + half, from.y,
        float(g_nav_origin_z + (cell / grid->width) * grid->cell) + half};
}

// Scene 3 (chapter1_scene3_1playcam, "Player Enters Room"), from the end of its transition: Cooper's walk-in cutscene
// (script type 0x53) and the door cutscene play, A closes Crivens' pages, Cooper spawns at the player marker (the
// walk-in's 0x00127FF0) and "Free Play" holds on its conditions (the trophy's ObjectPickedUp and the skeletons'
// GhoulyTriggered). Passive, the run holds in Free Play for 600 frames with the sticks centred (the skeletons stay
// asleep); driven, it walks Cooper to the nearest live skeleton (strategy type 0x51) and flicks the right stick in
// reach. `book` 1 requires the rare book at Free Play, 0 requires it absent (its challenge already won), -1 neither.
void play_scene3_run(gbtg::Game& game, bool drive, int book)
{
    std::string title = game.scene_name;
    const std::string scene_script = game.scene_script.name;
    auto& actors = game.factory_scene->start_actors(game);
    int pages = 0;
    bool page_up = false;
    int page_frame = -1;
    int moved_on = -1;
    int free_play = -1;
    int cooper_frame = -1;
    std::map<gbtg::u32, gbtg::u16> skeleton_states;
    std::set<gbtg::u16> states_seen;
    std::optional<gbtg::Vec3> walk_last;
    std::optional<float> walk_commanded;
    float walk_offset = 0.f;
    bool skeletons_seen = false;
    int trophy_frame = -1;
    int trophy_picked = -1;
    int trophy_event = -1;
    int book_collected = -1;
    // After the throw: the walk to the rare book, then to the exit volume.
    std::optional<gbtg::Vec3> exit_last;
    std::optional<float> exit_commanded;
    float exit_offset = 0.f;
    gbtg::u32 trophy_tag = 0;
    constexpr size_t kTrophyWaypoints = 5;
    size_t trophy_waypoint = 0;
    const auto actor_of_class = [&game](gbtg::u32 key) -> gbtg::Actor*
    {
        for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
        {
            if (actor != nullptr && actor->class_key == key)
            {
                return actor;
            }
        }
        return nullptr;
    };
    const int frames = drive ? 9000 : 3600;
    for (int frame = 0; frame < frames && moved_on < 0 && (drive || free_play < 0 || frame < free_play + 600); ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5;
        std::optional<gbtg::Vec3> cooper;
        std::vector<gbtg::RuntimeDirectorActor> skeletons;
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor == nullptr)
            {
                continue;
            }
            if (mark.actor->class_key == 4)
            {
                cooper = gbtg::actor_world_position(*mark.actor);
            }
            else if (mark.actor->class_key == 52 && !mark.strategy.empty() && !mark.body.empty()
                && gbtg::body_record(mark.body).energy != 0)
            {
                // The skeletons (class 52, strategy type 0x51) with energy left.
                skeletons.push_back(mark);
            }
        }
        if (cooper && cooper_frame < 0)
        {
            cooper_frame = frame;
            std::cout << "scene3 frame " << frame << ": Cooper spawns at " << cooper->x << ", " << cooper->y << ", "
                      << cooper->z << "\n";
        }
        for (const auto& skeleton : skeletons)
        {
            const gbtg::u16 state = gbtg::strategy_base(skeleton.strategy).state;
            auto [found, inserted] = skeleton_states.try_emplace(skeleton.address, state);
            if (inserted || found->second != state)
            {
                std::cout << "scene3 frame " << frame << ": skeleton " << skeleton.address << " strategy state 0x"
                          << std::hex << state << std::dec << "\n";
                found->second = state;
                states_seen.insert(state);
            }
        }
        skeletons_seen |= !skeletons.empty();
        // Once both skeletons are down, Free Play's ObjectPickedUp condition (opcode 3) names the trophy's tag:
        // Cooper walks to it and presses A about it, as the chair driver does.
        gbtg::Actor* trophy = nullptr;
        if (drive && skeletons_seen && skeletons.empty() && free_play >= 0)
        {
            if (trophy_tag == 0)
            {
                for (const auto& condition : gbtg::g_script_conds)
                {
                    if (gbtg::cond_u32(condition, 0) == 3)
                    {
                        trophy_tag = gbtg::cond_u32(condition, 8) & 0xFFFFu;
                    }
                }
            }
            trophy = trophy_tag == 0 ? nullptr : actor_of_class(trophy_tag);
            if (trophy != nullptr && trophy_frame < 0)
            {
                trophy_frame = frame;
                const gbtg::Vec3 at = gbtg::actor_world_position(*trophy);
                std::cout << "scene3 frame " << frame << ": the skeletons are down; Cooper goes for the trophy (tag "
                          << trophy_tag << ") at " << at.x << ", " << at.y << ", " << at.z << "\n";
            }
            if (trophy != nullptr && trophy_picked < 0 && cooper && trophy->spawn_owner == actor_of_class(4))
            {
                trophy_picked = frame;
                std::cout << "scene3 frame " << frame << ": Cooper picks up the trophy\n";
                capture_readback("cache/scene3_trophy_pickup.bmp");
            }
            for (const gbtg::LiveScript* node = gbtg::g_live_scripts; node != nullptr && trophy_event < 0;
                 node = node->next)
            {
                if (node->type == 0xAD && node->word_c == gbtg::u16(trophy_tag))
                {
                    trophy_event = frame;
                    for (const auto& condition : gbtg::g_script_conds)
                    {
                        std::cout << "scene3 condition opcode " << gbtg::cond_u32(condition, 0) << " words "
                                  << std::hex << gbtg::cond_u32(condition, 4) << " " << gbtg::cond_u32(condition, 8)
                                  << " " << gbtg::cond_u32(condition, 0xC) << std::dec << "\n";
                    }
                    std::cout << "scene3 frame " << frame << ": event 0xAD with the trophy's tag\n";
                }
            }
        }
        if (drive && !page && free_play >= 0 && cooper && (!skeletons.empty() || (trophy != nullptr && trophy_picked < 0)))
        {
            const gbtg::Vec3* nearest = nullptr;
            float best = 1e30f;
            static gbtg::Vec3 target;
            for (const auto& skeleton : skeletons)
            {
                const gbtg::Vec3 position = gbtg::actor_world_position(*skeleton.actor);
                const float dx = position.x - cooper->x;
                const float dz = position.z - cooper->z;
                if (dx * dx + dz * dz < best)
                {
                    best = dx * dx + dz * dz;
                    target = position;
                    nearest = &target;
                }
            }
            if (nearest == nullptr)
            {
                // Round the snooker table by the open side of the room Cooper crossed from his spawn
                // (-115.6, 79.5): up to z 60, across to x -115, down x -100, clear of the furniture between x -117 and
                // -108 at z 16 that pins a straight walk, and in to the trophy from (-104, -30).
                const std::array<gbtg::Vec3, kTrophyWaypoints> waypoints{gbtg::Vec3{cooper->x, 0.f, 60.f},
                    gbtg::Vec3{-115.f, 0.f, 60.f}, gbtg::Vec3{-100.f, 0.f, 30.f}, gbtg::Vec3{-100.f, 0.f, -20.f},
                    gbtg::Vec3{-104.f, 0.f, -30.f}};
                while (trophy_waypoint < waypoints.size())
                {
                    const float wx = waypoints[trophy_waypoint].x - cooper->x;
                    const float wz = waypoints[trophy_waypoint].z - cooper->z;
                    // The leg into the trophy starts from (-104, -30) itself: begun 10 short of it, it runs into the
                    // cabinet's corner.
                    const float reach = trophy_waypoint + 1 == waypoints.size() ? 16.f : 100.f;
                    if (wx * wx + wz * wz > reach)
                    {
                        break;
                    }
                    ++trophy_waypoint;
                }
                target = trophy_waypoint < waypoints.size() ? waypoints[trophy_waypoint]
                    : gbtg::actor_world_position(*trophy);
                if (trophy_waypoint == 0)
                {
                    target.x = waypoints[0].x;
                }
                nearest = &target;
            }
            // The stick is camera-relative: learn the offset between the stick's angle and the direction Cooper
            // actually moved, and steer by it.
            if (walk_last && walk_commanded)
            {
                const float mx = cooper->x - walk_last->x;
                const float mz = cooper->z - walk_last->z;
                if (mx * mx + mz * mz > 0.01f)
                {
                    const float error = std::remainder(std::atan2(mx, mz) - (*walk_commanded + walk_offset),
                        6.2831853f);
                    walk_offset += error * 0.5f;
                }
            }
            const float dx = nearest->x - cooper->x;
            const float dz = nearest->z - cooper->z;
            const float length = std::sqrt(dx * dx + dz * dz);
            // With the skeletons down, each leg follows the navigation grid's walk (a fight can end below the room's
            // floor, where a straight leg runs into a wall); the leg's own target still decides when it is reached.
            gbtg::Vec3 heading = *nearest;
            if (skeletons.empty())
            {
                if (const auto step = nav_waypoint(*cooper, *nearest); step)
                {
                    heading = *step;
                }
            }
            const float stick = std::atan2(heading.x - cooper->x, heading.z - cooper->z) - walk_offset;
            if (skeletons.empty() && frame % 120 == 0)
            {
                std::cout << "scene3 frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->y << ", "
                          << cooper->z << ", waypoint " << trophy_waypoint << ", " << length << " from its target\n";
            }
            walk_commanded.reset();
            // The trophy's cabinet stops Cooper about 12 from its centre; circle it from 14.
            if (length > (skeletons.empty() && trophy_waypoint >= kTrophyWaypoints ? 14.f : 10.f))
            {
                input.pads[0].lx = gbtg::i16(std::sin(stick) * 32767.f);
                input.pads[0].ly = gbtg::i16(-std::cos(stick) * 32767.f);
                walk_commanded = stick;
            }
            else if (skeletons.empty() && trophy_waypoint >= kTrophyWaypoints)
            {
                // Beside the trophy: A, then a short step about it, until a grab point (0x00147430) is in reach.
                if (frame % 40 < 6)
                {
                    input.accept = true;
                }
                else if (frame % 40 >= 16)
                {
                    const float about = stick + 0.785398f;
                    input.pads[0].lx = gbtg::i16(std::sin(about) * 16000.f);
                    input.pads[0].ly = gbtg::i16(-std::cos(about) * 16000.f);
                }
            }
            else if ((frame / 8) % 2 == 0)
            {
                // The mind's stick gesture (0x0007D5A0) attacks when the stick comes back after a push.
                input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
                input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
            }
            walk_last = *cooper;
        }
        // Holding the trophy, Cooper throws it: the right stick pushed toward the room's middle and released (the
        // mind's attack gesture, 0x0007D5A0).
        if (drive && !page && trophy_picked >= 0 && frame >= trophy_picked + 90 && frame < trophy_picked + 98)
        {
            input.pads[0].rx = 32767;
            if (frame == trophy_picked + 90)
            {
                std::cout << "scene3 frame " << frame << ": Cooper throws the trophy" << "\n";
            }
        }
        // The scene's rare book is an FX anim emitter (type 0xD) holding powerup kind 1; in its state 1 (+0x93C) a
        // live player touching it collects it (0x0010A590 -> 0x0010A6D0), which sets state 2. After the throw Cooper
        // walks onto it, then, once Free Play's section holds at its end (type 0xF, DAT_00512518), into the exit
        // volume (mask 1, 0x00140E30) as --player-fight does.
        if (drive && !page && cooper && trophy_picked >= 0 && frame >= trophy_picked + 240)
        {
            std::optional<gbtg::Vec3> goal;
            for (const auto& object : game.factory_scene->objects)
            {
                if (object.type != 0xD || object.bytes == nullptr || object.size < 0x940)
                {
                    continue;
                }
                const gbtg::u32 state = gbtg::read_u32({object.bytes.get(), object.size}, 0x93C);
                if (state == 2 && book_collected < 0)
                {
                    book_collected = frame;
                    std::cout << "scene3 frame " << frame << ": Cooper collects the rare book\n";
                    capture_readback("cache/s3w_captures/scene3_book.bmp");
                }
                if (state == 1)
                {
                    if (const gbtg::Actor* book = actor_of_class(0x6C); book != nullptr)
                    {
                        goal = gbtg::actor_world_position(*book);
                        // Come at the book from the open floor in front of the sofa, clear of the cabinet.
                        if (cooper->z > goal->z + 25.f)
                        {
                            goal = gbtg::Vec3{goal->x + 15.f, 0.f, goal->z + 20.f};
                        }
                    }
                }
            }
            if (!goal && gbtg::g_gate_2518 != 0 && gbtg::g_occupancy_pool != nullptr)
            {
                const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
                for (const auto* record = pool.begin; record < pool.cursor; ++record)
                {
                    if ((record->mask & 1) != 0)
                    {
                        // Line up with the box across x before going in, as --player-fight does.
                        const float reach = record->extent_x * 2.f;
                        if (std::abs(cooper->z - record->center.z) > record->extent_z * 0.5f
                            && std::abs(cooper->x - record->center.x) > reach * 0.5f)
                        {
                            const float side = cooper->x < record->center.x ? -1.f : 1.f;
                            goal = gbtg::Vec3{record->center.x + side * reach, record->center.y, record->center.z};
                        }
                        else
                        {
                            goal = record->center;
                        }
                        break;
                    }
                }
            }
            if (book_collected >= 0 && frame == book_collected + 15)
            {
                capture_readback("cache/s3w_captures/scene3_book_burst.bmp");
            }
            if (goal && book_collected >= 0 && (frame - book_collected) % 60 == 0)
            {
                capture_readback("cache/s3w_captures/scene3_exit_" + std::to_string(frame - book_collected) + ".bmp");
            }
            if (goal)
            {
                if (exit_last && exit_commanded)
                {
                    const float mx = cooper->x - exit_last->x;
                    const float mz = cooper->z - exit_last->z;
                    if (mx * mx + mz * mz > 0.01f)
                    {
                        exit_offset += std::remainder(std::atan2(mx, mz) - (*exit_commanded + exit_offset),
                            6.2831853f) * 0.5f;
                    }
                }
                const float dx = goal->x - cooper->x;
                const float dz = goal->z - cooper->z;
                exit_commanded.reset();
                if (dx * dx + dz * dz > 1.f)
                {
                    const float stick = std::atan2(dx, dz) - exit_offset;
                    input.pads[0].lx = gbtg::i16(std::sin(stick) * 32767.f);
                    input.pads[0].ly = gbtg::i16(-std::cos(stick) * 32767.f);
                    exit_commanded = stick;
                }
                if (frame % 120 == 0)
                {
                    std::cout << "scene3 frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                              << " heads for " << goal->x << ", " << goal->z << (book_collected < 0 ? " (book)" : " (exit)")
                              << ", gate " << gbtg::g_gate_2518 << "\n";
                }
            }
            exit_last = *cooper;
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        if (drive && trophy_picked >= 0 && frame > trophy_picked + 90 && frame <= trophy_picked + 240)
        {
            const int after = frame - trophy_picked - 90;
            gbtg::Actor* thrown = actor_of_class(trophy_tag);
            if (after % 10 == 0)
            {
                if (thrown == nullptr)
                {
                    std::cout << "scene3 frame " << frame << ": the trophy is gone\n";
                }
                else
                {
                    const gbtg::Vec3 at = gbtg::actor_world_position(*thrown);
                    std::cout << "scene3 frame " << frame << ": trophy at " << at.x << ", " << at.y << ", " << at.z
                              << " velocity " << thrown->anchor.x << ", " << thrown->anchor.y << ", "
                              << thrown->anchor.z << "\n";
                }
            }
            if (after == 8 || after == 20 || after == 40 || after == 80)
            {
                capture_readback("cache/s3w_captures/scene3_throw_" + std::to_string(after) + ".bmp");
            }
        }
        const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        if (page_now && !page_up)
        {
            ++pages;
            page_frame = frame;
            std::cout << "scene3 frame " << frame << ": page " << pages << "\n";
        }
        if (page_now && frame == page_frame + 20)
        {
            capture_readback("cache/scene3_page" + std::to_string(pages) + ".bmp");
        }
        page_up = page_now;
        if (game.scene_name != title)
        {
            title = game.scene_name;
            std::cout << "scene3 frame " << frame << ": title " << title << "\n";
            if (title == "Free Play")
            {
                free_play = frame;
                const int books = count_rare_books(game);
                std::cout << "scene3 frame " << frame << ": " << books << " rare book(s) at Free Play\n";
                if (book >= 0 && (books > 0) != (book == 1))
                {
                    throw gbtg::Error(book == 1 ? "scene 3 starts Free Play with no rare book"
                        : "scene 3 spawned the rare book although its challenge (bit 1) is won");
                }
            }
        }
        if (frame % 300 == 0 || (free_play >= 0 && (frame - free_play) % 60 == 59 && frame - free_play < 1200))
        {
            const gbtg::EntityCamNode* active = gbtg::entity_detail::g_cam_active[0];
            std::cout << "scene3 frame " << frame << ": camera " << (active == nullptr ? "none" : active->name)
                      << ", live skeletons " << skeletons.size() << "\n";
            capture_readback("cache/scene3_" + std::to_string(frame) + ".bmp");
        }
        if (game.scene_script.name != scene_script)
        {
            moved_on = frame;
            std::cout << "scene3 frame " << frame << ": the script moves on to " << game.scene_script.name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    for (const auto& condition : gbtg::g_script_conds)
    {
        std::cout << "scene3 end condition opcode " << gbtg::cond_u32(condition, 0) << " words " << std::hex
                  << gbtg::cond_u32(condition, 4) << " " << gbtg::cond_u32(condition, 8) << " "
                  << gbtg::cond_u32(condition, 0xC) << std::dec << "\n";
    }
    std::cout << "scene3: trophy from frame " << trophy_frame << ", picked up at " << trophy_picked << ", event 0xAD at "
              << trophy_event << "\n";
    std::cout << "scene3: pages " << pages << ", title " << game.scene_name << ", Cooper from frame " << cooper_frame
              << ", Free Play from frame " << free_play << "\n";
    if (cooper_frame < 0)
    {
        throw gbtg::Error("scene 3's walk-in never spawned Cooper (0x00127FF0)");
    }
    if (free_play < 0)
    {
        throw gbtg::Error("scene 3's script never reached Free Play: title " + game.scene_name);
    }
    if (moved_on >= 0 && moved_on < free_play + 60)
    {
        throw gbtg::Error("scene 3's Free Play did not hold: the script moved on to " + game.scene_script.name);
    }
    if (gbtg::entity_detail::g_cam_active[0] == nullptr)
    {
        throw gbtg::Error("scene 3 ends with no camera view bound in slot 0");
    }
    if (drive && (trophy_picked < 0 || trophy_event < 0))
    {
        throw gbtg::Error("scene 3's driven run did not pick up the trophy (type 0x7D, 0x0009E6B0) and queue its "
                          "event 0xAD");
    }
    if (drive)
    {
        std::cout << "PASS: scene 3's skeletons fall to Cooper and he lifts the trophy, queueing its event 0xAD"
                  << "\n";
    }
    if (drive && book_collected < 0)
    {
        throw gbtg::Error("scene 3's driven run did not collect the rare book (type 0xD, 0x0010A6D0)");
    }
    if (drive && moved_on < 0)
    {
        throw gbtg::Error("scene 3's driven run did not leave through the exit volume: the script holds at " +
                          game.scene_script.name);
    }
    if (drive)
    {
        std::cout << "PASS: scene 3's rare book is collected at frame " << book_collected << " and Cooper leaves "
                  << "through the open door at frame " << moved_on << " (" << game.scene_script.name << ")\n";
    }
    std::cout << "PASS: scene 3's walk-in spawns Cooper, Crivens' pages close on A and Free Play holds with the "
                 "player camera\n";
}

// The actor at an object, through the scene's retail address map; null when it has none.
const gbtg::Actor* actor_of_object(gbtg::Game& game, const gbtg::FactoryObject& object)
{
    const auto& addresses = game.factory_scene->start_actors(game).actor_addresses();
    const auto found = addresses.find(game.factory_scene->address_of(object.bytes.get()));
    return found == addresses.end() ? nullptr : found->second;
}

// Every scene object with an actor: its factory type, class key and position.
void log_scene_objects(gbtg::Game& game, const std::string& label, int frame)
{
    for (const auto& object : game.factory_scene->objects)
    {
        if (object.bytes == nullptr)
        {
            continue;
        }
        const gbtg::Actor* actor = actor_of_object(game, object);
        if (actor == nullptr)
        {
            continue;
        }
        const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
        std::cout << label << " frame " << frame << ": object type 0x" << std::hex << object.type << " class 0x"
                  << actor->class_key << std::dec << " at " << at.x << ", " << at.y << ", " << at.z;
        if (object.type == 0xD && object.size >= 0x950)
        {
            // An FX anim emitter's powerup: its kind (+0x940) and start state (+0x93C).
            const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
            std::cout << " powerup kind 0x" << std::hex << gbtg::read_u32(bytes, 0x940) << std::dec << " state "
                      << gbtg::read_u32(bytes, 0x93C);
        }
        if (object.type == 0x24 && object.size >= 0xA7C)
        {
            // A bg-break setup: its ghoulybox (+0x934), whether its part is gone (+0xA50) and the part (+0xA58).
            const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
            std::cout << " box " << reinterpret_cast<const char*>(bytes.data() + 0x934) << " broken "
                      << gbtg::read_u32(bytes, 0xA50) << " part " << gbtg::i32(gbtg::read_u32(bytes, 0xA58));
        }
        if (object.type == 0xA2 && object.size >= 0xEE0)
        {
            // A container weapon: its contents (+0xE60), uses left (+0xE58) and weapon state (+0xC0C).
            const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
            std::cout << " contents " << reinterpret_cast<const char*>(bytes.data() + 0xE60) << " left "
                      << gbtg::read_u32(bytes, 0xE58) << " state " << gbtg::read_u16(bytes, 0xC0C);
        }
        std::cout << "\n";
    }
}

// Scene 4_2's breakables: the bg-break keys (type 0x1E), the bg-break setups (type 0x24) and Cooper.
void log_scene4_breakables(gbtg::Game& game, const std::string& label, int frame)
{
    if (const auto* steps = gbtg::g_event_sequence.steps; steps != nullptr)
    {
        std::cout << label << " frame " << frame << ": event state " << gbtg::g_event_sequence_state << ", steps";
        for (const gbtg::u8* step = steps->begin; step < steps->end; step += steps->stride)
        {
            std::cout << " " << gbtg::read_u32({step, 0x88}, 0);
        }
        std::cout << "\n";
    }
    for (const auto& object : game.factory_scene->objects)
    {
        if (object.bytes == nullptr || (object.type != 0x1E && object.type != 0x24 && object.type != 5))
        {
            continue;
        }
        const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
        const gbtg::Actor* actor = actor_of_object(game, object);
        std::cout << label << " frame " << frame << ": type 0x" << std::hex << object.type << std::dec;
        if (actor != nullptr)
        {
            const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
            std::cout << " at " << at.x << ", " << at.y << ", " << at.z;
        }
        if (object.type == 0x1E)
        {
            std::cout << " background 0x" << std::hex << gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).background << std::dec << " part "
                      << gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).part << " armed " << gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).armed << " active "
                      << gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).active;
        }
        else if (object.type == 0x24)
        {
            std::cout << " box '" << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).box.data() << "' background 0x" << std::hex
                      << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).background << std::dec << " part " << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).part
                      << " broken " << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).broken << " radius " << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).radius
                      << " grouped " << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).grouped << " hold " << gbtg::actor_record_view<gbtg::GhoulyBgBreakState>(bytes).hold_part;
        }
        std::cout << "\n";
    }
}

// Steers Cooper with the camera-relative left stick: it learns the offset between the stick's angle and the
// direction Cooper actually moved, as scene 3's driver does.
struct StickSteering
{
    std::optional<gbtg::Vec3> last;
    std::optional<float> commanded;
    float offset = 0.f;
    float fastest = 0.f;
    // The stick angle toward `goal`, after learning from the last move. Only a move at nearly the recent fastest speed
    // is learned from: a slide along a wall is slower and turned off the stick's line, and learning it steers Cooper
    // back along the wall. The fastest speed fades, so a knockback's burst does not stop the learning for good.
    float angle(const gbtg::Vec3& cooper, const gbtg::Vec3& goal)
    {
        if (last && commanded)
        {
            const float mx = cooper.x - last->x;
            const float mz = cooper.z - last->z;
            const float moved = mx * mx + mz * mz;
            fastest = std::max(moved, fastest * 0.98f);
            if (moved > 0.01f && moved >= fastest * 0.5f)
            {
                offset += std::remainder(std::atan2(mx, mz) - (*commanded + offset), 6.2831853f) * 0.5f;
            }
        }
        last = cooper;
        commanded.reset();
        return std::atan2(goal.x - cooper.x, goal.z - cooper.z) - offset;
    }
    void walk(gbtg::InputState& input, float stick)
    {
        input.pads[0].lx = gbtg::i16(std::sin(stick) * 32767.f);
        input.pads[0].ly = gbtg::i16(-std::cos(stick) * 32767.f);
        commanded = stick;
    }
};

// Scene 4 (chapter1_scene4_1book, "Partner and the False Fireplace"), from the end of its transition: A closes each
// page, and the run logs its titles and scripts and captures every 300 frames until the script moves on.
void play_scene4_run(gbtg::Game& game, const std::string& label, bool drive)
{
    std::string title = game.scene_name;
    const std::string scene_script = game.scene_script.name;
    auto& actors = game.factory_scene->start_actors(game);
    int pages = 0;
    bool page_up = false;
    int page_frame = -1;
    int moved_on = -1;
    StickSteering steering;
    int key_out = -1;
    for (int frame = 0; frame < 12000 && moved_on < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5;
        // Driven: once the challenge runs, Cooper walks to the key's background part (the bg-break key, type 0x1E,
        // sits beside it) and swings at it until it breaks; after the key's show, into the exit volume.
        std::optional<gbtg::Vec3> cooper;
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor != nullptr && mark.actor->class_key == 4)
            {
                cooper = gbtg::actor_world_position(*mark.actor);
            }
        }
        std::optional<gbtg::Vec3> key_at;
        bool key_active = false;
        for (const auto& object : game.factory_scene->objects)
        {
            if (object.type == 0x1E && object.bytes != nullptr)
            {
                const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                key_active = gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).active != 0;
                if (const gbtg::Actor* actor = actor_of_object(game, object); actor != nullptr)
                {
                    key_at = gbtg::actor_world_position(*actor);
                }
            }
        }
        if (key_active && key_out < 0)
        {
            key_out = frame;
            std::cout << label << " frame " << frame << ": the key comes out\n";
        }
        if (key_out >= 0 && (frame - key_out) % 20 == 0 && frame - key_out <= 140)
        {
            capture_readback("cache/" + label + "_key_" + std::to_string(frame - key_out) + ".bmp");
        }
        if (drive && !page && cooper && title != "Exit Door Closes" && key_out < 0 && key_at)
        {
            const float dx = key_at->x - cooper->x;
            const float dz = key_at->z - cooper->z;
            const float stick = steering.angle(*cooper, *key_at);
            if (dx * dx + dz * dz > 18.f * 18.f)
            {
                steering.walk(input, stick);
            }
            else if ((frame / 8) % 2 == 0)
            {
                // The mind's stick gesture (0x0007D5A0) swings when the stick comes back after a push.
                input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
                input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << ", the key's part " << std::sqrt(dx * dx + dz * dz) << " away\n";
            }
        }
        else if (drive && !page && cooper && key_out >= 0 && !key_active && gbtg::g_occupancy_pool != nullptr)
        {
            const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
            for (const auto* record = pool.begin; record < pool.cursor; ++record)
            {
                if ((record->mask & 1) != 0)
                {
                    // Over the navigation grid: the straight line from the key catches on the debris by the side
                    // doorway.
                    const auto step = nav_waypoint(*cooper, record->center);
                    steering.walk(input, steering.angle(*cooper, step ? *step : record->center));
                    if (frame % 120 == 0)
                    {
                        std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                                  << " heads for the exit at " << record->center.x << ", " << record->center.z
                                  << (step ? " by the grid" : " straight") << "\n";
                    }
                    break;
                }
            }
        }
        if (frame % 600 == 150 && label == "scene4_2")
        {
            log_scene4_breakables(game, label, frame);
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        if (page_now && !page_up)
        {
            ++pages;
            page_frame = frame;
            std::cout << label + " frame " << frame << ": page " << pages << "\n";
        }
        if (page_now && frame == page_frame + 20)
        {
            capture_readback("cache/" + label + "_page" + std::to_string(pages) + ".bmp");
        }
        page_up = page_now;
        if (game.scene_name != title)
        {
            title = game.scene_name;
            std::cout << label + " frame " << frame << ": title " << title << "\n";
            if (title == "Event 'Find The Key'")
            {
                key_out = -1; // A restarted scene hides the key again.
            }
            if (label == "scene5" && title == "Free Play")
            {
                log_scene_objects(game, label, frame);
            }
        }
        if (frame % 300 == 0)
        {
            capture_readback("cache/" + label + "_" + std::to_string(frame) + ".bmp");
        }
        if (game.scene_script.name != scene_script)
        {
            moved_on = frame;
            std::cout << label + " frame " << frame << ": the script moves on to " << game.scene_script.name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    std::cout << label + ": pages " << pages << ", title " << title << "\n";
    if (moved_on < 0)
    {
        throw gbtg::Error(label + " did not move on: the script holds at " + game.scene_script.name + " (title "
            + title + ")");
    }
    if (drive && key_out < 0)
    {
        throw gbtg::Error(label + " moved on without the bg-break key's show (type 0x1E, 0x0006CD40)");
    }
    if (drive)
    {
        std::cout << "PASS: " + label + "'s key part breaks under Cooper's bat and the key comes out at frame "
                  << key_out << "\n";
    }
    std::cout << "PASS: " + label + " plays to its end and the script moves on to " << game.scene_script.name << "\n";
}

// Scene 5 (chapter1_scene5_1playcam, the snooker room), from the end of its transition. Free Play holds until a
// weapon is picked up (ObjectPickedUp on the cue or the trophy); driven, Cooper walks to the nearest cue (type 0xAA)
// and presses A beside it, then attacks the nearest live skeleton (class 52) while any is up, and once the exit
// volume (mask 1) is open walks into it. Pages close on A; titles, scripts and every 300th frame are logged.
void play_scene5_run(gbtg::Game& game, const std::string& label)
{
    float closest_to_broken_table = 1e9f;
    bool table_taken = false; // The bob reached state 3 (held), as the swing state 0x78 takes it.
    std::string title = game.scene_name;
    const std::string scene_script = game.scene_script.name;
    auto& actors = game.factory_scene->start_actors(game);
    int pages = 0;
    bool page_up = false;
    int moved_on = -1;
    int picked = -1;
    bool fainted = false;
    std::set<std::string> titles;
    StickSteering steering;
    for (int frame = 0; frame < 18000 && moved_on < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5;
        std::optional<gbtg::Vec3> cooper;
        const gbtg::Actor* held = nullptr;
        std::optional<gbtg::Vec3> skeleton;
        float nearest = 1e9f;
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor == nullptr)
            {
                continue;
            }
            if (mark.actor->class_key == 4)
            {
                cooper = gbtg::actor_world_position(*mark.actor);
                // A lost skeleton fight: Cooper's energy (body +0x94) runs out and the event goes back to Free Play.
                fainted = fainted || (picked >= 0 && !mark.body.empty() && gbtg::body_record(mark.body).energy == 0);
            }
        }
        for (const auto& mark : actors.marked_actors())
        {
            if (cooper && mark.actor != nullptr && mark.actor->class_key == 52 && !mark.body.empty()
                && gbtg::body_record(mark.body).energy != 0)
            {
                const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                if (d < nearest)
                {
                    nearest = d;
                    skeleton = at;
                }
            }
        }
        std::optional<gbtg::Vec3> cue;
        float cue_distance = 1e9f;
        for (const auto& object : game.factory_scene->objects)
        {
            if (object.type != 0xAA || object.bytes == nullptr || !cooper)
            {
                continue;
            }
            if (const gbtg::Actor* actor = actor_of_object(game, object); actor != nullptr)
            {
                const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                if (d < cue_distance)
                {
                    cue_distance = d;
                    cue = at;
                }
            }
        }
        (void)held;
        if (!page && cooper)
        {
            std::optional<gbtg::Vec3> goal;
            bool swing = false;
            bool grab = false;
            // Stage 2's skeleton waves keep coming while its section holds at its end with the exit open
            // (g_gate_2518): leave through the exit volume, by a waypoint in front of it, rather than fight on.
            std::optional<gbtg::Vec3> exit;
            if (gbtg::g_gate_2518 != 0 && gbtg::g_occupancy_pool != nullptr)
            {
                const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
                for (const auto* record = pool.begin; record < pool.cursor; ++record)
                {
                    if ((record->mask & 1) != 0)
                    {
                        exit = record->center;
                        break;
                    }
                }
            }
            // Stage 2: the uncovered snooker table (the bob, type 0xA0) is the weapon. Cooper takes it with A beside
            // it, then swings it at the skeletons; its state word (+0xC0C) leaves 0x10 (idle) once it is in use.
            std::optional<gbtg::Vec3> table;
            std::optional<gbtg::Vec3> table_grip;
            gbtg::u8 table_mask = 0;
            gbtg::u32 table_holders = 0;
            gbtg::u32 table_blocked = 0;
            gbtg::u16 table_state = 0;
            for (const auto& object : game.factory_scene->objects)
            {
                if (object.type == 0xA0 && object.bytes != nullptr && object.size > 0xC0E)
                {
                    table_state = gbtg::read_u16({object.bytes.get(), object.size}, 0xC0C);
                    table_mask = object.bytes[0x93C];
                    table_holders = gbtg::read_u32({object.bytes.get(), object.size}, 0xE54);
                    table_blocked = gbtg::read_u32({object.bytes.get(), object.size}, 0xAD0);
                    if (const gbtg::Actor* actor = actor_of_object(game, object); actor != nullptr)
                    {
                        table = gbtg::actor_world_position(*actor);
                        // The pick-up sphere nearest Cooper (0x001476D0), where a player would stand to take it.
                        gbtg::Vec3 point;
                        float radius = 0.f;
                        gbtg::i32 index = 0;
                        if (gbtg::select_actor_spawn_point(*actor, *cooper, point, radius, index))
                        {
                            table_grip = point;
                        }
                    }
                }
            }
            if (frame % 300 == 0 && table)
            {
                std::cout << label << " frame " << frame << ": table state 0x" << std::hex << table_state << std::dec
                          << " at " << table->x << ", " << table->z << " mask " << int(table_mask) << " holders "
                          << table_holders << " blocked " << table_blocked << "\n";
                if (frame == 0)
                {
                    for (const auto& object : game.factory_scene->objects)
                    {
                        if (object.type == 0xA0 && object.bytes != nullptr)
                        {
                            std::cout << label << ": table names +0xDD0 '"
                                      << reinterpret_cast<const char*>(object.bytes.get() + 0xDD0) << "' +0xE5C '"
                                      << reinterpret_cast<const char*>(object.bytes.get() + 0xE5C) << "' +0xEDC '"
                                      << reinterpret_cast<const char*>(object.bytes.get() + 0xEDC) << "' +0xF60 '"
                                      << reinterpret_cast<const char*>(object.bytes.get() + 0xF60) << "'\n";
                        }
                    }
                }
            }
            if (title == "stage 2" && table)
            {
                table_taken = table_taken || table_state == 3;
                if (table_state == 0x10 && table_mask == 0)
                {
                    const float tx = table->x - cooper->x;
                    const float tz = table->z - cooper->z;
                    const float gap = std::sqrt(tx * tx + tz * tz);
                    if (gap < closest_to_broken_table)
                    {
                        closest_to_broken_table = gap;
                        std::cout << label << " frame " << frame << ": Cooper " << gap
                                  << " from the broken table's centre\n";
                    }
                }
                // A broken table (+0x93C clear after its swing) can't be taken again: fight on by hand.
                if (table_state == 0x10 && table_mask != 0 && table_grip)
                {
                    const float gx = table_grip->x - cooper->x;
                    const float gz = table_grip->z - cooper->z;
                    goal = table_grip;
                    grab = gx * gx + gz * gz < 12.f * 12.f;
                }
                else if (skeleton)
                {
                    goal = skeleton;
                    swing = true;
                }
            }
            else if (exit && title == "Free Play" && picked >= 0)
            {
                const gbtg::Vec3 front{exit->x, exit->y, exit->z - 60.f};
                const bool before = cooper->z < front.z - 10.f && std::abs(cooper->x - front.x) > 20.f;
                goal = before ? front : *exit;
                swing = skeleton && nearest < 10.f * 10.f;
            }
            else if (skeleton)
            {
                goal = skeleton;
                swing = nearest < 16.f * 16.f;
            }
            else if (picked < 0 && title == "Free Play" && cue)
            {
                goal = cue;
                grab = cue_distance < 12.f * 12.f;
            }
            else if (gbtg::g_gate_2518 != 0 && gbtg::g_occupancy_pool != nullptr)
            {
                const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
                for (const auto* record = pool.begin; record < pool.cursor; ++record)
                {
                    if ((record->mask & 1) != 0)
                    {
                        goal = record->center;
                        break;
                    }
                }
            }
            if (goal)
            {
                const float stick = steering.angle(*cooper, *goal);
                if (grab)
                {
                    if (frame % 40 < 6)
                    {
                        input.accept = true;
                    }
                    else if (frame % 40 >= 16)
                    {
                        const float about = stick + 0.785398f;
                        input.pads[0].lx = gbtg::i16(std::sin(about) * 16000.f);
                        input.pads[0].ly = gbtg::i16(-std::cos(about) * 16000.f);
                    }
                }
                else if (swing)
                {
                    if ((frame / 8) % 2 == 0)
                    {
                        input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
                        input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
                    }
                }
                else
                {
                    steering.walk(input, stick);
                }
                if (frame % 120 == 0)
                {
                    std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                              << " heads for " << goal->x << ", " << goal->z
                              << (skeleton ? " (skeleton)" : picked < 0 ? " (cue)" : " (exit)") << "\n";
                }
            }
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        if (page_now && !page_up)
        {
            ++pages;
            std::cout << label << " frame " << frame << ": page " << pages << "\n";
            capture_readback("cache/" + label + "_page" + std::to_string(pages) + ".bmp");
        }
        page_up = page_now;
        if (game.scene_name != title)
        {
            const std::string previous = title;
            title = game.scene_name;
            titles.insert(title);
            std::cout << label << " frame " << frame << ": title " << title << "\n";
            if (picked < 0 && title != "Free Play" && titles.count("Free Play") != 0)
            {
                picked = frame;
            }
            else if (title == "Free Play" && (fainted || previous == "stage 2"))
            {
                // The event was lost (Cooper fainted, or its time ran out in stage 2): take the cue again to
                // restart it.
                std::cout << label << " frame " << frame << ": the event was lost, the cue is taken again\n";
                picked = -1;
                fainted = false;
            }
        }
        if (frame % 300 == 0)
        {
            capture_readback("cache/" + label + "_" + std::to_string(frame) + ".bmp");
            if (label == "scene5")
            {
                std::cout << label << " frame " << frame << ": title mode " << gbtg::g_state_248c << ", decay "
                          << int(gbtg::scene_detail::g_decay_arm) << ", conditions";
                for (const auto& rec : gbtg::g_script_conds)
                {
                    std::cout << " [" << gbtg::cond_u32(rec, 0) << " link +0x" << std::hex << gbtg::cond_u32(rec, 4)
                              << std::dec << " " << gbtg::cond_f32(rec, 8) << " " << gbtg::cond_u32(rec, 0xC) << "]";
                }
                std::cout << "\n";
                if (const auto* steps = gbtg::g_event_sequence.steps; steps != nullptr)
                {
                    std::cout << label << " frame " << frame << ": event state " << gbtg::g_event_sequence_state;
                    for (const gbtg::u8* step = steps->begin; step < steps->end; step += steps->stride)
                    {
                        const std::span<const gbtg::u8> row{step, 0x88};
                        std::cout << " [type " << gbtg::read_u32(row, 0) << " need " << gbtg::read_u32(row, 0x18)
                                  << " tag " << gbtg::read_u16(row, 0x1C) << " kills "
                                  << gbtg::script_statistic_at_address(0x00512B78u + gbtg::read_u16(row, 0x1C) * 4u)
                                  << "]";
                    }
                    std::cout << "\n";
                }
                // Cooper's energy (body +0x94) and the body states (factory types 0x5E..0x8F) whose +8 is his body.
                for (const auto& mark : actors.marked_actors())
                {
                    if (mark.actor == nullptr || mark.actor->class_key != 4 || mark.body.empty())
                    {
                        continue;
                    }
                    const gbtg::u32 body = game.factory_scene->address_of(mark.body.data());
                    std::cout << label << " frame " << frame << ": Cooper energy " << gbtg::body_record(mark.body).energy
                              << ", states";
                    std::span<const gbtg::u8> second_bytes;
                    const auto [first_action, second_action] = actors.player_action_types(*mark.actor, &second_bytes);
                    std::cout << " first 0x" << std::hex << first_action << " second 0x" << second_action << std::dec;
                    if (auto* stack = gbtg::find_scene_cutscene_runtime(game); stack != nullptr)
                    {
                        std::cout << " cutscenes " << stack->index << " stop " << stack->stop << " linked " << stack->linked;
                    }
                    std::cout << " bound " << (mark.actor->bind != nullptr && mark.actor->bind->parent != nullptr)
                              << " held 0x" << std::hex << gbtg::body_record(mark.body).attached << " +0x4C 0x"
                              << gbtg::body_record(mark.body).unk_4C << std::dec << " +0x30 " << gbtg::body_record(mark.body).move_holds
                              << " +0x258 " << gbtg::body_record(mark.body).holds << " +0x6A " << int(gbtg::body_record(mark.body).unk_6A);
                    if (second_action == 0x7A && second_bytes.size() > 0x60)
                    {
                        std::cout << " knockdown phase " << gbtg::read_u32(second_bytes, 0x18) << " +0x34 "
                                  << gbtg::read_u32(second_bytes, 0x34) << " +0x3C " << gbtg::read_u32(second_bytes, 0x3C)
                                  << " +0x40 " << gbtg::read_u32(second_bytes, 0x40) << " +0x50 "
                                  << gbtg::read_u32(second_bytes, 0x50) << " +0x20 " << gbtg::read_f32(second_bytes, 0x20)
                                  << " rate " << mark.actor->animation_rate << " pause " << mark.actor->animation_pause
                                  << " +0x5E " << gbtg::read_u16(second_bytes, 0x5E) << " +0xC "
                                  << gbtg::read_u16(second_bytes, 0xC) << " body +0x73C "
                                  << gbtg::body_record(mark.body).unk_73C;
                    }
                    for (const auto& object : game.factory_scene->objects)
                    {
                        const gbtg::u32 at = object.bytes == nullptr ? 0 : game.factory_scene->address_of(object.bytes.get());
                        if (at != 0 && (at == gbtg::body_record(mark.body).unk_740
                            || at == gbtg::body_record(mark.body).unk_744))
                        {
                            std::cout << " 0x" << std::hex << object.type << std::dec
                                      << (at == gbtg::body_record(mark.body).unk_744 ? " (+0x744)" : " (+0x740)");
                        }
                    }
                    std::cout << "\n";
                }
            }
        }
        if (game.scene_script.name != scene_script)
        {
            moved_on = frame;
            std::cout << label << " frame " << frame << ": the script moves on to " << game.scene_script.name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    std::cout << label << ": pages " << pages << ", title " << title << "\n";
    if (moved_on < 0)
    {
        throw gbtg::Error(label + " did not move on: the script holds at " + game.scene_script.name + " (title "
            + title + ")");
    }
    if (!table_taken)
    {
        throw gbtg::Error(label + ": Cooper never took the snooker table");
    }
    // The broken table keeps its volume: Cooper stays outside its radius (about 25) instead of walking into it.
    if (closest_to_broken_table < 20.f)
    {
        throw gbtg::Error(label + ": Cooper walked " + std::to_string(closest_to_broken_table)
            + " from the broken table's centre");
    }
    std::cout << "PASS: " + label + " swings the snooker table, which breaks and stays solid (closest "
              << closest_to_broken_table << ")\n";
    std::cout << "PASS: " + label + " plays to its end and the script moves on to " << game.scene_script.name << "\n";
}

// Scene 6 (chapter1_scene6_1playcam, the Lower Corridors), from the end of its transition. Pages close on A; titles,
// scripts, Cooper's position and every 300th frame are logged until the script moves on.
void play_scene6_1_run(gbtg::Game& game, const std::string& label)
{
    std::string title = game.scene_name;
    const std::string scene_script = game.scene_script.name;
    auto& actors = game.factory_scene->start_actors(game);
    int pages = 0;
    bool page_up = false;
    int page_shown = -1;
    int moved_on = -1;
    StickSteering steering;
    for (int frame = 0; frame < 9000 && moved_on < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        // A closes a page once it has been up a second.
        input.accept = page && page_shown >= 0 && frame > page_shown + 60 && frame % 30 < 5;
        std::optional<gbtg::Vec3> cooper;
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor != nullptr && mark.actor->class_key == 4)
            {
                cooper = gbtg::actor_world_position(*mark.actor);
            }
        }
        // Free Play waits for the scare figure (type 0x1D) to see Cooper: walk him to its trigger point (+0x934).
        std::optional<gbtg::Vec3> scare;
        for (const auto& object : game.factory_scene->objects)
        {
            if (object.type == 0x1D && object.bytes != nullptr && object.size >= 0xB84)
            {
                const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                if (gbtg::read_u32(bytes, 0x960) == 0)
                {
                    scare = gbtg::Vec3{gbtg::read_f32(bytes, 0x934), gbtg::read_f32(bytes, 0x938),
                        gbtg::read_f32(bytes, 0x93C)};
                }
            }
        }
        // The Ghouly Generator Introduction waits for a spider: the bg-break setups (type 0x24) break their part and
        // spawn their ghoulybox once Cooper is inside their radius (+0xA38). Walk him under the nearest unbroken one.
        std::optional<gbtg::Vec3> generator;
        if (cooper && title == "Ghouly Generator Introduction")
        {
            float nearest = 1e9f;
            for (const auto& object : game.factory_scene->objects)
            {
                if (object.type != 0x24 || object.bytes == nullptr || object.size < 0xA60
                    || gbtg::read_u32({object.bytes.get(), object.size}, 0xA50) != 0
                    || gbtg::read_f32({object.bytes.get(), object.size}, 0xA38) == 0.f)
                {
                    continue;
                }
                if (const gbtg::Actor* actor = actor_of_object(game, object); actor != nullptr)
                {
                    const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
                    const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                    if (d < nearest)
                    {
                        nearest = d;
                        generator = at;
                    }
                }
            }
        }
        // Any live ghouly (a player actor other than Cooper with body +0x94 energy) within 50 units is fought first.
        std::optional<gbtg::Vec3> ghouly;
        if (cooper && title == "Ghouly Generator Introduction")
        {
            float nearest = 50.f * 50.f;
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor == nullptr || mark.actor->class_key == 4 || mark.body.empty()
                    || gbtg::body_record(mark.body).energy == 0)
                {
                    continue;
                }
                const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                if (d < nearest)
                {
                    nearest = d;
                    ghouly = at;
                }
            }
        }
        // A section held at its end (type 0xF, DAT_00512518) waits for the player to leave: walk into the exit volume
        // (mask 1), lined up with it across x first, rather than fight the generator's spiders on.
        std::optional<gbtg::Vec3> exit;
        if (cooper && title == "Ghouly Generator Introduction" && gbtg::g_gate_2518 != 0 && gbtg::g_occupancy_pool != nullptr)
        {
            const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
            for (const auto* record = pool.begin; record < pool.cursor; ++record)
            {
                if ((record->mask & 1) != 0)
                {
                    // The corridor from the scare figure runs south along x -168, then east along z -620; the exit is
                    // a doorway in its south wall.
                    exit = record->center;
                    if (cooper->z > -600.f)
                    {
                        exit = gbtg::Vec3{-168.f, record->center.y, -620.f};
                    }
                    else if (std::abs(cooper->x - record->center.x) > record->extent_x * 0.5f)
                    {
                        exit = gbtg::Vec3{record->center.x, record->center.y, -620.f};
                    }
                    break;
                }
            }
        }
        // On the way out, a spider within 20 units is fought off first.
        const bool close = ghouly && (ghouly->x - cooper->x) * (ghouly->x - cooper->x)
            + (ghouly->z - cooper->z) * (ghouly->z - cooper->z) < 20.f * 20.f;
        if (!page && cooper && exit && !close)
        {
            steering.walk(input, steering.angle(*cooper, *exit));
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << " heads for the exit at " << exit->x << ", " << exit->z << "\n";
            }
        }
        else if (!page && cooper && ghouly)
        {
            const float stick = steering.angle(*cooper, *ghouly);
            const float dx = ghouly->x - cooper->x;
            const float dz = ghouly->z - cooper->z;
            if (dx * dx + dz * dz < 16.f * 16.f)
            {
                if ((frame / 8) % 2 == 0)
                {
                    input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
                    input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
                }
            }
            else
            {
                steering.walk(input, stick);
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << " fights a ghouly at " << ghouly->x << ", " << ghouly->z << "\n";
            }
        }
        else if (!page && cooper && generator)
        {
            const float dx = generator->x - cooper->x;
            const float dz = generator->z - cooper->z;
            if (dx * dx + dz * dz > 9.f)
            {
                steering.walk(input, steering.angle(*cooper, *generator));
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << " heads for the ghouly setup at " << generator->x << ", " << generator->z << "\n";
                for (const auto& object : game.factory_scene->objects)
                {
                    if (object.type == 0x24 && object.bytes != nullptr && object.size >= 0xA60)
                    {
                        const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                        std::cout << label << " frame " << frame << ": ghouly setup radius "
                                  << gbtg::read_f32(bytes, 0xA38) << " chance " << gbtg::read_f32(bytes, 0xA3C)
                                  << " broken " << gbtg::read_u32(bytes, 0xA50) << " box '"
                                  << reinterpret_cast<const char*>(bytes.data() + 0x934) << "'\n";
                    }
                }
            }
        }
        if (!page && cooper && scare && title == "Free Play")
        {
            const float dx = scare->x - cooper->x;
            const float dz = scare->z - cooper->z;
            if (dx * dx + dz * dz > 4.f)
            {
                steering.walk(input, steering.angle(*cooper, *scare));
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << " heads for the scare figure at " << scare->x << ", " << scare->z << "\n";
            }
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        if (page_now && !page_up)
        {
            ++pages;
            page_shown = frame;
            std::cout << label << " frame " << frame << ": page " << pages << "\n";
        }
        if (page_now && frame == page_shown + 60)
        {
            capture_readback("cache/" + label + "_page" + std::to_string(pages) + ".bmp");
        }
        page_up = page_now;
        if (game.scene_name != title)
        {
            title = game.scene_name;
            std::cout << label << " frame " << frame << ": title " << title << "\n";
            log_scene_objects(game, label, frame);
        }
        if (frame % 300 == 0)
        {
            capture_readback("cache/" + label + "_" + std::to_string(frame) + ".bmp");
            if (frame == 0)
            {
                log_scene_objects(game, label, frame);
            }
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.actor->class_key == 4)
                {
                    const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                    std::cout << label << " frame " << frame << ": Cooper at " << at.x << ", " << at.y << ", " << at.z
                              << "\n";
                }
            }
            std::cout << label << " frame " << frame << ": section end gate " << gbtg::g_gate_2518 << "\n";
        }
        if (game.scene_script.name != scene_script)
        {
            moved_on = frame;
            std::cout << label << " frame " << frame << ": the script moves on to " << game.scene_script.name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    std::cout << label << ": pages " << pages << ", title " << title << "\n";
    if (moved_on < 0)
    {
        if (gbtg::g_occupancy_pool != nullptr)
        {
            const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
            for (const auto* record = pool.begin; record < pool.cursor; ++record)
            {
                std::cout << label << " volume mask " << int(record->mask) << " value " << record->value_0 << " at "
                          << record->center.x << ", " << record->center.y << ", " << record->center.z << " extent "
                          << record->extent_x << ", " << record->extent_z << " kind " << record->kind << " yaw "
                          << record->yaw << "\n";
            }
        }
        for (const auto& condition : gbtg::g_script_conds)
        {
            std::cout << label << " end condition opcode " << gbtg::cond_u32(condition, 0) << " words " << std::hex
                      << gbtg::cond_u32(condition, 4) << " " << gbtg::cond_u32(condition, 8) << " "
                      << gbtg::cond_u32(condition, 0xC) << std::dec << "\n";
        }
        for (const gbtg::LiveScript* node = gbtg::g_live_scripts; node != nullptr; node = node->next)
        {
            std::cout << label << " live script type 0x" << std::hex << node->type << " word_c 0x" << node->word_c
                      << std::dec << "\n";
        }
        throw gbtg::Error(label + " did not move on: the script holds at " + game.scene_script.name + " (title "
            + title + ")");
    }
    std::cout << "PASS: " + label + " plays to its end and the script moves on to " << game.scene_script.name << "\n";
}

// Scene 7 (chapter1_scene7_1playcam), from the end of its transition. Pages close on A; titles, the scene's objects,
// Cooper's position and the section gate are logged, and every 300th frame is captured, until the script moves on.
// `fight_range` is how far a live ghouly may be from Cooper to be fought before he walks to a goal; with a
// `range_title`, ghoulies are fought only while the scene title is that one, and never while it is `calm_title`.
// `doors_first` walks to a door a live PlayerApproachesDoor names before any region.
void play_scene7_run(gbtg::Game& game, const std::string& label, float fight_range, const std::string& range_title,
    bool doors_first, bool answer_challenges, const std::string& calm_title, const std::string& powerup_box,
    gbtg::u32 powerup_kind, bool spare_watched, gbtg::u16 flee_class, float weapon_reach, bool invincible,
    bool take_dropped)
{
    // With `powerup_box`, Cooper first goes to the bg-break setup (type 0x24) whose ghoulybox (+0x934) names it and
    // punches until its background part (+0xA50) breaks, then walks onto the powerup (type 0xD) of `powerup_kind`
    // that comes out, before anything else: scene 2c2's invisibility, in a suitcase, gets him past the room unseen.
    // A `powerup_kind` of 0 expects no powerup: the box holds ghoulies (scene 2c12's key holder), fought as usual.
    bool powerup_done = powerup_box.empty();
    // `powerup_kind` kEveryBox breaks every box of `powerup_box`'s name in turn, fighting between them.
    constexpr gbtg::u32 kEveryBox = 0xFFFFFFFFu;
    bool powerup_seen = false; // The box's powerup has come out.
    bool key_found = false; // An armed bg-break key has come out.
    std::string title = game.scene_name;
    const std::string scene_script = game.scene_script.name;
    auto& actors = game.factory_scene->start_actors(game);
    if (game.factory_scene->actor_start < 0)
    {
        // The scene would fall back to the legacy script path, which skips what the native start did not reach.
        throw gbtg::Error(label + ": the native scene start stopped (see the log above)");
    }
    int pages = 0;
    bool page_up = false;
    int page_shown = -1;
    int moved_on = -1;
    bool reached_door = false;
    gbtg::i32 last_door = -1; // The last PlayerApproachesDoor door walked to; the exit is lined up through it.
    StickSteering steering;
    std::set<gbtg::i32> avoided;
    gbtg::Vec3 stuck_at{};
    int stuck_since = -1;
    // A ghouly walked to for a minute without Cooper getting within 16 is left: scene 2b6's vampire chickens stay in
    // their pen behind a fence.
    const gbtg::Actor* chased = nullptr;
    int chased_since = 0;
    std::set<const gbtg::Actor*> unreachable;
    std::set<const gbtg::Actor*> exploding_actors; // Bodies seen in the explode state (type 0x68).
    int explode_capture_until = -1;
    // A ghouly fought up close for 20 s in all without its energy (body +0x94) falling is left too: scenes 2b8 and
    // 2b9's Mr Ribs stands by the exit with an egg and takes no hits. The time is kept per ghouly, so switching
    // between him and another does not reset it.
    std::map<const gbtg::Actor*, std::pair<int, gbtg::u16>> fought_frames;
    // Pinned in one spot for 4 s (an actor or a crate's edge in the way, which the grid's held cells do not
    // cover), Cooper side-steps for 40 frames, alternating sides, then walks on.
    gbtg::Vec3 pinned_at{};
    int pinned_since = -1;
    int side_steps = 0;
    gbtg::u32 last_challenge = 0xFFFFFFFFu;
    gbtg::u16 last_energy = 0; // Cooper's body +0x94, logged as it changes.
    gbtg::u16 invincible_energy = 0; // The most energy Cooper has had, kept with `invincible`.
    gbtg::u32 taken_weapon = 0; // The last dropped weapon Cooper went for with `take_dropped`.
    bool kill_all = false; // Several kill-all steps were queued at once (see below).
    bool one_type_chase = false; // This frame's target class comes from a step or a condition (see below).
    std::optional<gbtg::Vec3> flee_goal; // With `flee_class`, the spot Cooper runs for (see below).
    int flee_chosen = 0;
    int flee_angle = -1; // Which of the eight spots, and where Cooper stood when it was chosen.
    gbtg::Vec3 flee_from{};
    std::array<int, 8> flee_blocked{}; // The frame each direction may be tried again.
    // Scene 9's walk to the mummy's room takes some 7200 frames on its own; scene 2a13's pirate waves, with Cooper's
    // walks to the soups between them, up to some 17500. Scene 2c7 runs twice when the cursed mummy's slab whacks
    // make Cooper faint (about 22000 frames to its last event, which takes some 5500 more). An invincible run (scene
    // 4_1b's Baron, his cane and then his plane's four waves) gets three times as long.
    const int frame_limit = invincible ? 108000 : 36000;
    for (int frame = 0; frame < frame_limit && moved_on < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        // A closes a page once it has been up a second.
        input.accept = page && page_shown >= 0 && frame > page_shown + 60 && frame % 30 < 5;
        // A challenge's restart menu (DAT_004E17C8 6): B goes back to game select.
        input.back = gbtg::g_challenge_run.state == 6 && frame % 60 == 30;
        // The final score's Super Scary Score page (state 5) waits on A at its OK prompt.
        const gbtg::FinalScoreServices* final_score = gbtg::g_scene_control_owner != nullptr
            ? gbtg::g_scene_control_owner->context.final_score : nullptr;
        if (!page && final_score != nullptr && final_score->super_scary != nullptr
            && final_score->super_scary->state == 5 && frame % 30 < 5)
        {
            input.accept = true;
        }
        // The player is DAT_00511B40's actor (link - 0x93C): Cooper (class 4), or chapter 5 scene 13's cook.
        gbtg::u16 player_class = 4;
        if (const gbtg::u32 link = actors.player_link(); link != 0 && link != 0x93C)
        {
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.address == link - 0x93C)
                {
                    player_class = mark.actor->class_key;
                }
            }
        }
        std::optional<gbtg::Vec3> cooper;
        bool holding = false;
        gbtg::u32 held_weapon = 0;
        gbtg::u16 energy = 0;
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor != nullptr && mark.actor->class_key == player_class)
            {
                cooper = gbtg::actor_world_position(*mark.actor);
                // Body +0x48, the held weapon.
                holding = !mark.body.empty() && gbtg::body_record(mark.body).attached != 0;
                held_weapon = mark.body.empty() ? 0u : gbtg::body_record(mark.body).attached;
                energy = mark.body.empty() ? 0 : gbtg::body_record(mark.body).energy; // Body +0x94.
                // A driver cheat, not game behaviour: with `invincible`, Cooper's energy goes back to the most he has
                // had each frame, so a fight the driver cannot win safely still reaches its later phases.
                if (invincible && !mark.body.empty())
                {
                    invincible_energy = std::max(invincible_energy, energy);
                    if (energy != 0 && energy < invincible_energy)
                    {
                        std::memcpy(const_cast<gbtg::u8*>(mark.body.data()) + 0x94, &invincible_energy, 2);
                        energy = invincible_energy;
                    }
                }
            }
        }
        if (cooper && energy != last_energy)
        {
            std::cout << label << " frame " << frame << ": Cooper's energy " << last_energy << " -> " << energy << "\n";
            if (last_energy == 0 && energy != 0 && frame != 0 && !powerup_box.empty())
            {
                // A faint restarts the scene and its boxes: scene 3b4's energy soup is fetched again.
                powerup_done = false;
                powerup_seen = false;
            }
            last_energy = energy;
        }
        // Walk Cooper into the region a live PlayerEntersRegion (condition 8, its +8 word) names; with none, into the
        // exit volume (mask 1) while a section holds at its end (type 0xF, DAT_00512518).
        std::optional<gbtg::Vec3> goal;
        std::string goal_name;
        bool to_exit = false; // The live region is off the navigation grid.
        if (cooper && !page && gbtg::g_occupancy_pool != nullptr)
        {
            const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
            // With `doors_first`, a live PlayerApproachesDoor (condition 6, its +8 word the door) sends Cooper to the
            // door's actor (its clip's first track, 0x0005BEC0) before any region.
            for (const auto& condition : gbtg::g_script_conds)
            {
                if (!doors_first || goal || gbtg::cond_u32(condition, 0) != 6 || gbtg::g_event_sequence_context == nullptr)
                {
                    continue;
                }
                const auto door = gbtg::u16(gbtg::cond_u32(condition, 8));
                gbtg::Vec3 at{};
                float distance = 0.f;
                if (gbtg::runtime_door_distance(door, *cooper, at, distance, gbtg::g_event_sequence_context->doors))
                {
                    goal = at;
                    goal_name = "door " + std::to_string(door);
                    last_door = door;
                }
            }
            for (const auto& condition : gbtg::g_script_conds)
            {
                if (goal || gbtg::cond_u32(condition, 0) != 8)
                {
                    continue;
                }
                // A region may be several records (scene 2c5's dungweed region 1 is five spheres, four of them behind
                // the garden wall): Cooper heads for the nearest.
                const gbtg::u32 region = gbtg::cond_u32(condition, 8);
                float nearest = 0.f;
                for (const auto* record = pool.begin; record < pool.cursor; ++record)
                {
                    if ((record->mask & 1) != 0 || gbtg::u32(record->value_0) != region)
                    {
                        continue;
                    }
                    const float dx = record->center.x - cooper->x;
                    const float dz = record->center.z - cooper->z;
                    if (!goal || dx * dx + dz * dz < nearest)
                    {
                        nearest = dx * dx + dz * dz;
                        goal = record->center;
                        goal_name = "region " + std::to_string(region);
                    }
                }
                // A region off the navigation grid (scene 5_13's, past the front gate's fence): Cooper takes the exit
                // instead, which lies straight ahead of his start there.
                const gbtg::i32 goal_floor = goal ? gbtg::entity_detail::navigation_floor_index(*goal, -1, 0) : -1;
                if (goal && (goal_floor < 0 || gbtg::entity_detail::navigation_cell_index(goal_floor, *goal) < 0))
                {
                    goal.reset();
                    to_exit = true;
                }
            }
            if (!goal && (gbtg::g_gate_2518 != 0 || to_exit))
            {
                for (const auto* record = pool.begin; record < pool.cursor && !goal; ++record)
                {
                    if ((record->mask & 1) != 0)
                    {
                        goal = record->center;
                        goal_name = "the exit";
                    }
                }
                // A "Player Leaves Through Door N" section: the exit volume sits behind the wall and the open door
                // panel swings into the room, so Cooper first lines up 8 units in front of the door's actor (its
                // clip's first track, 0x0005BEC0) on the exit's z, then walks straight through. A section that names
                // no door (scene 2a10's second "Free Play") uses the door Cooper came in by.
                const std::string leaves = "Player Leaves Through Door ";
                const bool named = title.rfind(leaves, 0) == 0;
                if (goal && !reached_door && (named || last_door >= 0) && gbtg::g_event_sequence_context != nullptr)
                {
                    const auto door = gbtg::u16(named ? std::stoi(title.substr(leaves.size())) : last_door);
                    gbtg::Vec3 at{};
                    float distance = 0.f;
                    if (gbtg::runtime_door_distance(door, *cooper, at, distance, gbtg::g_event_sequence_context->doors)
                        && (named || (at.x - goal->x) * (at.x - goal->x) + (at.z - goal->z) * (at.z - goal->z) < 25.f * 25.f))
                    {
                        const gbtg::Vec3 front{at.x + (cooper->x < at.x ? -8.f : 8.f), at.y, goal->z};
                        const float dx = front.x - cooper->x;
                        const float dz = front.z - cooper->z;
                        if (dx * dx + dz * dz > 3.f * 3.f)
                        {
                            goal = front;
                            goal_name = "the front of door " + std::to_string(door);
                        }
                        else
                        {
                            reached_door = true;
                        }
                    }
                }
            }
        }
        // Any live ghouly (a player actor other than Cooper with body +0x94 energy) within the fight range is fought
        // first. The prisoners (tags 0x26..0x30, objTag_Actor_PrisonerChestChap..ThornGirl) and the cook (tag 8,
        // objTag_Actor_Cook) are not ghoulies, nor are scene 2a9's Cameron (tag 6, objTag_Actor_Cameron) and scene 2a11's
        // groundskeeper (tag 0xB, its GROUNDSKEEPER cues), and the ugly
        // girl (tag 0x37, objTag_Actor_UglyGirl) is Cooper's partner from scene 11 on.
        std::optional<gbtg::Vec3> ghouly;
        gbtg::u16 ghouly_class = 0;
        const gbtg::Actor* ghouly_actor = nullptr;
        gbtg::u16 ghouly_energy = 0;
        if (cooper && !page)
        {
            // With `spare_watched`, nothing is fought while a live PlayerHitGhouly (condition 14) watches for a hit:
            // scene 2c5's pirate turns the garden on Cooper when he hits one of the digging skeletons.
            bool watched = false;
            for (const auto& condition : gbtg::g_script_conds)
            {
                watched = watched || (spare_watched && gbtg::cond_u32(condition, 0) == 14);
            }
            const bool fights = (range_title.empty() || title == range_title)
                && (calm_title.empty() || title != calm_title) && !watched;
            const float range = fights ? fight_range : 0.f;
            float nearest = range * range;
            // A queued one-type step (type 0xD, its tag at +0x18) or kill-count step (type 3, its tag at +0x1C) sends
            // Cooper after that class; another ghouly within 10 is fought off first (the step only counts that class, so
            // other kills cost nothing, and a time limit leaves none for the rest).
            gbtg::u16 one_type = 0;
            std::optional<gbtg::u16> kill_type;
            int kill_steps = 0;
            if (const auto* pool = gbtg::g_event_sequence.steps; pool != nullptr)
            {
                for (const gbtg::u8* row = pool->begin; row < pool->end; row += 0x88)
                {
                    if (gbtg::read_u32({row, 0x88}, 0) == 0xD)
                    {
                        one_type = gbtg::read_u16({row, 0x88}, 0x18);
                    }
                    if (gbtg::read_u32({row, 0x88}, 0) == 3)
                    {
                        one_type = gbtg::read_u16({row, 0x88}, 0x1C);
                    }
                    if (gbtg::read_u32({row, 0x88}, 0) == 2)
                    {
                        // Once several kill-all steps (type 2, its tag at +0x18) were queued together, the first one's
                        // class is gone after: scene 2a13's pirate leader, whose pirates keep coming while he stands,
                        // then the pirates. With only ever one (scene 2a12's hunchback) the nearest ghouly is fought.
                        ++kill_steps;
                        if (!kill_type)
                        {
                            kill_type = gbtg::read_u16({row, 0x88}, 0x18);
                        }
                    }
                }
            }
            kill_all = kill_all || kill_steps > 1;
            if (one_type == 0 && kill_all && kill_type)
            {
                one_type = *kill_type;
            }
            if (one_type == 0 && kill_steps == 0)
            {
                // A pending PlayerHitGhouly (opcode 14, its tag at +8) names the ghouly the script waits on: scene
                // 2b6's Mr Ribs by the exit, while the vampire chickens (tag 0x39) sit penned behind a fence.
                for (const auto& condition : gbtg::g_script_conds)
                {
                    // A pending tagged-event wait (opcode 7) names the same way the haunted ghouly to wake: scene 3c8's
                    // haunted door, with the Jessie Clydes forced angry between it and Cooper.
                    if ((gbtg::cond_u32(condition, 0) == 14 || gbtg::cond_u32(condition, 0) == 7)
                        && gbtg::cond_u32(condition, 8) != 0)
                    {
                        one_type = gbtg::u16(gbtg::cond_u32(condition, 8));
                    }
                }
            }
            one_type_chase = one_type != 0;
            for (const auto& mark : actors.marked_actors())
            {
                const bool other_type = one_type != 0 && mark.actor != nullptr && mark.actor->class_key != one_type;
                if (other_type)
                {
                    const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                    const float dx = at.x - cooper->x;
                    const float dz = at.z - cooper->z;
                    if (dx * dx + dz * dz >= 10.f * 10.f)
                    {
                        continue;
                    }
                }
                // Mr Ribs (RibsFNPC, class 0x33), chapter 5's guide, takes no hits: with a region or exit to walk to,
                // he is passed over, so a nearer Ribs no longer hides a ghouly behind him (scene 5_2's Hunchback) and
                // Cooper goes on to the goal when he is the only one (scene 5_13's front gate).
                if (mark.actor == nullptr || unreachable.contains(mark.actor) || mark.actor->class_key == player_class
                    || mark.body.empty()
                    || gbtg::body_record(mark.body).energy == 0
                    || (mark.actor->class_key >= 0x26 && mark.actor->class_key <= 0x30) || mark.actor->class_key == 0x37
                    || mark.actor->class_key == 8 || mark.actor->class_key == 6 || mark.actor->class_key == 0xB
                    || (mark.actor->class_key == 0x33 && goal))
                {
                    continue;
                }
                const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                // Within 15 in height: a hurt zombie climbs onto scene 2a2's plate stacks.
                if (d < nearest && std::abs(at.y - cooper->y) < 15.f)
                {
                    nearest = d;
                    ghouly = at;
                    ghouly_class = mark.actor->class_key;
                    ghouly_actor = mark.actor;
                    ghouly_energy = gbtg::body_record(mark.body).energy;
                }
            }
        }
        // With `take_dropped`, empty-handed Cooper goes for a weapon a ghouly has dropped (its strategy +0x38, no
        // longer in its body +0x48) and taps A beside it to pick it up (0x0007D210's button 0x100 edge): scene 4_1b's
        // Baron drops his cane when knocked down, and only the cane hurts him.
        bool taking_weapon = false;
        if (take_dropped && cooper && !holding && !page)
        {
            const auto marks = actors.marked_actors();
            for (const auto& mark : marks)
            {
                if (mark.actor == nullptr || mark.actor->class_key == player_class || mark.strategy.size() < 0x3C
                    || mark.body.size() < 0x4C)
                {
                    continue;
                }
                const gbtg::u32 dropped = gbtg::strategy_view<gbtg::CarriedWeaponStrategy>(mark.strategy).weapon;
                if (dropped == 0 || gbtg::body_record(mark.body).attached == dropped)
                {
                    continue;
                }
                const auto& addresses = game.factory_scene->start_actors(game).actor_addresses();
                if (const auto weapon = addresses.find(dropped); weapon != addresses.end() && weapon->second != nullptr)
                {
                    ghouly = gbtg::actor_world_position(*weapon->second);
                    ghouly_class = weapon->second->class_key;
                    ghouly_actor = weapon->second;
                    ghouly_energy = 0;
                    taking_weapon = true;
                    taken_weapon = dropped;
                }
            }
        }
        // Below half his health, Cooper leaves the fight for the nearest soup on the floor: an FX anim emitter (type
        // 0xD) holding a powerup (+0x940) past its start (+0x93C 1 or 2) whose 0x00123E70 award is translated: kind 3
        // (more energy), 7 (the ghoulies frozen) or 8, 0xA, 0xB, 0xD (timed powers). Scene 2a13's pirates keep coming
        // in waves that wear him down otherwise. A ghouly within 16 is fought off first: walking away from one leaves
        // Cooper's back to its hits.
        const bool engaged = ghouly
            && (ghouly->x - cooper->x) * (ghouly->x - cooper->x) + (ghouly->z - cooper->z) * (ghouly->z - cooper->z)
                < 16.f * 16.f;
        if (cooper && !page && !engaged)
        {
            int health = -1;
            int full = 0;
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.actor->class_key == player_class && !mark.body.empty())
                {
                    health = gbtg::body_record(mark.body).energy;
                    full = gbtg::body_record(mark.body).max_energy;
                }
            }
            std::optional<gbtg::Vec3> soup;
            float nearest = 0.f;
            for (const auto& object : game.factory_scene->objects)
            {
                if (health < 0 || health * 2 >= full || object.type != 0xD || object.bytes == nullptr
                    || object.size < 0x950)
                {
                    continue;
                }
                const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                const gbtg::u32 state = gbtg::read_u32(bytes, 0x93C);
                const gbtg::Actor* actor = actor_of_object(game, object);
                const gbtg::u32 kind = gbtg::read_u32(bytes, 0x940);
                if (actor == nullptr || (state != 1 && state != 2)
                    || (kind != 3 && kind != 7 && kind != 8 && kind != 0xA && kind != 0xB && kind != 0xD))
                {
                    continue;
                }
                const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                if (std::abs(at.y - cooper->y) < 5.f && (!soup || d < nearest))
                {
                    soup = at;
                    nearest = d;
                }
            }
            if (soup)
            {
                ghouly.reset();
                goal = soup;
                goal_name = "a soup";
            }
        }
        bool punch_box = false;
        std::optional<gbtg::Vec3> box_at;
        if (cooper && !page && !powerup_done)
        {
            std::optional<gbtg::Vec3> powerup;
            bool powerup_out = false;
            bool box_waiting = false;
            bool box_broken = false; // A box of that name has broken (+0xA50).
            for (const auto& object : game.factory_scene->objects)
            {
                if (object.bytes == nullptr)
                {
                    continue;
                }
                const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                const gbtg::Actor* actor = actor_of_object(game, object);
                if (actor == nullptr)
                {
                    continue;
                }
                if (object.type == 0x24 && object.size >= 0xA7C
                    && std::string_view(reinterpret_cast<const char*>(bytes.data() + 0x934)) == powerup_box
                    && gbtg::read_u32(bytes, 0xA50) != 0)
                {
                    box_broken = true;
                }
                if (object.type == 0x24 && object.size >= 0xA7C
                    && std::string_view(reinterpret_cast<const char*>(bytes.data() + 0x934)) == powerup_box
                    && gbtg::read_u32(bytes, 0xA50) == 0)
                {
                    box_waiting = true;
                    // With several boxes of that name, the nearest to Cooper.
                    const gbtg::Vec3 at = gbtg::actor_world_position(*actor);
                    const auto reach = [&](const gbtg::Vec3& point)
                    {
                        const float dx = point.x - cooper->x;
                        const float dz = point.z - cooper->z;
                        return dx * dx + dz * dz;
                    };
                    if (!box_at || reach(at) < reach(*box_at))
                    {
                        box_at = at;
                    }
                }
                if (object.type == 0xD && object.size >= 0x950 && gbtg::read_u32(bytes, 0x940) == powerup_kind
                    && gbtg::read_u32(bytes, 0x93C) <= 1)
                {
                    // Still in flight (+0x93C 0) it is waited for; landed (1) it is walked onto. At 2 it has been
                    // collected (0x0010A6D0's award).
                    powerup_out = true;
                    powerup_seen = true;
                    if (gbtg::read_u32(bytes, 0x93C) == 1)
                    {
                        powerup = gbtg::actor_world_position(*actor);
                    }
                }
            }
            if (powerup_kind != 0 && powerup_out)
            {
                // A broken box's powerup comes before another box of the same name: walked onto once it lands.
                ghouly.reset();
                if (powerup)
                {
                    goal = powerup;
                    goal_name = "the powerup";
                }
            }
            else if (powerup_kind == kEveryBox)
            {
                // Every box of that name, one at a time, with what comes out of each fought first: scene 5_8's
                // skeleton washing machines, among zombie ones, for its "kill three skeletons" event. They wait until
                // an event's steps run (DAT_005064E0 holds some), so the walk into the event's region comes first.
                const auto* steps = gbtg::g_event_sequence.steps;
                const bool event_running = steps != nullptr && steps->end != steps->begin;
                // Mr Ribs (RibsFNPC, class 0x33), who leads Cooper through chapter 5 and takes no hits, does not hold
                // the errand up.
                if (event_running && box_waiting && (!ghouly || ghouly_class == 0x33))
                {
                    ghouly.reset();
                    gbtg::Vec3 approach = *box_at;
                    using namespace gbtg::entity_detail;
                    const gbtg::i32 floor = navigation_floor_index(*box_at, -1, 0);
                    const gbtg::i32 cell = floor < 0 ? -1 : navigation_cell_index(floor, *box_at);
                    if (cell >= 0)
                    {
                        navigation_nearest_open_cell(gbtg::u32(cell), floor, &approach);
                    }
                    goal = approach;
                    goal_name = "the " + powerup_box + " setup";
                    const float dx = box_at->x - cooper->x;
                    const float dz = box_at->z - cooper->z;
                    // The washing machines need Cooper close by: from 19 away his kicks do not reach them.
                    punch_box = dx * dx + dz * dz < 16.f * 16.f;
                    if (punch_box)
                    {
                        goal.reset();
                    }
                }
                else if (event_running && !box_waiting)
                {
                    std::cout << label << " frame " << frame << ": every " << powerup_box << " setup is broken\n";
                    powerup_done = true;
                }
            }
            else if (powerup_kind == 0 && box_broken)
            {
                // A box broken for the way past it (scene 3c10's soup crate in the attic's gap) is the errand, not
                // the other boxes of its name.
                std::cout << label << " frame " << frame << ": the " << powerup_box << " setup is broken\n";
                powerup_done = true;
            }
            else if (powerup_kind != 0 && powerup_seen)
            {
                // One soup is the errand: scene 3c10's attic has three energyboost1 boxes, the others across the room
                // from the exit.
                std::cout << label << " frame " << frame << ": the " << powerup_box << " powerup is taken\n";
                powerup_done = true;
            }
            else if (box_waiting)
            {
                ghouly.reset();
                // The suitcase stands on furniture: Cooper walks to the nearest open floor cell beside it.
                gbtg::Vec3 approach = *box_at;
                using namespace gbtg::entity_detail;
                const gbtg::i32 floor = navigation_floor_index(*box_at, -1, 0);
                const gbtg::i32 cell = floor < 0 ? -1 : navigation_cell_index(floor, *box_at);
                if (cell >= 0)
                {
                    navigation_nearest_open_cell(gbtg::u32(cell), floor, &approach);
                }
                goal = approach;
                goal_name = "the " + powerup_box + " setup";
                const float dx = box_at->x - cooper->x;
                const float dz = box_at->z - cooper->z;
                // A box on a rack (scene 2c12's barrels) is some 21 from the nearest open cell: from within 24 of it
                // Cooper stands where he is and punches, as walking on toward the cell spoils the punches.
                punch_box = dx * dx + dz * dz < 16.f * 16.f;
                if (!punch_box && dx * dx + dz * dz < 24.f * 24.f)
                {
                    punch_box = true;
                    goal.reset();
                }
            }
            else if (powerup_kind == 0)
            {
                // A box that holds ghoulies (scene 2c12's key holder): once it breaks, Cooper fights what comes out.
                std::cout << label << " frame " << frame << ": the " << powerup_box << " setup is broken\n";
                powerup_done = true;
            }
            else if (powerup)
            {
                ghouly.reset();
                goal = powerup;
                goal_name = "the powerup";
                // One against a wall (scene 3c9's speed-up soup) sits in a closed cell the walk cannot end in: from
                // over 30 away Cooper walks to the nearest open cell, then straight on.
                const float dx = powerup->x - cooper->x;
                const float dz = powerup->z - cooper->z;
                using namespace gbtg::entity_detail;
                const gbtg::i32 floor = navigation_floor_index(*powerup, -1, 0);
                const gbtg::i32 cell = floor < 0 ? -1 : navigation_cell_index(floor, *powerup);
                if (dx * dx + dz * dz > 30.f * 30.f && cell >= 0)
                {
                    gbtg::Vec3 approach = *powerup;
                    navigation_nearest_open_cell(gbtg::u32(cell), floor, &approach);
                    goal = approach;
                }
            }
            else if (powerup_out || !powerup_seen)
            {
                // In flight, or the broken box has not spawned it yet.
                ghouly.reset();
            }
            else
            {
                std::cout << label << " frame " << frame << ": the " << powerup_box << " powerup is taken\n";
                powerup_done = true;
            }
        }
        // An event's start arms the bg-break key (type 0x1E, +0x9BC) it asks for: until one is out (+0x9C0), Cooper
        // walks beside the background part that hides it and punches, as for the powerup box.
        if (cooper && !page && !box_at && !key_found)
        {
            std::optional<gbtg::Vec3> key_at;
            for (const auto& object : game.factory_scene->objects)
            {
                if (object.type != 0x1E || object.bytes == nullptr)
                {
                    continue;
                }
                const std::span<const gbtg::u8> bytes{object.bytes.get(), object.size};
                if (gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).active != 0)
                {
                    std::cout << label << " frame " << frame << ": the key comes out\n";
                    key_found = true;
                    key_at.reset();
                    break;
                }
                const gbtg::Actor* actor = actor_of_object(game, object);
                if (actor != nullptr && gbtg::actor_record_view<gbtg::BgBreakKeyState>(bytes).armed != 0)
                {
                    key_at = gbtg::actor_world_position(*actor);
                }
            }
            if (key_at)
            {
                ghouly.reset();
                gbtg::Vec3 approach = *key_at;
                using namespace gbtg::entity_detail;
                const gbtg::i32 floor = navigation_floor_index(*key_at, -1, 0);
                const gbtg::i32 cell = floor < 0 ? -1 : navigation_cell_index(floor, *key_at);
                if (cell >= 0)
                {
                    navigation_nearest_open_cell(gbtg::u32(cell), floor, &approach);
                }
                goal = approach;
                goal_name = "the key's part";
                box_at = key_at;
                const float dx = key_at->x - cooper->x;
                const float dz = key_at->z - cooper->z;
                // Scene 3b6's key sits up behind a barrel, 19 from the nearest open cell: once Cooper stands on that
                // cell he punches at it from within 24.
                const float ax = approach.x - cooper->x;
                const float az = approach.z - cooper->z;
                punch_box = dx * dx + dz * dz < 16.f * 16.f
                    || (dx * dx + dz * dz < 24.f * 24.f && ax * ax + az * az < 6.f * 6.f);
            }
        }
        // A ghouly within 16 is attacked with the right stick; a farther one is walked to like a goal, so the walk's
        // held-cell avoidance applies to it too. Holding a weapon, `weapon_reach` replaces the 16: scene 3c11's popcan
        // gun, with Cooper on one energy among the hallway's skeletons.
        // A weapon Cooper took from a ghouly (scene 4_1b's cane) is swung from close by; the reach is for weapons the
        // scene hands him (4_1b's popcan gun against the plane).
        const float reach = holding && held_weapon != taken_weapon ? weapon_reach : 16.f;
        const bool close = ghouly
            && (ghouly->x - cooper->x) * (ghouly->x - cooper->x) + (ghouly->z - cooper->z) * (ghouly->z - cooper->z)
                < reach * reach;
        if (ghouly && !close && ghouly_actor != nullptr)
        {
            if (ghouly_actor != chased)
            {
                chased = ghouly_actor;
                chased_since = frame;
            }
            // A chase of one class gives up on a ghouly after ten seconds instead of a minute: scene 3c9's haunted coats,
            // one behind a laundry cart, under the event's clock.
            else if (frame > chased_since + (one_type_chase ? 600 : 3600))
            {
                std::cout << label << " frame " << frame << ": Cooper leaves the ghouly (class 0x" << std::hex
                          << ghouly_class << std::dec << ") at " << ghouly->x << ", " << ghouly->z
                          << ", out of reach for " << (one_type_chase ? "ten seconds" : "a minute") << "\n";
                unreachable.insert(ghouly_actor);
                chased = nullptr;
            }
        }
        if (close)
        {
            chased = nullptr;
            auto& [frames, energy] = fought_frames[ghouly_actor];
            if (frames == 0 || ghouly_energy != energy)
            {
                frames = 1;
                energy = ghouly_energy;
            }
            else if (++frames > 1200)
            {
                std::cout << label << " frame " << frame << ": Cooper leaves the ghouly (class 0x" << std::hex
                          << ghouly_class << std::dec << ") at " << ghouly->x << ", " << ghouly->z
                          << ", which takes no hits\n";
                unreachable.insert(ghouly_actor);
                fought_frames.erase(ghouly_actor);
            }
        }
        if (ghouly && !close)
        {
            goal = ghouly;
            std::ostringstream name;
            name << "a ghouly (class 0x" << std::hex << ghouly_class << ")";
            goal_name = name.str();
        }
        if (!ghouly && cooper && !page && powerup_done)
        {
            // A pending tagged-event wait (opcode 7, its tag at +8) names the haunted ghouly whose wake (event 0xB4)
            // the script needs: scene 3c8's haunted door, asleep across the dormitory, wakes as Cooper comes near.
            for (const auto& condition : gbtg::g_script_conds)
            {
                if (gbtg::cond_u32(condition, 0) != 7 || gbtg::cond_u32(condition, 8) == 0)
                {
                    continue;
                }
                for (const auto& mark : actors.marked_actors())
                {
                    if (mark.actor != nullptr && mark.actor->class_key == gbtg::cond_u32(condition, 8))
                    {
                        goal = gbtg::actor_world_position(*mark.actor);
                        std::ostringstream name;
                        name << "the sleeping ghouly (class 0x" << std::hex << mark.actor->class_key << ")";
                        goal_name = name.str();
                    }
                }
            }
        }
        if (flee_class != 0 && cooper && !page && !calm_title.empty() && title == calm_title)
        {
            // With `flee_class`, Cooper keeps away from that ghouly while the calm title shows: scene 3c9's Grim
            // Reaper, kited round the bathroom until the event's clocks run out and the exit opens. Every half second
            // the walkable spot 80 out (of eight around him) farthest from the nearest one is chosen.
            std::optional<gbtg::Vec3> hunter;
            float hunter_d = 1.0e30f;
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor == nullptr || mark.actor->class_key != flee_class)
                {
                    continue;
                }
                const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                const float d = (at.x - cooper->x) * (at.x - cooper->x) + (at.z - cooper->z) * (at.z - cooper->z);
                if (d < hunter_d)
                {
                    hunter_d = d;
                    hunter = at;
                }
            }
            if (!hunter)
            {
                flee_goal.reset();
            }
            else
            {
                if (!flee_goal || frame >= flee_chosen + 30)
                {
                    // A direction he gained under 10 in is left out for the next two seconds: the grid opens onto
                    // walls and fittings he only slides along.
                    if (flee_goal && flee_angle >= 0
                        && (cooper->x - flee_from.x) * (cooper->x - flee_from.x)
                                + (cooper->z - flee_from.z) * (cooper->z - flee_from.z)
                            < 10.f * 10.f)
                    {
                        flee_blocked[flee_angle] = frame + 120;
                    }
                    flee_goal.reset();
                    flee_angle = -1;
                    flee_from = *cooper;
                    float best = -1.f;
                    for (int i = 0; i < 8; ++i)
                    {
                        const float angle = float(i) * 0.785398f;
                        const gbtg::Vec3 spot{cooper->x + 80.f * std::sin(angle), cooper->y,
                            cooper->z + 80.f * std::cos(angle)};
                        if (frame < flee_blocked[i] || !nav_waypoint(*cooper, spot, 3, &avoided, true))
                        {
                            continue;
                        }
                        const float d = (spot.x - hunter->x) * (spot.x - hunter->x)
                            + (spot.z - hunter->z) * (spot.z - hunter->z);
                        if (d > best)
                        {
                            best = d;
                            flee_goal = spot;
                            flee_angle = i;
                        }
                    }
                    flee_chosen = frame;
                }
                if (flee_goal)
                {
                    ghouly.reset();
                    goal = flee_goal;
                    std::ostringstream name;
                    name << "away from a ghouly (class 0x" << std::hex << flee_class << std::dec << ", at " << hunter->x
                         << ", " << hunter->z << ")";
                    goal_name = name.str();
                }
            }
        }
        if (punch_box && box_at && (frame / 8) % 2 == 0)
        {
            // The suitcase's part is punched open, the right stick tapped toward it.
            const float stick = steering.angle(*cooper, *box_at);
            input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
            input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
        }
        if (ghouly && close)
        {
            // Bare-handed, the right stick is tapped for punches; a held weapon (the extinguisher's spray) is aimed
            // with the stick held.
            const float stick = steering.angle(*cooper, *ghouly);
            if (taking_weapon)
            {
                if (frame % 10 < 3)
                {
                    input.pads[0].buttons = gbtg::u16(input.pads[0].buttons | 0x100);
                }
            }
            else if (holding || (frame / 8) % 2 == 0)
            {
                input.pads[0].rx = gbtg::i16(std::sin(stick) * 32767.f);
                input.pads[0].ry = gbtg::i16(-std::cos(stick) * 32767.f);
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->z
                          << " fights a ghouly (class 0x" << std::hex << ghouly_class << std::dec << ") at "
                          << ghouly->x << ", " << ghouly->z << "\n";
            }
        }
        else if (goal)
        {
            // Follow the navigation grid's walk where it has one; otherwise head straight for the goal. Held in
            // place for a second and a half (a coffin or crate the grid leaves open), the walk's next cell is
            // avoided from then on and the walk is searched again.
            const float moved_x = cooper->x - stuck_at.x;
            const float moved_z = cooper->z - stuck_at.z;
            if (stuck_since < 0 || moved_x * moved_x + moved_z * moved_z > 6.f * 6.f)
            {
                stuck_at = *cooper;
                stuck_since = frame;
            }
            else if (frame > stuck_since + 90)
            {
                using namespace gbtg::entity_detail;
                if (const auto next = nav_waypoint(*cooper, *goal, 1, &avoided, true); next && g_nav_count > 0)
                {
                    const gbtg::i32 floor = navigation_floor_index(*cooper, -1, 0);
                    const gbtg::i32 cell = navigation_cell_index(floor, *next);
                    if (cell >= 0)
                    {
                        avoided.insert(cell);
                        std::cout << label << " frame " << frame << ": Cooper held at " << cooper->x << ", "
                                  << cooper->z << "; the walk avoids the cell at " << next->x << ", " << next->z
                                  << "\n";
                    }
                }
                stuck_since = frame;
            }
            std::optional<gbtg::Vec3> step = nav_waypoint(*cooper, *goal, 3, &avoided, true);
            if (!step && !avoided.empty() && nav_waypoint(*cooper, *goal, 3, nullptr, true))
            {
                // The cells avoided so far (in a long fight, around many held spots) leave no walk at all: they are
                // forgotten and the walk searched again.
                std::cout << label << " frame " << frame << ": the walk forgets " << avoided.size()
                          << " avoided cells\n";
                avoided.clear();
                step = nav_waypoint(*cooper, *goal, 3, &avoided, true);
            }
            const gbtg::Vec3 toward = step ? *step : *goal;
            const float dx = toward.x - cooper->x;
            const float dz = toward.z - cooper->z;
            const float px = cooper->x - pinned_at.x;
            const float pz = cooper->z - pinned_at.z;
            if (pinned_since < 0 || px * px + pz * pz > 6.f * 6.f)
            {
                pinned_at = *cooper;
                pinned_since = frame;
            }
            if (frame > pinned_since + 240 && frame <= pinned_since + 280)
            {
                const float side = side_steps % 2 == 0 ? 1.5707964f : -1.5707964f;
                steering.walk(input, steering.angle(*cooper, toward) + side);
                if (frame == pinned_since + 280)
                {
                    ++side_steps;
                    pinned_since = frame;
                }
            }
            else if (dx * dx + dz * dz > 4.f)
            {
                steering.walk(input, steering.angle(*cooper, toward));
            }
            if (frame % 120 == 0)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->y << ", "
                          << cooper->z << " (floor " << gbtg::entity_detail::navigation_floor_index(*cooper, -1, 0)
                          << " of " << gbtg::entity_detail::g_nav_count << ") heads for " << goal_name << " at "
                          << goal->x << ", " << goal->z << " (floor "
                          << gbtg::entity_detail::navigation_floor_index(*goal, -1, 0) << ")"
                          << (step ? " by the grid" : " straight") << "\n";
            }
        }
        // With `answer_challenges`, a Super Scary challenge (factory type 0x98) waiting on Cooper (states 5 and 6)
        // gets the next button in its row, as A..Y (0x100 << button).
        if (const auto* challenge = gbtg::find_runtime_scene_control(0x98); answer_challenges && challenge != nullptr)
        {
            const gbtg::u32 phase = gbtg::u32(challenge->super_scary->phase);
            if ((phase == 5 || phase == 6) && frame % 8 < 3)
            {
                const gbtg::i32 presses = challenge->super_scary->presses;
                const gbtg::u32 button = challenge->super_scary->buttons[size_t(presses)];
                input.pads[0].buttons = gbtg::u16(0x100u << button);
            }
        }
        // An imp riding Cooper's head (body +0x718, the jump on/off state type 0x79) is shaken off by swinging the left
        // stick from side to side; its 0x00109810 tracking reads the stick's change each frame.
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor != nullptr && mark.actor->class_key == player_class && !mark.body.empty()
                && gbtg::body_record(mark.body).rider != 0)
            {
                input.pads[0].lx = frame % 2 == 0 ? gbtg::i16(32767) : gbtg::i16(-32767);
                input.pads[0].ly = 0;
            }
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        {
            const auto* challenge = gbtg::find_runtime_scene_control(0x98);
            const gbtg::u32 now =
                challenge == nullptr ? 0xFFFFFFFFu : gbtg::u32(challenge->super_scary->phase);
            if (now != last_challenge)
            {
                std::cout << label << " frame " << frame << ": Super Scary state " << int(now) << "\n";
                last_challenge = now;
            }
        }
        const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        if (page_now && !page_up)
        {
            ++pages;
            page_shown = frame;
            std::cout << label << " frame " << frame << ": page " << pages << "\n";
        }
        if (page_now && frame == page_shown + 60)
        {
            capture_readback("cache/" + label + "_page" + std::to_string(pages) + ".bmp");
        }
        page_up = page_now;
        if (game.scene_name != title)
        {
            title = game.scene_name;
            std::cout << label << " frame " << frame << ": title " << title << "\n";
            log_scene_objects(game, label, frame);
        }
        // A body entering the explode state (type 0x68, scene 3b2's worm) is logged, and its blast captured every
        // 10 frames for a second.
        for (const auto& mark : actors.marked_actors())
        {
            if (mark.actor == nullptr || mark.body.empty())
            {
                continue;
            }
            const auto [first_action, second_action] = actors.player_action_types(*mark.actor);
            if ((first_action == 0x68 || second_action == 0x68) && !exploding_actors.contains(mark.actor))
            {
                const gbtg::Vec3 at = gbtg::actor_world_position(*mark.actor);
                std::cout << label << " frame " << frame << ": a ghouly (class 0x" << std::hex << mark.actor->class_key
                          << std::dec << ") at " << at.x << ", " << at.z << " explodes\n";
                exploding_actors.insert(mark.actor);
                explode_capture_until = frame + 60;
            }
        }
        if (frame <= explode_capture_until && frame % 10 == 0)
        {
            capture_readback("cache/" + label + "_explode_" + std::to_string(frame) + ".bmp");
        }
        if (frame % 300 == 0)
        {
            capture_readback("cache/" + label + "_" + std::to_string(frame) + ".bmp");
            if (frame == 0)
            {
                log_scene_objects(game, label, frame);
                if (gbtg::g_occupancy_pool != nullptr)
                {
                    // The occupancy records (DAT_00512344): the regions PlayerEntersRegion names and the exit volume.
                    const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
                    for (const auto* record = pool.begin; record < pool.cursor; ++record)
                    {
                        std::cout << label << " frame 0: region " << record->value_0 << " mask " << record->mask
                                  << " kind " << record->kind << " at " << record->center.x << ", "
                                  << record->center.y << ", " << record->center.z << " extent " << record->extent_x
                                  << " x " << record->extent_z << " yaw " << record->yaw << "\n";
                    }
                }
            }
            if (cooper)
            {
                std::cout << label << " frame " << frame << ": Cooper at " << cooper->x << ", " << cooper->y << ", "
                          << cooper->z << " energy " << energy << "\n";
            }
            std::cout << label << " frame " << frame << ": section end gate " << gbtg::g_gate_2518 << "\n";
        }
        if (game.scene_script.name != scene_script)
        {
            moved_on = frame;
            std::cout << label << " frame " << frame << ": the script moves on to " << game.scene_script.name << "\n";
        }
        gbtg::gfx_end_frame();
    }
    std::cout << label << ": pages " << pages << ", title " << title << "\n";
    if (moved_on < 0)
    {
        if (gbtg::g_occupancy_pool != nullptr)
        {
            const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
            for (const auto* record = pool.begin; record < pool.cursor; ++record)
            {
                std::cout << label << " volume mask " << int(record->mask) << " value " << record->value_0 << " at "
                          << record->center.x << ", " << record->center.y << ", " << record->center.z << " extent "
                          << record->extent_x << ", " << record->extent_z << " kind " << record->kind << " yaw "
                          << record->yaw << "\n";
            }
        }
        for (const auto& condition : gbtg::g_script_conds)
        {
            std::cout << label << " end condition opcode " << gbtg::cond_u32(condition, 0) << " words " << std::hex
                      << gbtg::cond_u32(condition, 4) << " " << gbtg::cond_u32(condition, 8) << " "
                      << gbtg::cond_u32(condition, 0xC) << std::dec << "\n";
        }
        throw gbtg::Error(label + " did not move on: the script holds at " + game.scene_script.name + " (title "
            + title + ")");
    }
    std::cout << "PASS: " + label + " plays to its end and the script moves on to " << game.scene_script.name << "\n";
}

void play_scene7(gbtg::Game& game, const std::string& label, float fight_range = 50.f,
    const std::string& range_title = {}, bool doors_first = false, bool answer_challenges = false,
    const std::string& calm_title = {}, const std::string& powerup_box = {}, gbtg::u32 powerup_kind = 0,
    bool spare_watched = false, gbtg::u16 flee_class = 0, float weapon_reach = 16.f, bool invincible = false,
    bool take_dropped = false)
{
    try
    {
        play_scene7_run(game, label, fight_range, range_title, doors_first, answer_challenges, calm_title, powerup_box,
            powerup_kind, spare_watched, flee_class, weapon_reach, invincible, take_dropped);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

void play_scene6_1(gbtg::Game& game, const std::string& label)
{
    try
    {
        play_scene6_1_run(game, label);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

void play_scene5(gbtg::Game& game, const std::string& label)
{
    try
    {
        play_scene5_run(game, label);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

void play_scene4(gbtg::Game& game, const std::string& label, bool drive = false)
{
    try
    {
        play_scene4_run(game, label, drive);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

// A stop inside the scene is reported before the game is torn down: the teardown of a scene stopped mid-frame can
// raise again (its actors' release messages reach views that are not translated yet), which would terminate the run.
// --pause: scene 3 from its chapter title. Once Cooper is in play, Start asks for scene state 6 (0x0011E7F0); the
// pause screen (0x0011CE10) opens its book with "Paused", the chapter line and the prompts, and holds open (phase 3).
// Start again closes it (0x0011DEE0); once the book has finished, the scene state returns to 1 and the scene runs on.
// A second pause takes the prompt buttons: X (pause_Quit, 0x0011DFA0) shows the quit confirmation, B (pause_No,
// 0x0011E1A0) brings the game's prompts back and B (pause_Return, 0x0011DEE0) closes the book.
void play_pause_run(gbtg::Game& game)
{
    auto& actors = game.factory_scene->start_actors(game);
    const auto scene_state = []()
    {
        return gbtg::read_u32({gbtg::g_scene_record, 0x108}, 0x104); // DAT_00510F5C
    };
    int cooper_frame = -1;
    int free_frame = -1;
    // Per pause: when Start asked for it, when the book held open, when it began closing and when the scene ran on.
    struct Pause
    {
        int paused = -1;
        int open = -1;
        int closing = -1;
        int resumed = -1;
        gbtg::i32 entries = -1; // The input stack's depth (DAT_0050E0EC) before the pause pushed its entry.
    };
    std::array<Pause, 2> pauses{};
    size_t current = 0;
    int confirm_frame = -1;
    int cancel_frame = -1;
    gbtg::u32 last_state = scene_state();
    gbtg::i32 last_phase = gbtg::g_scene_pause.phase;
    gbtg::i32 last_prompt = gbtg::g_scene_pause.prompt;
    for (int frame = 0; frame < 9000 && (pauses[1].resumed < 0 || frame < pauses[1].resumed + 120); ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5 && pauses[0].paused < 0;
        if (cooper_frame < 0)
        {
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.actor->class_key == 4)
                {
                    cooper_frame = frame;
                    std::cout << "pause frame " << frame << ": Cooper is in play\n";
                }
            }
        }
        // The walk-in holds DAT_00510CEC until Free Play, and the scene tick ignores Start while it is held.
        if (cooper_frame >= 0 && free_frame < 0 && gbtg::g_pause_count == 0)
        {
            free_frame = frame;
            std::cout << "pause frame " << frame << ": nothing holds the scene (DAT_00510CEC 0)\n";
        }
        // Each press lasts 3 frames.
        const auto pressed = [frame](int at)
        {
            return at >= 0 && frame >= at && frame < at + 3;
        };
        Pause& now = pauses[current];
        // The first pause: Start, then Start again 120 frames into the open book.
        // The second pause, 120 frames after the first resumes: X (pause_Quit) 60 frames in, B (pause_No) 120 frames
        // after the confirmation, then B (pause_Return) 120 frames after the game's prompts are back (the prompt
        // widget's swap, out and in again, takes about 66 frames).
        const int start_at = current == 0 ? (free_frame >= 0 ? free_frame + 60 : -1)
            : (pauses[0].resumed >= 0 ? pauses[0].resumed + 120 : -1);
        if (now.paused < 0 && !page && pressed(start_at))
        {
            input.start = true;
        }
        if (current == 0 && now.open >= 0 && now.closing < 0 && pressed(now.open + 120))
        {
            input.start = true;
        }
        if (current == 1 && now.open >= 0 && confirm_frame < 0 && pressed(now.open + 60))
        {
            input.page = true;
        }
        if (current == 1 && confirm_frame >= 0 && cancel_frame < 0 && pressed(confirm_frame + 120))
        {
            input.back = true;
        }
        if (current == 1 && cancel_frame >= 0 && now.closing < 0 && pressed(cancel_frame + 120))
        {
            input.back = true;
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const gbtg::u32 state = scene_state();
        const gbtg::i32 phase = gbtg::g_scene_pause.phase;
        const gbtg::i32 prompt = gbtg::g_scene_pause.prompt;
        if (state != last_state)
        {
            std::cout << "pause frame " << frame << ": scene state " << state << "\n";
            if (state == 6 && now.paused < 0)
            {
                now.paused = frame;
                now.entries = game.native_input->selected - 1;
            }
            if (state == 1 && now.closing >= 0 && now.resumed < 0)
            {
                now.resumed = frame;
                if (game.native_input->selected != now.entries)
                {
                    throw gbtg::Error("the pause left " + std::to_string(game.native_input->selected)
                        + " input entries, not " + std::to_string(now.entries));
                }
            }
            last_state = state;
        }
        if (phase != last_phase)
        {
            std::cout << "pause frame " << frame << ": pause phase " << phase << "\n";
            if (phase == 3 && now.open < 0)
            {
                now.open = frame;
            }
            if (phase == 6 && now.closing < 0)
            {
                now.closing = frame;
            }
            last_phase = phase;
        }
        if (prompt != last_prompt)
        {
            std::cout << "pause frame " << frame << ": prompt row " << prompt << "\n";
            if (prompt == 9 && confirm_frame < 0)
            {
                confirm_frame = frame;
            }
            if (confirm_frame >= 0 && prompt != 9 && cancel_frame < 0)
            {
                cancel_frame = frame;
            }
            last_prompt = prompt;
        }
        const std::string label = "cache/pause" + std::to_string(current + 1) + "_";
        if (now.paused >= 0 && frame == now.paused + 10)
        {
            capture_readback(label + "opening.bmp");
        }
        if (now.open >= 0 && frame == now.open + 40)
        {
            capture_readback(label + "open.bmp");
        }
        if (current == 1 && confirm_frame >= 0 && frame == confirm_frame + 40)
        {
            capture_readback(label + "confirm_quit.bmp");
        }
        if (current == 1 && cancel_frame >= 0 && frame == cancel_frame + 40)
        {
            capture_readback(label + "cancelled.bmp");
        }
        if (now.closing >= 0 && frame == now.closing + 10)
        {
            capture_readback(label + "closing.bmp");
        }
        if (now.resumed >= 0 && frame == now.resumed + 60)
        {
            capture_readback(label + "resumed.bmp");
        }
        if (current == 0 && now.resumed >= 0)
        {
            current = 1;
        }
        gbtg::gfx_end_frame();
    }
    if (cooper_frame < 0)
    {
        throw gbtg::Error("Cooper never came into play");
    }
    for (size_t index = 0; index < pauses.size(); ++index)
    {
        const Pause& pause = pauses[index];
        const std::string which = index == 0 ? "the first pause" : "the second pause";
        if (pause.paused < 0)
        {
            throw gbtg::Error("Start did not pause the scene (scene state 6) for " + which);
        }
        if (pause.open < 0)
        {
            throw gbtg::Error("the book never held open (phase 3) in " + which);
        }
        if (index == 1 && confirm_frame < 0)
        {
            throw gbtg::Error("X (pause_Quit) did not show the quit confirmation (prompt row 9)");
        }
        if (index == 1 && cancel_frame < 0)
        {
            throw gbtg::Error("B (pause_No) did not return to the game's prompts");
        }
        if (pause.closing < 0)
        {
            throw gbtg::Error(std::string(index == 0 ? "Start" : "B (pause_Return)") + " did not close " + which);
        }
        if (pause.resumed < 0)
        {
            throw gbtg::Error("the scene did not run on after " + which + " (scene state 1)");
        }
    }
    std::cout << "PASS: Start pauses scene 3 and closes the pause (frames " << pauses[0].paused << " .. "
              << pauses[0].resumed << "); a second pause shows the quit confirmation on X, returns on B and closes on B "
              << "(frames " << pauses[1].paused << " .. " << pauses[1].resumed << ")\n";
}

// --pause-rules: chapter 5 scene 2's Hunchback event (it runs on: Cooper never reaches the Hunchback). Once its steps
// run (DAT_005064D8 3), Start pauses, Y (pause_EventRules, 0x0011E150) shows the event's reminder pages (0x000D0780,
// phase 5), A closes them and the book holds open again (phase 3); B (pause_Return) closes the pause.
void play_pause_rules_run(gbtg::Game& game)
{
    game.factory_scene->start_actors(game);
    int event_frame = -1;
    int open_frame = -1;
    int rules_frame = -1;
    int reopened_frame = -1;
    int resumed_frame = -1;
    gbtg::u32 queued = 0;
    for (int frame = 0; frame < 9000 && resumed_frame < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        const auto pressed = [frame](int at)
        {
            return at >= 0 && frame >= at && frame < at + 3;
        };
        // Pages close on A, before the pause and while the rules show.
        input.accept = page && frame % 30 < 5 && (open_frame < 0 || rules_frame >= 0);
        if (event_frame >= 0 && open_frame < 0 && !page && pressed(event_frame + 60))
        {
            input.start = true;
        }
        if (open_frame >= 0 && rules_frame < 0 && pressed(open_frame + 60))
        {
            input.pads[0].buttons |= 0x0800; // Y
        }
        if (reopened_frame >= 0 && pressed(reopened_frame + 60))
        {
            input.back = true;
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        if (event_frame < 0 && gbtg::g_event_sequence_state == 3)
        {
            event_frame = frame;
            std::cout << "pause-rules frame " << frame << ": the event's steps run\n";
        }
        if (event_frame >= 0 && open_frame < 0 && gbtg::g_scene_pause.phase == 3)
        {
            open_frame = frame;
            std::cout << "pause-rules frame " << frame << ": the pause book is open, prompt row "
                      << gbtg::g_scene_pause.prompt << "\n";
        }
        if (open_frame >= 0 && rules_frame < 0 && gbtg::g_scene_pause.phase == 5)
        {
            rules_frame = frame;
            queued = game.script_dialogue != nullptr ? game.script_dialogue->dialogue.state.count : 0;
            std::cout << "pause-rules frame " << frame << ": Event Rules (phase 5), " << queued
                      << " dialogue pages queued\n";
        }
        if (rules_frame >= 0 && frame == rules_frame + 40)
        {
            capture_readback("cache/pause_rules.bmp");
        }
        if (rules_frame >= 0 && reopened_frame < 0 && gbtg::g_scene_pause.phase == 3)
        {
            reopened_frame = frame;
            std::cout << "pause-rules frame " << frame << ": the rules closed and the book holds open\n";
        }
        if (reopened_frame >= 0 && resumed_frame < 0 && gbtg::read_u32({gbtg::g_scene_record, 0x108}, 0x104) == 1)
        {
            resumed_frame = frame;
        }
    }
    if (event_frame < 0 || open_frame < 0 || rules_frame < 0 || reopened_frame < 0 || resumed_frame < 0)
    {
        throw gbtg::Error("the event rules did not run through (event " + std::to_string(event_frame) + ", open "
            + std::to_string(open_frame) + ", rules " + std::to_string(rules_frame) + ", reopened "
            + std::to_string(reopened_frame) + ", resumed " + std::to_string(resumed_frame) + ")");
    }
    if (queued == 0)
    {
        throw gbtg::Error("Event Rules queued no reminder page");
    }
    std::cout << "PASS: Event Rules during chapter 5 scene 2's event queues " << queued
              << " reminder pages, closes them on A and returns to the pause (frames " << rules_frame << " .. "
              << reopened_frame << ")\n";
}

// --pause-map: scene 3 with the map's progress bit (progress byte 4 bit 8, a driver cheat that stands in for a game
// past the map's scene). Start pauses (prompt row 5: Return, Quit, Map), White (pause_Map, 0x0011E140) turns the book to
// the map (phase 7, the room's spot 0x001410F0 at book +0x118) and shows it (phase 8, 0x000F6F70 and 0x00142720), B
// (pause_MapReturn, 0x0011E3E0) goes back (phase 9, then 3) and B closes the pause.
void play_pause_map_run(gbtg::Game& game)
{
    auto& actors = game.factory_scene->start_actors(game);
    gbtg::g_runtime_profile.progress.bytes[4] |= 8;
    int free_frame = -1;
    int cooper_frame = -1;
    int open_frame = -1;
    int map_frame = -1;
    int returned_frame = -1;
    int resumed_frame = -1;
    std::string spot;
    for (int frame = 0; frame < 9000 && resumed_frame < 0; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5 && open_frame < 0;
        if (cooper_frame < 0)
        {
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.actor->class_key == 4)
                {
                    cooper_frame = frame;
                }
            }
        }
        if (cooper_frame >= 0 && free_frame < 0 && gbtg::g_pause_count == 0)
        {
            free_frame = frame;
        }
        const auto pressed = [frame](int at)
        {
            return at >= 0 && frame >= at && frame < at + 3;
        };
        if (open_frame < 0 && !page && pressed(free_frame >= 0 ? free_frame + 60 : -1))
        {
            input.start = true;
        }
        if (open_frame >= 0 && map_frame < 0 && pressed(open_frame + 60))
        {
            input.pads[0].buttons |= 0x2000; // White, the Map row entry's button code 5
        }
        if (map_frame >= 0 && returned_frame < 0 && pressed(map_frame + 120))
        {
            input.back = true;
        }
        if (returned_frame >= 0 && pressed(returned_frame + 120))
        {
            input.back = true;
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        const gbtg::i32 phase = gbtg::g_scene_pause.phase;
        static gbtg::i32 last_phase = -1;
        if (phase != last_phase)
        {
            std::cout << "pause-map frame " << frame << ": phase " << phase << "\n";
            last_phase = phase;
        }
        if (open_frame < 0 && phase == 3)
        {
            open_frame = frame;
            std::cout << "pause-map frame " << frame << ": the pause book is open, prompt row "
                      << gbtg::g_scene_pause.prompt << "\n";
        }
        if (open_frame >= 0 && map_frame < 0 && phase == 8)
        {
            map_frame = frame;
            if (gbtg::g_scene_pause.book != nullptr)
            {
                spot = gbtg::g_scene_pause.book->map_from.data();
            }
            std::cout << "pause-map frame " << frame << ": the map shows (phase 8), spot \"" << spot << "\"\n";
        }
        if (map_frame >= 0 && frame == map_frame + 60)
        {
            capture_readback("cache/pause_map.bmp");
        }
        if (map_frame >= 0 && returned_frame < 0 && phase == 3)
        {
            returned_frame = frame;
            std::cout << "pause-map frame " << frame << ": back from the map, prompt row "
                      << gbtg::g_scene_pause.prompt << "\n";
        }
        if (returned_frame >= 0 && resumed_frame < 0 && gbtg::read_u32({gbtg::g_scene_record, 0x108}, 0x104) == 1)
        {
            resumed_frame = frame;
        }
    }
    if (open_frame < 0 || map_frame < 0 || returned_frame < 0 || resumed_frame < 0)
    {
        throw gbtg::Error("the pause map did not run through (open " + std::to_string(open_frame) + ", map "
            + std::to_string(map_frame) + ", returned " + std::to_string(returned_frame) + ", resumed "
            + std::to_string(resumed_frame) + ")");
    }
    std::cout << "PASS: Map in scene 3's pause shows the map (spot \"" << spot << "\"), returns on B and closes on B"
              << " (frames " << map_frame << " .. " << resumed_frame << ")\n";
}

std::filesystem::path test_save_folder();
std::vector<char> saved_progress_file();

// --pause-quit-save: a saved game's scene 3. Once nothing holds the scene, Start pauses it, X (pause_Quit) asks to
// quit and A (pause_Yes, 0x0011E1C0) quits: the scene's time joins the profile's (DAT_00506C78), 0x000F2CB0 writes
// it into the save (+0x18C) with the scene's script (+0x102), and the closed book leads to game select
// (0x0011CFB0 choice 2, a named game).
void play_pause_quit_run(gbtg::Game& game)
{
    auto& actors = game.factory_scene->start_actors(game);
    int cooper_frame = -1;
    int free_frame = -1;
    int open_frame = -1;
    int confirm_frame = -1;
    int quit_frame = -1;
    int gameselect_frame = -1;
    for (int frame = 0; frame < 9000; ++frame)
    {
        gbtg::InputState input;
        const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
        input.accept = page && frame % 30 < 5 && open_frame < 0;
        if (cooper_frame < 0)
        {
            for (const auto& mark : actors.marked_actors())
            {
                if (mark.actor != nullptr && mark.actor->class_key == 4)
                {
                    cooper_frame = frame;
                }
            }
        }
        if (cooper_frame >= 0 && free_frame < 0 && gbtg::g_pause_count == 0)
        {
            free_frame = frame;
            std::cout << "pause-quit frame " << frame << ": nothing holds the scene\n";
        }
        const auto pressed = [frame](int at)
        {
            return at >= 0 && frame >= at && frame < at + 3;
        };
        if (open_frame < 0 && !page && pressed(free_frame >= 0 ? free_frame + 60 : -1))
        {
            input.start = true;
        }
        if (open_frame >= 0 && confirm_frame < 0 && pressed(open_frame + 60))
        {
            input.page = true;
        }
        if (confirm_frame >= 0 && quit_frame < 0 && pressed(confirm_frame + 120))
        {
            input.accept = true;
        }
        gbtg::gfx_begin_frame();
        game.tick(1.f / 60.f, input);
        if (open_frame < 0 && gbtg::g_scene_pause.phase == 3)
        {
            open_frame = frame;
            std::cout << "pause-quit frame " << frame << ": the pause book is open\n";
        }
        if (confirm_frame < 0 && gbtg::g_scene_pause.prompt == 9)
        {
            confirm_frame = frame;
            std::cout << "pause-quit frame " << frame << ": the quit warning shows\n";
        }
        if (confirm_frame >= 0 && quit_frame < 0 && gbtg::g_scene_pause.choice == 2)
        {
            quit_frame = frame;
            std::cout << "pause-quit frame " << frame << ": pause_Yes quits (choice 2), profile time "
                      << gbtg::g_runtime_profile.times[0] << "\n";
        }
        if (quit_frame >= 0 && gameselect_frame < 0 && game.scene_name == "aid_script_ghoulies_gameselect_gameselect")
        {
            gameselect_frame = frame;
            std::cout << "pause-quit frame " << frame << ": game select\n";
        }
        if (gameselect_frame >= 0 && gbtg::g_save_scan != nullptr && gbtg::g_save_scan->pending == 0
            && frame > gameselect_frame + 120)
        {
            break;
        }
    }
    if (quit_frame < 0)
    {
        throw gbtg::Error("pause_Yes did not quit the scene (open " + std::to_string(open_frame) + ", warning "
            + std::to_string(confirm_frame) + ")");
    }
    if (gameselect_frame < 0)
    {
        throw gbtg::Error("quitting from the pause did not lead to game select (scene " + game.scene_name + ")");
    }
    const std::vector<char> bytes = saved_progress_file();
    if (bytes.size() < 0x268)
    {
        throw gbtg::Error("no saved progress.xsv after the quit");
    }
    const std::string script(bytes.data() + 0x102);
    gbtg::u32 time = 0;
    std::memcpy(&time, bytes.data() + 0x18C, 4);
    std::cout << "pause-quit save: script " << script << ", time " << time << "\n";
    if (script != "aid_script_ghoulies_chapter1_scene3_1playcam" || time == 0)
    {
        throw gbtg::Error("the quit's save holds script " + script + " and time " + std::to_string(time));
    }
    std::cout << "PASS: pause_Yes quits scene 3 to game select and saves its script and time ("
              << time << ")\n";
}

std::vector<char> saved_progress_file()
{
    if (std::filesystem::exists(test_save_folder()))
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
        {
            if (entry.is_regular_file() && entry.path().filename() == "progress.xsv")
            {
                std::ifstream in(entry.path(), std::ios::binary);
                return std::vector<char>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            }
        }
    }
    return {};
}

void play_pause(gbtg::Game& game)
{
    try
    {
        play_pause_run(game);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

void play_scene3(gbtg::Game& game, bool drive, int book = -1)
{
    try
    {
        play_scene3_run(game, drive, book);
    }
    catch (const std::exception& error)
    {
        std::cout << "FAIL: " << error.what() << std::endl;
        std::_Exit(1);
    }
}

// Each run keeps its U: (saves) and Z: (utility) folders under cache/test_runs/<mode>-<pid>, so modes can run in
// parallel. A child run (std::system) inherits its parent's folder through GBTG_TEST_RUN, and the folder that made
// it removes it at a normal exit; a runner removes cache/test_runs/<mode>-* after a failed one.
std::filesystem::path g_test_run;

std::filesystem::path test_save_folder()
{
    return g_test_run / "test_saves";
}

void open_test_run(int argc, char** argv)
{
    if (const char* inherited = std::getenv("GBTG_TEST_RUN"); inherited != nullptr && inherited[0] != '\0')
    {
        g_test_run = inherited;
    }
    else
    {
        const std::string mode = argc >= 3 ? std::string(argv[2]).substr(argv[2][0] == '-' && argv[2][1] == '-' ? 2 : 0)
                                           : "plain";
        g_test_run = std::filesystem::path("cache") / "test_runs" / (mode + "-" + std::to_string(_getpid()));
        std::filesystem::remove_all(g_test_run);
        std::filesystem::create_directories(g_test_run);
        // Z: starts as the shared utility cache's copy, as a lone run would find it.
        if (std::filesystem::is_directory("cache/xbox_utility"))
        {
            std::filesystem::copy("cache/xbox_utility", g_test_run / "xbox_utility",
                std::filesystem::copy_options::recursive);
        }
        _putenv_s("GBTG_TEST_RUN", g_test_run.string().c_str());
        std::atexit([]
        {
            std::error_code ec;
            std::filesystem::remove_all(g_test_run, ec);
        });
    }
    gbtg::set_utility_root(g_test_run / "xbox_utility");
}

// Byte 0 of the challenge block in the test save's progress.xsv: the record (0x268 bytes), then the progress
// block (8 bytes), then the challenges (0x0010EB60's layout). 0xFF when there is no save.
gbtg::u32 saved_challenge_byte()
{
    if (!std::filesystem::exists(test_save_folder()))
    {
        return 0xFF;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
    {
        if (entry.is_regular_file() && entry.path().filename() == "progress.xsv")
        {
            std::ifstream in(entry.path(), std::ios::binary);
            std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            return bytes.size() > 0x270 ? gbtg::u8(bytes[0x270]) : 0xFF;
        }
    }
    return 0xFF;
}

// --gameselect-replay-scene's save: a child --gameselect-new-game run saves a new game, then its progress.xsv's
// record script (+0x102, read by 0x0010DB10) is set to `script` and the file is signed again as 0x0010EB60 signs
// it (0x000E9AF0, flags 0: the last 0x14 bytes), so the native load (0x0010EF10) verifies it.
void set_saved_script(const std::string& script);

void write_replay_save(const char* exe, const char* original, const std::string& script)
{
    const std::string command = "\"\"" + std::string(exe) + "\" \"" + original + "\" --gameselect-new-game\"";
    if (std::system(command.c_str()) != 0)
    {
        throw gbtg::Error("the --gameselect-new-game run that saves the game failed");
    }
    set_saved_script(script);
}

// The test save's record script (+0x102) becomes `script` and the file is signed again; its other bytes stay.
void set_saved_script(const std::string& script)
{
    std::filesystem::path file;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
    {
        if (entry.is_regular_file() && entry.path().filename() == "progress.xsv")
        {
            file = entry.path();
        }
    }
    if (file.empty())
    {
        throw gbtg::Error("the test saves hold no progress.xsv");
    }
    std::vector<gbtg::u8> bytes;
    {
        std::ifstream in(file, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    const gbtg::u32 size = gbtg::xbox_signature_size(0);
    if (bytes.size() < 0x268 + size)
    {
        throw gbtg::Error("progress.xsv is too short");
    }
    std::cout << "saved script " << reinterpret_cast<const char*>(bytes.data() + 0x102) << "\n";
    std::memset(bytes.data() + 0x102, 0, 0x80);
    std::memcpy(bytes.data() + 0x102, script.c_str(), script.size() + 1);
    const std::uintptr_t handle = gbtg::begin_xbox_signature(0);
    if (handle == std::uintptr_t(-1)
        || gbtg::update_xbox_signature(handle, bytes.data(), gbtg::u32(bytes.size() - size)) != 0
        || gbtg::end_xbox_signature(handle, bytes.data() + (bytes.size() - size)) != 0)
    {
        throw gbtg::Error("signing progress.xsv failed");
    }
    std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),
        std::streamsize(bytes.size()));
    std::cout << "wrote " << file.generic_string() << " with script " << script << "\n";
}

// Start opens game select; once it is ready, A on the saved game's slot opens its contents and A on Play starts the
// save's scene (0x000F3020 loads its challenges and progress into the current game).
void start_saved_game(gbtg::Game& game, const std::string& label)
{
    int ready = -1;
    bool started = false;
    std::string script = game.scene_script.name;
    for (int frame = 0; frame < 3000 && !started; ++frame)
    {
        if (game.scene_script.name != script)
        {
            script = game.scene_script.name;
            std::cout << label << ": frame " << frame << ", script " << script << "\n";
        }
        gbtg::InputState input;
        input.start = frame == 120;
        if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
        {
            ready = frame;
            std::cout << label << ": game select ready at frame " << frame << "\n";
        }
        const int at = ready < 0 ? -1 : frame - ready;
        input.accept = at == 180 || at == 900;
        hold_gameselect_press(game, input, ready);
        gbtg::gfx_begin_frame();
        game.tick(1.f / 59.f, input); // Game select's tests step the wall-clock path.
        gbtg::gfx_end_frame();
        started = gbtg::g_scene_transition.phase != 0 && at > 900;
    }
    const std::u16string name(gbtg::g_runtime_profile.name.data());
    if (!started || name.empty())
    {
        throw gbtg::Error("Play on the saved game did not start its scene with a named game");
    }
    std::cout << label << ": Play starts the saved game's scene\n";
}

int main(int argc, char** argv)
{
    gbtg::install_throw_trace(); // A failed .at() lookup or a gbtg::Error prints where it was thrown.
    open_test_run(argc, argv);
    // Tests play at 10% master volume (AGENTS.md); the master voice takes it when it is created.
    gbtg::pcm_set_master_volume(0.1f);
    // Test frames don't run in real time: cue state follows game time, so runs of one build are identical.
    gbtg::pcm_use_virtual_audio_clock();
#if !defined(GBTG_RETAIL_GAME_ONLY)
    // GBTG_PLUGINS_DIR loads that folder's plugins as gbtg does (docs/PLUGINS.md), for --gameselect-feature on a
    // plugin's story. Without it the tests load none.
    if (const char* plugins = std::getenv("GBTG_PLUGINS_DIR"); plugins != nullptr && argc >= 2)
    {
        gbtg::load_plugins({std::filesystem::path(plugins)}, argv[1], std::filesystem::path(argv[0]).parent_path());
    }
#endif
    const bool audio_test = argc == 3 && std::string_view(argv[2]) == "--audio-preparation";
    const bool title_a_test = argc == 3 && std::string_view(argv[2]) == "--title-a";
    // --attract leaves the attract reel to play its clips and the scripts that follow, with no input.
    const bool attract_test = argc == 3 && std::string_view(argv[2]) == "--attract";
    // --gameselect presses Start into game select, then Right, A and B there, capturing each screen.
    // --gameselect-native installs the game-select control (factory type 0x9A) and reports where its native start
    // stops (docs/NATIVE_FRONTEND.md).
    // --gameselect-new-game (native): A on the empty first slot, then A on the keyboard's Done with the default
    // name, which saves the new game into the empty test save folder.
    const bool new_game_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-new-game";
    const bool delete_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-delete";
    const bool stats_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-stats";
    // --gameselect-features: a new game's contents, Right to the Features panel and A; in the features menu Down
    // twice to Replay Scene and A (an error page for a new game), A to close it, then B back to the contents.
    const bool features_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-features";
    // --gameselect-replay-scene: a save on Chapter 1 Scene 2. A child run of --gameselect-new-game saves a game, its
    // progress.xsv's script (record +0x102) becomes aid_script_ghoulies_chapter1_scene2_1viewcam and is signed again,
    // and this run loads it: A on its slot, Right and A to the features menu, Down twice and A on Replay Scene, which
    // must request the scene replay (row 10) rather than show the not-past-scene-one page.
    // --gameselect-replay-paging: the same on a Chapter 1 Scene 10 save, whose nine scenes are more than the menu's
    // eight rows. X (Page Down, code 2) from the first scene shows the ninth alone and selects it; Y (Page Up, code
    // 3) goes back to the first page and its first scene.
    const bool replay_paging_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-replay-paging";
    // --gameselect-retail-save: a save written by the retail game (a finished game) is copied into the empty test
    // save folder from GBTG_RETAIL_SAVES (default ../Saves/Xbox/4d530053, the title's U: folder), outside the
    // repository. The native scan must verify its signature; A opens it, Right and A the features, Down to Replay
    // Scene and A, whose list must hold all 100 scenes; X pages down twice and Y once, then B goes back.
    const bool retail_save_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-retail-save";
    // --gameselect-challenges: the same retail save; A opens it, Right twice and A the challenges (row 6). Right
    // turns the carousel, Y shows the challenge's intro page and A closes it, then A plays the selected challenge,
    // whose script game select must start.
    // --gameselect-challenge-restart: the same run, but A (Restart Challenge) on the restart menu, which must start
    // the challenge again; the second run's restart menu then quits with B.
    const bool challenge_restart_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-challenge-restart";
    // --gameselect-feature: the retail save's features menu; Down to the item GBTG_FEATURE names and A. The page it
    // opens is logged and captured (cache/gameselect_feature_N.bmp); then A on its first item (or B with
    // GBTG_FEATURE_BACK), and the row or script that follows is reported.
    const bool feature_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-feature";
    // --gameselect-corrupt-save: the retail save with its progress.xsv damaged. Slot select's scan must show the app
    // error screen (apperror slotselection_corruptgamemessage, 0x000DF6B0); A closes it (0x000DF760) and slot select
    // runs on (cache/gameselect_corrupt_N.bmp).
    const bool corrupt_save_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-corrupt-save";
    // --gameselect-duplicate-name: a new game with the default name in slot 1, B back to slot select, then slot 2 and
    // Done with the same default name. The keyboard's check (0x001411C0, 2) must show the dialogs page
    // errormessages_notuniquegamename (0x000582A0) and keep the naming page open without a second save
    // (cache/gameselect_duplicate_N.bmp).
    const bool duplicate_name_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-duplicate-name";
    // --gameselect-secret: a new game's contents take the secret sequence (0x000DB1A0): both triggers held, then
    // left stick, right stick, White, Black, White, right stick, left stick. The progress bit 0x3D must be set and
    // saved (0x000DB380); then Right and A open the features, whose Artwork item (now listed) A starts
    // (aid_script_ghoulies_gameselect_unlockableart).
    const bool secret_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-secret";
    // --gameselect-loading: the utility drive's cached harddisk wavebanks are deleted first, so boot copies them
    // again (0x00039C50, 0x000E9090, a buffer a frame through 0x000E9280). Game select opening while stream 2's
    // copy runs (0x000E5AE0) must show the loading screen (row 18, cache/gameselect_loading_N.bmp) and then slot
    // select.
    const bool loading_test = argc == 3 && std::string_view(argv[2]) == "--gameselect-loading";
    const bool challenges_test = challenge_restart_test
        || (argc == 3 && std::string_view(argv[2]) == "--gameselect-challenges");
    const bool replay_scene_test = replay_paging_test
        || (argc == 3 && std::string_view(argv[2]) == "--gameselect-replay-scene");
    if (replay_scene_test || retail_save_test || challenges_test)
    {
        std::cout << std::unitbuf; // its output survives a crash in teardown
    }
    const bool gameselect_native_test = new_game_test || delete_test || stats_test || features_test || replay_scene_test
        || retail_save_test || challenges_test || feature_test || corrupt_save_test || duplicate_name_test
        || secret_test || loading_test
        || (argc == 3 && std::string_view(argv[2]) == "--gameselect-native");
    const bool gameselect_test = gameselect_native_test || (argc == 3 && std::string_view(argv[2]) == "--gameselect");
    const bool title_test = title_a_test || attract_test || gameselect_test
        || (argc == 3 && std::string_view(argv[2]) == "--title");
    const bool goop_test = argc == 3 && std::string_view(argv[2]) == "--goop";
    const bool ui_test = argc == 3 && std::string_view(argv[2]) == "--ui";
    const bool dialogue_resources_test = argc == 3 && std::string_view(argv[2]) == "--dialogue-resources";
    // The player modes tick 1/60 s, the step their checkpoints were taken at. A trailing --retail-step ticks 1/30 s,
    // retail's step: it swaps every second vblank (PresentationInterval 2) and 0x000FC030 counts the fields.
    const bool retail_step = argc == 4 && std::string_view(argv[3]) == "--retail-step";
    const auto player_mode = [&](std::string_view name)
    {
        return (argc == 3 || retail_step) && std::string_view(argv[2]) == name;
    };
    const float player_step = retail_step ? 2.f / 60.f : 1.f / 60.f;
    // A count of 1/60 s frames as ticks of player_step, for the drivers' waits.
    const auto player_ticks = [player_step](int frames)
    {
        return int(float(frames) / 60.f / player_step + .5f);
    };
    // --player-fight runs the --player-opening flow, then fights the imps until the script passes its imp gate.
    // --player-death runs the --player-opening flow, then leaves Cooper to the imps until he dies and the scene restarts.
    // --player-brew: the death test with the brew unlocked (profile unlock bit 8). The restarted scene opens on the
    // Butler's Brew prompt (player state 0x3F1): its meter (0x0012D300) and A/B widget (0x0012D290).
    const bool brew_test = player_mode("--player-brew");
    const bool death_test = player_mode("--player-death") || brew_test;
    const bool fight_test = player_mode("--player-fight") || death_test;
    // --player-controls runs the --player-opening flow, then checks run speed across frame rates and the right stick.
    const bool controls_test = player_mode("--player-controls");
    const bool player_test = player_mode("--player-opening") || fight_test || controls_test;
    // The death run gives Cooper no sticks: the imps win.
    const bool passive = death_test;
    const bool decoder_test = argc == 3 && std::string_view(argv[2]) == "--ghoulybox-decoder";
    const bool actor_test = argc == 3 && std::string_view(argv[2]) == "--actor-loading";
    const bool model_resources_test = argc == 3 && std::string_view(argv[2]) == "--scene-models";
    const bool scene_start_test = argc == 3 && std::string_view(argv[2]) == "--scene-start";
    const bool spawn_test = (argc == 3 && std::string_view(argv[2]) == "--scene-spawn") || scene_start_test;
    // --scene6 boots straight into scene 6 (chapter1_scene2_1viewcam), presses A on the chapter title and plays on.
    // --scene6-save plays scene 6 from a save: a child --gameselect-new-game run saves a game whose script becomes
    // chapter1_scene2_1viewcam, game select loads it (A on its slot, A on Play), the trophy heads' award saves
    // challenge 1 and scene 3's rare book challenge 2 into its progress.xsv (0x000F24C0, 0x000F3230, 0x0010DA80).
    const bool scene6_save_test = argc == 3 && std::string_view(argv[2]) == "--scene6-save";
    // --scene6-save-reload (run by --scene6-save after the write): the written save, its script set to scene 3, is
    // loaded from game select again, and scene 3 must start without the rare book (0x00109FC0 skips a powerup kind 1
    // emitter whose challenge bit is set).
    const bool scene6_reload_test = argc == 3 && std::string_view(argv[2]) == "--scene6-save-reload";
    const bool scene6_test = scene6_save_test || (argc == 3 && std::string_view(argv[2]) == "--scene6");
    // --scene3 boots straight into scene 3 (chapter1_scene3_1playcam), presses A on the chapter title and plays on.
    const bool scene3_test = argc == 3 && std::string_view(argv[2]) == "--scene3";
    // --pause boots scene 3 like --scene3, then pauses and resumes it with Start.
    const bool pause_test = argc == 3 && std::string_view(argv[2]) == "--pause";
    // --pause-quit-save loads a scene 3 save (a child --gameselect-new-game run, its script set to scene 3), then
    // quits the scene from the pause (Start, X, A) and checks the save holds the scene's script and time.
    const bool pause_quit_test = argc == 3 && std::string_view(argv[2]) == "--pause-quit-save";
    const bool pause_rules_test = argc == 3 && std::string_view(argv[2]) == "--pause-rules";
    const bool pause_map_test = argc == 3 && std::string_view(argv[2]) == "--pause-map";
    // --chapter5-save loads a chapter 5 scene 1 save and plays scene 1 to its end. Scene 2's start (0x000F2C40)
    // must record the furthest book scene (+0x25D, 0x000F34C0) and, inside the countdown's scenes, leave the save's
    // script at scene 1 (0x000F3410) and its times alone (0x000F3590).
    const bool chapter5_save_test = argc == 3 && std::string_view(argv[2]) == "--chapter5-save";
    // --chapter5-finale-save loads a save at chapter5_scene13_1book: its start (0x000F34C0, book scene 13) sets the
    // save's +0x264 and leaves the book scene byte (+0x25D) alone.
    const bool chapter5_finale_test = argc == 3 && std::string_view(argv[2]) == "--chapter5-finale-save";
    // --scene4 boots scene 4 (chapter1_scene4_1book), closes its pages with A and plays until the script moves on.
    const bool scene4_test = argc == 3 && std::string_view(argv[2]) == "--scene4";
    // --scene5 boots scene 5 (chapter1_scene5_1playcam), closes its pages with A and plays until the script moves on.
    const bool scene5_test = argc == 3 && std::string_view(argv[2]) == "--scene5";
    // --scene6-1 boots scene 6 (chapter1_scene6_1playcam, the Lower Corridors), closes its pages with A and plays until
    // the script moves on.
    const bool scene6_1_test = argc == 3 && std::string_view(argv[2]) == "--scene6-1";
    // --scene7 boots scene 7 (chapter1_scene7_1playcam), closes its pages with A and plays until the script moves on.
    const bool scene7_test = argc == 3 && std::string_view(argv[2]) == "--scene7";
    const bool scene8_test = argc == 3 && std::string_view(argv[2]) == "--scene8";
    const bool scene9_test = argc == 3 && std::string_view(argv[2]) == "--scene9";
    const bool scene10_test = argc == 3 && std::string_view(argv[2]) == "--scene10";
    const bool scene11_test = argc == 3 && std::string_view(argv[2]) == "--scene11";
    const bool scene12_test = argc == 3 && std::string_view(argv[2]) == "--scene12";
    const bool scene2a1b_test = argc == 3 && std::string_view(argv[2]) == "--scene2a1b";
    const bool scene2a1_test = argc == 3 && std::string_view(argv[2]) == "--scene2a1";
    // --scene2a2 boots chapter 2a scene 2 (chapter2a_scene2_1playcam), closes its pages with A and plays until the
    // script moves on.
    const bool scene2a2_test = argc == 3 && std::string_view(argv[2]) == "--scene2a2";
    // --scene2a2-reaper plays chapter 2a scene 2 without fighting, so the event's time limit runs out and the Grim
    // Reaper comes.
    const bool scene2a2_reaper_test = argc == 3 && std::string_view(argv[2]) == "--scene2a2-reaper";
    // --scene2a3 boots chapter 2a scene 3 (chapter2a_scene3_1playcam), closes its pages with A and plays until the
    // script moves on.
    const bool scene2a3_test = argc == 3 && std::string_view(argv[2]) == "--scene2a3";
    // --scene2a4 boots chapter 2a scene 4 (chapter2a_scene4_2playcam), closes its pages with A and plays until the
    // script moves on.
    const bool scene2a4_test = argc == 3 && std::string_view(argv[2]) == "--scene2a4";
    // --scene2a5 boots chapter 2a scene 5 (chapter2a_scene5_1playcam), closes its pages with A and plays until the
    // script moves on.
    const bool scene2a5_test = argc == 3 && std::string_view(argv[2]) == "--scene2a5";
    // --scene2a6 boots chapter 2a scene 6 (chapter2a_scene6_1book), passes its book and plays the scene it moves on
    // to until the script moves on again.
    const bool scene2a6_test = argc == 3 && std::string_view(argv[2]) == "--scene2a6";
    // --scene2a7 boots chapter 2a scene 7 (chapter2a_scene7_2playcam) and plays it until the script moves on.
    const bool scene2a7_test = argc == 3 && std::string_view(argv[2]) == "--scene2a7";
    // --scene2a8 boots chapter 2a scene 8 (chapter2a_scene8_1playcam) and plays it until the script moves on.
    const bool scene2a8_test = argc == 3 && std::string_view(argv[2]) == "--scene2a8";
    // --scene2a9 boots chapter 2a scene 9 (chapter2a_scene9_1playcam) and plays it until the script moves on.
    const bool scene2a9_test = argc == 3 && std::string_view(argv[2]) == "--scene2a9";
    // --scene2a10 boots chapter2a_scene10_1playcam and plays it until the script moves on.
    const bool scene2a10_test = argc == 3 && std::string_view(argv[2]) == "--scene2a10";
    // --scene2a11 boots chapter2a_scene11_1playcam and plays it until the script moves on.
    const bool scene2a11_test = argc == 3 && std::string_view(argv[2]) == "--scene2a11";
    // --scene2a12 boots chapter2a_scene12_2playcam and plays it until the script moves on.
    const bool scene2a12_test = argc == 3 && std::string_view(argv[2]) == "--scene2a12";
    // --scene2a13 boots chapter2a_scene13_2playcam and plays it until the script moves on.
    const bool scene2a13_test = argc == 3 && std::string_view(argv[2]) == "--scene2a13";
    // --scene2a14 boots chapter2a_scene14_1playcam and plays it until the script moves on.
    const bool scene2a14_test = argc == 3 && std::string_view(argv[2]) == "--scene2a14";
    // --scene2a15 boots chapter2a_scene15_1playcam and plays it until the script moves on.
    const bool scene2a15_test = argc == 3 && std::string_view(argv[2]) == "--scene2a15";
    // --scene2a16 boots chapter2a_scene16_1playcam and plays it until the script moves on.
    const bool scene2a16_test = argc == 3 && std::string_view(argv[2]) == "--scene2a16";
    // --scene2a17 boots chapter2a_scene17_1playcam and plays it until the script moves on.
    const bool scene2a17_test = argc == 3 && std::string_view(argv[2]) == "--scene2a17";
    // --scene2b1 boots chapter2b_scene1_1playcam and plays it until the script moves on.
    const bool scene2b1_test = argc == 3 && std::string_view(argv[2]) == "--scene2b1";
    // --scene2b2 boots chapter2b_scene2_1playcam and plays it until the script moves on.
    const bool scene2b2_test = argc == 3 && std::string_view(argv[2]) == "--scene2b2";
    // --scene2b3 boots chapter2b_scene3_2playcam and plays it until the script moves on.
    const bool scene2b3_test = argc == 3 && std::string_view(argv[2]) == "--scene2b3";
    // --scene2b4 boots chapter2b_scene4_1playcam and plays it until the script moves on.
    const bool scene2b4_test = argc == 3 && std::string_view(argv[2]) == "--scene2b4";
    // --scene2b4b boots chapter2b_scene4_2book and plays it until the script moves on.
    const bool scene2b4b_test = argc == 3 && std::string_view(argv[2]) == "--scene2b4b";
    // --scene2b5 boots chapter2b_scene5_1playcam and plays it until the script moves on.
    const bool scene2b5_test = argc == 3 && std::string_view(argv[2]) == "--scene2b5";
    // --scene2b6 boots chapter2b_scene6_1book and plays it until the script moves on.
    const bool scene2b6_test = argc == 3 && std::string_view(argv[2]) == "--scene2b6";
    // --scene2b6b boots chapter2b_scene6_2playcam and plays it until the script moves on.
    const bool scene2b6b_test = argc == 3 && std::string_view(argv[2]) == "--scene2b6b";
    // --scene2b7 boots chapter2b_scene7_1playcam and plays it until the script moves on.
    const bool scene2b7_test = argc == 3 && std::string_view(argv[2]) == "--scene2b7";
    // --scene2b8 boots chapter2b_scene8_1playcam and plays it until the script moves on.
    const bool scene2b8_test = argc == 3 && std::string_view(argv[2]) == "--scene2b8";
    // --scene2b9 boots chapter2b_scene9_1playcam and plays it until the script moves on.
    const bool scene2b9_test = argc == 3 && std::string_view(argv[2]) == "--scene2b9";
    // --scene2b10 boots chapter2b_scene10_1playcam and plays it until the script moves on.
    const bool scene2b10_test = argc == 3 && std::string_view(argv[2]) == "--scene2b10";
    // --scene2b10b boots chapter2b_scene10_2book and plays it until the script moves on.
    const bool scene2b10b_test = argc == 3 && std::string_view(argv[2]) == "--scene2b10b";
    // --scene2c1 boots chapter2c_scene1_1playcam and plays it until the script moves on.
    const bool scene2c1_test = argc == 3 && std::string_view(argv[2]) == "--scene2c1";
    // --scene2c2 boots chapter2c_scene2_1playcam and plays it until the script moves on.
    const bool scene2c2_test = argc == 3 && std::string_view(argv[2]) == "--scene2c2";
    // --scene2c3 boots chapter2c_scene3_1playcam and plays it until the script moves on.
    const bool scene2c3_test = argc == 3 && std::string_view(argv[2]) == "--scene2c3";
    // --scene2c4 boots chapter2c_scene4_1book and plays it until the script moves on.
    const bool scene2c4_test = argc == 3 && std::string_view(argv[2]) == "--scene2c4";
    // --scene2c4b boots chapter2c_scene4_2playcam and plays it until the script moves on.
    const bool scene2c4b_test = argc == 3 && std::string_view(argv[2]) == "--scene2c4b";
    // --scene2c5 boots chapter2c_scene5_1playcam and plays it until the script moves on.
    const bool scene2c5_test = argc == 3 && std::string_view(argv[2]) == "--scene2c5";
    // --scene2c6 boots chapter2c_scene6_1playcam and plays it until the script moves on.
    const bool scene2c6_test = argc == 3 && std::string_view(argv[2]) == "--scene2c6";
    // --scene2c7 boots chapter2c_scene7_1playcam and plays it until the script moves on.
    const bool scene2c7_test = argc == 3 && std::string_view(argv[2]) == "--scene2c7";
    // --scene2c8 boots chapter2c_scene8_1playcam and plays it until the script moves on.
    const bool scene2c8_test = argc == 3 && std::string_view(argv[2]) == "--scene2c8";
    // --scene2c9 boots chapter2c_scene9_1playcam and plays it until the script moves on.
    const bool scene2c9_test = argc == 3 && std::string_view(argv[2]) == "--scene2c9";
    // --scene2c10 boots chapter2c_scene10_1playcam and plays it until the script moves on.
    const bool scene2c10_test = argc == 3 && std::string_view(argv[2]) == "--scene2c10";
    // --scene2c11 boots chapter2c_scene11_1playcam and plays it until the script moves on.
    const bool scene2c11_test = argc == 3 && std::string_view(argv[2]) == "--scene2c11";
    // --scene2c11b boots chapter2c_scene11_2book and plays it until the script moves on.
    const bool scene2c11b_test = argc == 3 && std::string_view(argv[2]) == "--scene2c11b";
    // --scene2c12 boots chapter2c_scene12_1playcam and plays it until the script moves on.
    const bool scene2c12_test = argc == 3 && std::string_view(argv[2]) == "--scene2c12";
    // --scene2c13 boots chapter2c_scene13_1book and plays it until the script moves on.
    const bool scene2c13_test = argc == 3 && std::string_view(argv[2]) == "--scene2c13";
    // --scene2c13b boots chapter2c_scene13_2playcam and plays it until the script moves on.
    const bool scene2c13b_test = argc == 3 && std::string_view(argv[2]) == "--scene2c13b";
    // --scene2c13c boots chapter2c_scene13_3book and plays it until the script moves on.
    const bool scene2c13c_test = argc == 3 && std::string_view(argv[2]) == "--scene2c13c";
    // --scene3a1 boots chapter3a_scene1_2book and plays it until the script moves on.
    const bool scene3a1_test = argc == 3 && std::string_view(argv[2]) == "--scene3a1";
    // --scene3a1b boots chapter3a_scene1_3playcam and plays it until the script moves on.
    const bool scene3a1b_test = argc == 3 && std::string_view(argv[2]) == "--scene3a1b";
    // --scene3a2 boots chapter3a_scene2_1playcam and plays it until the script moves on.
    const bool scene3a2_test = argc == 3 && std::string_view(argv[2]) == "--scene3a2";
    // --scene3a3 boots chapter3a_scene3_1playcam and plays it until the script moves on.
    const bool scene3a3_test = argc == 3 && std::string_view(argv[2]) == "--scene3a3";
    // --scene3a3b boots chapter3a_scene3_2book and plays it until the script moves on.
    const bool scene3a3b_test = argc == 3 && std::string_view(argv[2]) == "--scene3a3b";
    // --scene3a3c boots chapter3a_scene3_3playcam and plays it until the script moves on.
    const bool scene3a3c_test = argc == 3 && std::string_view(argv[2]) == "--scene3a3c";
    // --scene3a4 boots chapter3a_scene4_1playcam and plays it until the script moves on.
    const bool scene3a4_test = argc == 3 && std::string_view(argv[2]) == "--scene3a4";
    // --scene3a5 boots chapter3a_scene5_1playcam and plays it until the script moves on.
    const bool scene3a5_test = argc == 3 && std::string_view(argv[2]) == "--scene3a5";
    // --scene3a6 boots chapter3a_scene6_1playcam and plays it until the script moves on.
    const bool scene3a6_test = argc == 3 && std::string_view(argv[2]) == "--scene3a6";
    // --scene3a7 boots chapter3a_scene7_1playcam and plays it until the script moves on.
    const bool scene3a7_test = argc == 3 && std::string_view(argv[2]) == "--scene3a7";
    // --scene3a8 boots chapter3a_scene8_1playcam and plays it until the script moves on.
    const bool scene3a8_test = argc == 3 && std::string_view(argv[2]) == "--scene3a8";
    // --scene3a9 boots chapter3a_scene9_1playcam and plays it until the script moves on.
    const bool scene3a9_test = argc == 3 && std::string_view(argv[2]) == "--scene3a9";
    // --scene3a10 boots chapter3a_scene10_2playcam and plays it until the script moves on.
    const bool scene3a10_test = argc == 3 && std::string_view(argv[2]) == "--scene3a10";
    // --scene3a11 boots chapter3a_scene11_1playcam and plays it until the script moves on.
    const bool scene3a11_test = argc == 3 && std::string_view(argv[2]) == "--scene3a11";
    // --scene3a12 boots chapter3a_scene12_1playcam and plays it until the script moves on.
    const bool scene3a12_test = argc == 3 && std::string_view(argv[2]) == "--scene3a12";
    // --scene3b1 boots chapter3b_scene1_1playcam and plays it until the script moves on.
    const bool scene3b1_test = argc == 3 && std::string_view(argv[2]) == "--scene3b1";
    // --scene3b2 boots chapter3b_scene2_1playcam and plays it until the script moves on.
    const bool scene3b2_test = argc == 3 && std::string_view(argv[2]) == "--scene3b2";
    // --scene3b3 boots chapter3b_scene3_1playcam and plays it until the script moves on.
    const bool scene3b3_test = argc == 3 && std::string_view(argv[2]) == "--scene3b3";
    // --scene3b4 boots chapter3b_scene4_2playcam and plays it until the script moves on.
    const bool scene3b4_test = argc == 3 && std::string_view(argv[2]) == "--scene3b4";
    // --scene3b5 boots chapter3b_scene5_2playcam and plays it until the script moves on.
    const bool scene3b5_test = argc == 3 && std::string_view(argv[2]) == "--scene3b5";
    // --scene3b6 boots chapter3b_scene6_1playcam and plays it until the script moves on.
    const bool scene3b6_test = argc == 3 && std::string_view(argv[2]) == "--scene3b6";
    // --scene3b7 boots chapter3b_scene7_1playcam and plays it until the script moves on.
    const bool scene3b7_test = argc == 3 && std::string_view(argv[2]) == "--scene3b7";
    // --scene3c1 boots chapter3c_scene1_1playcam and plays it until the script moves on.
    const bool scene3c1_test = argc == 3 && std::string_view(argv[2]) == "--scene3c1";
    // --scene3c2 boots chapter3c_scene2_1playcam and plays it until the script moves on.
    const bool scene3c2_test = argc == 3 && std::string_view(argv[2]) == "--scene3c2";
    // --scene3c3 boots chapter3c_scene3_2playcam and plays it until the script moves on.
    const bool scene3c3_test = argc == 3 && std::string_view(argv[2]) == "--scene3c3";
    // --scene3c4 boots chapter3c_scene4_1playcam and plays it until the script moves on.
    const bool scene3c4_test = argc == 3 && std::string_view(argv[2]) == "--scene3c4";
    // --scene3c5 boots chapter3c_scene5_1playcam and plays it until the script moves on.
    const bool scene3c5_test = argc == 3 && std::string_view(argv[2]) == "--scene3c5";
    // --scene3c6 boots chapter3c_scene6_1playcam and plays it until the script moves on.
    const bool scene3c6_test = argc == 3 && std::string_view(argv[2]) == "--scene3c6";
    // --scene3c7 boots chapter3c_scene7_1playcam and plays it until the script moves on.
    const bool scene3c7_test = argc == 3 && std::string_view(argv[2]) == "--scene3c7";
    // --scene3c8 boots chapter3c_scene8_1playcam and plays it until the script moves on.
    const bool scene3c8_test = argc == 3 && std::string_view(argv[2]) == "--scene3c8";
    // --scene3c9 boots chapter3c_scene9_1playcam and plays it until the script moves on.
    const bool scene3c9_test = argc == 3 && std::string_view(argv[2]) == "--scene3c9";
    // --scene3c10 boots chapter3c_scene10_1playcam and plays it until the script moves on.
    const bool scene3c10_test = argc == 3 && std::string_view(argv[2]) == "--scene3c10";
    // --scene3c11 boots chapter3c_scene11_1playcam and plays it until the script moves on.
    const bool scene3c11_test = argc == 3 && std::string_view(argv[2]) == "--scene3c11";
    // --scene3c12 boots chapter3c_scene12_1playcam and plays it until the script moves on.
    const bool scene3c12_test = argc == 3 && std::string_view(argv[2]) == "--scene3c12";
    // --scene3c13 boots chapter3c_scene13_2book and plays it until the script moves on.
    const bool scene3c13_test = argc == 3 && std::string_view(argv[2]) == "--scene3c13";
    // --scene3c13b boots chapter3c_scene13_3playcam and plays it until the script moves on.
    const bool scene3c13b_test = argc == 3 && std::string_view(argv[2]) == "--scene3c13b";
    // --scene3c14 boots chapter3c_scene14_1playcam and plays it until the script moves on.
    const bool scene3c14_test = argc == 3 && std::string_view(argv[2]) == "--scene3c14";
    // --scene3c15 boots chapter3c_scene15_1playcam and plays it until the script moves on.
    const bool scene3c15_test = argc == 3 && std::string_view(argv[2]) == "--scene3c15";
    // --scene3c15b boots chapter3c_scene15_2book and plays it until the script moves on.
    const bool scene3c15b_test = argc == 3 && std::string_view(argv[2]) == "--scene3c15b";
    // --scene3c15c boots chapter3c_scene15_3playcam and plays it until the script moves on.
    const bool scene3c15c_test = argc == 3 && std::string_view(argv[2]) == "--scene3c15c";
    // --scene4_1 boots chapter4_scene1_1book and plays it until the script moves on.
    const bool scene4_1_test = argc == 3 && std::string_view(argv[2]) == "--scene4_1";
    // --scene4_1b boots chapter4_scene1_1playcam and plays it until the script moves on.
    const bool scene4_1b_test = argc == 3 && std::string_view(argv[2]) == "--scene4_1b";
    // --scene4_1c boots chapter4_scene1_2book and plays it until the script moves on.
    const bool scene4_1c_test = argc == 3 && std::string_view(argv[2]) == "--scene4_1c";
    // --scene5_1 boots chapter5_scene1_1playcam and plays it until the script moves on.
    const bool scene5_1_test = argc == 3 && std::string_view(argv[2]) == "--scene5_1";
    // --scene5_2 boots chapter5_scene2_1playcam and plays it until the script moves on.
    const bool scene5_2_test = argc == 3 && std::string_view(argv[2]) == "--scene5_2";
    // --scene5_3 boots chapter5_scene3_1playcam and plays it until the script moves on.
    const bool scene5_3_test = argc == 3 && std::string_view(argv[2]) == "--scene5_3";
    // --scene5_4 boots chapter5_scene4_1playcam and plays it until the script moves on.
    const bool scene5_4_test = argc == 3 && std::string_view(argv[2]) == "--scene5_4";
    // --scene5_5 boots chapter5_scene5_1playcam and plays it until the script moves on.
    const bool scene5_5_test = argc == 3 && std::string_view(argv[2]) == "--scene5_5";
    // --scene5_6 boots chapter5_scene6_1playcam and plays it until the script moves on.
    const bool scene5_6_test = argc == 3 && std::string_view(argv[2]) == "--scene5_6";
    // --scene5_7 boots chapter5_scene7_1playcam and plays it until the script moves on.
    const bool scene5_7_test = argc == 3 && std::string_view(argv[2]) == "--scene5_7";
    // --scene5_8 boots chapter5_scene8_1playcam and plays it until the script moves on.
    const bool scene5_8_test = argc == 3 && std::string_view(argv[2]) == "--scene5_8";
    // --scene5_9 boots chapter5_scene9_1playcam and plays it until the script moves on.
    const bool scene5_9_test = argc == 3 && std::string_view(argv[2]) == "--scene5_9";
    // --scene5_10 boots chapter5_scene10_1playcam and plays it until the script moves on.
    const bool scene5_10_test = argc == 3 && std::string_view(argv[2]) == "--scene5_10";
    // --scene5_11 boots chapter5_scene11_1playcam and plays it until the script moves on.
    const bool scene5_11_test = argc == 3 && std::string_view(argv[2]) == "--scene5_11";
    // --scene5_12 boots chapter5_scene12_1playcam and plays it until the script moves on.
    const bool scene5_12_test = argc == 3 && std::string_view(argv[2]) == "--scene5_12";
    // --scene5_13 boots chapter5_scene13_1playcam and plays it until the script moves on.
    const bool scene5_13_test = argc == 3 && std::string_view(argv[2]) == "--scene5_13";
    // --scene5_13b boots chapter5_scene13_1book and plays it until the script moves on.
    const bool scene5_13b_test = argc == 3 && std::string_view(argv[2]) == "--scene5_13b";
    // --scene5_13c boots chapter5_scene13_2playcam and plays it until the script moves on.
    const bool scene5_13c_test = argc == 3 && std::string_view(argv[2]) == "--scene5_13c";
    // --scene5_13d boots chapter5_scene13_3book and plays it until the script moves on.
    const bool scene5_13d_test = argc == 3 && std::string_view(argv[2]) == "--scene5_13d";
    // --scenefinal boots finalscore_finalscore and plays it until the script moves on.
    const bool scenefinal_test = argc == 3 && std::string_view(argv[2]) == "--scenefinal";
    // --scenecredits boots credits_final and plays it until the script moves on.
    const bool scenecredits_test = argc == 3 && std::string_view(argv[2]) == "--scenecredits";
    if (spawn_test)
    {
        std::cout << std::unitbuf;
    }
    if (argc != 2 && argc != 4 && !title_test && !goop_test && !ui_test && !player_test && !actor_test && !decoder_test && !audio_test && !dialogue_resources_test
        && !model_resources_test && !spawn_test && !scene6_test && !scene6_reload_test && !scene4_test && !scene5_test && !scene6_1_test && !scene7_test && !scene8_test && !scene9_test && !scene10_test && !scene11_test && !scene12_test && !scene2a1_test && !scene2a1b_test && !scene2a2_test && !scene2a2_reaper_test && !scene2a3_test && !scene2a4_test && !scene2a5_test && !scene2a6_test && !scene2a7_test && !scene2a8_test && !scene2a9_test && !scene2a10_test && !scene2a11_test && !scene2a12_test && !scene2a13_test && !scene2a14_test && !scene2a15_test && !scene2a16_test && !scene2a17_test && !scene2b1_test && !scene2b2_test && !scene2b3_test && !scene2b4_test && !scene2b4b_test && !scene2b5_test && !scene2b6_test && !scene2b6b_test && !scene2b7_test && !scene2b8_test && !scene2b9_test && !scene2b10_test && !scene2b10b_test && !scene2c1_test && !scene2c2_test && !scene2c3_test && !scene2c4_test && !scene2c4b_test && !scene2c5_test && !scene2c6_test && !scene2c7_test && !scene2c8_test && !scene2c9_test && !scene2c10_test && !scene2c11_test && !scene2c11b_test && !scene2c12_test && !scene2c13_test && !scene2c13b_test && !scene2c13c_test && !scene3a1_test && !scene3a1b_test && !scene3a2_test && !scene3a3_test && !scene3a3b_test && !scene3a3c_test && !scene3a4_test && !scene3a5_test && !scene3a6_test && !scene3a7_test && !scene3a8_test && !scene3a9_test && !scene3a10_test && !scene3a11_test && !scene3a12_test && !scene3b1_test && !scene3b2_test && !scene3b3_test && !scene3b4_test && !scene3b5_test && !scene3b6_test && !scene3b7_test && !scene3c1_test && !scene3c2_test && !scene3c3_test && !scene3c4_test && !scene3c5_test && !scene3c6_test && !scene3c7_test && !scene3c8_test && !scene3c9_test && !scene3c10_test && !scene3c11_test && !scene3c12_test && !scene3c13_test && !scene3c13b_test && !scene3c14_test && !scene3c15_test && !scene3c15b_test && !scene3c15c_test && !scene4_1_test && !scene4_1b_test && !scene4_1c_test && !scene5_1_test && !scene5_2_test && !scene5_3_test && !scene5_4_test && !scene5_5_test && !scene5_6_test && !scene5_7_test && !scene5_8_test && !scene5_9_test && !scene5_10_test && !scene5_11_test && !scene5_12_test && !scene5_13_test && !scene5_13b_test && !scene5_13c_test && !scene5_13d_test && !scenefinal_test && !scenecredits_test && !scene3_test && !pause_test && !pause_quit_test && !pause_rules_test && !pause_map_test && !chapter5_save_test && !chapter5_finale_test)
    {
        std::cerr << "Usage: gbtg-transition-tests GAME_DIRECTORY [WIDTH HEIGHT | --title | --title-a | --attract | --gameselect | --gameselect-native | --gameselect-new-game | --gameselect-delete | --gameselect-stats | --gameselect-features | --goop | --ui | --player-opening | --player-fight | --player-controls | --player-death [--retail-step] | --dialogue-resources | --actor-loading | --ghoulybox-decoder | --audio-preparation | --scene-models | --scene-spawn | --scene-start | --scene6 | --scene6-save | --scene6-save-reload | --scene3 | --pause | --scene4 | --scene5 | --scene6-1 | --scene7]\n";
        return 2;
    }
    try
    {
        if (model_resources_test)
        {
            gbtg::AidStore aids;
            aids.open(argv[1]);
            check_scene_model_resources(aids);
            std::cout << "PASS: retail opening models relocate, post-load and instance through the scene provider\n";
            return 0;
        }
        if (dialogue_resources_test)
        {
            gbtg::AidStore aids;
            aids.open(argv[1]);
            check_player_dialogue_resources(aids);
            std::cout << "PASS: retail player tutorial localization, font metrics and dialogue page setup\n";
            return 0;
        }
        if (audio_test)
        {
            check_native_audio_preparation(argv[1]);
            return 0;
        }
        if (decoder_test)
        {
            check_ghoulybox_decoder(argv[1]);
            std::cout << "PASS: retail ghoulybox node decoding and spawn pool growth\n";
            return 0;
        }
        if (actor_test)
        {
            // No scene: DAT_00511AA4 is empty, so 0x00122C70's broadcast reaches no actor.
            gbtg::g_actor_broadcast = [](const gbtg::i32*)
            {
            };
            check_actor_spawn_eligibility();
            check_actor_spawn_filters();
            check_actor_navigation();
            check_actor_occupancy_and_paths();
            check_spawn_point_queries();
            check_spawn_scheduler();
            check_spawn_shape_registration();
            check_spawn_trigger_and_lifecycle();
            check_spawn_render_registration();
            check_spawn_model_anchors();
            check_spawn_bind_tree();
            check_spawn_live_attachment_release();
            check_spawn_sound_pose();
            check_spawn_effect_allocation();
            check_spawn_effect_stream();
            check_actor_loading();
            std::cout << "PASS: actor spawn eligibility, swap setup dispatch and asynchronous script gate\n";
            return 0;
        }
        gbtg::GfxConfig config;
        config.visible = false;
        config.width = argc == 4 && !player_test ? std::stoi(argv[2]) : 640;
        config.height = argc == 4 && !player_test ? std::stoi(argv[3]) : 480;
        // GBTG_WIDESCREEN=1: the [Video] Widescreen setting (DAT_00509703, the presenter's 16:9 frame) in an 854x480
        // target.
        if (const char* widescreen = std::getenv("GBTG_WIDESCREEN"); widescreen != nullptr && widescreen[0] == '1')
        {
            config.widescreen = true;
            config.width = 854;
            gbtg::entity_detail::g_video_widescreen = 1;
            std::cout << "widescreen\n";
        }
#if !defined(GBTG_RETAIL_GAME_ONLY)
        // GBTG_WIDE_ASPECT=<ratio> with GBTG_WIDESCREEN=1: the wide_aspect plugin's row (camera/wide_aspect.h) in a
        // target 480 high and that ratio wide.
        // GBTG_HUD_ASPECT=<ratio>: its HUD shape (0 stretches, as retail).
        const char* wide = std::getenv("GBTG_WIDE_ASPECT");
        const char* hud = std::getenv("GBTG_HUD_ASPECT");
        if ((wide != nullptr || hud != nullptr) && config.widescreen)
        {
            const float ratio = wide != nullptr ? std::strtof(wide, nullptr) : 16.f / 9.f;
            config.width = int(480.f * ratio + 0.5f);
            gbtg::request_wide_aspect(ratio);
            if (hud != nullptr)
            {
                gbtg::request_hud_aspect(std::strtof(hud, nullptr));
            }
            config.frame_aspect = gbtg::apply_wide_aspect(config.width, config.height, true);
            std::cout << "wide aspect " << config.frame_aspect << " hud " << (hud != nullptr ? hud : "stretch") << "\n";
        }
#endif
        // GBTG_GFX picks a compiled backend (d3d11, vulkan or null), as gbtg's --gfx does.
        if (const char* backend = std::getenv("GBTG_GFX"); backend != nullptr)
        {
            gbtg::gfx_select_backend(backend);
        }
        if (!gbtg::gfx_init(config))
        {
            throw gbtg::Error("test graphics initialization failed");
        }
        std::cout << "gfx " << gbtg::gfx_backend_name() << "\n";
        if (player_test)
        {
            // Its unit checks build their own factory owners, so they run before the game's boot-owned services exist.
            gbtg::AidStore aids;
            aids.open(argv[1]);
            check_player_dialogue_resources(aids);
        }
        gbtg::Game game;
        if (replay_scene_test || scene6_save_test)
        {
            write_replay_save(argv[0], argv[1], replay_paging_test ? "aid_script_ghoulies_chapter1_scene10_1playcam"
                : "aid_script_ghoulies_chapter1_scene2_1viewcam");
        }
        else if (pause_quit_test)
        {
            write_replay_save(argv[0], argv[1], "aid_script_ghoulies_chapter1_scene3_1playcam");
        }
        else if (chapter5_finale_test)
        {
            write_replay_save(argv[0], argv[1], "aid_script_ghoulies_chapter5_scene13_1book");
        }
        else if (chapter5_save_test)
        {
            write_replay_save(argv[0], argv[1], "aid_script_ghoulies_chapter5_scene1_1playcam");
        }
        else if (scene6_reload_test)
        {
            if (saved_challenge_byte() == 0xFF || (saved_challenge_byte() & 6) != 6)
            {
                throw gbtg::Error("--scene6-save-reload needs --scene6-save's save with challenges 1 and 2");
            }
            set_saved_script("aid_script_ghoulies_chapter1_scene3_1playcam");
        }
        else
        {
            // Each run starts with an empty U: (the save folder), so earlier runs' saves never change the screens.
            std::filesystem::remove_all(test_save_folder());
        }
        if (retail_save_test || challenges_test || feature_test || corrupt_save_test)
        {
            const char* from = std::getenv("GBTG_RETAIL_SAVES");
            const std::filesystem::path source = from != nullptr ? from : "../Saves/Xbox/4d530053";
            if (!std::filesystem::is_directory(source))
            {
                throw gbtg::Error("no retail save folder at " + source.generic_string() + " (set GBTG_RETAIL_SAVES)");
            }
            std::filesystem::create_directories(test_save_folder());
            std::filesystem::copy(source, test_save_folder(), std::filesystem::copy_options::recursive);
            std::cout << "copied the retail saves from " << source.generic_string() << "\n";
            if (corrupt_save_test)
            {
                // Flip bytes past the signature so the verify (0x000E9B60) fails.
                for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
                {
                    if (entry.path().filename() != "progress.xsv")
                    {
                        continue;
                    }
                    std::fstream file(entry.path(), std::ios::in | std::ios::out | std::ios::binary);
                    file.seekg(0x40);
                    char bytes[16]{};
                    file.read(bytes, sizeof(bytes));
                    for (char& b : bytes)
                    {
                        b = char(~b);
                    }
                    file.seekp(0x40);
                    file.write(bytes, sizeof(bytes));
                    std::cout << "damaged " << entry.path().generic_string() << "\n";
                }
            }
        }
        if (loading_test && std::filesystem::is_directory(gbtg::utility_root()))
        {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(gbtg::utility_root()))
            {
                const auto name = entry.path().filename().string();
                if (entry.is_regular_file() && (name == "02110fd0" || name == "02110fd1"))
                {
                    std::filesystem::remove(entry.path());
                    std::cout << "removed the cached wavebank " << entry.path().generic_string() << "\n";
                }
            }
        }
        gbtg::set_save_root(test_save_folder());
        game.init(argv[1], scene6_save_test || scene6_reload_test || pause_quit_test || chapter5_save_test || chapter5_finale_test ? "" : scene6_test ? "6" : scene3_test || pause_test || pause_map_test ? "7"
            : scene4_test ? "chapter1_scene4_1book" : scene5_test ? "chapter1_scene5_1playcam"
            : scene6_1_test ? "chapter1_scene6_1playcam"
            : scene7_test ? "chapter1_scene7_1playcam"
            : scene8_test ? "chapter1_scene8_1playcam"
            : scene9_test ? "chapter1_scene9_1playcam"
            : scene10_test ? "chapter1_scene10_1playcam"
            : scene11_test ? "chapter1_scene11_2book"
            : scene12_test ? "chapter1_scene12_1playcam"
            : scene2a1_test ? "chapter2a_scene1_1playcam"
            : scene2a1b_test ? "chapter2a_scene1_2book"
            : scene2a2_test || scene2a2_reaper_test ? "chapter2a_scene2_1playcam"
            : scene2a3_test ? "chapter2a_scene3_1playcam"
            : scene2a4_test ? "chapter2a_scene4_2playcam"
            : scene2a5_test ? "chapter2a_scene5_1playcam"
            : scene2a6_test ? "chapter2a_scene6_1book"
            : scene2a7_test ? "chapter2a_scene7_2playcam"
            : scene2a8_test ? "chapter2a_scene8_1playcam"
            : scene2a9_test ? "chapter2a_scene9_1playcam"
            : scene2a10_test ? "chapter2a_scene10_1playcam"
            : scene2a11_test ? "chapter2a_scene11_1playcam"
            : scene2a12_test ? "chapter2a_scene12_2playcam"
            : scene2a13_test ? "chapter2a_scene13_2playcam"
            : scene2a14_test ? "chapter2a_scene14_1playcam"
            : scene2a15_test ? "chapter2a_scene15_1playcam"
            : scene2a16_test ? "chapter2a_scene16_1playcam"
            : scene2a17_test ? "chapter2a_scene17_1playcam"
            : scene2b1_test ? "chapter2b_scene1_1playcam"
            : scene2b2_test ? "chapter2b_scene2_1playcam"
            : scene2b3_test ? "chapter2b_scene3_2playcam"
            : scene2b4_test ? "chapter2b_scene4_1playcam"
            : scene2b4b_test ? "chapter2b_scene4_2book"
            : scene2b5_test ? "chapter2b_scene5_1playcam"
            : scene2b6_test ? "chapter2b_scene6_1book"
            : scene2b6b_test ? "chapter2b_scene6_2playcam"
            : scene2b7_test ? "chapter2b_scene7_1playcam"
            : scene2b8_test ? "chapter2b_scene8_1playcam"
            : scene2b9_test ? "chapter2b_scene9_1playcam"
            : scene2b10_test ? "chapter2b_scene10_1playcam"
            : scene2b10b_test ? "chapter2b_scene10_2book"
            : scene2c1_test ? "chapter2c_scene1_1playcam"
            : scene2c2_test ? "chapter2c_scene2_1playcam"
            : scene2c3_test ? "chapter2c_scene3_1playcam"
            : scene2c4_test ? "chapter2c_scene4_1book"
            : scene2c4b_test ? "chapter2c_scene4_2playcam"
            : scene2c5_test ? "chapter2c_scene5_1playcam"
            : scene2c6_test ? "chapter2c_scene6_1playcam"
            : scene2c7_test ? "chapter2c_scene7_1playcam"
            : scene2c8_test ? "chapter2c_scene8_1playcam"
            : scene2c9_test ? "chapter2c_scene9_1playcam"
            : scene2c10_test ? "chapter2c_scene10_1playcam"
            : scene2c11_test ? "chapter2c_scene11_1playcam"
            : scene2c11b_test ? "chapter2c_scene11_2book"
            : scene2c12_test ? "chapter2c_scene12_1playcam"
            : scene2c13_test ? "chapter2c_scene13_1book"
            : scene2c13b_test ? "chapter2c_scene13_2playcam"
            : scene2c13c_test ? "chapter2c_scene13_3book"
            : scene3a1_test ? "chapter3a_scene1_2book"
            : scene3a1b_test ? "chapter3a_scene1_3playcam"
            : scene3a2_test ? "chapter3a_scene2_1playcam"
            : scene3a3_test ? "chapter3a_scene3_1playcam"
            : scene3a3b_test ? "chapter3a_scene3_2book"
            : scene3a3c_test ? "chapter3a_scene3_3playcam"
            : scene3a4_test ? "chapter3a_scene4_1playcam"
            : scene3a5_test ? "chapter3a_scene5_1playcam"
            : scene3a6_test ? "chapter3a_scene6_1playcam"
            : scene3a7_test ? "chapter3a_scene7_1playcam"
            : scene3a8_test ? "chapter3a_scene8_1playcam"
            : scene3a9_test ? "chapter3a_scene9_1playcam"
            : scene3a10_test ? "chapter3a_scene10_2playcam"
            : scene3a11_test ? "chapter3a_scene11_1playcam"
            : scene3a12_test ? "chapter3a_scene12_1playcam"
            : scene3b1_test ? "chapter3b_scene1_1playcam"
            : scene3b2_test ? "chapter3b_scene2_1playcam"
            : scene3b3_test ? "chapter3b_scene3_1playcam"
            : scene3b4_test ? "chapter3b_scene4_2playcam"
            : scene3b5_test ? "chapter3b_scene5_2playcam"
            : scene3b6_test ? "chapter3b_scene6_1playcam"
            : scene3b7_test ? "chapter3b_scene7_1playcam"
            : scene3c1_test ? "chapter3c_scene1_1playcam"
            : scene3c2_test ? "chapter3c_scene2_1playcam"
            : scene3c3_test ? "chapter3c_scene3_2playcam"
            : scene3c4_test ? "chapter3c_scene4_1playcam"
            : scene3c5_test ? "chapter3c_scene5_1playcam"
            : scene3c6_test ? "chapter3c_scene6_1playcam"
            : scene3c7_test ? "chapter3c_scene7_1playcam"
            : scene3c8_test ? "chapter3c_scene8_1playcam"
            : scene3c9_test ? "chapter3c_scene9_1playcam"
            : scene3c10_test ? "chapter3c_scene10_1playcam"
            : scene3c11_test ? "chapter3c_scene11_1playcam"
            : scene3c12_test ? "chapter3c_scene12_1playcam"
            : scene3c13_test ? "chapter3c_scene13_2book"
            : scene3c13b_test ? "chapter3c_scene13_3playcam"
            : scene3c14_test ? "chapter3c_scene14_1playcam"
            : scene3c15_test ? "chapter3c_scene15_1playcam"
            : scene3c15b_test ? "chapter3c_scene15_2book"
            : scene3c15c_test ? "chapter3c_scene15_3playcam"
            : scene4_1_test ? "chapter4_scene1_1book"
            : scene4_1b_test ? "chapter4_scene1_1playcam"
            : scene4_1c_test ? "chapter4_scene1_2book"
            : scene5_1_test ? "chapter5_scene1_1playcam"
            : scene5_2_test || pause_rules_test ? "chapter5_scene2_1playcam"
            : scene5_3_test ? "chapter5_scene3_1playcam"
            : scene5_4_test ? "chapter5_scene4_1playcam"
            : scene5_5_test ? "chapter5_scene5_1playcam"
            : scene5_6_test ? "chapter5_scene6_1playcam"
            : scene5_7_test ? "chapter5_scene7_1playcam"
            : scene5_8_test ? "chapter5_scene8_1playcam"
            : scene5_9_test ? "chapter5_scene9_1playcam"
            : scene5_10_test ? "chapter5_scene10_1playcam"
            : scene5_11_test ? "chapter5_scene11_1playcam"
            : scene5_12_test ? "chapter5_scene12_1playcam"
            : scene5_13_test ? "chapter5_scene13_1playcam"
            : scene5_13b_test ? "chapter5_scene13_1book"
            : scene5_13c_test ? "chapter5_scene13_2playcam"
            : scene5_13d_test ? "chapter5_scene13_3book"
            : scenefinal_test ? "finalscore_finalscore"
            : scenecredits_test ? "credits_final"
            : player_test || spawn_test ? "chapter1_scene1_2playcam"
            : title_test ? "" : "scene0_1book");
        if (scene2a1b_test)
        {
            pass_chapter_title(game, "scene2a1b", "aid_script_ghoulies_chapter2a_scene1_2book", false);
            play_scene7(game, "scene2a1_2");
            pass_chapter_title(game, "scene2a1_to_3", "aid_script_ghoulies_chapter2a_scene1_3playcam", false);
            play_scene7(game, "scene2a1_3", 1000.f, "Event - Kill All", true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scenecredits_test)
        {
            pass_chapter_title(game, "scenecredits", "aid_script_ghoulies_credits_final", false);
            play_scene7(game, "scenecredits", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scenefinal_test)
        {
            pass_chapter_title(game, "scenefinal", "aid_script_ghoulies_finalscore_finalscore", false);
            play_scene7(game, "scenefinal", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_13d_test)
        {
            pass_chapter_title(game, "scene5_13d", "aid_script_ghoulies_chapter5_scene13_3book", false);
            play_scene7(game, "scene5_13d", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_13c_test)
        {
            pass_chapter_title(game, "scene5_13c", "aid_script_ghoulies_chapter5_scene13_2playcam", false);
            play_scene7(game, "scene5_13c", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_13b_test)
        {
            pass_chapter_title(game, "scene5_13b", "aid_script_ghoulies_chapter5_scene13_1book", false);
            play_scene7(game, "scene5_13b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_13_test)
        {
            pass_chapter_title(game, "scene5_13", "aid_script_ghoulies_chapter5_scene13_1playcam", false);
            play_scene7(game, "scene5_13", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_12_test)
        {
            pass_chapter_title(game, "scene5_12", "aid_script_ghoulies_chapter5_scene12_1playcam", false);
            play_scene7(game, "scene5_12", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_11_test)
        {
            pass_chapter_title(game, "scene5_11", "aid_script_ghoulies_chapter5_scene11_1playcam", false);
            play_scene7(game, "scene5_11", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_10_test)
        {
            pass_chapter_title(game, "scene5_10", "aid_script_ghoulies_chapter5_scene10_1playcam", false);
            play_scene7(game, "scene5_10", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_9_test)
        {
            pass_chapter_title(game, "scene5_9", "aid_script_ghoulies_chapter5_scene9_1playcam", false);
            play_scene7(game, "scene5_9", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_8_test)
        {
            pass_chapter_title(game, "scene5_8", "aid_script_ghoulies_chapter5_scene8_1playcam", false);
            play_scene7(game, "scene5_8", 1000.f, {}, true, true, {}, "aid_ghoulybox_single_skeleton", 0xFFFFFFFFu);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_7_test)
        {
            pass_chapter_title(game, "scene5_7", "aid_script_ghoulies_chapter5_scene7_1playcam", false);
            play_scene7(game, "scene5_7", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_6_test)
        {
            pass_chapter_title(game, "scene5_6", "aid_script_ghoulies_chapter5_scene6_1playcam", false);
            play_scene7(game, "scene5_6", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_5_test)
        {
            pass_chapter_title(game, "scene5_5", "aid_script_ghoulies_chapter5_scene5_1playcam", false);
            play_scene7(game, "scene5_5", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_4_test)
        {
            pass_chapter_title(game, "scene5_4", "aid_script_ghoulies_chapter5_scene4_1playcam", false);
            play_scene7(game, "scene5_4", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_3_test)
        {
            pass_chapter_title(game, "scene5_3", "aid_script_ghoulies_chapter5_scene3_1playcam", false);
            play_scene7(game, "scene5_3", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_2_test)
        {
            pass_chapter_title(game, "scene5_2", "aid_script_ghoulies_chapter5_scene2_1playcam", false);
            play_scene7(game, "scene5_2", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_1_test)
        {
            pass_chapter_title(game, "scene5_1", "aid_script_ghoulies_chapter5_scene1_1playcam", false);
            play_scene7(game, "scene5_1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene4_1c_test)
        {
            pass_chapter_title(game, "scene4_1c", "aid_script_ghoulies_chapter4_scene1_2book", false);
            play_scene7(game, "scene4_1c", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene4_1b_test)
        {
            pass_chapter_title(game, "scene4_1b", "aid_script_ghoulies_chapter4_scene1_1playcam", false);
            play_scene7(game, "scene4_1b", 1000.f, {}, true, true, {}, {}, 0, false, 0, 16.f, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene4_1_test)
        {
            pass_chapter_title(game, "scene4_1", "aid_script_ghoulies_chapter4_scene1_1book", false);
            play_scene7(game, "scene4_1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c15c_test)
        {
            pass_chapter_title(game, "scene3c15c", "aid_script_ghoulies_chapter3c_scene15_3playcam", false);
            play_scene7(game, "scene3c15c", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c15b_test)
        {
            pass_chapter_title(game, "scene3c15b", "aid_script_ghoulies_chapter3c_scene15_2book", false);
            play_scene7(game, "scene3c15b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c15_test)
        {
            pass_chapter_title(game, "scene3c15", "aid_script_ghoulies_chapter3c_scene15_1playcam", false);
            play_scene7(game, "scene3c15", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c14_test)
        {
            pass_chapter_title(game, "scene3c14", "aid_script_ghoulies_chapter3c_scene14_1playcam", false);
            play_scene7(game, "scene3c14", 1000.f, {}, true, true, {}, "aid_ghoulybox_powerups_supersoups_energyboost5", 3, false, 0, 60.f);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c13b_test)
        {
            pass_chapter_title(game, "scene3c13b", "aid_script_ghoulies_chapter3c_scene13_3playcam", false);
            play_scene7(game, "scene3c13b", 1000.f, {}, true, true, {}, "aid_ghoulybox_powerups_supersoups_onehitkill", 8);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c13_test)
        {
            pass_chapter_title(game, "scene3c13", "aid_script_ghoulies_chapter3c_scene13_2book", false);
            play_scene7(game, "scene3c13", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c12_test)
        {
            pass_chapter_title(game, "scene3c12", "aid_script_ghoulies_chapter3c_scene12_1playcam", false);
            play_scene7(game, "scene3c12", 1000.f, {}, true, true, "free play", "aid_ghoulybox_single_warlock", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c11_test)
        {
            pass_chapter_title(game, "scene3c11", "aid_script_ghoulies_chapter3c_scene11_1playcam", false);
            play_scene7(game, "scene3c11", 1000.f, {}, true, true, {}, {}, 0, false, 0, 60.f);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c10_test)
        {
            pass_chapter_title(game, "scene3c10", "aid_script_ghoulies_chapter3c_scene10_1playcam", false);
            play_scene7(game, "scene3c10", 0.f, {}, true, true, {}, "aid_ghoulybox_powerups_supersoups_energyboost1", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c9_test)
        {
            pass_chapter_title(game, "scene3c9", "aid_script_ghoulies_chapter3c_scene9_1playcam", false);
            play_scene7(game, "scene3c9", 1000.f, {}, true, true, "event - one type(random)/time limit", {}, 0,
                false, 0xF);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c8_test)
        {
            pass_chapter_title(game, "scene3c8", "aid_script_ghoulies_chapter3c_scene8_1playcam", false);
            play_scene7(game, "scene3c8", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c7_test)
        {
            pass_chapter_title(game, "scene3c7", "aid_script_ghoulies_chapter3c_scene7_1playcam", false);
            play_scene7(game, "scene3c7", 1000.f, {}, true, true, {}, "aid_ghoulybox_single_warlock", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c6_test)
        {
            pass_chapter_title(game, "scene3c6", "aid_script_ghoulies_chapter3c_scene6_1playcam", false);
            play_scene7(game, "scene3c6", 0.f, {}, true, true, {}, "aid_ghoulybox_powerups_supersoups_energyboost5", 3);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c5_test)
        {
            pass_chapter_title(game, "scene3c5", "aid_script_ghoulies_chapter3c_scene5_1playcam", false);
            play_scene7(game, "scene3c5", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c4_test)
        {
            pass_chapter_title(game, "scene3c4", "aid_script_ghoulies_chapter3c_scene4_1playcam", false);
            play_scene7(game, "scene3c4", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c3_test)
        {
            pass_chapter_title(game, "scene3c3", "aid_script_ghoulies_chapter3c_scene3_2playcam", false);
            play_scene7(game, "scene3c3", 50.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c2_test)
        {
            pass_chapter_title(game, "scene3c2", "aid_script_ghoulies_chapter3c_scene2_1playcam", false);
            play_scene7(game, "scene3c2", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3c1_test)
        {
            pass_chapter_title(game, "scene3c1", "aid_script_ghoulies_chapter3c_scene1_1playcam", false);
            play_scene7(game, "scene3c1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b7_test)
        {
            pass_chapter_title(game, "scene3b7", "aid_script_ghoulies_chapter3b_scene7_1playcam", false);
            play_scene7(game, "scene3b7", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b6_test)
        {
            pass_chapter_title(game, "scene3b6", "aid_script_ghoulies_chapter3b_scene6_1playcam", false);
            play_scene7(game, "scene3b6", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b5_test)
        {
            pass_chapter_title(game, "scene3b5", "aid_script_ghoulies_chapter3b_scene5_2playcam", false);
            play_scene7(game, "scene3b5", 0.f, {}, true, true, {}, "aid_ghoulybox_powerups_supersoups_energyboost5", 3);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b4_test)
        {
            pass_chapter_title(game, "scene3b4", "aid_script_ghoulies_chapter3b_scene4_2playcam", false);
            play_scene7(game, "scene3b4", 1000.f, "event - survive/no weapons", true, true, {},
                "aid_ghoulybox_powerups_supersoups_energyboost5", 3);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b3_test)
        {
            pass_chapter_title(game, "scene3b3", "aid_script_ghoulies_chapter3b_scene3_1playcam", false);
            play_scene7(game, "scene3b3", 1000.f, {}, true, true, {}, "aid_ghoulybox_single_warlock", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b2_test)
        {
            pass_chapter_title(game, "scene3b2", "aid_script_ghoulies_chapter3b_scene2_1playcam", false);
            play_scene7(game, "scene3b2", 50.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3b1_test)
        {
            pass_chapter_title(game, "scene3b1", "aid_script_ghoulies_chapter3b_scene1_1playcam", false);
            play_scene7(game, "scene3b1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a12_test)
        {
            pass_chapter_title(game, "scene3a12", "aid_script_ghoulies_chapter3a_scene12_1playcam", false);
            play_scene7(game, "scene3a12", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a11_test)
        {
            pass_chapter_title(game, "scene3a11", "aid_script_ghoulies_chapter3a_scene11_1playcam", false);
            play_scene7(game, "scene3a11", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a10_test)
        {
            pass_chapter_title(game, "scene3a10", "aid_script_ghoulies_chapter3a_scene10_2playcam", false);
            play_scene7(game, "scene3a10", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a9_test)
        {
            pass_chapter_title(game, "scene3a9", "aid_script_ghoulies_chapter3a_scene9_1playcam", false);
            play_scene7(game, "scene3a9", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a8_test)
        {
            pass_chapter_title(game, "scene3a8", "aid_script_ghoulies_chapter3a_scene8_1playcam", false);
            play_scene7(game, "scene3a8", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a7_test)
        {
            pass_chapter_title(game, "scene3a7", "aid_script_ghoulies_chapter3a_scene7_1playcam", false);
            play_scene7(game, "scene3a7", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a6_test)
        {
            pass_chapter_title(game, "scene3a6", "aid_script_ghoulies_chapter3a_scene6_1playcam", false);
            play_scene7(game, "scene3a6", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a5_test)
        {
            pass_chapter_title(game, "scene3a5", "aid_script_ghoulies_chapter3a_scene5_1playcam", false);
            play_scene7(game, "scene3a5", 1000.f, {}, true, true, "free play");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a4_test)
        {
            pass_chapter_title(game, "scene3a4", "aid_script_ghoulies_chapter3a_scene4_1playcam", false);
            play_scene7(game, "scene3a4", 1000.f, {}, true, true, {}, "aid_ghoulybox_ghoulies_chapter3a_warlockscene4", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a3c_test)
        {
            pass_chapter_title(game, "scene3a3c", "aid_script_ghoulies_chapter3a_scene3_3playcam", false);
            play_scene7(game, "scene3a3c", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a3b_test)
        {
            pass_chapter_title(game, "scene3a3b", "aid_script_ghoulies_chapter3a_scene3_2book", false);
            play_scene7(game, "scene3a3b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a3_test)
        {
            pass_chapter_title(game, "scene3a3", "aid_script_ghoulies_chapter3a_scene3_1playcam", false);
            play_scene7(game, "scene3a3", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a2_test)
        {
            pass_chapter_title(game, "scene3a2", "aid_script_ghoulies_chapter3a_scene2_1playcam", false);
            play_scene7(game, "scene3a2", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a1b_test)
        {
            pass_chapter_title(game, "scene3a1b", "aid_script_ghoulies_chapter3a_scene1_3playcam", false);
            play_scene7(game, "scene3a1b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3a1_test)
        {
            pass_chapter_title(game, "scene3a1", "aid_script_ghoulies_chapter3a_scene1_2book", false);
            play_scene7(game, "scene3a1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c13c_test)
        {
            pass_chapter_title(game, "scene2c13c", "aid_script_ghoulies_chapter2c_scene13_3book", false);
            play_scene7(game, "scene2c13c", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c13b_test)
        {
            pass_chapter_title(game, "scene2c13b", "aid_script_ghoulies_chapter2c_scene13_2playcam", false);
            play_scene7(game, "scene2c13b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c13_test)
        {
            pass_chapter_title(game, "scene2c13", "aid_script_ghoulies_chapter2c_scene13_1book", false);
            play_scene7(game, "scene2c13", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c12_test)
        {
            pass_chapter_title(game, "scene2c12", "aid_script_ghoulies_chapter2c_scene12_1playcam", false);
            play_scene7(game, "scene2c12", 1000.f, {}, true, true, {},
                "aid_ghoulybox_ghoulies_chapter2_winercellarkeyholder", 0);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c11b_test)
        {
            pass_chapter_title(game, "scene2c11b", "aid_script_ghoulies_chapter2c_scene11_2book", false);
            play_scene7(game, "scene2c11b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c11_test)
        {
            pass_chapter_title(game, "scene2c11", "aid_script_ghoulies_chapter2c_scene11_1playcam", false);
            play_scene7(game, "scene2c11", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c10_test)
        {
            pass_chapter_title(game, "scene2c10", "aid_script_ghoulies_chapter2c_scene10_1playcam", false);
            play_scene7(game, "scene2c10", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c9_test)
        {
            pass_chapter_title(game, "scene2c9", "aid_script_ghoulies_chapter2c_scene9_1playcam", false);
            play_scene7(game, "scene2c9", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c8_test)
        {
            pass_chapter_title(game, "scene2c8", "aid_script_ghoulies_chapter2c_scene8_1playcam", false);
            play_scene7(game, "scene2c8", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c7_test)
        {
            pass_chapter_title(game, "scene2c7", "aid_script_ghoulies_chapter2c_scene7_1playcam", false);
            play_scene7(game, "scene2c7", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c6_test)
        {
            pass_chapter_title(game, "scene2c6", "aid_script_ghoulies_chapter2c_scene6_1playcam", false);
            // The study: the event arms one of the three bg-break keys and the driver punches its part open; the exit
            // door's opening lets the ninja imps in, so FREE PLAY is fought before Cooper walks out.
            play_scene7(game, "scene2c6", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c5_test)
        {
            pass_chapter_title(game, "scene2c5", "aid_script_ghoulies_chapter2c_scene5_1playcam", false);
            // The pirate has his skeletons dig for the dungweed: a skeleton hit (PlayerHitGhouly) turns him and the
            // garden's ghoulies on Cooper, so while that is watched Cooper fights nothing (no section is titled) and
            // walks into the dungweed's region (PlayerEntersRegion); the ghoulies that come after are fought.
            play_scene7(game, "scene2c5", 1000.f, {}, true, true, "FREE PLAY", {}, 0, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c4b_test)
        {
            pass_chapter_title(game, "scene2c4b", "aid_script_ghoulies_chapter2c_scene4_2playcam", false);
            play_scene7(game, "scene2c4b", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c4_test)
        {
            pass_chapter_title(game, "scene2c4", "aid_script_ghoulies_chapter2c_scene4_1book", false);
            play_scene7(game, "scene2c4", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c3_test)
        {
            pass_chapter_title(game, "scene2c3", "aid_script_ghoulies_chapter2c_scene3_1playcam", false);
            play_scene7(game, "scene2c3", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c2_test)
        {
            pass_chapter_title(game, "scene2c2", "aid_script_ghoulies_chapter2c_scene2_1playcam", false);
            // The invisibility in the suitcase by the window (bg-break setup, part 7) lets Cooper leave unseen.
            play_scene7(game, "scene2c2", 1000.f, {}, true, true, "FREE PLAY",
                "aid_ghoulybox_powerups_supersoups_invisibility", 9);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2c1_test)
        {
            pass_chapter_title(game, "scene2c1", "aid_script_ghoulies_chapter2c_scene1_1playcam", false);
            play_scene7(game, "scene2c1", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b10b_test)
        {
            pass_chapter_title(game, "scene2b10b", "aid_script_ghoulies_chapter2b_scene10_2book", false);
            play_scene7(game, "scene2b10b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b10_test)
        {
            pass_chapter_title(game, "scene2b10", "aid_script_ghoulies_chapter2b_scene10_1playcam", false);
            play_scene7(game, "scene2b10", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b9_test)
        {
            pass_chapter_title(game, "scene2b9", "aid_script_ghoulies_chapter2b_scene9_1playcam", false);
            // As in scene 7, free play leaves the room's ghoulies (the haunted doors still stand after their event)
            // and walks to the exit.
            play_scene7(game, "scene2b9", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b8_test)
        {
            pass_chapter_title(game, "scene2b8", "aid_script_ghoulies_chapter2b_scene8_1playcam", false);
            play_scene7(game, "scene2b8", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b7_test)
        {
            pass_chapter_title(game, "scene2b7", "aid_script_ghoulies_chapter2b_scene7_1playcam", false);
            // Free play's mummies keep coming from the workshop spawn; the driver walks to the exit rather than fight
            // them.
            play_scene7(game, "scene2b7", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b6_test)
        {
            pass_chapter_title(game, "scene2b6", "aid_script_ghoulies_chapter2b_scene6_1book", false);
            play_scene7(game, "scene2b6", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b6b_test)
        {
            pass_chapter_title(game, "scene2b6b", "aid_script_ghoulies_chapter2b_scene6_2playcam", false);
            play_scene7(game, "scene2b6b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b5_test)
        {
            pass_chapter_title(game, "scene2b5", "aid_script_ghoulies_chapter2b_scene5_1playcam", false);
            play_scene7(game, "scene2b5", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b4b_test)
        {
            pass_chapter_title(game, "scene2b4b", "aid_script_ghoulies_chapter2b_scene4_2book", false);
            play_scene7(game, "scene2b4b", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b4_test)
        {
            pass_chapter_title(game, "scene2b4", "aid_script_ghoulies_chapter2b_scene4_1playcam", false);
            play_scene7(game, "scene2b4", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b3_test)
        {
            pass_chapter_title(game, "scene2b3", "aid_script_ghoulies_chapter2b_scene3_2playcam", false);
            play_scene7(game, "scene2b3", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b2_test)
        {
            pass_chapter_title(game, "scene2b2", "aid_script_ghoulies_chapter2b_scene2_1playcam", false);
            play_scene7(game, "scene2b2", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2b1_test)
        {
            pass_chapter_title(game, "scene2b1", "aid_script_ghoulies_chapter2b_scene1_1playcam", false);
            play_scene7(game, "scene2b1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a17_test)
        {
            pass_chapter_title(game, "scene2a17", "aid_script_ghoulies_chapter2a_scene17_1playcam", false);
            play_scene7(game, "scene2a17", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a16_test)
        {
            pass_chapter_title(game, "scene2a16", "aid_script_ghoulies_chapter2a_scene16_1playcam", false);
            play_scene7(game, "scene2a16", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a15_test)
        {
            pass_chapter_title(game, "scene2a15", "aid_script_ghoulies_chapter2a_scene15_1playcam", false);
            play_scene7(game, "scene2a15", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a14_test)
        {
            pass_chapter_title(game, "scene2a14", "aid_script_ghoulies_chapter2a_scene14_1playcam", false);
            play_scene7(game, "scene2a14", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a13_test)
        {
            pass_chapter_title(game, "scene2a13", "aid_script_ghoulies_chapter2a_scene13_2playcam", false);
            play_scene7(game, "scene2a13", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a12_test)
        {
            pass_chapter_title(game, "scene2a12", "aid_script_ghoulies_chapter2a_scene12_2playcam", false);
            play_scene7(game, "scene2a12", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a11_test)
        {
            pass_chapter_title(game, "scene2a11", "aid_script_ghoulies_chapter2a_scene11_1playcam", false);
            play_scene7(game, "scene2a11", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a10_test)
        {
            pass_chapter_title(game, "scene2a10", "aid_script_ghoulies_chapter2a_scene10_1playcam", false);
            play_scene7(game, "scene2a10", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a9_test)
        {
            pass_chapter_title(game, "scene2a9", "aid_script_ghoulies_chapter2a_scene9_1playcam", false);
            play_scene7(game, "scene2a9", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a8_test)
        {
            pass_chapter_title(game, "scene2a8", "aid_script_ghoulies_chapter2a_scene8_1playcam", false);
            play_scene7(game, "scene2a8", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a7_test)
        {
            pass_chapter_title(game, "scene2a7", "aid_script_ghoulies_chapter2a_scene7_2playcam", false);
            play_scene7(game, "scene2a7", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a6_test)
        {
            pass_chapter_title(game, "scene2a6", "aid_script_ghoulies_chapter2a_scene6_1book", false);
            play_scene7(game, "scene2a6_1");
            pass_chapter_title(game, "scene2a6_to_2", "aid_script_ghoulies_chapter2a_scene6_2playcam", false);
            play_scene7(game, "scene2a6_2", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a5_test)
        {
            pass_chapter_title(game, "scene2a5", "aid_script_ghoulies_chapter2a_scene5_1playcam", false);
            // Free play ends at door 0xF (or on a kill); the driver walks to the door rather than fight the ninja imp.
            play_scene7(game, "scene2a5", 1000.f, {}, true, true, "FREE PLAY");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a4_test)
        {
            pass_chapter_title(game, "scene2a4", "aid_script_ghoulies_chapter2a_scene4_2playcam", false);
            // The first free play ends on region 11, through the balcony gate the script opens, and the second at the
            // exit; the dancing skeletons are not fought in either.
            play_scene7(game, "scene2a4", 1000.f, {}, true, true, "free play");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a3_test)
        {
            pass_chapter_title(game, "scene2a3", "aid_script_ghoulies_chapter2a_scene3_1playcam", false);
            play_scene7(game, "scene2a3", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a2_reaper_test)
        {
            pass_chapter_title(game, "scene2a2r", "aid_script_ghoulies_chapter2a_scene2_1playcam", false);
            play_scene7(game, "scene2a2r", 0.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a2_test)
        {
            pass_chapter_title(game, "scene2a2", "aid_script_ghoulies_chapter2a_scene2_1playcam", false);
            play_scene7(game, "scene2a2", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene2a1_test)
        {
            pass_chapter_title(game, "scene2a1", "aid_script_ghoulies_chapter2a_scene1_1playcam", false);
            play_scene7(game, "scene2a1", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene12_test)
        {
            pass_chapter_title(game, "scene12", "aid_script_ghoulies_chapter1_scene12_1playcam", false);
            play_scene7(game, "scene12", 1000.f, {}, true, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene11_test)
        {
            pass_chapter_title(game, "scene11", "aid_script_ghoulies_chapter1_scene11_2book", false);
            play_scene7(game, "scene11_2");
            pass_chapter_title(game, "scene11_to_3", "aid_script_ghoulies_chapter1_scene11_3viewcam", false);
            play_scene7(game, "scene11_3", 1000.f, {}, false, true);
            // The exit door's transition into scene 12: the screen must not stay black (a black screen that only a
            // restart clears was seen here).
            pass_chapter_title(game, "scene11_to_12", "aid_script_ghoulies_chapter1_scene12_1playcam", false);
            for (int frame = 0; frame < 300; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, {});
                if (frame % 60 == 0)
                {
                    capture_readback("cache/scene11_to_12_" + std::to_string(frame) + ".bmp");
                }
                gbtg::gfx_end_frame();
            }
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene10_test)
        {
            pass_chapter_title(game, "scene10", "aid_script_ghoulies_chapter1_scene10_1playcam", false);
            // "Event: Kill All" waits for a knockdown of the ghouly across the store; outside it nothing is fought.
            play_scene7(game, "scene10", 1000.f, "Event: Kill All", true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene9_test)
        {
            pass_chapter_title(game, "scene9", "aid_script_ghoulies_chapter1_scene9_1playcam", false);
            // The cellar's regions lie past the mummy's door, so Cooper heads for door 8 first.
            play_scene7(game, "scene9", 50.f, {}, true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene8_test)
        {
            pass_chapter_title(game, "scene8", "aid_script_ghoulies_chapter1_scene8_1playcam", false);
            // The haunted chairs wait round the whole room after a retry, so during the event any live one is fought;
            // outside it the room's chairs sleep until hit, so none is.
            play_scene7(game, "scene8", 1000.f, "Event 'Kill All'");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene7_test)
        {
            pass_chapter_title(game, "scene7", "aid_script_ghoulies_chapter1_scene7_1playcam", false);
            play_scene7(game, "scene7");
            pass_chapter_title(game, "scene7_to_2", "aid_script_ghoulies_chapter1_scene7_2book", false);
            play_scene7(game, "scene7_2");
            pass_chapter_title(game, "scene7_to_3", "aid_script_ghoulies_chapter1_scene7_3playcam", false);
            play_scene7(game, "scene7_3");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene6_1_test)
        {
            pass_chapter_title(game, "scene6_1", "aid_script_ghoulies_chapter1_scene6_1playcam", false);
            play_scene6_1(game, "scene6_1");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene5_test)
        {
            pass_chapter_title(game, "scene5", "aid_script_ghoulies_chapter1_scene5_1playcam", false);
            play_scene5(game, "scene5");
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene4_test)
        {
            pass_chapter_title(game, "scene4", "aid_script_ghoulies_chapter1_scene4_1book", false);
            play_scene4(game, "scene4");
            pass_chapter_title(game, "scene4_to_2", "aid_script_ghoulies_chapter1_scene4_2playcam", false);
            play_scene4(game, "scene4_2", true);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene6_reload_test)
        {
            start_saved_game(game, "scene6-save-reload");
            if ((gbtg::g_runtime_profile.challenges.bytes[0] & 6) != 6)
            {
                throw gbtg::Error("loading the save did not bring back challenges 1 and 2");
            }
            std::cout << "PASS: loading the saved game brings back challenges 1 and 2\n";
            pass_chapter_title(game, "scene3_reload", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            play_scene3(game, false, 0);
            std::cout << "PASS: the reloaded game's scene 3 starts without the rare book\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (pause_map_test)
        {
            pass_chapter_title(game, "pause_map", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            try
            {
                play_pause_map_run(game);
            }
            catch (const std::exception& error)
            {
                std::cout << "FAIL: " << error.what() << std::endl;
                std::_Exit(1);
            }
            gbtg::gfx_shutdown();
            return 0;
        }
        if (pause_rules_test)
        {
            pass_chapter_title(game, "pause_rules", "aid_script_ghoulies_chapter5_scene2_1playcam", false);
            try
            {
                play_pause_rules_run(game);
            }
            catch (const std::exception& error)
            {
                std::cout << "FAIL: " << error.what() << std::endl;
                std::_Exit(1);
            }
            gbtg::gfx_shutdown();
            return 0;
        }
        if (chapter5_finale_test)
        {
            start_saved_game(game, "chapter5-finale-save");
            int written = -1;
            for (int frame = 0; frame < 3000 && written < 0; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, {});
                const std::vector<char> bytes = saved_progress_file();
                gbtg::u32 finale = 0;
                if (bytes.size() >= 0x268)
                {
                    std::memcpy(&finale, bytes.data() + 0x264, 4);
                }
                if (finale != 0 && gbtg::g_save_scan->pending == 0)
                {
                    written = frame;
                }
            }
            const std::vector<char> bytes = saved_progress_file();
            if (written < 0)
            {
                std::cout << "FAIL: chapter5_scene13_1book's start set no +0x264 (scene " << game.scene_name << ")"
                          << std::endl;
                return 1;
            }
            gbtg::u32 gold = 0;
            std::memcpy(&gold, bytes.data() + 0x260, 4);
            std::cout << "PASS: chapter5_scene13_1book's start sets the save's +0x264 at frame " << written
                      << " (book scene " << int(gbtg::u8(bytes[0x25D])) << ", gold " << gold << ")" << std::endl;
            gbtg::gfx_shutdown();
            return 0;
        }
        if (chapter5_save_test)
        {
            start_saved_game(game, "chapter5-save");
            pass_chapter_title(game, "chapter5_save", "aid_script_ghoulies_chapter5_scene1_1playcam", false);
            const std::vector<char> before = saved_progress_file();
            play_scene7(game, "chapter5_save", 1000.f, {}, true, true);
            int written = -1;
            for (int frame = 0; frame < 3000 && written < 0; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, {});
                const std::vector<char> bytes = saved_progress_file();
                if (bytes.size() >= 0x268 && gbtg::u8(bytes[0x25D]) >= 2 && gbtg::g_save_scan->pending == 0)
                {
                    written = frame;
                }
            }
            const std::vector<char> bytes = saved_progress_file();
            if (written < 0 || before.size() < 0x268)
            {
                std::cout << "FAIL: scene 2's start wrote no chapter 5 book scene (scene " << game.scene_name
                          << ")" << std::endl;
                return 1;
            }
            const std::string script(bytes.data() + 0x102);
            const int reached = gbtg::u8(bytes[0x25D]);
            const bool times_kept = std::memcmp(before.data() + 0x18C, bytes.data() + 0x18C, 0x14) == 0;
            std::cout << "chapter5-save: scene " << game.scene_name << ", book scene " << reached << ", script "
                      << script << ", times " << (times_kept ? "kept" : "changed") << "\n";
            if (reached != 2 || script != "aid_script_ghoulies_chapter5_scene1_1playcam" || !times_kept)
            {
                std::cout << "FAIL: chapter 5's scene 2 save holds book scene " << reached << " and script " << script
                          << std::endl;
                return 1;
            }
            std::cout << "PASS: chapter 5's scene 2 start saves book scene 2 and keeps scene 1's script and the times\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (pause_quit_test)
        {
            start_saved_game(game, "pause-quit-save");
            pass_chapter_title(game, "pause_quit", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            try
            {
                play_pause_quit_run(game);
            }
            catch (const std::exception& error)
            {
                std::cout << "FAIL: " << error.what() << std::endl;
                std::_Exit(1);
            }
            gbtg::gfx_shutdown();
            return 0;
        }
        if (pause_test)
        {
            pass_chapter_title(game, "pause", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            play_pause(game);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene3_test)
        {
            pass_chapter_title(game, "scene3", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            play_scene3(game, true, 1);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (scene6_test)
        {
            const auto capture = capture_readback;
            if (scene6_save_test)
            {
                if (saved_challenge_byte() != 0)
                {
                    throw gbtg::Error("the new game's save already has challenge bits");
                }
                start_saved_game(game, "scene6-save");
            }
            pass_chapter_title(game, "scene6", "aid_script_ghoulies_chapter1_scene2_1viewcam");
            // The trophy-heads controller (factory type 0x92, entitySceneControlChapter1Scene8) runs the scene.
            if (gbtg::find_runtime_scene_control(0x92) == nullptr)
            {
                throw gbtg::Error("scene 6 has no trophy-heads control (factory type 0x92)");
            }
            // The heads' cutscene plays; a head whose cutscene row is live attacks (type 0x92 state 2), which spawns
            // Cooper and creates the Super Scary challenge (factory type 0x98). The driver leaves the first
            // challenge's buttons alone, so its heartbeats run out (state 10), and presses the shown buttons in
            // later ones (state 8). A closes Crivens' page after each; the heads' cutscene then resumes (state 1).
            std::string title = game.scene_name;
            const std::string scene_script = game.scene_script.name;
            int challenges = 0;
            int failures = 0;
            int successes = 0;
            int pages = 0;
            int resumed = 0;
            gbtg::u32 last_heads = 0xFFFFFFFFu;
            gbtg::u32 last_challenge = 0xFFFFFFFFu;
            bool page_up = false;
            int page_frame = -1;
            gbtg::u32 last_phase = 0xFFFFFFFFu;
            int phase_frames = 0;
            int moved_on = -1;
            for (int frame = 0; frame < 36000 && moved_on < 0; ++frame)
            {
                gbtg::InputState input;
                const auto* challenge = gbtg::find_runtime_scene_control(0x98);
                const gbtg::u32 phase =
                    challenge == nullptr ? 0xFFFFFFFFu : gbtg::u32(challenge->super_scary->phase);
                if (challenge != nullptr && (phase == 5 || phase == 6) && challenges > 1)
                {
                    // Cooper's press: the next button in the row, as A..Y (0x100 << button).
                    const gbtg::i32 presses = challenge->super_scary->presses;
                    const gbtg::u32 button =
                        challenge->super_scary->buttons[size_t(presses)];
                    if (frame % 8 < 3)
                    {
                        input.pads[0].buttons = gbtg::u16(0x100u << button);
                    }
                }
                const bool page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
                input.accept = input.accept || (page && frame % 30 < 5);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, input);
                challenge = gbtg::find_runtime_scene_control(0x98);
                const gbtg::u32 now =
                    challenge == nullptr ? 0xFFFFFFFFu : gbtg::u32(challenge->super_scary->phase);
                if (now != last_challenge)
                {
                    std::cout << "scene6 frame " << frame << ": Super Scary state " << int(now) << "\n";
                    if (now == 1)
                    {
                        ++challenges;
                    }
                    if (now == 8)
                    {
                        ++successes;
                    }
                    if (now == 10)
                    {
                        ++failures;
                    }
                    last_challenge = now;
                }
                if (now != 0xFFFFFFFFu)
                {
                    phase_frames = now == last_phase ? phase_frames + 1 : 0;
                    last_phase = now;
                    // Each challenge state 20 and 80 frames in, per challenge.
                    if (phase_frames == 20 || phase_frames == 80)
                    {
                        capture("cache/scene6_c" + std::to_string(challenges) + "_s" + std::to_string(now) + "_"
                            + std::to_string(phase_frames) + ".bmp");
                    }
                }
                if (const auto* heads = gbtg::find_runtime_scene_control(0x92); heads != nullptr)
                {
                    const gbtg::u32 state = heads->trophy_heads->phase;
                    if (state != last_heads)
                    {
                        std::cout << "scene6 frame " << frame << ": trophy heads state " << state << ", health "
                                  << heads->trophy_heads->health << ", attacks " << heads->trophy_heads->attacks << "\n";
                        resumed += state == 1 && last_heads != 0xFFFFFFFFu ? 1 : 0;
                        last_heads = state;
                    }
                }
                const bool page_now = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
                if (page_now && !page_up)
                {
                    ++pages;
                    page_frame = frame;
                }
                if (page_now && frame == page_frame + 20)
                {
                    capture("cache/scene6_page" + std::to_string(pages) + ".bmp");
                }
                page_up = page_now;
                if (game.scene_name != title)
                {
                    title = game.scene_name;
                    std::cout << "scene6 frame " << frame << ": title " << title << "\n";
                }
                if (frame % 600 == 0)
                {
                    capture("cache/scene6_" + std::to_string(frame) + ".bmp");
                }
                if (game.scene_script.name != scene_script)
                {
                    moved_on = frame;
                    std::cout << "scene6 frame " << frame << ": the script moves on to " << game.scene_script.name
                              << "\n";
                }
                gbtg::gfx_end_frame();
            }
            std::cout << "scene6: challenges " << challenges << ", failures " << failures << ", successes " << successes
                      << ", pages " << pages << ", heads resumed " << resumed << "\n";
            if (challenges == 0)
            {
                throw gbtg::Error("no trophy head attacked: the Super Scary challenge never started");
            }
            if (failures == 0 || pages == 0)
            {
                throw gbtg::Error("the unanswered challenge did not fail into Crivens' page");
            }
            if (challenges > 1 && successes == 0)
            {
                throw gbtg::Error("pressing the shown buttons did not win the Super Scary challenge");
            }
            if (resumed == 0)
            {
                throw gbtg::Error("the trophy heads' cutscene did not resume after a challenge");
            }
            std::cout << "PASS: scene 6's heads attack, the Super Scary challenge fails unanswered and is won by "
                         "the shown buttons, and the heads' cutscene resumes\n";
            if (moved_on < 0 || game.scene_script.name != "aid_script_ghoulies_chapter1_scene3_1playcam")
            {
                throw gbtg::Error("scene 6 moved on to " + game.scene_script.name);
            }
            // Scene 6's end runs the book transition into scene 3 ("Player Enters Room").
            pass_chapter_title(game, "scene6_to_3", "aid_script_ghoulies_chapter1_scene3_1playcam", false);
            if (scene6_save_test)
            {
                // Scene 3 is driven to its end: Cooper collects the rare book, whose award (its challenge 2,
                // 0x0010A6D0 -> 0x000F24C0) saves the game again.
                play_scene3(game, true, 1);
                // Each write waits for the save timer (0x0010DA80 / 0x0010F440); play on until the file has both bits.
                int written = -1;
                for (int frame = 0; frame < 1200 && written < 0; ++frame)
                {
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 60.f, {});
                    gbtg::gfx_end_frame();
                    if (frame % 30 == 29 && saved_challenge_byte() != 0xFF && (saved_challenge_byte() & 6) == 6)
                    {
                        written = frame;
                    }
                }
                std::cout << "scene6-save: challenge byte " << int(saved_challenge_byte()) << " in progress.xsv\n";
                if (written < 0)
                {
                    throw gbtg::Error("the game's save lacks challenge 1 (the trophy heads) or 2 (the rare book)");
                }
                // The file is signed as 0x0010EB60 signs it (0x000E9AF0, flags 0: the last 0x14 bytes), so the next
                // load (0x0010EF10) verifies it.
                for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
                {
                    if (!entry.is_regular_file() || entry.path().filename() != "progress.xsv")
                    {
                        continue;
                    }
                    std::ifstream in(entry.path(), std::ios::binary);
                    std::vector<gbtg::u8> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    const gbtg::u32 size = gbtg::xbox_signature_size(0);
                    std::vector<gbtg::u8> signature(size);
                    const std::uintptr_t handle = gbtg::begin_xbox_signature(0);
                    if (bytes.size() < size || handle == std::uintptr_t(-1)
                        || gbtg::update_xbox_signature(handle, bytes.data(), gbtg::u32(bytes.size() - size)) != 0
                        || gbtg::end_xbox_signature(handle, signature.data()) != 0
                        || std::memcmp(signature.data(), bytes.data() + (bytes.size() - size), size) != 0)
                    {
                        throw gbtg::Error("the saved progress.xsv's signature does not verify");
                    }
                }
                std::cout << "PASS: the trophy heads' and the rare book's awards save challenges 1 and 2 into the "
                             "saved game's progress.xsv\n";
                gbtg::gfx_shutdown();
                const std::string command = "\"\"" + std::string(argv[0]) + "\" \"" + argv[1]
                    + "\" --scene6-save-reload\"";
                if (std::system(command.c_str()) != 0)
                {
                    std::cout << "FAIL: the --scene6-save-reload run of the written save failed\n";
                    return 1;
                }
                return 0;
            }
            play_scene3(game, false);
            gbtg::gfx_shutdown();
            return 0;
        }
        if (spawn_test)
        {
            check_scene_spawn(game, scene_start_test);
            std::cout << "PASS: opening scene markers create the retail actors and player\n";
            return 0;
        }
        if (player_test)
        {
            bool saw_dialogue = false;
            bool saw_bat_hop = false;
            // DAT_00511B38's players (Cooper class 4, the imps class 23) with their strategy, body and actor words.
            const auto marked = [&game]()
            {
                return game.factory_scene->start_actors(game).marked_actors();
            };
            const auto strategy_state = [](const gbtg::RuntimeDirectorActor& mark)
            {
                return mark.strategy.empty() ? 0 : gbtg::strategy_base(mark.strategy).state;
            };
            // Script type 0x2F sends Cooper goal 7 (message 0x4D): his strategy holds in its goal state 0x3FB until
            // type 0x30 (message 0x32) releases him after the tutorial.
            bool cooper_locked = false;
            // The page's 0x00117400 sends 0x27 down DAT_00511B38: every imp's strategy waits in state 1000.
            bool imps_paused = false;
            // The native prompt activates after the opening transition: the scene tick
            // 0x0011E7F0 (and the script's 3-second timeout) runs only once 0x0011E4C0
            // leaves state 5. Leave enough live time for its detached bat's 4.5-second
            // idle period and hop.
            for (int frame = 0; frame < 720; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(player_step, {});
                gbtg::gfx_end_frame();
                saw_dialogue |= gbtg::g_flag_2474 != 0 && gbtg::g_draw_e1ec != 0;
                if (frame == 719)
                {
                    bool all_paused = true;
                    int imps_seen = 0;
                    for (const auto& mark : marked())
                    {
                        if (mark.actor->class_key == 4)
                        {
                            cooper_locked = strategy_state(mark) == 0x3FB;
                        }
                        else if (mark.actor->class_key == 23)
                        {
                            ++imps_seen;
                            all_paused &= strategy_state(mark) == 0x3E8;
                        }
                    }
                    imps_paused = all_paused && imps_seen > 0 && game.script_dialogue != nullptr
                        && game.script_dialogue->dialogue.state.mode != 0;
                }
                if (game.script_dialogue != nullptr)
                {
                    auto& active = game.script_dialogue->dialogue;
                    if (active.state.widget != nullptr && active.state.widget->bat != nullptr)
                    {
                        const auto bat = active.widget_update.bats(active.state.widget->bat);
                        saw_bat_hop |= bat.bat.state == 3 && bat.frame.elapsed > 0.f;
                    }
                }
            }
            if (game.scene_script.name != "aid_script_ghoulies_chapter1_scene1_2playcam")
            {
                throw gbtg::Error("player test loaded the wrong scene");
            }
            std::cout << "player opening: dialogue=" << saw_dialogue << "\n";
            for (const gbtg::u32 cursor : gbtg::g_script_fired)
            {
                auto node = gbtg::script_node_at(game.scene_script, cursor);
                std::cout << "player script waiting: offset=0x" << std::hex << cursor
                          << " type=0x" << (!node ? 0 : node->type) << std::dec << "\n";
            }
            gbtg::RgbaImage image;
            if (!gbtg::gfx_readback(image))
            {
                throw gbtg::Error("player opening readback failed");
            }
            gbtg::write_bmp("cache/player_opening_check.bmp", image);
            // The scoreboard's 0x00132290 draws nothing while DAT_004EE1EC (the page's mode) is set: no heart at the
            // top right behind the tutorial page.
            const auto heart_pixels = [](const gbtg::RgbaImage& frame)
            {
                int count = 0;
                for (int y = 40; y < 110 && y < frame.height; ++y)
                {
                    for (int x = 480; x < 580 && x < frame.width; ++x)
                    {
                        const size_t at = (size_t(y) * size_t(frame.width) + size_t(x)) * 4;
                        const int b = frame.bgra[at];
                        const int g = frame.bgra[at + 1];
                        const int r = frame.bgra[at + 2];
                        count += r > 180 && g < 90 && b < 90 ? 1 : 0;
                    }
                }
                return count;
            };
            const int page_heart = heart_pixels(image);
            std::cout << "player opening: heart pixels behind the page " << page_heart << "\n";
            if (page_heart > 20)
            {
                throw gbtg::Error("the HUD heart draws behind the tutorial page (DAT_004EE1EC gate)");
            }
            if (!cooper_locked)
            {
                throw gbtg::Error("Cooper is not held in goal 7 (strategy state 0x3FB) before the tutorial ends");
            }
            if (!imps_paused)
            {
                throw gbtg::Error("the imps' strategies did not wait (state 1000) behind the tutorial page");
            }
            std::cout << "PASS: Cooper is locked, the imps pause and the HUD hides behind the tutorial page\n";
            if (!saw_dialogue)
            {
                throw gbtg::Error("player opening must hold chapter1_scene1_fight1_howtofight until A; "
                                  "native dialogue did not hold the live script");
            }
            if (!saw_bat_hop)
            {
                const auto& active = game.script_dialogue->dialogue;
                if (active.state.widget == nullptr || active.state.widget->bat == nullptr)
                {
                    throw gbtg::Error("native tutorial widget has no button bat after 600 frames");
                }
                const auto bat = active.widget_update.bats(active.state.widget->bat);
                throw gbtg::Error("native tutorial button bat did not hop: state=" + std::to_string(bat.bat.state)
                    + " idle=" + std::to_string(bat.bat.idle_timer) + " elapsed=" + std::to_string(bat.frame.elapsed)
                    + " dt=" + std::to_string(bat.context.dt) + " rate=" + std::to_string(bat.bat.actor.animation_rate));
            }
            auto& dialogue = gbtg::scene_dialogue_context(game).dialogue;
            if (dialogue.state.widget == nullptr || dialogue.state.cutscene == nullptr ||
                dialogue.state.input.data() == nullptr || dialogue.page.text == nullptr)
            {
                throw gbtg::Error("player tutorial flags have no native widget, cutscene, input or page ownership");
            }
            const std::u16string_view tutorial_page(dialogue.page.text);
            const size_t last_word = tutorial_page.find(u"attack!");
            if (last_word == std::u16string_view::npos ||
                dialogue.page.next_text < dialogue.page.text + last_word + 7)
            {
                throw gbtg::Error("opening tutorial text no longer fits its retail page after widget drawing");
            }
            bool cutscene_after_accept = false;
            // Give each native page a fresh A edge, followed by release and
            // enough time for the fade. Holding A cannot stand in for a new edge.
            for (int frame = 0; frame < 600 && !cutscene_after_accept; ++frame)
            {
                gbtg::InputState input;
                input.accept = frame % 120 < 5;
                gbtg::gfx_begin_frame();
                game.tick(player_step, input);
                gbtg::gfx_end_frame();
                if (frame == 0 && dialogue.state.completion == 0 && dialogue.state.mode == 2)
                {
                    const auto samples = gbtg::runtime_pad_state_views();
                    const auto& widget = *dialogue.state.widget;
                    throw gbtg::Error("native widget missed a fresh A edge: pad="
                        + std::to_string(gbtg::read_u16(samples.controllers, 0x38))
                        + " kind=" + std::to_string(gbtg::read_u32(dialogue.state.input, 0))
                        + " held=" + std::to_string(gbtg::read_u16(dialogue.state.input, 0x40))
                        + " previous=" + std::to_string(gbtg::read_u16(dialogue.state.input, 0x7C))
                        + " widget=" + std::to_string(widget.mode) + " count=" + std::to_string(widget.count)
                        + " icon=" + std::to_string(gbtg::read_u32({widget.current_set, 0x310}, 0x40)));
                }
                cutscene_after_accept = dialogue.state.mode == 0 ||
                    (dialogue.state.mode == 4 && dialogue.state.count == 1);
            }
            if (cutscene_after_accept && dialogue.state.mode == 4)
            {
                gbtg::update_runtime_dialogue(dialogue);
            }
            if (!cutscene_after_accept || dialogue.state.mode != 0)
            {
                throw gbtg::Error("player tutorial A edges did not release dialogue and start the opening cutscene: mode="
                    + std::to_string(dialogue.state.mode) + " completion=" + std::to_string(dialogue.state.completion)
                    + " button=" + std::to_string(dialogue.state.button) + " script=" + std::to_string(gbtg::g_flag_2474));
            }
            std::cout << "PASS: player tutorial holds without A, animates its bat and closes after fresh A edges\n";
            // The condition nodes seek their block's closing 0x3C (0x00125450 -> 0x000532B0), so the main
            // fired cursor waits at a type 0xF gate instead of draining the fired list, which would end the
            // scene (0x00117140 requests the next scene AID).
            // An imp's first sighting of Cooper queues the ghoul intro (0x000F4870); the scene tick shows it
            // (0x000F4A00 / 0x000F4560): scene flag 4 hides the HUD, the intro camera holds four shots, then
            // 0x000F4B90 ends it. Play on until it has ended.
            int intro_start = -1;
            int intro_end = -1;
            for (int frame = 0; frame < 300 || (intro_start >= 0 && intro_end < 0 && frame < 900); ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(player_step, {});
                const bool showing = (gbtg::g_flags_ea9c & 4u) != 0;
                if (showing && intro_start < 0)
                {
                    intro_start = frame;
                }
                if (!showing && intro_start >= 0 && intro_end < 0)
                {
                    intro_end = frame;
                }
                if (showing && intro_end < 0 && (frame - intro_start) % 20 == 0)
                {
                    // Each of the intro's four shots should show the imp facing the camera.
                    capture_readback("cache/player_intro_" + std::to_string(frame - intro_start) + ".bmp");
                }
                if (intro_start >= 0 && frame == intro_start + player_ticks(90))
                {
                    gbtg::RgbaImage intro;
                    if (gbtg::gfx_readback(intro))
                    {
                        gbtg::write_bmp("cache/player_intro_check.bmp", intro);
                    }
                }
                gbtg::gfx_end_frame();
            }
            std::cout << "ghoul intro: frames " << intro_start << " to " << intro_end << "\n";
            if (intro_start < 0 || intro_end < 0)
            {
                throw gbtg::Error("the imps' ghoul intro did not play after the tutorial");
            }
            for (const auto& mark : marked())
            {
                if (mark.actor->class_key == 4 && strategy_state(mark) == 0x3FB)
                {
                    throw gbtg::Error("Cooper is still held in goal 7 after the tutorial released him");
                }
            }
            std::cout << "PASS: the ghoul intro plays on an imp after the tutorial and Cooper is released\n";
            if (game.scene_script.name != "aid_script_ghoulies_chapter1_scene1_2playcam" || gbtg::g_script_fired.empty())
            {
                throw gbtg::Error("opening script left the scene after its tutorial: scene=" + game.scene_script.name);
            }
            auto waiting = gbtg::script_node_at(game.scene_script, gbtg::g_script_fired.front());
            if (!waiting || waiting->type != 0xF || game.scene_name != "(next bit)")
            {
                throw gbtg::Error("opening script is not waiting at the (next bit) gate after its tutorial: title="
                    + game.scene_name);
            }
            std::cout << "PASS: the opening script holds at its (next bit) gate after the tutorial\n";
            // Cooper (class 4) walks under the left stick: 0x001555B0's packet reaches the player mind's
            // control update (0x0007D130) and the body's walk action.
            const auto cooper_position = [&game]() -> std::optional<gbtg::Vec3>
            {
                if (game.factory_scene == nullptr)
                {
                    return std::nullopt;
                }
                for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
                {
                    if (actor != nullptr && actor->class_key == 4)
                    {
                        return gbtg::actor_world_position(*actor);
                    }
                }
                return std::nullopt;
            };
            const auto before = cooper_position();
            if (!before)
            {
                throw gbtg::Error("opening scene has no class 4 player actor");
            }
            for (int frame = 0; frame < player_ticks(120); ++frame)
            {
                gbtg::InputState stick;
                stick.pads[0].ly = 32767;
                gbtg::gfx_begin_frame();
                game.tick(player_step, stick);
                if (frame == player_ticks(120) - 1)
                {
                    gbtg::RgbaImage walked;
                    if (!gbtg::gfx_readback(walked))
                    {
                        throw gbtg::Error("player walk readback failed");
                    }
                    gbtg::write_bmp("cache/player_walk_check.bmp", walked);
                }
                gbtg::gfx_end_frame();
            }
            const auto after = cooper_position();
            const float dx = after->x - before->x;
            const float dz = after->z - before->z;
            std::cout << "cooper moved " << dx << " " << (after->y - before->y) << " " << dz << "\n";
            if (dx * dx + dz * dz < 1.f)
            {
                throw gbtg::Error("left stick did not move Cooper after the tutorial");
            }
            if (controls_test)
            {
                // Hold the left stick for one second at each host rate. Cooper's speed is the median of his per-frame
                // planar speeds over the second half (after the 0x0002C2B0 velocity ease has settled), so an imp
                // blocking or hitting him for a few frames does not move it.
                const auto run_speed = [&](float hz, gbtg::i16 lx, gbtg::i16 ly)
                {
                    const int frames = int(hz);
                    std::vector<float> samples;
                    auto previous = cooper_position();
                    for (int frame = 0; frame < frames; ++frame)
                    {
                        gbtg::InputState stick;
                        stick.pads[0].lx = lx;
                        stick.pads[0].ly = ly;
                        gbtg::gfx_begin_frame();
                        game.tick(1.f / hz, stick);
                        gbtg::gfx_end_frame();
                        const auto now = cooper_position();
                        if (frame >= frames / 2)
                        {
                            const float dx = now->x - previous->x;
                            const float dz = now->z - previous->z;
                            samples.push_back(std::sqrt(dx * dx + dz * dz) * hz);
                        }
                        previous = now;
                    }
                    std::nth_element(samples.begin(), samples.begin() + samples.size() / 2, samples.end());
                    return samples[samples.size() / 2];
                };
                const auto settle = [&]()
                {
                    for (int frame = 0; frame < 30; ++frame)
                    {
                        gbtg::gfx_begin_frame();
                        game.tick(player_step, {});
                        gbtg::gfx_end_frame();
                    }
                };
                std::vector<float> speeds;
                for (const float hz : {30.f, 60.f, 144.f, 240.f})
                {
                    // Four directions; walls and imps only slow him, so the fastest is his free run.
                    float fastest = 0.f;
                    std::cout << "run speed at " << hz << " Hz:";
                    for (const auto& [lx, ly] : {std::pair<gbtg::i16, gbtg::i16>{32767, 0}, {-32767, 0}, {0, -32767},
                             {0, 32767}})
                    {
                        settle();
                        const float speed = run_speed(hz, lx, ly);
                        std::cout << " " << speed;
                        fastest = std::max(fastest, speed);
                    }
                    std::cout << " units/s\n";
                    speeds.push_back(fastest);
                }
                for (const float speed : speeds)
                {
                    if (std::abs(speed - speeds.front()) > speeds.front() * .05f)
                    {
                        throw gbtg::Error("Cooper's run speed depends on the host frame rate");
                    }
                }
                std::cout << "PASS: Cooper's run speed is the same at 30 (retail), 60, 144 and 240 Hz\n";
                // Flick the right stick away from every imp. With nothing in range, 0x0007E880 falls back to the
                // mind's own +0xB8 record (0x0007EA20), 80 units along the stick, and Cooper swings at the air.
                settle();
                const auto start = cooper_position();
                float ax = 0.f;
                float az = 0.f;
                for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
                {
                    if (actor != nullptr && actor->class_key == 23)
                    {
                        const auto imp = gbtg::actor_world_position(*actor);
                        ax -= imp.x - start->x;
                        az -= imp.z - start->z;
                    }
                }
                const float away = std::sqrt(ax * ax + az * az);
                ax = away > 0.f ? ax / away : 1.f;
                az = away > 0.f ? az / away : 0.f;
                float swing = 0.f;
                for (int frame = 0; frame < 64; ++frame)
                {
                    gbtg::InputState input;
                    if ((frame / 8) % 2 == 0)
                    {
                        input.pads[0].rx = gbtg::i16(ax * 32767.f);
                        input.pads[0].ry = gbtg::i16(-az * 32767.f);
                    }
                    gbtg::gfx_begin_frame();
                    game.tick(player_step, input);
                    if (frame == 12)
                    {
                        gbtg::RgbaImage swung;
                        if (gbtg::gfx_readback(swung))
                        {
                            gbtg::write_bmp("cache/player_air_swing.bmp", swung);
                        }
                    }
                    gbtg::gfx_end_frame();
                    const auto now = cooper_position();
                    swing = std::max(swing, std::sqrt((now->x - start->x) * (now->x - start->x)
                        + (now->z - start->z) * (now->z - start->z)));
                }
                std::cout << "air swing: direction " << ax << " " << az << ", Cooper moved up to " << swing << "\n";
                if (swing < 1.f)
                {
                    throw gbtg::Error("the right stick did not swing Cooper with no imp in range");
                }
                std::cout << "PASS: the right stick swings Cooper at the air with no imp in range\n";
                // The triggers turn the follow camera (0x0013DC40): left alone turns one way, right the other.
                const auto camera_yaw = []()
                {
                    const auto forward = gbtg::entity_detail::g_display_view->forward;
                    return std::atan2(forward.x, forward.z);
                };
                const auto turn = [&](gbtg::u8 left, gbtg::u8 right)
                {
                    settle();
                    const float before = camera_yaw();
                    for (int frame = 0; frame < 60; ++frame)
                    {
                        gbtg::InputState input;
                        input.pads[0].trigger_l = left;
                        input.pads[0].trigger_r = right;
                        gbtg::gfx_begin_frame();
                        game.tick(player_step, input);
                        gbtg::gfx_end_frame();
                    }
                    float delta = camera_yaw() - before;
                    while (delta > 3.14159265f)
                    {
                        delta -= 6.28318531f;
                    }
                    while (delta < -3.14159265f)
                    {
                        delta += 6.28318531f;
                    }
                    return delta;
                };
                const float left_turn = turn(255, 0);
                const float right_turn = turn(0, 255);
                std::cout << "camera turn: left trigger " << left_turn << ", right trigger " << right_turn << " rad\n";
                if (!(left_turn * right_turn < 0.f))
                {
                    throw gbtg::Error("the triggers did not turn the follow camera in opposite directions");
                }
                std::cout << "PASS: the triggers turn the follow camera\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            if (fight_test)
            {
                // Cooper walks to the nearest imp (left stick) and holds the right stick toward it. The stick axes
                // follow the play camera, which looks down -Z: up is -Z and right is +X.
                const auto imps = [&game]()
                {
                    std::vector<gbtg::Vec3> positions;
                    for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
                    {
                        if (actor != nullptr && actor->class_key == 23)
                        {
                            positions.push_back(gbtg::actor_world_position(*actor));
                        }
                    }
                    return positions;
                };
                // The script's "Kill All" gate (0x00132680) counts the DAT_00511B48 actors whose body +0x94 is
                // nonzero; count the same list.
                const auto alive_count = []()
                {
                    int count = 0;
                    for (const auto* node = static_cast<const gbtg::entity_detail::ListHead*>(
                             gbtg::entity_detail::g_world.alt.next);
                         node != nullptr; node = static_cast<const gbtg::entity_detail::ListHead*>(node->next))
                    {
                        if (gbtg::read_u16(gbtg::g_alt_node_body(node), 0x94) != 0)
                        {
                            ++count;
                        }
                    }
                    return count;
                };
                const size_t imps_at_start = imps().size();
                const int alive_at_start = alive_count();
                if (imps_at_start == 0 || alive_at_start == 0)
                {
                    throw gbtg::Error("the fight starts with no live imps on DAT_00511B48");
                }
                const std::string gate_title = game.scene_name;
                std::string title = game.scene_name;
                // The heart shows at the top right once the fight has touched health; behind a script page the
                // scoreboard's 0x00132290 draws nothing while DAT_004EE1EC (the page's mode) is set.
                int hud_heart_frames = 0;
                int page_frames = 0;
                int pages_checked = 0;
                // The region walk's last position, last stick angle and the learned camera offset.
                std::optional<gbtg::Vec3> walk_last;
                std::optional<float> walk_commanded;
                float walk_offset = 0.f;
                // The centre of the first region record named by a live PlayerEntersRegion condition.
                const auto region_goal = [&]() -> std::optional<gbtg::Vec3>
                {
                    if (gbtg::g_occupancy_pool == nullptr)
                    {
                        return std::nullopt;
                    }
                    const auto& pool = *static_cast<const gbtg::OccupancyPool*>(gbtg::g_occupancy_pool);
                    for (const auto& condition : gbtg::g_script_conds)
                    {
                        if (gbtg::cond_u32(condition, 0) != 8)
                        {
                            continue;
                        }
                        const gbtg::u32 region = gbtg::cond_u32(condition, 8);
                        for (const auto* record = pool.begin; record < pool.cursor; ++record)
                        {
                            if (record->value_0 == region)
                            {
                                return record->center;
                            }
                        }
                    }
                    // A section held at its end (type 0xF, DAT_00512518) waits for the player to leave: walk into
                    // an exit volume (mask 1, 0x00140E30).
                    if (gbtg::g_gate_2518 != 0)
                    {
                        for (const auto* record = pool.begin; record < pool.cursor; ++record)
                        {
                            if ((record->mask & 1) != 0)
                            {
                                // Line up with the box across x before going in: a straight walk from the room
                                // clips the door frame.
                                const auto at = cooper_position();
                                const float reach = record->extent_x * 2.f;
                                if (at && std::abs(at->z - record->center.z) > record->extent_z * 0.5f
                                    && std::abs(at->x - record->center.x) > reach * 0.5f)
                                {
                                    const float side = at->x < record->center.x ? -1.f : 1.f;
                                    return gbtg::Vec3{record->center.x + side * reach, record->center.y,
                                        record->center.z};
                                }
                                return record->center;
                            }
                        }
                    }
                    return std::nullopt;
                };
                // One frame of play: walk Cooper to the nearest imp and flick the right stick to attack; with no
                // imp to fight, give fresh A edges for the script's pages, as the tutorial does, and walk him into a
                // region the script waits for.
                const auto play_frame = [&](int frame, const std::string& prefix)
                {
                    const auto cooper = cooper_position();
                    const auto alive = imps();
                    gbtg::InputState input;
                    float length = 0.f;
                    // A script page (the butler's dialogue) holds the scene until A; give it fresh A edges and
                    // leave the sticks centred.
                    const bool page = game.script_dialogue != nullptr &&
                        game.script_dialogue->dialogue.state.widget != nullptr &&
                        game.script_dialogue->dialogue.state.mode != 0;
                    if (page)
                    {
                        input.accept = frame % 30 < 5;
                    }
                    else if (!cooper)
                    {
                        // No A outside a page.
                    }
                    else if (alive.empty() || alive_count() == 0)
                    {
                        // No A outside a page. A live PlayerEntersRegion (condition 8, 0x00132940, region at record
                        // +8), or a section held at its end, is waiting for Cooper: walk him toward that region's or
                        // the exit's centre.
                        if (const auto goal = region_goal())
                        {
                            // The stick is camera-relative: learn the offset between the stick's angle and the
                            // direction Cooper actually moved, and steer by it.
                            if (walk_last && walk_commanded)
                            {
                                const float mx = cooper->x - walk_last->x;
                                const float mz = cooper->z - walk_last->z;
                                if (mx * mx + mz * mz > 0.01f)
                                {
                                    float error = std::atan2(mx, mz) - (*walk_commanded + walk_offset);
                                    error = std::remainder(error, 6.2831853f);
                                    walk_offset += error * 0.5f;
                                }
                            }
                            const float dx = goal->x - cooper->x;
                            const float dz = goal->z - cooper->z;
                            if (dx * dx + dz * dz > 1.f)
                            {
                                const float stick = std::atan2(dx, dz) - walk_offset;
                                input.pads[0].lx = gbtg::i16(std::sin(stick) * 32767.f);
                                input.pads[0].ly = gbtg::i16(-std::cos(stick) * 32767.f);
                                walk_commanded = stick;
                            }
                            else
                            {
                                walk_commanded.reset();
                            }
                            walk_last = *cooper;
                        }
                    }
                    else if (!passive)
                    {
                        const gbtg::Vec3* nearest = &alive.front();
                        float best = 1e30f;
                        for (const auto& imp : alive)
                        {
                            const float dx = imp.x - cooper->x;
                            const float dz = imp.z - cooper->z;
                            if (dx * dx + dz * dz < best)
                            {
                                best = dx * dx + dz * dz;
                                nearest = &imp;
                            }
                        }
                        const float dx = nearest->x - cooper->x;
                        const float dz = nearest->z - cooper->z;
                        length = std::sqrt(dx * dx + dz * dz);
                        const float ux = length > 0.f ? dx / length : 0.f;
                        const float uz = length > 0.f ? dz / length : 0.f;
                        // Keep a short standoff: close in from afar, step back when standing on the imp, where no
                        // direction toward it falls inside the target cone. Imps further than 80 units are in
                        // another room or behind a wall the straight-line walk cannot pass; wait for them to come
                        // back.
                        if (length > 10.f && length < 80.f)
                        {
                            input.pads[0].lx = gbtg::i16(ux * 32767.f);
                            input.pads[0].ly = gbtg::i16(-uz * 32767.f);
                        }
                        else if (length < 4.f)
                        {
                            input.pads[0].lx = gbtg::i16(-ux * 32767.f);
                            input.pads[0].ly = gbtg::i16(uz * 32767.f);
                        }
                        // The mind's stick gesture (0x0007D5A0) attacks when the stick comes back after a push, so
                        // the right stick is flicked: eight frames out, eight frames centred. Out of reach a flick
                        // swings at the air (0x0007EA20), so only flick inside the standoff.
                        if (length <= 10.f && (frame / 8) % 2 == 0)
                        {
                            input.pads[0].rx = gbtg::i16(ux * 32767.f);
                            input.pads[0].ry = gbtg::i16(-uz * 32767.f);
                        }
                    }
                    gbtg::gfx_begin_frame();
                    game.tick(player_step, input);
                    page_frames = page ? page_frames + 1 : 0;
                    if (frame % 120 == 119 || page_frames == 30)
                    {
                        gbtg::RgbaImage capture;
                        if (gbtg::gfx_readback(capture))
                        {
                            if (frame % 120 == 119)
                            {
                                gbtg::write_bmp("cache/" + prefix + std::to_string(frame + 1) + ".bmp", capture);
                                hud_heart_frames += !page && heart_pixels(capture) > 100 ? 1 : 0;
                            }
                            if (page_frames == 30)
                            {
                                ++pages_checked;
                                if (heart_pixels(capture) > 20)
                                {
                                    gbtg::write_bmp("cache/player_page_hud.bmp", capture);
                                    throw gbtg::Error("the HUD heart draws behind a script page (DAT_004EE1EC gate)");
                                }
                            }
                        }
                    }
                    gbtg::gfx_end_frame();
                    if (frame % 60 == 0 && cooper && !alive.empty())
                    {
                        // Imps beyond the walk's reach (80 units) are in another room, such as behind a door.
                        int distant = 0;
                        for (const auto& imp : alive)
                        {
                            const float dx = imp.x - cooper->x;
                            const float dz = imp.z - cooper->z;
                            distant += dx * dx + dz * dz >= 80.f * 80.f ? 1 : 0;
                        }
                        // DAT_0050E568 / DAT_0050E56C: the script's live ghouly maximum and minimum for spawns.
                        std::cout << prefix << "frame " << frame << ": imps " << alive.size() << " alive "
                                  << alive_count() << " far " << distant << " limits "
                                  << gbtg::scene_word(0x0050E568u) << "/" << gbtg::scene_word(0x0050E56Cu)
                                  << " nearest " << length << " cooper " << cooper->x << " " << cooper->z
                                  << (page ? " page" : "") << " title " << game.scene_name << "\n";
                    }
                };
                bool cleared = false;
                // A reacting imp (action 0x7E, its hit reaction) can be finished: one hit empties its stagger
                // meter (body +0x16C; the imp's statsheet count is 1), so 0x001413C0 passes on its IsBeingHit bit and
                // Cooper's next attack (0x0001CDA0) is the knockdown finisher that takes it from the reaction to 0.
                std::unordered_map<gbtg::u32, std::pair<gbtg::u16, gbtg::u16>> reactions; // Address: action, health.
                bool rehit = false;
                // The heart only draws once Cooper's health changes; with the finisher he may clear the wave unhit.
                int cooper_start_health = -1;
                bool cooper_hit = false;
                const auto track_reactions = [&]()
                {
                    for (const auto& mark : marked())
                    {
                        if (mark.actor->class_key == 4)
                        {
                            const int health = gbtg::body_record(mark.body).energy;
                            if (cooper_start_health < 0)
                            {
                                cooper_start_health = health;
                            }
                            cooper_hit = cooper_hit || health < cooper_start_health;
                        }
                        if (mark.actor->class_key != 23)
                        {
                            continue;
                        }
                        const gbtg::u16 action = gbtg::body_record(mark.body).action_key;
                        const gbtg::u16 health = gbtg::body_record(mark.body).energy;
                        const auto found = reactions.find(mark.address);
                        if (found != reactions.end() && found->second.first == 0x7E && found->second.second != 0
                            && health == 0)
                        {
                            rehit = true;
                        }
                        reactions[mark.address] = {action, health};
                    }
                };
                if (death_test)
                {
                    // Cooper stands still in the fight. The imps' hits empty his health; the player's death state
                    // (factory type 0x69) plays his dying animations, starts the scene's transition (0x0012C5D0)
                    // and restarts the scene (0x0008F650, 0x00134AE0).
                    const std::string fight_title = game.scene_name;
                    if (brew_test)
                    {
                        gbtg::g_runtime_profile.unlocks.bytes[0] |= 8; // **(DAT_00506C70+8): the brew is unlocked.
                        // A brew needs a kill in the scene (DAT_00506C8C >= DAT_00430D9C); Cooper stands still here.
                        // The death restart keeps the count (DAT_00430D94).
                        gbtg::g_runtime_profile.times[5] = 1;
                    }
                    const auto cooper_health = [&]() -> int
                    {
                        for (const auto& mark : marked())
                        {
                            if (mark.actor->class_key == 4)
                            {
                                return gbtg::body_record(mark.body).energy;
                            }
                        }
                        return -1;
                    };
                    int died = -1;
                    int transition = -1;
                    int restarted = -1;
                    for (int frame = 0; frame < player_ticks(36000) && restarted < 0; ++frame)
                    {
                        play_frame(frame, "player_death_");
                        const int health = cooper_health();
                        if (died < 0 && health == 0)
                        {
                            died = frame;
                            std::cout << "death frame " << frame << ": Cooper's health is 0, title " << game.scene_name
                                      << "\n";
                        }
                        if (died >= 0 && frame - died <= 300 && (frame - died) % 15 == 5)
                        {
                            // The death's camera pulse (0x00061960) blurs the captured view (0x000615E0).
                            capture_readback("cache/player_death_blur_" + std::to_string(frame - died) + ".bmp");
                        }
                        if (died >= 0 && transition < 0 && gbtg::g_scene_transition.phase != 0)
                        {
                            transition = frame;
                            std::cout << "death frame " << frame << ": the transition starts (row "
                                      << gbtg::g_scene_transition.row << ")\n";
                        }
                        if (transition >= 0 && health > 0 && game.scene_name != fight_title)
                        {
                            restarted = frame;
                        }
                        if (frame % player_ticks(600) == 0)
                        {
                            std::cout << "death frame " << frame << ": health " << health << " title "
                                      << game.scene_name << " transition " << gbtg::g_scene_transition.phase << "\n";
                        }
                    }
                    if (died < 0)
                    {
                        throw gbtg::Error("the imps never emptied Cooper's health");
                    }
                    if (transition < 0)
                    {
                        throw gbtg::Error("Cooper's death never started the scene transition");
                    }
                    if (restarted < 0)
                    {
                        throw gbtg::Error("the scene did not restart after Cooper's death: title " + game.scene_name);
                    }
                    if (brew_test)
                    {
                        // The restart's scene start (0x7D) moves Cooper to 0x3F1: the meter fades in with the brew
                        // count and the widget's A/B rows; A drinks, doubling his health.
                        const auto cooper_state = [&]() -> int
                        {
                            for (const auto& mark : marked())
                            {
                                if (mark.actor->class_key == 4)
                                {
                                    return mark.strategy.empty() ? 0 : gbtg::strategy_base(mark.strategy).state;
                                }
                            }
                            return -1;
                        };
                        int prompt = -1;
                        int drank = -1;
                        int before = -1;
                        for (int frame = 0; frame < player_ticks(3600) && drank < 0; ++frame)
                        {
                            gbtg::InputState input;
                            const int state = cooper_state();
                            if (prompt < 0 && state == 0x3F1)
                            {
                                prompt = frame;
                                before = cooper_health();
                                std::cout << "brew frame " << frame << ": Cooper waits in 0x3F1, health " << before
                                          << "\n";
                            }
                            // A script page (the restarted opening's) holds Cooper in 0x3FB until A.
                            const bool page = game.script_dialogue != nullptr
                                && game.script_dialogue->dialogue.state.widget != nullptr
                                && game.script_dialogue->dialogue.state.mode != 0;
                            input.accept = (prompt < 0 && page && frame % 30 < 5)
                                || (prompt >= 0 && frame - prompt == player_ticks(90));
                            gbtg::gfx_begin_frame();
                            game.tick(player_step, input);
                            if (prompt >= 0 && (frame - prompt == player_ticks(45) || frame - prompt == player_ticks(85)
                                || frame - prompt == player_ticks(100) || frame - prompt == player_ticks(130)))
                            {
                                gbtg::RgbaImage capture;
                                if (gbtg::gfx_readback(capture))
                                {
                                    gbtg::write_bmp("cache/player_brew_" + std::to_string(frame - prompt) + ".bmp",
                                        capture);
                                }
                            }
                            gbtg::gfx_end_frame();
                            if (prompt >= 0 && frame - prompt > player_ticks(130) && cooper_state() != 0x3F1)
                            {
                                drank = frame;
                            }
                        }
                        if (prompt < 0)
                        {
                            throw gbtg::Error("the restarted scene never offered the brew (state 0x3F1)");
                        }
                        if (drank < 0)
                        {
                            throw gbtg::Error("A never took Cooper out of the brew prompt");
                        }
                        const int after = cooper_health();
                        std::cout << "brew frame " << drank << ": Cooper left 0x3F1 (state " << cooper_state()
                                  << "), health " << before << " -> " << after << "\n";
                        if (after != before * 2)
                        {
                            throw gbtg::Error("the brew did not double Cooper's health");
                        }
                        std::cout << "PASS: the restarted scene offers the brew and A drinks it\n";
                        gbtg::gfx_shutdown();
                        return 0;
                    }
                    // The restarted scene runs on: its script passes the 0x6F player wait (node 0x254), which a
                    // failed native start (factory_scene->actor_start -1) holds forever.
                    const gbtg::u32 restart_node = gbtg::g_script_fired.empty() ? 0u : gbtg::g_script_fired.back();
                    bool resumed = false;
                    for (int frame = 0; frame < player_ticks(600) && !resumed; ++frame)
                    {
                        gbtg::InputState input;
                        gbtg::gfx_begin_frame();
                        game.tick(player_step, input);
                        resumed = !gbtg::g_script_fired.empty() && gbtg::g_script_fired.back() != restart_node;
                        if (resumed)
                        {
                            gbtg::RgbaImage capture;
                            if (gbtg::gfx_readback(capture))
                            {
                                gbtg::write_bmp("cache/player_death_restart.bmp", capture);
                            }
                        }
                        gbtg::gfx_end_frame();
                    }
                    if (!resumed)
                    {
                        throw gbtg::Error("the restarted scene's script never moved past its player wait");
                    }
                    std::cout << "PASS: the imps beat Cooper, his death state turns the page and the scene restarts ("
                              << game.scene_name << ")\n";
                    gbtg::gfx_shutdown();
                    return 0;
                }
                for (int frame = 0; frame < 7200; ++frame)
                {
                    if (alive_count() == 0 && game.scene_name != gate_title)
                    {
                        std::cout << "fight frame " << frame << ": every imp defeated, title " << game.scene_name << "\n";
                        cleared = true;
                        break;
                    }
                    play_frame(frame, "player_fight_");
                    track_reactions();
                    if (game.scene_name != title)
                    {
                        std::cout << "fight frame " << frame << ": title " << game.scene_name << "\n";
                        title = game.scene_name;
                    }
                }
                gbtg::RgbaImage final_frame;
                if (gbtg::gfx_readback(final_frame))
                {
                    gbtg::write_bmp("cache/player_fight_end.bmp", final_frame);
                }
                const int alive_at_end = alive_count();
                std::cout << "fight: imps " << imps_at_start << " -> " << imps().size() << ", alive " << alive_at_start
                          << " -> " << alive_at_end << ", title " << game.scene_name << "\n";
                if (alive_at_end >= alive_at_start)
                {
                    throw gbtg::Error("no imp was defeated in the fight");
                }
                if (!cleared)
                {
                    throw gbtg::Error("the opening script did not pass its Kill All gate: " + std::to_string(alive_at_end)
                        + " imps alive, title " + game.scene_name);
                }
                std::cout << "PASS: Cooper defeats the opening imps and the script passes Kill All\n";
                if (!rehit)
                {
                    throw gbtg::Error("no imp was finished from its hit reaction (action 0x7E)");
                }
                std::cout << "PASS: a reacting imp is finished before its reaction ends\n";
                std::cout << "HUD: heart in " << hud_heart_frames << " fight captures, " << pages_checked
                          << " script pages checked\n";
                if ((cooper_hit && hud_heart_frames == 0) || pages_checked == 0)
                {
                    throw gbtg::Error("the fight hit Cooper but never showed the heart, or no script page was checked");
                }
                if (!cooper_hit)
                {
                    std::cout << "HUD: Cooper was not hit in the fight, so the heart had no reason to show\n";
                }
                std::cout << "PASS: the HUD shows in the fight and hides behind the butler's pages\n";
                // "Freeplay (Temporary Weapon Explained)": the chair stands in the hall, type 0x96 put Cooper on
                // marker 1 inside region 1, and the section waits for the chair's pickup (ObjectPickedUp, event
                // 0xAD with the chair's tag), for Cooper to leave region 1 (0x00124860), or for its 5 s timeout.
                if (game.scene_name != "Freeplay (Temporary Weapon Explained)")
                {
                    throw gbtg::Error("Kill All did not lead to the chair's section: title " + game.scene_name);
                }
                const gbtg::u32 chair_tag = gbtg::script_find_actor_tag("objTag_Weapon_Chair");
                bool picked_up_condition = false;
                for (const auto& condition : gbtg::g_script_conds)
                {
                    picked_up_condition |= gbtg::cond_u32(condition, 0) == 3
                        && (gbtg::cond_u32(condition, 8) & 0xFFFFu) == chair_tag && chair_tag != 0;
                }
                const auto placed = cooper_position();
                if (!picked_up_condition || !placed || !gbtg::occupancy_region_contains(1, *placed))
                {
                    throw gbtg::Error("the chair's section does not wait for the chair with Cooper in region 1");
                }
                // The chair (its actor +6 is the chair's tag): Cooper walks up to it, where his mind takes it as the
                // pending pickup (mind +0x134), and A picks it up (0x0007D210): the pick-up body state (factory type
                // 0x7C, 0x0009DF90) attaches it (0x0001B190), and its exit (0x0009DEA0) queues event 0xAD with the
                // chair's tag, which ends the section before its 5 s timeout.
                const auto chair_actor = [&game, chair_tag]() -> gbtg::Actor*
                {
                    for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
                    {
                        if (actor != nullptr && actor->class_key == chair_tag)
                        {
                            return actor;
                        }
                    }
                    return nullptr;
                };
                const auto cooper_actor = [&game]() -> gbtg::Actor*
                {
                    for (const auto& [address, actor] : game.factory_scene->start_actors(game).actor_addresses())
                    {
                        if (actor != nullptr && actor->class_key == 4)
                        {
                            return actor;
                        }
                    }
                    return nullptr;
                };
                int held = 0;
                int picked_frame = -1;
                int event_frame = -1;
                while (game.scene_name == "Freeplay (Temporary Weapon Explained)" && held < 1200)
                {
                    gbtg::InputState input;
                    gbtg::Actor* chair = chair_actor();
                    const auto cooper = cooper_position();
                    float distance = 0.f;
                    if (held >= player_ticks(60) && chair != nullptr && cooper && picked_frame < 0)
                    {
                        const gbtg::Vec3 at = gbtg::actor_world_position(*chair);
                        const float dx = at.x - cooper->x;
                        const float dz = at.z - cooper->z;
                        distance = std::sqrt(dx * dx + dz * dz);
                        // The chair's collision shape stops Cooper short of its centre, and A picks it up only
                        // inside the cylinder round a grab point (0x00147430). Close in, then circle the chair
                        // between A presses until a grab point is in reach.
                        if (distance > 10.f)
                        {
                            input.pads[0].lx = gbtg::i16(dx / distance * 32767.f);
                            input.pads[0].ly = gbtg::i16(-dz / distance * 32767.f);
                        }
                        else if (held % player_ticks(20) < player_ticks(3))
                        {
                            input.accept = true;
                        }
                        else if (held % player_ticks(20) >= player_ticks(8))
                        {
                            // Half toward the chair, half along its rim.
                            const float tx = (dx - dz) / (distance * 1.41421356f);
                            const float tz = (dz + dx) / (distance * 1.41421356f);
                            input.pads[0].lx = gbtg::i16(tx * 16000.f);
                            input.pads[0].ly = gbtg::i16(-tz * 16000.f);
                        }
                    }
                    gbtg::gfx_begin_frame();
                    game.tick(player_step, input);
                    if (held == player_ticks(40))
                    {
                        gbtg::RgbaImage chair_capture;
                        if (gbtg::gfx_readback(chair_capture))
                        {
                            gbtg::write_bmp("cache/player_chair_check.bmp", chair_capture);
                        }
                    }
                    chair = chair_actor();
                    if (picked_frame < 0 && chair != nullptr && chair->spawn_owner != nullptr
                        && chair->spawn_owner == cooper_actor())
                    {
                        picked_frame = held;
                        std::cout << "chair picked up at frame " << held << "\n";
                        gbtg::RgbaImage pickup_capture;
                        if (gbtg::gfx_readback(pickup_capture))
                        {
                            gbtg::write_bmp("cache/player_chair_pickup.bmp", pickup_capture);
                        }
                    }
                    if (picked_frame >= 0 && held == picked_frame + player_ticks(100))
                    {
                        gbtg::RgbaImage held_capture;
                        if (gbtg::gfx_readback(held_capture))
                        {
                            gbtg::write_bmp("cache/player_chair_held.bmp", held_capture);
                        }
                    }
                    // The pick-up state's exit queues {0xAD, chair tag} (0x00116810); node +0xC is the tag.
                    for (const gbtg::LiveScript* node = gbtg::g_live_scripts; node != nullptr && event_frame < 0;
                         node = node->next)
                    {
                        if (node->type == 0xAD && node->word_c == gbtg::u16(chair_tag))
                        {
                            event_frame = held;
                            std::cout << "event 0xAD with the chair's tag queued at frame " << held << "\n";
                        }
                    }
                    gbtg::gfx_end_frame();
                    if (held % 30 == 0)
                    {
                        std::cout << "chair frame " << held << ": distance " << distance << "\n";
                    }
                    ++held;
                }
                std::cout << "chair section held " << held << " frames, then title " << game.scene_name << "\n";
                if (picked_frame < 0)
                {
                    throw gbtg::Error("A beside the chair did not pick it up");
                }
                // The TimeOut ends the section after 5 s (frame 299 at 1/60 s); ObjectPickedUp (opcode 3) ends it
                // once its delay (record +0xC) has run out with the event queued.
                const int timeout_frames = int(5.f / player_step + .5f);
                if (event_frame < picked_frame || game.scene_name != "Event Door Intro" || held >= timeout_frames - 5)
                {
                    throw gbtg::Error("the chair's section did not move on through the pickup (event 0xAD) before its "
                        "5 s timeout: title " + game.scene_name);
                }
                std::cout << "PASS: A beside the chair picks it up and the section moves on through event 0xAD\n";
                // Play on: the door opens, a second wave comes in for the script's "Kill All" event, the exit door
                // opens, Cooper walks into region 2 for the butler's "leave room" pages, then out through the exit
                // (0x00140E30's event 0xB0), and the script asks for the next scene.
                const std::string scene_script = game.scene_script.name;
                for (int frame = 0; frame < 14400 && game.scene_script.name == scene_script; ++frame)
                {
                    play_frame(frame, "player_after_");
                    if (game.scene_name != title)
                    {
                        std::cout << "after frame " << frame << ": title " << game.scene_name << "\n";
                        title = game.scene_name;
                    }
                }
                std::cout << "after: script " << game.scene_script.name << ", title " << game.scene_name << "\n";
                if (game.scene_script.name == scene_script)
                {
                    throw gbtg::Error("the play-on stalled with the opening script still running: title "
                        + game.scene_name);
                }
                if (game.scene_script.name != "aid_script_ghoulies_chapter1_scene2_1viewcam")
                {
                    throw gbtg::Error("the opening scene led to " + game.scene_script.name);
                }
                std::cout << "PASS: the opening scene plays to its end and the next scene ("
                          << game.scene_script.name << ") starts\n";
                // The book turns its page, then the chapter title (0x0012B810: the chapter number and name and the
                // "transitionMenu_basic" prompts) holds the transition until A; A moves it on to phase 2, the load.
                bool titled = false;
                bool moved_on = false;
                for (int frame = 0; frame < player_ticks(1200) && !moved_on; ++frame)
                {
                    gbtg::InputState input;
                    input.accept = titled && frame % player_ticks(30) < player_ticks(5);
                    gbtg::gfx_begin_frame();
                    game.tick(player_step, input);
                    if (!titled && gbtg::g_scene_transition.phase == 1 && gbtg::g_scene_transition.title_widget != nullptr)
                    {
                        titled = true;
                        std::cout << "chapter title shows at frame " << frame << "\n";
                    }
                    if (titled && frame % player_ticks(30) == player_ticks(29))
                    {
                        gbtg::RgbaImage capture;
                        if (gbtg::gfx_readback(capture))
                        {
                            gbtg::write_bmp("cache/player_chapter_title.bmp", capture);
                        }
                    }
                    moved_on = titled && gbtg::g_scene_transition.phase == 2;
                    gbtg::gfx_end_frame();
                }
                if (!titled || !moved_on)
                {
                    throw gbtg::Error(std::string("the chapter title ") + (titled ? "did not move on after A"
                        : "never showed"));
                }
                std::cout << "PASS: the chapter title shows after the page turn and A moves the transition on\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            // The script's later fight node (type 0x55, objTag_Actor_Imp) switches to the imps' fight music.
            std::optional<gbtg::ScriptNode> fight;
            for (auto node = gbtg::script_node_at(game.scene_script, 0); node && node->type != 0;
                 node = gbtg::script_node_at(game.scene_script, node->link))
            {
                if (node->type == 0x55 && gbtg::node_cstr(game.scene_script, *node, 8) == "objTag_Actor_Imp")
                {
                    fight = node;
                    break;
                }
            }
            if (!fight)
            {
                throw gbtg::Error("opening script has no imp fight music node");
            }
            gbtg::script_handle_event_actions(game, *fight);
            for (int frame = 0; frame < 30; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(player_step, {});
                gbtg::gfx_end_frame();
            }
            auto& streams = game.factory_scene->controls->context.streams();
            const auto* music = gbtg::find_tracked_audio_bank("XACT_SOUNDBANK_GSTREAMSFIGHT_0003_IMPS", streams);
            const auto* walkabout = gbtg::find_tracked_audio_bank(
                "XACT_SOUNDBANK_GSTREAMSWALK_WALKABOUT37", streams);
            if (music == nullptr || std::int8_t(music->channel) == -1)
            {
                throw gbtg::Error("imp fight music did not acquire a stream channel");
            }
            const auto& fight_channel = streams.channels[size_t(std::int8_t(music->channel))];
            if (fight_channel.mode != 2 || streams.voices.voices[size_t(fight_channel.voice)].state != 6)
            {
                throw gbtg::Error("imp fight music is not playing");
            }
            // Priority 3 pauses the priority-2 walkabout stream (0x00038AD0).
            if (walkabout != nullptr && std::int8_t(walkabout->channel) != -1
                && streams.channels[size_t(std::int8_t(walkabout->channel))].mode != 3)
            {
                throw gbtg::Error("walkabout music did not pause under the fight music");
            }
            int free_voices = 0;
            for (const auto& voice : streams.voices.voices)
            {
                free_voices += voice.state == 0 ? 1 : 0;
            }
            if (free_voices < 24)
            {
                throw gbtg::Error("finished actor sounds did not return their event voices");
            }
            std::cout << "PASS: the left stick walks Cooper after the tutorial, then the imp fight music starts\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (ui_test)
        {
            const auto load_icon = [&game](const char* name)
            {
                const gbtg::BundleEntry* entry = game.aids.load_aid(name);
                gbtg::TextureHeader header;
                if (entry == nullptr || entry->cpu.size() < 28 || !gbtg::parse_texture_header(entry->cpu, header))
                {
                    throw gbtg::Error(std::string("story dialogue icon was not loaded: ") + name);
                }
                std::span<const gbtg::u8> gpu = entry->gpu;
                if (!entry->gpu_chunks.empty())
                {
                    const auto& chunk = entry->gpu_chunks[0];
                    if (size_t(chunk.offset) + chunk.size <= entry->gpu.size())
                    {
                        gpu = entry->gpu.subspan(chunk.offset, chunk.size);
                    }
                }
                return gbtg::decode_xbox_texture(header, gpu);
            };
            gbtg::RgbaImage icon = load_icon("aid_texture_joypad_buttons_a");
            const gbtg::RgbaImage other = load_icon("aid_texture_joypad_buttons_b");
            gbtg::RgbaImage backdrop;
            backdrop.width = 1;
            backdrop.height = 1;
            backdrop.bgra = {0, 0, 0, 255};
            auto capture = [&]()
            {
                gbtg::gfx_begin_frame();
                gbtg::gfx_sprite_submit(backdrop, 0.f, 0.f, 640.f, 480.f);
                gbtg::gfx_sprite_submit(icon, 288.f, 208.f, 64.f, 64.f);
                gbtg::gfx_end_frame();
                gbtg::RgbaImage image;
                if (!gbtg::gfx_readback(image))
                {
                    throw gbtg::Error("UI sprite readback failed");
                }
                return image;
            };
            const auto expected = capture();
            // Seed the same image address with another uploaded button, as can
            // happen when the scene's UI storage is reused after Game Select.
            gbtg::gfx_release_sprite(icon);
            icon = other;
            const auto substituted = capture();
            if (substituted.bgra == expected.bgra)
            {
                throw gbtg::Error("UI cache regression did not seed a different glyph");
            }
            gbtg::gfx_release_sprite(icon);
            icon = load_icon("aid_texture_joypad_buttons_a");
            const auto restored = capture();
            if (restored.bgra != expected.bgra)
            {
                throw gbtg::Error("UI reload retained the previous button's GPU texture");
            }
            std::cout << "PASS: story A glyph restored after UI texture storage reuse\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (feature_test)
        {
#if !defined(GBTG_RETAIL_GAME_ONLY)
            gbtg::plugins_boot(game); // Plugin stories read their labels from the game's loctext.
#endif
            const char* wanted_text = std::getenv("GBTG_FEATURE");
            const std::string wanted = wanted_text != nullptr ? wanted_text : "Replay Chapter";
            const bool back = std::getenv("GBTG_FEATURE_BACK") != nullptr;
            const auto text_of = [](const gbtg::ButtonBatMenuItem& entry)
            {
                std::string text;
                for (const char16_t* c = entry.text.data(); *c != 0; ++c)
                {
                    text += char(*c < 0x80 ? *c : '?');
                }
                return text;
            };
            const auto selected_text = [&text_of]() -> std::string
            {
                const auto* menu = gbtg::g_frontend.bat_menu;
                if (menu == nullptr || menu->selected < 0 || size_t(menu->selected) >= menu->items.size())
                {
                    return {};
                }
                return text_of(*menu->items[size_t(menu->selected)]);
            };
            int ready = -1;
            int features_at = -1;
            int chosen_at = -1;
            int page_at = -1;
            int acted_at = -1;
            gbtg::u32 page_row = 0;
            std::string started;
            for (int frame = 0; frame < 6000; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                const int at = ready < 0 ? -1 : frame - ready;
                const bool info = wanted == "@info";
                // "@info": Y (Info) on a finished game's Play panel instead of the features.
                input.accept = at == 180 || (!info && at == 900);
                input.right = !info && at == 700;
                if (info && at == 700)
                {
                    input.pads[0].buttons |= 0x0800; // Y
                    chosen_at = frame;
                }
                const auto* control = gbtg::find_runtime_scene_control(0x9A);
                if (features_at < 0 && gbtg::g_frontend.features_widget != nullptr
                    && gbtg::g_frontend.features_widget->state == 2)
                {
                    features_at = frame;
                    for (const auto& item : gbtg::g_frontend.bat_menu->items)
                    {
                        std::cout << "feature \"" << text_of(*item) << "\"" << std::endl;
                    }
                }
                if (features_at >= 0 && chosen_at < 0 && (frame - features_at) % 100 == 50)
                {
                    if (selected_text() == wanted)
                    {
                        input.accept = true;
                        chosen_at = frame;
                        std::cout << "A on \"" << wanted << "\"\n";
                    }
                    else if (frame - features_at > 1500)
                    {
                        throw gbtg::Error("the features menu has no \"" + wanted + "\"");
                    }
                    else
                    {
                        input.down = true;
                    }
                }
                if (chosen_at >= 0 && page_at < 0 && frame - chosen_at > 30 && control != nullptr
                    && control->gameselect->current_row != 8 && control->gameselect->current_row != 3
                    && control->gameselect->requested_row == 0)
                {
                    page_at = frame;
                    page_row = control->gameselect->current_row;
                    std::cout << "row " << page_row << " open at +" << at << "\n";
                }
                if (page_at >= 0 && frame - page_at == 300)
                {
                    const auto* menu = gbtg::g_frontend.bat_menu;
                    if (menu != nullptr)
                    {
                        std::cout << "page items " << menu->items.size() << "\n";
                        for (const auto& item : menu->items)
                        {
                            std::cout << "  \"" << text_of(*item) << "\"" << (item->icon != nullptr ? " (icon)" : "")
                                      << "\n";
                        }
                    }
                    if (back)
                    {
                        input.back = true;
                    }
                    else
                    {
                        input.accept = true;
                    }
                    acted_at = frame;
                }
                if (info && page_at >= 0 && frame - page_at > 300)
                {
                    // The Super Scary page's OK waits until its count is shown: A every 100 frames until row 3.
                    input.accept = (frame - page_at) % 100 == 0;
                    if (control != nullptr && control->gameselect->current_row == 3)
                    {
                        std::cout << "row 3 back at +" << frame - page_at << "\n";
                        break;
                    }
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (page_at >= 0 && (frame - page_at == 60 || frame - page_at == 250))
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_feature_" + std::to_string(frame - page_at) + ".bmp", image);
                    }
                }
                if (features_at >= 0 && frame - features_at == 60)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_features_menu.bmp", image);
                    }
                }
                if (chosen_at >= 0 && game.scene_script.name.find("gameselect") == std::string::npos)
                {
                    started = game.scene_script.name; // A feature may start a script rather than a page.
                    // GBTG_FEATURE_PLAY=N plays the started script N frames more and captures it
                    // (cache/gameselect_feature_play.bmp), for a feature that starts a story.
                    const char* play = std::getenv("GBTG_FEATURE_PLAY");
                    for (int more = play != nullptr ? std::atoi(play) : 0; more > 0; --more)
                    {
                        gbtg::gfx_begin_frame();
                        game.tick(1.f / 59.f, gbtg::InputState{});
                        gbtg::gfx_end_frame();
                    }
                    if (play != nullptr)
                    {
                        gbtg::RgbaImage image;
                        if (gbtg::gfx_readback(image))
                        {
                            gbtg::write_bmp("cache/gameselect_feature_play.bmp", image);
                        }
                        std::cout << "played " << play << " frames: scene " << game.scene_name << "\n";
                    }
                    break;
                }
                if (acted_at >= 0 && frame - acted_at == 600)
                {
                    break;
                }
            }
            if (page_at < 0 && started.empty())
            {
                throw gbtg::Error("A on \"" + wanted + "\" opened no page");
            }
            const auto* control = gbtg::find_runtime_scene_control(0x9A);
            std::cout << "after " << (back ? "B" : "A") << ": "
                      << (started.empty() ? "row " + std::to_string(control == nullptr ? 0u
                              : control->gameselect->current_row)
                                          : "script " + started)
                      << "\n";
            std::cout << "unlock bits " << int(gbtg::g_runtime_profile.unlocks.bytes[0]) << ", saved "
                      << int(gbtg::save_game_unlocks(gbtg::g_frontend.slot_names[0].data())->bytes[0]) << "\n";
            std::cout << "PASS: \"" << wanted << "\" opened row " << page_row << "\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (challenges_test)
        {
            int ready = -1;
            int menu_at = -1;
            int moved_at = -1;
            int intro_at = -1;
            int played_at = -1;
            int started_at = -1;
            gbtg::i32 first = -1;
            gbtg::i32 moved = -1;
            std::string started;
            int menu_shown_at = -1;
            int back_at = -1;
            int restarted_at = -1;
            // GBTG_CHALLENGE (1..20) turns the carousel right to that challenge and plays it without its Rules page.
            const char* target_text = std::getenv("GBTG_CHALLENGE");
            const int target = target_text != nullptr ? std::atoi(target_text) - 1 : -1;
            // GBTG_CHALLENGE_FIGHT hands the running challenge to the scene driver (fights at any range), which
            // closes its pages and quits from its restart menu.
            const bool fight = std::getenv("GBTG_CHALLENGE_FIGHT") != nullptr;
            // GBTG_CHALLENGE_BLACK presses Black on the challenges and checks the bonus-book scenes (row 11).
            const bool black = std::getenv("GBTG_CHALLENGE_BLACK") != nullptr;
            int black_at = -1;
            // GBTG_CHALLENGE_BOOKS marks every bonus book found in the copied save's challenge block and its
            // challenge-21 progress bit (in memory, before its slot opens), a driver cheat that unlocks challenges
            // 16..21.
            const bool all_books = std::getenv("GBTG_CHALLENGE_BOOKS") != nullptr;
            bool fought = false;
            for (int frame = 0; frame < (target < 0 ? 12000 : 15000) && back_at < 0; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                const int at = ready < 0 ? -1 : frame - ready;
                const auto* contents = gbtg::g_frontend.contents_widget;
                const auto* menu = gbtg::g_frontend.challenges_widget;
                input.accept = at == 180;
                if (all_books && at == 170)
                {
                    gbtg::RuntimeProfileBlock* books = gbtg::save_game_challenges(gbtg::g_frontend.slot_names[0].data());
                    if (books == nullptr)
                    {
                        throw gbtg::Error("the copied save has no challenge block to mark");
                    }
                    std::fill(books->bytes.begin(), books->bytes.end(), gbtg::u8(0xFF));
                    // The progress bit 0x25 (byte 4, 0x20) shows challenge 21 (0x000D5880's count of 21).
                    gbtg::RuntimeProfileBlock* progress = gbtg::save_game_progress(gbtg::g_frontend.slot_names[0].data());
                    if (progress != nullptr)
                    {
                        progress->bytes[4] |= 0x20;
                    }
                }
                if (contents != nullptr && menu == nullptr && 700 <= at && at % 100 == 0)
                {
                    // Right until the Challenges panel (2) is selected, then A.
                    if (contents->chapter_index == 2)
                    {
                        input.accept = true;
                    }
                    else
                    {
                        input.right = true;
                    }
                }
                if (black && menu_at >= 0)
                {
                    // Black on the challenges opens the bonus-book scenes (row 11); B there goes back.
                    const int since = frame - menu_at;
                    if (since == 100)
                    {
                        input.pads[0].buttons |= 0x1000; // Black
                    }
                    const auto* control = gbtg::find_runtime_scene_control(0x9A);
                    if (black_at < 0 && control != nullptr && control->gameselect->current_row == 11
                        && gbtg::g_frontend.bat_menu != nullptr)
                    {
                        black_at = frame;
                    }
                    if (black_at >= 0 && frame - black_at == 300)
                    {
                        const auto& items = gbtg::g_frontend.bat_menu->items;
                        size_t icons = 0;
                        for (const auto& item : items)
                        {
                            icons += item->icon != nullptr ? 1 : 0;
                        }
                        std::cout << "row 11: " << items.size() << " scenes, " << icons << " with the bonus-book icon\n";
                        input.back = true;
                        gbtg::RgbaImage image;
                        if (gbtg::gfx_readback(image))
                        {
                            gbtg::write_bmp("cache/gameselect_bonus_books.bmp", image);
                        }
                    }
                    if (black_at >= 0 && frame - black_at > 300 && control != nullptr
                        && control->gameselect->current_row == 6 && menu != nullptr && menu->state == 2)
                    {
                        std::cout << "PASS: Black opened the bonus-book scenes (row 11) and B went back to the challenges\n";
                        gbtg::gfx_shutdown();
                        return 0;
                    }
                    hold_gameselect_press(game, input, ready);
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 59.f, input);
                    gbtg::gfx_end_frame();
                    continue;
                }
                if (menu_at < 0 && menu != nullptr && menu->state == 2)
                {
                    menu_at = frame;
                    first = gbtg::g_challenge_selection.selected;
                    std::cout << "challenges open at +" << at << ", challenge " << first + 1 << " of " << menu->count
                              << "\n";
                }
                if (menu_at >= 0 && target >= 0)
                {
                    const int since = frame - menu_at;
                    if (moved_at < 0 && menu != nullptr && menu->state == 2)
                    {
                        if (gbtg::g_challenge_selection.selected == target)
                        {
                            moved_at = frame;
                            moved = target;
                            intro_at = frame - 150; // No Rules page: A follows once the menu is still.
                            std::cout << "at challenge " << moved + 1 << "\n";
                        }
                        else
                        {
                            input.right = since % 120 == 100;
                        }
                    }
                    if (intro_at >= 0 && played_at < 0 && (frame - intro_at) % 100 == 0 && 200 <= frame - intro_at
                        && menu != nullptr && menu->state == 2)
                    {
                        input.accept = true;
                        played_at = frame;
                    }
                }
                else if (menu_at >= 0)
                {
                    const int since = frame - menu_at;
                    input.right = since == 100;
                    if (moved_at < 0 && since > 100 && menu != nullptr && menu->state == 2)
                    {
                        moved_at = frame;
                        moved = gbtg::g_challenge_selection.selected;
                        std::cout << "Right moved to challenge " << moved + 1 << " at +" << since << "\n";
                    }
                    if (moved_at >= 0)
                    {
                        const int after = frame - moved_at;
                        if (after == 100)
                        {
                            input.pads[0].buttons |= 0x0800; // Y, the intro page
                        }
                        if (intro_at < 0 && after > 100 && menu != nullptr && menu->state == 4)
                        {
                            intro_at = frame;
                            std::cout << "intro page at +" << after << "\n";
                        }
                        if (intro_at >= 0 && (frame - intro_at) % 50 == 0 && played_at < 0)
                        {
                            std::cout << "intro +" << frame - intro_at << ": menu state "
                                      << (menu == nullptr ? -1 : menu->state) << "\n";
                        }
                        // A closes the page; once the menu is back (state 2), A plays the challenge.
                        if (intro_at >= 0 && played_at < 0 && (frame - intro_at) % 100 == 0 && 200 <= frame - intro_at)
                        {
                            input.accept = true;
                            if (menu != nullptr && menu->state == 2)
                            {
                                played_at = frame;
                                std::cout << "A on the challenge at intro +" << frame - intro_at << "\n";
                            }
                        }
                    }
                }
                if (fight && started_at >= 0 && !fought && gbtg::g_challenge_run.state == 3)
                {
                    fought = true;
                    for (int scene = 0; scene < 40 && game.scene_script.name.find("gameselect") == std::string::npos;
                         ++scene)
                    {
                        std::cout << "challenge " << target + 1 << " scene " << scene << ": " << game.scene_script.name
                                  << " (state " << gbtg::g_challenge_run.state << ")\n";
                        play_scene7(game, "challenge " + std::to_string(target + 1), 1000.f);
                    }
                    menu_shown_at = frame - 1000; // Past the B the plain run presses.
                }
                if (started_at >= 0)
                {
                    // The results page closes with A; on the restart menu (state 6) B quits to game select.
                    const int run_state = gbtg::g_challenge_run.state;
                    if (run_state == 5 && (frame - started_at) % 100 == 0)
                    {
                        input.accept = true;
                    }
                    if (run_state == 6 && menu_shown_at < 0)
                    {
                        menu_shown_at = frame;
                    }
                    if (challenge_restart_test && restarted_at < 0 && menu_shown_at >= 0)
                    {
                        // A restarts; the next run's start (state 3) re-arms the menu wait for its B.
                        input.accept = frame - menu_shown_at == 150;
                        if (frame - menu_shown_at > 150 && run_state == 3)
                        {
                            restarted_at = frame;
                            menu_shown_at = -1;
                            std::cout << "the challenge restarted at +" << frame - started_at << "\n";
                        }
                    }
                    else
                    {
                        input.back = menu_shown_at >= 0 && frame - menu_shown_at == 150;
                    }
                    if (menu_shown_at >= 0 && gbtg::g_frontend.challenges_widget != nullptr
                        && gbtg::g_frontend.challenges_widget->state == 2 && back_at < 0)
                    {
                        back_at = frame;
                    }
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (menu_shown_at >= 0 && frame - menu_shown_at == 100)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/challenge_run_restart_menu.bmp", image);
                    }
                }
                if (played_at >= 0 && started_at < 0 && game.scene_script.name.find("gameselect") == std::string::npos)
                {
                    started_at = frame;
                    started = game.scene_script.name;
                }
                if (started_at >= 0)
                {
                    static int last_state = -100;
                    const int run_state = gbtg::g_challenge_run.state;
                    if (run_state != last_state)
                    {
                        std::cout << "challenge state " << run_state << " (mode " << gbtg::g_challenge_run.mode
                                  << ", score " << gbtg::g_challenge_run.score << ", tier " << gbtg::g_challenge_run.tier
                                  << ") at +" << frame - started_at << " in " << game.scene_script.name << "\n";
                        last_state = run_state;
                        gbtg::RgbaImage image;
                        if (gbtg::gfx_readback(image))
                        {
                            gbtg::write_bmp("cache/challenge_run_state" + std::to_string(run_state) + ".bmp", image);
                        }
                    }
                    if ((frame - started_at) % 600 == 300)
                    {
                        gbtg::RgbaImage image;
                        if (gbtg::gfx_readback(image))
                        {
                            gbtg::write_bmp("cache/challenge_run_" + std::to_string(frame - started_at) + ".bmp", image);
                        }
                    }
                }
                const bool capture = (menu_at >= 0 && (frame - menu_at == 1 || frame - menu_at == 60))
                    || (moved_at >= 0 && frame - moved_at == 30) || (intro_at >= 0 && frame - intro_at == 120)
                    || (played_at >= 0 && (frame - played_at == 20 || frame - played_at == 60));
                if (capture)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_challenges_" + std::to_string(at) + ".bmp", image);
                    }
                    std::cout << "capture +" << at << " (frame " << frame << ")\n";
                }
            }
            if (menu_at < 0)
            {
                throw gbtg::Error("A on the Challenges panel did not open the challenges (row 6)");
            }
            if (moved_at < 0 || (target < 0 && moved == first))
            {
                throw gbtg::Error("Right in the challenges did not move to another challenge");
            }
            if (intro_at < 0 && target < 0)
            {
                throw gbtg::Error("Y in the challenges did not show the intro page");
            }
            if (started_at < 0)
            {
                throw gbtg::Error("A on the challenge did not start its script");
            }
            if (menu_shown_at < 0)
            {
                throw gbtg::Error("the challenge never reached its restart menu (DAT_004E17C8 6)");
            }
            if (back_at < 0)
            {
                throw gbtg::Error("B on the restart menu did not return to the challenges (row 6)");
            }
            if (challenge_restart_test && restarted_at < 0)
            {
                throw gbtg::Error("A on the restart menu did not run the challenge again");
            }
            gbtg::i32 best = 0;
            std::memcpy(&best, gbtg::g_game_progress.words_18f8.data() + size_t(moved) * 4, 4);
            gbtg::i32 trophy = 0;
            std::memcpy(&trophy, gbtg::g_game_progress.words_194c.data() + size_t(moved) * 4, 4);
            std::cout << "challenge " << moved + 1 << " best " << best << ", trophy word " << trophy << "\n";
            // 0x0004B070's save (0x000F2E20) wrote the words into the save's record (progress.xsv +0x1B4, +0x208).
            bool saved = false;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
            {
                if (entry.is_regular_file() && entry.path().filename() == "progress.xsv")
                {
                    std::ifstream in(entry.path(), std::ios::binary);
                    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    gbtg::i32 file_best = 0;
                    gbtg::i32 file_trophy = 0;
                    std::memcpy(&file_best, bytes.data() + 0x1B4 + size_t(moved) * 4, 4);
                    std::memcpy(&file_trophy, bytes.data() + 0x208 + size_t(moved) * 4, 4);
                    std::cout << "saved best " << file_best << ", trophy word " << file_trophy << "\n";
                    saved = file_best == best && file_trophy == trophy;
                }
            }
            if (!saved)
            {
                throw gbtg::Error("the challenge's best score and trophy were not written to progress.xsv");
            }
            {
                gbtg::RgbaImage image;
                if (gbtg::gfx_readback(image))
                {
                    gbtg::write_bmp("cache/challenge_run_back.bmp", image);
                }
            }
            std::cout << "PASS: the challenges opened, moved, showed the intro, played " << started
                      << ", showed its results and restart menu, and B went back to the challenges\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (loading_test)
        {
            int wait_at = -1;
            int closed_at = -1;
            int slots_at = -1;
            for (int frame = 0; frame < 6000; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                const auto* wait = gbtg::g_frontend.stream_wait_widget;
                if (wait_at < 0 && wait != nullptr)
                {
                    wait_at = frame;
                    gbtg::u32 left = 0;
                    gbtg::u32 total = 0;
                    const auto& rows = gbtg::native_audio_runtime().wavebank_streams;
                    for (size_t row = 0; row < rows.ids.size(); ++row)
                    {
                        if (rows.ids[row] == 2 && rows.jobs[row] != nullptr)
                        {
                            gbtg::runtime_file_copy_progress(rows.jobs[row], left, total, *gbtg::g_runtime_file_copies);
                        }
                    }
                    std::cout << "loading screen open at frame " << frame << ", wait " << wait->wait << ", stream 2 "
                              << left << " of " << total << " bytes left\n";
                }
                if (wait_at >= 0 && closed_at < 0 && wait == nullptr)
                {
                    closed_at = frame;
                    std::cout << "loading screen closed at frame " << frame << "\n";
                }
                if (closed_at >= 0 && slots_at < 0 && gbtg::g_frontend.slot_widget != nullptr)
                {
                    slots_at = frame;
                    std::cout << "slot select at frame " << frame << "\n";
                }
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (wait_at >= 0 && closed_at < 0 && (frame - wait_at == 20 || frame - wait_at == 80 || frame - wait_at == 140 || frame - wait_at == 200))
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_loading_" + std::to_string(frame - wait_at) + ".bmp", image);
                    }
                }
                // The copy runs on after slot select; the run ends once it is done, so the utility drive keeps a
                // current copy (an interrupted one is copied again at the next boot).
                if (slots_at >= 0 && frame - slots_at > 120 && gbtg::g_runtime_file_copies->state.copy_head.next == nullptr)
                {
                    std::cout << "the wavebank copies finished by frame " << frame << "\n";
                    break;
                }
            }
            if (wait_at < 0)
            {
                throw gbtg::Error("game select did not open on the loading screen while stream 2 copied");
            }
            if (gbtg::g_runtime_file_copies->state.copy_head.next != nullptr)
            {
                throw gbtg::Error("the wavebank copies did not finish");
            }
            if (slots_at < 0)
            {
                throw gbtg::Error("the loading screen did not give way to slot select");
            }
            std::cout << "PASS: game select showed the loading screen while stream 2 copied, then slot select\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (secret_test)
        {
            const auto text_of = [](const gbtg::ButtonBatMenuItem& entry)
            {
                std::string text;
                for (const char16_t* c = entry.text.data(); *c != 0; ++c)
                {
                    text += char(*c < 0x80 ? *c : '?');
                }
                return text;
            };
            const std::array<gbtg::u16, 7> presses{0x40, 0x80, 0x2000, 0x1000, 0x2000, 0x80, 0x40};
            int ready = -1;
            int contents_at = -1;
            int unlocked_at = -1;
            int features_at = -1;
            int chosen_at = -1;
            std::string artwork_script;
            int gallery_open_at = -1;
            int& gallery_open_at_input = gallery_open_at;
            int gallery_state = -1;
            int gallery_page = -1;
            int turned_page = -1;
            int left_at = -1;
            int returned_at = -1;
            int wait_seen = -1;
            int wait_closed = -1;
            for (int frame = 0; frame < 7000; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                const int at = ready < 0 ? -1 : frame - ready;
                auto& frontend = gbtg::g_frontend;
                if (contents_at < 0 && frontend.contents_widget != nullptr && frontend.contents_widget->state == 2)
                {
                    contents_at = frame;
                    std::cout << "contents open at frame " << frame << ", sequence "
                              << (frontend.contents_widget->sequence != nullptr ? "armed" : "missing") << "\n";
                }
                input.accept = at == 180 || at == 420 || at == 900;
                if (contents_at >= 0 && unlocked_at < 0)
                {
                    const int since = frame - contents_at;
                    if (since >= 100)
                    {
                        input.pads[0].buttons |= 0xC000; // Both triggers.
                    }
                    const int step = (since - 140) / 24;
                    if (since >= 140 && step < int(presses.size()) && (since - 140) % 24 < 6)
                    {
                        input.pads[0].buttons |= presses[size_t(step)];
                    }
                }
                if (unlocked_at < 0 && (gbtg::g_runtime_profile.progress.bytes[7] & 0x20) != 0)
                {
                    unlocked_at = frame;
                    std::cout << "progress bit 0x3D set at frame " << frame << "\n";
                }
                input.right = unlocked_at >= 0 && frame - unlocked_at == 200;
                input.accept = input.accept || (unlocked_at >= 0 && frame - unlocked_at == 400);
                if (features_at < 0 && frontend.features_widget != nullptr && frontend.features_widget->state == 2)
                {
                    features_at = frame;
                    std::cout << "features:";
                    for (const auto& item : frontend.bat_menu->items)
                    {
                        std::cout << " \"" << text_of(*item) << "\"";
                    }
                    std::cout << "\n";
                }
                if (gallery_open_at_input >= 0)
                {
                    const int since = frame - gallery_open_at_input;
                    if (since >= 200 && since < 212)
                    {
                        input.pads[0].lx = 30000; // The stick right.
                    }
                    if (since >= 900 && since < 912)
                    {
                        input.back = true;
                    }
                }
                if (features_at >= 0 && chosen_at < 0)
                {
                    const int since = frame - features_at;
                    if (since % 100 == 50)
                    {
                        const auto* menu = frontend.bat_menu;
                        const bool on_artwork = menu != nullptr && menu->selected >= 0
                            && text_of(*menu->items[size_t(menu->selected)]) == "Gallery";
                        if (on_artwork)
                        {
                            input.accept = true;
                            chosen_at = frame;
                        }
                        else if (since < 2000)
                        {
                            input.down = true;
                        }
                    }
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (unlocked_at >= 0 && frame - unlocked_at == 30)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_secret_unlocked.bmp", image);
                    }
                }
                if (chosen_at >= 0 && artwork_script.empty()
                    && game.scene_script.name.find("unlockableart") != std::string::npos)
                {
                    artwork_script = game.scene_script.name;
                    std::cout << "artwork started " << artwork_script << " at frame " << frame << "\n";
                }
                // The Gallery (type 0x9F): open, the stick turns a page, then B turns back and leaves.
                const auto* gallery_control = gbtg::find_runtime_scene_control(0x9F);
                const gbtg::GalleryControlState* gallery = gallery_control != nullptr
                    ? gallery_control->gallery.get() : nullptr;
                if (gallery != nullptr && gallery_open_at < 0 && (gallery->state == 2 || gallery->state == 3))
                {
                    gallery_open_at = frame;
                    std::cout << "gallery open at frame " << frame << ", " << gbtg::count_runtime_gallery_pages(
                        *gbtg::g_app_error_services) << " pages\n";
                }
                if (gallery != nullptr)
                {
                    gallery_state = gallery->state;
                    gallery_page = gallery->page;
                }
                const auto capture_as = [](const std::string& name)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_secret_" + name + ".bmp", image);
                    }
                };
                if (gallery_open_at >= 0)
                {
                    const int since = frame - gallery_open_at;
                    if (since == 60)
                    {
                        capture_as("gallery_open");
                    }
                    if (since == 400)
                    {
                        std::cout << "after the stick: page " << gallery_page << ", state " << gallery_state << "\n";
                        turned_page = gallery_page;
                        capture_as("gallery_turned");
                    }
                    if (since == 900 && left_at < 0)
                    {
                        left_at = frame;
                    }
                }
                if (gbtg::g_frontend.stream_wait_widget != nullptr && wait_seen < 0)
                {
                    wait_seen = frame;
                    std::cout << "stream wait (row 18) open at frame " << frame << ", wait "
                              << gbtg::g_frontend.stream_wait_widget->wait << "\n";
                }
                if (wait_seen >= 0 && wait_closed < 0 && gbtg::g_frontend.stream_wait_widget == nullptr)
                {
                    wait_closed = frame;
                    std::cout << "stream wait closed at frame " << frame << "\n";
                }
                if (wait_seen >= 0 && frame - wait_seen == 30)
                {
                    capture_as("stream_wait");
                }
                if (left_at >= 0 && returned_at < 0 && game.scene_script.name == "aid_script_ghoulies_gameselect_gameselect"
                    && gbtg::g_transition_phase == 0)
                {
                    returned_at = frame;
                    std::cout << "back in game select at frame " << frame << "\n";
                }
                if (returned_at >= 0 && frame - returned_at == 200)
                {
                    capture_as("gallery_returned");
                }
                if (returned_at >= 0 && frame - returned_at > 200 && (wait_seen < 0 || wait_closed >= 0))
                {
                    break;
                }
            }
            if (unlocked_at < 0)
            {
                throw gbtg::Error("the secret sequence did not set the progress bit");
            }
            if (chosen_at < 0)
            {
                throw gbtg::Error("the features menu did not list Artwork after the secret sequence");
            }
            if (artwork_script.empty())
            {
                throw gbtg::Error("A on Artwork did not start aid_script_ghoulies_gameselect_unlockableart");
            }
            if (gallery_open_at < 0)
            {
                throw gbtg::Error("the Gallery (type 0x9F) did not open");
            }
            if (turned_page != 1)
            {
                throw gbtg::Error("the stick did not turn the Gallery to its second page");
            }
            if (returned_at < 0)
            {
                throw gbtg::Error("B in the Gallery did not return to game select");
            }
            std::cout << "PASS: the secret sequence unlocked the Gallery, its page turned and B went back to game "
                         "select\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (duplicate_name_test)
        {
            int ready = -1;
            int contents_at = -1;
            int slots_at = -1;
            int naming_at = -1;
            int done_at = -1;
            for (int frame = 0; frame < 4200; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                const int at = ready < 0 ? -1 : frame - ready;
                auto& frontend = gbtg::g_frontend;
                // contents_at: the first naming page has closed (Done saved the game).
                if (contents_at < 0 && at > 420 && frontend.naming_widget == nullptr)
                {
                    contents_at = frame;
                    std::cout << "first game named at frame " << frame << "\n";
                }
                if (contents_at >= 0 && slots_at < 0 && frontend.slot_widget != nullptr)
                {
                    slots_at = frame;
                    std::cout << "slot select again at frame " << frame << "\n";
                }
                if (slots_at >= 0 && naming_at < 0 && frontend.naming_widget != nullptr)
                {
                    naming_at = frame;
                    std::cout << "naming again at frame " << frame << "\n";
                }
                input.accept = at == 180 || at == 420 || (slots_at >= 0 && frame - slots_at == 500)
                    || (naming_at >= 0 && frame - naming_at == 300);
                input.right = slots_at >= 0 && frame - slots_at == 300;
                if (naming_at >= 0 && done_at < 0 && frame - naming_at == 300)
                {
                    done_at = frame;
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (done_at >= 0 && (frame - done_at == 60 || frame - done_at == 120 || frame - done_at == 240))
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_duplicate_" + std::to_string(frame - done_at) + ".bmp", image);
                    }
                }
                if (done_at >= 0 && frame - done_at > 300)
                {
                    break;
                }
            }
            if (done_at < 0)
            {
                throw gbtg::Error("the duplicate name test did not reach the second naming page");
            }
            int saves = 0;
            for (const auto& entry : std::filesystem::directory_iterator(test_save_folder()))
            {
                saves += entry.is_directory() ? 1 : 0;
            }
            std::cout << "save folders " << saves << ", naming open " << (gbtg::g_frontend.naming_widget != nullptr)
                      << "\n";
            if (gbtg::g_frontend.naming_widget == nullptr || saves != 1)
            {
                throw gbtg::Error("Done on a duplicate name left the naming page or saved a second game");
            }
            std::cout << "PASS: Done on a duplicate name kept the naming page open without a second save\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (corrupt_save_test)
        {
            const auto ascii = [](const char16_t* text)
            {
                std::string out;
                for (; *text != 0; ++text)
                {
                    out += char(*text < 0x80 ? *text : '?');
                }
                return out;
            };
            int ready = -1;
            int shown_at = -1;
            int closed_at = -1;
            for (int frame = 0; frame < 2400; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                auto& error = gbtg::g_app_error;
                if (shown_at < 0 && error.phase == 2)
                {
                    shown_at = frame;
                    std::cout << "app error shown at frame " << frame << ": \"" << ascii(error.message.data())
                              << "\" (title \"" << ascii(error.title.data()) << "\", A " << error.a_key << ")\n";
                }
                input.accept = shown_at >= 0 && frame - shown_at == 150;
                if (shown_at >= 0 && closed_at < 0 && error.phase == 4)
                {
                    closed_at = frame;
                    std::cout << "app error closed at frame " << frame << ", choice " << error.choice << ", flags "
                              << std::hex << gbtg::g_flags_ea9c << std::dec << "\n";
                }
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                const bool capture = (shown_at >= 0 && closed_at < 0
                    && (frame - shown_at == 60 || frame - shown_at == 140))
                    || (closed_at >= 0 && frame - closed_at == 200);
                if (capture)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        const int offset = closed_at >= 0 ? 1000 + frame - closed_at : frame - shown_at;
                        gbtg::write_bmp("cache/gameselect_corrupt_" + std::to_string(offset) + ".bmp", image);
                    }
                }
                if (closed_at >= 0 && frame - closed_at > 240)
                {
                    break;
                }
            }
            if (shown_at < 0)
            {
                throw gbtg::Error("the damaged save did not show the app error screen");
            }
            if (closed_at < 0 || (gbtg::g_flags_ea9c & 0x40000u) != 0)
            {
                throw gbtg::Error("A did not close the app error screen");
            }
            if (gbtg::g_frontend.slot_widget == nullptr)
            {
                throw gbtg::Error("slot select did not run on after the app error screen");
            }
            std::cout << "PASS: the damaged save showed the app error screen, A closed it and slot select ran on\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (retail_save_test)
        {
            const auto text_of = [](const gbtg::ButtonBatMenuItem& entry)
            {
                std::string text;
                for (const char16_t* c = entry.text.data(); *c != 0; ++c)
                {
                    text += char(*c < 0x80 ? *c : '?');
                }
                return text;
            };
            const auto selected_text = [&text_of]() -> std::string
            {
                const auto* menu = gbtg::g_frontend.bat_menu;
                if (menu == nullptr || menu->selected < 0 || size_t(menu->selected) >= menu->items.size())
                {
                    return {};
                }
                return text_of(*menu->items[size_t(menu->selected)]);
            };
            int ready = -1;
            int features_at = -1;
            int replay_at = -1;
            size_t scenes = 0;
            std::vector<std::string> pages;
            for (int frame = 0; frame < 4200; ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                }
                const int at = ready < 0 ? -1 : frame - ready;
                if (features_at < 0 && gbtg::g_frontend.features_widget != nullptr
                    && gbtg::g_frontend.features_widget->state == 2)
                {
                    features_at = frame;
                    std::cout << "features open at +" << at << "\n";
                }
                if (replay_at < 0 && gbtg::g_frontend.scene_replay_widget != nullptr
                    && gbtg::g_frontend.scene_replay_widget->state == 2)
                {
                    replay_at = frame;
                    scenes = gbtg::g_frontend.bat_menu->items.size();
                    std::cout << "scene replay open at +" << at << " with " << scenes << " scenes, first \""
                              << text_of(*gbtg::g_frontend.bat_menu->items.front()) << "\", last \""
                              << text_of(*gbtg::g_frontend.bat_menu->items.back()) << "\"\n";
                }
                input.accept = at == 180 || at == 900;
                input.right = at == 700;
                if (features_at >= 0 && replay_at < 0)
                {
                    // Down until Replay Scene is selected, then A.
                    const int since = frame - features_at;
                    if (since % 100 == 50)
                    {
                        if (selected_text() == "Replay Scene")
                        {
                            input.accept = true;
                        }
                        else
                        {
                            input.down = true;
                        }
                    }
                }
                if (replay_at >= 0)
                {
                    const int since = frame - replay_at;
                    input.page = since == 100 || since == 400; // X, Page Down
                    if (since == 700)
                    {
                        input.pads[0].buttons |= 0x0800; // Y, Page Up
                    }
                    input.back = since == 1000;
                    if (since == 390 || since == 690 || since == 990)
                    {
                        const auto* menu = gbtg::g_frontend.bat_menu;
                        pages.push_back(std::to_string(menu == nullptr ? -9 : menu->selected) + " \"" + selected_text()
                            + "\"");
                        std::cout << "after page " << pages.size() << ": " << pages.back() << "\n";
                    }
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                const bool capture = at == 300 || at == 650 || at == 1100 || (replay_at >= 0
                    && (frame - replay_at == 50 || frame - replay_at == 390 || frame - replay_at == 690
                        || frame - replay_at == 990 || frame - replay_at == 1300));
                if (capture)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        const std::string name = replay_at >= 0 ? "replay_" + std::to_string(frame - replay_at)
                                                                : std::to_string(at);
                        gbtg::write_bmp("cache/gameselect_retail_" + name + ".bmp", image);
                    }
                }
            }
            if (features_at < 0 || replay_at < 0)
            {
                throw gbtg::Error("the retail save did not reach the scene replay");
            }
            // 12 + 17 + 10 + 13 + 12 + 7 + 15 + 1 + 13 scenes (DAT_002FE670): a finished game with +0x260 lists 100.
            if (scenes != 100 || pages.size() != 3)
            {
                throw gbtg::Error("the retail save's scene replay listed " + std::to_string(scenes) + " scenes");
            }
            const auto* control = gbtg::find_runtime_scene_control(0x9A);
            if (control == nullptr || control->gameselect->current_row != 8)
            {
                throw gbtg::Error("B in the scene replay did not return to the features (row 8)");
            }
            std::cout << "PASS: the retail save loaded, its scene replay listed 100 scenes and paged, and B went back\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (gameselect_test)
        {
            // Start at frame 120 opens game select. Once its page turn has finished, the screens are captured at
            // fixed offsets: slot select, Right, A on the selected slot, then B.
            int ready = -1;
            int features_selected = -3;
            bool features_page = false;
            bool replay_opened = false;
            size_t replay_items = 0;
            std::string replay_item;
            gbtg::i32 paged_down = -3;
            gbtg::i32 paged_up = -3;
            size_t paged_visible = 0;
            for (int frame = 0; frame < (replay_paging_test ? 3100 : replay_scene_test ? 3000 : features_test ? 3200 : new_game_test ? 2700 : delete_test || stats_test ? 2000
                     : 1400); ++frame)
            {
                gbtg::InputState input;
                input.start = frame == 120;
                if (ready < 0 && game.scene_script.name.find("gameselect") != std::string::npos
                    && gbtg::g_transition_phase == 0)
                {
                    ready = frame;
                    std::cout << "game select ready at frame " << frame << "\n";
                }
                const int at = ready < 0 ? -1 : frame - ready;
                if (new_game_test)
                {
                    input.accept = at == 180 || at == 420 || at == 900 || at == 1300;
                }
                else if (replay_scene_test)
                {
                    // A on the saved game's slot opens its contents; Right, A to the features menu, Down twice, A on
                    // Replay Scene, then A on the first scene in its list.
                    input.accept = at == 180 || at == 900 || at == 1400 || (!replay_paging_test && at == 2000);
                    input.right = at == 700;
                    input.down = at == 1150 || at == 1250;
                    if (replay_paging_test)
                    {
                        input.page = at == 2000; // X, Page Down
                        if (at == 2300)
                        {
                            input.pads[0].buttons |= 0x0800; // Y, Page Up
                        }
                    }
                }
                else if (stats_test)
                {
                    // A names and saves a game and opens its contents; Y (Info, code 3) on Play shows the stats.
                    input.accept = at == 180 || at == 420 || at == 900;
                    if (at == 1300)
                    {
                        input.pads[0].buttons |= 0x0800;
                    }
                }
                else if (features_test)
                {
                    // A names and saves a game and opens its contents; Right reaches Features (the middle panel)
                    // and A opens it. Then Down, Down, A on Replay Scene, A on its page, and B.
                    input.accept = at == 180 || at == 420 || at == 900 || at == 1500 || at == 2000 || at == 2300;
                    input.right = at == 1300;
                    input.down = at == 1750 || at == 1850;
                    input.back = at == 2500;
                }
                else if (delete_test)
                {
                    // A names a game on the empty slot and Done saves it; X (Delete) on it, then A deletes it.
                    input.accept = at == 180 || at == 420 || at == 1100;
                    input.page = at == 900;
                }
                else
                {
                    input.right = at == 90;
                    input.accept = at == 180;
                    input.back = at == 360;
                }
                hold_gameselect_press(game, input, ready);
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (replay_scene_test && at == 1990 && gbtg::g_frontend.scene_replay_widget != nullptr
                    && gbtg::g_frontend.bat_menu != nullptr)
                {
                    // Row 10 is open with its scene list (the bat menu's items).
                    replay_opened = true;
                    const auto& items = gbtg::g_frontend.bat_menu->items;
                    replay_items = items.size();
                    if (!items.empty())
                    {
                        for (const char16_t* c = items.back()->text.data(); *c != 0; ++c)
                        {
                            replay_item += char(*c < 0x80 ? *c : '?');
                        }
                    }
                }
                if (replay_paging_test && (at == 2290 || at == 2590) && gbtg::g_frontend.bat_menu != nullptr)
                {
                    const auto& menu = *gbtg::g_frontend.bat_menu;
                    (at == 2290 ? paged_down : paged_up) = menu.selected;
                    if (at == 2290)
                    {
                        for (const auto& entry : menu.items)
                        {
                            paged_visible += entry->visible != 0 ? 1 : 0;
                        }
                    }
                }
                if ((features_test || replay_scene_test) && at == (replay_scene_test ? 1390 : 1990)
                    && gbtg::g_frontend.bat_menu != nullptr)
                {
                    features_selected = gbtg::g_frontend.bat_menu->selected;
                }
                if (features_test && at == 2150)
                {
                    features_page = game.script_dialogue != nullptr && game.script_dialogue->dialogue.state.mode != 0;
                }
                if (stats_test && at == 1400)
                {
                    // 0x000582E0 queued the page and the dialogue shows it (mode 2).
                    if (game.script_dialogue == nullptr || game.script_dialogue->dialogue.state.mode != 2)
                    {
                        throw gbtg::Error("Y on Play did not show the stats dialog");
                    }
                }
                if (gameselect_native_test && 0 <= at)
                {
                    // The last frame the native control drew before a raise stops it.
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_native_last.bmp", image);
                    }
                }
                const bool capture = replay_scene_test ? (at == 120 || at == 300 || at == 650 || at == 800 || at == 1100
                                         || at == 1350 || at == 1500 || at == 1700 || at == 1900 || at == 2100
                                         || at == 2400 || at == 2900 || at == 2200 || at == 2500)
                                   : features_test ? (at == 1420 || at == 1560 || at == 1700 || at == 1800
                                         || at == 1950 || at == 2150 || at == 2450 || at == 2700 || at == 3100)
                                   : stats_test ? (at == 1290 || at == 1330 || at == 1400 || at == 1500 || at == 1580)
                                   : delete_test ? (at == 800 || at == 1000 || at == 1200 || at == 1400 || at == 1700)
                                   : new_game_test ? (at == 300 || at == 500 || at == 800 || at == 960 || at == 1000 || at == 1100 || at == 1200 || at == 1290 || at == 1500 || at == 1800 || at == 2200)
                                                   : (at == 60 || at == 150 || at == 300 || at == 480);
                // GBTG_GAMESELECT_CAPTURE_EVERY=N[@first-last]: also every Nth frame (from `first` to `last`), to
                // follow the page turns.
                static const std::array<int, 3> capture_every = []()
                {
                    std::array<int, 3> every{0, 0, 1 << 30};
                    if (const char* text = std::getenv("GBTG_GAMESELECT_CAPTURE_EVERY"); text != nullptr)
                    {
                        every[0] = std::atoi(text);
                        if (const char* range = std::strchr(text, '@'); range != nullptr)
                        {
                            every[1] = std::atoi(range + 1);
                            if (const char* dash = std::strchr(range, '-'); dash != nullptr)
                            {
                                every[2] = std::atoi(dash + 1);
                            }
                        }
                    }
                    return every;
                }();
                if (capture || (capture_every[0] > 0 && capture_every[1] <= at && at <= capture_every[2]
                        && at % capture_every[0] == 0))
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        const std::string prefix = replay_paging_test ? "cache/gameselect_replay_paging_"
                                                 : replay_scene_test ? "cache/gameselect_replay_scene_"
                                                 : features_test ? "cache/gameselect_features_"
                                                 : stats_test ? "cache/gameselect_stats_"
                                                 : delete_test ? "cache/gameselect_delete_"
                                                 : new_game_test ? "cache/gameselect_new_game_"
                                                                 : "cache/gameselect_check_";
                        gbtg::write_bmp(prefix + std::to_string(at) + ".bmp", image);
                    }
                    std::cout << "game select capture +" << at << " (frame " << frame << ") scene "
                              << game.scene_script.name << "\n";
                }
            }
            if (ready < 0)
            {
                throw gbtg::Error("Start did not open game select");
            }
            if (replay_scene_test)
            {
                if (features_selected != 2)
                {
                    throw gbtg::Error("Down twice in the features menu selected item "
                        + std::to_string(features_selected));
                }
                if (!replay_opened)
                {
                    throw gbtg::Error("A on Replay Scene in a Chapter 1 Scene 2 save did not open the scene replay");
                }
                std::cout << "scene replay list: " << replay_items << " item(s), \"" << replay_item << "\"\n";
                if (replay_paging_test)
                {
                    std::cout << "page down selected " << paged_down << " with " << paged_visible
                              << " visible; page up selected " << paged_up << "\n";
                    if (replay_items != 9 || paged_down != 8 || paged_visible != 1 || paged_up != 0)
                    {
                        throw gbtg::Error("the scene replay's pages are wrong");
                    }
                    std::cout << "PASS: the scene replay listed nine scenes, paged down to the ninth and back up\n";
                    gbtg::gfx_shutdown();
                    return 0;
                }
                // Scene 2's save can replay one scene: ordinal 0, chapter 1's book and scene 1.
                if (replay_items != 1)
                {
                    throw gbtg::Error("the scene replay listed " + std::to_string(replay_items) + " scenes, not 1");
                }
                // 0x000E3130 holds game select, the scene's scripts and game select, and starts the first script.
                const auto& order = gbtg::g_frontend.scene_order_heap;
                std::vector<std::string> held;
                for (size_t offset = 0; offset + 4 <= order.size()
                     && std::memcmp(order.data() + offset, "aid_", 4) == 0; offset += 0x80)
                {
                    held.emplace_back(reinterpret_cast<const char*>(order.data() + offset));
                    std::cout << "held scene order [" << held.size() - 1 << "] " << held.back() << "\n";
                }
                if (held.size() < 3 || held.front() != "aid_script_ghoulies_gameselect_gameselect"
                    || held.back() != "aid_script_ghoulies_gameselect_gameselect")
                {
                    throw gbtg::Error("the replay did not hold game select around the scene's scripts");
                }
                if (game.scene_script.name != held[1])
                {
                    throw gbtg::Error("the replay did not start " + held[1] + " (scene " + game.scene_script.name + ")");
                }
                std::cout << "PASS: Replay Scene in a Chapter 1 Scene 2 save listed scene one and replayed it from "
                          << game.scene_script.name << "\n";
                // Run on through the held order: the books end through the script VM (0x00117140), whose next
                // scene comes from the held order (0x00035960 over DAT_00511B64), with A for their pages and titles.
                // The opening's fight is --player-opening's: once its scene runs, the VM's done latch (DAT_0050E788)
                // is set as its last event would, and the VM must pick game select, the held order's next entry.
                std::vector<std::string> visited{game.scene_script.name};
                int played = -1;
                bool returned = false;
                for (int frame = 0; frame < 24000 && !returned; ++frame)
                {
                    gbtg::InputState input;
                    input.accept = frame % 30 == 0;
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 59.f, input);
                    gbtg::gfx_end_frame();
                    if (game.scene_script.name != visited.back())
                    {
                        visited.push_back(game.scene_script.name);
                        std::cout << "replay run-on frame " << frame << ": scene " << game.scene_script.name << "\n";
                    }
                    const bool running = gbtg::g_transition_phase == 0;
                    if (game.scene_script.name == held[held.size() - 2] && running && played < 0)
                    {
                        played = frame;
                    }
                    if (played >= 0 && frame == played + 60 && game.scene_script.name == held[held.size() - 2])
                    {
                        gbtg::RgbaImage image;
                        if (gbtg::gfx_readback(image))
                        {
                            gbtg::write_bmp("cache/gameselect_replay_scene_playcam.bmp", image);
                        }
                        gbtg::g_script_e788 = 1;
                    }
                    returned = played >= 0 && running && game.scene_script.name == held.back();
                }
                for (int frame = 0; frame < 300 && returned; ++frame)
                {
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 59.f, {});
                    gbtg::gfx_end_frame();
                }
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/gameselect_replay_scene_returned.bmp", image);
                    }
                }
                if (visited != std::vector<std::string>(held.begin() + 1, held.end()))
                {
                    std::string path;
                    for (const auto& name : visited)
                    {
                        path += " " + name;
                    }
                    throw gbtg::Error("the replay did not follow its held order:" + path);
                }
                const auto* gameselect = gbtg::find_runtime_scene_control(0x9A);
                if (!returned || gameselect == nullptr || gameselect->gameselect == nullptr)
                {
                    throw gbtg::Error("the replayed scene did not return to game select (scene "
                        + game.scene_script.name + ")");
                }
                std::cout << "PASS: the replayed scene ran on through its held order and returned to "
                          << game.scene_script.name << "\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            if (features_test)
            {
                // Down twice from the first item (Replay Storybook) reaches Replay Scene (2).
                if (features_selected != 2)
                {
                    throw gbtg::Error("Down twice in the features menu selected item "
                        + std::to_string(features_selected));
                }
                if (!features_page)
                {
                    throw gbtg::Error("A on Replay Scene in a new game did not show its error page");
                }
                const auto* control = gbtg::find_runtime_scene_control(0x9A);
                if (control == nullptr || control->gameselect->current_row != 3)
                {
                    throw gbtg::Error("B in the features menu did not return to the contents (row 3)");
                }
                std::cout << "PASS: game select opened the features menu, moved in it, showed an error page and went back\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            if (stats_test)
            {
                std::cout << "PASS: game select showed the stats dialog over the contents\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            if (delete_test)
            {
                size_t files = 0;
                for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
                {
                    std::cout << "save file " << entry.path().generic_string() << "\n";
                    ++files;
                }
                if (files != 0)
                {
                    throw gbtg::Error("the deleted game left files in the save folder");
                }
                if (native_slot_select() == nullptr)
                {
                    throw gbtg::Error("slot select did not come back after the delete");
                }
                std::cout << "PASS: game select deleted the new game and returned to slot select\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            if (new_game_test)
            {
                if (!std::filesystem::exists(test_save_folder()))
                {
                    throw gbtg::Error("the new game left no save folder");
                }
                for (const auto& entry : std::filesystem::recursive_directory_iterator(test_save_folder()))
                {
                    std::cout << "save file " << entry.path().generic_string() << " "
                              << (entry.is_regular_file() ? entry.file_size() : 0) << "\n";
                }
                if (game.scene_script.name.find("chapter1") == std::string::npos)
                {
                    throw gbtg::Error("Play did not start the new game (scene " + game.scene_script.name + ")");
                }
                std::cout << "PASS: game select named and saved a new game, opened it and played it into "
                          << game.scene_script.name << "\n";
                gbtg::gfx_shutdown();
                return 0;
            }
            std::cout << "PASS: game select opened and took Right, A and B\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (attract_test)
        {
            // The seven attract clips last about 108 seconds; run past them into the script that follows.
            std::vector<std::string> scenes;
            gbtg::i32 clips = 0;
            const void* previous_clip = nullptr;
            for (int frame = 0; frame < 8000; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, gbtg::InputState{});
                gbtg::gfx_end_frame();
                if (scenes.empty() || scenes.back() != game.scene_script.name)
                {
                    scenes.push_back(game.scene_script.name);
                    std::cout << "attract scene " << game.scene_script.name << " at frame " << frame << "\n";
                }
                if (const auto* stack = gbtg::find_scene_cutscene_runtime(game); stack != nullptr)
                {
                    const void* clip = -1 < stack->index ? stack->instances[size_t(stack->index)] : nullptr;
                    if (clip != nullptr && clip != previous_clip)
                    {
                        ++clips;
                    }
                    previous_clip = clip;
                }
                if (frame % 1000 == 500)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/attract_check_" + std::to_string(frame) + ".bmp", image);
                    }
                }
            }
            std::cout << "PASS: attract ran 8000 frames across " << scenes.size() << " scripts and " << clips
                      << " clip starts natively\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (title_test)
        {
            bool animated = false;
            bool accepted = false;
            for (int frame = 0; frame < 600; ++frame)
            {
                gbtg::InputState input;
                input.start = !title_a_test && frame == 120;
                input.accept = title_a_test && frame == 120;
                gbtg::gfx_begin_frame();
                // Exercise the ordinary wall-clock path, including the first
                // frame after loading, rather than a headless crash check.
                game.tick(1.f / 59.f, input);
                gbtg::gfx_end_frame();
                if (frame == 30 || frame == 60 || frame == 119 || frame == 300)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/title_check_" + std::to_string(frame) + ".bmp", image);
                    }
                }
                if (frame == 119)
                {
                    // The attract control (type 0x9B) plays its clips on the native scene cutscene stack.
                    const auto* stack = gbtg::find_scene_cutscene_runtime(game);
                    animated = stack != nullptr && -1 < stack->index
                        && stack->instances[size_t(stack->index)]->time > 1.f;
                }
                auto* slots = native_slot_select();
                if (game.scene_script.name.find("gameselect") != std::string::npos && slots != nullptr
                    && gbtg::g_transition_phase == 0)
                {
                    // Let the slot prompts come up, then press Right and give its callback (0x000E0210) time.
                    const auto tick = [&game](const gbtg::InputState& state, int frames)
                    {
                        for (int step = 0; step < frames; ++step)
                        {
                            gbtg::gfx_begin_frame();
                            game.tick(1.f / 59.f, state);
                            gbtg::gfx_end_frame();
                        }
                    };
                    for (int wait = 0; wait < 600 && !gameselect_listening(game); ++wait)
                    {
                        tick(gbtg::InputState{}, 1);
                    }
                    slots = native_slot_select();
                    if (slots == nullptr)
                    {
                        throw gbtg::Error("slot selection closed before Right");
                    }
                    const auto previous_slot = gbtg::u32(slots->slot);
                    gbtg::InputState right;
                    right.right = true;
                    tick(right, 1);
                    tick(gbtg::InputState{}, 30);
                    slots = native_slot_select();
                    gbtg::u32 slot = 0;
                    if (slots != nullptr)
                    {
                        slot = gbtg::u32(slots->slot);
                    }
                    if (slots == nullptr || slot != (previous_slot + 1) % 3)
                    {
                        throw gbtg::Error("slot selection ignored Right input");
                    }
                    accepted = true;
                    break;
                }
            }
            if (!animated || !accepted)
            {
                throw gbtg::Error("title animation or confirmation input stalled");
            }
            std::cout << "PASS: title animates and " << (title_a_test ? "A" : "Start")
                      << " opens slot selection and Right changes slot\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        bool saw_intro = false;
        bool saw_map = false;
        bool saw_load_release = false;
        bool saw_close = false;
        bool saw_next = false;
        int goop_enter_frames = 0;
        int goop_exit_frames = 0;
        int map_frames = 0;
        for (int frame = 0; frame < 18000; ++frame)
        {
            gbtg::InputState input;
            // Advance the retail dialogue pages with distinct A presses.
            input.accept = frame % 30 == 0;
            gbtg::gfx_begin_frame();
            game.tick(1.f / 60.f, input);
            gbtg::gfx_end_frame();
            saw_intro = saw_intro || game.scene_script.name.find("scene0_1book") != std::string::npos;
            if (gbtg::g_transition_row == 15 && (gbtg::g_transition_phase == 1 || gbtg::g_transition_phase == 3))
            {
                int& frames = gbtg::g_transition_phase == 1 ? goop_enter_frames : goop_exit_frames;
                ++frames;
                if (frames == 30)
                {
                    gbtg::RgbaImage image;
                    if (!gbtg::gfx_readback(image))
                    {
                        throw gbtg::Error("goop transition readback failed");
                    }
                    gbtg::write_bmp(gbtg::g_transition_phase == 1 ? "cache/goop_enter_check.bmp" :
                                                                         "cache/goop_exit_check.bmp", image);
                }
                if (frames == 5 || frames == 15 || frames == 60)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp(std::string("cache/transition_goop_") + (gbtg::g_transition_phase == 1 ? "enter_" : "exit_")
                            + std::to_string(frames) + ".bmp", image);
                    }
                }
            }
            if (gbtg::g_transition_row == 8 && gbtg::g_transition_phase != 0)
            {
                if (!saw_intro)
                {
                    throw gbtg::Error("map started before the intro");
                }
                saw_map = true;
                ++map_frames;
                saw_load_release = saw_load_release || gbtg::g_transition_phase == 2;
                saw_close = saw_close || gbtg::g_transition_phase == 3;
                if (map_frames == 5 || map_frames == 30 || map_frames == 90 || map_frames == 150 || map_frames == 270
                    || map_frames == 330 || map_frames == 400)
                {
                    gbtg::RgbaImage image;
                    if (gbtg::gfx_readback(image))
                    {
                        gbtg::write_bmp("cache/transition_map_" + std::to_string(map_frames) + ".bmp", image);
                    }
                }
                if (map_frames == 210)
                {
                    gbtg::RgbaImage screenshot;
                    if (gbtg::gfx_readback(screenshot))
                    {
                        gbtg::write_bmp("cache/transition_map_check.bmp", screenshot);
                    }
                }
            }
            // The story's cutscene plays on the native scene cutscene stack (0x0004EDA0 / 0x0004EBD0).
            const auto* story = gbtg::find_scene_cutscene_runtime(game);
            if (game.scene_script.name.find("scene1_1book") != std::string::npos && story != nullptr && -1 < story->index)
            {
                if (!saw_map || !saw_load_release || !saw_close)
                {
                    throw gbtg::Error("next story started before the map load barrier");
                }
                saw_next = true;
                if (gbtg::g_transition_phase == 0 && story->instances[size_t(story->index)]->time > 1.f)
                {
                    break;
                }
            }
        }
        if (!saw_intro || !saw_map || !saw_load_release || !saw_close || !saw_next || gbtg::g_transition_phase != 0)
        {
            throw gbtg::Error("intro -> map -> next story did not complete");
        }
        if (map_frames < 300)
        {
            throw gbtg::Error("map route was skipped");
        }
        if (goop_enter_frames < 80 || goop_exit_frames < 80)
        {
            throw gbtg::Error("goop entry or exit was skipped");
        }
        if (goop_test)
        {
            gbtg::script_request(game, "aid_script_ghoulies_chapter1_scene0_1book");
            int entering = 0;
            int exiting = 0;
            for (int frame = 0; frame < 600; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 59.f, {});
                gbtg::gfx_end_frame();
                if (gbtg::g_transition_row == 15 && gbtg::g_transition_phase == 1)
                {
                    ++entering;
                    if (entering == 30)
                    {
                        gbtg::RgbaImage image;
                        if (!gbtg::gfx_readback(image))
                        {
                            throw gbtg::Error("second goop readback failed");
                        }
                        gbtg::write_bmp("cache/goop_second_check.bmp", image);
                    }
                }
                if (gbtg::g_transition_row == 15 && gbtg::g_transition_phase == 3)
                {
                    ++exiting;
                }
                const auto* story = gbtg::find_scene_cutscene_runtime(game);
                if (entering > 80 && exiting > 80 && gbtg::g_transition_phase == 0 && story != nullptr && -1 < story->index)
                {
                    break;
                }
            }
            if (entering < 80 || exiting < 80 || gbtg::g_transition_phase != 0)
            {
                throw gbtg::Error("second goop transition did not complete");
            }
            std::cout << "PASS: repeated goop scene transition completes\n";
        }
        std::cout << "PASS: scene0_1book -> front-garden map -> scene1_1book; map frames=" << map_frames << "\n";
        gbtg::gfx_shutdown();
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << "\n";
        gbtg::gfx_shutdown();
        return 1;
    }
}
