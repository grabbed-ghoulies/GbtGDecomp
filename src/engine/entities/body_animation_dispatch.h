#pragma once
#include "body_effect_commands.h"
#include "player_attachment.h"
#include "../audio/body_sound_commands.h"

namespace gbtg
{
struct RuntimeBodyAnimationDispatch
{
    Actor& actor;
    EntityBitBlock& flags;
    RuntimeBodyCollisionState*& collision;
    entity_detail::LatchNode*& latches;
    RuntimeBodyCommandAttachment* attached_shapes;
    const RuntimeBodyCommandTarget* target;
    RuntimeBodySoundCommandContext& sounds;
    RuntimePlayerAttachmentCreation& attachments;
    RuntimeBodyProjectileCreation& projectiles;
    std::vector<FactoryObject>& created_objects;
    std::span<char, 128> name_scratch;
    const std::function<void(RuntimeBodyAttachedActor&, std::span<char, 128>)>& query_name; // Actual actor message 0x99.
    RuntimeBodyEffectCommands& effects;
    u32& effect_flags;
    const ActorAnimationState& animation;
    RuntimeBodyCommandCallout* primary_callout;
    RuntimeBodyCommandCallout* secondary_callout;
    std::span<const char* const, 0x55> attachment_names;
    std::span<const char* const, 0x41> flag_names;
    RuntimeActorProximityNode*& proximity;
    u32 body_address;
    const u8* attachment_record; // Actor +0x8C, the callout records 0x0001AE00 walks into body +0x2D8.
    ActorAttachmentEventContext& attachment_events;
    RuntimeBodyAttachmentLists& attachment_lists;
};
u32 dispatch_runtime_body_animation(std::span<u8> body, std::span<const u8> message,
    std::span<const u8> command,
    RuntimeBodyAnimationDispatch& context); // Xbox 0x0001A8F0
}
