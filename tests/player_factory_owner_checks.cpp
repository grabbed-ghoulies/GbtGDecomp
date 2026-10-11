#include "player_factory_owner_checks.h"
#include "../src/engine/entities/player_factory_owner.h"
#include "../src/engine/entities/entity_heap.h"
#include "gbtg/model_runtime.h"
#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
using namespace gbtg;
void require(bool value, const char* message)
{
    if (!value)
    {
        throw Error(message);
    }
}

void put16(std::span<u8> bytes, size_t offset, u16 value)
{
    std::memcpy(bytes.data() + offset, &value, 2);
}

void put32(std::span<u8> bytes, size_t offset, u32 value)
{
    std::memcpy(bytes.data() + offset, &value, 4);
}

// Scene contexts the constructor paths never read; they only satisfy references.
template <typename T>
T& unused()
{
    alignas(64) static u8 storage[0x4000]{};
    return *reinterpret_cast<T*>(storage);
}
}

void check_player_factory_owner()
{
    // Actor owner (types 1, 2) over a full-size type table.
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    error.suppressed = 1;
    RuntimeModelResourceViews models;
    AnimationTableResourceViews tables;
    std::unordered_map<const u8*, ActorModel> model_views;
    RuntimeModelDeferredPool deferred{};
    u32 fence = 0;
    RuntimeShaderRegistrationState shaders{};
    RuntimeShaderAnimationScratch scratch{};
    const std::function<void(u8*)> free = [](u8* allocation)
    {
        engine_heap_free(allocation);
    };
    RuntimeActorLink live{};
    u32 random = 11;
    RuntimeActorStartupContext startup{registry, error, models, tables, model_views, deferred, fence, {}, shaders,
        scratch, free, live, random};
    std::vector<ObjectFactoryType> types(0x60);
    ObjectFactoryRegistry factory{types};
    RuntimeActorSoundQueues sounds{};
    std::unordered_map<ActorFrameAttachment*, FactoryObject*> frames;
    const char empty[4]{};
    NativeActorFactoryOwner actors{NativeActorFactoryBindings{startup, factory, sounds, frames,
        [](Actor&, const void*)
        {
            throw Error("player fixture actor sent an unexpected render message");
        },
        empty}};
    actors.install(types);

    // Child templates the player constructor clones.
    std::vector<u8> body_template(0x820);
    put16(body_template, 0, 0x820);
    put16(body_template, 4, 0x2A);
    std::vector<u8> mind_template(0xA4);
    put16(mind_template, 0, 0xA4);
    put16(mind_template, 4, 0x34);
    std::vector<u8> strategy_template(0x18);
    put16(strategy_template, 0, 0x18);
    put16(strategy_template, 4, 0x4F);
    put16(strategy_template, 0xC, 1000);
    std::array<ExecutableResourceRecord, 3> records{{{"aid_test_player_body", body_template},
        {"aid_test_player_mind", mind_template}, {"aid_test_player_strategy", strategy_template}}};
    std::vector<ExecutableResourcePool> pools;
    register_executable_resource_pool(pools, records, "test", true);

    // Scene bindings.
    std::unordered_map<u32, std::span<u8>> input_addresses;
    const RuntimeInputReleaseContext release{input_addresses};
    GhoulySpawnPool entries{0x50, 1, nullptr, nullptr, nullptr};
    i32 selected = 0;
    std::array<u8, 0x3C> keyboard{};
    std::array<u32, 5> ports{};
    RuntimeInputActivationContext activation{entries, selected, {}, keyboard, ports.data() + 1, input_addresses};
    const u32 runtime_flags = 0;
    const u32 story_flags = 0;
    u32 player_count = 0;
    u32 active_strategies = 0;
    std::array<u8, 0x100> address_zero{};
    std::unordered_map<const void*, u32> addresses;
    u32 next_address = 0x20000000;
    const auto address_of = [&](const void* native)
    {
        auto [found, inserted] = addresses.try_emplace(native, next_address);
        if (inserted)
        {
            next_address += 0x1000;
        }
        return found->second;
    };
    std::array<const char*, 0xE9> actor_tags{};
    actor_tags.fill("");
    std::array<u8, 0x87> tag_rows{};
    const char* weapon_names[]{""};
    const char* class_names[]{""};
    const RuntimeShaderMatrixBoundaries matrix_boundaries{};
    const std::unordered_map<const u8*, RuntimePlayerAttachmentActor*> attachment_objects;
    const std::unordered_map<RuntimeBodyAttachedActor*, RuntimePlayerAttachmentActor*> attachment_actors;
    std::array<u8, 12> curve{};
    const std::unordered_map<const u8*, const RuntimeScriptLookupTable*> action_tables;
    const std::unordered_map<const u8*, const u8*> sequences;
    i32 cache_count = 0;
    const std::unordered_map<u32, const char*> action_names;
    static const RuntimeBodyModelNodes no_nodes{};
    const std::array<u16, 2> settings_rows{};
    const RuntimeMindClassSettings mind_settings{0, {reinterpret_cast<const u8*>(settings_rows.data()),
        sizeof(settings_rows)}};
    u32 intro_enabled = 0;
    float intro_scale = 0.f;
    i16 intro_voice = 0;
    const u8 intro_configuration = 0;
    RuntimePlayerIntroSound intro{intro_enabled, intro_scale, intro_voice, intro_configuration, sounds};
    const i32 camera_players = 1;
    u32 camera_messages = 0;
    RuntimePlayerCameraContext camera{factory, pools, "aid_missing_player_camera", address_zero, runtime_flags,
        camera_players,
        [&](FactoryObject& object, const std::array<u32, 3>& message)
        {
            require(object.bytes == nullptr && message[0] == 0xBA, "missing camera receives its 0xBA notice");
            ++camera_messages;
        }};
    std::vector<FactoryObject> cameras;
    const u8 scene_flags = 0;
    const i32 health = 7;
    const i32 chapter_mode = 1;
    const RuntimeStrategyClassRanges strategy_ranges{0, {}};
    std::array<u32, 2> pause_references{};
    RuntimePauseActor* first_pause = nullptr;
    RuntimeActorPauseGroups pause{pause_references, first_pause};
    u32 override_words[8]{};
    RuntimePlayerScriptOverrides overrides{override_words[0], override_words[1], override_words[2], override_words[3],
        override_words[4], override_words[5], override_words[6], override_words[7]};
    entity_detail::LatchNode* latches = nullptr;
    float freeze = 0.f;
    float dt = 0.1f;
    float scale = 1.f;
    u32 block_decay = 0;
    std::array<u8, 4> decay_enabled{1, 1, 1, 1};
    u32 decay_index = 0;
    const RuntimeActorClock clock{freeze, dt, scale, block_decay, decay_enabled, decay_index};
    std::array<u32, 2> resume_references{};
    std::array<u8, 4> resume_table{};
    const RuntimePlayerStrategyResume resume{resume_references, resume_table, decay_index};
    std::unordered_map<scene_detail::CamFocus*, RuntimeScriptActorQueryView> query_views;
    const RuntimeScriptActorQueryContext query{nullptr, query_views, address_zero.data()};
    RuntimeMindTarget* targets = nullptr;
    const void* script_records = nullptr;
    std::array<u8, 0x200> descriptors{};
    std::array<u8, 0x200> request_sizes{};
    std::string goal_scratch;
    ModelNodeFlagCache node_cache{};
    const AnimationMatrix reset_matrix{1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    std::vector<FactoryObject> callout_objects;
    const std::unordered_map<const u8*, const u8*> linked_records;
    u32 brews_taken = 0;
    u32 brew_accepted = 0;
    const i32 prompt_lock = -1;
    const u32 prompt_owner = 0;
    const i32 difficulty = 1;
    const u8 unlock_flags = 0;
    const i32 brews_owned = 0;
    const i32 brews_required = 0;
    u8 story_byte = 0;
    u32 intro_pending = 0;
    std::array<char, 0x40> intro_name{};
    const entity_detail::ListHead cases{};
    NativePlayerFactoryOwner players{NativePlayerFactoryBindings{actors, NativePlayerSceneBindings{factory, pools,
        registry, error, random, release, activation, runtime_flags, story_flags, player_count, active_strategies,
        address_zero, address_of, actor_tags.data(), tag_rows.data(), tag_rows, weapon_names, class_names, 0,
        matrix_boundaries, attachment_objects, attachment_actors, {}, 0, {}, 0, curve, curve, {}, action_tables,
        sequences, {}, cache_count, action_names, {},
        [](const Actor&)
        {
            return NativePlayerModelViews{&no_nodes, nullptr};
        },
        mind_settings, intro, camera, cameras, scene_flags, health, chapter_mode, "test", strategy_ranges, pause,
        overrides, latches, unused<Game>(), clock, resume, query,
        [](u32) -> RuntimeStrategyMessageActor*
        {
            return nullptr;
        },
        [](u32) -> RuntimeMindTarget*
        {
            return nullptr;
        },
        targets, script_records, descriptors, request_sizes, nullptr, goal_scratch,
        [](std::span<const u8>) -> const RuntimeMindGoalTable*
        {
            return nullptr;
        },
        unused<ActorEffectStreamContext>(), node_cache, reset_matrix, nullptr, callout_objects, linked_records,
        unused<RuntimeBrewMeter>(), unused<RuntimePlayerHealthDisplay>(), unused<RuntimeDialogueInputState>(),
        keyboard, unused<AudioEventVoices>(), brews_taken, brew_accepted, prompt_lock, prompt_owner, difficulty,
        unlock_flags, brews_owned, brews_required, story_byte, intro_pending, intro_name, cases,
        [](const entity_detail::ListHead*) -> const EntityBitBlock*
        {
            return nullptr;
        }}}};
    players.install(types);
    require(types[5].parent == 2 && types[6].parent == 1 && types[0x27].parent == 6 && types[0x2A].parent == 0x27
        && types[8].parent == 1 && types[0x4F].parent == 8 && types[7].parent == 1 && types[0x34].parent == 7,
        "player owner installs the retail parent chains");
    require(types[5].vtable()->address == 0x004DD6B8 && types[0x2A].vtable()->address == 0x00505248
        && types[0x4F].vtable()->address == 0x00505FA0, "player owner installs the retail vtable addresses");

    // A player whose parameters name a body and a mind but no strategy.
    std::vector<u8> parameters(0x63C);
    put16(parameters, 0, 0x63C);
    put16(parameters, 4, 5);
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x43A), "aid_test_player_body");
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x3BA), "aid_test_player_mind");
    FactoryObject actor = create_factory_object(factory, parameters);
    require(actor.bytes != nullptr && actor.size == 0xCF8, "player construction through types 1, 2 and 5");
    auto& player = players.player(actor);
    require(player.body_object.bytes != nullptr && player.body_object.size == 0x950
        && player.mind_object.bytes != nullptr && player.mind_object.size == 0x198
        && player.strategy_object.bytes == nullptr, "player constructor creates its body and mind children");
    require(players.find_player(player.body_object) == &player && players.find_player(player.mind_object) == &player
        && &players.player_at(player.actor_address) == &player, "children resolve to their player");
    require(read_u32(player.body, 8) == player.actor_address && read_u32(player.mind, 8) == player.actor_address,
        "body and mind constructors store the actor address from their cloned parameters");
    require(read_u16(player.body, 0x94) == 7 && read_u16(player.body, 0x96) == 7 && camera_messages == 1
        && player.mind_present && player.mind_requests == &player.requests,
        "player mind constructor writes body health and sends its camera notice");
    require(player.input.data() != nullptr && player.actor_bytes[0x298] == 1, "player constructor input and flag");

    // The strategy chain on its own: types 1, 8 and 0x4F with the player's address.
    std::vector<u8> strategy_parameters(0x18);
    put16(strategy_parameters, 0, 0x18);
    put16(strategy_parameters, 4, 0x4F);
    put32(strategy_parameters, 8, player.actor_address);
    FactoryObject strategy = create_factory_object(factory, strategy_parameters);
    require(strategy.bytes != nullptr && strategy.size == 0x54 && players.find_player(strategy) == &player
        && player.strategy.data() == strategy.bytes.get(), "strategy construction resolves its player");
    const u32 alert_message[]{0x2C};
    players.strategy_message(strategy, alert_message);
    require(read_u32(player.strategy, 0x3C) == 1, "strategy owner dispatches the player virtual message override");
    release_factory_object(factory, strategy);
    require(players.find_player(strategy) == nullptr && player.strategy.empty(), "strategy release unbinds it");

    // A second player whose constructor also creates its strategy (0x00028EB0 leaves and enters state 1000).
    std::strcpy(reinterpret_cast<char*>(parameters.data() + 0x33A), "aid_test_player_strategy");
    FactoryObject second = create_factory_object(factory, parameters);
    auto& second_player = players.player(second);
    require(second.bytes != nullptr && second_player.strategy_object.bytes != nullptr
        && players.find_player(second_player.strategy_object) == &second_player
        && read_u16(second_player.strategy, 0xE) == 1000 && read_u32(second_player.strategy, 8)
            == second_player.actor_address, "player constructor creates and enters its strategy");
    release_factory_object(factory, second);
    require(players.player_total() == 1, "second player release keeps the first player");

    release_factory_object(factory, actor);
    require(actor.bytes == nullptr && factory.count == 0 && players.child_count() == 0 && players.player_total() == 0
        && actors.actors.empty(), "player release destroys its children and both owners' views");
    for (auto& object : cameras)
    {
        require(object.bytes == nullptr, "missing camera resource yields no camera object");
    }
}
