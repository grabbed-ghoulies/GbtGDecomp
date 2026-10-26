#pragma once
#include "gbtg/entity.h"
#include <functional>
#include <span>
#include "ghouly_reference_names.h"
#include "body_death_drops.h"
#include "player_attachment.h"

namespace gbtg
{
struct Game;
struct RuntimeBodyDeathNotice
{
    Game& game;
    std::span<u8>& body; // Actual actor +0x95C, reread after callbacks.
    u32 actor_address;
    const char* name; // Actor +0xBA0.
    GhoulySpawnRecord*& spawn_record; // Actor +0xB98.
    std::span<ExecutableResourcePool> resources;
};
void notify_runtime_body_death_classes(RuntimeBodyDeathNotice& context); // Xbox 0x00134EF0
struct RuntimeBodyDeathEffects
{
    RuntimeBodyDeathNotice& notices;
    RuntimeBodyDeathDrops& drops;
    std::span<const u8>& attributes; // Body +0x268, reread after attachment release.
    u32& live; // Actor +0xCE0.
    u32& flags; // DAT_0050EA98.
    const char* default_name; // Actual 0x000411A0 initializer name.
    const char* fallback_name; // DAT_00546208.
    RuntimePlayerAttachment& attachment;
};
void create_runtime_body_death_effect(const char* name, u32 activate,
    RuntimeBodyDeathEffects& context); // Xbox 0x00134C10
void create_runtime_body_named_death_effect(const char* name, const char* variant,
    u32 activate, RuntimeBodyDeathEffects& context); // Xbox 0x00134D20
void request_runtime_body_death_effect(u32 activate,
    RuntimeBodyDeathEffects& context); // Xbox 0x00024890
void finish_runtime_body_death(u32 create_effect, u32& scene_flags,
    u32& restart_requested, RuntimeBodyDeathEffects& context); // Xbox 0x00134AE0
void initialize_runtime_body_death_effect(std::span<u8, 0x55C> parameters,
    const Actor& actor, u32 actor_address, float first, float lifetime,
    const char* default_name); // Xbox 0x00068770
// The actor bursts into bits (0x00068770's effect, `name` at +0x3B8 when set) for `lifetime` seconds; `create` is
// 0x00107520 into the scene's objects.
void burst_runtime_actor_into_bits(const Actor& actor, u32 actor_address, float lifetime, const char* name,
    const char* default_name, const std::function<void(std::span<const u8>)>& create); // Xbox 0x0013B1F0, ESI name
}
