#include "body_player_death_state.h"
#include "mind_fields.h"
#include "body_damage_notices.h"
#include "body_damage_tables.h"
#include "player_goal_cleanup.h"
#include "player_mind_requests.h"
#include "player_attachment_helpers.h"
#include "body_action_ids.h"
#include "../scripts/script_conditions.h"
#include "../scripts/script_vm.h"
#include "../scripts/script_chapter.h"
#include "gbtg/anim.h"
#include "body_fields.h"

#include <bit>
#include <cstring>
#include <unordered_map>

namespace gbtg
{
namespace parameter = player_death_state_parameter;

namespace
{
// The animation records each death state found; the state's found flags only say whether one was found.
struct PlayerDeathRecords
{
    const AnimationRecord* dying = nullptr;
    const AnimationRecord* dead = nullptr;
};
std::unordered_map<const u8*, PlayerDeathRecords> player_death_records;

const PlayerDeathRecords& records_of(const PlayerDeathState& state)
{
    const auto found = player_death_records.find(body_state_key(state));
    if (found == player_death_records.end())
    {
        throw Error("player death state has no animation records");
    }
    return found->second;
}

// 0x0001AFA0 with ECX the record; retail reads the record's fields, so a missing one is not played.
void start_record(const AnimationRecord* record, RuntimeBodyActionSelection& actions)
{
    if (record == nullptr)
    {
        throw Error("player death animation record is null; 0x0001AFA0 reads it");
    }
    start_runtime_body_animation_record(*record, actions);
}

bool live_player(RuntimePlayerDeathContext& context)
{
    return runtime_actor_is_live_player(context.knockdown.actor, context.knockdown.death.live); // 0x00028D20
}

// Body +0x715 / +0x717 back to what they were when the state began.
void release_contacts(PlayerDeathState& state, RuntimePlayerDeathContext& context)
{
    if (state.contacts_raised != 0)
    {
        clear_runtime_body_contacts(context.knockdown); // 0x00025FA0, EDX body
        body_record(context.knockdown.body).collision_kind = u8(state.saved_717);
        state.contacts_raised = 0;
    }
}

// The brew offer (state 4's entry and the waiting state's choice): the story bit 0x21 and "intros_brewoffer".
void offer_brew(RuntimePlayerDeathContext& context)
{
    if ((context.story_bits[4] & 2) == 0 && context.frontend_state != 3)
    {
        set_runtime_story_bit(context.story_bits, 0x21); // 0x000F29D0
        enqueue_runtime_dialogue_key("intros_brewoffer", 0, context.dialogue); // 0x000582A0, 0x00302878
    }
}
}

// Xbox 0x0008EEB0
void player_death_state_object_sizes(ObjectSizes& output)
{
    output.instance = sizeof(PlayerDeathState);
    output.parameters = parameter::kSize;
}

// Xbox 0x0008EF30
bool initialize_runtime_player_death_state(PlayerDeathState& state, std::span<const u8> parameters,
    RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    knockdown.bits.bytes[3] |= 8;
    knockdown.bits.bytes[3] |= 0x10;
    ++body_record(body).unk_30C;
    ++body_record(body).unk_30A;
    ++body_record(body).unk_309;
    ++body_record(body).unk_30B;
    body_record(body).move_holds = u16(body_record(body).move_holds + 1);
    body_record(body).unk_38 = 1;
    body_record(body).unk_39 = 1;
    increment_runtime_body_latches(body, knockdown.actor, context.latches); // 0x0001FC40, ECX body
    body_record(body).unk_715 = 1;
    const u8 saved = body_record(body).collision_kind;
    body_record(body).collision_kind = 1;
    state.contacts_raised = 1;
    state.saved_717 = saved;
    state.death_word = get(parameters, parameter::death_word);
    state.fail_stream = 0;
    const RuntimeBodyAttachedActor* attached = knockdown.attachment.ownership.attached; // Body +0x48.
    if (attached == nullptr || runtime_attachment_available(attached->bytes, attached->host))
    {
        release_runtime_player_attachment(0, 0, knockdown.attachment); // 0x0001B810, EAX body
    }
    if (!context.mind.empty())
    {
        mind_record(context.mind).target = 0;
        if (mind_record(context.mind).target_record != 0)
        {
            const std::array<u32, 2> message{100, 0};
            knockdown.actor_message(message); // Mind +8 (the actor) vtable +0xC.
        }
        mind_record(context.mind).target_record = 0;
        mind_record(context.mind).action_record = 0;
        for (LiveScript* event = context.mind_events == nullptr ? nullptr : context.mind_events->next; event != nullptr;
             event = event->next)
        {
            if (u16(event->type) == 3)
            {
                script_release_event(event); // 0x000E9210
                break;
            }
        }
    }
    state.saved_shape_mode = set_runtime_body_shape_mode(2, knockdown.shape); // 0x0001FEB0, BL 2
    auto& records = player_death_records[body_state_key(state)];
    const auto name_at = [&knockdown](u32 address) -> const char*
    {
        return knockdown.string_at(address);
    };
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    records.dying = get(parameters, parameter::dying) == 0
        ? nullptr
        : find_runtime_body_animation(name_at(get(parameters, parameter::dying)), knockdown.actions);
    state.dying_found = records.dying != nullptr ? 1 : 0;
    records.dead = find_runtime_body_animation(name_at(get(parameters, parameter::dead)), knockdown.actions);
    state.dead_found = records.dead != nullptr ? 1 : 0;
    if (state.dying_found != 0)
    {
        state.phase = 0;
        start_record(records.dying, knockdown.actions); // 0x0001AFA0
    }
    else
    {
        enter_runtime_player_death_phase(state, 1, context); // 0x0008F4D0
    }
    if (live_player(context))
    {
        ++context.active_strategies;
        ++context.scene_counter;
    }
    return true;
}

// Xbox 0x0008F1A0
void destroy_runtime_player_death_state(PlayerDeathState& state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    body_record(body).move_holds = u16(body_record(body).move_holds - 1);
    --body_record(body).unk_309;
    --body_record(body).unk_30A;
    --body_record(body).unk_30B;
    --body_record(body).unk_30C;
    set_runtime_body_shape_mode(state.saved_shape_mode, knockdown.shape); // 0x0001FEB0, BL the saved mode
    release_contacts(state, context);
    decrement_runtime_body_latches(body, knockdown.actor, context.latches); // 0x0001FD20, ECX body
    knockdown.bits.bytes[3] &= 0xEF;
    if (live_player(context))
    {
        --context.active_strategies;
        --context.scene_counter;
    }
    if (state.fail_stream != 0)
    {
        release_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_FAILPITCHBEND", context.streams); // 0x00038780
        state.fail_stream = 0;
    }
    player_death_records.erase(body_state_key(state));
}

// Xbox 0x0008F4D0
void enter_runtime_player_death_phase(PlayerDeathState& state, u32 phase, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    state.phase = phase;
    switch (phase)
    {
    case 0:
        start_record(records_of(state).dying, knockdown.actions); // 0x0001AFA0
        return;
    case 1:
    {
        const std::array<u32, 2> message{0x34, state.death_word};
        knockdown.actor_message(message); // Actor vtable +0xC.
        start_record(records_of(state).dead, knockdown.actions); // 0x0001AFA0
        const RuntimeBodyAttachedActor* attached = knockdown.attachment.ownership.attached; // Body +0x48.
        if (attached != nullptr && runtime_attached_actor_allows_range(*attached)) // 0x001460C0
        {
            release_runtime_player_attachment(1, 1, knockdown.attachment); // 0x0001B810
        }
        notify_runtime_body_death_once(knockdown.body, knockdown.actor_address, knockdown.game); // 0x00024910
        if (live_player(context))
        {
            float duration = 0.f;
            const auto* controller = knockdown.animation.controller.get(); // Actor +0x50.
            if (controller != nullptr && controller->slots[0].player != nullptr)
            {
                duration = controller->slots[0].player->duration; // Slot 0 +0x20.
            }
            start_runtime_camera_pulse(duration, 0.f, 10.f, 1.f, 0.f, 1, knockdown.scene_flags,
                context.pulse); // 0x00061960
            if ((knockdown.scene_flags & 0x4000) == 0 || scene_end_flag_set()) // 0x00049D10
            {
                start_runtime_player_fail_stream(state, context); // 0x0008F6A0
            }
            context.transition_row_two = 1;
        }
        return;
    }
    case 2:
        release_contacts(state, context);
        state.wait_timer = 1.f; // DAT_0032B0F8
        if (live_player(context) && (knockdown.scene_flags & 0x8000) == 0)
        {
            context.begin_transition(); // 0x0012C5D0
        }
        return;
    case 4:
        offer_brew(context);
        return;
    default:
        return;
    }
}

// Xbox 0x0008F650
void restart_runtime_player_death(PlayerDeathState& state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    if (live_player(context))
    {
        context.restart_pending = 1;
    }
    finish_runtime_body_death(0, knockdown.scene_flags, knockdown.restart_requested, knockdown.death); // 0x00134AE0
    state.phase = 3;
}

// Xbox 0x0008F6A0
void start_runtime_player_fail_stream(PlayerDeathState& state, RuntimePlayerDeathContext& context)
{
    state.fail_stream = 1;
    request_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_FAILPITCHBEND", 0xE, context.streams); // 0x00038720
    hold_lower_priority_audio_banks(0xE, context.streams); // 0x00038A10, EBX 0xE
}

// Xbox 0x0002B630
bool count_runtime_state_timer(float& timer, const Actor& actor, const RuntimeActorClock& clock)
{
    const float step = runtime_frame_step(clock);
    const float rate = runtime_actor_rate(actor);
    if (0.f < timer)
    {
        timer = timer - rate * step;
        if (timer <= 0.f)
        {
            timer = 0.f;
            return true;
        }
    }
    return false;
}

// Xbox 0x0008F290
void tick_runtime_player_death_state(PlayerDeathState& state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    const float elapsed = runtime_actor_rate(knockdown.actor) * runtime_frame_step(knockdown.clock)
        + state.base.elapsed;
    state.base.elapsed = elapsed;
    const auto* controller = knockdown.animation.controller.get(); // Actor +0x50.
    const MinaPlayer* slot = controller == nullptr ? nullptr : controller->slots[0].player.get();
    if (slot != nullptr && slot->position > 0.5f) // DAT_0032B0F4
    {
        const float facing = read_f32(knockdown.actor_bytes, 0x18);
        // 0x00136150
        body_record(body).euler = Vec3{runtime_floor_pitch(knockdown.actor, knockdown.actor_bytes), facing, 0.f};
    }
    else
    {
        level_body_euler(body);
    }
    switch (state.phase)
    {
    case 0:
        if (runtime_actor_animation_slot_complete(knockdown.animation, 0)) // 0x0003B290, ECX 0
        {
            enter_runtime_player_death_phase(state, 1, context);
        }
        return;
    case 1:
        if (runtime_actor_animation_slot_complete(knockdown.animation, 0)) // 0x0003B290, ECX 0
        {
            state.phase = 2;
            release_contacts(state, context);
            state.wait_timer = 1.f; // DAT_0032B0F8
            if (live_player(context) && (knockdown.scene_flags & 0x8000) == 0)
            {
                context.begin_transition(); // 0x0012C5D0
            }
        }
        return;
    case 2:
        if (!live_player(context))
        {
            float timer = state.wait_timer;
            const bool done = count_runtime_state_timer(timer, knockdown.actor, knockdown.clock); // 0x0002B630
            state.wait_timer = timer;
            if (done)
            {
                restart_runtime_player_death(state, context);
            }
            return;
        }
        if (context.transition_phase != 2)
        {
            return;
        }
        if (test_runtime_unlock_bit(context.unlock_bits, 3) && context.scene_kills >= context.brew_kills_required
            && script_chapter_index(context.scene_name) < 8 && context.frontend_state != 3) // 0x000F2B30, 0x0004B4D0
        {
            state.phase = 4;
            offer_brew(context);
            return;
        }
        restart_runtime_player_death(state, context);
        return;
    case 4:
        if (context.prompt_owner == 0)
        {
            restart_runtime_player_death(state, context);
        }
        return;
    default:
        return;
    }
}
}
