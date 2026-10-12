#include "gbtg/engine_internal.h"
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
#include "../src/engine/entities/ghouly_box_controller.h"
#include "../src/engine/entities/ghouly_box_messages.h"
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
#include "../src/engine/scripts/script_conditions.h"

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
extern i32 g_546328;
extern i32 g_54632c;
extern i32 g_flag_2474;
extern i32 g_draw_e1ec;
extern i32 g_script_30d0c;
}

void check_camera_display();
void check_script_actor_query();
void check_scene_render();

namespace
{
struct RequestProbe : gbtg::GhoulyBoxController
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
    scene_detail::PulseNode first;
    scene_detail::PulseNode second;
    scene_detail::PulseNode third;
    first.next = &second;
    second.next = &third;
    first.word_1fc = 7;
    first.word_1fe = 0;
    second.word_1fe = 7;
    third.word_1fe = 7;
    auto* saved_head = scene_detail::g_pulse_nodes;
    scene_detail::g_pulse_nodes = &first;
    if (find_actor_by_group(7) != &second || find_actor_by_group(0) != &first ||
        find_actor_by_group(8) != nullptr)
    {
        throw Error("actor group lookup changed its key offset or first-match behavior");
    }
    scene_detail::g_pulse_nodes = nullptr;
    if (find_actor_by_group(0) != nullptr)
    {
        throw Error("empty actor group list returned an actor");
    }
    scene_detail::g_pulse_nodes = saved_head;
    Actor actor;
    ActorLink link;
    int source = 0;
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
    link.head = &source;
    if (actor_can_supply_ghoulybox(actor))
    {
        throw Error("bound actor accepted as a ghoulybox source");
    }
    link.head = nullptr;
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
    auto* retained = create_actor_joint_attachment(pending_attachments,
        attachment_script.name.c_str(), 7, -1, &options);
    create_actor_joint_attachment(pending_attachments, "plain_rejected", 0, -1, nullptr);
    create_actor_joint_attachment(pending_attachments,
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
    const std::unordered_map<u32, void (*)()> frame_widget_callbacks;
    DialogueWidgetUpdateContext bat_widget_update{frame_widget_context, frame_input, frame_voices,
        frame_bats, frame_widget_callbacks, 0.f, 0.1f, 2.f};
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
    if (scales.x != 5.f || scales.y != 0.f || scales.z != 2.f)
    {
        throw Error("model matrix scale extraction changed column lengths");
    }
}

void check_spawn_live_attachment_release()
{
    using namespace gbtg;
    static i32 calls;
    calls = 0;
    scene_detail::PulseNode probe;
    probe.on_message = [](scene_detail::PulseNode*, const i32* raw)
    {
        const auto& message = *reinterpret_cast<const ActorAttachmentReleaseMessage*>(raw);
        if (message.id != 0x8D || message.record == nullptr || message.value != 0)
        {
            throw Error("attachment release broadcast lost its record pointer");
        }
        ++calls;
    };
    auto* saved_pulse = scene_detail::g_pulse_nodes;
    scene_detail::g_pulse_nodes = &probe;
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
    scene_detail::g_pulse_nodes = saved_pulse;
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
    Vec3 previous{3.f, 0.f, 0.f};
    GhoulySpawnPlayerLink link;
    link.actor = &player;
    link.previous_position = &previous;
    auto& world = entity_detail::g_world;
    const auto saved = world.slot;
    world.slot.next = &link;
    u32 random = 7;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, 0, random) != 1 || random == 7)
    {
        throw Error("spawn trigger failed to detect a player entering its radius");
    }
    const u32 advanced = random;
    previous = player.position;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, 0, random) != 0 || random != advanced)
    {
        throw Error("spawn trigger fired again for a player already inside");
    }
    previous = Vec3{3.f, 0.f, 0.f};
    link.disabled_ce0 = 1;
    link.mode_cdc = 2;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, 1.f, 1, random) != 0 || random != advanced)
    {
        throw Error("spawn trigger ignored the disabled player gate");
    }
    link.disabled_ce0 = 0;
    if (check_ghouly_spawn_trigger(Vec3{}, 2.f, -1.f, 0, random) != -1)
    {
        throw Error("spawn trigger fabricated a successful probability test");
    }
    RequestProbe controller;
    Actor source;
    source.position = Vec3{5.f, 6.f, 7.f};
    controller.source_93c = &source;
    Vec3 source_position;
    Vec3 source_euler;
    Vec3 position;
    Vec3 euler;
    if (!query_ghouly_spawn_pose(controller, source_position, source_euler, position, euler) ||
        position.z != 7.f)
    {
        throw Error("controller pose query lost its actual spawn source");
    }
    controller.value_a48 = std::bit_cast<u32>(2.f);
    controller.value_a4c = std::bit_cast<u32>(1.f);
    std::strcpy(controller.box_944, "aid_ghoulybox_trigger_test");
    std::strcpy(controller.attributes_9c4, "attributes");
    tick_ghouly_box_controller(controller, 0, random);
    if (controller.calls != 1 || std::strcmp(controller.received.box, controller.box_944) != 0)
    {
        throw Error("controller tick did not send its actual queued request");
    }
    controller.spawned_940 = 1;
    tick_ghouly_box_controller(controller, 0, random);
    if (controller.calls != 1)
    {
        throw Error("controller tick retriggered after successful live spawning");
    }
    controller.load_938 = allocate_ghouly_spawn_record();
    tick_ghouly_box_controller(controller, 0, random);
    if (controller.load_938 != nullptr)
    {
        throw Error("controller tick retained a completed spawn record");
    }
    const size_t count = g_ghouly_controllers.size();
    g_ghouly_controllers.push_back({123, &controller});
    controller.load_938 = allocate_ghouly_spawn_record();
    destroy_ghouly_box_controller(controller);
    if (controller.load_938 != nullptr || g_ghouly_controllers.size() != count)
    {
        throw Error("controller shutdown retained its record or registration");
    }
    world.slot = saved;
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
        static i32 calls;
        calls = 0;
        const auto saved_mark = entity_detail::g_world.mark;
        ActorWorldMessageNode first{};
        ActorWorldMessageNode second{};
        first.link.next = &second;
        first.message = [](ActorWorldMessageNode& node, const void* raw)
        {
            if (*static_cast<const u32*>(raw) == 0x37)
            {
                ++calls;
            }
            node.link.next = nullptr;
        };
        second.message = [](ActorWorldMessageNode&, const void* raw)
        {
            if (*static_cast<const u32*>(raw) == 0x37)
            {
                ++calls;
            }
        };
        entity_detail::g_world.mark.next = &first;
        const u32 message = 0x37;
        broadcast_actor_world_message(&message);
        entity_detail::g_world.mark = saved_mark;
        if (calls != 2)
        {
            throw Error("world actor broadcast did not preserve its next node across receiver mutation");
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
        if (!request_actor_trail(actor, 6.f, timer, 30, trail) || trail_calls != 1 ||
            objects.size() != 1 || read_u16(received, 2) != 0x43 ||
            read_u16(received, 4) != 0x10 || read_u32(received, 0x338) != 0x11223344 ||
            read_f32(received, 0x24) != 2.f || read_f32(received, 0x33C) != 6.f ||
            read_f32(received, 0x344) != 3.f || read_f32(received, 0x348) != 4.f ||
            read_f32(received, 0x34C) != 5.f || received[0x340] != 0x80)
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
    void* saved_pool = g_512344;
    const i32 saved_mask = g_512348;
    g_512344 = &pool;
    g_512348 = 4;
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
    g_512344 = saved_pool;
    g_512348 = saved_mask;
}

void check_native_audio_preparation(const char* directory)
{
    using namespace gbtg;
    Game game;
    game.aids.open(directory);
    // 0x00080CB0 -> 0x0013BAF0: this one-shot must outlive the intro clip.
    const char* intro_cue = "XACT_SOUNDBANK_GBARON_STARTBUTTONLAUGH";
    const u64 intro_handle = pcm_play_owned_cue(game, intro_cue);
    if (intro_handle == 0 || !pcm_owned_cue_playing(intro_handle))
    {
        throw Error("intro confirmation could not start the retail Baron laugh");
    }
    pcm_cutscene_sync(game);
    if (!pcm_owned_cue_playing(intro_handle))
    {
        throw Error("intro cutscene cleanup stopped the transition's Baron laugh");
    }
    pcm_stop_owned_cue(intro_handle);
    if (pcm_owned_cue_playing(intro_handle))
    {
        throw Error("intro confirmation retained its voice after release");
    }
    std::cout << "PASS: intro Baron laugh starts and survives outgoing cutscene cleanup\n";
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
    GhoulyBoxController controller;
    Actor source;
    source.position = Vec3{3.f, 4.f, 5.f};
    source.mask_flag = 1;
    source.spawn_point_count = 1;
    ActorAnimationState animation;
    RuntimeActorStartupState state{controller.actor, animation};
    ActorLink link;
    link.actor = &controller.actor;
    controller.actor.link = &link;
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
    ActorScareContext scares{factory, {}, objects, flags, player, euler};
    AudioCoreSources audio_sources{};
    AudioCoreCues audio_cues{audio_sources};
    AudioEventVoices voices{audio_cues, {}, {}, {}, 0, origin, euler};
    ActorEventContext events{state, effects, trails, scares, voices, {}, timer, scale, scale};
    entity_detail::ShapeProbe shape;
    entity_detail::SegmentProbe segment;
    ActorRenderRegistration render{controller.actor, shape, segment, state.live};
    const std::unordered_map<const u8*, const u8*> relocated;
    ActorAttachmentEventContext callouts{controller.actor, state.attachments, effects, factory, {}, objects, 0x10000, relocated};
    entity_detail::ListHead anchors;
    entity_detail::ListHead live_attachments;
    ActorLink world_bind;
    void* watched = &source;
    ModelAnimationState pose;
    ModelNodeFlagCache cache{};
    AnimationMatrix reset{};
    ActorBaseContext base{events, render, callouts, anchors, live_attachments, world_bind, watched, pose, nullptr, cache, reset};
    scene_detail::PulseNode source_node;
    source_node.word_1fe = 7;
    scene_detail::PulseNode anchor_node;
    anchor_node.word_1fc = 2;
    source_node.next = &anchor_node;
    GhoulyController anchor;
    const std::unordered_map<scene_detail::PulseNode*, Actor*> actors{{&source_node, &source}};
    const std::unordered_map<scene_detail::PulseNode*, GhoulyController*> controllers{{&anchor_node, &anchor}};
    GhoulyBoxMessageContext context{base, actors, controllers, nullptr};
    auto* saved_pulses = scene_detail::g_pulse_nodes;
    scene_detail::g_pulse_nodes = &source_node;
    state.setting_1fc = 7;
    state.setting_1fe = 2;
    u32 initialize = 0x7D;
    dispatch_ghouly_box_message(controller, &initialize, context);
    if (controller.source_93c != &source || controller.anchor_934 != &anchor)
    {
        throw Error("ghoulybox receiver confused its source group and anchor id");
    }
    controller.anchor_934 = nullptr;
    GhoulyPoseMessage pose_message;
    dispatch_ghouly_box_message(controller, &pose_message, context);
    if (pose_message.result != 1 || pose_message.output_position.x != 3.f || pose_message.output_position.z != 5.f)
    {
        throw Error("ghoulybox receiver did not return its source actor pose");
    }
    GhoulySpawnMessage spawn;
    source.spawn_blocked = 1;
    dispatch_ghouly_box_message(controller, &spawn, context);
    if (spawn.result != -2 || controller.spawned_940 != 0)
    {
        throw Error("ghoulybox receiver ignored source eligibility");
    }
    source.spawn_blocked = 0;
    GhoulyRequest request;
    std::strcpy(request.box, resource.name.c_str());
    std::strcpy(request.attributes, "aid_actorattribs_receiver_test");
    dispatch_ghouly_box_message(controller, &request, context);
    auto* load = static_cast<GhoulySpawnRecord*>(controller.load_938);
    if (request.result != 1 || load == nullptr || load->controller_24 != &controller ||
        load->flags_2c != 0 || load->prev != &g_side_list || std::strcmp(load->attributes_30, request.attributes) != 0)
    {
        throw Error("ghoulybox receiver failed to enqueue the decoded request");
    }
    GhoulyRequest duplicate;
    std::strcpy(duplicate.box, resource.name.c_str());
    dispatch_ghouly_box_message(controller, &duplicate, context);
    if (duplicate.result != 0 || controller.load_938 != load)
    {
        throw Error("ghoulybox receiver replaced its pending request");
    }
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
    if (controller.actor.position.x != 7.f || controller.actor.xform_valid != 0)
    {
        throw Error("base actor receiver did not invalidate its moved actor transform");
    }
    ActorReferenceMessage released{0x7C, &source};
    dispatch_ghouly_box_message(controller, &released, context);
    if (controller.source_93c != nullptr || watched != nullptr)
    {
        throw Error("ghoulybox receiver did not propagate source release to the base actor");
    }
    {
        // Player dispatch 0x00028F60/0x00028FC0 through the same base receiver; ids 0xA9, 0xF,
        // 0x2D and 9 are ignored by 0x0003EF70.
        std::vector<u8> player_bytes(0xE00);
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
        std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> views;
        RuntimeScriptActorQueryContext query{nullptr, views, zero.data()};
        std::vector<u8> body_storage(0x900);
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
        if (variant.result != 1 || read_u16(player_bytes, 0xB9C) != 1 || forwarded.size() != 2 || forwarded[0].first != &strategy
            || forwarded[1].first != &body || forwarded[1].second != 0xA9)
        {
            throw Error("player dispatch must handle 0xA9, then forward to the present strategy and body only");
        }
        u32 bit_message = 0x2D;
        dispatch_runtime_player_message(&bit_message, player, children);
        u32 pause_message = 9;
        dispatch_runtime_player_message(&pause_message, player, children);
        if (bit_bytes[0] != 2 || read_u32(player_bytes, 0xCD8) != 1 || player_actor.parked_gate != 1 || forwarded.size() != 6)
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
    destroy_ghouly_box_controller(controller);
    scene_detail::g_pulse_nodes = saved_pulses;
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
        std::array<DialogueRenderTargetHook, 2> hooks{};
        hooks[1] = { [&calls, &require](std::uintptr_t first, std::uintptr_t second)
        {
            ++calls;
            require(first == 17 && second == 29, "dialogue target callback arguments");
            return DialogueSurfaceHandle{41};
        }, 17, 29 };
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
        std::array<ObjectFactoryType, 0x16> types{};
        ObjectFactoryRegistry factory{types};
        RuntimeActorSoundQueues sounds{};
        std::unordered_map<ActorFrameAttachment*, FactoryObject*> frames;
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
        std::array<u8, 0x964> owner{};
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
        if (read_u32(owner, 0x960) != 0 || read_u32(pool, 16) != 0
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
        std::array<u8, 0x964> player_actor{};
        const u32 player_device = 0x1234;
        std::memcpy(player_actor.data() + 0x960, &player_device, 4);
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
        std::memcpy(player_actor.data() + 0x960, &player_device, 4);
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
        CutsceneCameraImpulse impulse{0.f, 0.f, 0.f, 10.f, 0};
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
        record.name = "aid_objparams_ghoulies_misc_actor";
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
        const std::unordered_map<Actor*, FactoryObject*> released_objects{{&actor, &objects.back()}};
        const std::unordered_map<Actor*, ActorRenderRegistration*> released_render{{&actor, &render}};
        CutsceneActorReleaseContext release_context{
            factory, released_objects, released_render, event_resources, registry};
        row.listed = 1;
        row.elapsed = 42;
        link.previous = &previous;
        link.next = &next;
        previous.next = &link;
        next.previous = &link;
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
        DialogueMeasurementContext measurement{tag_context, metrics, time_context, {1, bits},
            10, 2, 3, 4, 1, 3600, 60.f, 2.f, 0.5f, 0, wide_scratch, narrow_scratch};
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
        const std::unordered_map<u32, void (*)()> widget_callbacks{{0x101, []()
        {
            ++widget_callback_count;
        }}};
        DialogueWidgetUpdateContext update_context{timing_context, widget_input, widget_voices,
            widget_bats, widget_callbacks, 0.f, 0.5f, 1.f};
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
        game.scene_script = parse_scene_script(bytes, "vm-traversal-regression");
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
            read_u32(state.matrices.records, 0) != 0xFFFFFFFFu ||
            read_u32(state.matrices.records, 0x48) != 7 ||
            read_f32(state.matrices.records, 0x1C) != 1.f ||
            read_u32(state.matrices.records, 0x48 + 0x44) != 0 ||
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
    int pending_token = 0;
    probe.load_938 = &pending_token;
    probe.anchor_934 = &pending_token;
    probe.source_93c = &pending_token;
    g_ghouly_controllers.clear();
    if (!initialize_ghouly_box_controller(probe, parameters, random_state) ||
        g_ghouly_controllers.size() != 1 || g_ghouly_controllers[0].key != 7 ||
        g_ghouly_controllers[0].controller != &probe || random_state != 0x3C6EF35Fu ||
        probe.anchor_934 != nullptr || probe.source_93c != nullptr || probe.load_938 != &pending_token ||
        std::strcmp(probe.box_944, initial_box) != 0 ||
        std::strcmp(probe.attributes_9c4, initial_attributes) != 0 ||
        probe.value_a48 != 12 || probe.value_a4c != 34 || probe.value_a50 != 56)
    {
        throw Error("controller initialization lost registry, random, or parameter fields");
    }
    // An unkeyed initialization still draws random and updates +0xA50.
    // A rejected chance must preserve the earlier names and selection fields.
    parameter_word(0x334, 0);
    parameter_word(0x438, std::bit_cast<u32>(-1.f));
    parameter_word(0x43C, 99);
    parameter_word(0x444, 78);
    random_state = 0;
    initialize_ghouly_box_controller(probe, parameters, random_state);
    if (g_ghouly_controllers.size() != 1 || random_state != 0x3C6EF35Fu ||
        probe.value_a48 != 12 || probe.value_a50 != 78 || std::strcmp(probe.box_944, initial_box) != 0)
    {
        throw Error("unkeyed or rejected-chance initialization changed the wrong fields");
    }
    const float boundary = std::bit_cast<float>((0x3C6EF35Fu & 0x7FFFFFu) | 0x3F800000u) - 1.f;
    parameter_word(0x438, std::bit_cast<u32>(boundary));
    random_state = 0;
    initialize_ghouly_box_controller(probe, parameters, random_state);
    if (probe.value_a48 != 99)
    {
        throw Error("controller chance equality was rejected");
    }
    parameter_word(0x438, std::bit_cast<u32>(std::numeric_limits<float>::quiet_NaN()));
    parameter_word(0x43C, 100);
    random_state = 0;
    initialize_ghouly_box_controller(probe, parameters, random_state);
    if (probe.value_a48 != 99)
    {
        throw Error("NaN controller chance selected a ghoulybox");
    }
    parameter_word(0x438, std::bit_cast<u32>(1.f));
    parameter_word(0x338, 0);
    random_state = 0;
    initialize_ghouly_box_controller(probe, parameters, random_state);
    if (probe.value_a48 != 99 || random_state != 0x3C6EF35Fu)
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
    game.scene_script = parse_scene_script(bytes, "loading_test");
    const ScriptNode& node = game.scene_script.nodes.front();
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
    int view_token = 0;
    entity_detail::g_world.view.next = &view_token;
    if (script_handle_actor_actions(game, node) != 0)
    {
        throw Error("pending world-view registration did not hold loading");
    }
    entity_detail::g_world.view.next = nullptr;
    u16 actor_state = 2;
    EntityMaskSpan span{2, 0, 0, 1, reinterpret_cast<const u8*>(&actor_state)};
    scene_detail::CamFocus actor;
    actor.span_980 = &span;
    scene_detail::RangeLink link;
    link.host = &actor;
    scene_detail::g_range_links = &link;
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
    actor.at_984 = 1;
    if (!actor_loading_complete())
    {
        throw Error("retail actor state override was ignored");
    }
    actor.at_984 = 0;
    actor.at_98c = 1;
    if (!actor_loading_complete())
    {
        throw Error("retail secondary actor state override was ignored");
    }
    scene_detail::g_range_links = nullptr;
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
void check_scene_spawn(gbtg::Game& game)
{
    using namespace gbtg;
    for (int frame = 0; frame < 600 && game.factory_scene == nullptr; ++frame)
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
    services.start_markers();
    std::cout << "scene spawn: objects=" << game.factory_scene->objects.size()
              << " players=" << services.player_total() << " factory=" << game.factory_scene->factory.count
              << " player link=0x" << std::hex << services.player_link() << std::dec << "\n";
    if (services.player_total() == 0)
    {
        throw Error("opening markers created no player actors");
    }
}

void check_player_dialogue_resources(gbtg::AidStore& aids)
{
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
            std::array<u8, 0x890> body_bytes{};
            const float factors[]{2.f, 3.f};
            std::memcpy(body_bytes.data() + 0x884, factors, 8);
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
                    if (camera.bytes == nullptr || payload != std::array<u32, 3>{0xBA, 0, 0x12345678})
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
            std::array<u8, 0xCE4> actor_bytes{};
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
                std::memcpy(body_bytes.data() + 0x94, &prior_health, 2);
                std::memcpy(body_bytes.data() + 0x96, &prior_health, 2);
                intro_enabled = 0;
                if (!initialize_runtime_player_mind(mind_bytes, mind_parameters, setup))
                {
                    throw Error("translated player mind constructor changed its return");
                }
                const u16 expected_health = variant == 1 || variant == 2 ? 10 : variant == 3 ? prior_health : 7;
                if (read_u16(body_bytes, 0x94) != expected_health || read_u16(body_bytes, 0x96) != expected_health
                    || intro_enabled != (variant == 0 ? 1u : 0u)
                    || read_u32(actor_bytes, 0xCE0) != 1 || actor_bytes[0x331] != 1
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
        unbound.scene_script.bytes.resize(0x98);
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
        if (!missing_owner || unbound.script_dialogue != nullptr || unbound.cutscenes.playing)
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
    gbtg::DialogueMeasurementContext measurement{tags, resources.metrics, resources.text,
        {}, 0, 0, 0, 0, 0, 0, 0.f, 1.f, 1.f, 0, wide.data(), narrow.data()};
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

int main(int argc, char** argv)
{
    const bool audio_test = argc == 3 && std::string_view(argv[2]) == "--audio-preparation";
    const bool title_a_test = argc == 3 && std::string_view(argv[2]) == "--title-a";
    const bool title_test = title_a_test || (argc == 3 && std::string_view(argv[2]) == "--title");
    const bool goop_test = argc == 3 && std::string_view(argv[2]) == "--goop";
    const bool ui_test = argc == 3 && std::string_view(argv[2]) == "--ui";
    const bool dialogue_resources_test = argc == 3 && std::string_view(argv[2]) == "--dialogue-resources";
    const bool player_test = argc == 3 && std::string_view(argv[2]) == "--player-opening";
    const bool decoder_test = argc == 3 && std::string_view(argv[2]) == "--ghoulybox-decoder";
    const bool actor_test = argc == 3 && std::string_view(argv[2]) == "--actor-loading";
    const bool model_resources_test = argc == 3 && std::string_view(argv[2]) == "--scene-models";
    const bool spawn_test = argc == 3 && std::string_view(argv[2]) == "--scene-spawn";
    if (argc != 2 && argc != 4 && !title_test && !goop_test && !ui_test && !player_test && !actor_test && !decoder_test && !audio_test && !dialogue_resources_test
        && !model_resources_test && !spawn_test)
    {
        std::cerr << "Usage: gbtg-transition-tests GAME_DIRECTORY [WIDTH HEIGHT | --title | --title-a | --goop | --ui | --player-opening | --dialogue-resources | --actor-loading | --ghoulybox-decoder | --audio-preparation | --scene-models | --scene-spawn]\n";
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
            std::cout << "PASS: actor spawn eligibility, group lookup, controller dispatch and asynchronous script gate\n";
            return 0;
        }
        gbtg::GfxConfig config;
        config.visible = false;
        config.width = argc == 4 ? std::stoi(argv[2]) : 640;
        config.height = argc == 4 ? std::stoi(argv[3]) : 480;
        if (!gbtg::gfx_init(config))
        {
            throw gbtg::Error("test graphics initialization failed");
        }
        gbtg::Game game;
        game.init(argv[1], player_test || spawn_test ? "chapter1_scene1_2playcam" : title_test ? "" : "scene0_1book");
        if (spawn_test)
        {
            check_scene_spawn(game);
            std::cout << "PASS: opening scene markers create the retail actors and player\n";
            return 0;
        }
        if (player_test)
        {
            check_player_dialogue_resources(game.aids);
            bool saw_dialogue = false;
            bool cutscene_before_accept = false;
            for (int frame = 0; frame < 300; ++frame)
            {
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, {});
                gbtg::gfx_end_frame();
                saw_dialogue |= gbtg::g_flag_2474 != 0 && gbtg::g_draw_e1ec != 0;
                cutscene_before_accept |= game.cutscenes.playing;
            }
            if (game.scene_script.name != "aid_script_ghoulies_chapter1_scene1_2playcam")
            {
                throw gbtg::Error("player test loaded the wrong scene");
            }
            std::cout << "player opening: dialogue=" << saw_dialogue
                      << " cutscene before A=" << cutscene_before_accept
                      << " actors=" << game.actors.size()
                      << " entities=" << game.entities.size() << "\n";
            for (const gbtg::u32 cursor : gbtg::g_script_fired)
            {
                const auto* node = gbtg::script_node_at(game.scene_script, cursor);
                std::cout << "player script waiting: offset=0x" << std::hex << cursor
                          << " type=0x" << (node == nullptr ? 0 : node->type) << std::dec << "\n";
            }
            gbtg::RgbaImage image;
            if (!gbtg::gfx_readback(image))
            {
                throw gbtg::Error("player opening readback failed");
            }
            gbtg::write_bmp("cache/player_opening_check.bmp", image);
            if (!saw_dialogue || cutscene_before_accept)
            {
                throw gbtg::Error("player opening must hold chapter1_scene1_fight1_howtofight until A; "
                                  "native dialogue did not hold the live script");
            }
            auto& dialogue = gbtg::scene_dialogue_context(game).dialogue;
            if (dialogue.state.widget == nullptr || dialogue.state.cutscene == nullptr ||
                dialogue.state.input.data() == nullptr || dialogue.page.text == nullptr)
            {
                throw gbtg::Error("player tutorial flags have no native widget, cutscene, input or page ownership");
            }
            bool cutscene_after_accept = false;
            // Give each native page a fresh A edge, followed by release and
            // enough time for the fade. Holding A cannot stand in for a new edge.
            for (int frame = 0; frame < 600 && !cutscene_after_accept; ++frame)
            {
                gbtg::InputState input;
                input.accept = frame % 120 < 5;
                gbtg::gfx_begin_frame();
                game.tick(1.f / 60.f, input);
                gbtg::gfx_end_frame();
                cutscene_after_accept = game.cutscenes.playing;
            }
            if (!cutscene_after_accept || gbtg::g_flag_2474 != 0 || dialogue.state.mode != 0)
            {
                throw gbtg::Error("player tutorial A edges did not release dialogue and start the opening cutscene");
            }
            std::cout << "PASS: player tutorial holds without A and releases to the opening cutscene after A\n";
            gbtg::gfx_shutdown();
            return 0;
        }
        if (ui_test)
        {
            gbtg::load_ui(game);
            if (game.ui.size() < 2 || game.ui[0].name != "aid_texture_joypad_buttons_a")
            {
                throw gbtg::Error("story dialogue A icon was not loaded");
            }
            gbtg::RgbaImage backdrop;
            backdrop.width = 1;
            backdrop.height = 1;
            backdrop.bgra = {0, 0, 0, 255};
            auto capture = [&]()
            {
                gbtg::gfx_begin_frame();
                gbtg::gfx_sprite_submit(backdrop, 0.f, 0.f, 640.f, 480.f);
                gbtg::gfx_sprite_submit(game.ui[0].image, 288.f, 208.f, 64.f, 64.f);
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
            gbtg::gfx_release_sprite(game.ui[0].image);
            game.ui[0].image = game.ui[1].image;
            const auto substituted = capture();
            if (substituted.bgra == expected.bgra)
            {
                throw gbtg::Error("UI cache regression did not seed a different glyph");
            }
            gbtg::load_ui(game);
            const auto restored = capture();
            if (restored.bgra != expected.bgra)
            {
                throw gbtg::Error("UI reload retained the previous button's GPU texture");
            }
            std::cout << "PASS: story A glyph restored after UI texture storage reuse\n";
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
                if (frame == 119)
                {
                    animated = game.cutscenes.playing && game.cutscenes.time > 1.f;
                }
                if (game.scene_script.name.find("gameselect") != std::string::npos &&
                    game.frontend_mode == 1 && gbtg::g_54632c == 0)
                {
                    const int previous_slot = game.slot;
                    gbtg::InputState right;
                    right.right = true;
                    gbtg::gfx_begin_frame();
                    game.tick(1.f / 59.f, right);
                    gbtg::gfx_end_frame();
                    if (game.slot != (previous_slot + 1) % 3)
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
            if (gbtg::g_546328 == 15 && (gbtg::g_54632c == 1 || gbtg::g_54632c == 3))
            {
                int& frames = gbtg::g_54632c == 1 ? goop_enter_frames : goop_exit_frames;
                ++frames;
                if (frames == 30)
                {
                    gbtg::RgbaImage image;
                    if (!gbtg::gfx_readback(image))
                    {
                        throw gbtg::Error("goop transition readback failed");
                    }
                    gbtg::write_bmp(gbtg::g_54632c == 1 ? "cache/goop_enter_check.bmp" :
                                                                         "cache/goop_exit_check.bmp", image);
                }
            }
            if (gbtg::g_546328 == 8 && gbtg::g_54632c != 0)
            {
                if (!saw_intro)
                {
                    throw gbtg::Error("map started before the intro");
                }
                saw_map = true;
                ++map_frames;
                saw_load_release = saw_load_release || gbtg::g_54632c == 2;
                saw_close = saw_close || gbtg::g_54632c == 3;
                if (map_frames == 210)
                {
                    gbtg::RgbaImage screenshot;
                    if (gbtg::gfx_readback(screenshot))
                    {
                        gbtg::write_bmp("cache/transition_map_check.bmp", screenshot);
                    }
                }
            }
            if (game.scene_script.name.find("scene1_1book") != std::string::npos && game.cutscenes.playing)
            {
                if (!saw_map || !saw_load_release || !saw_close)
                {
                    throw gbtg::Error("next story started before the map load barrier");
                }
                saw_next = true;
                if (gbtg::g_54632c == 0 && game.cutscenes.time > 1.f)
                {
                    break;
                }
            }
        }
        if (!saw_intro || !saw_map || !saw_load_release || !saw_close || !saw_next || gbtg::g_54632c != 0)
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
                if (gbtg::g_546328 == 15 && gbtg::g_54632c == 1)
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
                if (gbtg::g_546328 == 15 && gbtg::g_54632c == 3)
                {
                    ++exiting;
                }
                if (entering > 80 && exiting > 80 && gbtg::g_54632c == 0 && game.cutscenes.playing)
                {
                    break;
                }
            }
            if (entering < 80 || exiting < 80 || gbtg::g_54632c != 0)
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
