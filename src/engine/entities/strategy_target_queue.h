#pragma once
#include "player_mind_targets.h"
#include "strategy_target_selection.h"

namespace gbtg
{
// Strategy links (strategy +0x20) queue behind a target record in one of the 20 DAT_0050E284
// slots. A slot is the queue anchor: its records head is the anchor's previous/next pair and its
// owner is the anchor's +8 word. Member links keep their actor at +8.
struct RuntimeStrategyTargetQueue
{
    std::span<ActorAttachmentSlot, 20> slots; // DAT_0050E284.
    std::span<const u32> limits; // DAT_0050E570; word 0 is the queue depth.
    std::span<const u32> class_limits; // DAT_0050E660, by actor class key.
    std::function<const RuntimeMindTarget*(const ActorLiveAttachment*)> target_of; // Slot owner record view.
    std::function<std::span<const u8>(const void*)> actor_bytes; // Member link +8 actor bytes.
};
void exchange_runtime_strategy_links(RuntimeStrategyLink& first,
    RuntimeStrategyLink& second); // Xbox 0x0002CC30, ECX first, EAX second
void sort_runtime_strategy_target_queue(RuntimeStrategyLink& anchor); // Xbox 0x00112B40, EAX anchor
void queue_runtime_strategy_link(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context); // Xbox 0x00112970, ESI link, EAX record
ActorLiveAttachment* runtime_strategy_link_target(RuntimeStrategyLink& link,
    const RuntimeStrategyTargetQueue& context); // Xbox 0x00112A70
bool join_runtime_strategy_target_queue(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context); // Xbox 0x0002B910, EBX strategy, EDI record
bool hold_runtime_strategy_target_queue(RuntimeStrategyLink& link, ActorLiveAttachment* record, Actor* actor,
    RuntimeStrategyTargetQueue& context); // Xbox 0x0002B9D0, EAX strategy, EDI record
extern float g_50e280; // DAT_0050E280, the queues' resort timer.
// Each second (DAT_0050E280 counts the step down), every DAT_0050E284 queue with an owner is sorted again.
void resort_runtime_strategy_target_queues(float step,
    std::span<ActorAttachmentSlot, 20> slots); // Xbox 0x001128A0
bool runtime_strategy_target_attackable(float range, RuntimeStrategyLink& link, RuntimeStrategyTargetChecks& checks,
    RuntimeStrategyTargetQueue& context); // Xbox 0x0013A330
}
