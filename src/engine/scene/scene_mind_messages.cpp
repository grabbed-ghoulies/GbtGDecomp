#include "scene_mind_messages.h"
#include "../entities/entity.h"
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
          []() -> u16
          {
              unbound("body effect action (vtable +0x28)");
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
          []() -> u16
          {
              unbound("body range action (vtable +0x48)");
          },
          []() -> u16
          {
              unbound("body contact action (vtable +0x44)");
          }},
      cinema_{parent_, boost_, player.actor_bytes, player.actor_address, services.allowed_count, services.allowed_rows,
          [](u32) -> RuntimeMindTarget&
          {
              unbound("mind target records (DAT_00545988)");
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
          services.address_of},
      player_messages_{player.mind_events, feedback_, boost_, parent_, binding_, services.resume, services.game,
          player.actor_address, player.actor_bytes, *player.body_flags, target_, &action_target_, &event_source_,
          &actor_mind_, &body_state_view_, services.grids, services.nodes, player.mind_storage,
          player.actor_instance.animation, services.chapter_mode, services.runtime_flags, services.pulse,
          services.address_of,
          [](u32) -> const char*
          {
              unbound("player mind message string argument");
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
      cinema_tick_{cinema_, avoidance_, gaze_, control_, player.axis_device, services.keyboard}
{
    slots.object_at = [this](u32 address)
    {
        return services_.bytes_at(address);
    };
    cinema_mind_ = slots.cinema != nullptr;
    if (slots.cinema != nullptr)
    {
        slots.cinema->messages = &cinema_;
        slots.cinema->tick = &cinema_tick_;
        slots.cinema->target_bytes = [this]() -> std::span<const u8>
        {
            const u32 target = read_u32(player_.mind, 0x10);
            return target == 0 ? std::span<const u8>() : services_.bytes_at(target);
        };
    }
    else
    {
        slots.messages = &player_messages_;
    }
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
    body_query_.state = i16(read_u16(body, 0x30));
    std::memcpy(&body_query_.state_time, body.data() + 0x34, 4);
    body_query_.case_actor = player_.attached != nullptr ? player_.attached->actor : nullptr;
    body_query_.node = nullptr;
    body_query_.blocked = body[0x69];
    body_query_.active = body[0x6A];
    body_query_.suspended = body[0x6B];
    body_query_.permitted = body[0x6C];
    std::memcpy(&body_query_.timer, body.data() + 0x70, 4);
    body_query_.energy = i16(read_u16(body, 0x94));
    body_query_.maximum_energy = i16(read_u16(body, 0x96));
    body_query_.bits = player_.body_flags;
    target_ = object_at(read_u32(mind, 0x10), "mind +0x10 target");
    parent_.target_bytes = target_;
    parent_.target_actor = target_.empty() ? nullptr : services_.actor_at(read_u32(mind, 0x10));
    action_target_ = object_at(read_u32(mind, 0x14), "mind +0x14 action target");
    actor_mind_ = mind;
    body_state_view_ = object_at(read_u32(body, 0x84), "body +0x84 state");
    body_state_ = body_state_view_.data();
    cinema_.floor = i32(read_u32(player_.actor_bytes, 0xCEC));
    if (cinema_mind_ == false)
    {
        event_source_ = object_at(read_u32(mind, 0x88), "player mind +0x88 event source");
    }
    feedback_.input = player_.input;
    feedback_.input_address = player_.input.empty() ? 0 : services_.address_of(player_.input.data());
}

void SceneMindMessages::prepare_tick()
{
    refresh();
    refresh_gaze();
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
    const u32 target = read_u32(mind, 0xC);
    gaze_target_ = target == 0 ? nullptr : services_.actor_at(target);
    anchor_addresses_.clear();
    for (auto* node = static_cast<entity_detail::ListHead*>(g_actor_model_anchors.next); node != nullptr;
         node = static_cast<entity_detail::ListHead*>(node->next))
    {
        auto* anchor = reinterpret_cast<ActorModelAnchor*>(reinterpret_cast<u8*>(node)
            - offsetof(ActorModelAnchor, global_link));
        anchor_addresses_[anchor] = services_.address_of(anchor);
    }
    const u32 anchor = read_u32(mind, 0x30);
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
            unbound("mind +0x30 gaze anchor outside the model anchor list");
        }
    }
}
}
