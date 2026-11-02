#include "scene_mind_messages.h"
#include "../entities/mind_fields.h"
#include "../entities/body_action_ids.h"
#include "../entities/entity.h"
#include "../camera/display_view.h"
#include "../entities/body_fields.h"
#include <cstddef>
#include <cstring>

namespace gbtg
{
namespace
{
[[noreturn]] void unbound(const std::string& what)
{
    throw Error("scene mind message service is not bound: " + what);
}

std::span<u8> tail(std::span<u8> bytes, size_t offset)
{
    return bytes.empty() ? std::span<u8>() : bytes.subspan(offset);
}

EntityBitBlock& body_bits(NativePlayerInstance& player)
{
    if (player.body_bits == nullptr)
    {
        unbound("mind control without the body +0x15C bit block");
    }
    return *player.body_bits;
}
}

SceneMindMessages::SceneMindMessages(SceneMindServices& services, NativePlayerInstance& player,
    NativePlayerMindSlots& slots)
    : services_(services),
      player_(player),
      event_record_(player.actor_instance.startup.resource.resource.data()),
      actions_{player.attachment, player.body_selection,
          [&player](std::span<const u32> message)
          {
              player.self.message(message);
          },
          event_record_, player.callout},
      attachment_{player.body, tables_, player.body_selection, player.secondary, player.actor.class_key, body_state_,
          [this](const u8* record)
          {
              return services_.address_of(record);
          }},
      parent_{player.mind_events, player.mind_goals, attachment_, player.actor, player.actor, *player.body_flags,
          body_query_, player.actor_instance.animation, player.actor_instance.startup.resource, player.callout,
          services.flag_names, services.action_names, services.callout_names, services.goal_flag_names,
          services.random,
          [&player](std::span<u32> message)
          {
              player.self.message(message);
          },
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body effect action (vtable +0x28)");
              }
              return player.body_slot_key(0x28);
          },
          [this](const Actor* actor) -> u32
          {
              return actor == nullptr ? 0 : services_.address_of(actor);
          },
          {}, nullptr},
      // Shared 0x00137A80/0x00137C30 boost state: cinema minds at +0x94, player minds at +0x158.
      boost_{tail(player.mind, slots.cinema != nullptr ? 0x94 : 0x158), player.actor_bytes, player.actor, player.actor,
          *player.body_flags, player.attachment, services.resume, services.story_bits, services.story_word,
          player.actor_address,
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body range action (vtable +0x48)");
              }
              return player.body_slot_key(0x48);
          },
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body contact action (vtable +0x44)");
              }
              return player.body_slot_key(0x44);
          }},
      cinema_{parent_, boost_, player.actor_bytes, player.actor_address, services.allowed_count, services.allowed_rows,
          [&services](u32 address) -> RuntimeMindTarget&
          {
              return services.target_at(address);
          },
          i32(read_u32(player.actor_bytes, 0xCEC)), services.grids, services.nodes, player.mind_storage,
          services.address_of},
      feedback_{player.mind, player.body, player.actor, player.input, 0, services.activation, actions_,
          services.event_ids, services.registry, services.error},
      binding_{player.actor_bytes, player.actor_address, player.links[1], entity_detail::g_world.case_head,
          player.input, services.runtime_flags, services.port_devices, services.release, services.activation,
          [this](std::span<u8> device) -> u32
          {
              return services_.address_of(device.data());
          },
          services.address_of, player.actor},
      player_messages_{player.mind_events, feedback_, boost_, parent_, binding_, services.resume, services.game,
          player.actor_address, player.actor_bytes, *player.body_flags, target_, &action_target_, &event_source_,
          &actor_mind_, &body_state_view_, services.grids, services.nodes, player.mind_storage,
          player.actor_instance.animation, services.chapter_mode, services.runtime_flags, services.pulse,
          services.address_of,
          [this](u32 address) -> const char*
          {
              return address == 0 ? nullptr : reinterpret_cast<const char*>(services_.resolve(address));
          },
          [this](u32 address) -> const Actor*
          {
              return services_.actor_at(address);
          }},
      mind_tick_{player.mind_events, player.requests, player.actor, player.actor, player.body, services.clock,
          [this](RuntimePlayerMindRequest& request)
          {
              return i32(services_.initialize_request(player_.mind_object, request));
          },
          [this](RuntimePlayerMindRequest& request)
          {
              return services_.update_request(player_.mind_object, request);
          }},
      avoidance_{mind_tick_, player.ring, services.address_zero, player.request_anchor, player.scene.mind_request_sizes,
          i32(read_u32(player.actor_bytes, 0xCEC)), player.allowed, services.random, services.occupancy_initial},
      gaze_{player.mind, player.body, player.actor, player.actor, gaze_target_, gaze_anchor_, player.input_device,
          services.input_state, anchor_addresses_, services.random},
      control_{player.mind, player.attachment, body_bits(player), services.clock, player.input_device,
          services.input_state,
          [&player]()
          {
              return player.body_action_key();
          }},
      cinema_tick_{cinema_, avoidance_, gaze_, control_, player.axis_device, services.keyboard},
      player_tick_{mind_tick_, player.actor_bytes, player.body_bits, active_player_link_, services.pause_references,
          services.release, services.health},
      navigation_{path_search_, player.actor, player.body,
          *reinterpret_cast<i32*>(player.actor_bytes.data() + 0xCEC), player.navigation, services.clock},
      navigation_control_{navigation_, player.actor, body_bits(player), player.input_device, services.input_state,
          player.axis_device, services.keyboard, nullptr},
      eligibility_{player.body, body_bits(player), player.actor, player.attached},
      escape_{player.actor, player.actor_bytes, {}, static_cast<const u8*>(services.markers),
          player.allowed, services.random, services.occupancy_initial},
      movement_{player.mind, player.actor_bytes, player.body, player.actor, eligibility_,
          navigation_control_, escape_, services.clock, player.room, player.allowed, services.random,
          services.occupancy_initial, nullptr, body_query_},
      spawn_{movement_, player.mind_action,
          [this](Actor& actor) -> RuntimeBodyAttachedActor&
          {
              return services_.attached_actor_at(services_.address_of(&actor));
          }},
      cinema_request_{movement_, spawn_, parent_,
          [this]() -> RuntimeMindChainRequestSelection&
          {
              // 0x0007B090 passes the mind's +0x10 target record to 0x00136770, as 0x0007DF50 does for players.
              return target_action(get(player_.mind, mind_field::target_record)).requests;
          },
          player.body, player.input, services.release, player.cinema_resource, services.sounds,
          services.damage_tables, services.mode_scales, player.body_selection,
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body effect action (vtable +0x38)");
              }
              return player.body_slot_key(0x38);
          },
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body contact action (vtable +0x3C)");
              }
              return player.body_slot_key(0x3C);
          },
          [&player](RuntimeBodyAttachedActor& attached) -> u16
          {
              // Body vtable +0x5C: the base 0x00026C60 for types 0x27, 0x2A and 0x2D (no getter replaces it).
              return runtime_body_near_action_id(player.body, player.actor, attached, player.action_table);
          }},
      boost_preset_(services.boost_preset),
      focus_{player.mind, player.actor, gaze_target_, eligibility_, services.target_eligibility,
          services.target_addresses, services.query, retained_cone_distance_},
      search_{player.actor, player.actor_bytes, eligibility_, nullptr, services.address_zero, retained_angle_,
          services.angular_limit},
      shape_target_{player.mind, player.body, player.actor, nullptr, body_bits(player),
          *reinterpret_cast<i32*>(player.actor_bytes.data() + 0xCEC),
          player.mind.empty() ? 0 : services.address_of(player.mind.data())},
      proposal_{search_, shape_target_, player.mind_events, services.target_views, embedded_target_},
      target_control_{focus_, proposal_, feedback_, services.clock, services.address_zero},
      action_control_{target_control_, attachment_, body_bits(player), player.input_device, services.input_state,
          [this](u32 target) -> RuntimePlayerTargetActionBinding&
          {
              return target_action(target);
          },
          services.attachment_notice},
      pending_{player.mind, pending_attachment_, player.mind_action, eligibility_, player.body,
          player.body_selection, player.input_device, services.input_state,
          [&player](RuntimeBodyAttachedActor& attached) -> u32
          {
              // Body vtable +0x5C: the base 0x00026C60 for types 0x27, 0x2A and 0x2D (no getter replaces it).
              return runtime_body_near_action_id(player.body, player.actor, attached, player.action_table);
          }},
      inactivity_{player.mind, body_bits(player), services.clock, player.input_device, services.input_state},
      player_avoidance_{player.mind, player.body, player.actor,
          *reinterpret_cast<i32*>(player.actor_bytes.data() + 0xCEC), player.input_device, services.input_state,
          player.axis_device, services.keyboard, services.random, services.occupancy_initial, retained_direction_},
      control_update_{action_control_, pending_, gaze_, inactivity_, control_, boost_, player_avoidance_,
          player.actor_instance.animation, boost_preset_, camera_direction_, services.view_invert, services.random},
      player_request_{parent_, control_update_, navigation_control_, player.body, body_bits(player),
          player.body_selection, services.flag_names,
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body effect action (vtable +0x38)");
              }
              return player.body_slot_key(0x38);
          },
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body contact action (vtable +0x3C)");
              }
              return player.body_slot_key(0x3C);
          }}
{
    services.target_eligibility[&player.actor] = &eligibility_;
    services.target_addresses[&player.actor] = player.actor_address;
    services.target_queries[&player.actor] = &body_query_;
    // 0x0007EF10 writes mind +0xC8 (the record's +0x10) with this actor and +0xCC with mode 3. The record's
    // bytes and address are bound in refresh_control, once the mind exists.
    embedded_record_ = std::make_unique<RuntimeMindTarget>(RuntimeMindTarget{nullptr, {}, 0, player.actor,
        player.actor_bytes, body_query_, player.current, nullptr});
    slots.object_at = [this](u32 address)
    {
        return services_.bytes_at(address);
    };
    // Mind +0x10 object +0x20, or null.
    slots.target_position = [this]() -> const Vec3*
    {
        const u32 target = get(player_.mind, mind_field::target_record);
        if (target == 0)
        {
            return nullptr;
        }
        return reinterpret_cast<const Vec3*>(services_.bytes_at(target).subspan(0x20, sizeof(Vec3)).data());
    };
    // The tick's input word is actor +0x960, this player's input record.
    slots.input_at = [this](u32 address) -> std::span<u8>
    {
        if (player_.input.empty() || address != services_.address_of(player_.input.data()))
        {
            unbound("mind tick input word that is not this player's input record");
        }
        return player_.input;
    };
    cinema_mind_ = slots.cinema != nullptr;
    if (slots.cinema != nullptr)
    {
        slots.cinema->messages = &cinema_;
        slots.cinema->tick = &cinema_tick_;
        slots.cinema->request_update = &cinema_request_;
        slots.cinema->target_bytes = [this]() -> std::span<const u8>
        {
            const u32 target = get(player_.mind, mind_field::target_record);
            return target == 0 ? std::span<const u8>() : services_.bytes_at(target);
        };
    }
    else
    {
        slots.messages = &player_messages_;
        slots.tick = &player_tick_;
        slots.request_update = &player_request_;
    }
}

void SceneMindMessages::refresh_chain_image()
{
    const auto* groups = player_.animation_groups;
    const u32 count = player_.animation_group_count;
    if (groups == chain_image_source_ && chain_rows_.count == count)
    {
        return;
    }
    chain_image_source_ = groups;
    size_t words = 0;
    for (u32 index = 0; index < count; ++index)
    {
        words += size_t(groups[index].length) * groups[index].count;
    }
    chain_image_.assign(size_t(count) * 8 + words * 4, 0);
    chain_image_base_ = chain_image_.empty() ? 0 : services_.heap_address(chain_image_.data(), chain_image_.size());
    u32 value_offset = count * 8;
    for (u32 index = 0; index < count; ++index)
    {
        const auto& group = groups[index];
        u8* row = chain_image_.data() + index * 8;
        std::memcpy(row, &group.length, 2);
        std::memcpy(row + 2, &group.count, 2);
        const u32 values = chain_image_base_ + value_offset;
        std::memcpy(row + 4, &values, 4);
        for (u32 value = 0; value < u32(group.length) * group.count; ++value)
        {
            const u32 address = services_.address_of(group.values[value]);
            std::memcpy(chain_image_.data() + value_offset, &address, 4);
            value_offset += 4;
        }
    }
    chain_rows_ = RuntimeAttachmentChainRows{chain_image_.empty() ? nullptr : chain_image_.data(), u8(count)};
}

const u8* SceneMindMessages::resolve_chain_address(u32 address) const
{
    if (chain_image_base_ != 0 && address >= chain_image_base_ && address - chain_image_base_ < chain_image_.size())
    {
        return chain_image_.data() + (address - chain_image_base_);
    }
    return services_.resolve(address);
}

SceneMindMessages::TargetAction::TargetAction(SceneMindMessages& owner, RuntimeMindTarget& target)
    : bits{owner.player_.animation_bits == nullptr ? 0u : owner.player_.animation_bits->bit_count,
          owner.player_.animation_bits == nullptr ? std::span<u8>()
              : std::span<u8>(owner.player_.animation_bits->bytes, owner.player_.animation_bits->byte_count)},
      selection{owner.attachment_, owner.eligibility_, owner.player_.actor, target, owner.player_.body_attributes.data(),
          bits, owner.services_.random,
          owner.player_.animation_resource.empty() ? 0u : owner.services_.address_of(owner.player_.animation_resource.data())},
      mind{selection, owner.player_.mind_events, body_bits(owner.player_), owner.player_.actor_bytes},
      chains{selection, owner.chain_rows_,
          [&owner](u32 address)
          {
              return owner.resolve_chain_address(address);
          }},
      length{owner.player_.body, owner.player_.actor_bytes, owner.player_.body_attributes.data(),
          body_bits(owner.player_), owner.services_.chain_limits[0], owner.services_.chain_limits[1],
          read_f32(owner.services_.chain_limits, 4), read_f32(owner.services_.chain_limits, 8),
          owner.services_.class_table},
      requests{mind, chains, length, owner.services_.special_first, owner.services_.special_second,
          owner.services_.target_body(owner.services_.address_of(&target.actor))}, // Record +0x10 actor's body.
      binding{requests, requests.target_body}
{
}

// The request contexts of 0x0007DF50 against `target`, rebuilt from the current body state on each use.
RuntimePlayerTargetActionBinding& SceneMindMessages::target_action(u32 target)
{
    if (player_.animation_bits == nullptr)
    {
        unbound("target action without the body +0x60 request bits");
    }
    refresh_chain_image();
    auto& action = target_actions_[target];
    action = std::make_unique<TargetAction>(*this, services_.target_at(target));
    return action->binding;
}

SceneMindMessages::~SceneMindMessages()
{
    services_.target_eligibility.erase(&player_.actor);
    services_.target_addresses.erase(&player_.actor);
    services_.target_queries.erase(&player_.actor);
    if (embedded_target_.address != 0)
    {
        services_.embedded_targets.erase(embedded_target_.address);
    }
}

// Views 0x0007D130 reads when the player mind's request slot runs.
void SceneMindMessages::refresh_control()
{
    const auto mind = player_.mind;
    if (entity_detail::g_display_view == nullptr)
    {
        unbound("player input rotation without the DAT_005077A8 display view");
    }
    camera_direction_ = entity_detail::g_display_view->forward;
    search_.first = services_.first_target;
    const u32 pending = read_u32(mind, 0x134);
    pending_attachment_ = pending == 0 ? nullptr : &services_.attached_actor_at(pending);
    if (read_u32(player_.actor_bytes, 0x8F8) != 0)
    {
        unbound("actor +0x8F8 live attachment view for the player shape target");
    }
    // Actor +0x8F8 is the first record on the actor's +0x8F4 list (0x0012AFE0 links each new one at its head).
    shape_target_.attachment = static_cast<ActorLiveAttachment*>(player_.actor_instance.live_attachments.next);
    const u32 embedded_owner = read_u32(mind, 0xB8 + 0x10);
    if (embedded_owner != player_.actor_address)
    {
        unbound("player mind +0xC8 naming an actor other than its own");
    }
    const u32 embedded_address = services_.address_of(mind.data()) + 0xB8;
    if (embedded_target_.address != embedded_address)
    {
        if (embedded_target_.address != 0)
        {
            services_.embedded_targets.erase(embedded_target_.address);
        }
        services_.embedded_targets[embedded_address] = {&embedded_target_, embedded_record_.get()};
    }
    embedded_record_->bytes = mind.subspan(0xB8, 0x68);
    embedded_record_->address = embedded_address;
    embedded_record_->occupancy = player_.current; // Actor +0xCA8.
    embedded_target_ = RuntimePlayerTargetView{mind.subspan(0xB8, 0x68), embedded_address, &player_.actor,
        embedded_record_.get()};
}

void SceneMindMessages::refresh()
{
    const auto body = player_.body;
    const auto mind = player_.mind;
    const auto object_at = [this](u32 address, const char* field) -> std::span<const u8>
    {
        if (address == 0)
        {
            return {};
        }
        try
        {
            return services_.bytes_at(address);
        }
        catch (const std::exception& error)
        {
            unbound(std::string(field) + " view: " + error.what());
        }
    };
    tables_ = player_.animation_resource.data() == nullptr ? nullptr
        : services_.attack_tables.at(player_.animation_resource.data());
    body_query_.state = i16(get(body, body_field::move_holds));
    std::memcpy(&body_query_.state_time, body.data() + 0x34, 4);
    body_query_.case_actor = player_.attached != nullptr ? player_.attached->actor : nullptr;
    body_query_.node = nullptr;
    body_query_.blocked = body[0x69];
    body_query_.active = body[0x6A];
    body_query_.suspended = body[0x6B];
    body_query_.permitted = body[0x6C];
    std::memcpy(&body_query_.timer, body.data() + 0x70, 4);
    body_query_.energy = i16(get(body, body_field::energy));
    body_query_.maximum_energy = i16(get(body, body_field::max_energy));
    body_query_.bits = player_.body_flags;
    target_ = object_at(get(mind, mind_field::target_record), "mind +0x10 target");
    parent_.target_bytes = target_;
    player_messages_.target = target_;
    parent_.target_actor = target_.empty() ? nullptr : services_.actor_at(get(mind, mind_field::target_record));
    action_target_ = object_at(get(mind, mind_field::action_record), "mind +0x14 action target");
    actor_mind_ = mind;
    // Body +0x84 is an attackdata request row (0x0001D2E0), stride 0x24 or more, by its data address.
    const u32 request = get(body, body_field::hit_record);
    body_state_view_ = request == 0 ? std::span<const u8>() : std::span<const u8>(services_.resolve(request), 0x24);
    body_state_ = body_state_view_.data();
    cinema_.floor = i32(read_u32(player_.actor_bytes, 0xCEC));
    if (cinema_mind_ == false)
    {
        event_source_ = object_at(read_u32(mind, 0x88), "player mind +0x88 event source");
    }
    feedback_.input = player_.input;
    feedback_.input_address = player_.input.empty() ? 0 : services_.address_of(player_.input.data());
}

bool SceneMindMessages::select_attachment_request(u16 key)
{
    refresh();
    return select_runtime_attachment_request(attachment_, key); // 0x0001D210
}

void SceneMindMessages::prepare_tick()
{
    refresh();
    refresh_gaze();
    active_player_link_ = services_.active_player_link();
    cinema_tick_.player_link = active_player_link_;
    path_search_ = player_.mind_storage;
    navigation_.body = player_.body;
    eligibility_.body = player_.body;
    eligibility_.attached = player_.attached;
    movement_.mind = player_.mind;
    movement_.body = player_.body;
    movement_.required = player_.room;
    movement_.allowed = player_.allowed;
    movement_.occupancy_initial = services_.occupancy_initial;
    const u32 target = get(player_.mind, mind_field::target_record);
    movement_.target = target == 0 ? nullptr : &services_.target_at(target);
    escape_.target = target == 0 ? std::span<const u8>() : movement_.target->bytes;
    escape_.markers = static_cast<const u8*>(services_.markers);
    escape_.allowed = player_.allowed;
    escape_.occupancy_initial = services_.occupancy_initial;
    const u32 spawn = get(player_.mind, mind_field::action_actor);
    player_.mind_action = spawn == 0 ? nullptr : &services_.attached_actor_at(spawn);
    cinema_request_.body = player_.body;
    cinema_request_.input = player_.input;
    if (!cinema_mind_)
    {
        refresh_control();
    }
}

void SceneMindMessages::refresh_gaze()
{
    const auto mind = player_.mind;
    mind_tick_.body = player_.body;
    avoidance_.goals = player_.ring;
    avoidance_.floor = i32(read_u32(player_.actor_bytes, 0xCEC));
    avoidance_.allowed = player_.allowed;
    gaze_.mind = mind;
    gaze_.body = player_.body;
    control_.mind = mind;
    const u32 target = get(mind, mind_field::target);
    gaze_target_ = target == 0 ? nullptr : services_.actor_at(target);
    anchor_addresses_.clear();
    for (auto* node = static_cast<entity_detail::ListHead*>(g_actor_model_anchors.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        auto* anchor = reinterpret_cast<ActorModelAnchor*>(reinterpret_cast<u8*>(node)
            - offsetof(ActorModelAnchor, global_link));
        anchor_addresses_[anchor] = services_.address_of(anchor);
    }
    const u32 anchor = get(mind, mind_field::gaze_anchor);
    gaze_anchor_ = nullptr;
    if (anchor != 0)
    {
        for (const auto& [native, address] : anchor_addresses_)
        {
            if (address == anchor)
            {
                gaze_anchor_ = native;
            }
        }
        if (gaze_anchor_ == nullptr)
        {
            unbound("mind +0x30 gaze anchor " + std::to_string(anchor) + " outside the model anchor list");
        }
    }
}

SceneCameraMindParent::SceneCameraMindParent(SceneMindServices& services, NativePlayerInstance& player)
    : services_(services),
      player_(player),
      attachment_{player.body, tables_, player.body_selection, player.secondary, player.actor.class_key, body_state_,
          [this](const u8* record)
          {
              return services_.address_of(record);
          }},
      parent_{player.mind_events, player.mind_goals, attachment_, player.actor, player.actor, *player.body_flags,
          body_query_, player.actor_instance.animation, player.actor_instance.startup.resource, player.callout,
          services.flag_names, services.action_names, services.callout_names, services.goal_flag_names,
          services.random,
          [&player](std::span<u32> message)
          {
              player.self.message(message);
          },
          [&player]() -> u16
          {
              if (!player.body_slot_key)
              {
                  unbound("body effect action (vtable +0x28)");
              }
              return player.body_slot_key(0x28);
          },
          [this](const Actor* actor) -> u32
          {
              return actor == nullptr ? 0 : services_.address_of(actor);
          },
          {}, nullptr}
{
}

i32 SceneCameraMindParent::update(std::span<const u8> request)
{
    const auto body = player_.body;
    const auto mind = player_.mind;
    tables_ = player_.animation_resource.data() == nullptr ? nullptr
        : services_.attack_tables.at(player_.animation_resource.data());
    body_query_.state = i16(get(body, body_field::move_holds));
    std::memcpy(&body_query_.state_time, body.data() + 0x34, 4);
    body_query_.case_actor = player_.attached != nullptr ? player_.attached->actor : nullptr;
    body_query_.node = nullptr;
    body_query_.blocked = body[0x69];
    body_query_.active = body[0x6A];
    body_query_.suspended = body[0x6B];
    body_query_.permitted = body[0x6C];
    std::memcpy(&body_query_.timer, body.data() + 0x70, 4);
    body_query_.energy = i16(get(body, body_field::energy));
    body_query_.maximum_energy = i16(get(body, body_field::max_energy));
    body_query_.bits = player_.body_flags;
    const u32 target = get(mind, mind_field::target_record);
    target_ = target == 0 ? std::span<const u8>{} : services_.bytes_at(target);
    parent_.target_bytes = target_;
    parent_.target_actor = target_.empty() ? nullptr : services_.actor_at(target);
    const u32 state = get(body, body_field::hit_record);
    body_state_view_ = state == 0 ? std::span<const u8>() : std::span<const u8>(services_.resolve(state), 0x24);
    body_state_ = body_state_view_.data();
    return update_runtime_parent_mind_request(parent_, request); // 0x000295E0
}
}
