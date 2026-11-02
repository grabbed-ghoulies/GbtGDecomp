#include "player_factory_owner.h"
#include "mind_fields.h"
#include "../scene/cutscene_scene_runtime.h"
#include "../camera/display_view.h"
#include "player_state.h"
#include "actor_attachment_events.h"
#include "body_request_selection.h"
#include "player_attachment_helpers.h"

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
    player.constructing = &object;
    struct ConstructionEnd
    {
        NativePlayerInstance& player;
        ~ConstructionEnd()
        {
            player.constructing = nullptr;
        }
    } end{player};
#if defined(GBTG_INSPECTOR_APP)
    try
    {
#endif
    return initialize_runtime_player(player.actor_bytes, parameters, player.construction);
#if defined(GBTG_INSPECTOR_APP)
    }
    catch (...)
    {
        // Children must go while the base actor and its shape/model views still
        // exist. The generic factory then unwinds that completed base actor.
        destroy_runtime_player(player.actor_bytes, player.destruction);
        throw;
    }
#endif
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

// Getter 0x000761A0: entityBodyZombie keeps the 0x27 parent's sizes (0x00076180) and message slot (0x00075A60 jumps
// to 0x00074080); its own construct and destroy are 0x000CFFD0 and the empty 0x001628B0.
const ObjectFactoryVtable* zombie_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505368, parent_player_body_object_sizes,
        accept_runtime_object_construction, destroy_empty_player_object};
    return &table;
}

// Getter 0x00075EC0 (body 0x00075ED0): entityBodyVampire has the zombie's slots but for +0xC (0x00075FC0), +0x20
// (0x00075F30), +0x30 (0x00075F90) and +0x34 (0x00075F60); its +0x2C stays the base 0x00026A90.
const ObjectFactoryVtable* vampire_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x005052A8, parent_player_body_object_sizes,
        accept_runtime_object_construction, destroy_empty_player_object};
    return &table;
}

// Getter 0x00076090 (body 0x000760A0): entityBodyWorm has the zombie's slots but for +0x20 (0x00076100), +0x2C
// (0x00076130) and +0x4C (0x00076160).
const ObjectFactoryVtable* worm_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505308, parent_player_body_object_sizes,
        accept_runtime_object_construction, destroy_empty_player_object};
    return &table;
}

// Getter 0x000759F0: entityBodyJessieClyde has the zombie's slots but for +0x20 and +0x2C (0x00075A70 / 0x00075AA0).
const ObjectFactoryVtable* jessie_clyde_body_lifecycle()
{
    static const ObjectFactoryVtable table{0x005051E8, parent_player_body_object_sizes,
        accept_runtime_object_construction, destroy_empty_player_object};
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

bool construct_butler_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3B;
    player->refresh_views();
    const u32 name = parameter_word(parameters, 0x18);
    return initialize_runtime_butler_strategy(player->strategy, player->actor_bytes,
        name == 0 ? nullptr : player->scene.string_at(name));
}

bool construct_skeleton_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x51;
    player->refresh_views();
    return accept_runtime_object_construction(object, parameters);
}

bool construct_spider_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x52;
    player->refresh_views();
    return accept_runtime_object_construction(object, parameters);
}

const ObjectFactoryVtable* spider_strategy_lifecycle()
{
    // Sizes are the skeleton's 0x000A9FC0; vtable +8 is the empty 0x001628B0.
    static const ObjectFactoryVtable table{0x00505FF4, skeleton_strategy_object_sizes,
        construct_spider_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_zombie_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x58;
    player->refresh_views();
    return initialize_runtime_zombie_strategy(bytes_of(object));
}

const ObjectFactoryVtable* zombie_strategy_lifecycle()
{
    // Sizes are the director's 0x000AAE40; vtable +8 is the empty 0x001628B0.
    static const ObjectFactoryVtable table{0x0050609C, director_strategy_object_sizes,
        construct_zombie_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_haunted_tv_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x44;
    player->refresh_views();
    return initialize_runtime_haunted_tv_strategy(parameters, player->haunted_tv); // 0x000B1F10
}

void destroy_haunted_tv_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_haunted_tv_strategy(player->haunted_tv); // 0x000B1F40
}

// Getter 0x000B1EC0.
const ObjectFactoryVtable* haunted_tv_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505E6C, haunted_tv_strategy_object_sizes,
        construct_haunted_tv_strategy_view, destroy_haunted_tv_strategy_view};
    return &table;
}

bool construct_generic_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3C;
    player->refresh_views();
    return initialize_runtime_generic_strategy(player->generic); // 0x000ABBD0
}

// Getter 0x000ABB80; vtable +0 is 0x000ABB60 (shared with the haunted chair), +8 the empty 0x001628B0.
const ObjectFactoryVtable* generic_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505D8C, haunted_chair_strategy_object_sizes,
        construct_generic_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_ghoulie_girl_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3D;
    player->refresh_views();
    return initialize_runtime_ghoulie_girl_strategy(bytes_of(object), parameters, player->ghoulie_girl); // 0x000AC1B0
}

// Getter 0x000AC160; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* ghoulie_girl_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505DA8, ghoulie_girl_strategy_object_sizes,
        construct_ghoulie_girl_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_haunted_chair_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x40;
    player->refresh_views();
    return initialize_runtime_haunted_chair_strategy(bytes_of(object)); // 0x000AE770
}

// Getter 0x000AE720; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* haunted_chair_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505DFC, haunted_chair_strategy_object_sizes,
        construct_haunted_chair_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_grim_reaper_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3F;
    player->refresh_views();
    return initialize_runtime_grim_reaper_strategy(player->grim_reaper); // 0x000AD400
}

void destroy_grim_reaper_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_grim_reaper_strategy(player->grim_reaper); // 0x000AD570
}

// Getter 0x000AD3B0.
const ObjectFactoryVtable* grim_reaper_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505DE0, grim_reaper_strategy_object_sizes,
        construct_grim_reaper_strategy_view, destroy_grim_reaper_strategy_view};
    return &table;
}

bool construct_mummy_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x4C;
    player->refresh_views();
    return initialize_runtime_mummy_strategy(bytes_of(object), player->actor_bytes); // 0x000BBC70
}

// Getter 0x000BBC20; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* mummy_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505F4C, mummy_strategy_object_sizes, construct_mummy_strategy_view,
        destroy_empty_player_object};
    return &table;
}

bool construct_vampire_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x54;
    player->refresh_views();
    return initialize_runtime_vampire_strategy(bytes_of(object)); // 0x000C2050
}

// Getter 0x000C2000; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* vampire_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x0050602C, vampire_strategy_object_sizes, construct_vampire_strategy_view,
        destroy_empty_player_object};
    return &table;
}

bool construct_worm_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x57;
    player->refresh_views();
    return initialize_runtime_worm_strategy(bytes_of(object), parameters); // 0x000C45D0
}

void destroy_worm_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_worm_strategy(player->worm); // 0x000C4620
}

// Getter 0x000C4580.
const ObjectFactoryVtable* worm_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506080, worm_strategy_object_sizes, construct_worm_strategy_view,
        destroy_worm_strategy_view};
    return &table;
}

bool construct_imp_flamethrower_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x47;
    player->refresh_views();
    return accept_runtime_object_construction(object, parameters); // 0x000CFFD0
}

// Getter 0x000B5520: the director's sizes 0x000AAE40, the shared constructor and the empty 0x001628B0.
const ObjectFactoryVtable* imp_flamethrower_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505EC0, director_strategy_object_sizes,
        construct_imp_flamethrower_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_chicken_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3A;
    player->refresh_views();
    return initialize_runtime_chicken_strategy(bytes_of(object)); // 0x000AA030
}

// Getter 0x000A9FE0: sizes are the skeleton's 0x000A9FC0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* chicken_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505D38, skeleton_strategy_object_sizes,
        construct_chicken_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_ribs_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x50;
    player->refresh_views();
    return initialize_runtime_ribs_strategy(player->ribs); // 0x000BF7B0
}

// Getter 0x000BF760: sizes are the base strategy's 0x0002B6A0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* ribs_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505FBC, base_strategy_object_sizes, construct_ribs_strategy_view,
        destroy_empty_player_object};
    return &table;
}

bool construct_giant_hand_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x3E;
    player->refresh_views();
    return initialize_runtime_giant_hand_strategy(bytes_of(object), parameters, player->giant_hand); // 0x000AC930
}

// Getter 0x000AC8E0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* giant_hand_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505DC4, giant_hand_strategy_object_sizes,
        construct_giant_hand_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_haunted_coat_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x41;
    player->refresh_views();
    return initialize_runtime_haunted_coat_strategy(player->haunted_coat); // 0x000AF0A0
}

// Getter 0x000AF050; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* haunted_coat_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505E18, haunted_coat_strategy_object_sizes,
        construct_haunted_coat_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_haunted_picture_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x43;
    player->refresh_views();
    return accept_runtime_object_construction(object, parameters); // 0x000CFFD0
}

// Getter 0x000B0BB0: the shared constructor 0x000CFFD0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* haunted_picture_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505E50, haunted_picture_strategy_object_sizes,
        construct_haunted_picture_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_warlock_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x56;
    player->refresh_views();
    return initialize_runtime_warlock_strategy(bytes_of(object), parameters, player->warlock); // 0x000C3D10
}

void destroy_warlock_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_warlock_strategy(player->warlock); // 0x000C3E90
}

// Getter 0x000C3CC0.
const ObjectFactoryVtable* warlock_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506064, warlock_strategy_object_sizes,
        construct_warlock_strategy_view, destroy_warlock_strategy_view};
    return &table;
}

bool construct_vampire_chicken_strategy_view(FactoryObject& object, std::span<const u8>)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x55;
    player->vampire_chicken.held_actor = nullptr;
    player->refresh_views();
    return initialize_runtime_vampire_chicken_strategy(bytes_of(object)); // 0x000C2930
}

// Getter 0x000C28E0: sizes are the director's 0x000AAE40; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* vampire_chicken_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506048, director_strategy_object_sizes,
        construct_vampire_chicken_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_imp_flying_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x48;
    player->refresh_views();
    return initialize_runtime_imp_flying_strategy(bytes_of(object), parameters); // 0x000B6BC0
}

// Getter 0x000B6B70; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* imp_flying_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505EDC, imp_flying_strategy_object_sizes,
        construct_imp_flying_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_imp_ninja_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x49;
    player->refresh_views();
    return initialize_runtime_imp_ninja_strategy(bytes_of(object), parameters); // 0x000B7B00
}

// Getter 0x000B7AB0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* imp_ninja_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505EF8, imp_ninja_strategy_object_sizes,
        construct_imp_ninja_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_jessie_clyde_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x4A;
    player->refresh_views();
    return initialize_runtime_jessie_clyde_strategy(parameters, player->jessie_clyde); // 0x000B9490
}

void destroy_jessie_clyde_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_jessie_clyde_strategy(player->jessie_clyde); // 0x000B95A0
}

// Getter 0x000B9440.
const ObjectFactoryVtable* jessie_clyde_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505F14, jessie_clyde_strategy_object_sizes,
        construct_jessie_clyde_strategy_view, destroy_jessie_clyde_strategy_view};
    return &table;
}

bool construct_mummy_cursed_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x4D;
    player->refresh_views();
    return initialize_runtime_mummy_cursed_strategy(parameters, player->mummy_cursed); // 0x000BDA70
}

void destroy_mummy_cursed_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_mummy_cursed_strategy(player->mummy_cursed); // 0x000BDB30
}

// Getter 0x000BDA20.
const ObjectFactoryVtable* mummy_cursed_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505F68, mummy_cursed_strategy_object_sizes,
        construct_mummy_cursed_strategy_view, destroy_mummy_cursed_strategy_view};
    return &table;
}

bool construct_medusa_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x4B;
    player->refresh_views();
    return initialize_runtime_medusa_strategy(bytes_of(object), parameters, player->medusa); // 0x000BB220
}

// Getter 0x000BB1D0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* medusa_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505F30, medusa_strategy_object_sizes, construct_medusa_strategy_view,
        destroy_empty_player_object};
    return &table;
}

bool construct_pirate_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x4E;
    player->refresh_views();
    return initialize_runtime_pirate_strategy(bytes_of(object), parameters); // 0x000BE130
}

// Getter 0x000BE0E0; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* pirate_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505F84, pirate_strategy_object_sizes, construct_pirate_strategy_view,
        destroy_empty_player_object};
    return &table;
}

// 0x000A7850: 0x000A73A0 with EAX the parameters, then 1.
bool construct_baron_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x37;
    player->refresh_views();
    initialize_runtime_baron_strategy(parameters, player->baron); // 0x000A73A0
    return true;
}

// 0x000A7870: the tail jump to 0x000A7440.
void destroy_baron_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_baron_strategy(player->baron); // 0x000A7440
}

// Getter 0x000A7800.
const ObjectFactoryVtable* baron_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505D00, baron_strategy_object_sizes, construct_baron_strategy_view,
        destroy_baron_strategy_view};
    return &table;
}

bool construct_baron_plane_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x38;
    player->refresh_views();
    return initialize_runtime_baron_plane_strategy(bytes_of(object), parameters); // 0x000A8C60
}

// Getter 0x000A8C10; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* baron_plane_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505D1C, baron_plane_strategy_object_sizes,
        construct_baron_plane_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_hunchback_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x45;
    player->refresh_views();
    return initialize_runtime_hunchback_strategy(bytes_of(object), parameters, player->actor); // 0x000B3AE0
}

// Getter 0x000B3A90; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* hunchback_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505E88, hunchback_strategy_object_sizes,
        construct_hunchback_strategy_view, destroy_empty_player_object};
    return &table;
}

bool construct_haunted_door_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x42;
    player->refresh_views();
    return initialize_runtime_haunted_door_strategy(parameters, player->haunted_door); // 0x000AFD80
}

void destroy_haunted_door_strategy_view(FactoryObject& object)
{
    auto* player = active_owner->find_player(object);
    player->refresh_views();
    destroy_runtime_haunted_door_strategy(player->haunted_door); // 0x000AFDA0
}

// Getter 0x000AFD30.
const ObjectFactoryVtable* haunted_door_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00505E34, haunted_door_strategy_object_sizes,
        construct_haunted_door_strategy_view, destroy_haunted_door_strategy_view};
    return &table;
}

bool construct_ugly_girl_strategy_view(FactoryObject& object, std::span<const u8> parameters)
{
    auto* player = active_owner->find_player(object);
    player->strategy_type = 0x53;
    player->refresh_views();
    return initialize_runtime_ugly_girl_strategy(bytes_of(object), parameters); // 0x000C1370
}

// Getter 0x000C1320; vtable +8 is the empty 0x001628B0.
const ObjectFactoryVtable* ugly_girl_strategy_lifecycle()
{
    static const ObjectFactoryVtable table{0x00506010, ugly_girl_strategy_object_sizes,
        construct_ugly_girl_strategy_view, destroy_empty_player_object};
    return &table;
}

const ObjectFactoryVtable* skeleton_strategy_lifecycle()
{
    // Vtable +8 is the empty 0x001628B0.
    static const ObjectFactoryVtable table{0x00505FD8, skeleton_strategy_object_sizes,
        construct_skeleton_strategy_view, destroy_empty_player_object};
    return &table;
}

const ObjectFactoryVtable* butler_strategy_lifecycle()
{
    // Vtable +8 is the shared empty 0x000D4179 case.
    static const ObjectFactoryVtable table{0x00505D70, butler_strategy_object_sizes,
        construct_butler_strategy_view, destroy_empty_player_object};
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
              if (!actor_message && constructing != nullptr && scene.attach_actor_messages)
              {
                  scene.attach_actor_messages(*constructing, *this);
              }
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
              if (strategy_type == 0x51)
              {
                  // Skeleton strategy vtable +0x18, 0x000C0130.
                  refresh_views();
                  leave_runtime_skeleton_state(old_state, skeleton);
                  return;
              }
              if (strategy_type == 0x44)
              {
                  // Haunted TV strategy vtable +0x18, 0x000B2900.
                  refresh_views();
                  leave_runtime_haunted_tv_state(old_state, next_state, haunted_tv);
                  return;
              }
              if (strategy_type == 0x3F)
              {
                  // Grim Reaper strategy vtable +0x18, 0x000ADB10.
                  refresh_views();
                  leave_runtime_grim_reaper_state(old_state, next_state, grim_reaper);
                  return;
              }
              if (strategy_type == 0x3C)
              {
                  // Generic strategy vtable +0x18, 0x000AB8C0.
                  refresh_views();
                  leave_runtime_generic_state(old_state, generic);
                  return;
              }
              if (strategy_type == 0x3D)
              {
                  // Ghoulie girl strategy vtable +0x18, 0x000AC460.
                  refresh_views();
                  leave_runtime_ghoulie_girl_state(old_state, ghoulie_girl);
                  return;
              }
              if (strategy_type == 0x40)
              {
                  // Haunted chair strategy vtable +0x18, 0x000AEB30.
                  refresh_views();
                  leave_runtime_haunted_chair_state(old_state, next_state, haunted_chair);
                  return;
              }
              if (strategy_type == 0x4C)
              {
                  // Mummy strategy vtable +0x18, 0x000BC520.
                  refresh_views();
                  leave_runtime_mummy_state(old_state, mummy);
                  return;
              }
              if (strategy_type == 0x54)
              {
                  // Vampire strategy vtable +0x18, 0x000C2420.
                  refresh_views();
                  leave_runtime_vampire_state(old_state, vampire);
                  return;
              }
              if (strategy_type == 0x57)
              {
                  // Worm strategy vtable +0x18, 0x000C4C10.
                  refresh_views();
                  leave_runtime_worm_state(old_state, worm);
                  return;
              }
              if (strategy_type == 0x53)
              {
                  // Ugly girl strategy vtable +0x18, 0x000C14E0.
                  refresh_views();
                  leave_runtime_ugly_girl_state(old_state, ugly_girl);
                  return;
              }
              if (strategy_type == 0x47)
              {
                  // Flying imp strategy vtable +0x18, the spider's 0x000B5AF0.
                  refresh_views();
                  leave_runtime_spider_state(old_state, spider);
                  return;
              }
              if (strategy_type == 0x3E)
              {
                  // Giant hand strategy vtable +0x18, 0x000ACBF0.
                  refresh_views();
                  leave_runtime_giant_hand_state(old_state, giant_hand);
                  return;
              }
              if (strategy_type == 0x43)
              {
                  // Haunted picture strategy vtable +0x18, 0x000B12D0.
                  refresh_views();
                  leave_runtime_haunted_picture_state(old_state, haunted_picture);
                  return;
              }
              if (strategy_type == 0x56)
              {
                  // Warlock strategy vtable +0x18, 0x000C40F0.
                  refresh_views();
                  leave_runtime_warlock_state(old_state, warlock);
                  return;
              }
              if (strategy_type == 0x41)
              {
                  // Haunted coat strategy vtable +0x18, 0x000AF6E0.
                  refresh_views();
                  leave_runtime_haunted_coat_state(old_state, haunted_coat);
                  return;
              }
              if (strategy_type == 0x50)
              {
                  // Ribs strategy vtable +0x18, the base 0x000BFA50.
                  leave_runtime_base_strategy_state(bytes_of(object), old_state);
                  return;
              }
              if (strategy_type == 0x3A)
              {
                  // Chicken strategy vtable +0x18, the base 0x000BFA50.
                  leave_runtime_base_strategy_state(bytes_of(object), old_state);
                  return;
              }
              if (strategy_type == 0x55)
              {
                  // Vampire chicken strategy vtable +0x18, 0x000C2D10.
                  refresh_views();
                  leave_runtime_vampire_chicken_state(old_state, vampire_chicken);
                  return;
              }
              if (strategy_type == 0x48)
              {
                  // Flying imp strategy vtable +0x18, 0x000B7050.
                  refresh_views();
                  leave_runtime_imp_flying_state(old_state, imp_flying);
                  return;
              }
              if (strategy_type == 0x49)
              {
                  // Ninja imp strategy vtable +0x18, 0x000B7F20.
                  refresh_views();
                  leave_runtime_imp_ninja_state(old_state, imp_ninja);
                  return;
              }
              if (strategy_type == 0x4D)
              {
                  // Cursed mummy strategy vtable +0x18, 0x000BE010.
                  refresh_views();
                  leave_runtime_mummy_cursed_state(old_state, mummy_cursed);
                  return;
              }
              if (strategy_type == 0x4A)
              {
                  // Jessie Clyde strategy vtable +0x18, 0x000B9AC0.
                  refresh_views();
                  leave_runtime_jessie_clyde_state(old_state, jessie_clyde);
                  return;
              }
              if (strategy_type == 0x4B)
              {
                  // Medusa strategy vtable +0x18, 0x000BB7D0.
                  refresh_views();
                  leave_runtime_medusa_state(old_state, medusa);
                  return;
              }
              if (strategy_type == 0x4E)
              {
                  // Pirate strategy vtable +0x18, 0x000BEA60.
                  refresh_views();
                  leave_runtime_pirate_state(old_state, pirate);
                  return;
              }
              if (strategy_type == 0x37)
              {
                  // Baron strategy vtable +0x18, 0x000A8590.
                  refresh_views();
                  leave_runtime_baron_state(old_state, baron);
                  return;
              }
              if (strategy_type == 0x38)
              {
                  // Baron plane strategy vtable +0x18, 0x000A9980.
                  refresh_views();
                  leave_runtime_baron_plane_state(old_state, baron_plane);
                  return;
              }
              if (strategy_type == 0x45)
              {
                  // Hunchback strategy vtable +0x18, 0x000B3E70.
                  refresh_views();
                  leave_runtime_hunchback_state(old_state, hunchback);
                  return;
              }
              if (strategy_type == 0x42)
              {
                  // Haunted door strategy vtable +0x18, 0x000B0520.
                  refresh_views();
                  leave_runtime_haunted_door_state(old_state, haunted_door);
                  return;
              }
              if (strategy_type == 0x52)
              {
                  // Spider strategy vtable +0x18, 0x000B5AF0.
                  refresh_views();
                  leave_runtime_spider_state(old_state, spider);
                  return;
              }
              if (strategy_type == 0x58)
              {
                  // Zombie strategy vtable +0x18, 0x000C5AD0.
                  refresh_views();
                  leave_runtime_zombie_state(old_state, next_state, zombie);
                  return;
              }
              if (strategy_type == 0x3B)
              {
                  // Butler strategy vtable +0x18, 0x000AB220.
                  refresh_views();
                  leave_runtime_butler_state(old_state, butler);
                  return;
              }
              if (strategy_type == 0x39)
              {
                  // Director strategy vtable +0x18, 0x000AA450.
                  leave_runtime_director_state(bytes_of(object), old_state);
                  return;
              }
              // Player strategy vtable +0x18, 0x000BF410.
              if (scene.refresh_pause)
              {
                  scene.refresh_pause();
              }
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
              if (strategy_type == 0x51)
              {
                  // Skeleton strategy vtable +0x14, 0x000BFF80.
                  enter_runtime_skeleton_state(state, previous, skeleton);
                  return;
              }
              if (strategy_type == 0x44)
              {
                  // Haunted TV strategy vtable +0x14, 0x000B2650.
                  enter_runtime_haunted_tv_state(state, previous, haunted_tv);
                  return;
              }
              if (strategy_type == 0x3F)
              {
                  // Grim Reaper strategy vtable +0x14, 0x000ADA70.
                  enter_runtime_grim_reaper_state(state, previous, grim_reaper);
                  return;
              }
              if (strategy_type == 0x3C)
              {
                  // Generic strategy vtable +0x14, 0x000AB770.
                  enter_runtime_generic_state(state, generic);
                  return;
              }
              if (strategy_type == 0x3D)
              {
                  // Ghoulie girl strategy vtable +0x14, 0x000AC3E0.
                  enter_runtime_ghoulie_girl_state(state, ghoulie_girl);
                  return;
              }
              if (strategy_type == 0x40)
              {
                  // Haunted chair strategy vtable +0x14, 0x000AEA10.
                  enter_runtime_haunted_chair_state(state, previous, haunted_chair);
                  return;
              }
              if (strategy_type == 0x4C)
              {
                  // Mummy strategy vtable +0x14, 0x000BC350.
                  enter_runtime_mummy_state(state, previous, mummy);
                  return;
              }
              if (strategy_type == 0x54)
              {
                  // Vampire strategy vtable +0x14, 0x000C2350.
                  enter_runtime_vampire_state(state, previous, vampire);
                  return;
              }
              if (strategy_type == 0x57)
              {
                  // Worm strategy vtable +0x14, 0x000C4B20.
                  enter_runtime_worm_state(state, previous, worm);
                  return;
              }
              if (strategy_type == 0x53)
              {
                  // Ugly girl strategy vtable +0x14, 0x000C1430.
                  enter_runtime_ugly_girl_state(state, previous, ugly_girl);
                  return;
              }
              if (strategy_type == 0x47)
              {
                  // Flying imp strategy vtable +0x14, 0x000B58F0.
                  enter_runtime_imp_flamethrower_state(state, previous, imp_flamethrower);
                  return;
              }
              if (strategy_type == 0x3E)
              {
                  // Giant hand strategy vtable +0x14, 0x000ACBB0.
                  enter_runtime_giant_hand_state(state, previous, giant_hand);
                  return;
              }
              if (strategy_type == 0x43)
              {
                  // Haunted picture strategy vtable +0x14, 0x000B1150.
                  enter_runtime_haunted_picture_state(state, previous, haunted_picture);
                  return;
              }
              if (strategy_type == 0x56)
              {
                  // Warlock strategy vtable +0x14, 0x000C40B0.
                  enter_runtime_warlock_state(state, previous, warlock);
                  return;
              }
              if (strategy_type == 0x41)
              {
                  // Haunted coat strategy vtable +0x14, 0x000AF5A0.
                  enter_runtime_haunted_coat_state(state, previous, haunted_coat);
                  return;
              }
              if (strategy_type == 0x50)
              {
                  // Ribs strategy vtable +0x14, 0x000BF910.
                  enter_runtime_ribs_state(state, previous, ribs);
                  return;
              }
              if (strategy_type == 0x3A)
              {
                  // Chicken strategy vtable +0x14, 0x000AA1D0.
                  enter_runtime_chicken_state(state, chicken);
                  return;
              }
              if (strategy_type == 0x55)
              {
                  // Vampire chicken strategy vtable +0x14, 0x000C2C70.
                  enter_runtime_vampire_chicken_state(state, previous, vampire_chicken);
                  return;
              }
              if (strategy_type == 0x48)
              {
                  // Flying imp strategy vtable +0x14, 0x000B6F80.
                  enter_runtime_imp_flying_state(state, previous, imp_flying);
                  return;
              }
              if (strategy_type == 0x49)
              {
                  // Ninja imp strategy vtable +0x14, 0x000B7E60.
                  enter_runtime_imp_ninja_state(state, previous, imp_ninja);
                  return;
              }
              if (strategy_type == 0x4D)
              {
                  // Cursed mummy strategy vtable +0x14, 0x000BDF90.
                  enter_runtime_mummy_cursed_state(state, mummy_cursed);
                  return;
              }
              if (strategy_type == 0x4A)
              {
                  // Jessie Clyde strategy vtable +0x14, 0x000B9A20.
                  enter_runtime_jessie_clyde_state(state, previous, jessie_clyde);
                  return;
              }
              if (strategy_type == 0x4B)
              {
                  // Medusa strategy vtable +0x14, 0x000BB720.
                  enter_runtime_medusa_state(state, previous, medusa);
                  return;
              }
              if (strategy_type == 0x4E)
              {
                  // Pirate strategy vtable +0x14, 0x000BE820.
                  enter_runtime_pirate_state(state, previous, pirate);
                  return;
              }
              if (strategy_type == 0x37)
              {
                  // Baron strategy vtable +0x14, 0x000A80B0.
                  enter_runtime_baron_state(state, previous, baron);
                  return;
              }
              if (strategy_type == 0x38)
              {
                  // Baron plane strategy vtable +0x14, 0x000A9430.
                  enter_runtime_baron_plane_state(state, previous, baron_plane);
                  return;
              }
              if (strategy_type == 0x45)
              {
                  // Hunchback strategy vtable +0x14, 0x000B3DD0.
                  enter_runtime_hunchback_state(state, previous, hunchback);
                  return;
              }
              if (strategy_type == 0x42)
              {
                  // Haunted door strategy vtable +0x14, 0x000B0340.
                  enter_runtime_haunted_door_state(state, previous, haunted_door);
                  return;
              }
              if (strategy_type == 0x52)
              {
                  // Spider strategy vtable +0x14, 0x000C1040.
                  enter_runtime_spider_state(state, previous, spider);
                  return;
              }
              if (strategy_type == 0x58)
              {
                  // Zombie strategy vtable +0x14, 0x000C5970.
                  enter_runtime_zombie_state(state, previous, zombie);
                  return;
              }
              if (strategy_type == 0x3B)
              {
                  // Butler strategy vtable +0x14, 0x000AB1A0.
                  enter_runtime_butler_state(state, previous, butler);
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
              if (!body_action_key && scene.attach_body_methods)
              {
                  scene.attach_body_methods(*this);
              }
              if (!body_action_key)
              {
                  throw Error("player body vtable +0x20 (0x00075B90) is not attached");
              }
              return body_action_key();
          },
          [this]() -> RuntimeBodyAttachedActor*
          {
              const u32 action = mind.empty() ? 0 : get(mind, mind_field::action_actor);
              if (action == 0)
              {
                  return nullptr;
              }
              if (!scene.attached_actor_at)
              {
                  throw Error("0x0001B810 reads mind +0x18 without the scene's attached actor views");
              }
              return &scene.attached_actor_at(action);
          }},
      attachment_creation{attachment, child, scene_bindings.attachment_view},
      construction{owner_instance.bat.actor, attributes, spawn, body_object, mind_object, strategy_object,
          strategy_setup, input, scene_bindings.input_release, scene_bindings.tag_rows, scene_bindings.object_tags,
          attachment_creation, created_attachment, scene_bindings.initial_channel_word, scene_bindings.player_count,
          scene_bindings.story_flags, matrices, scene_bindings.matrix_boundaries, scene_bindings.address_of},
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
              if (!body_slot_key && scene.attach_body_methods)
              {
                  scene.attach_body_methods(*this);
              }
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
          scene_bindings.scene_kills, scene_bindings.brew_kills_required, scene_bindings.story_byte,
          scene_bindings.intro_pending, scene_bindings.intro_name, scene_bindings.cases,
          scene_bindings.case_body_bits, "idle"},
      target_checks{strategy_goals, attribute_resource, scene_bindings.difficulty, scene_bindings.active_actors,
          scene_bindings.notices, scene_bindings.occupancy_initial},
      target_queue{scene_bindings.target_slots, scene_bindings.queue_limits, scene_bindings.class_queue_limits,
          scene_bindings.target_of, scene_bindings.actor_bytes_of},
      imp_fire{strategy_messages, target_checks, target_queue, scene_bindings.imp_fire_parameters,
          owner_instance.animation, scene_bindings.registry, scene_bindings.create_effect, scene_bindings.object_message,
          scene_bindings.body_at, scene_bindings.body_of, scene_bindings.model_of, scene_bindings.attachment_limit,
          "aid_objparams_ghoulies_fx_mutatehalo", "attack", "mutateup", "jumpbite", "jumpbiteprepare"},
      butler{strategy_messages, scene_bindings.actor_address, scene_bindings.first_live_attachment_at,
          scene_bindings.actor_bytes_at,
          "scared", "far"},
      strategy_views{scene_bindings.actor_at, scene_bindings.actor_bytes_at, scene_bindings.body_of,
          scene_bindings.query, scene_bindings.story_flags, scene_bindings.mind_bytes_at,
          scene_bindings.first_live_attachment_at},
      skeleton{strategy_messages, target_checks, target_queue, strategy_views, scene_bindings.skeleton_fetch_delay,
          "attack", "knockdown", "attackwithprojectile", "attackwithweapon", "duckndodge?", "far", "getweapon"},
      zombie{strategy_messages, target_checks, target_queue, strategy_views, scene_bindings.zombie_parameters,
          "swipeaimless", "swipe", "special_attack", "kickbig", "getweapon"},
      spider{strategy_messages, target_checks, target_queue, strategy_views, "attack", "attack_nodelay",
          "stickywebspit"},
      haunted_tv{strategy_messages, target_checks, strategy_views, scene_bindings.registry, scene_bindings.error,
          scene_bindings.resources, scene_bindings.object_tags, scene_bindings.sounds, scene_bindings.latches,
          collision, spawn.record, spawn.active, actor_instance.live_attachments, animation_resource,
          scene_bindings.loaded_sizes, scene_bindings.attack_tables, scene_bindings.address_of,
          scene_bindings.texture_bytes, scene_bindings.create_effect,
          [this](u32 object)
          {
              return scene.release_frame_object(actor, object);
          },
          scene_bindings.spawns, scene_bindings.story_bits, "generateghouly", "knocked_down", "sleep", "wakeup",
          "wait"},
      grim_reaper{strategy_messages, target_checks, strategy_views, scene_bindings.grim_reaper_parameters, 0x00302B7Cu,
          scene_bindings.sounds, scene_bindings.audio_banks, scene_bindings.blob_shadows, collision, animation_resource,
          scene_bindings.attack_tables, scene_bindings.address_of, actor_instance.startup.resource,
          callout, body_lists,
          [this](u32 slot, u8 value)
          {
              // 0x000F96F0 on the +0x204 and +0x310 models (AL value, stack slot).
              if (actor.model != nullptr && actor.model->runtime != nullptr)
              {
                  set_runtime_model_node_state(*actor.model->runtime, u8(slot), value, scene.node_cache);
              }
              if (auto* secondary = actor_instance.startup.models.secondary.get())
              {
                  set_runtime_model_node_state(*secondary, u8(slot), value, scene.node_cache);
              }
          },
          "dizzy", "knockeddown"},
      haunted_chair{strategy_messages, target_checks, target_queue, strategy_views,
          scene_bindings.haunted_chair_parameters, attributes.resource, "swingkick", "wakeup", "slide",
          "slideattarget", "mimic"},
      generic{strategy_messages, target_checks, target_queue, strategy_views, scene_bindings.damage_tables,
          animation_resource, "far",
          "getweapon", "attack", "attackprojectile", "attackgun", "attackgun_secondary"},
      ghoulie_girl{strategy_messages, target_checks, target_queue, strategy_views, scene_bindings.create_effect,
          scene_bindings.object_message, scene_bindings.latches, "wake", "randomwander", "knockdown"},
      mummy{strategy_messages, target_checks, target_queue, strategy_views,
          [](std::span<const u8> body) -> const RuntimeScriptLookupTable*
          {
              const NativePlayerInstance* owner = active_owner == nullptr ? nullptr
                  : active_owner->find_child_owner(body.data());
              if (owner == nullptr)
              {
                  throw Error("mummy strategy: a target body (+0x73C) belongs to no native player");
              }
              return owner->action_table;
          },
          "attack", "ankh_getout", "ankh_whack", "bondage_bandage", "far", "scared"},
      vampire{strategy_messages, target_checks, target_queue, strategy_views, "knockback", "knockbackfollowup",
          "coffincloseknockback", "knockdown", "coffinopencheer", "coffinclose", "special", "coffinlook",
          "randomwander", "waitingforfollowup",
          [this]()
          {
              if (!scene.knock_back)
              {
                  throw Error("vampire strategy: 0x00020690 is not bound");
              }
              scene.knock_back(*this);
          }},
      worm{strategy_messages, target_checks, target_queue, strategy_views, actor_instance.live_attachments,
          scene_bindings.latches, scene_bindings.create_effect, scene_bindings.release_frame_object,
          [this](u32 address) -> std::span<const u8>
          {
              if (!scene.event_at)
              {
                  throw Error("worm strategy: the animation event view is not bound");
              }
              return scene.event_at(address);
          },
          scene_bindings.flag_names,
          [this](u16 key)
          {
              if (!scene.select_request)
              {
                  throw Error("worm strategy: 0x0001D210 is not bound");
              }
              return scene.select_request(*this, key);
          },
          "far", "attack", "attackprepare"},
      ugly_girl{strategy_messages, owner_instance.startup.attachments, body_lists, "attack", "far"},
      haunted_door{strategy_messages, target_checks, target_queue, strategy_views, body_selection,
          scene_bindings.registry, scene_bindings.error, scene_bindings.loaded_sizes, animation_resource,
          scene_bindings.attack_tables, attributes.resource, scene_bindings.latches, actor_instance.live_attachments,
          self, scene_bindings.overrides, scene_bindings.address_of,
          [this](std::span<const u8> target)
          {
              if (body_bits == nullptr)
              {
                  throw Error("haunted door strategy: 0x0013A5B0 without the body +0x15C bit block");
              }
              const RuntimePlayerAttachmentEligibility eligibility{body, *body_bits, actor, attached};
              return runtime_body_target_surface_distance(actor, eligibility, target);
          },
          [this](u32 address, i32 id, const RuntimeActorCreationOptions* options)
          {
              if (!scene.actor_callouts)
              {
                  throw Error("haunted door strategy: 0x0003C040 over an actor's callouts is not bound");
              }
              scene.actor_callouts(address, u32(id), options); // 0x0003C040
          },
          "knobattack_overlay", "knockdown", "attack_special", "knocked_down", "knocked_down_wait", "sleep",
          "wakeup", "celebrate_knockdown"},
      imp_flamethrower{spider, "fireball", "flamethrower", "wait"},
      chicken{spider, scene_bindings.chicken_parameters, "far", "transform"},
      vampire_chicken{spider, "emerge", "randomwander", "knockbackprepare", "knockdown"},
      imp_flying{spider, scene_bindings.imp_flying_parameters, scene_bindings.create_effect,
          scene_bindings.object_message, "evade", "wingwhackprepare", "arrowdive", "arrowdiveunstuck"},
      imp_ninja{spider, scene_bindings.imp_ninja_parameters, scene_bindings.damage_tables,
          scene_bindings.tag_rows, scene_bindings.object_tags, scene_bindings.create_effect,
          scene_bindings.object_message,
          [this](std::span<const u32> message)
          {
              if (!actor_message)
              {
                  throw Error("Ninja imp strategy: the actor's message slot (vtable +0xC) is not attached");
              }
              actor_message(message);
          },
          "psycheup", "psycheuptraitor", "spinkick", "jumpkickbig", "jumpkick", "innerforce", "innerforceoutof",
          "randommove", "randomwalk", "meditate", "meditateoutof"},
      jessie_clyde{spider, scene_bindings.jessie_clyde_parameters, 0x003031B8u, scene_bindings.create_effect,
          scene_bindings.object_message, scene_bindings.latches, actor_instance.startup.resource, callout, body_lists,
          [this](u32 slot, u8 value)
          {
              // 0x000F96F0 on the +0x204 and +0x310 models (AL value, stack slot).
              if (actor.model != nullptr && actor.model->runtime != nullptr)
              {
                  set_runtime_model_node_state(*actor.model->runtime, u8(slot), value, scene.node_cache);
              }
              if (auto* secondary = actor_instance.startup.models.secondary.get())
              {
                  set_runtime_model_node_state(*secondary, u8(slot), value, scene.node_cache);
              }
          },
          [&bindings = scene_bindings](const Vec3& point, u32 actor, float intensity, float scale, i32 broadcast,
              u32 value)
          {
              if (!bindings.scare_at)
              {
                  throw Error("Jessie Clyde strategy: 0x0013B270's scare is not bound");
              }
              bindings.scare_at(point, actor, intensity, scale, broadcast, value);
          },
          "randomwalk", "angryrandomwalk", "transform", "shout", "knockdown", "specialprepare", "special"},
      mummy_cursed{spider, scene_bindings.mummy_cursed_parameters, 0x00428848u, scene_bindings.create_effect,
          [&bindings = scene_bindings](const char* name, const std::function<void(std::span<u8>)>& patch) -> u32
          {
              if (!bindings.create_patched)
              {
                  throw Error("cursed mummy strategy: 0x00107720 / 0x00107520 for its spawn are not bound");
              }
              return bindings.create_patched(name, patch);
          },
          scene_bindings.object_message,
          [&bindings = scene_bindings](u32 address) -> std::span<u8>
          {
              // The scene object's bytes are writable; the shared binding hands them out read-only.
              const std::span<const u8> bytes = bindings.actor_bytes_at(address);
              return {const_cast<u8*>(bytes.data()), bytes.size()};
          },
          [&bindings = scene_bindings](u32 address) -> EntityBitBlock&
          {
              Actor* actor = address == 0 ? nullptr : bindings.actor_at(address);
              const RuntimeStrategyBodyView body = actor == nullptr ? RuntimeStrategyBodyView{0, {}, nullptr}
                                                                    : bindings.body_of(*actor);
              if (body.bits == nullptr)
              {
                  throw Error("cursed mummy strategy reads the body +0x15C bits of actor " + std::to_string(address)
                      + ", which has none");
              }
              return *body.bits;
          },
          [&bindings = scene_bindings](u32 address) -> std::span<u8>
          {
              Actor* actor = address == 0 ? nullptr : bindings.actor_at(address);
              const RuntimeStrategyBodyView body = actor == nullptr ? RuntimeStrategyBodyView{0, {}, nullptr}
                                                                    : bindings.body_of(*actor);
              if (body.bytes.data() == nullptr)
              {
                  throw Error("cursed mummy strategy reads the body (+0x95C) of actor " + std::to_string(address)
                      + ", which has none");
              }
              return body.bytes;
          },
          [&bindings = scene_bindings](u32 address)
          {
              if (!bindings.activate)
              {
                  throw Error("cursed mummy strategy: 0x00112CC0 is not bound");
              }
              bindings.activate(address);
          },
          [&bindings = scene_bindings](u32 address, float lifetime, const char* name)
          {
              if (!bindings.burst)
              {
                  throw Error("cursed mummy strategy: 0x0013B1F0 is not bound");
              }
              bindings.burst(address, lifetime, name);
          },
          [this](float radius)
          {
              if (!scene.scare_near)
              {
                  throw Error("cursed mummy strategy: 0x0013B660 is not bound");
              }
              scene.scare_near(actor, radius);
          },
          [&bindings = scene_bindings](u32 actor, u32 target, float value)
          {
              if (!bindings.start_meter)
              {
                  throw Error("cursed mummy strategy: 0x0012E5E0 is not bound");
              }
              bindings.start_meter(actor, target, value);
          },
          [&bindings = scene_bindings]()
          {
              if (!bindings.stop_meter)
              {
                  throw Error("cursed mummy strategy: 0x0012E6C0 is not bound");
              }
              bindings.stop_meter();
          },
          [&bindings = scene_bindings]()
          {
              if (!bindings.meter_shown)
              {
                  throw Error("cursed mummy strategy: DAT_00545A18 is not bound");
              }
              return bindings.meter_shown();
          },
          [&bindings = scene_bindings]()
          {
              if (!bindings.meter_actor)
              {
                  throw Error("cursed mummy strategy: DAT_00545A20 is not bound");
              }
              return bindings.meter_actor();
          },
          [this](u32 slot, u8 value)
          {
              // 0x00040D50: 0x000F96F0 on the +0x204 and +0x310 models (BL value, stack slot).
              if (actor.model != nullptr && actor.model->runtime != nullptr)
              {
                  set_runtime_model_node_state(*actor.model->runtime, u8(slot), value, scene.node_cache);
              }
              if (auto* secondary = actor_instance.startup.models.secondary.get())
              {
                  set_runtime_model_node_state(*secondary, u8(slot), value, scene.node_cache);
              }
          },
          actor_instance.startup.resource, callout, body_lists, scene_bindings.game, scene_bindings.runtime_flags,
          "randomwalk", "randomworriedwalk", "evade", "evadeknockdown", "comeintorangeknockdown", "evadespecial",
          "comeintorangespecial", "slabwhackprepare", "slabwhack", "slabwhacksneaky", "scared", "deathcurse"},
      medusa{spider, scene_bindings.medusa_parameters, scene_bindings.light_cones, animation_resource,
          scene_bindings.attack_tables, scene_bindings.address_of, scene_bindings.sounds, body_selection,
          scene_bindings.latches,
          [this]()
          {
              if (body_bits == nullptr)
              {
                  throw Error("Medusa strategy: 0x0001D3D0 without the body +0x15C bit block");
              }
              const RuntimePlayerAttachmentEligibility eligibility{body, *body_bits, actor, attached};
              return runtime_player_body_attachment_radius(eligibility); // 0x0001D3D0
          },
          [this](entity_detail::SegmentProbe& probe) -> RuntimeSegmentContactActor&
          {
              if (!scene.contact_actor)
              {
                  throw Error("Medusa strategy: the scene's segment probe actors are not bound");
              }
              return scene.contact_actor(probe);
          },
          [this](RuntimeContactReport& report)
          {
              if (!scene.contact_report)
              {
                  throw Error("Medusa strategy: the actor's message slot (vtable +0xC) is not bound");
              }
              scene.contact_report(*this, report);
          },
          [this](std::span<const u8> parameters)
          {
              if (!scene.create_object)
              {
                  throw Error("Medusa strategy: 0x00107520 is not bound");
              }
              scene.create_object(parameters);
          },
          scene_bindings.runtime_flags, "specialattack", "spin", "specialattackwalk", "specialattacksucceed",
          "backtonormal", "randomwalk", "attack", "shockcam1", "shockcam2"},
      pirate{strategy_messages, target_checks, target_queue, strategy_views, attachment, scene_bindings.latches,
          scene_bindings.create_effect, scene_bindings.object_message, scene_bindings.actor_address, "attack",
          "attack_knockdown", "attack_knockdown_confused", "angry", "attack_special", "attack_specialnosword",
          "throw_multiplechests", "throw_singlechest", "drink", "far", "getweapon", "getweapon_confused"},
      baron{strategy_messages, target_checks, strategy_views, attachment, scene_bindings.registry, scene_bindings.error,
          scene_bindings.loaded_sizes, scene_bindings.latches, scene_bindings.address_of, scene_bindings.create_effect,
          scene_bindings.object_message,
          [&bindings = scene_bindings]()
          {
              return RuntimeIntroNotices{bindings.runtime_flags, bindings.scene_flags, bindings.intro_pending,
                  bindings.intro_name, bindings.cases, bindings.case_body_bits};
          },
          "fetch_cane", "aloof_prepare", "aloof", "attack", "dizzy", "far", "get_weapon", "knocked_down", "celebrate",
          "worried"},
      baron_plane{strategy_messages, target_checks, strategy_views, collision, animation_resource,
          scene_bindings.attack_tables, scene_bindings.sounds, scene_bindings.pause, scene_bindings.latches,
          scene_bindings.address_of, scene_bindings.create_effect, scene_bindings.object_message,
          [&game = scene_bindings.game]() -> SceneCutsceneRuntime&
          {
              return scene_cutscene_runtime(game);
          },
          [this]()
          {
              if (!scene.markers)
              {
                  throw Error("Baron plane strategy: the scene markers (DAT_0051108C) are not bound");
              }
              return scene.markers();
          },
          "attack", "waitforstand", "far", "gethelp", "celebrate", "spin"},
      hunchback{strategy_messages, target_checks, strategy_views, scene_bindings.hunchback_parameters, collision,
          animation_resource, scene_bindings.attack_tables, actor_instance.live_attachments, scene_bindings.latches,
          axis_device, scene_bindings.keyboard, scene_bindings.create_effect, scene_bindings.object_message,
          [this](u32 actor, u32 id, const RuntimeActorCreationOptions* options)
          {
              if (!scene.actor_callouts)
              {
                  throw Error("hunchback strategy: 0x0003BE20 over an actor's callouts is not bound");
              }
              scene.actor_callouts(actor, id, options);
          },
          "evade", "intofar", "randomrun", "tauntnoturn", "intoblock", "specialattackprepare",
          "specialattackprepare2", "specialattack", "specialattackfinish"},
      ribs{strategy_messages, target_checks, body_selection,
          [this]()
          {
              if (!body_action_key && scene.attach_body_methods)
              {
                  scene.attach_body_methods(*this);
              }
              if (!body_action_key)
              {
                  throw Error("ribs strategy: body vtable +0x20 is not attached");
              }
              return body_action_key();
          },
          [this](u32 slot, u8 value)
          {
              // 0x000F96F0 on the +0x204 and +0x310 models (AL value, stack slot).
              if (actor.model != nullptr && actor.model->runtime != nullptr)
              {
                  set_runtime_model_node_state(*actor.model->runtime, u8(slot), value, scene.node_cache);
              }
              if (auto* secondary = actor_instance.startup.models.secondary.get())
              {
                  set_runtime_model_node_state(*secondary, u8(slot), value, scene.node_cache);
              }
          },
          "far"},
      giant_hand{strategy_messages, actor_instance, attribute_resource,
          [this]()
          {
              if (!scene.body_state)
              {
                  throw Error("giant hand strategy: 0x0001FEB0's body views are not bound");
              }
              return scene.body_state(*this);
          },
          [this](std::span<const u32> message)
          {
              if (!scene.tell_players)
              {
                  throw Error("giant hand strategy: 0x00122D10 is not bound");
              }
              scene.tell_players(message);
          },
          [this](std::span<const u8> message)
          {
              if (!scene.queue_event)
              {
                  throw Error("giant hand strategy: 0x00116810 is not bound");
              }
              scene.queue_event(message);
          },
          [this](std::span<const u8> parameters)
          {
              if (!scene.create_object)
              {
                  throw Error("giant hand strategy: 0x00107520 is not bound");
              }
              scene.create_object(parameters);
          },
          scene_bindings.runtime_flags, scene_bindings.chapter_mode, "grab"},
      haunted_coat{strategy_messages, target_checks, target_queue, strategy_views, body_selection,
          actor_instance.live_attachments, self, scene_bindings.overrides, scene_bindings.occupancy_initial,
          [this]() -> const EntityBitBlock&
          {
              if (body_bits == nullptr)
              {
                  throw Error("haunted coat strategy: 0x000AF750 without the body +0x15C bit block");
              }
              return *body_bits;
          },
          [this](std::span<const u8> target)
          {
              if (body_bits == nullptr)
              {
                  throw Error("haunted coat strategy: 0x00134350 without the body +0x15C bit block");
              }
              const RuntimePlayerAttachmentEligibility eligibility{body, *body_bits, actor, attached};
              return runtime_player_body_point_is_near(eligibility, target);
          },
          [this](std::span<const u32> message)
          {
              if (!scene.object_message || body.empty())
              {
                  throw Error("haunted coat strategy: the body's vtable +0xC is not bound");
              }
              scene.object_message(scene.address_of(body.data()), message);
          },
          "attack", "sleep", "sleep_wall", "wakeup", "wakeup_wall"},
      haunted_picture{strategy_messages, target_checks, target_queue, strategy_views, actor_instance.live_attachments,
          self, scene_bindings.overrides, scene_bindings.haunted_picture_movement, scene_bindings.occupancy_initial,
          "attack", "attack_special", "getrandomdest", "sleep", "wakeup"},
      warlock{strategy_messages, target_checks, strategy_views, actor_instance, body_lists,
          [this]()
          {
              if (!scene.body_state)
              {
                  throw Error("warlock strategy: 0x0001FEB0's body views are not bound");
              }
              return scene.body_state(*this);
          },
          scene_bindings.object_message, scene_bindings.create_effect,
          [this](u32 actor, u32 id, const RuntimeActorCreationOptions* options)
          {
              if (!scene.actor_callouts)
              {
                  throw Error("warlock strategy: 0x0003BE20 over an actor's callouts is not bound");
              }
              scene.actor_callouts(actor, id, options);
          },
          [this](const Actor& source, float radius)
          {
              if (!scene.scare_near)
              {
                  throw Error("warlock strategy: 0x0013B660 is not bound");
              }
              scene.scare_near(source, radius);
          },
          scene_bindings.story_bits,
          [this]() -> RuntimeActorPauseGroups&
          {
              if (scene.refresh_pause)
              {
                  scene.refresh_pause();
              }
              return scene.pause;
          },
          scene_bindings.latches, entity_detail::g_display_view, "reappear", "attackprepare", "attack",
          "attackfinish"}
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

void NativePlayerFactoryOwner::butler_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3B)
    {
        throw Error("butler strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_butler_message(message, player->butler);
}

void NativePlayerFactoryOwner::skeleton_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x51)
    {
        throw Error("skeleton strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_skeleton_message(message, player->skeleton);
}

void NativePlayerFactoryOwner::skeleton_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x51)
    {
        throw Error("skeleton strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_skeleton_strategy(player->skeleton);
}

void NativePlayerFactoryOwner::spider_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x52)
    {
        throw Error("spider strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_spider_message(message, player->spider);
}

void NativePlayerFactoryOwner::spider_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x52)
    {
        throw Error("spider strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_spider_strategy(player->spider);
}

void NativePlayerFactoryOwner::haunted_tv_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x44)
    {
        throw Error("haunted TV strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_haunted_tv_message(message, player->haunted_tv);
}

void NativePlayerFactoryOwner::generic_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3C)
    {
        throw Error("generic strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_generic_message(message, player->generic);
}

void NativePlayerFactoryOwner::ghoulie_girl_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3D)
    {
        throw Error("ghoulie girl strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_ghoulie_girl_message(message, player->ghoulie_girl);
}

void NativePlayerFactoryOwner::ghoulie_girl_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3D)
    {
        throw Error("ghoulie girl strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_ghoulie_girl_strategy(player->ghoulie_girl);
}

void NativePlayerFactoryOwner::generic_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3C)
    {
        throw Error("generic strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_generic_strategy(player->generic);
}

void NativePlayerFactoryOwner::haunted_chair_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x40)
    {
        throw Error("haunted chair strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_haunted_chair_message(message, player->haunted_chair);
}

void NativePlayerFactoryOwner::haunted_chair_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x40)
    {
        throw Error("haunted chair strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_haunted_chair_strategy(player->haunted_chair);
}

void NativePlayerFactoryOwner::grim_reaper_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3F)
    {
        throw Error("Grim Reaper strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_grim_reaper_message(message, player->grim_reaper);
}

void NativePlayerFactoryOwner::grim_reaper_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3F)
    {
        throw Error("Grim Reaper strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_grim_reaper_strategy(player->grim_reaper);
}

void NativePlayerFactoryOwner::mummy_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4C)
    {
        throw Error("mummy strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_mummy_message(message, player->mummy);
}

void NativePlayerFactoryOwner::mummy_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4C)
    {
        throw Error("mummy strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_mummy_strategy(player->mummy);
}

void NativePlayerFactoryOwner::vampire_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x54)
    {
        throw Error("vampire strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_vampire_message(message, player->vampire);
}

void NativePlayerFactoryOwner::vampire_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x54)
    {
        throw Error("vampire strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_vampire_strategy(player->vampire);
}

void NativePlayerFactoryOwner::worm_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x57)
    {
        throw Error("worm strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_worm_message(message, player->worm);
}

void NativePlayerFactoryOwner::worm_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x57)
    {
        throw Error("worm strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_worm_strategy(player->worm);
}

void NativePlayerFactoryOwner::imp_flamethrower_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x47)
    {
        throw Error("flying imp strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_imp_flamethrower_message(message, player->imp_flamethrower);
}

void NativePlayerFactoryOwner::imp_flamethrower_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x47)
    {
        throw Error("flying imp strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_imp_flamethrower_strategy(player->imp_flamethrower);
}

void NativePlayerFactoryOwner::chicken_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3A)
    {
        throw Error("Chicken strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_chicken_message(message, player->chicken);
}

void NativePlayerFactoryOwner::chicken_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3A)
    {
        throw Error("Chicken strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_chicken_strategy(player->chicken);
}

void NativePlayerFactoryOwner::ribs_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x50)
    {
        throw Error("ribs strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_ribs_message(message, player->ribs);
}

void NativePlayerFactoryOwner::giant_hand_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3E)
    {
        throw Error("giant hand strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_giant_hand_message(message, player->giant_hand);
}

void NativePlayerFactoryOwner::haunted_coat_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x41)
    {
        throw Error("haunted coat strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_haunted_coat_message(message, player->haunted_coat);
}

void NativePlayerFactoryOwner::haunted_picture_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x43)
    {
        throw Error("haunted picture strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_haunted_picture_message(message, player->haunted_picture);
}

void NativePlayerFactoryOwner::warlock_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x56)
    {
        throw Error("warlock strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_warlock_message(message, player->warlock);
}

void NativePlayerFactoryOwner::warlock_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x56)
    {
        throw Error("warlock strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_warlock_strategy(player->warlock);
}

void NativePlayerFactoryOwner::haunted_picture_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x43)
    {
        throw Error("haunted picture strategy method has no owning instance");
    }
    if (player->haunted_picture.movement.empty())
    {
        throw Error("haunted picture strategy: the 0x00302E80 movement table is not bound");
    }
    player->refresh_views();
    update_runtime_haunted_picture_strategy(player->haunted_picture);
}

void NativePlayerFactoryOwner::haunted_coat_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x41)
    {
        throw Error("haunted coat strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_haunted_coat_strategy(player->haunted_coat);
}

void NativePlayerFactoryOwner::giant_hand_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3E)
    {
        throw Error("giant hand strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_giant_hand_strategy(player->giant_hand);
}

void NativePlayerFactoryOwner::ribs_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x50)
    {
        throw Error("ribs strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_ribs_strategy(player->ribs);
}

void NativePlayerFactoryOwner::vampire_chicken_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x55)
    {
        throw Error("Vampire chicken strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_vampire_chicken_message(message, player->vampire_chicken);
}

void NativePlayerFactoryOwner::vampire_chicken_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x55)
    {
        throw Error("Vampire chicken strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_vampire_chicken_strategy(player->vampire_chicken);
}

void NativePlayerFactoryOwner::imp_flying_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x48)
    {
        throw Error("Flying imp strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_imp_flying_message(message, player->imp_flying);
}

void NativePlayerFactoryOwner::imp_flying_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x48)
    {
        throw Error("Flying imp strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_imp_flying_strategy(player->imp_flying);
}

void NativePlayerFactoryOwner::imp_ninja_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x49)
    {
        throw Error("Ninja imp strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_imp_ninja_message(message, player->imp_ninja);
}

void NativePlayerFactoryOwner::imp_ninja_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x49)
    {
        throw Error("Ninja imp strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_imp_ninja_strategy(player->imp_ninja);
}

void NativePlayerFactoryOwner::jessie_clyde_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4A)
    {
        throw Error("Jessie Clyde strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_jessie_clyde_message(message, player->jessie_clyde);
}

void NativePlayerFactoryOwner::jessie_clyde_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4A)
    {
        throw Error("Jessie Clyde strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_jessie_clyde_strategy(player->jessie_clyde);
}

void NativePlayerFactoryOwner::mummy_cursed_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4D)
    {
        throw Error("cursed mummy strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_mummy_cursed_message(message, player->mummy_cursed);
}

void NativePlayerFactoryOwner::mummy_cursed_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4D)
    {
        throw Error("cursed mummy strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_mummy_cursed_strategy(player->mummy_cursed);
}

void NativePlayerFactoryOwner::medusa_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4B)
    {
        throw Error("Medusa strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_medusa_message(message, player->medusa);
}

void NativePlayerFactoryOwner::medusa_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4B)
    {
        throw Error("Medusa strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_medusa_strategy(player->medusa);
}

void NativePlayerFactoryOwner::pirate_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4E)
    {
        throw Error("pirate strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_pirate_message(message, player->pirate);
}

void NativePlayerFactoryOwner::pirate_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x4E)
    {
        throw Error("pirate strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_pirate_strategy(player->pirate);
}

void NativePlayerFactoryOwner::baron_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x37)
    {
        throw Error("Baron strategy method has no owning instance");
    }
    player->refresh_views();
    // 0x000A7D30 answers message 0x59 in place.
    dispatch_runtime_baron_message({const_cast<u32*>(message.data()), message.size()}, player->baron);
}

void NativePlayerFactoryOwner::baron_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x37)
    {
        throw Error("Baron strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_baron_strategy(player->baron);
}

void NativePlayerFactoryOwner::baron_plane_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x38)
    {
        throw Error("Baron plane strategy method has no owning instance");
    }
    player->refresh_views();
    // 0x000A91A0 answers message 0x58 in place.
    dispatch_runtime_baron_plane_message({const_cast<u32*>(message.data()), message.size()}, player->baron_plane);
}

void NativePlayerFactoryOwner::baron_plane_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x38)
    {
        throw Error("Baron plane strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_baron_plane_strategy(player->baron_plane);
}

void NativePlayerFactoryOwner::hunchback_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x45)
    {
        throw Error("hunchback strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_hunchback_message(message, player->hunchback);
}

void NativePlayerFactoryOwner::hunchback_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x45)
    {
        throw Error("hunchback strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_hunchback_strategy(player->hunchback);
}

void NativePlayerFactoryOwner::haunted_door_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x42)
    {
        throw Error("haunted door strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_haunted_door_message(message, player->haunted_door);
}

void NativePlayerFactoryOwner::haunted_door_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x42)
    {
        throw Error("haunted door strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_haunted_door_strategy(player->haunted_door);
}

void NativePlayerFactoryOwner::ugly_girl_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x53)
    {
        throw Error("ugly girl strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_ugly_girl_message(message, player->ugly_girl);
}

void NativePlayerFactoryOwner::ugly_girl_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x53)
    {
        throw Error("ugly girl strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_ugly_girl_strategy(player->ugly_girl);
}

void NativePlayerFactoryOwner::haunted_tv_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x44)
    {
        throw Error("haunted TV strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_haunted_tv_strategy(player->haunted_tv);
}

void NativePlayerFactoryOwner::zombie_message(FactoryObject& strategy, std::span<const u32> message)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x58)
    {
        throw Error("zombie strategy method has no owning instance");
    }
    player->refresh_views();
    dispatch_runtime_zombie_message(message, player->zombie);
}

void NativePlayerFactoryOwner::zombie_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x58)
    {
        throw Error("zombie strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_zombie_strategy(player->zombie);
}

void NativePlayerFactoryOwner::butler_update(FactoryObject& strategy)
{
    auto* player = find_player(strategy);
    if (player == nullptr || player->strategy.data() != strategy.bytes.get() || player->strategy_type != 0x3B)
    {
        throw Error("butler strategy method has no owning instance");
    }
    player->refresh_views();
    update_runtime_butler_strategy(player->butler);
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
    // Lifecycle views of retail getters 0x00027F60, 0x0001DA50, 0x000733F0, 0x00075AF0, 0x000761A0, 0x0002B6C0, 0x000C57A0, 0x000C0B60,
    // 0x000BED40, 0x000B4480 and 0x000AB040; types 1 and 2 stay with the actor owner, types 7 and 0x34 with the mind owner.
    types[5] = ObjectFactoryType{2, player_lifecycle};
    types[6] = ObjectFactoryType{1, parent_body_lifecycle};
    types[0x27] = ObjectFactoryType{6, player_body_parent_lifecycle};
    types[0x2A] = ObjectFactoryType{0x27, player_body_lifecycle};
    types[0x2D] = ObjectFactoryType{0x27, zombie_body_lifecycle};
    types[0x2C] = ObjectFactoryType{0x27, vampire_body_lifecycle};
    types[0x2B] = ObjectFactoryType{0x27, worm_body_lifecycle};
    types[0x29] = ObjectFactoryType{0x27, jessie_clyde_body_lifecycle};
    types[8] = ObjectFactoryType{1, base_strategy_lifecycle};
    types[0x4F] = ObjectFactoryType{8, player_strategy_lifecycle};
    types[0x46] = ObjectFactoryType{8, imp_fire_strategy_lifecycle};
    types[0x3B] = ObjectFactoryType{8, butler_strategy_lifecycle};
    types[0x51] = ObjectFactoryType{8, skeleton_strategy_lifecycle};
    types[0x58] = ObjectFactoryType{8, zombie_strategy_lifecycle};
    types[0x52] = ObjectFactoryType{8, spider_strategy_lifecycle};
    types[0x44] = ObjectFactoryType{8, haunted_tv_strategy_lifecycle};
    types[0x40] = ObjectFactoryType{8, haunted_chair_strategy_lifecycle};
    types[0x3C] = ObjectFactoryType{8, generic_strategy_lifecycle};
    types[0x3D] = ObjectFactoryType{8, ghoulie_girl_strategy_lifecycle}; // Getter 0x000AC160.
    types[0x3F] = ObjectFactoryType{8, grim_reaper_strategy_lifecycle};
    types[0x4C] = ObjectFactoryType{8, mummy_strategy_lifecycle};
    types[0x54] = ObjectFactoryType{8, vampire_strategy_lifecycle}; // Getter 0x000C2000.
    types[0x57] = ObjectFactoryType{8, worm_strategy_lifecycle}; // Getter 0x000C4580.
    types[0x53] = ObjectFactoryType{8, ugly_girl_strategy_lifecycle};
    types[0x42] = ObjectFactoryType{8, haunted_door_strategy_lifecycle};
    types[0x47] = ObjectFactoryType{8, imp_flamethrower_strategy_lifecycle};
    types[0x3A] = ObjectFactoryType{8, chicken_strategy_lifecycle};
    types[0x48] = ObjectFactoryType{8, imp_flying_strategy_lifecycle};
    types[0x55] = ObjectFactoryType{8, vampire_chicken_strategy_lifecycle};
    types[0x50] = ObjectFactoryType{8, ribs_strategy_lifecycle};
    types[0x3E] = ObjectFactoryType{8, giant_hand_strategy_lifecycle};
    types[0x41] = ObjectFactoryType{8, haunted_coat_strategy_lifecycle};
    types[0x43] = ObjectFactoryType{8, haunted_picture_strategy_lifecycle};
    types[0x56] = ObjectFactoryType{8, warlock_strategy_lifecycle}; // Getter 0x000C3CC0.
    types[0x49] = ObjectFactoryType{8, imp_ninja_strategy_lifecycle};
    types[0x4A] = ObjectFactoryType{8, jessie_clyde_strategy_lifecycle};
    types[0x4D] = ObjectFactoryType{8, mummy_cursed_strategy_lifecycle};
    types[0x4B] = ObjectFactoryType{8, medusa_strategy_lifecycle};
    types[0x4E] = ObjectFactoryType{8, pirate_strategy_lifecycle};
    types[0x37] = ObjectFactoryType{8, baron_strategy_lifecycle};
    types[0x38] = ObjectFactoryType{8, baron_plane_strategy_lifecycle};
    types[0x45] = ObjectFactoryType{8, hunchback_strategy_lifecycle};
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
    result->controller.message = [](GhoulyController& controller, void* message)
    {
        if (!active_owner->bindings.scene.controller_message)
        {
            throw Error("a player's ghouly spawn controller (actor vtable +0xC) is not bound");
        }
        active_owner->bindings.scene.controller_message(controller, message);
    };
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

NativePlayerInstance* NativePlayerFactoryOwner::find_player_at(u32 actor_address)
{
    const auto found = addresses_.find(actor_address);
    return found == addresses_.end() ? nullptr : found->second;
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
        FactoryObject& weapon = found->second->created_attachment;
        if (weapon.bytes != nullptr)
        {
            if (!bindings.scene.keep_object)
            {
                throw Error("a released player's created weapon has no scene object list to go to");
            }
            bindings.scene.keep_object(std::move(weapon));
        }
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
