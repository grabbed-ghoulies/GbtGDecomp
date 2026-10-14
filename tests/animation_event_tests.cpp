#include "gbtg/anim.h"
#include "gbtg/resource_error.h"
#include "gbtg/sound_runtime.h"
#include <bit>

#include <cstring>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace gbtg;

void require(bool condition, const char* message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        std::vector<u8> event(16);
        event[0] = 16;
        float begin = 0.3f;
        float end = 0.6f;
        std::memcpy(event.data() + 4, &begin, 4);
        std::memcpy(event.data() + 8, &end, 4);
        std::vector<int> phases;
        const auto callback = [&](int phase)
        {
            phases.push_back(phase);
        };
        require(!dispatch_animation_event(event, 20, 0, {0.4f, 0.2f, 1.f}, callback),
                "wrong-sized event was dispatched");
        require(phases.empty(), "wrong-sized event reached callback");
        require(!dispatch_animation_event(event, 16, 0, {begin, 0.2f, 1.f}, callback),
                "begin fired at the exclusive current-time boundary");
        require(dispatch_animation_event(event, 16, 0, {0.4f, begin, 1.f}, callback),
                "begin missed the inclusive previous-time boundary");
        require(phases == std::vector<int>{0}, "wrong begin phase");
        phases.clear();
        require(!dispatch_animation_event(event, 16, 4, {0.5f, 0.4f, 1.f}, callback),
                "active phase incorrectly reported an endpoint crossing");
        require(phases == std::vector<int>{2}, "active interval missed callback");
        phases.clear();
        require(dispatch_animation_event(event, 16, 0, {0.7f, end, 1.f}, callback), "end crossing missed");
        require(phases == std::vector<int>{1}, "wrong end phase");
        phases.clear();
        require(dispatch_animation_event(event, 16, 0, {0.2f, 0.4f, -1.f}, callback),
                "reverse begin crossing missed");
        require(phases == std::vector<int>{0}, "wrong reverse phase");
        phases.clear();
        require(dispatch_animation_event(event, 16, 0, {0.4f, 0.8f, 1.f}, callback),
                "wrapped begin crossing missed");
        require(phases == std::vector<int>{0}, "wrong wrap phase");
        phases.clear();
        require(!dispatch_animation_event(event, 16, 0, {0.4f, 0.4f, 1.f}, callback),
                "stationary player dispatched an endpoint");
        require(phases.empty(), "stationary player reached callback");
        require(dispatch_animation_event(event, 16, 0, {0.4f, 0.2f, 1.f}, {}),
                "missing callback changed crossing result");
        end = 0.f;
        std::memcpy(event.data() + 8, &end, 4);
        require(!dispatch_animation_event(event, 16, 0, {0.1f, 0.9f, 1.f}, callback),
                "zero end time fired during wrap");
        require(phases.empty(), "zero end reached callback");
        MinaPlayback player;
        player.position = 0.75f;
        player.speed = 1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 0.25f && player.cycles == 1, "loop crossing differs");
        player.position = 0.25f;
        player.speed = -1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 0.75f && player.cycles == -1, "reverse loop crossing differs");
        player.mode = 1;
        player.position = 0.75f;
        player.speed = 1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 1.f, "clamped player overran end");
        player.position = 0.25f;
        player.speed = -1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 0.f, "clamped player overran start");
        player.mode = 2;
        player.position = 0.75f;
        player.speed = 1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 0.75f && player.speed == -1.f, "ping-pong end failed");
        player.position = 0.25f;
        player.speed = -1.f;
        advance_mina_playback(player, 0.5f, nullptr, nullptr, false);
        require(player.position == 0.25f && player.speed == 1.f, "ping-pong start failed");
        // Two root frames, x=100 then x=103, encoded with a 2-bit delta.
        std::vector<u8> mina(92);
        std::memcpy(mina.data(), "MINA", 4);
        mina[0xC] = 0x40;
        mina[0x1A] = 2;
        mina[0x1F] = 2; // quaternion exponent 4
        mina[0x40] = 20;
        mina[0x42] = 2;
        mina[0x44] = 2;
        mina[0x46] = 1;
        mina[0x4C] = 2;
        mina[0x4D] = 0x80;
        mina[0x54] = 100;
        mina[0x56] = 1;
        mina[0x59] = 3;
        require(sample_mina_root(mina, 0.f).translation.x == 100.f, "root first frame differs");
        require(sample_mina_root(mina, 1.f).translation.x == 103.f, "root last frame differs");
        require(sample_mina_root(mina, 0.5f).translation.x == 101.5f, "root interpolation differs");
        require(mina_root_cycle_translation(mina).x == 3.f, "root loop displacement differs");
        player.mina = mina;
        player.mode = 0;
        player.position = 0.75f;
        player.speed = 1.f;
        AnimationRoot root;
        advance_mina_playback(player, 0.5f, &root, nullptr, false);
        require(root.translation.x == 1.5f, "root motion lost loop displacement");
        require(root.rotation.w == 1.f, "root identity orientation differs");
        float duration = 2.f;
        std::memcpy(mina.data() + 8, &duration, 4);
        bool dirty = false;
        retime_mina_playback(&player, 0.4f, dirty);
        require(dirty && player.duration == 0.4f && player.speed == 2.5f,
                "explicit retiming differs");
        player.speed = -1.f;
        retime_mina_playback(&player, 0.f, dirty);
        require(player.duration == 2.f && player.speed == -0.5f,
                "default retiming lost reverse direction");
        dirty = false;
        retime_mina_playback(nullptr, 1.f, dirty);
        require(dirty, "empty slot did not dirty the controller");
        require(mina_playback_completed(nullptr), "empty slot is incomplete");
        player.mode = 0;
        player.position = 1.f;
        require(!mina_playback_completed(&player), "looping slot completed");
        player.mode = 1;
        player.speed = 1.f;
        require(mina_playback_completed(&player), "forward endpoint is incomplete");
        player.position = 0.5f;
        require(!mina_playback_completed(&player), "midpoint completed");
        player.speed = -1.f;
        player.position = 0.f;
        require(mina_playback_completed(&player), "reverse endpoint is incomplete");
        AnimationPose first;
        AnimationPose second;
        first.joints.push_back(JointPose{Quat{0.f, 0.f, 0.f, 1.f}, Vec3{2.f, 4.f, 6.f}, Vec3{1.f, 2.f, 3.f}});
        second.joints.push_back(JointPose{Quat{0.f, 0.f, 0.f, -1.f}, Vec3{10.f, 12.f, 14.f}, Vec3{5.f, 6.f, 7.f}});
        first.auxiliary.fill(2.f);
        second.auxiliary.fill(10.f);
        blend_animation_pose(first, first, second, 0.25f);
        require(first.joints[0].translation.x == 4.f && first.joints[0].scale.z == 4.f,
                "aliased pose crossfade differs");
        require(first.joints[0].rotation.w == 1.f, "crossfade ignored antipodal quaternion");
        require(first.auxiliary[47] == 4.f && first.auxiliary[48] == 2.f,
                "crossfade changed its unblended auxiliary channel");
        std::vector<u8> auxiliary_mina(308);
        const auto write_word = [&](size_t offset, u32 value)
        {
            std::memcpy(auxiliary_mina.data() + offset, &value, 4);
        };
        write_word(0x20, 96);
        write_word(0x24, 4);
        write_word(96, 292);
        write_word(96 + 0xC0, 292);
        for (u32 frame = 0; frame < 4; ++frame)
        {
            const float value = float(frame * 10);
            std::memcpy(auxiliary_mina.data() + 292 + frame * 4, &value, 4);
        }
        player.mina = auxiliary_mina;
        player.mode = 1;
        player.position = 0.5f;
        std::array<float, 64> channels;
        sample_mina_auxiliary(player, channels);
        require(channels[0] == 15.f && channels[48] == 10.f,
                "clamped auxiliary sampling differs");
        require(channels[1] == 0.f, "absent auxiliary track produced a value");
        player.position = 1.f;
        sample_mina_auxiliary(player, channels);
        require(channels[0] == 30.f && channels[48] == 30.f,
                "auxiliary endpoint differs");
        player.mode = 0;
        player.position = 0.875f;
        sample_mina_auxiliary(player, channels);
        require(channels[0] == 15.f && channels[48] == 20.f,
                "wrapped auxiliary interpolation differs");
        MinaPlayer live;
        start_mina_player(live, mina, 0.25f, 0.4f, 1, 0);
        require(live.pose != nullptr && live.position == 0.25f && live.speed == 2.5f,
                "borrowed player initialization differs");
        // 0x0002D8E0 samples the cycle root at position 1 into player +0xF0 (LEA ESI,[EBP + 0xF0]).
        require(live.reference_root.translation.x == 3.f && live.cached_root.translation.x == 0.f,
                "initial root sampling differs");
        live.blend_duration = 0.2f;
        tick_mina_player(live, 0.1f, false);
        require(live.sampled && live.ticks == 1 && live.previous == 0.25f && live.position == 0.5f,
                "player tick did not publish sampled pose and clock");
        require(live.root.translation.x == 0.75f, "player tick root displacement differs");
        live.pose->auxiliary[63] = 9.f;
        start_mina_player(live, mina, 0.f, 0.f, 1, 1);
        require(live.old_pose != nullptr && live.old_pose->auxiliary[63] == 9.f &&
                live.blend_remaining == 0.2f && live.speed == -0.5f,
                "animation replacement discarded the crossfade pose");
        live.paused = true;
        tick_mina_player(live, 1.f, true);
        require(live.step == 0.f && live.age == 0.f && live.ticks == 1 && !live.sampled,
                "paused skip-pose tick differs");
        require(live.old_pose == nullptr && live.blend_remaining == 0.f,
                "skip-pose tick retained a crossfade allocation");
        std::vector<u8> flight_events(188);
        const auto event_header = [&](size_t offset, u16 size, u16 type, float time)
        {
            std::memcpy(flight_events.data() + offset, &size, 2);
            std::memcpy(flight_events.data() + offset + 12, &type, 2);
            std::memcpy(flight_events.data() + offset + 4, &time, 4);
        };
        event_header(0, 152, 29, 0.3f);
        event_header(152, 20, 31, 0.921f);
        event_header(172, 16, 0, 0.f);
        std::vector<int> dispatched_types;
        const AnimationEventCallback flight_callback = [&](std::span<const u8> record, int phase)
        {
            require(phase == 0, "flight event received the wrong phase");
            dispatched_types.push_back(read_u16(record, 12));
        };
        AnimationEventClock clock{0.4f, 0.f, 1.f};
        dispatch_animation_events(flight_events, clock, flight_callback);
        require(dispatched_types == std::vector<int>{29}, "flight event list missed its sound cue");
        dispatched_types.clear();
        end_animation_events(flight_events, clock, flight_callback);
        require(dispatched_types == std::vector<int>{31}, "shutdown missed the flagged voice stop");
        require(clock.position == 0.4f && clock.previous == 0.f, "shutdown changed the player clock");
        dispatched_types.clear();
        clock = AnimationEventClock{0.95f, 0.7f, 1.f};
        begin_animation_events(flight_events, clock, flight_callback);
        require(dispatched_types == std::vector<int>{31}, "startup replayed an unflagged event");
        require(clock.previous == 0.7f, "startup changed the previous position");
        dispatched_types.clear();
        clock = AnimationEventClock{0.8f, 0.7f, -1.f};
        begin_animation_events(flight_events, clock, flight_callback);
        require(dispatched_types == std::vector<int>{31}, "reverse startup used the wrong endpoint");
        const AnimationTable& table = button_bat_animation_table();
        const AnimationRecord* flight = find_animation(table, "flybetween");
        require(flight != nullptr && flight->mode == 1 && flight->old_pose_duration == 0.f,
                "flight table parameters differ");
        const AnimationRecord* stand = find_animation(table, "stand");
        require(stand != nullptr && stand->mode == 0 && stand->fade_duration == 0.2f,
                "stand table parameters differ");
        require(find_animation(table, "STAND") == nullptr && find_animation(table, "missing") == nullptr,
                "animation lookup invented a case-insensitive match or fallback");
        const std::array<AnimationAlias, 2> aliases
        {
            AnimationAlias{"idle", "stand"}, AnimationAlias{"move", "flybetween"}
        };
        const AnimationTable alias_table{table.records, aliases};
        require(find_animation(alias_table, "idle") == stand && find_animation(alias_table, "move") == flight,
                "animation alias lookup differs");
        const AnimationMix retained = animation_mix(*flight, true);
        require(retained.position == -1.f && retained.blend_mode == 0x21 && retained.priority == 0x31,
                "animation mix construction differs");
        require(animation_mix(*flight, false).position == 0.f, "mix lost its initial position");
        u32 animation_random = 0x12345678u;
        const u32 unchanged_random = animation_random;
        require(find_actor_animation(nullptr, "stand", nullptr, animation_random) == nullptr &&
                find_actor_animation(&table, nullptr, nullptr, animation_random) == nullptr &&
                find_actor_animation(&table, "stand", nullptr, animation_random) == stand &&
                find_actor_animation(&table, "stand", "gold", animation_random) == nullptr &&
                find_actor_animation(&table, "missing?", nullptr, animation_random) == nullptr &&
                animation_random == unchanged_random,
                "actor lookup invented a variant fallback or consumed random state without matches");
        std::array<AnimationRecord, 4> numbered_records{*stand, *stand, *stand, *stand};
        numbered_records[0].name = "idle1";
        numbered_records[1].name = "idle2";
        numbered_records[2].name = "idle4";
        numbered_records[3].name = "stand_[gold]";
        const std::array<AnimationAlias, 1> variant_aliases{AnimationAlias{"rest_[gold]", "stand_[gold]"}};
        const AnimationTable numbered_table{numbered_records, variant_aliases};
        require(find_actor_animation(&numbered_table, "rest", "gold", animation_random) == &numbered_records[3],
                "actor variant alias lookup differs");
        const u32 next_random = animation_random * 0x19660du + 0x3c6ef35fu;
        const float selected_fraction = std::bit_cast<float>((next_random & 0x7fffffu) | 0x3f800000u) - 1.f;
        const AnimationRecord* selected = find_actor_animation(&numbered_table, "idle?ignored", nullptr,
                                                               animation_random);
        require(selected == &numbered_records[int(selected_fraction * 2.f)] && animation_random == next_random,
                "wildcard lookup did not stop at the first gap or discard the suffix");
        MinaPlayback initialized;
        initialize_mina_playback(initialized, live.mina, 0.75f, -2.f, 2, 1);
        require(initialized.position == 0.75f && initialized.rate_factor == -2.f &&
                initialized.speed == 1.f && initialized.duration == 2.f &&
                initialized.mode == 2 && initialized.sampling_mode == 1,
                "explicit playback initialization changed signed rate or sampling mode");
        initialize_mina_playback(initialized, live.mina, 0.25f, 1, 0);
        require(initialized.rate_factor == 1.f && initialized.speed == 0.5f &&
                initialized.position == 0.25f && initialized.weight == 1.f,
                "default playback initialization differs");
        AnimationRoot root_a;
        AnimationRoot root_b;
        root_a.translation = Vec3{4.f, 8.f, 12.f};
        root_b.translation = Vec3{12.f, 16.f, 20.f};
        AnimationRoot root_output;
        blend_animation_root(root_output, &root_a, &root_b, 0.25f);
        require(root_output.translation.x == 6.f && root_output.translation.y == 10.f &&
                root_output.translation.z == 14.f, "controller root blend differs");
        blend_animation_root(root_output, &root_a, nullptr, 0.25f);
        require(root_output.translation.x == 3.f && root_output.translation.y == 6.f,
                "missing second root did not fade the first");
        blend_animation_root(root_output, nullptr, &root_b, 0.25f);
        require(root_output.translation.x == 3.f && root_output.translation.y == 4.f,
                "missing first root did not fade the second");
        const Vec3 retained_translation = root_output.translation;
        blend_animation_root(root_output, nullptr, nullptr, 0.25f);
        require(root_output.translation.x == retained_translation.x &&
                root_output.translation.y == retained_translation.y && root_output.rotation.w == 1.f,
                "two missing roots changed the retained translation");
        blend_animation_root(root_a, &root_a, &root_b, 0.25f);
        require(root_a.translation.x == 6.f && root_a.translation.y == 10.f,
                "controller root blend does not support aliased output");
        require(mix_auxiliary_linear(false, false, 4.f, 12.f, 0.25f) == 6.L,
                "linear auxiliary callback unexpectedly used presence flags");
        require(mix_auxiliary_additive(false, false, 4.f, 12.f, 0.25f) == 0.L &&
                mix_auxiliary_additive(false, true, 4.f, 12.f, 0.25f) == 12.L &&
                mix_auxiliary_additive(true, false, 4.f, 12.f, 0.25f) == 4.L &&
                mix_auxiliary_additive(true, true, 4.f, 12.f, 0.25f) == 7.L,
                "additive auxiliary presence branches differ");
        require(mix_auxiliary_masked(false, false, 4.f, 12.f, 0.25f) == 0.L &&
                mix_auxiliary_masked(false, true, 4.f, 12.f, 0.25f) == 12.L &&
                mix_auxiliary_masked(true, false, 4.f, 12.f, 0.25f) == 4.L &&
                mix_auxiliary_masked(true, true, 4.f, 12.f, 0.25f) == 6.L,
                "masked auxiliary presence branches differ");
        MinaPlayback aux_first;
        MinaPlayback aux_second;
        std::vector<u8> no_aux(64);
        aux_first.mina = no_aux;
        aux_second.mina = no_aux;
        AnimationPose aux_a;
        AnimationPose aux_b;
        AnimationPose aux_output;
        aux_a.auxiliary.fill(4.f);
        aux_b.auxiliary.fill(12.f);
        aux_output.auxiliary.fill(99.f);
        mix_animation_auxiliary(aux_output, aux_first, aux_a, aux_second, aux_b,
                                  0.25f, mix_auxiliary_linear);
        require(aux_output.auxiliary[0] == 0.f && aux_output.auxiliary[47] == 0.f &&
                aux_output.auxiliary[48] == 99.f,
                "absent auxiliary directories changed the retained trailer");
        std::vector<u8> aux_directory(64 + 48 * 4);
        const u32 directory_offset = 64;
        const u32 present_offset = 16;
        std::memcpy(aux_directory.data() + 0x20, &directory_offset, 4);
        std::memcpy(aux_directory.data() + 64, &present_offset, 4);
        aux_second.mina = aux_directory;
        mix_animation_auxiliary(aux_output, aux_first, aux_a, aux_second, aux_b,
                                  0.25f, mix_auxiliary_masked);
        require(aux_output.auxiliary[0] == 12.f && aux_output.auxiliary[1] == 0.f &&
                aux_output.auxiliary[48] == 99.f,
                "resource-relative auxiliary presence lookup differs");
        aux_first.mina = aux_directory;
        mix_animation_auxiliary(aux_a, aux_first, aux_a, aux_second, aux_b,
                                  0.25f, mix_auxiliary_linear);
        require(aux_a.auxiliary[0] == 6.f && aux_a.auxiliary[47] == 6.f &&
                aux_a.auxiliary[48] == 4.f,
                "auxiliary mixing does not support aliased output");
        auto controller = make_animation_controller(2);
        require(controller->slots.size() == 2 && controller->slot_count == 2 && controller->dirty &&
                !controller->idle && controller->selected_player == nullptr &&
                controller->selected_root == &controller->combined_root &&
                controller->combined_player.rate_factor == 0.f && controller->combined_player.weight == 0.f &&
                controller->combined_root.rotation.w == 1.f && controller->slots[0].player == nullptr,
                "controller allocation did not preserve the zeroed fields and root pointer");
        std::vector<u8> skeleton_mina(65);
        const u16 skeleton_count = 2;
        std::memcpy(skeleton_mina.data() + 0x14, &skeleton_count, 2);
        MinaPlayback skeleton_player;
        skeleton_player.mina = skeleton_mina;
        AnimationPose skeleton_pose;
        skeleton_pose.joints.resize(2);
        skeleton_pose.joints[0].rotation = Quat{0.f, 0.f, 0.f, 1.f};
        skeleton_pose.joints[1].rotation = Quat{0.f, 0.f, 0.f, 1.f};
        skeleton_pose.joints[0].translation.x = 1.f;
        skeleton_pose.joints[0].scale.x = 2.f;
        const std::array<Joint, 3> skeleton_joints
        {
            Joint{Vec3{1.f, 0.f, 0.f}, Vec3{1.f, 0.f, 0.f}, -1},
            Joint{Vec3{1.f, 0.f, 0.f}, Vec3{2.f, 0.f, 0.f}, 0},
            Joint{Vec3{1.f, 0.f, 0.f}, Vec3{3.f, 0.f, 0.f}, 1}
        };
        std::array<AnimationMatrix, 3> skeleton_output;
        const std::array<SkeletonCallbackRecord, 1> no_callbacks{SkeletonCallbackRecord{}};
        evaluate_mina_skeleton(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output,
                                no_callbacks, nullptr, nullptr);
        require(skeleton_output[0][0] == 2.f && skeleton_output[1][0] == 2.f &&
                skeleton_output[2][0] == 2.f && skeleton_output[0][3] == 0.f &&
                skeleton_output[1][3] == 0.f && skeleton_output[2][3] == 0.f,
                "skeleton parent concatenation or unanimated tail differs");
        const u32 compensation_mask = 64;
        std::memcpy(skeleton_mina.data() + 0x28, &compensation_mask, 4);
        skeleton_mina[64] = 2;
        evaluate_mina_skeleton(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output,
                                no_callbacks, nullptr, nullptr);
        require(skeleton_output[1][0] == 1.f && skeleton_output[2][0] == 1.f &&
                skeleton_output[1][3] == 2.f && skeleton_output[2][3] == 2.f,
                "skeleton parent scale compensation differs");
        const auto callback_output = skeleton_output;
        evaluate_mina_skeleton_plain(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output);
        require(skeleton_output == callback_output,
                "plain skeleton path changed compensated parent or unanimated tail");
        skeleton_pose.joints[0].scale = Vec3{0.f, 0.f, 0.f};
        evaluate_mina_skeleton_plain(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output);
        const float zero_scale = std::bit_cast<float>(0x38d1b717u);
        require(skeleton_pose.joints[0].scale.x == zero_scale &&
                skeleton_pose.joints[0].scale.y == zero_scale &&
                skeleton_pose.joints[0].scale.z == zero_scale,
                "plain skeleton path did not retain zero-parent-scale mutation");
        skeleton_pose.joints[0].scale = Vec3{2.f, 1.f, 1.f};
        int callback_calls = 0;
        const std::array<SkeletonCallbackRecord, 2> skeleton_callbacks
        {
            SkeletonCallbackRecord{2, [&](const AnimationMatrix& parent, const AnimationMatrix& local,
                                           AnimationMatrix& result, size_t ordinal, void* first, void* second)
            {
                require(ordinal == 0 && first == &callback_calls && second == &skeleton_player,
                        "skeleton callback context differs");
                ++callback_calls;
                result = parent;
                result[3] += local[3] + 7.f;
            }},
            SkeletonCallbackRecord{}
        };
        evaluate_mina_skeleton(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output,
                                skeleton_callbacks, &callback_calls, &skeleton_player);
        require(callback_calls == 1 && skeleton_output[2][3] == 9.f,
                "unanimated skeleton joint callback did not precede bind subtraction");
        skeleton_pose.joints[0].rotation = Quat{0.f, 0.f, 2.f, 2.f};
        evaluate_mina_skeleton(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output,
                                no_callbacks, nullptr, nullptr);
        require(skeleton_output[0][0] == 0.f && skeleton_output[0][1] == -1.f &&
                skeleton_output[0][4] == 2.f && skeleton_output[0][3] == 2.f &&
                skeleton_output[0][7] == -2.f,
                "skeleton quaternion matrix did not normalize a non-unit quaternion");
        const auto normalized_output = skeleton_output;
        evaluate_mina_skeleton_plain(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output);
        require(skeleton_output == normalized_output,
                "plain skeleton path changed non-unit quaternion normalization");
        skeleton_pose.joints[0].rotation = Quat{0.f, 0.f, 1.f, 1.f};
        evaluate_mina_skeleton(skeleton_player, skeleton_pose, skeleton_joints, skeleton_output,
                                no_callbacks, nullptr, nullptr);
        require(skeleton_output[0][0] == 2.f && skeleton_output[0][1] == 0.f && skeleton_output[0][3] == 0.f,
                "skeleton w-equals-one shortcut was replaced with general normalization");
        std::array<Joint, 5> bind_joints;
        std::array<AnimationMatrix, 5> bind_output;
        for (size_t index = 0; index < bind_joints.size(); ++index)
        {
            bind_joints[index].world = Vec3{1.f, 2.f, 3.f};
            bind_output[index] = AnimationMatrix{1.f, 0.f, 0.f, 10.f, 0.f, 1.f, 0.f, 20.f,
                                                  0.f, 0.f, 1.f, 30.f};
        }
        subtract_skeleton_bind_points(bind_output, bind_joints, 5);
        require(bind_output[0][3] == 9.f && bind_output[3][7] == 18.f && bind_output[4][11] == 27.f,
                "four-joint bind subtraction or remainder differs");
        ModelPoseBuffer primary;
        ModelPoseBuffer alternate;
        primary.count = 3;
        primary.matrices.resize(3);
        alternate.count = 3;
        alternate.matrices.resize(3);
        initialize_model_pose(primary);
        require(primary.dirty && primary.matrices[2][10] == 1.f && primary.matrices[2][11] == 0.f,
                "model pose initialization differs");
        primary.dirty = false;
        ModelAnimationState model_output;
        model_output.primary = &primary;
        model_output.alternate = &alternate;
        model_output.joint_count = 3;
        reset_model_pose(model_output, bind_output[0]);
        require(alternate.dirty && !primary.dirty && alternate.matrices[2][3] == 9.f &&
                primary.matrices[2][3] == 0.f,
                "model pose reset did not select the alternate buffer");
        model_output.alternate = nullptr;
        reset_model_pose(model_output, bind_output[4]);
        require(primary.dirty && primary.matrices[2][11] == 27.f,
                "model pose reset did not fall back to the primary buffer");
        float first_auxiliary = -1.f;
        float last_auxiliary = -1.f;
        model_output.auxiliary_targets[0] = &first_auxiliary;
        model_output.auxiliary_targets[47] = &last_auxiliary;
        write_model_auxiliary(model_output, std::span<const float, 48>(aux_b.auxiliary.data(), 48));
        require(first_auxiliary == 12.f && last_auxiliary == 12.f,
                "model auxiliary application skipped the final target");
        clear_model_auxiliary(model_output);
        require(first_auxiliary == 0.f && last_auxiliary == 0.f,
                "model auxiliary clearing differs");
        auto fresh_player = make_mina_player();
        require(fresh_player->rate_factor == 0.f && fresh_player->weight == 0.f &&
                fresh_player->root.rotation.w == 1.f && fresh_player->cached_root.rotation.w == 1.f &&
                fresh_player->reference_root.rotation.w == 0.f && fresh_player->blend_duration == 0.25f,
                "player allocation differs from the zeroed Xbox object");
        std::vector<u8> rootless_mina(64);
        const float rootless_duration = 1.f;
        std::memcpy(rootless_mina.data() + 8, &rootless_duration, 4);
        fresh_player->root.translation = Vec3{7.f, 8.f, 9.f};
        start_mina_player(*fresh_player, rootless_mina, 0.f, 0.f, 1, 0);
        require(fresh_player->root.translation.x == 7.f && fresh_player->root.translation.z == 9.f,
                "rootless restart reset a root field absent from the dump");
        fresh_player->pose.reset();
        tick_mina_player(*fresh_player, 0.1f, true);
        require(fresh_player->position == 0.1f && fresh_player->ticks == 1 && fresh_player->pose == nullptr,
                "skip-pose tick tested the pose pointer instead of the resource");
        fresh_player->mina = {};
        fresh_player->pose = std::make_unique<AnimationPose>();
        tick_mina_player(*fresh_player, 0.1f, true);
        require(fresh_player->position == 0.1f && fresh_player->ticks == 1,
                "resource-less player tick advanced an allocated pose");
        std::vector<u8> mix_mina(80);
        const u16 mix_count = 1;
        std::memcpy(mix_mina.data() + 8, &rootless_duration, 4);
        std::memcpy(mix_mina.data() + 0x14, &mix_count, 2);
        mix_mina[0x4c] = 0xe2;
        mix_mina[0x4d] = 0x80;
        auto mixed_controller = make_animation_controller(3);
        for (size_t slot = 0; slot < 3; ++slot)
        {
            auto& record = mixed_controller->slots[slot];
            record.player = make_mina_player();
            initialize_mina_playback(*record.player, mix_mina, 0.f, 1, 0);
            record.player->pose = std::make_unique<AnimationPose>();
            record.player->pose->joints.resize(1);
            record.player->pose->joints[0].rotation = Quat{0.f, 0.f, 0.f, 1.f};
            record.player->pose->joints[0].translation.x = 4.f + float(slot) * 8.f;
            record.player->root.translation.x = 2.f + float(slot) * 8.f;
            record.blend_mode = 0x21;
            record.fade_fraction = 0.5f;
            record.weight = slot == 1 ? 0.5f : 1.f;
        }
        mixed_controller->slots[0].player->speed = -1.f;
        combine_animation_controller(*mixed_controller);
        require(mixed_controller->selected_player == &mixed_controller->combined_player &&
                mixed_controller->selected_pose == mixed_controller->combined_pose.get() &&
                mixed_controller->selected_root == &mixed_controller->combined_root &&
                mixed_controller->combined_pose->joints[0].translation.x == 13.f &&
                mixed_controller->combined_root.translation.x == 11.f &&
                mixed_controller->combined_player.rate_factor == -1.f && mixed_controller->joint_capacity == 1,
                "controller did not combine later slots through its owned pose and root");
        AnimationPose mix_first = *mixed_controller->slots[0].player->pose;
        AnimationPose mix_second = *mixed_controller->slots[1].player->pose;
        AnimationPose mix_output = mix_first;
        mix_first.joints[0].translation.y = 8.f;
        mix_second.joints[0].translation.y = 20.f;
        const std::array<u8, 2> first_component_flags{0xe2, 0x80};
        const std::array<u8, 2> second_component_flags{0xe2, 0xc0};
        blend_animation_additive(mix_output.joints, mix_first.joints, mix_second.joints,
                                   first_component_flags, second_component_flags, 0.25f);
        require(mix_output.joints[0].translation.x == 7.f && mix_output.joints[0].translation.y == 20.f,
                "additive joint presence branches differ");
        const std::array<u8, 1> absent_flags{0};
        blend_animation_masked(mix_output.joints, mix_first.joints, mix_second.joints, absent_flags, 0.25f);
        require(mix_output.joints[0].translation.x == 4.f && mix_output.joints[0].translation.y == 8.f,
                "masked joint with no channels did not preserve the first pose");
        blend_animation_masked(mix_output.joints, mix_first.joints, mix_second.joints,
                                 second_component_flags, 0.25f);
        require(mix_output.joints[0].translation.x == 6.f && mix_output.joints[0].translation.y == 11.f,
                "masked joint did not blend all fields of a present joint");
        AnimationResourceRegistry registry;
        registry.loaded = {AnimationLoadedResource{flight_events}, AnimationLoadedResource{rootless_mina}};
        registry.records = {AnimationResourceRecord{1, 1}, AnimationResourceRecord{2, 1}};
        auto event_controller = make_animation_controller(2);
        auto& event_slot = event_controller->slots[1];
        event_slot.player = make_mina_player();
        event_slot.player->resource_registry = &registry;
        start_mina_player(*event_slot.player, rootless_mina, 0.f, 0.f, 1, 0);
        event_slot.player->owned_resource = true;
        event_slot.events = flight_events;
        event_slot.fade_mode = 1;
        event_slot.fade_duration = 0.2f;
        event_slot.priority = 0x32;
        event_controller->event_owner = &registry;
        dispatched_types.clear();
        event_controller->event_callback = [&](std::span<const u8> record, u32 slot, float position,
                                                 void* owner, int phase)
        {
            require(slot == 1 && owner == &registry && phase == 0 && position > 0.f,
                    "controller event callback lost slot, clock or owner");
            dispatched_types.push_back(read_u16(record, 12));
        };
        require(tick_animation_controller(*event_controller, 0.4f, true, registry) &&
                event_slot.fade_fraction == 1.f && event_slot.fade_mode == 0 &&
                dispatched_types == std::vector<int>{29},
                "controller fade-in or first event tick differs");
        require(tick_animation_controller(*event_controller, 0.6f, true, registry) &&
                event_slot.player->position == 1.f && dispatched_types == std::vector<int>{29, 31},
                "controller endpoint event dispatch differs");
        tick_animation_controller(*event_controller, 0.f, true, registry);
        require(event_slot.fade_mode == 3 && event_slot.player != nullptr,
                "priority-0x32 slot did not begin endpoint fade-out");
        tick_animation_controller(*event_controller, 0.2f, true, registry);
        require(event_slot.player == nullptr && event_slot.events.data() == nullptr &&
                registry.records[0].references == 0 && registry.records[1].references == 0 &&
                event_controller->dirty,
                "completed controller fade did not release owned events and player resource");
        event_controller->dirty = false;
        require(!tick_animation_controller(*event_controller, 0.1f, true, registry) && event_controller->idle,
                "empty clean controller did not become idle");
        AnimationResourceRegistry acquisition;
        int finds = 0;
        int loads = 0;
        int hooks = 0;
        const char* resource_name = "aid_mina_registry_test";
        register_animation_resource_provider(acquisition,
            [&](u32 handle, const AnimationResourceRecord& record, const void*& resolved)
            {
                ++finds;
                require(handle == 1 && record.name == resource_name,
                        "resource provider lost handle or name");
                resolved = &rootless_mina;
                return u32(2);
            },
            [&](u32 handle, const void* resolved, AnimationLoadedResource& loaded)
            {
                ++loads;
                require(handle == 1 && resolved == &rootless_mina && loaded.provider_slot == 2,
                        "resource loader lost provider resolution");
                loaded.cpu = rootless_mina;
                return true;
            }, {}, 7, nullptr);
        acquisition.hooks[0].type = "MINA";
        acquisition.hooks[0].post_load = [&](const AnimationLoadedResource& loaded)
        {
            require(loaded.cpu.data() == rootless_mina.data(), "post-load hook lost CPU resource");
            ++hooks;
        };
        require(acquire_animation_resource(acquisition, resource_name).data() == rootless_mina.data() &&
                finds == 1 && loads == 1 && hooks == 1 && acquisition.records[0].references == 1,
                "first resource acquisition did not load and invoke hook");
        acquire_animation_resource(acquisition, resource_name);
        require(finds == 1 && loads == 1 && hooks == 1 && acquisition.records[0].references == 2,
                "retained resource was loaded twice");
        release_animation_resource(acquisition, rootless_mina);
        release_animation_resource(acquisition, rootless_mina);
        acquire_animation_resource(acquisition, resource_name);
        require(finds == 1 && loads == 1 && hooks == 1 && acquisition.records[0].references == 1,
                "zero-reference loaded resource was reloaded");
        AnimationResourceRegistry promoted;
        promoted.loaded = {AnimationLoadedResource{rootless_mina}, AnimationLoadedResource{flight_events}};
        const u32 promoted_hash = animation_resource_hash(resource_name);
        promoted.deferred[0] = {AnimationResourceRecord{1, 0, resource_name, promoted_hash},
                               AnimationResourceRecord{1, 3, "aid_mina_other", 123}};
        promoted.deferred[1] = {AnimationResourceRecord{2, 4, resource_name, promoted_hash}};
        require(acquire_animation_resource(promoted, resource_name).data() == flight_events.data() &&
                promoted.records[0].references == 5 && promoted.deferred[0].size() == 1 &&
                promoted.deferred[0][0].hash == 123 && promoted.deferred[1].empty(),
                "deferred promotion did not replace with second pool and remove by swap");
        AnimationResourceRegistry absent;
        require(acquire_animation_resource(absent, resource_name).data() == nullptr &&
                absent.records[0].references == 0 && absent.records[0].resource_slot == 0,
                "missing resource acquired a fabricated handle");
        acquire_animation_resource(absent, "aid_mina_other");
        require(absent.records.size() == 1 && absent.records[0].name == "aid_mina_other",
                "unloaded name record was not reused");
        AnimationResourceRegistry failed_load;
        register_animation_resource_provider(failed_load,
            [](u32, const AnimationResourceRecord&, const void*&)
            {
                return u32(1);
            },
            [](u32, const void*, AnimationLoadedResource&)
            {
                return false;
            }, {}, 0, nullptr);
        require(acquire_animation_resource(failed_load, resource_name).data() == nullptr &&
                failed_load.records[0].resource_slot == 1 && failed_load.records[0].references == 1,
                "failed provider load rolled back the retail handle/reference behavior");
        ResourceErrorState suppressed_error{};
        suppressed_error.suppressed = 1;
        auto startup = make_animation_controller(2);
        AnimationMix startup_mix;
        startup_mix.duration = -2.f;
        startup_mix.position = 0.75f;
        startup_mix.fade_duration = 0.f;
        startup_mix.borrowed_events = flight_events;
        u32 startup_random = 123;
        require(start_controller_animation(*startup, 1, rootless_mina, nullptr, startup_mix,
                                             absent, suppressed_error, startup_random) &&
                startup->slots[1].player->speed == -0.5f &&
                startup->slots[1].player->position == 0.75f &&
                startup->slots[1].weight == 0.5f && startup->slots[1].borrowed_events &&
                startup->slots[1].fade_fraction == 1.f && startup->slots[1].fade_mode == 0,
                "controller startup lost reverse duration, automatic weight or borrowed events");
        startup_mix.position = -1.f;
        startup_mix.weight = 0.9f;
        startup_mix.borrowed_events = {};
        require(start_controller_animation(*startup, 1, rootless_mina, nullptr, startup_mix,
                                             absent, suppressed_error, startup_random) &&
                startup->slots[1].player->position == 0.75f && startup->slots[1].weight == 0.5f &&
                startup->slots[1].events.data() == nullptr,
                "controller retain-position or explicit-weight branch differs");
        startup_mix.position = -2.f;
        const u32 startup_next_random = startup_random * 0x19660du + 0x3c6ef35fu;
        const float random_position = std::bit_cast<float>((startup_next_random & 0x7fffffu) | 0x3f800000u) - 1.f;
        start_controller_animation(*startup, 1, rootless_mina, nullptr, startup_mix,
                                     absent, suppressed_error, startup_random);
        require(startup_random == startup_next_random && startup->slots[1].player->position == random_position,
                "controller randomized position differs");
        require(!start_controller_animation(*startup, 1, {}, "bad_name", startup_mix,
                                              absent, suppressed_error, startup_random) &&
                startup->slots[1].player == nullptr && startup->slots[1].fade_mode == 0,
                "failed non-primary startup did not clear its slot");
        require(!start_controller_animation(*startup, 0, {}, "aid_mina_missing", startup_mix,
                                              absent, suppressed_error, startup_random) &&
                startup->slots[0].player != nullptr && startup->slots[0].fade_mode == 1,
                "failed primary startup was cleared or fabricated success");
        auto owned_player = make_mina_player();
        owned_player->resource_registry = &acquisition;
        require(start_named_mina_player(*owned_player, acquisition, resource_name, 0.f, 0.f, 1, 0,
                                         suppressed_error) && owned_player->owned_resource &&
                owned_player->resource_name == resource_name && acquisition.records[0].references == 2,
                "named player startup lost ownership or resource reference");
        start_mina_player(*owned_player, rootless_mina, 0.f, 0.f, 1, 0);
        require(!owned_player->owned_resource && owned_player->resource_name.empty() &&
                acquisition.records[0].references == 1,
                "borrowed restart did not release previous named resource");
        PerformanceCounterState performance;
        initialize_performance_counter(performance);
        const auto before_wait = performance_counter_seconds(performance);
        wait_performance_counter(performance, 0.002f);
        const auto after_wait = performance_counter_seconds(performance);
        require(after_wait >= before_wait + 0.0019L,
                "performance-counter wait returned before its float deadline");
        std::vector<u8> error_pixels(size_t(640 * 480 * 4), 0);
        std::vector<u8> error_bitmap(0x3e + 8, 0);
        const u32 error_width = 1;
        const u32 error_height = 64;
        std::memcpy(error_bitmap.data() + 0x12, &error_width, 4);
        std::memcpy(error_bitmap.data() + 0x16, &error_height, 4);
        error_bitmap[0x3e + 7] = 0x80; // message 0, row 7
        ResourceErrorState error_screen{};
        error_screen.background = 0x12345678;
        std::vector<int> boundary_calls;
        auto& api = error_screen.boundaries;
        api.section_handle = [&](const char* section)
        {
            require(std::strcmp(section, "dirtydisk_bitmap") == 0, "wrong error bitmap section");
            boundary_calls.push_back(1);
            return 3;
        };
        api.load_section = [&](int handle)
        {
            require(handle == 3, "error bitmap handle changed");
            boundary_calls.push_back(2);
            return std::span<const u8>(error_bitmap);
        };
        api.section_size = [&](int)
        {
            boundary_calls.push_back(3);
            return u32(error_bitmap.size());
        };
        api.back_buffer = [&]() -> void*
        {
            boundary_calls.push_back(4);
            return &error_pixels;
        };
        api.lock_surface = [&](void* surface)
        {
            require(surface == &error_pixels, "wrong error back buffer");
            boundary_calls.push_back(5);
            return LockedErrorSurface{640 * 4, error_pixels.data()};
        };
        api.release_surface = [&](void*)
        {
            boundary_calls.push_back(6);
        };
        api.insert_fence = [&]()
        {
            boundary_calls.push_back(7);
            return u32(42);
        };
        api.swap = [&]()
        {
            boundary_calls.push_back(8);
        };
        api.block_on_fence = [&](u32 fence)
        {
            require(fence == 42, "error screen fence changed");
            boundary_calls.push_back(9);
        };
        draw_resource_error(error_screen, 0);
        require(boundary_calls == std::vector<int>{1, 2, 3, 4, 5, 6, 7, 8, 9} &&
                error_screen.surface_locks == 0 && error_screen.surface == nullptr &&
                read_u32(error_pixels, size_t((222 * 640 + 319) * 4)) == 0xffffffffu &&
                read_u32(error_pixels, size_t((223 * 640 + 320) * 4)) == 0xffffffffu &&
                read_u32(error_pixels, 0) == 0x12785634u,
                "resource error screen changed pixel order, bitmap expansion or D3D call order");
        boundary_calls.clear();
        api.section_handle = [&](const char*)
        {
            boundary_calls.push_back(1);
            return -1;
        };
        api.last_error = [&]()
        {
            boundary_calls.push_back(10);
            return u32(2);
        };
        draw_resource_error(error_screen, 0);
        require(boundary_calls == std::vector<int>{1, 10},
                "failed error-section lookup did not stop at the last-error gate");
        std::vector<u8> application_mina = skeleton_mina;
        application_mina.resize(320);
        const u32 application_directory = 80;
        const u32 application_node_track = 300;
        std::memcpy(application_mina.data() + 0x20, &application_directory, 4);
        std::memcpy(application_mina.data() + application_directory + 0xc0, &application_node_track, 4);
        skeleton_player.mina = application_mina;
        skeleton_pose.auxiliary[0] = 15.f;
        skeleton_pose.auxiliary[47] = 25.f;
        skeleton_pose.auxiliary[48] = std::bit_cast<float>(0x80000003u);
        auto application = make_animation_controller(1);
        application->slots[0].player = make_mina_player();
        application->selected_player = &skeleton_player;
        application->selected_pose = &skeleton_pose;
        application->dirty = true;
        model_output.skeleton = skeleton_joints;
        model_output.alternate = &alternate;
        const std::array<ModelResourceBlock, 2> application_blocks
        {
            ModelResourceBlock{9, 0xffffffffu}, ModelResourceBlock{1, 0x8001u}
        };
        model_output.resource_blocks = application_blocks;
        ModelNodeFlagCache node_cache{};
        primary.dirty = false;
        alternate.dirty = false;
        apply_controller_to_model(*application, model_output, false, nullptr, node_cache, bind_output[0]);
        require(!application->dirty && alternate.dirty && !primary.dirty &&
                first_auxiliary == 15.f && last_auxiliary == 25.f &&
                model_output.node_flags[0] == 3 && model_output.node_flags[3] == 0x80 &&
                node_cache[0] == 3 && node_cache[15] == 2,
                "controller model application lost alternate pose, auxiliary data or node mask");
        application->dirty = true;
        alternate.dirty = false;
        first_auxiliary = -1.f;
        apply_controller_to_model(*application, model_output, true, nullptr, node_cache, bind_output[0]);
        require(application->dirty && !alternate.dirty && first_auxiliary == -1.f,
                "skip-pose model application changed model or dirty state");
        application->idle = true;
        apply_controller_to_model(*application, model_output, false, nullptr, node_cache, bind_output[0]);
        require(application->dirty && !alternate.dirty,
                "idle model application changed model or dirty state");
        application->idle = false;
        application->slots[0].player.reset();
        application->combined_root.translation = Vec3{1.f, 2.f, 3.f};
        apply_controller_to_model(*application, model_output, false, nullptr, node_cache, bind_output[0]);
        require(!application->dirty && alternate.dirty && alternate.matrices[2] == bind_output[0] &&
                first_auxiliary == 0.f && last_auxiliary == 0.f &&
                application->combined_root.translation.x == 0.f &&
                application->combined_root.rotation.w == 1.f,
                "absent primary player did not reset model, root and auxiliary targets");
        const SkeletonDispatchContext dispatch_context{true, &callback_calls, skeleton_callbacks};
        callback_calls = 0;
        dispatch_mina_skeleton(&dispatch_context, skeleton_player, skeleton_pose, skeleton_joints,
                                skeleton_output, &skeleton_player);
        require(callback_calls == 1, "skeleton dispatch did not use enabled callback list");
        const SkeletonDispatchContext disabled_context{false, &callback_calls, skeleton_callbacks};
        dispatch_mina_skeleton(&disabled_context, skeleton_player, skeleton_pose, skeleton_joints,
                                skeleton_output, &skeleton_player);
        require(callback_calls == 1, "disabled skeleton dispatch still called callback");
        require(resource_names_match("aid_anim_TEST", "aid_ANIM_test") &&
                !resource_names_match("aid_anim_TEST", "aid_anim_other") &&
                resource_names_match("plain_first", "plain_second") &&
                !resource_names_match("plain_first", "aid_anim_TEST"),
                "resource name matching changed its AID-prefix or non-AID branch");
        std::array<AnimationBundleRecord, 2> bundle_records
        {
            AnimationBundleRecord{"aid_anim_other", 0, flight_events, {}, {}, {}},
            AnimationBundleRecord{"aid_anim_TEST", 0, rootless_mina, flight_events, rootless_mina, flight_events}
        };
        std::array<AnimationBundlePool, 2> bundle_pools
        {
            AnimationBundlePool{bundle_records, false}, AnimationBundlePool{bundle_records, true}
        };
        const void* resolved_bundle = nullptr;
        require(find_bundle_resource_provider(bundle_pools, "aid_ANIM_test", resolved_bundle) == 0xffffffffu &&
                resolved_bundle == &bundle_records[1],
                "bundle provider did not skip inactive pool or retain matched record");
        AnimationLoadedResource loaded_bundle;
        require(load_bundle_resource_provider(resolved_bundle, loaded_bundle) &&
                loaded_bundle.cpu.data() == rootless_mina.data() &&
                loaded_bundle.gpu.data() == flight_events.data() && bundle_records[1].references == 1,
                "bundle loader changed payload pointers or record reference count");
        unload_bundle_resource_provider(bundle_pools, "aid_anim_test");
        require(bundle_records[1].references == 0, "bundle release did not find and decrement record");
        std::array<ExecutableResourceRecord, 2> executable_records
        {
            ExecutableResourceRecord{"aid_objparams_other", rootless_mina, {}, 0},
            ExecutableResourceRecord{"aid_objparams_test", flight_events, {}, 0}
        };
        std::vector<ExecutableResourcePool> executable_pools;
        register_executable_resource_pool(executable_pools, executable_records, "objparams", false);
        require(find_executable_resource(executable_pools, "aid_objparams_TEST", false) == nullptr &&
                find_executable_resource(executable_pools, "aid_objparams_TEST", true) == &executable_records[1],
                "executable resource lookup lost hash, type or inactive-pool gate");
        executable_pools[0].active = true;
        const void* resolved_executable = nullptr;
        require(find_executable_resource_provider(executable_pools, "aid_objparams_test", resolved_executable) == 1 &&
                resolved_executable == &executable_records[1],
                "executable provider lost record resolution");
        loaded_bundle.cpu_storage = rootless_mina;
        loaded_bundle.gpu_storage = flight_events;
        require(load_executable_resource_provider(resolved_executable, loaded_bundle) &&
                loaded_bundle.cpu.data() == flight_events.data() &&
                loaded_bundle.cpu_storage.data() == nullptr && loaded_bundle.gpu_storage.data() == nullptr,
                "executable resource loader retained backing allocations");
        std::vector<u8> template_bytes(32, 0x5a);
        const u16 template_size = 24;
        std::memcpy(template_bytes.data(), &template_size, 2);
        executable_records[1].cpu = template_bytes;
        executable_pools[0].active = false;
        const auto cloned_template = clone_executable_resource(executable_pools, "aid_objparams_test");
        require(cloned_template.bytes != nullptr && cloned_template.size == 24 &&
                cloned_template.bytes.get() != template_bytes.data() &&
                std::memcmp(cloned_template.bytes.get(), template_bytes.data(), 24) == 0,
                "executable template clone did not copy its recorded size from an inactive pool");
        require(clone_executable_resource(executable_pools, nullptr).bytes == nullptr &&
                clone_executable_resource(executable_pools, "plain_template").bytes == nullptr &&
                clone_executable_resource(executable_pools, "aid_objparams_absent").bytes == nullptr,
                "missing template clone fabricated a resource");
        AnimationResourceRegistry actor_registry;
        register_animation_resource_provider(actor_registry,
            [&](u32, const AnimationResourceRecord& record, const void*& resolved)
            {
                return find_bundle_resource_provider(bundle_pools, record.name.c_str(), resolved);
            },
            [](u32, const void* resolved, AnimationLoadedResource& output)
            {
                return load_bundle_resource_provider(resolved, output);
            },
            [&](u32, const char* name, const AnimationLoadedResource&)
            {
                unload_bundle_resource_provider(bundle_pools, name);
            }, 3, nullptr);
        ActorAnimationState actor_startup;
        int actor_messages = 0;
        actor_startup.message_handler = [&](const ActorAnimationMessage& message)
        {
            require(message.id == 7 && message.slot == 0 && message.phase == 0 &&
                    read_u16(message.event, 12) == 29 && message.position > 0.f,
                    "actor animation virtual message changed its fields");
            ++actor_messages;
        };
        AnimationMix actor_mix;
        actor_mix.mode = 1;
        actor_mix.borrowed_events = flight_events;
        auto* ensured_controller = ensure_actor_animation_controller(actor_startup);
        require(ensure_actor_animation_controller(actor_startup) == ensured_controller &&
                ensured_controller->event_owner == &actor_startup,
                "actor animation controller lookup replaced the allocated controller");
        start_controller_animation_mix(*ensured_controller, 0, actor_mix, "aid_anim_TEST", actor_registry,
            suppressed_error, startup_random);
        require(actor_startup.controller->slot_count == 4 &&
                actor_startup.controller->event_owner == &actor_startup &&
                actor_startup.controller->slots[0].player->owned_resource,
                "actor startup lost four-slot allocation, owner or named resource");
        tick_animation_controller(*actor_startup.controller, 0.4f, true, actor_registry);
        require(actor_messages == 1, "actor event did not dispatch through virtual message bridge");
        set_actor_animation_position(actor_startup, 0, 1.f);
        require(actor_animation_completed(actor_startup, 0) &&
                actor_startup.controller->slots[0].player->previous == 1.f,
                "actor animation position update or completion differs");
        AnimationMix borrowed_mix;
        borrowed_mix.mode = 1;
        borrowed_mix.borrowed_animation = rootless_mina;
        const size_t records_before_borrow = actor_registry.records.size();
        start_controller_animation_mix(*ensured_controller, 1, borrowed_mix, "aid_anim_MISSING",
            actor_registry, suppressed_error, startup_random);
        require(!ensured_controller->slots[1].player->owned_resource &&
                ensured_controller->slots[1].player->mina.data() == rootless_mina.data() &&
                actor_registry.records.size() == records_before_borrow,
                "borrowed animation wrapper resolved the ignored name or acquired ownership");
        actor_startup.retain_position = 1;
        const AnimationRecord actor_record{"test", "aid_anim_TEST", "", 0.f, 0.f, 0.f, 0.f,
                                            1, 0x31, 0x21, 0};
        start_actor_animation(actor_startup, actor_record, actor_registry, suppressed_error, startup_random);
        require(actor_startup.retain_position == 0 &&
                actor_startup.controller->slots[0].player->position == 1.f,
                "actor record startup lost retain-position argument or its reset");
        require(start_actor_initial_animation(actor_startup, "aid_anim_TEST", actor_registry,
            suppressed_error, startup_random) && actor_startup.controller->slots[0].player->position == 0.f &&
            actor_startup.controller->slots[0].blend_mode == 0x21 &&
            actor_startup.controller->slots[0].priority == 0x31 &&
            actor_startup.controller->slots[0].fade_duration == 0.2f,
            "initial actor animation lost mix defaults or return value");
        release_animation_controller(actor_startup.controller, false, actor_registry);
        require(actor_startup.controller == nullptr && actor_registry.records[0].references == 0,
            "controller release lost player ownership");
        require(!start_actor_initial_animation(actor_startup, "aid_anim_MISSING", actor_registry,
            suppressed_error, startup_random), "missing initial animation fabricated successful startup");
        release_animation_controller(actor_startup.controller, false, actor_registry);
        AnimationCallbackPool callback_owner;
        callback_owner.count = 3;
        for (size_t array = 0; array < 3; ++array)
        {
            callback_owner.arrays[array] = std::make_unique<AnimationCallbackArray>();
            auto& storage = *callback_owner.arrays[array];
            storage.stride = u16((array + 1) * 2);
            storage.used = storage.stride * 3;
            storage.bytes.resize(storage.used);
            for (u16 index = 0; index < 3; ++index)
            {
                const u16 value = u16(10 + index + array * 100);
                std::memcpy(storage.bytes.data() + index * storage.stride, &value, 2);
            }
        }
        actor_startup.pose_callbacks = std::make_unique<AnimationCallbackPool>();
        actor_startup.pose_callbacks->arrays[1] = std::make_unique<AnimationCallbackArray>();
        actor_startup.callback_flags = 1;
        actor_startup.callback_id = 11;
        actor_startup.callback_owner = &callback_owner;
        ModelPoseBuffer primary_reset;
        primary_reset.count = 1;
        primary_reset.matrices.resize(1);
        primary_reset.matrices[0][0] = 7.f;
        ModelPoseBuffer alternate_reset;
        alternate_reset.count = 1;
        alternate_reset.matrices.resize(1);
        alternate_reset.matrices[0][0] = 8.f;
        ModelAnimationState model_reset;
        model_reset.primary = &primary_reset;
        model_reset.alternate = &alternate_reset;
        clear_actor_animation(actor_startup, &model_reset, actor_registry);
        require(actor_startup.controller == nullptr && actor_startup.pose_callbacks == nullptr &&
            callback_owner.count == 2 && alternate_reset.matrices[0][0] == 1.f &&
            primary_reset.matrices[0][0] == 7.f, "actor animation cleanup lost callbacks or alternate pose priority");
        for (size_t array = 0; array < 3; ++array)
        {
            const auto& storage = *callback_owner.arrays[array];
            u16 first;
            u16 second;
            std::memcpy(&first, storage.bytes.data(), 2);
            std::memcpy(&second, storage.bytes.data() + storage.stride, 2);
            require(storage.used == storage.stride * 2 && first == 10 + array * 100 &&
                second == 12 + array * 100, "callback removal changed stable parallel-array ordering");
        }
        clear_actor_animation(actor_startup, nullptr, actor_registry);
        require(retail_sin(0.f) == 0.L && retail_cos(0.f) == 1.L &&
            std::abs(double(retail_cos(3.141592741f) + 1.L)) < 0.00001 &&
            std::abs(double(retail_asin(1.f)) - 1.570796371) < 0.00001 &&
            std::abs(double(retail_asin(-1.f)) + 1.570796371) < 0.00001 &&
            std::abs(double(retail_asin(0.75f) + retail_asin(-0.75f))) < 0.00001,
            "retail scalar trigonometry changed endpoint or sign branches");
        require(retail_atan2(0.f, 1.f) == 0.L &&
            std::abs(double(retail_atan2(1.f, 0.f)) - 1.570796371) < 0.00001 &&
            std::abs(double(retail_atan2(0.f, -1.f)) - 3.141592741) < 0.00001 &&
            std::abs(double(retail_atan2(-1.f, -1.f)) + 2.35619449) < 0.00001 &&
            std::isnan(double(retail_atan2(0.f, 0.f))),
            "retail atan2 changed quadrant or undefined-origin behavior");
        AnimationMatrix normalized_fixture{2.f, 0.f, 0.f, 10.f,
            0.f, 3.f, 0.f, 20.f, 0.f, 0.f, 0.f, 30.f};
        normalize_model_matrix(normalized_fixture.data(), 2.f);
        require(std::abs(normalized_fixture[0] - 2.f) < 0.00001f &&
            std::abs(normalized_fixture[5] - 2.f) < 0.00001f && normalized_fixture[10] == 0.f &&
            normalized_fixture[3] == 10.f && normalized_fixture[7] == 20.f && normalized_fixture[11] == 30.f,
            "column normalization changed zero-column or translation behavior");
        RuntimeSoundBank sound_bank{reinterpret_cast<void*>(0x100)};
        RuntimeSoundCue sound_cue{&sound_bank, reinterpret_cast<void*>(0x200), 4};
        std::vector<int> sound_calls;
        int sound_errors = 0;
        u32 stopped_count = 0;
        RuntimeSoundBoundaries sound_boundaries;
        sound_boundaries.pause_cue = [&](void* bank, void* cue, bool paused)
        {
            require(bank == sound_bank.handle && cue == sound_cue.handle && !paused &&
                (sound_cue.flags & 2) != 0, "sound stop did not set flag before unpausing");
            sound_calls.push_back(1);
            return i32(0x80004005);
        };
        sound_boundaries.last_error = [&]()
        {
            ++sound_errors;
            return 5u;
        };
        sound_boundaries.stop_cue = [&](void* bank, u32 index, u32 flags, void* cue)
        {
            require(bank == sound_bank.handle && cue == sound_cue.handle && index == 0xFFFFFFFF && flags == 2,
                "paused sound stop lost immediate flag or cue arguments");
            sound_calls.push_back(2);
        };
        RuntimeSoundVoice sound_voice{6, &sound_cue};
        stop_runtime_sound_voice(sound_voice, 0, stopped_count, sound_boundaries);
        require(sound_voice.state == 9 && sound_cue.flags == 6 && stopped_count == 1 && sound_errors == 1 &&
            sound_calls == std::vector<int>{1, 2}, "pause failure prevented sound stop or changed flag retention");
        stop_runtime_sound_cue(sound_cue, 1, stopped_count, sound_boundaries);
        require(stopped_count == 1 && sound_calls.size() == 2, "sound cue stopped twice");
        RuntimeSoundVoice pending_voice{2, &sound_cue};
        stop_runtime_sound_voice(pending_voice, 0, stopped_count, sound_boundaries);
        require(pending_voice.state == 4 && stopped_count == 1, "pending sound voice called cue stop");
        RuntimeSoundVoice empty_voice{6, nullptr};
        stop_runtime_sound_voice(empty_voice, 0, stopped_count, sound_boundaries);
        require(empty_voice.state == 10, "missing sound cue did not retire voice");
        sound_cue.flags = 0;
        sound_cue.handle = nullptr;
        stop_runtime_sound_cue(sound_cue, 0, stopped_count, sound_boundaries);
        require(sound_cue.flags == 2 && stopped_count == 2 && sound_calls.size() == 2,
            "unbound cue stop called XACT or lost stop count");
        std::array<RuntimeSoundVoice, 2> queue_voices{{{2, &sound_cue}, {6, nullptr}}};
        RuntimeActorSoundQueues sound_queues;
        sound_queues.voices = queue_voices;
        RuntimeActorSound first_sound;
        RuntimeActorSound second_sound;
        sound_queues.groups[1].active.next = &first_sound;
        first_sound.previous = &sound_queues.groups[1].active;
        first_sound.next = &second_sound;
        second_sound.previous = &first_sound;
        second_sound.previous_bone = 0;
        require(actor_sound_is_active(second_sound, sound_queues), "active sound membership lost predecessor walk");
        release_actor_sound(second_sound, sound_queues);
        require(queue_voices[0].state == 4 && first_sound.next == nullptr &&
            second_sound.previous == nullptr && second_sound.next == nullptr &&
            second_sound.previous_bone == 0xFFFF && second_sound.enabled == 1,
            "active sound release lost voice retirement or list repair");
        sound_queues.groups[0].pending.next = &second_sound;
        second_sound.previous = &sound_queues.groups[0].pending;
        second_sound.previous_bone = 1;
        require(!actor_sound_is_active(second_sound, sound_queues), "pending queue counted as playing");
        release_actor_sound(second_sound, sound_queues);
        require(queue_voices[1].state == 6 && second_sound.previous_bone == 1 &&
            sound_queues.groups[0].pending.next == nullptr,
            "pending release incorrectly stopped a voice");
        Vec3 sound_direction{3.f, 4.f, 5.f};
        first_sound.previous_bone = 1;
        second_sound.previous = &sound_queues.groups[1].pending;
        second_sound.enabled = 0;
        sound_queues.groups[1].pending.next = &second_sound;
        std::array<i16, 2> static_sounds{0, -1};
        sound_queues.static_voice_indices = static_sounds;
        sound_queues.static_voice_count = 2;
        reset_actor_sound_queues(sound_queues);
        require(queue_voices[1].state == 10 && first_sound.enabled == 1 && second_sound.enabled == 0 &&
            first_sound.previous_bone == 0xFFFF && sound_queues.groups[1].active.next == nullptr &&
            second_sound.previous == nullptr && sound_queues.groups[1].pending.next == nullptr &&
            static_sounds[0] == -1 && sound_queues.static_voice_count == 0,
            "sound reset lost active/pending distinction or static voice retirement");
        add_sound_direction(sound_direction, 0.f, 0.f, 2.f);
        require(sound_direction.x == 3.f && sound_direction.y == 4.f && sound_direction.z == 7.f,
            "sound direction replaced vector rather than accumulating");
        require(animation_remainder(5.5f, 2.f) == 1.5L &&
                animation_remainder(-5.5f, 2.f) == -1.5L &&
                animation_remainder(5.5f, -2.f) == 1.5L &&
                animation_remainder(-0.5f, 2.f) == -0.5L &&
                animation_remainder(2.f, 2.f) == 0.L,
                "animation remainder changed sign, quotient truncation or magnitude gate");
        RuntimeActorSoundGroup dispatch_group;
        dispatch_group.capacity = 1;
        RuntimeActorSound request_a;
        RuntimeActorSound request_b;
        dispatch_group.pending.next = &request_a;
        request_a.previous = &dispatch_group.pending;
        request_a.next = &request_b;
        request_b.previous = &request_a;
        int starts = 0;
        dispatch_pending_actor_sounds(dispatch_group, [&](RuntimeActorSound&) -> i16
        {
            ++starts;
            return -1;
        }, [](Actor&, RuntimeActorSound&)
        {
            throw std::runtime_error("failed voice notified its owner");
        });
        require(starts == 1 && dispatch_group.active.next == nullptr &&
            dispatch_group.pending.next == nullptr && request_a.previous_bone == 0xFFFF &&
            request_b.previous == nullptr,
            "failed allocation fabricated an active voice or retried dropped requests");
        dispatch_group.pending.next = &request_a;
        request_a.previous = &dispatch_group.pending;
        dispatch_pending_actor_sounds(dispatch_group, [](RuntimeActorSound&) -> i16
        {
            return 7;
        }, [](Actor&, RuntimeActorSound&)
        {
        });
        dispatch_group.pending.next = &request_b;
        request_b.previous = &dispatch_group.pending;
        dispatch_pending_actor_sounds(dispatch_group, [&](RuntimeActorSound&) -> i16
        {
            ++starts;
            return 8;
        }, [](Actor&, RuntimeActorSound&)
        {
        });
        require(starts == 1 && dispatch_group.active.next == &request_a &&
            request_a.previous_bone == 7 && dispatch_group.pending.next == nullptr &&
            request_b.previous == nullptr,
            "sound group exceeded capacity or retained a dropped pending request");
        std::cout << "animation event boundary checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
