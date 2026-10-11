#pragma once
#include "body_action_ids.h"

namespace gbtg
{
struct RuntimeAttachmentRequestRows
{
    const u8* rows;
    u16 count;
};
struct RuntimeAttachmentRequestTables
{
    RuntimeAttachmentRequestRows first; // +0/+4, stride 0x24.
    RuntimeAttachmentRequestRows second; // +8/+6, stride 0x24.
    RuntimeAttachmentRequestRows pair; // +0x18/+0x14, stride 0x28.
    RuntimeAttachmentRequestRows category; // +0x20/+0x1C, stride 0x2C.
    RuntimeAttachmentRequestRows special; // +0x28/+0x24, stride 0x24.
};
const u8* find_runtime_attachment_first_request(const RuntimeAttachmentRequestTables& tables,
    u16 key); // Xbox 0x0001C560
const u8* find_runtime_attachment_second_request(const RuntimeAttachmentRequestTables& tables,
    u16 key); // Xbox 0x0001C5A0
const u8* find_runtime_attachment_pair_request(const RuntimeAttachmentRequestTables& tables,
    u16 key); // Xbox 0x0001CB20
const u8* find_runtime_attachment_category_request(const RuntimeAttachmentRequestTables& tables,
    u16 key); // Xbox 0x0001C7C0
u16 runtime_body_attached_class(const RuntimeBodyAttachedActor* attached,
    u16 owner_class); // Xbox 0x0001B170
u8 runtime_body_attached_category(const RuntimeBodyAttachedActor* attached); // Xbox 0x0001B150
bool runtime_attachment_request_matches(std::span<const u8> row,
    const RuntimeBodyAttachedActor* primary, const RuntimeBodyAttachedActor* secondary,
    u16 owner_class); // Xbox 0x0001C800
const u8* find_runtime_attachment_special_request(const RuntimeAttachmentRequestTables* tables,
    const RuntimeBodyAttachedActor* primary, u16 key); // Xbox 0x0001C930
struct RuntimeBodyAttachmentRequests
{
    std::span<u8> body;
    const RuntimeAttachmentRequestTables*& tables; // Body +0x5C.
    RuntimeBodyActionSelection& actions;
    RuntimeBodyAttachedActor*& secondary; // Body +0x4C.
    u16 owner_class;
    const u8*& state; // Body +0x84, native record view.
    std::function<u32(const u8*)> address_of; // Retail record identity.
};
bool accept_runtime_attachment_request(RuntimeBodyAttachmentRequests& context,
    const u8* row, u32 timer); // Xbox 0x0001D2E0
bool select_runtime_attachment_request(RuntimeBodyAttachmentRequests& context,
    u16 key); // Xbox 0x0001D210
bool runtime_body_accepts_target_request(std::span<const u8> body,
    const EntityBitBlock& flags, u16 owner_class); // Xbox 0x0001D180
u32 runtime_body_target_height(std::span<const u8> body, const Actor& actor,
    std::span<const u8> target, u8* shape); // Xbox 0x0001CD00
}
