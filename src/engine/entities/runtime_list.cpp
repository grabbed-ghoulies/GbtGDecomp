#include "runtime_list.h"

namespace gbtg
{
// Xbox 0x0002CBF0
void append_runtime_list_node(entity_detail::ListHead& head, entity_detail::ListHead& node)
{
    auto* tail = &head;
    while (tail->next != nullptr)
    {
        tail = static_cast<entity_detail::ListHead*>(tail->next);
    }
    tail->next = &node;
    node.prev = tail;
    node.next = nullptr;
}
}
