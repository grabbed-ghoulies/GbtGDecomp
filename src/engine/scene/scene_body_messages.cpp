#include "scene_body_messages.h"
#include "../entities/body_move_action_slots.h"
#include "../entities/body_stand_alert_tick.h"
#include "../entities/body_action_ids.h"
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
      processor_{player.actor, player.actor_address, *player.body_flags, player.attribute_resource, nullptr, nullptr,
          services.damage_tables, {}, services.amount_keys, services.player_damage, 0, services.player_blocked,
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
          player.body_attributes, player.body_resources[0], services.scene_flags, services.restart_requested},
      controls_{player.actor, player.actor_bytes[0x197], player.actor_instance.anchors, live_attachment_,
          player.actor_instance.startup.resource, player.callout, player.body_lists, *player.body_flags,
          player.scene.address_of(player.body.data())}
{
    // The state and move actions installed so far (0x5B, 0x5C, 0x87) all have the empty 0x001628B0 at +0xC.
    empty_child_.message = [](std::span<u8>)
    {
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
        if (child->type != 0x5B && child->type != 0x5C && child->type != 0x87)
        {
            unbound("body action message slot of factory type " + std::to_string(child->type));
        }
        children_[child->bytes.get()] = &empty_child_;
    }
    body_view_ = player_.body;
    mask_ = EntityMaskHost{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
    const u32 id = message[0];
    // Ids whose handlers read the animation, source or contact views built from message pointers.
    switch (id)
    {
    case 7:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x19:
    case 0x1B:
    case 0x1D:
    case 0x2A:
    case 0x45:
    case 0x60:
    case 0x63:
        unbound("body message views for id " + std::to_string(id));
    default:
        break;
    }
    std::span<u8> bytes{reinterpret_cast<u8*>(message.data()), message.size() * 4};
    RuntimePlayerBodyMessageViews views{{{}, nullptr, nullptr, nullptr}, nullptr, nullptr, {}, {}};
    std::optional<RuntimeSegmentContactActor> contact;
    std::array<u8, 0x2C> bound;
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

// Body action children of the classes installed so far: 0x5C (vtable 0x00505500), 0x5B (0x00505518) and
// 0x87 (0x00505BC0).
void SceneBodyMessages::action_tick(FactoryObject& action, u32)
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
        unbound("body action 0x5B vtable +0x14 (0x000804E0)");
    case 0x87:
        // 0x001628B0 returns at once.
        return;
    default:
        unbound("body action vtable +0x14 of factory type " + std::to_string(action.type));
    }
}

RuntimeBodyChannelContext SceneBodyMessages::channel_context()
{
    const std::span<const u8> body = player_.body;
    if (i32(read_u32(body, 0x154)) > 0 || read_u32(body, 0xFC) != 0)
    {
        unbound("body channel rows (+0x150) and actor +0x50 channel bindings");
    }
    channel_rows_ = {};
    channel_model_ = player_.actor_instance.startup.models.model.get();
    return RuntimeBodyChannelContext{player_.actor, services_.clock, channel_bindings_, channel_model_,
        channel_rows_, player_.body_resources, services_.channel_keys, services_.address_zero};
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
        unbound("body vtable +0x1C of factory type " + std::to_string(body.type));
    }
    RuntimeCameraBodySlots camera{player_.body, player_.actor, *player_.body_bits, services_.look_profile,
        services_.rig_tunables, player_.camera_view};
    return probe_camera_body_support(camera);
}
}
