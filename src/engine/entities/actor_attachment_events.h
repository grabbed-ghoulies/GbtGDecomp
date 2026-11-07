#pragma once

#include "actor_attachment_requests.h"
#include "gbtg/actor_spawn_factory.h"

namespace gbtg
{
// An attachment event record (0x0003BEC0 runs it by kind): a chain of records of varying size, each with a callout's
// name, a resource named at +0x48, then words by kind. Reads are by field, as a record can be shorter than kSize.
namespace attachment_event_field
{
constexpr RetailField<u16, 0x00> size{}; // the next record is this far on
constexpr RetailField<u16, 0x04> kind{}; // 0 ends the chain (0x0003BE20)
constexpr RetailField<std::array<char, 0x40>, 0x08> callout{};
constexpr RetailField<std::array<char, 0x80>, 0x48> name{};
constexpr u32 kSize = 0xD4;
} // namespace attachment_event_field
// Kind 1: a sound on a slot, then an effect stream when +0xCC names one ("aid_").
namespace sound_attachment_event
{
constexpr RetailField<u32, 0xC8> slot{};
constexpr RetailField<std::array<char, 8>, 0xCC> effect{}; // a name that may run on past this field's end
} // namespace sound_attachment_event
// Kind 4: an attachment at a joint, if it has one.
namespace joint_attachment_event
{
constexpr RetailField<u32, 0xC8> has_joint{};
constexpr RetailField<i16, 0xCC> joint{};
constexpr RetailField<u32, 0xD0> flags{};
} // namespace joint_attachment_event
// Kind 5: an attachment at a model binding of a kind.
namespace binding_attachment_event
{
constexpr RetailField<i16, 0xC8> binding_kind{};
constexpr RetailField<u32, 0xCC> flags{};
} // namespace binding_attachment_event
// Kind 6: an attachment at an index, unless +0xC8 is set.
namespace index_attachment_event
{
constexpr RetailField<u32, 0xC8> no_index{};
constexpr RetailField<i32, 0xCC> index{};
constexpr RetailField<u32, 0xD0> flags{};
} // namespace index_attachment_event

struct ActorAttachmentEventContext
{
    Actor& actor;
    ActorAttachmentList& attachments;
    ActorEffectStreamContext& effects;
    ObjectFactoryRegistry& factory;
    std::span<ExecutableResourcePool> resources;
    std::vector<FactoryObject>& objects; // host ownership of factory allocations
    u32 actor_address; // original data pointer at parameters+8, resolved by the factory's native views
    const std::unordered_map<const u8*, const u8*>& linked_records; // relocated record+0xC8 pointers
    std::span<u8> command_attachments; // Native view of callback parameter 4 at body +0x2D8.
};
using ActorAttachmentEventCallback = void (*)(const u8*, const RuntimeActorCreationOptions*,
    ActorAttachmentEventContext&, u32);
void request_attachment_joint_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BBF0
void request_attachment_binding_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC20
u16 request_attachment_index_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC40
void request_attachment_camera_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BC70
void request_attachment_resource_object(const u8* record,
    ActorAttachmentEventContext& context); // Xbox 0x0003BCE0
void request_attachment_camera_shake(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BD10
void request_attachment_sound_effect(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003BD80
void dispatch_actor_attachment_event(const u8* record, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value); // Xbox 0x0003BEC0
i32 dispatch_actor_attachment_records(const u8* record, i32 id,
    ActorAttachmentEventCallback callback, const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context, u32 value); // Xbox 0x0003BE20
void dispatch_actor_resource_callout(const RuntimeActorResourceReference& resource, i32 id,
    const RuntimeActorCreationOptions* options,
    ActorAttachmentEventContext& context); // Xbox 0x0003C040
}
