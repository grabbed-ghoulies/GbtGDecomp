#pragma once

namespace gbtg
{
// Xbox 0x0002CC30; shared by strategy targets and pending actor sounds.
template<class Link>
void exchange_runtime_links(Link& first, Link& second)
{
    const auto unlink = [](Link& link)
    {
        if (link.previous != nullptr)
        {
            link.previous->next = link.next;
        }
        if (link.next != nullptr)
        {
            link.next->previous = link.previous;
        }
        link.previous = nullptr;
        link.next = nullptr;
    };
    const auto insert_after = [](Link& link, Link& position)
    {
        auto* following = position.next;
        position.next = &link;
        link.previous = &position;
        link.next = following;
        if (following != nullptr)
        {
            following->previous = &link;
        }
    };
    if (first.next == &second)
    {
        unlink(first);
        insert_after(first, second);
        return;
    }
    unlink(second);
    insert_after(second, first);
}
}
