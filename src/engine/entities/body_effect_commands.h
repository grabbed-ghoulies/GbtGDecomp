#pragma once
#include "body_animation_commands.h"
#include "actor_template_creation.h"

namespace gbtg
{
struct RuntimeBodyEffectCommands
{
    Actor& actor;
    i32 floor; // Actor +0xCEC.
    const char* scene_name; // DAT_004DEA54.
    std::span<const char* const> surfaces; // DAT_004340A0.
    std::span<const char* const> tags; // DAT_0042A850.
    const char* suffix_zero; // DAT_00315A98.
    const char* suffix_one; // DAT_00315AE0.
    const char* side_zero; // DAT_00315AFC.
    const char* side_one; // DAT_00315AF4.
    ActorEffectStreamContext& effects;
};
bool format_runtime_body_surface_effect(const char* tag, u8 surface,
    const char* suffix, const char* side, char* output,
    RuntimeBodyEffectCommands& context); // Xbox 0x0013DEF0
bool select_runtime_body_surface_effect(std::span<const u8> body,
    const Vec3& position, i32 alternate, const char* side, char* output,
    RuntimeBodyEffectCommands& context); // Xbox 0x0001A130
void create_runtime_body_surface_effect(std::span<const u8> body,
    std::span<const u8> command, const Vec3& position, const char* side,
    const RuntimeActorCreationOptions* options,
    RuntimeBodyEffectCommands& context); // Xbox 0x0001A220
void command_runtime_body_surface_effect(std::span<const u8> body,
    std::span<const u8> command, RuntimeBodyEffectCommands& context); // Xbox 0x00019C60
struct RuntimeBodyProjectileCreation
{
    Actor& actor;
    RuntimeBodyAttachedActor& host;
    std::span<ExecutableResourcePool> resources;
    ObjectFactoryRegistry& factory;
    const Vec3& default_position;
    const Vec3& default_euler;
    std::vector<FactoryObject>& objects; // The scene's objects, which hold the projectile from its creation.
    std::function<RuntimeBodyAttachedActor&(const u8* bytes)> attached; // A scene object's attached view.
};
// The projectile is live as soon as it is made: the scene holds it before it is aimed (message 0xA5) and the host
// hears of it (message 0x5F).
void command_runtime_body_projectile(std::span<const u8> body, std::span<const u8> command,
    RuntimeBodyProjectileCreation& context); // Xbox 0x0001A3C0
}
