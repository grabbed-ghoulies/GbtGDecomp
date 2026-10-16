#include "scene_body_messages.h"
#include "../entities/body_move_action_slots.h"
#include "../entities/body_gravity_motion.h"
#include "../entities/body_stand_alert_tick.h"
#include "../entities/body_walk_alert_tick.h"
#include "../entities/body_action_ids.h"
#include <bit>
#include <cstring>
#include <optional>

namespace gbtg
{
namespace
{
[[noreturn]] void unbound(const std::string& what)
{
    throw Error("scene body message service is not bound: " + what);
}
}

SceneBodyMessages::SceneBodyMessages(SceneBodyServices& services, NativePlayerInstance& player)
    : services_(services),
      player_(player),
      event_record_(player.actor_instance.startup.resource.resource.data()),
      body_view_(player.body),
      actions_{player.attachment, player.body_selection,
          [&player](std::span<const u32> message)
          {
              player.self.message(message);
          },
          event_record_, player.callout},
      // Actor +0x6CC is sound slot 5 (slots of 0xB8 from +0x334).
      sound_commands_{player.actor, player.actor, player.body, services.damage_tables, services.effect_tags,
          services.footsteps, player.actor.sound_slots[5], services.sounds},
      projectiles_{player.actor, player.self, services.resources, services.factory, services.default_position,
          services.default_euler, created_},
      effect_commands_{player.actor, i32(read_u32(player.actor_bytes, 0xCEC)), services.effect_scene,
          services.surfaces, services.effect_tags, services.effect_suffixes[0], services.effect_suffixes[1],
          services.effect_suffixes[2], services.effect_suffixes[3], services.effects},
      query_name_([](RuntimeBodyAttachedActor&, std::span<char, 128>)
          {
              unbound("attached actor name query (actor message 0x99)");
          }),
      animation_{player.actor, *player.body_flags, player.collision, services.latches, nullptr, nullptr,
          sound_commands_, player.attachment_creation, projectiles_, services.objects, command_name_, query_name_,
          effect_commands_, effect_flags_, player.actor_instance.animation, nullptr, nullptr, services.callouts,
          services.flags, proximity_, player.scene.address_of(player.body.data()), player.body.data() + 0x2D8,
          player.callout, player.body_lists},
      spatial_{player.actor, player.actor_address, u8(read_u32(player.actor_bytes, 0xCEC)),
          player.actor_instance.live_attachments, services.global_attachments},
      parent_{player.actor, player.actor_bytes, actions_, children_, animation_, spatial_, shape_scratch_,
          services.latches, player.collision, first_timer_, second_timer_, services.contact_players,
          services.default_position},
      death_notice_{services.game, body_view_, player.actor_address,
          reinterpret_cast<const char*>(player.actor_bytes.data() + 0xBA0), player.spawn.record, services.resources},
      death_drops_{player.actor, player.actor_bytes, services.resources, services.factory, services.objects, created_,
          services.default_position, services.default_euler, services.random, services.occupancy_initial},
      death_{death_notice_, death_drops_, player.body_attributes,
          *reinterpret_cast<u32*>(player.actor_bytes.data() + 0xCE0), services.story_flags, services.empty_name,
          services.empty_name, player.attachment},
      processor_{player.actor, player.actor_address, *player.body_flags, player.body_attributes, nullptr, nullptr,
          services.damage_tables, services.push_keys, services.amount_keys, services.player_damage, 0, services.player_blocked,
          services.player_modes, services.player_mode, actions_,
          []() -> u16
          {
              unbound("body defensive action (vtable +0x44)");
          },
          services.death_actions, damage_rows_, std::span<char>(), death_, services.random, parameter_scratch_,
          []() -> u16
          {
              unbound("body death action (vtable +0x4C)");
          },
          first_timer_},
      timer_creation_{first_timer_, timer_object_, player.child, timers_},
      damage_{services.game, processor_, nullptr, timer_creation_},
      messages_{parent_, damage_, player.ring, services.address_zero, mask_,
          *reinterpret_cast<const i32*>(player.actor_bytes.data() + 0xCEC), player.actor_instance.render,
          services.clock, services.tag_rows, services.tag_names, services.shock_pass, services.shock_fail,
          player.body_attributes, hit_view_, services.scene_flags, services.restart_requested},
      controls_{player.actor, player.actor_bytes[0x197], player.actor_instance.anchors, live_attachment_,
          player.actor_instance.startup.resource, player.callout, player.body_lists, *player.body_flags,
          player.scene.address_of(player.body.data())}
{
    // The state and move actions installed so far (0x5B, 0x5C, 0x87) all have the empty 0x001628B0 at +0xC.
    empty_child_.message = [](std::span<u8>)
    {
    };
    walk_alert_child_.message = [](std::span<u8> message)
    {
        message_runtime_walk_alert(message, []()
        {
            unbound("walk-alert stop 0x00023890 (message 0x53)");
        });
    };
}

void SceneBodyMessages::dispatch(FactoryObject& body, std::span<u32> message)
{
    auto& actions = player_.body_selection;
    for (FactoryObject* child : {&actions.first, &actions.second})
    {
        if (child->bytes == nullptr)
        {
            continue;
        }
        if (child->type == 0x8E)
        {
            children_[child->bytes.get()] = &walk_alert_child_;
            continue;
        }
        if (child->type == 0x5D)
        {
            auto& stand = stand_children_[child->bytes.get()];
            if (stand == nullptr)
            {
                stand = std::make_unique<RuntimeBodyMessageChild>();
                stand->message = [this, child](std::span<u8> message)
                {
                    auto context = stand_context();
                    message_runtime_stand_action({child->bytes.get(), child->size}, message, context);
                };
            }
            children_[child->bytes.get()] = stand.get();
            continue;
        }
        if (child->type == 0x73)
        {
            auto& hit = hit_children_[child->bytes.get()];
            if (hit == nullptr)
            {
                hit = std::make_unique<RuntimeBodyMessageChild>();
                hit->message = [this, child](std::span<u8> message)
                {
                    const u32 id = read_u32(message, 0);
                    // Ids 0x41 and 0x5A: EAX of 0x000961C0 is the contact pool's +4 (its first row), read from
                    // message +0x18 (0x41) or +0x1C (0x5A).
                    const Vec3* contact = nullptr;
                    if (id == 0x41 || id == 0x5A)
                    {
                        const u32 pool = read_u32(message, id == 0x41 ? 0x18 : 0x1C);
                        if (pool == 0)
                        {
                            if (read_u32(services_.address_zero, 4) != 0)
                            {
                                unbound("hit state contact pool at address zero");
                            }
                        }
                        else
                        {
                            const auto* contacts = reinterpret_cast<const GhoulySpawnPool*>(
                                services_.event_at(pool).data());
                            contact = reinterpret_cast<const Vec3*>(contacts->begin);
                        }
                    }
                    RuntimeHitStateMessages context{player_.body, read_u32(player_.body, 8), mind_target(),
                        id == 7 ? services_.event_at(read_u32(message, 0xC)) : std::span<const u8>(), contact,
                        read_u32(player_.body, 0x48), services_.actor_callouts};
                    message_runtime_hit_state({child->bytes.get(), child->size}, message, context);
                };
            }
            children_[child->bytes.get()] = hit.get();
            continue;
        }
        // 0x74 has the empty 0x001628B0 message slot.
        if (child->type != 0x5B && child->type != 0x5C && child->type != 0x74 && child->type != 0x87)
        {
            unbound("body action message slot of factory type " + std::to_string(child->type));
        }
        children_[child->bytes.get()] = &empty_child_;
    }
    body_view_ = player_.body;
    mask_ = EntityMaskHost{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
    processor_.attributes = player_.body_attributes; // Body +0x268.
    refresh_damage_rows();
    const u32 request = read_u32(player_.body, 0x84);
    hit_view_ = request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24);
    const u32 id = message[0];
    // Ids whose handlers read the animation, source or contact views built from message pointers.
    switch (id)
    {
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1B:
    case 0x1D:
    case 0x2A:
    case 0x45:
    case 0x63:
        unbound("body message views for id " + std::to_string(id));
    default:
        break;
    }
    std::span<u8> bytes{reinterpret_cast<u8*>(message.data()), message.size() * 4};
    RuntimePlayerBodyMessageViews views{{{}, nullptr, nullptr, nullptr}, nullptr, nullptr, {}, {}};
    std::optional<entity_detail::SegmentPool> contact_pool;
    std::optional<RuntimeBodySourceContact> source_contact;
    std::array<u8, 0x2C> target_bound;
    std::optional<RuntimeSegmentContactActor> contact;
    std::array<u8, 0x2C> bound;
    if (id == 7)
    {
        views.parent.animation_command = services_.event_at(message[3]);
    }
    if (id == 0xF)
    {
        views.source = &services_.source_at(message[1]);
        views.source_body = services_.source_body_at(message[1]);
        // 0x00022A40 reads target actor +0x8F8 +0x20..+0x28 only when bound exists.
        if (live_attachment_ != nullptr)
        {
            std::memcpy(bound.data() + 0x20, live_attachment_->shape.data() + 4, 12);
            views.bound = bound;
        }
    }
    if (id == 0x15)
    {
        // {0x15, reply, sender, velocity, contacts, point} from 0x0001F8C0; the contacts are a copied segment pool.
        const auto& pool = *reinterpret_cast<const GhoulySpawnPool*>(services_.event_at(message[6]).data());
        contact_pool.emplace(entity_detail::SegmentPool{pool.stride, pool.grow, pool.begin, pool.cursor, pool.limit});
        auto& source = services_.source_at(message[2]);
        const auto source_body = services_.source_body_at(message[2]);
        const u32 request = read_u32(source_body, 0x84);
        RuntimeBodyAttachedActor* retained = nullptr;
        if (read_u32(player_.body, 0x278) == 0 && read_u32(player_.body, 0x270) != 0)
        {
            retained = &services_.source_at(read_u32(player_.body, 0x270));
        }
        std::span<const u8> bound_view;
        if (live_attachment_ != nullptr)
        {
            std::memcpy(target_bound.data() + 0x20, live_attachment_->shape.data() + 4, 12);
            bound_view = target_bound;
        }
        source_contact.emplace(RuntimeBodySourceContact{
            RuntimeBodyContactHitViews{*contact_pool, message[6], source, retained},
            {const_cast<u8*>(source_body.data()), source_body.size()},
            request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24), bound_view, player_.ring,
            services_.address_zero, player_.self, damage_, services_.damage_at(message[2])});
        views.source_contact = &*source_contact;
    }
    if (id == 6 || id == 0x7B)
    {
        const u32 address = message[1];
        contact.emplace(RuntimeSegmentContactActor{*services_.actor_at(address),
            read_u32(services_.bytes_at(address), 0x908), services_.body_node(address),
            [this, address](void* words)
            {
                services_.message(address, {static_cast<const u32*>(words), 0x42});
            }});
        views.parent.contact = &*contact;
    }
    if (id == 0x60)
    {
        // {0x60, pushing actor, push} from 0x00022400.
        views.source = &services_.source_at(message[1]);
    }
    if (id == 0x47 || id == 0x7C)
    {
        views.parent.referenced_actor = services_.actor_at(message[1]);
    }
    auto body_bytes = player_.body;
    switch (body.type)
    {
    case 0x27:
        dispatch_runtime_player_body_message(body_bytes, bytes, views, messages_); // Vtable 0x00504FE0 +0xC.
        break;
    case 0x2A:
        dispatch_runtime_player_body(body_bytes, bytes, views, messages_, controls_); // Vtable 0x00505248 +0xC.
        break;
    case 0x28:
        // Vtable 0x00504F80 +0xC, 0x00073320: every id but 0x63 goes to the parent body handler.
        dispatch_runtime_parent_body_message(body_bytes, bytes, views.parent, parent_);
        break;
    default:
        unbound("body message slot of factory type " + std::to_string(body.type));
    }
}

// Body action children of the classes installed so far: 0x5C (vtable 0x00505500), 0x5B (0x00505518),
// 0x73 (0x00505940), 0x74 (0x00505960), 0x87 (0x00505BC0) and 0x8E (0x00505CA0).
void SceneBodyMessages::action_tick(FactoryObject& action, u32 input)
{
    switch (action.type)
    {
    case 0x5C:
        tick_runtime_velocity_action({action.bytes.get(), action.size}, player_.body, player_.actor,
            services_.clock, services_.motion_steps);
        return;
    case 0x5B:
        tick_runtime_gravity_action({action.bytes.get(), action.size}, player_.body, player_.actor,
            services_.clock, services_.motion_steps);
        return;
    case 0x87:
    {
        RuntimeStandAlertTick tick{player_.body, player_.actor, *player_.body_bits,
            player_.actor_instance.animation, player_.body_selection, services_.clock, player_.input_device,
            player_.scene.input_state,
            !player_.mind.empty(), player_.mind.empty() ? u8(0) : player_.mind[0x20],
            services_.stand_input_threshold,
            [this]() -> u16
            {
                const RuntimeBodyActionIds ids{player_.attached, read_u16(player_.actor_bytes, 6),
                    services_.address_of(player_.body.data())};
                return runtime_body_damage_gated_id(ids, player_.action_table);
            },
            []()
            {
                unbound("stand-alert orientation 0x00134720");
            }};
        tick_runtime_stand_alert({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x5D:
    {
        auto context = stand_context();
        tick_runtime_stand_action({action.bytes.get(), action.size}, context);
        return;
    }
    case 0x8E:
    {
        // ECX of 0x000A5A60 is the tick's input word: the player's input record (actor +0x960) or zero.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("walk-alert input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        RuntimeWalkAlertTick tick{player_.body, player_.actor, *player_.body_bits, player_.body_selection,
            services_.clock, device, player_.scene.input_state, services_.damage_tables, services_.mode_scales,
            !player_.mind.empty(), player_.mind.empty() ? u8(0) : player_.mind[0x20],
            []()
            {
                unbound("walk-alert orientation 0x00134720");
            }};
        tick_runtime_walk_alert({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x73:
    {
        // The tick's second argument is the player's input record (actor +0x960) or zero, as for 0x8E.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("hit state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        const RuntimeDialogueInputDevice* own = nullptr;
        if (read_u32(player_.actor_bytes, 0x960) != 0)
        {
            own = &player_.input_device;
        }
        const u32 hit_address = read_u32(player_.body, 0x84);
        const u32 attached = read_u32(player_.body, 0x48);
        if (attached != 0)
        {
            unbound("hit state attached actor +0x998 collision view");
        }
        ActorTrailContext trails{services_.factory, services_.objects, player_.actor_address, services_.pause,
            services_.frame_delta, services_.time_scale, services_.pal, services_.pal_override,
            services_.empty_name};
        RuntimeHitStateTick tick{player_.body, player_.actor, player_.actor_bytes, player_.body_flags,
            player_.actor_instance.animation, services_.clock, services_.motion_steps, services_.default_position,
            mind_target(), hit_address == 0 ? std::span<const u8>() : services_.record_at(hit_address, 0x24),
            player_.body_attributes, player_.collision, nullptr, player_.actor_instance.render, trails, input, device,
            own,
            player_.scene.input_state, services_.address_zero, services_.damage_tables, services_.mode_scales,
            player_.action_table, first_timer_,
            [this](std::span<const u32> message)
            {
                // The receivers read a full message record; words past the retail stack slots stay zero.
                std::array<u32, 0x42> buffer{};
                std::copy_n(message.begin(), std::min<size_t>(message.size(), buffer.size()), buffer.begin());
                player_.self.message(buffer);
            },
            [this]()
            {
                return body_slot_key(0x20);
            },
            [this]()
            {
                return body_slot_key(0x2C);
            }};
        tick_runtime_hit_state({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x74:
    {
        // The tick's second argument is the player's input record (actor +0x960) or zero, as for 0x8E.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("furnace hit state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        RuntimeBodyRecoveryAction recovery{player_.body, player_.body_attributes, player_.action_table, first_timer_,
            player_.ring, read_u16(player_.actor_bytes, 0x990), services_.address_zero,
            [](u32&)
            {
                unbound("recovery goal action 0x00028360 / 0x00135790");
            },
            [](u32&)
            {
                unbound("recovery goal action 0x00135540");
            },
            [this]()
            {
                return body_slot_key(0x20);
            },
            [this]()
            {
                return body_slot_key(0x30);
            },
            [this]()
            {
                return body_slot_key(0x34);
            }};
        RuntimeHitFurnaceTick tick{player_.body, player_.actor, player_.actor_bytes,
            player_.actor_instance.animation, services_.clock, player_.body_attributes, device,
            player_.scene.input_state, services_.address_zero,
            []()
            {
                unbound("furnace hit scorch 0x000965C0");
            },
            recovery};
        tick_runtime_hit_furnace_state({action.bytes.get(), action.size}, tick);
        return;
    }
    default:
        unbound("body action vtable +0x10 of factory type " + std::to_string(action.type));
    }
}

void SceneBodyMessages::action_step(FactoryObject& action)
{
    switch (action.type)
    {
    case 0x5C:
        // 0x000803A0 jumps to 0x0002AD30 with the action; its +8 is this body.
        step_runtime_body_motion(player_.body, player_.actor, player_.actor_instance.animation, services_.clock);
        return;
    case 0x5B:
        // Xbox 0x000804E0: action +8 is the body, limits backwards motion and keeps the root direction.
        step_runtime_gravity_motion(player_.body, player_.actor_bytes, player_.actor,
            player_.actor_instance.animation, services_.clock, *player_.body_bits, services_.damage_tables,
            services_.movement_phases, services_.movement_ratio_scale, services_.backward_motion_lower,
            services_.reverse_turn_threshold, 1, 0);
        return;
    case 0x5D:
    {
        auto context = stand_context();
        step_runtime_stand_action({action.bytes.get(), action.size}, context, player_.actor_instance.animation);
        return;
    }
    case 0x73:
    case 0x74:
    case 0x87:
    case 0x8E:
        // 0x001628B0 returns at once.
        return;
    default:
        unbound("body action vtable +0x14 of factory type " + std::to_string(action.type));
    }
}

RuntimeBodyDamageEntry& SceneBodyMessages::damage_entry()
{
    body_view_ = player_.body;
    mask_ = EntityMaskHost{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
    processor_.attributes = player_.body_attributes; // Body +0x268.
    refresh_damage_rows();
    const u32 request = read_u32(player_.body, 0x84);
    hit_view_ = request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24);
    return damage_;
}

// 0x00024970 passes body +0x268 to 0x00020C80 as the direction rows (ECX): +0 the count, +4 the address of
// {angle in degrees, name address} rows. 0x00020B50 reads row 0's name even when the count is zero. The names are
// printed into DAT_00508EB8.
void SceneBodyMessages::refresh_damage_rows()
{
    auto& scratch = services_.name_scratch;
    if (scratch.size() < 0x800)
    {
        scratch.resize(0x800);
    }
    processor_.animation_name = std::span<char>(scratch.data(), scratch.size());
    damage_row_storage_.clear();
    const auto attributes = player_.body_attributes;
    if (attributes.data() == nullptr)
    {
        damage_rows_ = RuntimeDamageAnimationRows{0, {}};
        return;
    }
    const i32 count = std::bit_cast<i32>(read_u32(attributes, 0));
    const u32 address = read_u32(attributes, 4);
    // Rows at address zero, or a name at address zero, stay unresolved; 0x00020B50 raises if it reads them.
    damage_rows_ = RuntimeDamageAnimationRows{count, {}};
    if (address == 0)
    {
        return;
    }
    const i32 read = count > 0 ? count : 1;
    const auto rows = services_.record_at(address, size_t(read) * 8);
    for (i32 index = 0; index < read; ++index)
    {
        const u32 name = read_u32(rows, size_t(index) * 8 + 4);
        if (name == 0)
        {
            damage_row_storage_.clear();
            return;
        }
        damage_row_storage_.push_back(RuntimeDamageAnimationRow{read_f32(rows, size_t(index) * 8),
            reinterpret_cast<const char*>(services_.record_at(name, 1).data())});
    }
    damage_rows_ = RuntimeDamageAnimationRows{count, damage_row_storage_};
}

std::span<const u8> SceneBodyMessages::mind_target()
{
    if (player_.mind.empty())
    {
        return {};
    }
    const u32 target = read_u32(player_.mind, 0x14);
    return target == 0 ? std::span<const u8>() : services_.target_at(target);
}

// Body vtable +0x20 and +0x2C: 0x00075B90 / 0x00075C30 for type 0x2A (getter 0x00075AF0), the parent
// 0x00026950 / 0x00026A90 for type 0x27 (getter 0x000733F0 keeps the parent's slots from +0x20). +0x30 and +0x34
// are the parent's 0x00026B00 / 0x00026BB0 (parent table 0x0001DA60) in both.
u16 SceneBodyMessages::body_slot_key(u32 slot)
{
    const u32 type = player_.body_object.type;
    if (type != 0x27 && type != 0x2A)
    {
        unbound("body next-action slot " + std::to_string(slot) + " of factory type " + std::to_string(type));
    }
    if (player_.body_bits == nullptr)
    {
        unbound("body next-action slot without the body +0x15C bit block");
    }
    const RuntimeBodyActionIds ids{player_.attached, read_u16(player_.actor_bytes, 6),
        services_.address_of(player_.body.data())};
    const u8* mind = player_.mind.empty() ? nullptr : player_.mind.data();
    // The caller's ECX: read only when the attached actor's class leaves the action pair unset.
    const u16 retained = 0;
    if (slot == 0x20)
    {
        if (type == 0x2A)
        {
            return runtime_player_body_primary_id(player_.body, ids, mind, *player_.body_bits, player_.action_table,
                player_.ring, services_.address_zero,
                [this, &ids]()
                {
                    return runtime_body_primary_gated_id(ids, player_.action_table); // Body vtable +0x24.
                },
                retained);
        }
        return runtime_body_primary_parameter_id(ids, mind, *player_.body_bits, player_.action_table, retained);
    }
    // Slots +0x30 and +0x34 are the parent's 0x00026B00 / 0x00026BB0 in both classes.
    if (slot == 0x30)
    {
        return u16(runtime_body_primary_action_id(ids));
    }
    if (slot == 0x34)
    {
        return u16(runtime_body_secondary_action_id(ids));
    }
    if (slot == 0x2C)
    {
        if (type == 0x2A)
        {
            return runtime_player_body_secondary_parameter_id(ids, mind, *player_.body_bits, player_.action_table,
                retained);
        }
        return runtime_body_secondary_parameter_id(ids, mind, *player_.body_bits, player_.action_table, retained);
    }
    unbound("body next-action slot " + std::to_string(slot));
}

RuntimeStandActionContext SceneBodyMessages::stand_context()
{
    return RuntimeStandActionContext{player_.body, player_.actor, player_.mind, services_.clock,
        services_.motion_steps, services_.stand_tunables,
        [this](u32 address) -> const Actor*
        {
            return services_.actor_at(address);
        },
        [this](Vec3& output, const Vec3& from, const Vec3& to, u32 target, float range, float near_distance,
            float far_distance)
        {
            aim_camera_offset(output, from, to, target, range, near_distance, far_distance, services_.camera_rig);
        }};
}

RuntimeBodyChannelContext SceneBodyMessages::channel_context()
{
    const std::span<const u8> body = player_.body;
    channel_rows_ = {reinterpret_cast<u8*>(player_.body_hashes),
        size_t(player_.body_hash_count) * sizeof(RuntimeBodyResourceHash)};
    channel_bindings_ = nullptr;
    const auto* controller = player_.actor_instance.animation.controller.get();
    if (controller != nullptr)
    {
        const size_t count = controller->slot_count;
        channel_assets_.resize(count);
        channel_resources_.resize(count);
        channel_animation_rows_.resize(count);
        channel_directories_.resize(count);
        for (size_t slot = 0; slot < count; ++slot)
        {
            const auto* animation = controller->slots.at(slot).player.get();
            channel_animation_rows_[slot].resource = nullptr;
            if (animation != nullptr)
            {
                channel_resources_[slot].asset = nullptr;
                channel_animation_rows_[slot].resource = &channel_resources_[slot];
                if (!animation->mina.empty())
                {
                    channel_assets_[slot].channels = nullptr;
                    channel_resources_[slot].asset = &channel_assets_[slot];
                    const u32 directory = read_u32(animation->mina, 0x20);
                    if (directory != 0)
                    {
                        for (size_t channel = 0; channel < 48; ++channel)
                        {
                            channel_directories_[slot][channel] = read_u32(animation->mina, directory + channel * 4);
                        }
                        channel_assets_[slot].channels = channel_directories_[slot].data();
                    }
                }
            }
        }
        channel_binding_view_.rows = channel_animation_rows_;
        channel_bindings_ = &channel_binding_view_;
    }
    channel_model_ = player_.actor_instance.startup.models.model.get();
    auto banks = player_.body_resources;
    for (size_t bank = 0; bank < banks.size(); ++bank)
    {
        banks[bank] = banks[bank].first(size_t(player_.body_resource_counts[bank]) * 8);
    }
    return RuntimeBodyChannelContext{player_.actor, services_.clock, channel_bindings_, channel_model_,
        channel_rows_, banks, services_.channel_keys, services_.address_zero};
}

// Xbox 0x00025EC0 for the camera body (type 0x28).
void SceneBodyMessages::tick(FactoryObject& body, u32 input)
{
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A)
    {
        unbound("body vtable +0x10 of factory type " + std::to_string(body.type));
    }
    auto& actions = player_.body_selection;
    if (body.type != 0x28)
    {
        RuntimePlayerBodyRelayChild action{[this](u32 input)
        {
            action_tick(player_.body_selection.second, input);
        }};
        RuntimePlayerBodyRelayChild attachment{[this](u32 input)
        {
            action_tick(player_.body_selection.first, input);
        }};
        RuntimePlayerBodyRelayChild* action_view = actions.second.bytes != nullptr ? &action : nullptr;
        RuntimePlayerBodyRelayChild* attachment_view = actions.first.bytes != nullptr ? &attachment : nullptr;
        RuntimePlayerJointTargets joints{player_.actor, player_.body_flags, player_.joint_records, services_.clock,
            services_.address_of(player_.body.data())};
        relay_runtime_player_body(player_.body, input, action_view, attachment_view, joints);
        return;
    }
    // Body +0x744, then +0x314.
    if (actions.second.bytes != nullptr)
    {
        action_tick(actions.second, input);
    }
    if (actions.first.bytes != nullptr)
    {
        action_tick(actions.first, input);
    }
}

// Xbox 0x00072AC0 for the camera body.
void SceneBodyMessages::animate(FactoryObject& body)
{
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A)
    {
        unbound("body vtable +0x14 of factory type " + std::to_string(body.type));
    }
    auto& actions = player_.body_selection;
    first_step_.step = [this]()
    {
        action_step(player_.body_selection.first);
    };
    RuntimeBodyChannelChild* attachment = actions.first.bytes != nullptr ? &first_step_ : nullptr;
    RuntimeBodyChannelContext channels = channel_context();
    step_runtime_body_channels(player_.body, attachment, channels);
}

// Xbox 0x00072B10 for the camera body.
void SceneBodyMessages::settle(FactoryObject& body)
{
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A)
    {
        unbound("body vtable +0x18 of factory type " + std::to_string(body.type));
    }
    const std::span<const u8> actor = player_.actor_bytes;
    if (read_u32(actor, 0x8F8) != 0)
    {
        unbound("actor +0x8F8 bound view for the body apply 0x00025AE0");
    }
    if (read_u32(actor, 0x8FC) != 0)
    {
        unbound("actor +0x8FC shape pool for the body apply 0x00025AE0");
    }
    bound_ = {};
    shapes_ = nullptr;
    actor_view_ = player_.actor_bytes;
    actor_bound_ = player_.attribute_resource.empty() ? nullptr : player_.attribute_resource.data();
    secondary_model_ = player_.actor_instance.startup.models.secondary.get();
    second_step_.step = [this]()
    {
        action_step(player_.body_selection.second);
    };
    RuntimeBodyChannelChild* action = player_.body_selection.second.bytes != nullptr ? &second_step_ : nullptr;
    RuntimeBodyChannelContext channels = channel_context();
    RuntimeBodyApplyContext apply{channels, actor_view_, player_.actor_instance.anchors, *player_.body_bits, bound_,
        shapes_, services_.latches, first_timer_, second_timer_, actor_bound_, secondary_model_,
        services_.node_states, action};
    if (body.type != 0x28)
    {
        apply_runtime_player_body(player_.body, apply);
        return;
    }
    RuntimeCameraBodySlots camera{player_.body, player_.actor, *player_.body_bits, services_.look_profile,
        services_.rig_tunables, player_.camera_view};
    capture_camera_body_pose(camera, apply);
}

// Xbox 0x00072CB0 for the camera body.
u32 SceneBodyMessages::supported(FactoryObject& body)
{
    if (body.type != 0x28)
    {
        if (body.type != 0x27 && body.type != 0x2A)
        {
            unbound("body vtable +0x1C of factory type " + std::to_string(body.type));
        }
        const EntityMaskHost mask{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
        const i32 floor = i32(read_u32(player_.actor_bytes, 0xCEC));
        RuntimePlayerCollisionAction action{[this](RuntimePlayerCollisionReply& reply)
        {
            const u32 type = player_.body_selection.second.type;
            if (type == 0x8E)
            {
                message_runtime_walk_alert({reinterpret_cast<const u8*>(&reply.id), 4}, []()
                {
                    unbound("walk-alert stop 0x00023890 (message 0x53)");
                });
                return;
            }
            if (type != 0x87 && type != 0x5B && type != 0x5C && type != 0x74)
            {
                unbound("collision reply action vtable +0xC of factory type " + std::to_string(type));
            }
            // These four action tables have the empty 0x001628B0 message slot.
        }};
        RuntimePlayerCollisionAction* action_view = player_.body_selection.second.bytes == nullptr ? nullptr : &action;
        RuntimePlayerBodyCollision collision{player_.body, player_.actor, player_.body_bits, mask, floor,
            player_.shapes, services_.shape_push, action_view, services_.door_rows(), services_.collision_contacts,
            services_.default_position, services_.clock};
        return update_runtime_player_body_collision(collision) ? 1 : 0;
    }
    RuntimeCameraBodySlots camera{player_.body, player_.actor, *player_.body_bits, services_.look_profile,
        services_.rig_tunables, player_.camera_view};
    return probe_camera_body_support(camera);
}
}
