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

// Xbox 0x0002CC30 over intrusive list heads (prev +0, next +4), such as the actor view bins.
template<class Head>
void exchange_runtime_list_heads(Head& first, Head& second)
{
    const auto unlink = [](Head& link)
    {
        if (link.prev != nullptr)
        {
            static_cast<Head*>(link.prev)->next = link.next;
        }
        if (link.next != nullptr)
        {
            static_cast<Head*>(link.next)->prev = link.prev;
        }
        link.prev = nullptr;
        link.next = nullptr;
    };
    const auto insert_after = [](Head& link, Head& position)
    {
        auto* following = static_cast<Head*>(position.next);
        position.next = &link;
        link.prev = &position;
        link.next = following;
        if (following != nullptr)
        {
            following->prev = &link;
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
