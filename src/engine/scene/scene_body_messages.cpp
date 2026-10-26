#include "scene_body_messages.h"
#include "../entities/body_orientation.h"
#include "../camera/display_view.h"
#include "../entities/body_move_action_slots.h"
#include "../entities/body_gravity_motion.h"
#include "../entities/body_stand_alert_tick.h"
#include "../entities/body_struggle_state.h"
#include "../entities/actor_transform.h"
#include "../entities/body_walk_alert_tick.h"
#include "../entities/body_walk_target_state.h"
#include "../entities/body_hold_state.h"
#include "../entities/body_throw_state.h"
#include "../entities/body_action_ids.h"
#include "../transitions/scene_transition.h"
#include "../transitions/scene_transitions.h"
#include "../saves/runtime_profile.h"
#include "scene.h"
#include "../scripts/script_actions.h"
#include "scene_actor_services.h"
#include "gbtg/actor_spawn_factory.h"
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
          event_record_, player.callout, services.image_string},
      // Actor +0x6CC is sound slot 5 (slots of 0xB8 from +0x334).
      sound_commands_{player.actor, player.actor, player.body, services.damage_tables, services.effect_tags,
          services.footsteps, player.actor.sound_slots[5], services.sounds},
      projectiles_{player.actor, player.self, services.resources, services.factory, services.default_position,
          services.default_euler, created_},
      effect_commands_{player.actor, i32(read_u32(player.actor_bytes, 0xCEC)), services.effect_scene,
          services.surfaces, services.effect_tags, services.effect_suffixes[0], services.effect_suffixes[1],
          services.effect_suffixes[2], services.effect_suffixes[3], services.effects},
      query_name_([this](RuntimeBodyAttachedActor& attached, std::span<char, 128> name)
          {
              // Actual actor vtable +0xC with {0x99, the name buffer}: a container writes its contents' name.
              if (!services_.bind_block || !services_.unbind_block)
              {
                  unbound("attached actor name query (actor message 0x99)");
              }
              const u32 buffer = services_.bind_block(name.data(), name.size());
              const std::array<u32, 2> message{0x99, buffer};
              attached.message(message);
              services_.unbind_block(name.data());
          }),
      animation_{player.actor, *player.body_flags, player.collision, services.latches, nullptr, nullptr,
          sound_commands_, player.attachment_creation, projectiles_, services.objects, command_name_, query_name_,
          effect_commands_, effect_flags_, player.actor_instance.animation, nullptr, nullptr, services.callouts,
          services.flags, proximity_, player.scene.address_of(player.body.data()),
          player.actor_instance.startup.resource.resource.data(),
          player.callout, player.body_lists},
      spatial_{player.actor, player.actor_address, u8(read_u32(player.actor_bytes, 0xCEC)),
          player.actor_instance.live_attachments, services.global_attachments},
      parent_{player.actor, player.actor_bytes, actions_, children_, animation_, spatial_, shape_scratch_,
          services.latches, player.collision, first_timer_, second_timer_, services.contact_players,
          services.default_position, [this]()
          {
              bind_action_children();
          }},
      death_notice_{services.game, body_view_, player.actor_address,
          reinterpret_cast<const char*>(player.actor_bytes.data() + 0xBA0), player.spawn.record, services.resources},
      death_drops_{player.actor, player.actor_bytes, services.resources, services.factory,
          [this](FactoryObject object) -> u32
          {
              if (object.bytes == nullptr)
              {
                  return 0u;
              }
              services_.objects.push_back(std::move(object));
              return services_.address_of(services_.objects.back().bytes.get());
          },
          [this](u32 actor, std::span<const u32> words)
          {
              services_.message(actor, words);
          },
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
              // Body vtable +0x4C is the parent's 0x00027500 for types 0x27, 0x2A and 0x2D (no getter overrides it).
              return runtime_body_death_action_key();
          },
          first_timer_},
      timer_creation_{first_timer_, player.child,
          [this](FactoryObject object) -> RuntimeBodyTimerObject&
          {
              return keep_timer(std::move(object));
          }},
      damage_{services.game, processor_, nullptr, timer_creation_},
      messages_{parent_, damage_, player.ring, services.address_zero, mask_,
          *reinterpret_cast<const i32*>(player.actor_bytes.data() + 0xCEC), player.actor_instance.render,
          services.clock, services.tag_rows, services.tag_names, services.shock_pass, services.shock_fail,
          player.body_attributes, hit_view_, services.scene_flags, services.restart_requested},
      controls_{player.actor, player.actor_bytes[0x197], player.actor_instance.anchors, live_attachment_,
          player.actor_instance.startup.resource, player.callout, player.body_lists, *player.body_flags,
          player.scene.address_of(player.body.data())}
{
    // The player's view of its body's next-action slots (the mind reads +0x28, +0x38 and +0x3C).
    player_.body_slot_key = [this](u32 slot)
    {
        return body_slot_key(slot);
    };
    // The attachment's re-selection (0x000277E0) reads body vtable +0x20.
    player_.body_action_key = [this]()
    {
        return body_slot_key(0x20);
    };
    // The state and move actions installed so far (0x5B, 0x5C, 0x87) all have the empty 0x001628B0 at +0xC.
    empty_child_.message = [](std::span<u8>)
    {
    };
    walk_alert_child_.message = [this](std::span<u8> message)
    {
        message_runtime_walk_alert(message, [this, message]()
        {
            stop_runtime_walk_alert(player_.body, message, 1, [this]()
            {
                return u16(body_slot_key(0x20));
            }, parent_.actions); // 0x00023890, ECX 1
        });
    };
}

void SceneBodyMessages::bind_action_children()
{
    auto& actions = player_.body_selection;
    for (FactoryObject* child : {&actions.first, &actions.second})
    {
        if (child->bytes == nullptr)
        {
            continue;
        }
        if (child->type == 0x8E || child->type == 0x8F)
        {
            // 0x8F's +0xC is 0x8E's 0x000A5A40.
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
        if (child->type == 0x7A)
        {
            auto& knockdown = knockdown_children_[child->bytes.get()];
            if (knockdown == nullptr)
            {
                knockdown = std::make_unique<RuntimeBodyMessageChild>();
                knockdown->message = [this, child](std::span<u8> message)
                {
                    message_runtime_knockdown_state({child->bytes.get(), child->size}, message,
                        knockdown_context()); // 0x0009D0E0
                };
            }
            children_[child->bytes.get()] = knockdown.get();
            continue;
        }
        if (child->type == 0x5E)
        {
            auto& anim = anim_children_[child->bytes.get()];
            if (anim == nullptr)
            {
                anim = std::make_unique<RuntimeBodyMessageChild>();
                anim->message = [this, child](std::span<u8> message)
                {
                    message_runtime_anim_state({child->bytes.get(), child->size}, message,
                        anim_context()); // 0x00088960
                };
            }
            children_[child->bytes.get()] = anim.get();
            continue;
        }
        if (child->type == 0x62)
        {
            auto& die = die_children_[child->bytes.get()];
            if (die == nullptr)
            {
                die = std::make_unique<RuntimeBodyMessageChild>();
                die->message = [child](std::span<u8> message)
                {
                    message_runtime_die_state({child->bytes.get(), child->size}, message); // 0x0008C4D0
                };
            }
            children_[child->bytes.get()] = die.get();
            continue;
        }
        if (child->type == 0x64)
        {
            auto& dizzy = dizzy_children_[child->bytes.get()];
            if (dizzy == nullptr)
            {
                dizzy = std::make_unique<RuntimeBodyMessageChild>();
                dizzy->message = [child](std::span<u8> message)
                {
                    message_runtime_dizzy_state({child->bytes.get(), child->size}, message); // 0x0008CEA0
                };
            }
            children_[child->bytes.get()] = dizzy.get();
            continue;
        }
        if (child->type == 0x8B)
        {
            auto& thrown = throw_children_[child->bytes.get()];
            if (thrown == nullptr)
            {
                thrown = std::make_unique<RuntimeBodyMessageChild>();
                thrown->message = [this, child](std::span<u8> message)
                {
                    message_runtime_throw_state({child->bytes.get(), child->size}, message,
                        throw_context()); // 0x000A3D20
                };
            }
            children_[child->bytes.get()] = thrown.get();
            continue;
        }
        if (child->type == 0x66)
        {
            auto& arc = arc_children_[child->bytes.get()];
            if (arc == nullptr)
            {
                arc = std::make_unique<RuntimeBodyMessageChild>();
                arc->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_arc_state({child->bytes.get(), child->size}, words, arc_context()); // 0x0008DD30
                };
            }
            children_[child->bytes.get()] = arc.get();
            continue;
        }
        if (child->type == 0x8D)
        {
            auto& walk = walk_children_[child->bytes.get()];
            if (walk == nullptr)
            {
                walk = std::make_unique<RuntimeBodyMessageChild>();
                walk->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_walk_state({child->bytes.get(), child->size}, words, walk_context()); // 0x000A5270
                };
            }
            children_[child->bytes.get()] = walk.get();
            continue;
        }
        if (child->type == 0x79)
        {
            auto& jump_on = jump_on_children_[child->bytes.get()];
            if (jump_on == nullptr)
            {
                jump_on = std::make_unique<RuntimeBodyMessageChild>();
                jump_on->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_jump_on_state({child->bytes.get(), child->size}, words,
                        jump_on_context()); // 0x0009AE60
                };
            }
            children_[child->bytes.get()] = jump_on.get();
            continue;
        }
        if (child->type == 0x5F)
        {
            auto& arc_attack = arc_attack_children_[child->bytes.get()];
            if (arc_attack == nullptr)
            {
                arc_attack = std::make_unique<RuntimeBodyMessageChild>();
                arc_attack->message = [this, child](std::span<u8> message)
                {
                    const std::span<u32> words{reinterpret_cast<u32*>(message.data()), message.size() / 4};
                    if (!words.empty() && words[0] == 0x62)
                    {
                        unbound("arc attack state word message 0x62 without the collision reply's contact record");
                    }
                    message_runtime_arc_attack_state({child->bytes.get(), child->size}, words, {},
                        arc_attack_context()); // 0x00089530
                };
            }
            children_[child->bytes.get()] = arc_attack.get();
            continue;
        }
        if (child->type == 0x6F)
        {
            auto& generate = generate_ghouly_children_[child->bytes.get()];
            if (generate == nullptr)
            {
                generate = std::make_unique<RuntimeBodyMessageChild>();
                generate->message = [child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_generate_ghouly_state({child->bytes.get(), child->size}, words); // 0x00094150
                };
            }
            children_[child->bytes.get()] = generate.get();
            continue;
        }
        if (child->type == 0x78)
        {
            auto& swing = swing_children_[child->bytes.get()];
            if (swing == nullptr)
            {
                swing = std::make_unique<RuntimeBodyMessageChild>();
                swing->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_swing_state({child->bytes.get(), child->size}, words,
                        swing_context()); // 0x00098620
                };
            }
            children_[child->bytes.get()] = swing.get();
            continue;
        }
        if (child->type == 0x7E)
        {
            auto& scoop = scoop_whack_children_[child->bytes.get()];
            if (scoop == nullptr)
            {
                scoop = std::make_unique<RuntimeBodyMessageChild>();
                scoop->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_scoop_whack_state({child->bytes.get(), child->size}, words,
                        scoop_whack_context()); // 0x0009EED0
                };
            }
            children_[child->bytes.get()] = scoop.get();
            continue;
        }
        if (child->type == 0x67)
        {
            auto& dance = drone_dance_children_[child->bytes.get()];
            if (dance == nullptr)
            {
                dance = std::make_unique<RuntimeBodyMessageChild>();
                dance->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_drone_dance_state({child->bytes.get(), child->size}, words,
                        drone_dance_context()); // 0x0008E480
                };
            }
            children_[child->bytes.get()] = dance.get();
            continue;
        }
        if (child->type == 0x86)
        {
            auto& spin = spin_dizzy_children_[child->bytes.get()];
            if (spin == nullptr)
            {
                spin = std::make_unique<RuntimeBodyMessageChild>();
                spin->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_spin_dizzy_state({child->bytes.get(), child->size}, words,
                        spin_dizzy_context()); // 0x000A24B0
                };
            }
            children_[child->bytes.get()] = spin.get();
            continue;
        }
        if (child->type == 0x84)
        {
            auto& endure = endure_children_[child->bytes.get()];
            if (endure == nullptr)
            {
                endure = std::make_unique<RuntimeBodyMessageChild>();
                endure->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_endure_state({child->bytes.get(), child->size}, words, endure_context()); // 0x000A1850
                };
            }
            children_[child->bytes.get()] = endure.get();
            continue;
        }
        if (child->type == 0x89)
        {
            auto& stunned = stunned_children_[child->bytes.get()];
            if (stunned == nullptr)
            {
                stunned = std::make_unique<RuntimeBodyMessageChild>();
                stunned->message = [this, child](std::span<u8> message)
                {
                    message_runtime_stunned_state({child->bytes.get(), child->size}, message,
                        stunned_context()); // 0x000A3460
                };
            }
            children_[child->bytes.get()] = stunned.get();
            continue;
        }
        if (child->type == 0x76)
        {
            auto& stream = hit_stream_children_[child->bytes.get()];
            if (stream == nullptr)
            {
                stream = std::make_unique<RuntimeBodyMessageChild>();
                stream->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_hit_stream_state({child->bytes.get(), child->size}, words,
                        hit_stream_context()); // 0x00097890
                };
            }
            children_[child->bytes.get()] = stream.get();
            continue;
        }
        if (child->type == 0x6A)
        {
            auto& flame = flamethrower_children_[child->bytes.get()];
            if (flame == nullptr)
            {
                flame = std::make_unique<RuntimeBodyMessageChild>();
                flame->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_flamethrower_state({child->bytes.get(), child->size}, words,
                        flamethrower_context()); // 0x0008FBF0
                };
            }
            children_[child->bytes.get()] = flame.get();
            continue;
        }
        if (child->type == 0x83)
        {
            auto& slide = slide_children_[child->bytes.get()];
            if (slide == nullptr)
            {
                slide = std::make_unique<RuntimeBodyMessageChild>();
                slide->message = [this, child](std::span<u8> message)
                {
                    message_runtime_slide_state({child->bytes.get(), child->size}, message,
                        slide_context()); // 0x000A1070
                };
            }
            children_[child->bytes.get()] = slide.get();
            continue;
        }
        if (child->type == 0x8C)
        {
            auto& transform = transform_children_[child->bytes.get()];
            if (transform == nullptr)
            {
                transform = std::make_unique<RuntimeBodyMessageChild>();
                transform->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_transform_state({child->bytes.get(), child->size}, words,
                        transform_context()); // 0x000A49D0
                };
            }
            children_[child->bytes.get()] = transform.get();
            continue;
        }
        if (child->type == 0x7D)
        {
            auto& lift = lift_children_[child->bytes.get()];
            if (lift == nullptr)
            {
                lift = std::make_unique<RuntimeBodyMessageChild>();
                lift->message = [this, child](std::span<u8> message)
                {
                    const std::span<const u32> words{reinterpret_cast<const u32*>(message.data()), message.size() / 4};
                    message_runtime_lift_state({child->bytes.get(), child->size}, words,
                        pickup_context()); // 0x0009E9F0
                };
            }
            children_[child->bytes.get()] = lift.get();
            continue;
        }
        // 0x59, 0x74, 0x7C and 0x8A have the empty 0x001628B0 message slot (0x59's getter 0x0007FCE0 stores it at
        // 0x005054C4, 0x8A's getter 0x000A34C0 at 0x00505C2C).
        // 0x80's getter 0x0009F420 stores it at 0x00505AEC, 0x63's getter 0x0008C5A0 at 0x0050574C.
        if (child->type != 0x59 && child->type != 0x8A && child->type != 0x80 && child->type != 0x63
            && child->type != 0x7F && child->type != 0x6B
            && child->type != 0x77 && child->type != 0x6E && child->type != 0x6D && child->type != 0x81
            && child->type != 0x7B
            && child->type != 0x5A && child->type != 0x5B && child->type != 0x5C && child->type != 0x74
            && child->type != 0x7C && child->type != 0x87 && child->type != 0x88 && child->type != 0x69
            && child->type != 0x70)
        {
            unbound("body action message slot of factory type " + std::to_string(child->type));
        }
        children_[child->bytes.get()] = &empty_child_;
    }
}

void SceneBodyMessages::dispatch(FactoryObject& body, std::span<u32> message)
{
    bind_action_children();
    body_view_ = player_.body;
    mask_ = EntityMaskHost{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
    processor_.attributes = player_.body_attributes; // Body +0x268.
    refresh_damage_rows();
    const u32 request = read_u32(player_.body, 0x84);
    hit_view_ = request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24);
    // 0x0001A290 reads body +0x48: the held weapon and its +0x998 collision state.
    attached_shapes_.reset();
    animation_.attached_shapes = nullptr;
    if (const u32 held = read_u32(player_.body, 0x48); held != 0)
    {
        if (!services_.weapon_attachment_at || !services_.weapon_collision)
        {
            unbound("body +0x48 command views without the scene weapon views");
        }
        attached_shapes_.emplace(RuntimeBodyCommandAttachment{services_.weapon_attachment_at(held).object,
            services_.weapon_collision(held)});
        animation_.attached_shapes = &*attached_shapes_;
    }
    const u32 id = message[0];
    // Ids whose handlers read the animation, source or contact views built from message pointers.
    switch (id)
    {
    case 0x16:
    case 0x2A:
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
    std::optional<RuntimeBodyAttachedActor> impact_source;
    std::optional<RuntimeBodyContactHitViews> impact;
    if (id == 7)
    {
        views.parent.animation_command = services_.event_at(message[3]);
    }
    if (id == 0x63)
    {
        // {0x63, cylinder} from 0x0013D400: the bound lies in the message words from +4.
        views.parent.cylinder = reinterpret_cast<const EntityCylinderBound*>(message.data() + 1);
    }
    if (id == 0x45)
    {
        // {0x45, reply, reply, sender, direction, contact, flag << 8 | bone << 16} from 0x0009BD40.
        views.source = &services_.source_at(message[3]);
        views.source_body = services_.source_body_at(message[3]);
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
    if (id == 0x17)
    {
        // {0x17, reply, sender, contacts, point} from a knock-down sphere (0x0006EC10); the contacts are a copied
        // segment pool. 0x00023580 reads only the contacts, their address and the sender, so the source-contact
        // fields 0x00020020 uses stay empty.
        const auto& pool = *reinterpret_cast<const GhoulySpawnPool*>(services_.event_at(message[3]).data());
        contact_pool.emplace(entity_detail::SegmentPool{pool.stride, pool.grow, pool.begin, pool.cursor, pool.limit});
        auto& source = services_.source_at(message[2]);
        source_contact.emplace(RuntimeBodySourceContact{
            RuntimeBodyContactHitViews{*contact_pool, message[3], source, nullptr}, {}, {}, {}, player_.ring,
            services_.address_zero, player_.self, damage_, damage_});
        views.source_contact = &*source_contact;
    }
    if (id == 0x1D || id == 0x18)
    {
        // {0x1D, kind, strength, style, reply, weapon, velocity, contacts, point} from a weapon's contact test
        // (0x00141890), or 0x18 in the same layout from a spray (0x00141C00); the contacts are a copied segment
        // pool. 0x000240C0 reads the weapon's holder (+0xD8C) and the holder's body +0x84.
        const auto& pool = *reinterpret_cast<const GhoulySpawnPool*>(services_.event_at(message[9]).data());
        contact_pool.emplace(entity_detail::SegmentPool{pool.stride, pool.grow, pool.begin, pool.cursor, pool.limit});
        if (!services_.weapon_attachment_at)
        {
            unbound("weapon contact 0x1D without the scene weapon views");
        }
        auto& weapon = services_.weapon_attachment_at(message[5]).object;
        if (weapon.host == nullptr)
        {
            unbound("weapon contact 0x1D from a weapon with no holder at +0xD8C");
        }
        const u32 holder = weapon.host->address;
        const auto holder_body = services_.source_body_at(holder);
        const u32 request = read_u32(holder_body, 0x84);
        std::span<const u8> bound_view;
        if (live_attachment_ != nullptr)
        {
            std::memcpy(target_bound.data() + 0x20, live_attachment_->shape.data() + 4, 12);
            bound_view = target_bound;
        }
        source_contact.emplace(RuntimeBodySourceContact{
            RuntimeBodyContactHitViews{*contact_pool, message[9], weapon, nullptr},
            {const_cast<u8*>(holder_body.data()), holder_body.size()},
            request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24), bound_view, player_.ring,
            services_.address_zero, player_.self, damage_, services_.damage_at(holder)});
        views.source_contact = &*source_contact;
    }
    if (id == 0x19)
    {
        // {0x19, kind, strength, style, reply, 0, 0, weapon, velocity, contacts, point, quiet} from the large
        // projectile's contact test (0x00141A30); the contacts are a copied segment pool and the weapon is the sender.
        const auto& pool = *reinterpret_cast<const GhoulySpawnPool*>(services_.event_at(message[0xB]).data());
        contact_pool.emplace(entity_detail::SegmentPool{pool.stride, pool.grow, pool.begin, pool.cursor, pool.limit});
        if (!services_.weapon_attachment_at)
        {
            unbound("projectile contact 0x19 without the scene weapon views");
        }
        auto& weapon = services_.weapon_attachment_at(message[7]).object;
        impact.emplace(RuntimeBodyContactHitViews{*contact_pool, message[0xB], weapon, nullptr});
        views.impact = &*impact;
    }
    if (id == 0x1B)
    {
        // {0x1B, reply, sender, value, contacts, point} from a contact prop; the contacts are a copied segment pool.
        const auto& pool = *reinterpret_cast<const GhoulySpawnPool*>(services_.event_at(message[4]).data());
        contact_pool.emplace(entity_detail::SegmentPool{pool.stride, pool.grow, pool.begin, pool.cursor, pool.limit});
        const u32 sender = message[2];
        const auto sender_bytes = services_.bytes_at(sender);
        impact_source.emplace(RuntimeBodyAttachedActor{{const_cast<u8*>(sender_bytes.data()), sender_bytes.size()},
            sender,
            [this, sender](std::span<const u32> words)
            {
                services_.message(sender, words);
            },
            nullptr, services_.actor_at(sender)});
        impact.emplace(RuntimeBodyContactHitViews{*contact_pool, message[4], *impact_source, nullptr});
        views.impact = &*impact;
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
    case 0x2D: // Vtable 0x00505368 +0xC, 0x00075A60, jumps to the parent's 0x00074080.
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
// 0x73 (0x00505940), 0x74 (0x00505960), 0x87 (0x00505BC0), 0x8E (0x00505CA0) and 0x8F (0x00505CC0).
void SceneBodyMessages::action_tick(FactoryObject& action, u32 input)
{
    switch (action.type)
    {
    case 0x5A:
        tick_runtime_xz_gravity_action({action.bytes.get(), action.size}, player_.body, player_.actor,
            services_.clock);
        return;
    case 0x5C:
        tick_runtime_velocity_action({action.bytes.get(), action.size}, player_.body, player_.actor,
            services_.clock, services_.motion_steps);
        return;
    case 0x5B:
    case 0x59:
        // Both classes' +0x10 is 0x0007FD70.
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
            [this]()
            {
                settle_runtime_body_turn(player_.body, player_.actor); // 0x00134720, ESI body
            }};
        tick_runtime_stand_alert({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x69:
        tick_runtime_player_death_state({action.bytes.get(), action.size}, player_death_context()); // 0x0008F290
        return;
    case 0x88:
    {
        RuntimeStruggleTick tick{player_.body, player_.actor, player_.actor_bytes, *player_.body_bits,
            player_.body_selection, services_.clock, player_.input_device, player_.scene.input_state, player_.mind,
            [this]() -> u16
            {
                const RuntimeBodyActionIds ids{player_.attached, read_u16(player_.actor_bytes, 6),
                    services_.address_of(player_.body.data())};
                return runtime_body_damage_gated_id(ids, player_.action_table);
            },
            [this](u32 address) -> Vec3
            {
                Actor* target = services_.actor_at ? services_.actor_at(address) : nullptr;
                if (target == nullptr)
                {
                    unbound("struggle target actor view (mind +0xC)");
                }
                return actor_world_position(*target);
            }};
        tick_runtime_struggle_state({action.bytes.get(), action.size}, tick);
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
            [this]()
            {
                settle_runtime_body_turn(player_.body, player_.actor); // 0x00134720, ESI body
            }};
        tick_runtime_walk_alert({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x6D:
    {
        // EBX of 0x00091210 is the tick's input word: the player's input record (actor +0x960) or zero.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("fly input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        RuntimeWalkAlertTick tick{player_.body, player_.actor, *player_.body_bits, player_.body_selection,
            services_.clock, device, player_.scene.input_state, services_.damage_tables, services_.mode_scales,
            !player_.mind.empty(), player_.mind.empty() ? u8(0) : player_.mind[0x20], {}};
        tick_runtime_fly_state({action.bytes.get(), action.size}, tick); // 0x00091190
        return;
    }
    case 0x8B:
        tick_runtime_throw_state({action.bytes.get(), action.size}, throw_context()); // 0x000A3990
        return;
    case 0x66:
        tick_runtime_arc_state({action.bytes.get(), action.size}, arc_context()); // 0x0008DC70
        return;
    case 0x79:
        tick_runtime_jump_on_state({action.bytes.get(), action.size}, jump_on_context()); // 0x0009AB00
        return;
    case 0x8D:
        tick_runtime_walk_state({action.bytes.get(), action.size}, walk_context()); // 0x000A5100
        return;
    case 0x5F:
        tick_runtime_arc_attack_state({action.bytes.get(), action.size}, arc_attack_context()); // 0x00089060
        return;
    case 0x6F:
        tick_runtime_generate_ghouly_state({action.bytes.get(), action.size}, generate_ghouly_context()); // 0x00093EC0
        return;
    case 0x80:
        tick_runtime_shocked_state({action.bytes.get(), action.size}, shocked_context()); // 0x0009F5F0
        return;
    case 0x8C:
        tick_runtime_transform_state({action.bytes.get(), action.size}, transform_context()); // 0x000A46D0
        return;
    case 0x63:
        tick_runtime_death_state({action.bytes.get(), action.size}, death_context()); // 0x0008C7E0
        return;
    case 0x7F:
        tick_runtime_scrabble_state({action.bytes.get(), action.size}, scrabble_context()); // 0x0009F170
        return;
    case 0x7E:
        tick_runtime_scoop_whack_state({action.bytes.get(), action.size}, scoop_whack_context()); // 0x0009EC90
        return;
    case 0x6B:
        tick_runtime_flip_state({action.bytes.get(), action.size}, flip_context(input)); // 0x00090760
        return;
    case 0x77:
        tick_runtime_hover_state({action.bytes.get(), action.size}, hover_context(input)); // 0x00097CF0
        return;
    case 0x6A:
        tick_runtime_flamethrower_state({action.bytes.get(), action.size}, flamethrower_context()); // 0x0008F8F0
        return;
    case 0x81:
        tick_runtime_shoot_state({action.bytes.get(), action.size}, shoot_context(input)); // 0x0009FA80
        return;
    case 0x76:
        tick_runtime_hit_stream_state({action.bytes.get(), action.size}, hit_stream_context()); // 0x00097500
        return;
    case 0x84:
        tick_runtime_endure_state({action.bytes.get(), action.size}, endure_context()); // 0x000A1710
        return;
    case 0x89:
        tick_runtime_stunned_state({action.bytes.get(), action.size}, stunned_context()); // 0x000A3380
        return;
    case 0x67:
        tick_runtime_drone_dance_state({action.bytes.get(), action.size}, drone_dance_context()); // 0x0008E130
        return;
    case 0x7B:
        tick_runtime_look_for_weapon_state({action.bytes.get(), action.size}, look_for_weapon_context()); // 0x0009DB30
        return;
    case 0x6E:
    {
        // EBX of 0x000923B0 is the tick's input word: the player's input record (actor +0x960) or zero.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("fly-alert input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        player_.refresh_body_views();
        if (player_.body_bits == nullptr)
        {
            unbound("fly-alert state without the body +0x15C bit block");
        }
        RuntimeWalkTargetTick tick{player_.body, player_.actor, player_.body_selection, services_.clock, device,
            player_.scene.input_state, services_.damage_tables, services_.mode_scales, player_.mind,
            [this](u32 address) -> Vec3
            {
                Actor* target = services_.actor_at ? services_.actor_at(address) : nullptr;
                if (target == nullptr)
                {
                    unbound("fly-alert mind target actor view (mind +0xC)");
                }
                return actor_world_position(*target);
            }};
        tick_runtime_fly_alert({action.bytes.get(), action.size}, tick, player_.body_bits->bytes[0]); // 0x00091E10
        return;
    }
    case 0x83:
        tick_runtime_slide_state({action.bytes.get(), action.size}, slide_context()); // 0x000A0F90
        return;
    case 0x78:
        tick_runtime_swing_state({action.bytes.get(), action.size}, swing_context()); // 0x00098460
        return;
    case 0x8A:
    {
        RuntimeHoldStateTick tick{player_.body, player_.actor, player_.actor_address,
            services_.clock, recovery_action()};
        tick_runtime_hold_state({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x8F:
    {
        // 0x000A6520 takes only the state; 0x00019840 reads the stick through actor +0x960 itself.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (read_u32(player_.actor_bytes, 0x960) != 0)
        {
            device = &player_.input_device;
        }
        RuntimeWalkTargetTick tick{player_.body, player_.actor, player_.body_selection, services_.clock, device,
            player_.scene.input_state, services_.damage_tables, services_.mode_scales, player_.mind,
            [this](u32 address) -> Vec3
            {
                Actor* target = services_.actor_at ? services_.actor_at(address) : nullptr;
                if (target == nullptr)
                {
                    unbound("walk-target mind target actor view (mind +0xC)");
                }
                return actor_world_position(*target);
            }};
        tick_runtime_walk_target({action.bytes.get(), action.size}, tick);
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
        const RuntimeBodyCollisionState* attached_collision = nullptr;
        if (attached != 0)
        {
            if (!services_.weapon_collision)
            {
                unbound("hit state attached actor +0x998 collision view");
            }
            attached_collision = services_.weapon_collision(attached); // Body +0x48 -> +0x998.
        }
        ActorTrailContext trails{services_.factory, services_.objects, player_.actor_address, services_.pause,
            services_.frame_delta, services_.time_scale, services_.pal, services_.pal_override,
            services_.empty_name};
        RuntimeHitStateTick tick{player_.body, player_.actor, player_.actor_bytes, player_.body_flags,
            player_.actor_instance.animation, services_.clock, services_.motion_steps, services_.default_position,
            mind_target(), hit_address == 0 ? std::span<const u8>() : services_.record_at(hit_address, 0x24),
            player_.body_attributes, player_.collision, attached_collision, player_.actor_instance.render, trails, input,
            device,
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
        RuntimeHitFurnaceTick tick{player_.body, player_.actor, player_.actor_bytes,
            player_.actor_instance.animation, services_.clock, player_.body_attributes, device,
            player_.scene.input_state, services_.address_zero,
            []()
            {
                unbound("furnace hit scorch 0x000965C0");
            },
            recovery_action()};
        tick_runtime_hit_furnace_state({action.bytes.get(), action.size}, tick);
        return;
    }
    case 0x5E:
    {
        // The tick's second argument is the player's input record (actor +0x960) or zero, as for 0x73.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("anim state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        auto& context = anim_context();
        context.input = device;
        tick_runtime_anim_state({action.bytes.get(), action.size}, context);
        return;
    }
    case 0x62:
        tick_runtime_die_state({action.bytes.get(), action.size}, die_context());
        return;
    case 0x64:
    {
        // The tick's second argument is the player's input record (actor +0x960) or zero, as for 0x73.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("dizzy state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        auto& context = dizzy_context();
        context.input = device;
        tick_runtime_dizzy_state({action.bytes.get(), action.size}, context);
        return;
    }
    case 0x86:
    {
        // The tick's second argument goes on to 0x0008CC40, as for 0x64.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("type 0x86 state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        auto& context = spin_dizzy_context();
        context.dizzy.input = device;
        tick_runtime_spin_dizzy_state({action.bytes.get(), action.size}, context); // 0x000A23A0
        return;
    }
    case 0x7A:
        tick_runtime_knockdown_state({action.bytes.get(), action.size}, knockdown_context());
        return;
    case 0x70:
    case 0x7C:
    case 0x7D:
    {
        // The tick's second argument is the player's input record (actor +0x960) or zero, as for 0x5E.
        const RuntimeDialogueInputDevice* device = nullptr;
        if (input != 0)
        {
            if (player_.input.empty() || input != services_.address_of(player_.input.data()))
            {
                unbound("pick-up state input word that is not this player's input record");
            }
            device = &player_.input_device;
        }
        auto& context = pickup_context();
        context.input = device;
        if (action.type == 0x70)
        {
            RuntimeGrabbedStateContext grabbed{context, recovery_action(), hold_timer()};
            tick_runtime_grabbed_state({action.bytes.get(), action.size}, grabbed); // 0x00094530
            return;
        }
        if (action.type == 0x7D)
        {
            tick_runtime_lift_state({action.bytes.get(), action.size}, context); // 0x0009E6B0
            return;
        }
        tick_runtime_pickup_state({action.bytes.get(), action.size}, context);
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
    case 0x5A:
        // Type 0x5A's +0x14 is 0x0002AD30 itself.
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
    case 0x59:
        // Xbox 0x0007FDD0: 0x0002AE90 with both stack words 1.
        step_runtime_gravity_motion(player_.body, player_.actor_bytes, player_.actor,
            player_.actor_instance.animation, services_.clock, *player_.body_bits, services_.damage_tables,
            services_.movement_phases, services_.movement_ratio_scale, services_.backward_motion_lower,
            services_.reverse_turn_threshold, 1, 1);
        return;
    case 0x5D:
    {
        auto context = stand_context();
        step_runtime_stand_action({action.bytes.get(), action.size}, context, player_.actor_instance.animation);
        return;
    }
    case 0x5E:
    case 0x62:
    case 0x63:
    case 0x64:
    case 0x73:
    case 0x74:
    case 0x7A:
    case 0x7C:
    case 0x7D:
    case 0x69:
    case 0x70:
    case 0x87:
    case 0x88:
    case 0x8A:
    case 0x8B:
    case 0x8E:
    case 0x8F:
    case 0x66:
    case 0x78:
    case 0x5F:
    case 0x6F:
    case 0x80:
    case 0x7F:
    case 0x7E:
    case 0x6B:
    case 0x77:
    case 0x6E:
    case 0x6A:
    case 0x6D:
    case 0x81:
    case 0x76:
    case 0x7B:
    case 0x83:
    case 0x67:
    case 0x79:
    case 0x8D:
    case 0x84:
    case 0x86:
    case 0x89:
    case 0x8C:
        // 0x001628B0 (0x000D4179 for 0x69, 0x70 and 0x88) returns at once.
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

RuntimeBodyRecoveryAction& SceneBodyMessages::recovery_action()
{
    recovery_.emplace(RuntimeBodyRecoveryAction{player_.body, player_.body_attributes, player_.action_table,
        first_timer_, player_.ring, read_u16(player_.actor_bytes, 0x990), services_.address_zero,
        [this](u32& output)
        {
            // 0x00028360: the front goal's kind in AX (0 without a ring; an empty ring reads address zero).
            const RuntimePlayerGoalRing* ring = player_.ring;
            u16 kind = 0;
            if (ring != nullptr)
            {
                kind = ring->tail == ring->capacity ? read_u16(services_.address_zero, 0)
                    : read_u16(ring->rows, size_t(ring->head) * ring->stride);
            }
            select_runtime_goal_kind_action(kind, output, player_.strategy_goals); // 0x00135790
        },
        [this](u32& output)
        {
            select_runtime_player_goal_action(output, player_.strategy_goals); // 0x00135540
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
        }});
    return *recovery_;
}

RuntimeKnockdownContext& SceneBodyMessages::knockdown_context()
{
    // Body views only: a state destroyed with its player runs after the player's input device is released.
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("knockdown state without the body +0x15C bit block");
    }
    knockdown_shape_.emplace(services_.state_context(services_.address_of(player_.body.data())));
    knockdown_.emplace(RuntimeKnockdownContext{player_.body, player_.actor, player_.actor_bytes,
        read_u32(player_.body, 8), *player_.body_flags, player_.actor_instance.animation, services_.clock,
        services_.motion_steps, services_.default_position, *knockdown_shape_, player_.body_selection,
        player_.attachment, player_.callout, player_.actor_instance.startup.resource.resource.data(),
        player_.actor_instance.live_attachments, services_.address_of, player_.body_lists, death_,
        services_.game, services_.scene_flags, services_.restart_requested, player_.body_attributes,
        player_.action_table, services_.random, services_.tag_rows, services_.hit_bands, services_.hit_damage,
        services_.play_bounded, services_.play_bounds, services_.image_string,
        [this](u32 id, const RuntimeActorCreationOptions* options)
        {
            services_.actor_callouts_with(read_u32(player_.body, 8), id, options);
        },
        [this](std::span<const u32> message)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), buffer.size()), buffer.begin());
            player_.self.message(buffer);
        },
        services_.probe_messages, services_.record_at,
        [this](u32 actor) -> Vec3
        {
            if (const Vec3* last = services_.released_position ? services_.released_position(actor) : nullptr;
                last != nullptr)
            {
                return *last;
            }
            Actor* other = services_.actor_at(actor);
            if (other == nullptr)
            {
                unbound("knockdown contact actor has no scene view");
            }
            return actor_world_position(*other); // 0x000412A0
        },
        [this](u32 actor, std::span<u32> message)
        {
            services_.message(actor, message);
        },
        [this]()
        {
            hold_runtime_body_latches(player_.body, hold_timer()); // 0x00027BE0
        }});
    return *knockdown_;
}

RuntimeThrowStateContext& SceneBodyMessages::throw_context()
{
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("throw state without the body +0x15C bit block");
    }
    const u32 hit = read_u32(player_.body, 0x84);
    const RuntimeDialogueInputDevice* own = read_u32(player_.actor_bytes, 0x960) != 0 ? &player_.input_device
                                                                                       : nullptr;
    throw_.reset();
    throw_.emplace(RuntimeThrowStateContext{player_.body, player_.actor, player_.actor_bytes, *player_.body_flags,
        player_.actor_instance.animation, services_.clock, player_.actor_instance.live_attachments,
        player_.attachment, hit == 0 ? std::span<const u8>() : services_.record_at(hit, 0x24), mind_target(),
        [this](u32 address) -> const Actor&
        {
            Actor* actor = services_.actor_at ? services_.actor_at(address) : nullptr;
            if (actor == nullptr)
            {
                unbound("throw state target record +0x10 actor view");
            }
            return *actor;
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        own, player_.scene.input_state, services_.random});
    return *throw_;
}

RuntimeSwingStateContext& SceneBodyMessages::swing_context()
{
    if (!services_.marks || !services_.cutscenes || services_.door_names == nullptr || !services_.heap_address
        || !services_.weapon_collision)
    {
        unbound("swing state without the scene's actor list, cutscene stack, door names or weapon views");
    }
    auto& pickup = pickup_context();
    swing_shape_.reset();
    swing_shape_.emplace(services_.state_context(services_.address_of(player_.body.data())));
    swing_next_.reset();
    swing_next_.emplace(RuntimeBodyNextAction{player_.body, player_.body_attributes, player_.action_table, first_timer_,
        [this]()
        {
            return body_slot_key(0x20);
        },
        [this]()
        {
            return body_slot_key(0x2C);
        }});
    swing_.reset();
    swing_.emplace(RuntimeSwingStateContext{pickup, *swing_shape_, *swing_next_,
        [this](std::span<const u32> words)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            dispatch(player_.body_object, buffer); // Body vtable +0xC.
        },
        services_.weapon_collision, services_.marks,
        [this](u32 actor)
        {
            return ActorTrailContext{services_.factory, services_.objects, actor, services_.pause,
                services_.frame_delta, services_.time_scale, services_.pal, services_.pal_override,
                services_.empty_name};
        },
        services_.effects, services_.sounds, services_.cutscenes, services_.effects.models.registry,
        services_.door_names->room, services_.door_names->aliases, services_.image_string, services_.record_at,
        services_.actor_at, services_.heap_address, services_.random});
    return *swing_;
}

RuntimeArcStateContext& SceneBodyMessages::arc_context()
{
    player_.refresh_body_views();
    arc_.reset();
    arc_.emplace(RuntimeArcStateContext{player_.body, player_.actor, player_.actor_instance.animation,
        player_.body_selection, services_.clock, player_.ring, player_.actor_instance.startup.resource,
        player_.callout, player_.body_lists, services_.latches, services_.image_string,
        [this](std::span<const u32> words)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            dispatch(player_.body_object, buffer); // Body vtable +0xC.
        },
        [this](std::span<const u32> words)
        {
            services_.message(player_.actor_address, words);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        services_.address_of});
    return *arc_;
}

RuntimeWalkStateContext& SceneBodyMessages::walk_context()
{
    player_.refresh_body_views();
    if (player_.body_flags == nullptr || player_.body_bits == nullptr)
    {
        unbound("walk state without the body +0x15C bit block");
    }
    dizzy_stars_.create = [this](std::span<const u8> parameters)
    {
        return services_.create_object(parameters);
    };
    dizzy_stars_.actor = [this](u32 address) -> Actor&
    {
        return services_.actor_view(address).bat.actor;
    };
    dizzy_stars_.animation = [this](u32 address) -> ActorAnimationState&
    {
        return services_.actor_view(address).animation;
    };
    dizzy_stars_.render = [this](u32 address) -> ActorRenderRegistration&
    {
        return services_.actor_view(address).render;
    };
    dizzy_stars_.message = [this](u32 address, std::span<u32> message)
    {
        services_.message(address, message);
    };
    walk_.reset();
    walk_.emplace(RuntimeWalkStateContext{player_.body, player_.actor, *player_.body_flags, services_.clock,
        player_.strategy, dizzy_stars_, services_.default_position, services_.default_euler, services_.empty_name,
        services_.random,
        [this]()
        {
            // 0x00026950 on the body; the caller's ECX is read only when the attached class leaves the pair unset.
            const RuntimeBodyActionIds ids{player_.attached, read_u16(player_.actor_bytes, 6),
                services_.address_of(player_.body.data())};
            const u8* mind = player_.mind.empty() ? nullptr : player_.mind.data();
            return runtime_body_primary_parameter_id(ids, mind, *player_.body_bits, player_.action_table, 0);
        }});
    return *walk_;
}

RuntimeJumpOnStateContext& SceneBodyMessages::jump_on_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("jump on/off state of a body with no +0x15C bit block");
    }
    if (services_.intro == nullptr || services_.story_byte == nullptr || !services_.jump_peer
        || !services_.reply_message || services_.jump_globals.empty())
    {
        unbound("jump on/off state services");
    }
    jump_on_.reset();
    jump_on_.emplace(RuntimeJumpOnStateContext{player_.body, player_.actor, player_.actor_address,
        player_.actor_bytes, *player_.body_bits, player_.body_selection, services_.clock,
        player_.actor_instance.animation, player_.mind, player_.actor_instance.live_attachments, services_.latches,
        spatial_, services_.state_context(services_.address_of(player_.body.data())), services_.sounds,
        services_.jump_globals, services_.default_position, services_.default_euler, services_.random,
        services_.scene_flags, services_.player_blocked, services_.player_modes, services_.player_mode,
        *services_.intro, *services_.story_byte, player_.scene.input_state, services_.occupancy_initial,
        services_.image_string,
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        [this](std::span<const u32> words)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(words.begin(), std::min<size_t>(words.size(), buffer.size()), buffer.begin());
            dispatch(player_.body_object, buffer); // Body vtable +0xC.
        },
        services_.reply_message,
        [this](u32 actor, u32 id)
        {
            services_.actor_callouts_with(actor, id, nullptr);
        },
        services_.target_at, services_.jump_peer, services_.address_of});
    return *jump_on_;
}

RuntimeArcAttackStateContext& SceneBodyMessages::arc_attack_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("arc attack state without the body +0x15C bit block");
    }
    arc_attack_shape_.reset();
    arc_attack_shape_.emplace(services_.state_context(services_.address_of(player_.body.data())));
    arc_attack_.reset();
    arc_attack_.emplace(RuntimeArcAttackStateContext{player_.body, player_.actor, player_.actor_address,
        player_.actor_instance.animation, player_.body_selection, services_.clock, *player_.body_bits,
        *arc_attack_shape_, player_.collision, player_.mind, services_.target_at, services_.actor_at,
        services_.bytes_at, player_.actor_instance.startup.resource, player_.callout, services_.latches,
        services_.sounds,
        ActorTrailContext{services_.factory, services_.objects, player_.actor_address, services_.pause,
            services_.frame_delta, services_.time_scale, services_.pal, services_.pal_override,
            services_.empty_name},
        services_.damage_tables, services_.player_damage, services_.player_blocked, services_.player_modes,
        services_.player_mode, services_.image_string,
        [this](std::span<u32> words)
        {
            services_.message(player_.actor_address, words);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        services_.random});
    return *arc_attack_;
}

RuntimeGenerateGhoulyContext& SceneBodyMessages::generate_ghouly_context()
{
    player_.refresh_body_views();
    if (!services_.generate_ghouly)
    {
        unbound("generate ghouly state without the scene's light, display and actor list views");
    }
    generate_ghouly_.reset();
    generate_ghouly_.emplace(RuntimeGenerateGhoulyContext{services_.generate_ghouly(), player_.body, player_.actor,
        player_.actor_bytes, player_.actor_instance.animation, player_.body_selection, services_.clock,
        player_.actor_instance.live_attachments, hold_timer(), services_.sounds, services_.random,
        services_.occupancy_initial, services_.resources, services_.factory, services_.default_position,
        services_.default_euler, player_.spawn.record, services_.empty_name, services_.image_string,
        services_.actor_at,
        [this](u32 actor, std::span<const u32> words)
        {
            services_.message(actor, words);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    return *generate_ghouly_;
}

RuntimeShockedStateContext& SceneBodyMessages::shocked_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("shocked state without the body +0x15C bit block");
    }
    if (!services_.release_frame_object)
    {
        unbound("shocked state without the scene's 0x0003DE30 frame object release");
    }
    shocked_.reset();
    shocked_.emplace(RuntimeShockedStateContext{player_.body, player_.actor, player_.actor_address,
        *player_.body_bits, player_.attachment, services_.latches, services_.clock,
        player_.actor_instance.animation, services_.resources, services_.create_object,
        services_.release_frame_object, recovery_action()});
    return *shocked_;
}

RuntimeTransformStateContext& SceneBodyMessages::transform_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("transform state without the body +0x15C bit block");
    }
    auto& knockdown = knockdown_context();
    const u32 body_address = services_.address_of(player_.body.data());
    transform_.reset();
    transform_.emplace(RuntimeTransformStateContext{player_.body, player_.actor, player_.actor_address,
        player_.actor_bytes, *player_.body_bits, player_.actor_instance.animation, player_.actor_instance.render,
        player_.body_selection,
        [this, body_address]()
        {
            return services_.state_context(body_address);
        },
        services_.clock, services_.sounds, services_.resources, services_.create_object, services_.actor_view,
        services_.message,
        [this](i16 key) -> u16
        {
            SceneActorServices* actors = script_scene_actors(services_.game);
            if (actors == nullptr)
            {
                unbound("transform state's 0x0011F2E0 without the scene's marker records");
            }
            return find_scene_spawn_type(actors->marker_records(), key); // 0x0011F2E0
        },
        services_.image_string, services_.node_states, knockdown.death, knockdown.scene_flags,
        knockdown.restart_requested, services_.random});
    return *transform_;
}

RuntimeScrabbleStateContext& SceneBodyMessages::scrabble_context()
{
    player_.refresh_body_views();
    scrabble_.reset();
    scrabble_.emplace(RuntimeScrabbleStateContext{player_.body, player_.actor, player_.actor_bytes, player_.mind,
        player_.actor_instance.animation, services_.clock, services_.damage_tables, services_.mode_scales,
        services_.random,
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    return *scrabble_;
}

RuntimeSlideStateContext& SceneBodyMessages::slide_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("slide state without the body +0x15C bit block");
    }
    slide_.reset();
    slide_.emplace(RuntimeSlideStateContext{player_.body, player_.actor, player_.actor_address, player_.actor_bytes,
        player_.body_selection, services_.clock, *player_.body_bits, player_.collision, services_.latches,
        services_.sounds,
        ActorTrailContext{services_.factory, services_.objects, player_.actor_address, services_.pause,
            services_.frame_delta, services_.time_scale, services_.pal, services_.pal_override,
            services_.empty_name},
        services_.damage_tables, services_.mode_scales, services_.image_string,
        [this](u32 actor, u32 id, const RuntimeActorCreationOptions* options)
        {
            services_.actor_callouts_with(actor, id, options);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    return *slide_;
}

RuntimeFlamethrowerStateContext& SceneBodyMessages::flamethrower_context()
{
    player_.refresh_body_views();
    if (!services_.contact_actor || !services_.contact_words)
    {
        unbound("flamethrower state without the scene's segment probe actors");
    }
    flamethrower_.reset();
    flamethrower_.emplace(RuntimeFlamethrowerStateContext{player_.body, player_.actor, services_.clock, player_.mind,
        services_.target_at, services_.effects, services_.sounds,
        [this]()
        {
            return body_slot_key(0x20);
        },
        services_.contact_actor, services_.contact_players,
        [this](RuntimeContactReport& report)
        {
            // {0x41, word, velocity, object, contacts, point, reply}.
            std::array<u32, 0x42> words{};
            words[0] = report.id;
            words[1] = u32(report.word);
            words[2] = std::bit_cast<u32>(report.velocity.x);
            words[3] = std::bit_cast<u32>(report.velocity.y);
            words[4] = std::bit_cast<u32>(report.velocity.z);
            words[5] = services_.address_of(&report.object->actor);
            words[7] = std::bit_cast<u32>(report.point.x);
            words[8] = std::bit_cast<u32>(report.point.y);
            words[9] = std::bit_cast<u32>(report.point.z);
            words[10] = report.reply;
            services_.contact_words(player_, words, report.contacts, 6);
        },
        services_.actor_at});
    return *flamethrower_;
}

RuntimeLookForWeaponStateContext& SceneBodyMessages::look_for_weapon_context()
{
    player_.refresh_body_views();
    look_for_weapon_.reset();
    look_for_weapon_.emplace(RuntimeLookForWeaponStateContext{player_.body, player_.actor, services_.clock,
        player_.mind, player_.body_selection, services_.image_string, services_.effects.models.registry,
        player_.body_selection.error, services_.sounds, services_.random, services_.actor_at, services_.bytes_at,
        [this](const Actor& actor)
        {
            return services_.address_of(&actor);
        },
        [this]()
        {
            return body_slot_key(0x20);
        }});
    return *look_for_weapon_;
}

RuntimeDroneDanceStateContext& SceneBodyMessages::drone_dance_context()
{
    player_.refresh_body_views();
    drone_dance_.reset();
    drone_dance_.emplace(RuntimeDroneDanceStateContext{player_.body, player_.actor,
        player_.actor_instance.animation, services_.clock, player_.body_selection, services_.random, player_.room,
        services_.image_string, services_.actor_at});
    return *drone_dance_;
}

RuntimeSpinDizzyStateContext& SceneBodyMessages::spin_dizzy_context()
{
    auto& dizzy = dizzy_context();
    const auto range = services_.record_at(0x004286F0, 8);
    const auto rest = services_.record_at(0x0032B218, 0xC);
    spin_dizzy_.reset();
    spin_dizzy_.emplace(RuntimeSpinDizzyStateContext{dizzy, {read_f32(range, 0), read_f32(range, 4)},
        {read_u32(rest, 8), read_u32(rest, 4), read_u32(rest, 0)},
        [this]()
        {
            // 0x00026950 on the body, as the walk state reads it.
            const RuntimeBodyActionIds ids{player_.attached, read_u16(player_.actor_bytes, 6),
                services_.address_of(player_.body.data())};
            const u8* mind = player_.mind.empty() ? nullptr : player_.mind.data();
            return runtime_body_primary_parameter_id(ids, mind, *player_.body_bits, player_.action_table, 0);
        }});
    return *spin_dizzy_;
}

RuntimeStunnedStateContext& SceneBodyMessages::stunned_context()
{
    player_.refresh_body_views();
    stunned_shape_.emplace(services_.state_context(services_.address_of(player_.body.data())));
    stunned_.reset();
    stunned_.emplace(RuntimeStunnedStateContext{player_.body, player_.actor, *stunned_shape_, services_.clock,
        services_.sounds, player_.mind, player_.mind.empty() ? nullptr : &player_.mind_events.anchor,
        [this](std::span<const u32> message)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), buffer.size()), buffer.begin());
            player_.self.message(buffer);
        },
        [this](u32 address, std::span<u32> words)
        {
            services_.message(address, words);
        },
        [this]()
        {
            return u16(body_slot_key(0x20));
        },
        parent_.actions});
    return *stunned_;
}

RuntimeEndureStateContext& SceneBodyMessages::endure_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("type 0x84 state without the body +0x15C bit block");
    }
    endure_.reset();
    endure_.emplace(RuntimeEndureStateContext{player_.body, player_.actor, player_.actor_address,
        player_.actor_instance.animation, player_.body_selection, *player_.body_bits, services_.clock,
        services_.sounds, player_.mind, services_.target_at, services_.resources, services_.create_object,
        [this](u32 address, std::span<u32> words)
        {
            services_.message(address, words);
        },
        services_.image_string, services_.random});
    return *endure_;
}

RuntimeHitStreamStateContext& SceneBodyMessages::hit_stream_context()
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("hit-stream state without the body +0x15C bit block");
    }
    hit_stream_.reset();
    hit_stream_.emplace(RuntimeHitStreamStateContext{player_.body, player_.actor, player_.actor_address,
        player_.actor_bytes, *player_.body_bits, services_.clock, services_.sounds, player_.callout,
        player_.actor_instance.startup.resource.resource.data(), player_.body_lists, services_.resources,
        services_.create_object, services_.release_frame_object, services_.address_of, services_.actor_at,
        [this]() -> bool
        {
            return runtime_player_damage_enabled(player_.actor, services_.player_blocked, services_.player_modes,
                services_.player_mode) != 0; // 0x00028D50
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        first_timer_});
    return *hit_stream_;
}

RuntimeShootStateContext& SceneBodyMessages::shoot_context(u32 input)
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("shoot state without the body +0x15C bit block");
    }
    // EAX of 0x001088D0 in 0x0009FA80 is the tick's input word: the player's input record (actor +0x960) or zero.
    const RuntimeDialogueInputDevice* device = nullptr;
    if (input != 0)
    {
        if (player_.input.empty() || input != services_.address_of(player_.input.data()))
        {
            unbound("shoot input word that is not this player's input record");
        }
        device = &player_.input_device;
    }
    const u32 request = read_u32(player_.body, 0x84);
    shoot_.reset();
    shoot_.emplace(RuntimeShootStateContext{player_.body, player_.actor, *player_.body_bits, services_.clock,
        player_.mind, services_.target_at, request == 0 ? std::span<const u8>() : services_.record_at(request, 0x24),
        player_.body_selection, services_.image_string, read_u32(player_.body, 0x48), services_.message,
        services_.bytes_at,
        [this]() -> u32
        {
            return runtime_mind_engaged_actor(player_.mind.empty() ? nullptr : &player_.mind_events); // 0x00028C50
        },
        device, player_.scene.input_state, services_.sounds,
        [this]()
        {
            return body_slot_key(0x20);
        }});
    return *shoot_;
}

RuntimeHoverStateContext& SceneBodyMessages::hover_context(u32 input)
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("hover state without the body +0x15C bit block");
    }
    // ESI of 0x00097DC0 is the tick's input word: the player's input record (actor +0x960) or zero.
    const RuntimeDialogueInputDevice* device = nullptr;
    if (input != 0)
    {
        if (player_.input.empty() || input != services_.address_of(player_.input.data()))
        {
            unbound("hover input word that is not this player's input record");
        }
        device = &player_.input_device;
    }
    hover_.reset();
    hover_.emplace(RuntimeHoverStateContext{player_.body, player_.actor, *player_.body_bits, player_.body_selection,
        services_.clock, player_.actor_instance.animation, player_.mind, device, player_.scene.input_state,
        services_.actor_at, services_.image_string});
    return *hover_;
}

RuntimeFlipStateContext& SceneBodyMessages::flip_context(u32 input)
{
    player_.refresh_body_views();
    if (player_.body_bits == nullptr)
    {
        unbound("flip state without the body +0x15C bit block");
    }
    // ECX of 0x000908D0 is the tick's input word: the player's input record (actor +0x960) or zero.
    const RuntimeDialogueInputDevice* device = nullptr;
    if (input != 0)
    {
        if (player_.input.empty() || input != services_.address_of(player_.input.data()))
        {
            unbound("flip input word that is not this player's input record");
        }
        device = &player_.input_device;
    }
    flip_.reset();
    flip_.emplace(RuntimeFlipStateContext{player_.body, player_.actor, *player_.body_bits, player_.attachment,
        player_.body_selection, services_.latches, services_.clock, player_.actor_instance.animation, device,
        player_.scene.input_state, services_.default_position, recovery_action(), services_.image_string});
    return *flip_;
}

RuntimeScoopWhackStateContext& SceneBodyMessages::scoop_whack_context()
{
    player_.refresh_body_views();
    scoop_whack_.reset();
    scoop_whack_.emplace(RuntimeScoopWhackStateContext{player_.body, player_.actor, player_.actor_address,
        player_.mind, player_.actor_instance.animation, services_.clock, player_.actor_instance.live_attachments,
        services_.target_at, services_.actor_at, services_.message,
        [this](u32 actor, u32 id, const RuntimeActorCreationOptions* options)
        {
            services_.actor_callouts_with(actor, id, options);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    return *scoop_whack_;
}

RuntimePickupStateContext& SceneBodyMessages::pickup_context()
{
    // Body views only, as for the dizzy state: a state destroyed with its player outlives the input device.
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("pick-up state without the body +0x15C bit block");
    }
    if (!services_.weapon_attachment_at)
    {
        unbound("pick-up state without the scene weapon views");
    }
    // Actor +0x958 is the player's native mind object (mind_present says it is set).
    const std::span<u8> mind = player_.mind_present ? player_.mind : std::span<u8>();
    pickup_.emplace(RuntimePickupStateContext{player_.body, player_.actor, player_.actor_bytes, *player_.body_flags,
        player_.actor_instance.animation, services_.clock, player_.actor_instance.live_attachments,
        player_.attachment, services_.default_position, services_.default_euler, services_.stand_input_threshold,
        mind, services_.weapon_attachment_at,
        [this](u32 weapon, u32 id)
        {
            services_.actor_callouts_with(weapon, id, nullptr);
        },
        [this](std::span<const u32> message)
        {
            std::array<u32, 0x42> buffer{};
            std::copy_n(message.begin(), std::min<size_t>(message.size(), buffer.size()), buffer.begin());
            player_.self.message(buffer);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        },
        services_.game});
    pickup_->input_state = &player_.scene.input_state;
    pickup_->weapon_message = [this](u32 weapon, std::span<const u32> words)
    {
        services_.message(weapon, words); // Vtable +0xC on the caller's words, so replies come back.
    };
    pickup_->latches = &services_.latches;
    pickup_->actor_words = player_.actor_bytes;
    pickup_->address_zero = services_.address_zero;
    return *pickup_;
}

RuntimeDizzyStateContext& SceneBodyMessages::dizzy_context()
{
    // Body views only: a state destroyed with its player runs after the player's input device is released.
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("dizzy state without the body +0x15C bit block");
    }
    dizzy_stars_.create = [this](std::span<const u8> parameters)
    {
        return services_.create_object(parameters);
    };
    dizzy_stars_.actor = [this](u32 address) -> Actor&
    {
        return services_.actor_view(address).bat.actor;
    };
    dizzy_stars_.animation = [this](u32 address) -> ActorAnimationState&
    {
        return services_.actor_view(address).animation;
    };
    dizzy_stars_.render = [this](u32 address) -> ActorRenderRegistration&
    {
        return services_.actor_view(address).render;
    };
    dizzy_stars_.message = [this](u32 address, std::span<u32> message)
    {
        services_.message(address, message);
    };
    dizzy_.emplace(RuntimeDizzyStateContext{player_.body, player_.actor, player_.actor_bytes, *player_.body_flags,
        player_.actor_instance.animation, services_.clock, dizzy_stars_, read_u32(player_.body, 8),
        services_.default_position, services_.default_euler, services_.empty_name, services_.random,
        services_.sounds, recovery_action()});
    dizzy_->input_state = &player_.scene.input_state;
    dizzy_->input_preset = services_.dizzy_input_preset;
    dizzy_->input_scale = services_.dizzy_input_scale;
    return *dizzy_;
}

RuntimeAnimStateContext& SceneBodyMessages::anim_context()
{
    // Body views only, as for the dizzy state: a state destroyed with its player outlives the input device.
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("anim state without the body +0x15C bit block");
    }
    anim_.emplace(RuntimeAnimStateContext{player_.body, player_.actor, *player_.body_flags,
        player_.actor_instance.animation, services_.clock, player_.body_selection, services_.sounds,
        services_.stand_input_threshold, services_.image_string,
        [this]() -> u32
        {
            // Actor +0x958 is the mind; its +0xC is the target.
            const u32 mind = read_u32(player_.actor_bytes, 0x958);
            if (mind == 0)
            {
                return 0;
            }
            if (player_.mind.empty() || mind != services_.address_of(player_.mind.data()))
            {
                unbound("anim state actor +0x958 " + std::to_string(mind) + " is not the player's mind");
            }
            return read_u32(player_.mind, 0xC);
        },
        [this](u32 address) -> const Actor*
        {
            return services_.actor_at(address);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    anim_->input_state = &player_.scene.input_state;
    return *anim_;
}

RuntimeDieStateContext& SceneBodyMessages::die_context()
{
    // Body views only, as for the dizzy state: a state destroyed with its player outlives the input device.
    player_.refresh_body_views();
    if (player_.body_flags == nullptr)
    {
        unbound("die state without the body +0x15C bit block");
    }
    die_.emplace(RuntimeDieStateContext{player_.body, player_.actor, *player_.body_flags,
        player_.actor_instance.animation, services_.clock, player_.body_selection, services_.random,
        entity_detail::g_display_view->position,
        services_.image_string,
        [this]() -> u32
        {
            // Actor +0x958 is the mind; its +0xC is the target.
            const u32 mind = read_u32(player_.actor_bytes, 0x958);
            if (mind == 0)
            {
                return 0;
            }
            if (player_.mind.empty() || mind != services_.address_of(player_.mind.data()))
            {
                unbound("die state actor +0x958 " + std::to_string(mind) + " is not the player's mind");
            }
            return read_u32(player_.mind, 0xC);
        },
        [this](u32 address) -> const Actor*
        {
            return services_.actor_at(address);
        },
        [this](u32 slot)
        {
            return body_slot_key(slot);
        }});
    return *die_;
}

RuntimeBodyTimerObject& SceneBodyMessages::keep_timer(FactoryObject object)
{
    const u8* bytes = object.bytes.get();
    services_.objects.push_back(std::move(object)); // The scene keeps what 0x00107520 made.
    const u32 address = services_.address_of(bytes);
    auto& kept = timers_[bytes];
    kept = std::make_unique<KeptTimer>(RuntimeBodyTimerRecipient{
        [this, address](std::span<const u32> words)
        {
            services_.message(address, words); // The object's vtable +0xC.
        }}, address);
    return kept->view;
}

RuntimeBodyHoldTimerContext& SceneBodyMessages::hold_timer()
{
    if (!hold_timer_)
    {
        hold_timer_creation_.emplace(RuntimeBodyTimerCreation{second_timer_, player_.child,
            [this](FactoryObject object) -> RuntimeBodyTimerObject&
            {
                return keep_timer(std::move(object));
            }});
        hold_timer_.emplace(RuntimeBodyHoldTimerContext{*hold_timer_creation_, player_.actor, services_.latches});
    }
    return *hold_timer_;
}

RuntimeGrabbedStateContext& SceneBodyMessages::grabbed_context()
{
    auto& pickup = pickup_context();
    grabbed_.emplace(RuntimeGrabbedStateContext{pickup, recovery_action(), hold_timer()});
    return *grabbed_;
}

RuntimePlayerDeathContext& SceneBodyMessages::player_death_context()
{
    auto& knockdown = knockdown_context();
    if (services_.camera_pulse == nullptr || !services_.streams || services_.dialogue == nullptr
        || services_.frontend_state == nullptr || !services_.begin_transition)
    {
        unbound("player death state views (camera pulse, streams, dialogue, transition)");
    }
    auto& scene = player_.scene;
    player_death_.emplace(RuntimePlayerDeathContext{knockdown, services_.latches, player_.mind,
        player_.mind.empty() ? nullptr : &player_.mind_events.anchor, *services_.camera_pulse, services_.streams(),
        *services_.dialogue, g_510cec, g_4dea24, g_script_30d94, scene.brews_owned, scene.brews_required,
        *services_.frontend_state, scene.prompt_owner, g_scene_transition.phase, g_5493dc,
        g_runtime_profile.progress.bytes, g_runtime_profile.unlocks.bytes, reinterpret_cast<const char*>(g_510e58),
        services_.begin_transition});
    return *player_death_;
}

RuntimeDeathStateContext& SceneBodyMessages::death_context()
{
    auto& knockdown = knockdown_context();
    death_state_.reset();
    death_state_.emplace(RuntimeDeathStateContext{knockdown, services_.latches, player_.mind,
        player_.mind.empty() ? nullptr : &player_.mind_events.anchor});
    return *death_state_;
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
// 0x00026950 / 0x00026A90 for type 0x27 (getter 0x000733F0 keeps the parent's slots from +0x20), and 0x00076210 /
// 0x00076250 for type 0x2D (getter 0x000761A0), which fall through to the 0x27 parent's. +0x30 and +0x34 are the
// parent's 0x00026B00 / 0x00026BB0 (parent table 0x0001DA60) in all three.
u16 SceneBodyMessages::body_slot_key(u32 slot)
{
    const u32 type = player_.body_object.type;
    if (type != 0x27 && type != 0x2A && type != 0x2D)
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
        if (type == 0x2D)
        {
            return runtime_zombie_body_primary_parameter_id(player_.body, ids, mind, *player_.body_bits,
                player_.action_table, retained);
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
        if (type == 0x2D)
        {
            return runtime_zombie_body_secondary_parameter_id(player_.body, ids, mind, *player_.body_bits,
                player_.action_table, retained);
        }
        return runtime_body_secondary_parameter_id(ids, mind, *player_.body_bits, player_.action_table, retained);
    }
    // No getter (0x000733F0 for 0x27, 0x00075AF0 for 0x2A, 0x000761A0 for 0x2D) replaces these base slots
    // (0x0001DA60).
    if (slot == 0x28)
    {
        return u16(runtime_body_idle_action_id(ids));
    }
    if (slot == 0x38)
    {
        return runtime_body_effect_gated_id(ids, player_.action_table);
    }
    if (slot == 0x3C)
    {
        return runtime_body_contact_gated_id(ids, player_.action_table);
    }
    if (slot == 0x40)
    {
        return runtime_body_damage_gated_id(ids, player_.action_table);
    }
    if (slot == 0x24)
    {
        return runtime_body_primary_gated_id(ids, player_.action_table); // 0x000269C0
    }
    if (slot == 0x44)
    {
        return u16(runtime_body_contact_action_id(ids)); // 0x00027220
    }
    if (slot == 0x48)
    {
        return u16(runtime_body_range_action_id(ids)); // 0x000272D0
    }
    if (slot == 0x50)
    {
        return u16(runtime_body_effect_action_id(ids)); // 0x00027320
    }
    if (slot == 0x54)
    {
        return u16(runtime_body_damage_action_id(ids)); // 0x000273D0
    }
    if (slot == 0x58)
    {
        return u16(runtime_body_attachment_action_id(ids)); // 0x00027480
    }
    if (slot == 0x4C)
    {
        return runtime_body_death_action_key(); // 0x00027500
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
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A && body.type != 0x2D)
    {
        unbound("body vtable +0x10 of factory type " + std::to_string(body.type));
    }
    auto& actions = player_.body_selection;
    if (body.type != 0x28)
    {
        // 0x000744A0 reads body +0x744 and then +0x314 when it reaches each, so a slot the first tick empties or
        // fills is seen by the second; each view tests its slot as it is called.
        RuntimePlayerBodyRelayChild action{[this](u32 input)
        {
            if (player_.body_selection.second.bytes != nullptr)
            {
                action_tick(player_.body_selection.second, input);
            }
        }};
        RuntimePlayerBodyRelayChild attachment{[this](u32 input)
        {
            if (player_.body_selection.first.bytes != nullptr)
            {
                action_tick(player_.body_selection.first, input);
            }
        }};
        RuntimePlayerBodyRelayChild* action_view = &action;
        RuntimePlayerBodyRelayChild* attachment_view = &attachment;
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
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A && body.type != 0x2D)
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
    if (body.type != 0x28 && body.type != 0x27 && body.type != 0x2A && body.type != 0x2D)
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
    // Actor +0x8F8 is the first record on the actor's +0x8F4 list (0x0012AFE0 links each new one at its head). The
    // apply reads that record's retail image (+0x14 mode .. +0x68) and 0x0012B160 rewrites it, so it goes back to
    // the native record afterwards.
    bound_ = {};
    auto* bound_live = static_cast<ActorLiveAttachment*>(player_.actor_instance.live_attachments.next);
    if (bound_live != nullptr)
    {
        const ActorLiveAttachment& live = *bound_live;
        bound_record_.fill(0);
        bound_record_[0x14] = live.mode;
        bound_record_[0x15] = live.retained_15;
        std::memcpy(bound_record_.data() + 0x16, &live.tag, 2);
        bound_record_[0x18] = live.retained_18;
        bound_record_[0x19] = live.flag;
        std::memcpy(bound_record_.data() + 0x1C, live.shape.data(), 0x20);
        std::memcpy(bound_record_.data() + 0x3C, live.secondary_shape.data(), 0x20);
        std::memcpy(bound_record_.data() + 0x5C, &live.origin, 12);
        bound_ = bound_record_;
    }
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
        if (bound_live != nullptr && bound_.data() == bound_record_.data())
        {
            ActorLiveAttachment& live = *bound_live;
            live.flag = bound_record_[0x19];
            std::memcpy(live.shape.data(), bound_record_.data() + 0x1C, 0x20);
            std::memcpy(live.secondary_shape.data(), bound_record_.data() + 0x3C, 0x20);
            std::memcpy(&live.origin, bound_record_.data() + 0x5C, 12);
        }
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
        if (body.type != 0x27 && body.type != 0x2A && body.type != 0x2D)
        {
            unbound("body vtable +0x1C of factory type " + std::to_string(body.type));
        }
        const EntityMaskHost mask{player_.actor.mask_flag, player_.actor.mask_span, player_.actor.mask_mode};
        const i32 floor = i32(read_u32(player_.actor_bytes, 0xCEC));
        RuntimePlayerCollisionAction action{[this](RuntimePlayerCollisionReply& reply)
        {
            const u32 type = player_.body_selection.second.type;
            if (type == 0x8E || type == 0x8F)
            {
                const std::span<u8> words{reinterpret_cast<u8*>(&reply.id), 12}; // id, accepted, handled
                message_runtime_walk_alert(words, [this, words]()
                {
                    stop_runtime_walk_alert(player_.body, words, 1, [this]()
                    {
                        return u16(body_slot_key(0x20));
                    }, parent_.actions); // 0x00023890, ECX 1
                });
                return;
            }
            if (type == 0x7A)
            {
                // 0x0009D0E0 reads the reply's id and, for 0x7C, its next word.
                auto& second = player_.body_selection.second;
                message_runtime_knockdown_state({second.bytes.get(), second.size}, reply, knockdown_context());
                return;
            }
            if (type == 0x8B)
            {
                // 0x000A3D20 reads the reply's id: only 0x25 acts.
                auto& second = player_.body_selection.second;
                message_runtime_throw_state({second.bytes.get(), second.size},
                    {reinterpret_cast<const u8*>(&reply.id), 4}, throw_context());
                return;
            }
            if (type == 0x7D)
            {
                // 0x0009E9F0 reads the reply's id and, for 0x5C and 0x7C, its next word.
                auto& second = player_.body_selection.second;
                const std::array<u32, 2> words{reply.id, read_u32({reinterpret_cast<const u8*>(&reply.id), 8}, 4)};
                message_runtime_lift_state({second.bytes.get(), second.size}, words, pickup_context());
                return;
            }
            if (type == 0x5E)
            {
                // 0x00088960 reads the reply's id: only 0x25 acts.
                auto& second = player_.body_selection.second;
                message_runtime_anim_state({second.bytes.get(), second.size},
                    {reinterpret_cast<const u8*>(&reply.id), 8}, anim_context());
                return;
            }
            if (type == 0x62)
            {
                // 0x0008C4D0 reads the reply's id and, for 0x7C, its next word.
                auto& second = player_.body_selection.second;
                message_runtime_die_state({second.bytes.get(), second.size}, {reinterpret_cast<const u8*>(&reply.id), 8});
                return;
            }
            if (type == 0x89)
            {
                // 0x000A3460 reads the reply's id; for 0x53 0x00023890 reads +4 and writes +8.
                auto& second = player_.body_selection.second;
                message_runtime_stunned_state({second.bytes.get(), second.size},
                    {reinterpret_cast<u8*>(&reply.id), 12}, stunned_context());
                return;
            }
            if (type == 0x84)
            {
                // 0x000A1850 reads the reply's id and its next word.
                auto& second = player_.body_selection.second;
                const std::array<u32, 2> words{reply.id, read_u32({reinterpret_cast<const u8*>(&reply.id), 8}, 4)};
                message_runtime_endure_state({second.bytes.get(), second.size}, words, endure_context());
                return;
            }
            if (type == 0x86)
            {
                // 0x000A24B0 reads the reply's id and, for 0x7C, its next word.
                auto& second = player_.body_selection.second;
                const std::array<u32, 2> words{reply.id, read_u32({reinterpret_cast<const u8*>(&reply.id), 8}, 4)};
                message_runtime_spin_dizzy_state({second.bytes.get(), second.size}, words, spin_dizzy_context());
                return;
            }
            if (type == 0x64)
            {
                // 0x0008CEA0 reads the reply's id and, for 0x7C, its next word.
                auto& second = player_.body_selection.second;
                message_runtime_dizzy_state({second.bytes.get(), second.size},
                    {reinterpret_cast<const u8*>(&reply.id), 8});
                return;
            }
            if (type == 0x73)
            {
                // 0x00095C90 acts on ids 7, 0x41, 0x5A, 0x6E, 0x6F, 0x70 and 0x7C and returns for the others.
                const u32 id = reply.id;
                if (id == 7 || id == 0x41 || id == 0x5A || id == 0x6E || id == 0x6F || id == 0x70 || id == 0x7C)
                {
                    unbound("collision reply id " + std::to_string(id) + " to the hit state 0x00095C90");
                }
                return;
            }
            if (type == 0x78)
            {
                // 0x00098620 acts on ids 0x5A (with words +0x18 / +0x1C) and 0x7F (with +4).
                if (reply.id == 0x5A || reply.id == 0x7F)
                {
                    unbound("collision reply id " + std::to_string(reply.id) + " to the swing state 0x00098620");
                }
                return;
            }
            if (type == 0x7E)
            {
                // 0x0009EED0 acts on ids 0x41 (with +0x14 and the position at +0x1C) and 0x70 (with +4 / +8).
                if (reply.id == 0x41 || reply.id == 0x70)
                {
                    unbound("collision reply id " + std::to_string(reply.id) + " to the scoop and whack state 0x0009EED0");
                }
                return;
            }
            if (type == 0x83)
            {
                // 0x000A1070 acts on ids 0x41 (with +0x14 and the position at +0x1C) and 0x70 (with +4 / +8).
                if (reply.id == 0x41 || reply.id == 0x70)
                {
                    unbound("collision reply id " + std::to_string(reply.id) + " to the slide state 0x000A1070");
                }
                return;
            }
            if (type == 0x5F)
            {
                // 0x00089530: id 0x62 writes the reply's accepted / handled words and reads its +0x1C contact.
                auto& second = player_.body_selection.second;
                std::array<u32, 3> words{reply.id, reply.accepted, reply.handled};
                const std::span<const u8> contact = reply.contact == nullptr ? std::span<const u8>()
                                                                             : std::span<const u8>(reply.contact, 0x18);
                message_runtime_arc_attack_state({second.bytes.get(), second.size}, words, contact,
                    arc_attack_context());
                reply.accepted = words[1];
                reply.handled = words[2];
                return;
            }
            if (type == 0x66)
            {
                // 0x0008DD30 reads the reply's id: only 0x51 acts.
                auto& second = player_.body_selection.second;
                const std::array<u32, 1> words{reply.id};
                message_runtime_arc_state({second.bytes.get(), second.size}, words, arc_context());
                return;
            }
            if (type == 0x79)
            {
                // 0x0009AE60 reads past the id only for 0x7C (+4), 0x86 (+8) and 0x6E (+4, +0x10).
                if (reply.id == 0x7C || reply.id == 0x86 || reply.id == 0x6E || reply.id == 0x31)
                {
                    unbound("collision reply id " + std::to_string(reply.id) + " to the jump on/off state 0x0009AE60");
                }
                auto& second = player_.body_selection.second;
                const std::array<u32, 1> words{reply.id};
                message_runtime_jump_on_state({second.bytes.get(), second.size}, words, jump_on_context());
                return;
            }
            if (type == 0x67)
            {
                // 0x0008E480 acts only on 0x2D (reading its +8 actor) and 0x7C (its +4).
                if (reply.id == 0x2D || reply.id == 0x7C)
                {
                    unbound("collision reply id " + std::to_string(reply.id) + " to the drone dance state 0x0008E480");
                }
                return;
            }
            if (type == 0x76)
            {
                // 0x00097890 acts only on 0x18, reading its velocity and point.
                if (reply.id == 0x18)
                {
                    unbound("collision reply id 0x18 to the hit-stream state 0x00097890");
                }
                return;
            }
            if (type == 0x8C)
            {
                // 0x000A49D0 reads past the id only for 0x7C (its +4 actor).
                if (reply.id == 0x7C)
                {
                    unbound("collision reply id 0x7C to the transform state 0x000A49D0");
                }
                auto& second = player_.body_selection.second;
                const std::array<u32, 1> words{reply.id};
                message_runtime_transform_state({second.bytes.get(), second.size}, words, transform_context());
                return;
            }
            if (type == 0x6A)
            {
                // 0x0008FBF0 acts only on 0x7C, reading its +4 actor.
                if (reply.id == 0x7C)
                {
                    unbound("collision reply id 0x7C to the flamethrower state 0x0008FBF0");
                }
                return;
            }
            // 0x8A (getter 0x000A34C0) and 0x63 (getter 0x0008C5A0) keep the empty 0x001628B0 at +0xC too.
            if (type != 0x87 && type != 0x88 && type != 0x69 && type != 0x70 && type != 0x5B && type != 0x5C && type != 0x74 && type != 0x7C
                && type != 0x8A && type != 0x63 && type != 0x80 && type != 0x7F && type != 0x6B && type != 0x77
                && type != 0x6E && type != 0x6D && type != 0x81 && type != 0x7B)
            {
                unbound("collision reply action vtable +0xC of factory type " + std::to_string(type));
            }
            // These five action tables have the empty 0x001628B0 message slot.
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
