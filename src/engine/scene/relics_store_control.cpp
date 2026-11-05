#include "relics_store_control.h"
#include "scene_controls.h"
#include "trophy_heads_control.h"
#include "scene_control_fields.h"
#include "../cutscenes/actor_tracks.h"
#include "../entities/actor_model_anchors.h"
#include "../entities/actor_transform.h"
#include "../frontend/dialogue_resources.h"
#include <bit>
#include <cstring>

namespace gbtg
{
namespace
{

constexpr u32 kPulse = 0x00081630; // The walk cutscene's +0x150 callback.

void put(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

float frame_step(const RelicsStoreControlServices& services)
{
    return services.freeze > 0.f ? 0.f : services.dt * services.scene_scale;
}

RuntimeCandleFlame candle_flame(RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    return RuntimeCandleFlame{control.flame_block, *control.candle, control.binding,
        control.light, control.flame, control.glow, services.elapsed(*control.candle), services.candle,
        services.registry, services.error, services.sounds};
}

// The marker id of the drawn row (DAT_00427600, the low word of each 4-byte row).
u16 marker_id(const RelicsStoreControlState& control, RelicsStoreControlServices& services)
{
    return read_u16(services.markers, size_t(control.marker) * 4);
}

// The control's hold on the scene's pause counters is given back.
void release_pause(RelicsStoreControlState& control, RelicsStoreControlServices& services)
{
    ++services.pause_count;
    ++services.transition_pause_count;
    control.pause_held = 0u;
}
}

// Xbox 0x00080FF0
void relics_store_control_object_sizes(ObjectSizes& output)
{
    output.instance = 0x5C;
    output.parameters = 8;
}

// Xbox 0x00081060
bool construct_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    set_runtime_relics_store_state(base, 1, control, services); // 0x00081330, ESI state
    control.pause_held = 0u;
    control.pulses = 0;
    return true;
}

// Xbox 0x00081150
void destroy_runtime_relics_store_control(RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    if (control.candle != nullptr)
    {
        RuntimeCandleFlame flame = candle_flame(control, services);
        put_out_runtime_candle(flame); // 0x000C7260, EDI +0x40
        control.candle = nullptr;
    }
    if (control.pause_held != 0)
    {
        release_pause(control, services);
    }
}

// Xbox 0x00081080
void tick_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    if ((services.runtime_flags & 0x400) == 0 && services.gameplay_tick != services.gameplay_mark)
    {
        services.gameplay_time += frame_step(services);
        services.gameplay_mark = services.gameplay_tick;
    }
    switch (control.phase)
    {
    case 1:
        set_runtime_relics_store_state(base, 2, control, services); // 0x00081330
        break;
    case 4:
        control.mummy = services.find_class(0x22); // 0x00129EA0, EDX 0x22
        if (control.mummy != 0)
        {
            set_runtime_relics_store_state(base, 3, control, services); // 0x00081330
            break;
        }
        [[fallthrough]];
    case 2:
    {
        RuntimeCandleFlame flame = candle_flame(control, services);
        update_runtime_candle_flame(1.f, 1, flame); // 0x000C7060, EDI +0x40, ECX +0x3C, XMM0 DAT_0032B0F8
        break;
    }
    default:
        break;
    }
    base.elapsed = base.elapsed + frame_step(services);
}

// Xbox 0x00081310
void draw_runtime_relics_store_control(RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    if (control.candle != nullptr)
    {
        RuntimeCandleFlame flame = candle_flame(control, services);
        queue_runtime_candle_flame(flame); // 0x000C6C80, EBX +0x3C, EDI +0x40
    }
}

// Xbox 0x00081630
void pulse_runtime_relics_store_control(SceneControlBase& base, RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    if (control.pulses == control.marker)
    {
        set_runtime_relics_store_state(base, 4, control, services); // 0x00081330
        return;
    }
    control.pulses = control.pulses + 1;
}

// Xbox 0x00081650, EAX actor
Vec3 runtime_relics_store_partner(u32 actor, RelicsStoreControlServices& services)
{
    const u16 tag = read_u16(services.bytes_at(actor), 0x1FE);
    const u32 partner = tag == 0 ? 0 : services.find_id(tag); // The DAT_00511AA4 walk on +0x1FC.
    if (partner == 0)
    {
        throw Error("relics store control: no marker partner for 0x000412A0");
    }
    return actor_world_position(*services.actor_at(partner)); // 0x000412A0, ESI the partner
}

// Xbox 0x000815A0, EAX id
void start_runtime_relics_store_attack(i32 id, u32 mummy, const Vec3& position, RelicsStoreControlServices& services)
{
    char first[0x80];
    char second[0x80];
    format_dialogue_resource_name(first, "%s_%d", "aid_cutscene_ghoulies_chapter1_scene10_attack1", id); // 0x00035830
    format_dialogue_resource_name(second, "%s_%d", "aid_cutscene_ghoulies_chapter1_scene10_attack2", id);
    std::array<u8, 0x1D8> parameters{};
    build_runtime_scene_challenge_parameters(parameters, first, mummy, 0.55f, 0.55f, &position, nullptr, 1, nullptr,
        1, 1, 0, second, services.runtime_flags); // 0x000873A0, DAT 0x3F0CCCCD
    services.create_object(parameters); // 0x00107520
}

// Xbox 0x00081330, ESI state
void set_runtime_relics_store_state(SceneControlBase& base, u8 next, RelicsStoreControlState& control,
    RelicsStoreControlServices& services)
{
    switch (next)
    {
    case 1:
        control.mummy = 0;
        break;
    case 2:
    {
        control.cutscene = start_scene_cutscene("aid_cutscene_ghoulies_chapter1_scene10_walk", 0, 1,
            services.cutscenes); // 0x0004EDA0
        if (services.pal != 0 && services.pal_override == 0)
        {
            control.cutscene->flags |= 0x40; // +0x168
        }
        --services.pause_count;
        --services.transition_pause_count;
        control.pause_held = 1u;
        control.cutscene->pending[0] = kPulse; // +0x150
        control.cutscene->pending[1] = services.address_of(&base); // +0x154
        auto& runtime = services.cutscenes.bindings.runtime;
        const auto& view = runtime.views.at(control.cutscene->resource);
        auto& rows = runtime.instances.at(control.cutscene);
        control.candle = find_cutscene_actor_by_model(*control.cutscene, view.tracks.data(), rows.actors.data(),
            "aid_model_ghoulies_cutscene_reliccandle"); // 0x0004EA50
        if (control.candle == nullptr)
        {
            throw Error("relics store control: the walk cutscene has no relic candle for 0x000408D0");
        }
        control.binding = find_actor_model_binding(*control.candle, 0); // 0x000408D0, EDI 0
        CandleFlameBlock& block = control.flame_block;
        block.lit = 0u;
        control.flame = {};
        control.glow = {};
        block.light = std::int8_t(-1);
        block.point_light = -1;
        RuntimeCandleFlame flame = candle_flame(control, services);
        light_runtime_candle(flame); // 0x000C6FB0, EAX +0x40, ECX +0x3C
        update_runtime_candle_flame(1.f, 1, flame); // 0x000C7060, EDI +0x40, XMM0 DAT_0032B0F8, EAX 1
        services.random = services.random * 0x19660Du + 0x3C6EF35Fu;
        const float roll = std::bit_cast<float>((services.random & 0x7FFFFFu) | 0x3F800000u) - 1.f; // DAT_0032B0F8
        control.marker = i32(roll * 6.f); // DAT_0032B218; CVTTSS2SI
        break;
    }
    case 3:
    {
        RuntimeCandleFlame flame = candle_flame(control, services);
        put_out_runtime_candle(flame); // 0x000C7260, EDI +0x40, stack +0x3C
        control.candle = nullptr;
        Vec3 position = services.view->position; // DAT_005077A8
        if (services.cutscenes.index > 0) // DAT_004E1C34
        {
            pop_scene_cutscene(services.cutscenes); // 0x0004EAD0
        }
        else
        {
            release_scene_cutscenes(services.cutscenes); // 0x0004ECE0
        }
        control.player = services.spawn_player("aid_objparams_ghoulies_weapon_candle", position); // 0x00134920
        const u16 id = marker_id(control, services);
        const u32 marker = services.find_id(id); // 0x00129ED0, DX the row's id
        position = runtime_relics_store_partner(marker, services); // 0x00081650
        start_runtime_relics_store_attack(i32(id), control.mummy, position, services); // 0x000815A0
        break;
    }
    case 4:
    {
        services.runtime_flags |= 8;
        std::array<u32, 0x42> message{};
        message[0] = 0xA8;
        const char* name = "aid_ghoulybox_ghoulies_chapter1_scene10_sssmummy";
        std::memcpy(message.data() + 1, name, std::strlen(name) + 1);
        services.message(services.find_id(marker_id(control, services)), message); // 0x00129ED0, vtable +0xC
        break;
    }
    default:
        break;
    }
    control.phase = u32(next);
}

// Xbox 0x000811A0
void message_runtime_relics_store_control(SceneControlBase& base, std::span<const u32> message, const void* reference,
    RelicsStoreControlState& control, SceneControlInstance& instance, SceneControlLists& lists,
    RelicsStoreControlServices& services)
{
    switch (message[0])
    {
    case 0x7D:
        set_runtime_relics_store_state(base, 2, control, services); // 0x00081330
        break;
    case 0x7F:
        if (reference == control.cutscene && control.pause_held != 0)
        {
            release_pause(control, services);
        }
        break;
    case 0x8E:
        if (message[1] == 1)
        {
            std::array<u32, 0x42> stop{9};
            services.message(control.mummy, stop); // Vtable +0xC.
            // {0xB, slot 0, the 0x000306E0 mix}: blend 0x23, no fade, the mummy's cheer and its events.
            std::array<u32, 0x51> cheer{};
            cheer[0] = 0xB;
            cheer[1] = 0;
            const std::span<u8> mix{reinterpret_cast<u8*>(cheer.data()) + 8, cheer.size() * 4 - 8};
            put(mix, 0x00, 0x4D697850u); // "PxiM".
            put(mix, 0x08, std::bit_cast<u32>(-10.f)); // DAT_0032B1FC.
            put(mix, 0x0C, 0x23u);
            put(mix, 0x10, 0u); // The 0.2 of 0x000306E0 (DAT_0032B1AC), overwritten.
            put(mix, 0x1C, 0x31u);
            put(mix, 0x20, std::bit_cast<u32>(0.2f)); // DAT_0032B1AC.
            const char* events = "aid_animevents_ghoulies_actor_mummy_cheer";
            const char* animation = "aid_anim_ghoulies_actor_mummy_cheer";
            std::memcpy(mix.data() + 0x24, events, std::strlen(events) + 1);
            std::memcpy(mix.data() + 0xB0, animation, std::strlen(animation) + 1);
            services.message(control.mummy, cheer); // Vtable +0xC.
        }
        else if (message[1] > 6 && message[1] < 9)
        {
            std::array<u32, 0x42> end{10};
            services.message(control.mummy, end); // Vtable +0xC.
            services.runtime_flags &= ~8u;
            dispose_runtime_scene_control(instance, lists); // 0x00116140, EAX the control
            const u32 notice = 0xB3;
            services.queue_event({reinterpret_cast<const u8*>(&notice), sizeof(notice)}); // 0x00116810, EDI the notice
        }
        break;
    default:
        break;
    }
    if (message[0] == 0x80)
    {
        retire_runtime_scene_control(instance, lists); // 0x00115E70
    }
}
}
