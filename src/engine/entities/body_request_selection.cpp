#include "body_request_selection.h"
#include <cstddef>
#include "weapon_fields.h"
#include "player_mind_control.h"
#include "player_mind_targets.h"
#include "mind_fields.h"
#include "actor_transform.h"
#include "body_damage_motion.h"
#include "shape_copy.h"
#include "entity_heap.h"
#include "player_mind_target_control.h"
#include "player_strategy_goals.h"
#include "../audio/listener_settings.h"
#include "body_fields.h"
#include "player_fields.h"
#include <bit>
#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace gbtg
{
namespace
{
void write_u32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

void write_u16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
}

void engine_copy_bytes(void* destination, const void* source, u32 count); // Xbox 0x000F71F0

// Xbox 0x0007D890
void update_runtime_player_mind_action_control(RuntimePlayerMindActionControl& context)
{
    auto& control = context.targets;
    auto mind = control.focus.mind;
    auto body = context.body.body;
    std::array<float, 2> axes;
    runtime_input_secondary_axes(context.input, axes, context.input_state);
    const bool active = body_record(body).energy != 0 && !(player_mind_record(mind).control_lock > 0.f);
    auto* attached = context.body.actions.attached;
    update_runtime_player_input_direction(retail_bytes(player_mind_record(mind).stick), axes,
        runtime_frame_step(control.clock), attached != nullptr && weapon_base(attached->bytes).unk_93D == 4);
    if (active)
    {
        const auto stick = retail_bytes(player_mind_record(mind).stick);
        u32 enabled = ((stick_record(stick).press_timer > 0.f || stick_record(stick).pressed != 0)
            && stick_record(stick).unk_0C == 0.f) ? 1 : 0;
        attached = context.body.actions.attached;
        if (attached != nullptr && weapon_base(attached->bytes).unk_93D == 4)
        {
            const i32 mode = std::bit_cast<i32>(stick_record(stick).gesture);
            enabled = mode >= 2 && mode <= 3 ? 1 : 0;
        }
        update_runtime_player_target_control(control, enabled, stick_record(stick).direction);
        u32 target = 0;
        auto& events = control.proposal.events;
        for (LiveScript* event = events.anchor.next; event != nullptr; event = event->next)
        {
            if (u16(event->type) == 3)
            {
                target = read_u32(static_cast<RuntimeMindEvent*>(event)->bytes, 0x20);
                break;
            }
        }
        const Actor* focus = control.focus.target;
        if (target == 0 || enabled == 0)
        {
            player_mind_record(mind).attachment_noticed = 0;
        }
        else
        {
            if (body_record(body).unk_6A != 0)
            {
                set_runtime_mind_alert_event(events, target);
                if ((context.flags.bytes[5] & 0x80) != 0)
                {
                    set_runtime_mind_target(events, target);
                }
            }
            attached = context.body.actions.attached;
            const i32 mode = attached == nullptr ? 0 : weapon_base(attached->bytes).unk_93D;
            auto& binding = context.binding(target);
            update_runtime_player_target_action(binding.requests, mode, focus,
                binding.target_body, context.attachment_notice);
        }
    }
    else
    {
        player_mind_record(mind).attachment_noticed = 0;
    }
    const float step = runtime_frame_step(control.clock);
    const float timer = player_mind_record(mind).target_timer;
    if (timer > 0.f)
    {
        const float remaining = timer - step;
        player_mind_record(mind).target_timer = remaining;
        if (0.f >= remaining)
        {
            player_mind_record(mind).target_timer = 0.f;
            player_mind_record(mind).target_timer = 0.f;
            player_mind_record(mind).target_count = 0;
        }
    }
    if (body_record(body).unk_6A == 0)
    {
        mind_record(mind).action_record = 0;
        mind_record(mind).target_record = 0;
    }
}

// Xbox 0x0007DF50
void update_runtime_player_target_action(RuntimeMindChainRequestSelection& context,
    i32 mode, const Actor* focus, std::span<const u8> target_body, u32& attachment_notice)
{
    auto& selection = context.mind.selection;
    auto& events = context.mind.events;
    auto mind = events.mind;
    auto body = selection.requests.body;
    u32 direction = 0xFFFFFFFFu;
    if (mode < 3 || mode > 4)
    {
        direction = select_runtime_player_target_action_direction(mind, selection.body,
            selection.actor, selection.target, target_body, focus);
    }
    const u32 target = selection.target.address;
    const bool changed = target != player_mind_record(mind).target;
    u32 previous;
    if (changed)
    {
        previous = body_record(body).unk_88;
        body_record(body).unk_88 = 0;
    }
    if (target != 0)
    {
        auto* event = allocate_runtime_mind_event(events, 3, .5f);
        write_u32(event->bytes, 0x20, target);
    }
    else
    {
        for (LiveScript* event = events.anchor.next; event != nullptr; event = event->next)
        {
            if (u16(event->type) == 3)
            {
                script_release_event(event);
                break;
            }
        }
    }
    const auto* attached = selection.requests.actions.attached;
    u32 flags = 0x1E;
    if (attached != nullptr && weapon_base(attached->bytes).unk_93D == 4)
    {
        flags = player_mind_record(mind).stick.gesture == 3 ? 0x36 : 0x1E;
    }
    if (dispatch_runtime_mind_target_request(context, direction, flags) == 2)
    {
        player_mind_record(mind).stick.press_timer = 0.f;
        player_mind_record(mind).target = target;
        ++player_mind_record(mind).target_count;
        return;
    }
    if (changed)
    {
        body_record(body).unk_88 = previous;
    }
    attached = selection.requests.actions.attached;
    if (attached != nullptr && weapon_base(attached->bytes).unk_93D == 4
        && i32(player_mind_record(mind).stick.gesture) >= 2 && player_mind_record(mind).stick.gesture == 3
        && weapon_base(attached->bytes).mask == 0 && player_mind_record(mind).attachment_noticed == 0)
    {
        attachment_notice = 1;
        player_mind_record(mind).attachment_noticed = 1;
    }
}

// Xbox 0x00136770
u8 dispatch_runtime_mind_target_request(RuntimeMindChainRequestSelection& context,
    u32 direction, u32 flags)
{
    auto& mind = context.mind;
    const auto* attached = mind.selection.requests.actions.attached;
    const u8 kind = attached == nullptr ? 0 : weapon_base(attached->bytes).unk_93D;
    u8 result = 0;
    switch (kind)
    {
    case 0:
        if ((flags & 2) != 0)
        {
            result = request_runtime_mind_chain_target(context, direction, (flags & 4) != 0);
        }
        else if ((flags & 4) != 0)
        {
            result = request_runtime_mind_second_target(mind, direction);
        }
        if (result != 0)
        {
            return result;
        }
        [[fallthrough]];
    case 2:
    case 3:
        if ((flags & 0x10) != 0)
        {
            return request_runtime_mind_category_target(mind, direction);
        }
        break;
    case 1:
        if ((flags & 8) != 0)
        {
            return request_runtime_mind_pair_target(mind, direction);
        }
        break;
    case 4:
        if ((flags & 8) != 0)
        {
            result = request_runtime_mind_pair_target(mind, direction);
        }
        if ((flags & 0x20) != 0)
        {
            result = request_runtime_mind_special_target(mind, direction);
        }
        break;
    default:
        return 0;
    }
    return result;
}

// Xbox 0x00136830
u8 request_runtime_mind_chain_target(RuntimeMindChainRequestSelection& context,
    u32 direction, bool allow_fallback)
{
    auto& mind = context.mind;
    auto& selection = mind.selection;
    if (!runtime_body_accepts_target_request(selection.requests.body, mind.flags,
        player_record(mind.owner).owner_class))
    {
        return 1;
    }
    u32 sequence;
    const u8* row = select_runtime_body_chain_target_request(context, direction,
        sequence, allow_fallback);
    if (row != nullptr)
    {
        set_runtime_mind_target(mind.events, selection.target.address);
        mind_record(mind.events.mind).action_record = selection.target.address;
        accept_runtime_attachment_request(selection.requests, row, sequence);
    }
    return row != nullptr ? 2 : 0;
}

// Xbox 0x0001CDA0
const u8* select_runtime_body_chain_target_request(RuntimeMindChainRequestSelection& context,
    u32 direction, u32& sequence, bool allow_fallback)
{
    auto& selection = context.mind.selection;
    if (selection.requests.tables == nullptr)
    {
        return nullptr;
    }
    if (direction == 0xFFFFFFFFu)
    {
        direction = runtime_body_target_direction(selection.body.body_actor,
            selection.target.bytes, selection.body.body_actor.euler.y);
    }
    direction = u8(direction);
    std::array<u8, 32> shape;
    const u8 height_mask = u8(1u << (runtime_body_target_height(selection.requests.body,
        selection.body.body_actor, selection.target.bytes, shape.data()) & 31));
    u32 length;
    if (mind_target_record(selection.target.bytes).kind == 0)
    {
        if ((selection.target.body.bits->bytes[3] & 2) == 0)
        {
            length = u32(runtime_body_target_chain_length(context.target_length));
        }
        else
        {
            length = 0;
        }
    }
    else
    {
        length = mind_target_record(selection.target.bytes).kind == 3 ? 3 : 0;
    }
    const u8* selected = nullptr;
    u32 address = 0;
    const u32 previous = body_record(selection.requests.body).unk_88;
    if (previous != 0)
    {
        address = resume_runtime_attachment_chain(context.chains, previous, shape.data(),
            height_mask, selected);
    }
    // 0x001413C0 reads the target's body; ESI is this body's actor (local_24, the body's +8).
    const bool special = runtime_mind_target_requires_special_action(selection.target,
        context.target_body, context.mind.owner, context.first_mode, context.second_mode);
    if (address == 0 || special)
    {
        if (i32(length) < 1 || special)
        {
            if (allow_fallback)
            {
                selected = select_runtime_attachment_second_target_row(selection,
                    shape.data(), direction, height_mask);
                sequence = 0;
                return selected;
            }
            sequence = 0;
            return nullptr;
        }
        while (true)
        {
            const u8* chain = find_runtime_attachment_chain(context.chains.chains, length);
            if (chain == nullptr)
            {
                return nullptr;
            }
            address = select_runtime_attachment_chain_row(context.chains, chain, shape.data(),
                direction, height_mask, selected);
            if (address != 0)
            {
                break;
            }
            --length;
            if (i32(length) < 1)
            {
                return nullptr;
            }
        }
        if (i32(length) < 1)
        {
            return nullptr;
        }
    }
    sequence = address;
    return selected;
}

// Xbox 0x0001C0E0
u32 resume_runtime_attachment_chain(RuntimeAttachmentChainSelection& context,
    u32 address, const u8* shape, u8 height_mask, const u8*& selected)
{
    const u8* chain = find_runtime_attachment_chain_for_address(context.target.requests.tables,
        context.chains, address);
    const std::span<const u8> bytes{chain, 8};
    const i32 stride = read_u16(bytes, 0);
    const i32 offset = std::bit_cast<i32>(address - read_u32(bytes, 4)) >> 2;
    if (offset % stride + 1 != stride)
    {
        u32 optional;
        const u8* word = context.resolve(address + 4);
        const u8* row = find_runtime_attachment_target_request(context.target,
            context.resolve(read_u32({word, 4}, 0)), shape, 0xFFFFFFFFu, height_mask, optional);
        if (row != nullptr)
        {
            selected = row;
            return address + 4;
        }
    }
    return 0;
}

// Xbox 0x0001BE60
u32 select_runtime_attachment_chain_row(RuntimeAttachmentChainSelection& context,
    const u8* chain, const u8* shape, u32 direction, u8 height_mask, const u8*& selected)
{
    const std::span<const u8> header{chain, 8};
    const u32 stride = read_u16(header, 0);
    const u32 count = read_u16(header, 2);
    const u32 base = read_u32(header, 4);
    u32 capacity = count;
    const u16 increment = count == 0 ? 10 : u16(count);
    auto* storage = static_cast<u8*>(engine_heap_allocate(i32(count * 12 + 16)));
    write_u16({storage, 16}, 0, 12);
    write_u16({storage, 16}, 2, increment);
    // Native cursor offsets replace the pool's three 32-bit data pointers.
    u32 used = 0;
    u32 optional_count = 0;
    for (u32 index = 0; index < count; ++index)
    {
        u32 address = base + stride * index * 4;
        const u32 end = address + stride * 4;
        const u8* first = nullptr;
        u32 optional = 0;
        while (address < end)
        {
            const u8* word = context.resolve(address);
            const u8* row = find_runtime_attachment_target_request(context.target,
                context.resolve(read_u32({word, 4}, 0)), shape, direction, height_mask, optional);
            if (row == nullptr)
            {
                first = nullptr;
                break;
            }
            if (first == nullptr)
            {
                first = row;
            }
            address += 4;
        }
        if (first != nullptr)
        {
            if (used == capacity)
            {
                capacity = used + increment;
                storage = static_cast<u8*>(engine_heap_reallocate(storage, i32(capacity * 12 + 16)));
            }
            std::span<u8> candidate{storage + 16 + used * 12, 12};
            write_u32(candidate, 0, context.target.requests.address_of(first));
            write_u32(candidate, 4, index);
            write_u32(candidate, 8, optional);
            ++used;
            if (optional != 0)
            {
                ++optional_count;
            }
        }
    }
    u32 result = 0;
    if (used != 0)
    {
        if (optional_count != used)
        {
            u32 index = 0;
            while (index < used)
            {
                u8* candidate = storage + 16 + index * 12;
                if (read_u32({candidate, 12}, 8) != 0)
                {
                    if (index < used - 1)
                    {
                        engine_copy_bytes(candidate, storage + 16 + (used - 1) * 12, 12);
                    }
                    --used;
                }
                else
                {
                    ++index;
                }
            }
        }
        u32& random = context.target.random;
        random = random * 0x19660Du + 0x3C6EF35Fu;
        const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u) - 1.f;
        const i32 index = _mm_cvtt_ss2si(_mm_set_ss(fraction * float(i32(used))));
        const std::span<const u8> candidate{storage + 16 + index * 12, 12};
        selected = context.resolve(read_u32(candidate, 0));
        result = base + stride * read_u32(candidate, 4) * 4;
    }
    engine_heap_free(storage);
    return result;
}

// Xbox 0x000243C0
i32 runtime_body_target_chain_length(const RuntimeBodyChainLength& context)
{
    if (context.model_state == nullptr)
    {
        return 0;
    }
    float value = context.upper;
    const u16 classification = player_record(context.owner).damage_class;
    if (classification != 0 && context.maximum != context.minimum)
    {
        value = ((float(context.class_table[u32(classification) * 0x87 - 0x38])
            - float(context.minimum)) / (float(context.maximum) - float(context.minimum)))
            * (context.upper - context.lower) + context.lower;
        if (!(context.upper > context.lower))
        {
            if (context.upper > value)
            {
                value = context.upper;
            }
            else if (value > context.lower)
            {
                value = context.lower;
            }
        }
        else
        {
            if (value > context.upper)
            {
                value = context.upper;
            }
            else if (context.lower > value)
            {
                value = context.lower;
            }
        }
    }
    const i32 length = _mm_cvtt_ss2si(_mm_set_ss(value));
    if (length == 0)
    {
        return 1;
    }
    const float scale = body_record(context.body).stagger;
    if (context.model_state[0x1A4] != 0 && scale == 0.f
        && (context.flags.bytes[3] & 2) == 0)
    {
        return 1;
    }
    return _mm_cvtt_ss2si(_mm_set_ss(float(length) * scale + .5f));
}

// Xbox 0x0001C510
const u8* find_runtime_attachment_chain_for_address(const RuntimeAttachmentRequestTables* tables,
    const RuntimeAttachmentChainRows& chains, u32 address)
{
    if (tables == nullptr)
    {
        return nullptr;
    }
    for (u32 index = 0; index < chains.count; ++index)
    {
        const u8* row = chains.rows + index * 8;
        const std::span<const u8> bytes{row, 8};
        const u32 first = read_u32(bytes, 4);
        const u32 end = first + u32(read_u16(bytes, 2)) * u32(read_u16(bytes, 0)) * 4;
        if (first <= address && address < end)
        {
            return row;
        }
    }
    return nullptr;
}

// Xbox 0x001368B0
u8 request_runtime_mind_second_target(RuntimeMindTargetRequestSelection& context, u32 direction)
{
    auto& selection = context.selection;
    if (!runtime_body_accepts_target_request(selection.requests.body, context.flags,
        player_record(context.owner).owner_class))
    {
        return 1;
    }
    const u8* row = select_runtime_body_second_target_request(selection, direction);
    if (row != nullptr)
    {
        set_runtime_mind_target(context.events, selection.target.address);
        mind_record(context.events.mind).action_record = selection.target.address;
        accept_runtime_attachment_request(selection.requests, row, 0);
    }
    return row != nullptr ? 2 : 0;
}

// Xbox 0x00136920
u8 request_runtime_mind_pair_target(RuntimeMindTargetRequestSelection& context, u32 direction)
{
    auto& selection = context.selection;
    if (!runtime_body_accepts_target_request(selection.requests.body, context.flags,
        player_record(context.owner).owner_class))
    {
        return 1;
    }
    const u8* row = select_runtime_body_pair_target_request(selection, direction);
    if (row != nullptr)
    {
        set_runtime_mind_target(context.events, selection.target.address);
        mind_record(context.events.mind).action_record = selection.target.address;
        accept_runtime_attachment_request(selection.requests, row, 0);
    }
    return row != nullptr ? 2 : 0;
}

// Xbox 0x00136990
u8 request_runtime_mind_category_target(RuntimeMindTargetRequestSelection& context, u32 direction)
{
    auto& selection = context.selection;
    if (!runtime_body_accepts_target_request(selection.requests.body, context.flags,
        player_record(context.owner).owner_class))
    {
        return 1;
    }
    const u8* row = select_runtime_body_category_target_request(selection, direction);
    if (row != nullptr)
    {
        set_runtime_mind_target(context.events, selection.target.address);
        mind_record(context.events.mind).action_record = selection.target.address;
        accept_runtime_attachment_request(selection.requests, row, 0);
    }
    return row != nullptr ? 2 : 0;
}

// Xbox 0x00136A00
u8 request_runtime_mind_special_target(RuntimeMindTargetRequestSelection& context, u32 direction)
{
    auto& selection = context.selection;
    if (!runtime_body_accepts_target_request(selection.requests.body, context.flags,
        player_record(context.owner).owner_class))
    {
        return 1;
    }
    const u8* row = select_runtime_body_special_target_request(selection, direction);
    if (row != nullptr)
    {
        set_runtime_mind_target(context.events, selection.target.address);
        mind_record(context.events.mind).action_record = selection.target.address;
        accept_runtime_attachment_request(selection.requests, row, 0);
    }
    return row != nullptr ? 2 : 0;
}

// Xbox 0x0001C140
const u8* find_runtime_attachment_target_request(RuntimeAttachmentTargetSelection& context,
    const u8* key, const u8* shape, u32 direction, u8 height_mask, u32& optional)
{
    const u16 wanted = read_u16({key, 2}, 0);
    const auto& rows = context.requests.tables->first;
    for (u32 index = 0; index < rows.count; ++index)
    {
        const u8* row = rows.rows + index * 0x24;
        if (read_u16({row, 0x24}, 0) == wanted
            && runtime_attachment_target_row_matches(context.requests.body, context.actor,
                {row, 0x24}, context.target.bytes, shape, direction, height_mask))
        {
            if ((row[0x20] & 0x10) != 0)
            {
                optional = 1;
            }
            return row;
        }
    }
    return nullptr;
}

// Xbox 0x0001CF70
const u8* select_runtime_body_second_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction)
{
    if (context.requests.tables == nullptr)
    {
        return nullptr;
    }
    if (direction == 0xFFFFFFFFu)
    {
        direction = runtime_body_target_direction(context.body.body_actor,
            context.target.bytes, context.body.body_actor.euler.y);
    }
    std::array<u8, 32> shape;
    const u32 height = runtime_body_target_height(context.requests.body, context.body.body_actor,
        context.target.bytes, shape.data());
    return select_runtime_attachment_second_target_row(context, shape.data(), u8(direction),
        u8(1u << (height & 31)));
}

// Xbox 0x0001CFE0
const u8* select_runtime_body_pair_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction)
{
    if (context.requests.tables == nullptr || context.requests.tables->pair.rows == nullptr)
    {
        return nullptr;
    }
    if (direction == 0xFFFFFFFFu)
    {
        direction = runtime_body_target_direction(context.body.body_actor,
            context.target.bytes, context.body.body_actor.euler.y);
    }
    std::array<u8, 32> shape;
    const u32 height = runtime_body_target_height(context.requests.body, context.body.body_actor,
        context.target.bytes, shape.data());
    return select_runtime_attachment_pair_target_row(context, shape.data(), u8(direction),
        u8(1u << (height & 31)));
}

// Xbox 0x0001D060
const u8* select_runtime_body_category_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction)
{
    if (context.requests.tables == nullptr || context.requests.tables->category.rows == nullptr)
    {
        return nullptr;
    }
    if (direction == 0xFFFFFFFFu)
    {
        direction = runtime_body_target_direction(context.body.body_actor,
            context.target.bytes, context.body.body_actor.euler.y);
    }
    std::array<u8, 32> shape;
    const u32 height = runtime_body_target_height(context.requests.body, context.body.body_actor,
        context.target.bytes, shape.data());
    return select_runtime_attachment_category_target_row(context, shape.data(), u8(direction),
        u8(1u << (height & 31)));
}

// Xbox 0x0001D0E0
const u8* select_runtime_body_special_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction)
{
    if (context.requests.tables == nullptr || context.requests.tables->special.rows == nullptr)
    {
        return nullptr;
    }
    if (direction == 0xFFFFFFFFu)
    {
        direction = runtime_body_target_direction(context.body.body_actor,
            context.target.bytes, context.body.body_actor.euler.y);
    }
    return select_runtime_attachment_special_target_row(context, u8(direction), 0xFF);
}

// Xbox 0x00066180
i32 count_runtime_request_bits(const RuntimeRequestBits& bits)
{
    u32 count = 0;
    for (u8 byte : bits.bytes)
    {
        u32 value = byte;
        u32 local = 0;
        while (value != 0)
        {
            ++local;
            value &= value - 1;
        }
        count += local;
    }
    return std::bit_cast<i32>(count);
}

// Xbox 0x000661C0
bool select_random_runtime_request_bit(const RuntimeRequestBits& bits, u32& index, u32& random)
{
    const i32 count = count_runtime_request_bits(bits);
    if (count == 0)
    {
        return false;
    }
    random = random * 0x19660Du + 0x3C6EF35Fu;
    const float fraction = std::bit_cast<float>((random & 0x7FFFFFu) | 0x3F800000u);
    u32 ordinal = u32(truncate_runtime_float((static_cast<long double>(fraction) - 1.L) * count));
    for (u32 candidate = 0; candidate < bits.count; ++candidate)
    {
        if ((bits.bytes[candidate >> 3] & (1u << (candidate & 7))) != 0)
        {
            if (ordinal == 0)
            {
                index = candidate;
                return true;
            }
            --ordinal;
        }
    }
    return true;
}

// Xbox 0x0001BBB0
const u8* select_runtime_attachment_second_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask)
{
    const auto* tables = context.requests.tables;
    if (tables == nullptr)
    {
        return nullptr;
    }
    const auto& rows = tables->second;
    if (rows.rows != nullptr)
    {
        std::memset(context.bits.bytes.data(), 0, context.bits.bytes.size());
        u32 close = 0, optional = 0, accepted = 0;
        std::array<u8, 32> point_shape;
        if ((context.actor.mask_mode == 2 || context.actor.mask_flag != 0)
            && runtime_mind_target_uses_point_shape(context.target, context.model_state))
        {
            const Vec3 position = actor_world_position(context.actor);
            copy_actor_shape(point_shape.data(), shape);
            std::memcpy(point_shape.data() + 8, &position.y, 4);
            shape = point_shape.data();
            height_mask = 1;
        }
        u32 index = 0;
        for (; index < rows.count; ++index)
        {
            const std::span<const u8> row{rows.rows + index * 0x24, 0x24};
            if (runtime_attachment_target_row_matches(context.requests.body, context.body.body_actor,
                row, context.target.bytes, shape, direction, height_mask))
            {
                if ((read_u32(row, 0x20) & 4) != 0)
                {
                    ++close;
                }
                if ((read_u32(row, 0x20) & 0x10) != 0)
                {
                    ++optional;
                }
                ++accepted;
                context.bits.bytes[index >> 3] |= u8(1u << (index & 7));
            }
        }
        if (close != 0 && i32(accepted) > i32(close))
        {
            const float distance = runtime_body_target_surface_distance(context.body.body_actor,
                context.body, context.target.bytes);
            const float radius = runtime_player_body_attachment_radius(context.body);
            if (distance > radius * 3.f)
            {
                index = 0;
                for (; index < rows.count; ++index)
                {
                    if ((rows.rows[index * 0x24 + 0x20] & 4) == 0)
                    {
                        context.bits.bytes[index >> 3] &= u8(~(1u << (index & 7)));
                    }
                }
            }
        }
        if (optional != 0 && i32(accepted) > i32(optional))
        {
            index = 0;
            for (; index < rows.count; ++index)
            {
                if ((rows.rows[index * 0x24 + 0x20] & 0x10) != 0)
                {
                    context.bits.bytes[index >> 3] &= u8(~(1u << (index & 7)));
                }
            }
        }
        if (select_random_runtime_request_bit(context.bits, index, context.random))
        {
            return rows.rows + index * 0x24;
        }
    }
    return nullptr;
}

// Xbox 0x0001C9A0
const u8* select_runtime_attachment_pair_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask)
{
    const auto* tables = context.requests.tables;
    const auto* attached = context.requests.actions.attached;
    const u32 classification = attached == nullptr ? 0
        : classify_runtime_attached_actor(attached->bytes, context.requests.owner_class);
    const u16 category = attached == nullptr ? 0 : weapon_base(attached->bytes).unk_93D;
    std::memset(context.bits.bytes.data(), 0, context.bits.bytes.size());
    const auto& rows = tables->pair;
    u32 optional = 0, accepted = 0;
    for (u32 index = 0; index < rows.count; ++index)
    {
        const std::span<const u8> row{rows.rows + index * 0x28, 0x28};
        if (read_u16(row, 0x24) == classification && read_u16(row, 0x26) == category
            && runtime_attachment_target_row_matches(context.requests.body, context.body.body_actor,
                row, context.target.bytes, shape, direction, height_mask))
        {
            if ((row[0x20] & 0x10) != 0)
            {
                ++optional;
            }
            ++accepted;
            context.bits.bytes[index >> 3] |= u8(1u << (index & 7));
        }
    }
    if (optional != 0 && i32(accepted) > i32(optional))
    {
        for (u32 index = 0; index < rows.count; ++index)
        {
            if ((rows.rows[index * 0x28 + 0x20] & 0x10) != 0)
            {
                context.bits.bytes[index >> 3] &= u8(~(1u << (index & 7)));
            }
        }
    }
    u32 index = context.requests.actions.body_address;
    if (!select_random_runtime_request_bit(context.bits, index, context.random))
    {
        return nullptr;
    }
    return rows.rows + index * 0x28;
}

// Xbox 0x0001C5E0
const u8* select_runtime_attachment_category_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask)
{
    std::memset(context.bits.bytes.data(), 0, context.bits.bytes.size());
    const auto& rows = context.requests.tables->category;
    u32 index = 0;
    for (; index < rows.count; ++index)
    {
        const auto* attached = context.requests.secondary;
        if (attached == nullptr)
        {
            attached = context.requests.actions.attached;
        }
        u32 classification = 0;
        if (attached != nullptr)
        {
            const i16 count = i16(weapon_base(attached->bytes).accepted_class_count);
            i32 entry = 0;
            for (; entry < count; ++entry)
            {
                const size_t offset = 0x948 + size_t(entry) * 16;
                if (read_u16(attached->bytes, offset) == context.requests.owner_class)
                {
                    classification = read_u16(attached->bytes, offset + 4);
                    break;
                }
            }
            if (entry >= count && count != 0)
            {
                classification = read_u16(attached->bytes, offsetof(WeaponBase, accepted_classes) + 4);
            }
        }
        const u32 category = attached == nullptr ? 0 : weapon_base(attached->bytes).unk_93D;
        const std::span<const u8> row{rows.rows + index * 0x2C, 0x2C};
        if ((read_u32(row, 0x24) == 0 || read_u32(row, 0x24) == classification)
            && read_u32(row, 0x28) == category
            && runtime_attachment_target_row_matches(context.requests.body, context.body.body_actor,
                row, context.target.bytes, shape, direction, height_mask))
        {
            context.bits.bytes[index >> 3] |= u8(1u << (index & 7));
        }
    }
    if (!select_random_runtime_request_bit(context.bits, index, context.random))
    {
        return nullptr;
    }
    return rows.rows + index * 0x2C;
}

// Xbox 0x0001C860
const u8* select_runtime_attachment_special_target_row(RuntimeAttachmentTargetSelection& context,
    u32 direction, u8 height_mask)
{
    if (weapon_base(context.requests.actions.attached->bytes).mask != 0)
    {
        std::memset(context.bits.bytes.data(), 0, context.bits.bytes.size());
        const auto& rows = context.requests.tables->special;
        for (u32 index = 0; index < rows.count; ++index)
        {
            const std::span<const u8> row{rows.rows + index * 0x24, 0x24};
            if (runtime_attachment_target_row_matches(context.requests.body, context.body.body_actor,
                row, context.target.bytes, nullptr, direction, height_mask))
            {
                context.bits.bytes[index >> 3] |= u8(1u << (index & 7));
            }
        }
        u32 index = context.tables_address;
        if (select_random_runtime_request_bit(context.bits, index, context.random))
        {
            return rows.rows + index * 0x24;
        }
    }
    return nullptr;
}

// Xbox 0x0001BA20
bool runtime_attachment_target_row_matches(std::span<const u8> body, const Actor& actor,
    std::span<const u8> row, std::span<const u8> target,
    const u8* shape, u32 direction, u8 height_mask)
{
    if (direction != 0xFFFFFFFFu && row[6] != direction)
    {
        return false;
    }
    if ((row[7] & height_mask) == 0)
    {
        return false;
    }
    if (row[8] != 0 && body_record(body).unk_2D0 != 0 && body_record(body).unk_2D0 == row[8]
        && body_record(body).unk_2D2 != 0)
    {
        return false;
    }
    if ((target[0x16] & 2) != 0 && (row[0x20] & 2) != 0)
    {
        return false;
    }
    if (read_f32(row, 0xC) != 0.f)
    {
        const Vec3 position = actor_world_position(actor);
        const float radius = actor.radius_xz * read_f32(row, 0xC) + mind_target_record(target).shape.radius;
        const float dz = mind_target_record(target).shape.position.z - position.z;
        const float dx = mind_target_record(target).shape.position.x - position.x;
        if (dz * dz + dx * dx > radius * radius)
        {
            return false;
        }
    }
    if (shape != nullptr && (read_u16(row, 0x10) != 0 || read_u16(row, 0x12) != 0))
    {
        const Vec3 position = actor_world_position(actor);
        const std::span<const u8> shape_bytes{shape, 0x20};
        const float top = float(i16(read_u16(row, 0x12))) * actor.radius_xz + position.y;
        const float shape_bottom = read_f32(shape_bytes, 8);
        if (shape_bottom > top)
        {
            return false;
        }
        const float shape_top = read_f32(shape_bytes, 0x14) + shape_bottom;
        const float bottom = float(i16(read_u16(row, 0x10))) * actor.radius_xz + position.y;
        if (bottom > shape_top)
        {
            return false;
        }
    }
    return true;
}

// Xbox 0x0001BE20
const u8* find_runtime_attachment_chain(const RuntimeAttachmentChainRows& chains, u32& length)
{
    while (i32(length) > 0)
    {
        const u8* row = chains.rows;
        if (row != nullptr)
        {
            for (u32 index = 0; index < chains.count; ++index)
            {
                if (u32(read_u16({row, 8}, 0)) == length)
                {
                    return row;
                }
                row += 8;
            }
        }
        --length;
    }
    return nullptr;
}

// Xbox 0x00024600
u8 runtime_body_model_target_flag(const u8* state)
{
    if (state != nullptr)
    {
        return state[0x1A1];
    }
    return 0;
}

// Xbox 0x001415D0
bool runtime_mind_target_uses_point_shape(const RuntimeMindTarget& target, const u8* model_state)
{
    if (mind_target_record(target.bytes).kind == 0)
    {
        const auto* flags = target.body.bits->bytes;
        if ((flags[4] & 0x10) != 0)
        {
            return true;
        }
        if (player_record(target.actor_bytes).owner_class == 0 && (flags[2] & 2) != 0)
        {
            if ((flags[5] & 2) == 0)
            {
                return true;
            }
            if (runtime_body_model_target_flag(model_state) != 0)
            {
                const Vec3 position = actor_world_position(target.actor);
                if (mind_target_record(target.bytes).shape.position.y > mind_target_record(target.bytes).shape.height * .25f + position.y)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

// Xbox 0x0013A5B0
float runtime_body_target_surface_distance(const Actor& actor,
    const RuntimePlayerAttachmentEligibility& body, std::span<const u8> target)
{
    const Vec3 position = actor_world_position(actor);
    const long double dx = static_cast<long double>(position.x) - mind_target_record(target).shape.position.x;
    const long double dy = static_cast<long double>(position.y) - mind_target_record(target).shape.position.y;
    const long double dz = static_cast<long double>(position.z) - mind_target_record(target).shape.position.z;
    const long double distance = std::sqrt((dz * dz + dx * dx) + dy * dy);
    const long double reach = static_cast<long double>(body.body_actor.radius_xz)
        * body_record(body.body).radius_scale + mind_target_record(target).shape.radius;
    const long double gap = distance - reach;
    if (0.L >= gap)
    {
        return 0.f;
    }
    return float(gap);
}
}
