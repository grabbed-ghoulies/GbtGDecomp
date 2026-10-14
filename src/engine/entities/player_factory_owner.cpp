#include "player_factory_owner.h"
#include "player_state.h"

namespace gbtg
{
namespace
{
NativePlayerFactoryOwner* active_owner = nullptr;

std::span<u8> bytes_of(FactoryObject& object)
{
    return {object.bytes.get(), object.size};
}

u32 parameter_word(std::span<const u8> parameters, size_t offset)
{
    return read_u32(parameters, offset);
}

// Xbox 0x001628B0
void destroy_empty_player_object(FactoryObject&)
{
}

// Native lifecycle slot adapters. The translated constructors and destructors keep their retail
// branches; these resolve the owning player's native state. Children find their player through
// the actor address the player constructor writes into their parameters.
void player_sizes(ObjectSizes& output)
{
    std::array<u32, 2> sizes{};
    runtime_player_storage_sizes(sizes);
    output.instance = sizes[0];
    output.parameters = sizes[1];
}

bool construct_player_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& player = active_owner->construct_player(object);
    return initialize_runtime_player(player.actor_bytes, parameters, player.construction);
}

void destroy_player_view(FactoryObject& object)
{
    auto& player = active_owner->player(object);
    destroy_runtime_player(player.actor_bytes, player.destruction);
    if (!active_owner->defer_release)
    {
        active_owner->release_player(object);
    }
}

bool construct_parent_body_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& player = active_owner->player_at(parameter_word(parameters, 0x1B0));
    active_owner->bind_child(object, player);
    player.body = bytes_of(object);
    player.attachment.body = player.body;
    player.body_selection.body_address = player.scene.address_of(object.bytes.get());
    player.model = player.scene.model_views(player.actor);
    player.body_construction.nodes = player.model.nodes;
    player.body_construction.model_channels = player.model.channels;
    const bool result = initialize_runtime_parent_body(player.body, parameters, player.body_construction);
    player.refresh_body_views();
    return result;
}

void destroy_parent_body_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    destroy_runtime_parent_body(player->body, player->body_cleanup);
    player->refresh_body_views();
    player->body = {};
    player->attachment.body = {};
    active_owner->release_child(object);
}

bool construct_player_body_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->body_motion.model = player->actor.model;
    player->body_motion.nodes = player->model.nodes;
    return initialize_runtime_player_body(player->body, parameters, player->body_motion);
}

void destroy_player_body_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    destroy_runtime_player_body(player->body, player->shapes, player->joint_records);
}

bool construct_base_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto& player = active_owner->player_at(parameter_word(parameters, 8));
    active_owner->bind_child(object, player);
    player.strategy_type = object.type;
    player.strategy = bytes_of(object);
    player.strategy_entry.strategy = player.strategy;
    return initialize_runtime_base_strategy(player.strategy, parameters, player.actor.class_key,
        player.scene.strategy_ranges, player.scene.random);
}

void destroy_base_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    destroy_runtime_base_strategy(object, player->strategy_link, player->strategy_setup);
    player->strategy = {};
    player->strategy_entry.strategy = {};
    active_owner->release_child(object);
}

void destroy_player_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    destroy_runtime_player_strategy(player->strategy, player->scene.active_strategies);
}

bool construct_imp_fire_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x46;
    player->refresh_views();
    return initialize_runtime_imp_fire_strategy(player->imp_fire);
}

void destroy_imp_fire_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_imp_fire_strategy(player->imp_fire);
}

RuntimeCameraBodyContext camera_body_context(NativePlayerInstance& player)
{
    return RuntimeCameraBodyContext{player.body, player.actor, player.body_flags, player.camera_view};
}

bool construct_camera_body_view(FactoryObject& object, std::span<const u8>)
{
    auto context = camera_body_context(*active_owner->find_player(object));
    return initialize_runtime_camera_body(context);
}

void destroy_camera_body_view(FactoryObject& object)
{
    auto context = camera_body_context(*active_owner->find_player(object));
    destroy_runtime_camera_body(context);
}

bool construct_director_strategy_view(FactoryObject& object, std::span<const u8>)
{
    return initialize_runtime_director_strategy(bytes_of(object));
}

const ObjectFactoryVtable* camera_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504F80, camera_body_object_sizes, construct_camera_body_view,
        destroy_camera_body_view};
    return &table;
}

const ObjectFactoryVtable* camera_mind_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505458, camera_mind_object_sizes, accept_runtime_object_construction,
        destroy_empty_player_object};
    return &table;
}

const ObjectFactoryVtable* director_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505D54, director_strategy_object_sizes,
        construct_director_strategy_view, destroy_empty_player_object};
    return &table;
}

const ObjectFactoryVtable* player_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD6B8, player_sizes, construct_player_view, destroy_player_view};
    return &table;
}

const ObjectFactoryVtable* parent_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD658, base_body_object_sizes, construct_parent_body_view,
        destroy_parent_body_view};
    return &table;
}

const ObjectFactoryVtable* player_body_parent_lifecycle()
{
    static const ObjectFactoryVtable table{0x00504FE0, parent_player_body_object_sizes, construct_player_body_view,
        destroy_player_body_view};
    return &table;
}

const ObjectFactoryVtable* player_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505248, player_body_object_sizes, accept_runtime_object_construction,
        destroy_empty_player_object};
    return &table;
}

const ObjectFactoryVtable* base_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x004DD74C, base_strategy_object_sizes, construct_base_strategy_view,
        destroy_base_strategy_view};
    return &table;
}

const ObjectFactoryVtable* player_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505FA0, player_strategy_object_sizes,
        accept_runtime_object_construction, destroy_player_strategy_view};
    return &table;
}

const ObjectFactoryVtable* imp_fire_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505EA4, imp_fire_strategy_object_sizes,
        construct_imp_fire_strategy_view, destroy_imp_fire_strategy_view};
    return &table;
}
}

NativePlayerInstance::NativePlayerInstance(NativePlayerSceneBindings& scene_bindings,
    NativeActorFactoryInstance& owner_instance, FactoryObject& object)
    : scene(scene_bindings),
      actor_instance(owner_instance),
      actor(owner_instance.bat.actor),
      actor_bytes(bytes_of(object)),
      actor_address(scene_bindings.address_of(object.bytes.get())),
      self{actor_bytes, actor_address,
          [this](std::span<const u32> message)
          {
              if (!actor_message)
              {
                  throw Error("player actor message slot 0x00028F60 is not attached");
              }
              actor_message(message);
          },
          nullptr, &owner_instance.bat.actor},
      child{scene_bindings.factory, scene_bindings.resources, actor_address},
      attributes{attribute_resource, scene_bindings.registry, scene_bindings.error, scene_bindings.random,
          scene_bindings.actor_tags, scene_bindings.address_zero},
      body_animation{animation_resource, scene_bindings.animation_sequences, animation_groups, animation_group_count,
          animation_bits, scene_bindings.animation_cache, scene_bindings.animation_cache_count, scene_bindings.registry,
          scene_bindings.error},
      body_attachment{attached, body_actor_address, owner_instance.bat.actor, body_flags, first_receiver},
      body_lists{owner_instance.pending.next, owner_instance.startup.attachments.sentinel.next,
          scene_bindings.timed_attachments},
      body_health{actor_bytes, scene_bindings.health_curve, scene_bindings.tags, health_textures,
          scene_bindings.registry, scene_bindings.error},
      body_cleanup{body_animation, body_attachment, body_resources, body_hashes, body_hash_count, body_lists, collision,
          body_attributes, shapes, body_health, first_action, second_action, body_actions, scene_bindings.factory},
      body_selection{owner_instance.animation, attached, first_receiver, second_receiver, first_action, second_action,
          action_row, action_table, scene_bindings.action_names, child, scene_bindings.registry, scene_bindings.error,
          scene_bindings.random, 0},
      body_action{body_action_object, child, nullptr},
      body_construction{body_cleanup, body_selection, body_action, actor_bytes, nullptr, nullptr,
          scene_bindings.scale_exclusions, scene_bindings.scale_exclusion_count, scene_bindings.animation_classes,
          scene_bindings.animation_class_count, scene_bindings.friction_curve, scene_bindings.tags,
          body_resource_counts, scene_bindings.loaded_sizes, scene_bindings.action_tables},
      body_motion{nullptr, motion_controller, motion_secondary, motion_selector, joint_records, nullptr, shapes},
      requests{request_anchor.next, [](RuntimePlayerMindRequest&)
          {
              // Player mind vtable +0x1C is the empty 0x001628B0.
          }},
      camera_source{owner_instance.bat.actor, {}, {}},
      mind_setup{owner_instance.bat.actor, actor_bytes, {}, nullptr, actor_address, scene_bindings.scene_flags,
          scene_bindings.runtime_flags, scene_bindings.story_flags, scene_bindings.health, scene_bindings.chapter_mode,
          scene_bindings.scene_name, scene_bindings.intro_sound, camera_source, scene_bindings.camera,
          scene_bindings.cameras},
      strategy_setup{child, mind_requests,
          [this](FactoryObject& object, u16 old_state, u16 next_state)
          {
              if (strategy_type == 0x46)
              {
                  // Imp fire strategy vtable +0x18, 0x000B4A60.
                  refresh_views();
                  leave_runtime_imp_fire_state(old_state, next_state, imp_fire);
                  return;
              }
              if (strategy_type == 0x39)
              {
                  // Director strategy vtable +0x18, 0x000AA450.
                  leave_runtime_director_state(bytes_of(object), old_state);
                  return;
              }
              // Player strategy vtable +0x18, 0x000BF410.
              leave_runtime_player_strategy(bytes_of(object), old_state, next_state, body, scene.active_strategies,
                  scene.pause);
          },
          [this](FactoryObject&, u16 state, u16 previous)
          {
              refresh_views();
              if (strategy_type == 0x46)
              {
                  // Imp fire strategy vtable +0x14, 0x000B4940.
                  enter_runtime_imp_fire_state(state, previous, imp_fire);
                  return;
              }
              if (strategy_type == 0x39)
              {
                  // Director strategy vtable +0x14, 0x000AA3A0.
                  RuntimeDirectorStrategy director{strategy, request_anchor, scene.mind_request_sizes, strategy_entry};
                  enter_runtime_director_state(state, director);
                  return;
              }
              // Player strategy vtable +0x14, 0x000BF2B0.
              enter_runtime_player_strategy_state(state, flow);
          }},
      attachment{{}, self, body_attachment, body_selection, secondary, mind_action, mind_present,
          owner_instance.animation, scene_bindings.attachment_actors, scene_bindings.weapon_names,
          scene_bindings.class_names,
          [this]()
          {
              if (!body_action_key)
              {
                  throw Error("player body vtable +0x20 (0x00075B90) is not attached");
              }
              return body_action_key();
          }},
      attachment_creation{attachment, child, scene_bindings.attachment_objects},
      construction{owner_instance.bat.actor, attributes, spawn, body_object, mind_object, strategy_object,
          strategy_setup, input, scene_bindings.input_release, scene_bindings.tag_rows, scene_bindings.actor_tags,
          attachment_creation, created_attachment, scene_bindings.initial_channel_word, scene_bindings.player_count,
          scene_bindings.story_flags, matrices, scene_bindings.matrix_boundaries},
      goal_cleanup{self, ring, body, owner_instance.bat.actor, scene_bindings.latches, scene_bindings.address_zero,
          scene_bindings.overrides},
      destruction{construction, goal_cleanup, links, navigation},
      mind_events{event_anchor, {}, scene_bindings.mind_descriptors,
          [this](std::span<const u32> message)
          {
              self.message(message);
          }},
      mind_goals{nullptr, request_anchor, scene_bindings.mind_request_sizes, scene_bindings.mind_goal_names,
          scene_bindings.goal_scratch, scene_bindings.random, scene_bindings.registry, scene_bindings.error},
      strategy_entry{{}, owner_instance.bat.actor, actor_bytes, {}, mind_events, mind_goals, ring,
          scene_bindings.address_zero, allowed, scene_bindings.targets, "actionenableddelay", "celebrate", "coward",
          "worried", "taunt"},
      pose{self, owner_instance.bat.actor, receiver, owner_instance.animation, owner_instance.pose,
          owner_instance.startup.bone, scene_bindings.skeleton, scene_bindings.node_cache, scene_bindings.reset_matrix},
      strategy_goals{strategy_entry, strategy_object, strategy_setup, goal_cleanup, scene_bindings.resume, body_bits,
          body_selection,
          [this]()
          {
              return attachment.body_action_key();
          },
          scene_bindings.game,
          [this](u32 slot)
          {
              if (!body_slot_key)
              {
                  throw Error("player body vtable slots are not attached");
              }
              return body_slot_key(slot);
          },
          scene_bindings.target_at, scene_bindings.address_of, pose, scene_bindings.attachment_actors,
          scene_bindings.effects, {}, scene_bindings.input_release, room, current, goal_volume,
          scene_bindings.script_records, scene_bindings.runtime_flags, scene_bindings.random, scene_bindings.clock,
          "ghouly_intro", "drone_waitforscare"},
      strategy_messages{strategy_goals, scene_bindings.query, strategy_link, scene_bindings.message_actor, carried},
      callout{owner_instance.bat.actor, owner_instance.startup.attachments, scene_bindings.effects,
          scene_bindings.factory, scene_bindings.resources, scene_bindings.callout_objects, actor_address,
          scene_bindings.linked_records, {}},
      flow{strategy_messages, scene_bindings.pause, scene_bindings.active_strategies, scene_bindings.brew,
          scene_bindings.health_display, axis_device, input_device, scene_bindings.input_state,
          scene_bindings.keyboard, scene_bindings.voices, owner_instance.startup.resource, callout,
          scene_bindings.brews_taken, scene_bindings.brew_accepted, scene_bindings.prompt_lock,
          scene_bindings.prompt_owner, scene_bindings.difficulty, scene_bindings.chapter_mode,
          scene_bindings.unlock_flags, scene_bindings.scene_flags, scene_bindings.scene_name,
          scene_bindings.brews_owned, scene_bindings.brews_required, scene_bindings.story_byte,
          scene_bindings.intro_pending, scene_bindings.intro_name, scene_bindings.cases,
          scene_bindings.case_body_bits, "idle"},
      target_checks{strategy_goals, attribute_resource, scene_bindings.difficulty, scene_bindings.active_actors,
          scene_bindings.notices, scene_bindings.occupancy_initial},
      target_queue{scene_bindings.target_slots, scene_bindings.queue_limits, scene_bindings.class_queue_limits,
          scene_bindings.target_of, scene_bindings.actor_bytes_of},
      imp_fire{strategy_messages, target_checks, target_queue, scene_bindings.imp_fire_parameters,
          owner_instance.animation, scene_bindings.registry, scene_bindings.create_effect, scene_bindings.object_message,
          scene_bindings.body_at, scene_bindings.body_of, scene_bindings.model_of, scene_bindings.attachment_limit,
          "aid_objparams_ghoulies_fx_mutatehalo", "attack", "mutateup", "jumpbite", "jumpbiteprepare"}
{
}

void NativePlayerInstance::refresh_body_views()
{
    body_bits = body_flags;
    body_output.actor = &actor;
    body_output.bits = body_bits;
    actor.euler_out = body.empty() ? nullptr : &body_output;
    if (!body.empty())
    {
        body_output.position = Vec3{read_f32(body, 0x18), read_f32(body, 0x1C), read_f32(body, 0x20)};
        body_output.euler = Vec3{read_f32(body, 0x72C), read_f32(body, 0x730), read_f32(body, 0x734)};
    }
    if (body_flags != nullptr)
    {
        body_bit_block.bytes = body_flags->bytes;
    }
    carried = body_attributes.data();
    if (!body.empty())
    {
        std::memcpy(body.data() + 0x154, &body_hash_count, 4);
    }
    strategy_entry.body = body;
    callout.command_attachments = body.empty() ? std::span<u8>() : body.subspan(0x2D8);
}

void NativePlayerInstance::refresh_input_views()
{
    input_device.record = input;
    axis_device.record = input;
    strategy_goals.input = input;
    for (size_t index = 0; index < 4; ++index)
    {
        input_device.children[index] = nullptr;
        axis_device.children[index] = nullptr;
        if (input.size() < 0x18)
        {
            continue;
        }
        const auto found = scene.input_activation.devices.find(read_u32(input, 8 + 4 * index));
        if (found != scene.input_activation.devices.end())
        {
            input_children[index] = RuntimeDialogueInputDevice{found->second, {}};
            axis_children[index] = RuntimeInputAxisWriteDevice{found->second, {}};
            input_device.children[index] = &input_children[index];
            axis_device.children[index] = &axis_children[index];
        }
    }
}

void NativePlayerInstance::refresh_views()
{
    refresh_body_views();
    refresh_input_views();
    mind_events.mind = mind;
    mind_goals.table = mind.empty() ? nullptr : scene.goal_table(mind_script_table);
    strategy_entry.strategy = strategy;
}

void NativePlayerFactoryOwner::strategy_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get())
    {
        throw Error("player strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_player_strategy_message(message, player->flow);
}

void NativePlayerFactoryOwner::strategy_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get())
    {
        throw Error("player strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_player_strategy(player->flow);
}

void NativePlayerFactoryOwner::imp_fire_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x46)
    {
        throw Error("imp fire strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_imp_fire_message(message, player->imp_fire);
}

void NativePlayerFactoryOwner::imp_fire_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x46)
    {
        throw Error("imp fire strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_imp_fire_strategy(player->imp_fire);
}

NativePlayerFactoryOwner::NativePlayerFactoryOwner(NativePlayerFactoryBindings player_bindings)
    : bindings(std::move(player_bindings)),
      minds(NativePlayerMindBindings{
          [this](FactoryObject& object, std::span<const u8> parameters)
          {
              auto& player = player_at(read_u32(parameters, 0x9C));
              bind_child(object, player);
              player.mind = bytes_of(object);
              player.mind_present = true;
              player.mind_requests = &player.requests;
              player.mind_setup.body_bytes = player.body;
              player.mind_setup.animation_table = player.action_table;
              player.camera_source.body = player.body;
              player.camera_source.case_link = player.attached != nullptr ? std::span<const u8>(player.attached->bytes)
                                                                         : std::span<const u8>();
              auto& scene = player.scene;
              auto slots = std::make_unique<NativePlayerMindSlots>(NativePlayerMindSlots{player.mind,
                  player.actor.class_key, scene.mind_settings, scene.random, scene.resources, player.mind_script_table,
                  player.mind_setup, player.requests, player.event_anchor.next, player.mind_storage, player.actor_bytes,
                  scene.runtime_flags, scene.input_activation, nullptr, nullptr, nullptr, player.actor, nullptr,
                  nullptr, nullptr});
              if (read_u16(parameters, 4) == 0x35)
              {
                  player.cinema_lifecycle = std::make_unique<RuntimeCinemaMindLifecycle>(RuntimeCinemaMindLifecycle{
                      player.mind, player.actor_bytes, player.cinema_resource, scene.registry, scene.error,
                      scene.random, scene.address_of});
                  slots->cinema = std::make_unique<NativeCinemaMindSlots>(NativeCinemaMindSlots{
                      *player.cinema_lifecycle, nullptr, nullptr, nullptr,
                      []() -> std::span<const u8>
                      {
                          throw Error("cinema mind target view (mind +0x10) is not attached");
                      }});
              }
              return slots;
          },
          [this](const FactoryObject& object)
          {
              if (auto* player = find_player(object))
              {
                  player->cinema_lifecycle.reset();
                  player->mind = {};
                  player->mind_present = false;
                  player->mind_requests = nullptr;
              }
              release_child(object);
          }})
{
}

NativePlayerFactoryOwner::~NativePlayerFactoryOwner()
{
    if (active_owner == this)
    {
        active_owner = nullptr;
    }
}

void NativePlayerFactoryOwner::install(std::span<ObjectFactoryType> types)
{
    if (active_owner != nullptr && active_owner != this)
    {
        throw Error("native player factory already has an owner");
    }
    // Lifecycle views of retail getters 0x00027F60, 0x0001DA50, 0x000733F0, 0x00075AF0, 0x0002B6C0,
    // 0x000BED40 and 0x000B4480; types 1 and 2 stay with the actor owner, types 7 and 0x34 with the mind owner.
    types[5] = ObjectFactoryType{2, player_lifecycle};
    types[6] = ObjectFactoryType{1, parent_body_lifecycle};
    types[0x27] = ObjectFactoryType{6, player_body_parent_lifecycle};
    types[0x2A] = ObjectFactoryType{0x27, player_body_lifecycle};
    types[8] = ObjectFactoryType{1, base_strategy_lifecycle};
    types[0x4F] = ObjectFactoryType{8, player_strategy_lifecycle};
    types[0x46] = ObjectFactoryType{8, imp_fire_strategy_lifecycle};
    types[0x28] = ObjectFactoryType{6, camera_body_lifecycle};
    types[0x39] = ObjectFactoryType{8, director_strategy_lifecycle};
    minds.install(types);
    types[0x36] = ObjectFactoryType{7, camera_mind_lifecycle};
    active_owner = this;
}

NativePlayerInstance& NativePlayerFactoryOwner::construct_player(FactoryObject& object)
{
    auto& actor_instance = bindings.actors.view(object);
    auto instance = std::make_unique<NativePlayerInstance>(bindings.scene, actor_instance, object);
    auto* result = instance.get();
    addresses_[result->actor_address] = result;
    players_[object.bytes.get()] = std::move(instance);
    return *result;
}

NativePlayerInstance& NativePlayerFactoryOwner::player(const FactoryObject& object)
{
    return *players_.at(object.bytes.get());
}

NativePlayerInstance& NativePlayerFactoryOwner::player_at(u32 actor_address)
{
    const auto found = addresses_.find(actor_address);
    if (found == addresses_.end())
    {
        throw Error("player child names an actor address without a player");
    }
    return *found->second;
}

NativePlayerInstance* NativePlayerFactoryOwner::find_player(const FactoryObject& child) const
{
    const auto found = children_.find(child.bytes.get());
    return found != children_.end() ? found->second : nullptr;
}

NativePlayerInstance* NativePlayerFactoryOwner::find_child_owner(const u8* child) const
{
    const auto found = children_.find(child);
    return found != children_.end() ? found->second : nullptr;
}

void NativePlayerFactoryOwner::release_player(const FactoryObject& object)
{
    release_player(object.bytes.get());
}

void NativePlayerFactoryOwner::release_player(const u8* bytes)
{
    const auto found = players_.find(bytes);
    if (found != players_.end())
    {
        addresses_.erase(found->second->actor_address);
        players_.erase(found);
    }
}

void NativePlayerFactoryOwner::bind_child(const FactoryObject& child, NativePlayerInstance& owner)
{
    children_[child.bytes.get()] = &owner;
}

void NativePlayerFactoryOwner::release_child(const FactoryObject& child)
{
    children_.erase(child.bytes.get());
}

std::vector<NativePlayerInstance*> NativePlayerFactoryOwner::instances() const
{
    std::vector<NativePlayerInstance*> result;
    for (const auto& [bytes, player] : players_)
    {
        result.push_back(player.get());
    }
    return result;
}

size_t NativePlayerFactoryOwner::player_total() const
{
    return players_.size();
}

size_t NativePlayerFactoryOwner::child_count() const
{
    return children_.size();
}
}
