#include "actor_live_attachments.h"
#include "entity_heap.h"
#include "../camera/camera.h"
#include "shape_copy.h"

#include <cstring>

namespace gbtg
{
std::array<ActorAttachmentSlot, 20> g_actor_attachment_slots;

// Xbox 0x0012B1A0
void suspend_actor_live_attachment(ActorLiveAttachment& record, bool suspend)
{
    if (suspend)
    {
        if (record.retained_18 == 0)
        {
            ActorAttachmentReleaseMessage message{0x8D, &record, 1};
            broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message));
            release_actor_attachment_slot(&record);
        }
        ++record.retained_18;
        return;
    }
    --record.retained_18;
}

// Xbox 0x0012AFE0
ActorLiveAttachment* allocate_actor_live_attachment(Actor& actor,
    entity_detail::ListHead& owner, entity_detail::ListHead& global,
    u8 mode, u8 flag, u16 tag, const u8* shape, const Vec3& origin)
{
    auto* record = static_cast<ActorLiveAttachment*>(engine_heap_allocate(sizeof(ActorLiveAttachment)));
    std::memset(record, 0, sizeof(*record));
    record->mode = mode;
    record->actor = &actor;
    record->tag = tag;
    auto* head = &owner;
    while (head->prev != nullptr)
    {
        head = static_cast<entity_detail::ListHead*>(head->prev);
    }
    record->owner_link.next = head->next;
    head->next = record;
    record->owner_link.prev = head;
    if (record->owner_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record->owner_link.next)->prev = record;
    }
    head = &global;
    while (head->prev != nullptr)
    {
        head = static_cast<entity_detail::ListHead*>(head->prev);
    }
    record->global_link.next = head->next;
    head->next = &record->global_link;
    record->global_link.prev = head;
    if (record->global_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record->global_link.next)->prev = &record->global_link;
    }
    copy_actor_shape(record->shape.data(), shape);
    copy_actor_shape(record->secondary_shape.data(), shape);
    record->origin = origin;
    record->flag = flag;
    return record;
}

// Xbox 0x00122C70
std::function<void(const i32* message)> g_actor_broadcast;

void broadcast_actor_pulse_message(const i32* message)
{
    if (g_actor_broadcast)
    {
        g_actor_broadcast(message);
        return;
    }
    auto* node = scene_detail::g_pulse_nodes;
    while (node != nullptr)
    {
        auto* owner = node;
        node = node->next;
        owner->on_message(owner, message);
    }
}

// Xbox 0x00112910
void release_actor_attachment_slot(ActorLiveAttachment* owner)
{
    auto* slot = g_actor_attachment_slots.data();
    do
    {
        if (slot->owner == owner)
        {
            auto* record = static_cast<entity_detail::ListHead*>(slot->records.next);
            while (record != nullptr)
            {
                record = static_cast<entity_detail::ListHead*>(slot->records.next);
                if (record->prev != nullptr)
                {
                    static_cast<entity_detail::ListHead*>(record->prev)->next = record->next;
                }
                if (record->next != nullptr)
                {
                    static_cast<entity_detail::ListHead*>(record->next)->prev = record->prev;
                }
                record->prev = nullptr;
                record->next = nullptr;
                record = static_cast<entity_detail::ListHead*>(slot->records.next);
            }
            slot->owner = nullptr;
            return;
        }
        ++slot;
    }
    while (slot < g_actor_attachment_slots.data() + 20);
}

// Xbox 0x0012B0B0
void release_actor_live_attachment(ActorLiveAttachment& record)
{
    ActorAttachmentReleaseMessage message;
    message.record = &record;
    broadcast_actor_pulse_message(reinterpret_cast<const i32*>(&message));
    release_actor_attachment_slot(&record);
    if (record.owner_link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record.owner_link.prev)->next = record.owner_link.next;
    }
    if (record.owner_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record.owner_link.next)->prev = record.owner_link.prev;
    }
    record.owner_link.prev = nullptr;
    record.owner_link.next = nullptr;
    if (record.global_link.prev != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record.global_link.prev)->next = record.global_link.next;
    }
    if (record.global_link.next != nullptr)
    {
        static_cast<entity_detail::ListHead*>(record.global_link.next)->prev = record.global_link.prev;
    }
    record.global_link.prev = nullptr;
    record.global_link.next = nullptr;
    engine_heap_free(&record);
}

// Xbox 0x00042040
void clear_actor_live_attachments(entity_detail::ListHead& owner_head)
{
    auto* record = static_cast<ActorLiveAttachment*>(owner_head.next);
    while (record != nullptr)
    {
        auto* next = static_cast<ActorLiveAttachment*>(record->owner_link.next);
        release_actor_live_attachment(*record);
        record = next;
    }
}
}
