namespace
{

struct AnimationEventType
{
    u32 size;
    u32 flags;
};

// Xbox 0x00339B50, record stride 12; name pointers are not used by dispatch.
constexpr std::array<AnimationEventType, 60> animation_event_types
{
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{16, 0x00000001u},
    AnimationEventType{24, 0x00000001u},
    AnimationEventType{80, 0x00000000u},
    AnimationEventType{24, 0x00000001u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{20, 0x00000001u},
    AnimationEventType{20, 0x00000001u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{24, 0x00000001u},
    AnimationEventType{296, 0x00000000u},
    AnimationEventType{64, 0x00000001u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{144, 0x00000000u},
    AnimationEventType{296, 0x00000000u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{16, 0x00000001u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{144, 0x00000000u},
    AnimationEventType{44, 0x00000000u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{20, 0x00000001u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{24, 0x00000001u},
    AnimationEventType{28, 0x00000001u},
    AnimationEventType{144, 0x00000000u},
    AnimationEventType{28, 0x00000004u},
    AnimationEventType{296, 0x00000000u},
    AnimationEventType{296, 0x00000000u},
    AnimationEventType{36, 0x00000000u},
    AnimationEventType{148, 0x00000000u},
    AnimationEventType{144, 0x00000000u},
    AnimationEventType{80, 0x00000000u},
    AnimationEventType{80, 0x00000000u},
    AnimationEventType{148, 0x00000000u},
    AnimationEventType{24, 0x00000001u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{152, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{28, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{16, 0x00000000u},
    AnimationEventType{20, 0x00000000u},
    AnimationEventType{24, 0x00000000u},
    AnimationEventType{64, 0x00000000u},
    AnimationEventType{16, 0x00000001u},
};

void dispatch_list_event(std::span<const u8> event, AnimationEventClock& clock,
                         const AnimationEventCallback& callback)
{
    const auto& type = animation_event_types.at(read_u16(event, 12));
    dispatch_animation_event(event, type.size, type.flags, clock,
        [&](int phase)
        {
            callback(event, phase);
        });
}

} // namespace

// Xbox 0x0002DE10.
void dispatch_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                               const AnimationEventCallback& callback)
{
    if (callback)
    {
        while (read_u16(events, 12) != 0)
        {
            const u16 type = read_u16(events, 12);
            const u16 size = read_u16(events, 0);
            if (type >= 60 || animation_event_types[type].size == size)
            {
                dispatch_list_event(events.first(size), clock, callback);
            }
            events = events.subspan(size);
        }
    }
}

// Xbox 0x0002DF30.
void begin_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                            const AnimationEventCallback& callback)
{
    if (callback)
    {
        const float previous = clock.previous;
        float initial = 0.f;
        if (clock.speed <= 0.f && clock.speed != 0.f)
        {
            initial = 1.f;
        }
        clock.previous = initial;
        while (read_u16(events, 12) != 0)
        {
            const u16 type = read_u16(events, 12);
            if (((events[14] | animation_event_types.at(type).flags) & 1) != 0)
            {
                dispatch_list_event(events.first(read_u16(events, 0)), clock, callback);
            }
            events = events.subspan(read_u16(events, 0));
        }
        clock.previous = previous;
    }
}

// Xbox 0x0002DE70.
void end_animation_events(std::span<const u8> events, AnimationEventClock& clock,
                          const AnimationEventCallback& callback)
{
    if (callback)
    {
        const float previous = clock.previous;
        const float position = clock.position;
        float final = 0.f;
        if (0.f < clock.speed || clock.speed == 0.f)
        {
            final = 1.f;
        }
        clock.position = final;
        clock.previous = position;
        while (read_u16(events, 12) != 0)
        {
            const u16 type = read_u16(events, 12);
            if (((events[14] | animation_event_types.at(type).flags) & 1) != 0)
            {
                dispatch_list_event(events.first(read_u16(events, 0)), clock, callback);
            }
            events = events.subspan(read_u16(events, 0));
        }
        clock.position = position;
        clock.previous = previous;
    }
}

