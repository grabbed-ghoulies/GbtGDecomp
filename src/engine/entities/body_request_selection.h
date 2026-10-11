#pragma once
#include "body_attachment_requests.h"
#include "player_mind_targets.h"
#include "player_mind_events.h"

namespace gbtg
{
struct RuntimeRequestBits
{
    u32 count; // +0.
    std::span<u8> bytes; // +4 byte count, +8 storage.
};
i32 count_runtime_request_bits(const RuntimeRequestBits& bits); // Xbox 0x00066180
bool select_random_runtime_request_bit(const RuntimeRequestBits& bits,
    u32& index, u32& random); // Xbox 0x000661C0
struct RuntimeAttachmentTargetSelection
{
    RuntimeBodyAttachmentRequests& requests;
    const RuntimePlayerAttachmentEligibility& body;
    Actor& actor;
    const RuntimeMindTarget& target;
    const u8* model_state;
    RuntimeRequestBits& bits;
    u32& random;
    u32 tables_address;
};
const u8* find_runtime_attachment_target_request(RuntimeAttachmentTargetSelection& context,
    const u8* key, const u8* shape, u32 direction, u8 height_mask,
    u32& optional); // Xbox 0x0001C140
struct RuntimeMindTargetRequestSelection
{
    RuntimeAttachmentTargetSelection& selection;
    RuntimeMindEvents& events;
    const EntityBitBlock& flags;
    std::span<const u8> owner; // Actual actor bytes, including +0xCDE.
};
u8 request_runtime_mind_second_target(RuntimeMindTargetRequestSelection& context,
    u32 direction); // Xbox 0x001368B0
u8 request_runtime_mind_pair_target(RuntimeMindTargetRequestSelection& context,
    u32 direction); // Xbox 0x00136920
u8 request_runtime_mind_category_target(RuntimeMindTargetRequestSelection& context,
    u32 direction); // Xbox 0x00136990
u8 request_runtime_mind_special_target(RuntimeMindTargetRequestSelection& context,
    u32 direction); // Xbox 0x00136A00
const u8* select_runtime_attachment_second_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask); // Xbox 0x0001BBB0
const u8* select_runtime_attachment_pair_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask); // Xbox 0x0001C9A0
const u8* select_runtime_attachment_category_target_row(RuntimeAttachmentTargetSelection& context,
    const u8* shape, u32 direction, u8 height_mask); // Xbox 0x0001C5E0
const u8* select_runtime_attachment_special_target_row(RuntimeAttachmentTargetSelection& context,
    u32 direction, u8 height_mask); // Xbox 0x0001C860
const u8* select_runtime_body_second_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction); // Xbox 0x0001CF70
const u8* select_runtime_body_pair_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction); // Xbox 0x0001CFE0
const u8* select_runtime_body_category_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction); // Xbox 0x0001D060
const u8* select_runtime_body_special_target_request(RuntimeAttachmentTargetSelection& context,
    u32 direction); // Xbox 0x0001D0E0
bool runtime_attachment_target_row_matches(std::span<const u8> body, const Actor& actor,
    std::span<const u8> row, std::span<const u8> target,
    const u8* shape, u32 direction, u8 height_mask); // Xbox 0x0001BA20
struct RuntimeAttachmentChainRows
{
    const u8* rows; // Body +0x64, stride 8.
    u8 count; // Body +0x68.
};
const u8* find_runtime_attachment_chain(const RuntimeAttachmentChainRows& chains,
    u32& length); // Xbox 0x0001BE20
const u8* find_runtime_attachment_chain_for_address(const RuntimeAttachmentRequestTables* tables,
    const RuntimeAttachmentChainRows& chains, u32 address); // Xbox 0x0001C510
struct RuntimeBodyChainLength
{
    std::span<const u8> body;
    std::span<const u8> owner;
    const u8* model_state;
    const EntityBitBlock& flags;
    u8 minimum;
    u8 maximum;
    float lower;
    float upper;
    std::span<const u8> class_table; // DAT_00545AC4.
};
i32 runtime_body_target_chain_length(const RuntimeBodyChainLength& context); // Xbox 0x000243C0
struct RuntimeAttachmentChainSelection
{
    RuntimeAttachmentTargetSelection& target;
    const RuntimeAttachmentChainRows& chains;
    std::function<const u8*(u32)> resolve; // Native view of a retail data address.
};
u32 resume_runtime_attachment_chain(RuntimeAttachmentChainSelection& context,
    u32 address, const u8* shape, u8 height_mask, const u8*& selected); // Xbox 0x0001C0E0
u32 select_runtime_attachment_chain_row(RuntimeAttachmentChainSelection& context,
    const u8* chain, const u8* shape, u32 direction, u8 height_mask,
    const u8*& selected); // Xbox 0x0001BE60
struct RuntimeMindChainRequestSelection
{
    RuntimeMindTargetRequestSelection& mind;
    RuntimeAttachmentChainSelection& chains;
    const RuntimeBodyChainLength& target_length;
    const u32& first_mode;
    const u32& second_mode;
};
const u8* select_runtime_body_chain_target_request(RuntimeMindChainRequestSelection& context,
    u32 direction, u32& sequence, bool allow_fallback); // Xbox 0x0001CDA0
u8 request_runtime_mind_chain_target(RuntimeMindChainRequestSelection& context,
    u32 direction, bool allow_fallback); // Xbox 0x00136830
u8 dispatch_runtime_mind_target_request(RuntimeMindChainRequestSelection& context,
    u32 direction, u32 flags); // Xbox 0x00136770
void update_runtime_player_target_action(RuntimeMindChainRequestSelection& context,
    i32 mode, const Actor* focus, std::span<const u8> target_body,
    u32& attachment_notice); // Xbox 0x0007DF50, DAT_00433BD0.
struct RuntimePlayerMindTargetControl;
struct RuntimePlayerTargetActionBinding
{
    RuntimeMindChainRequestSelection& requests;
    std::span<const u8> target_body;
};
struct RuntimePlayerMindActionControl
{
    RuntimePlayerMindTargetControl& targets;
    RuntimeBodyAttachmentRequests& body;
    const EntityBitBlock& flags;
    const RuntimeDialogueInputDevice& input;
    const RuntimeDialogueInputState& input_state;
    const std::unordered_map<u32, RuntimePlayerTargetActionBinding>& bindings;
    u32& attachment_notice;
};
void update_runtime_player_mind_action_control(RuntimePlayerMindActionControl& context); // Xbox 0x0007D890
u8 runtime_body_model_target_flag(const u8* state); // Xbox 0x00024600
bool runtime_mind_target_uses_point_shape(const RuntimeMindTarget& target,
    const u8* model_state); // Xbox 0x001415D0
float runtime_body_target_surface_distance(const Actor& actor,
    const RuntimePlayerAttachmentEligibility& body,
    std::span<const u8> target); // Xbox 0x0013A5B0
}
