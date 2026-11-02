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
namespace
{
// The +0x34 / +0x38 animation records each death state found; the state words only say whether one was found.
struct PlayerDeathRecords
{
    const AnimationRecord* dying = nullptr; // +0x34
    const AnimationRecord* dead = nullptr; // +0x38
};
std::unordered_map<const u8*, PlayerDeathRecords> player_death_records;

u32 word(std::span<const u8> bytes, size_t offset)
{
    return read_u32(bytes, offset);
}

void put_word(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

void put_float(std::span<u8> bytes, size_t offset, float value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

const PlayerDeathRecords& records_of(std::span<const u8> state)
{
    const auto found = player_death_records.find(state.data());
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
void release_contacts(std::span<u8> state, RuntimePlayerDeathContext& context)
{
    if (word(state, 0x28) != 0)
    {
        clear_runtime_body_contacts(context.knockdown); // 0x00025FA0, EDX body
        context.knockdown.body[0x717] = state[0x30];
        put_word(state, 0x28, 0);
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
    output.instance = 0x3C;
    output.parameters = 0x18;
}

// Xbox 0x0008EF30
bool initialize_runtime_player_death_state(std::span<u8> state, std::span<const u8> parameters,
    RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    knockdown.bits.bytes[3] |= 8;
    knockdown.bits.bytes[3] |= 0x10;
    ++body[0x30C];
    ++body[0x30A];
    ++body[0x309];
    ++body[0x30B];
    const u16 count = u16(get(body, body_field::move_holds) + 1);
    std::memcpy(body.data() + 0x30, &count, 2);
    body[0x38] = 1;
    body[0x39] = 1;
    increment_runtime_body_latches(body, knockdown.actor, context.latches); // 0x0001FC40, ECX body
    body[0x715] = 1;
    const u8 saved = body[0x717];
    body[0x717] = 1;
    put_word(state, 0x28, 1);
    put_word(state, 0x30, saved);
    put_word(state, 0x24, word(parameters, 0x14));
    put_word(state, 0x2C, 0);
    const RuntimeBodyAttachedActor* attached = knockdown.attachment.ownership.attached; // Body +0x48.
    if (attached == nullptr || runtime_attachment_available(attached->bytes, attached->host))
    {
        release_runtime_player_attachment(0, 0, knockdown.attachment); // 0x0001B810, EAX body
    }
    if (!context.mind.empty())
    {
        set(context.mind, mind_field::target, 0);
        if (get(context.mind, mind_field::target_record) != 0)
        {
            const std::array<u32, 2> message{100, 0};
            knockdown.actor_message(message); // Mind +8 (the actor) vtable +0xC.
        }
        set(context.mind, mind_field::target_record, 0);
        set(context.mind, mind_field::action_record, 0);
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
    state[0x20] = set_runtime_body_shape_mode(2, knockdown.shape); // 0x0001FEB0, BL 2
    auto& records = player_death_records[state.data()];
    const auto name_at = [&knockdown](u32 address) -> const char*
    {
        return knockdown.string_at(address);
    };
    // The inlined 0x0003AAA0 pairs: the attached actor's +0xDB0 variant first, then the plain name.
    records.dying = word(parameters, 0xC) == 0 ? nullptr
        : find_runtime_body_animation(name_at(word(parameters, 0xC)), knockdown.actions);
    put_word(state, 0x34, records.dying != nullptr ? 1 : 0);
    records.dead = find_runtime_body_animation(name_at(word(parameters, 0x10)), knockdown.actions);
    put_word(state, 0x38, records.dead != nullptr ? 1 : 0);
    if (word(state, 0x34) != 0)
    {
        put_word(state, 0x18, 0);
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
void destroy_runtime_player_death_state(std::span<u8> state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    const u16 count = u16(get(body, body_field::move_holds) - 1);
    std::memcpy(body.data() + 0x30, &count, 2);
    --body[0x309];
    --body[0x30A];
    --body[0x30B];
    --body[0x30C];
    set_runtime_body_shape_mode(state[0x20], knockdown.shape); // 0x0001FEB0, BL state +0x20
    release_contacts(state, context);
    decrement_runtime_body_latches(body, knockdown.actor, context.latches); // 0x0001FD20, ECX body
    knockdown.bits.bytes[3] &= 0xEF;
    if (live_player(context))
    {
        --context.active_strategies;
        --context.scene_counter;
    }
    if (word(state, 0x2C) != 0)
    {
        release_audio_stream("XACT_SOUNDBANK_GSTREAMSDITTIES_FAILPITCHBEND", context.streams); // 0x00038780
        put_word(state, 0x2C, 0);
    }
    player_death_records.erase(state.data());
}

// Xbox 0x0008F4D0
void enter_runtime_player_death_phase(std::span<u8> state, u32 phase, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    put_word(state, 0x18, phase);
    switch (phase)
    {
    case 0:
        start_record(records_of(state).dying, knockdown.actions); // 0x0001AFA0
        return;
    case 1:
    {
        const std::array<u32, 2> message{0x34, word(state, 0x24)};
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
        put_float(state, 0x1C, 1.f); // DAT_0032B0F8
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
void restart_runtime_player_death(std::span<u8> state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    if (live_player(context))
    {
        context.restart_pending = 1;
    }
    finish_runtime_body_death(0, knockdown.scene_flags, knockdown.restart_requested, knockdown.death); // 0x00134AE0
    put_word(state, 0x18, 3);
}

// Xbox 0x0008F6A0
void start_runtime_player_fail_stream(std::span<u8> state, RuntimePlayerDeathContext& context)
{
    put_word(state, 0x2C, 1);
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
void tick_runtime_player_death_state(std::span<u8> state, RuntimePlayerDeathContext& context)
{
    auto& knockdown = context.knockdown;
    const std::span<u8> body = knockdown.body;
    const float elapsed = runtime_actor_rate(knockdown.actor) * runtime_frame_step(knockdown.clock)
        + read_f32(state, 0x10);
    put_float(state, 0x10, elapsed);
    const auto* controller = knockdown.animation.controller.get(); // Actor +0x50.
    const MinaPlayer* slot = controller == nullptr ? nullptr : controller->slots[0].player.get();
    if (slot != nullptr && slot->position > 0.5f) // DAT_0032B0F4
    {
        const float facing = read_f32(knockdown.actor_bytes, 0x18);
        put_float(body, 0x72C, runtime_floor_pitch(knockdown.actor, knockdown.actor_bytes)); // 0x00136150
        put_float(body, 0x730, facing);
        put_float(body, 0x734, 0.f);
    }
    else
    {
        put_float(body, 0x734, 0.f);
        put_float(body, 0x72C, 0.f);
    }
    switch (word(state, 0x18))
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
            put_word(state, 0x18, 2);
            release_contacts(state, context);
            put_float(state, 0x1C, 1.f); // DAT_0032B0F8
            if (live_player(context) && (knockdown.scene_flags & 0x8000) == 0)
            {
                context.begin_transition(); // 0x0012C5D0
            }
        }
        return;
    case 2:
        if (!live_player(context))
        {
            float timer = read_f32(state, 0x1C);
            const bool done = count_runtime_state_timer(timer, knockdown.actor, knockdown.clock); // 0x0002B630
            put_float(state, 0x1C, timer);
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
            put_word(state, 0x18, 4);
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
