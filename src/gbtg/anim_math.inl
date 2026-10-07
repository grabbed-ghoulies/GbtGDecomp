// Constants and scalar math used by Xbox 0x00154030.
constexpr float animation_float(u32 bits)
{
    return std::bit_cast<float>(bits);
}

// Xbox 0x00153450.
long double animation_rsqrt_estimate(float value)
{
    const float half = 0.5f * value;
    const u32 bits = std::bit_cast<u32>(value);
    float estimate = animation_float(((0x7f000000u - bits) >> 1) + 0x1fc00000u);
    if ((bits & 0x08000000u) == 0)
    {
        estimate *= animation_float(0x3f3504f3u);
    }
    const long double refined = (1.5L - static_cast<long double>(estimate) * estimate * half) * estimate;
    return (1.5L - refined * refined * half) * refined;
}

// Xbox 0x001534C0.
long double animation_rsqrt(float value)
{
    float estimate = float(animation_rsqrt_estimate(value));
    estimate = (1.5f - 0.5f * value * estimate * estimate) * estimate;
    return (1.5L - static_cast<long double>(0.5f * value) * estimate * estimate) * estimate;
}

// Xbox 0x00152B30.
long double animation_sqrt(float value)
{
    float estimate = float(animation_rsqrt_estimate(value));
    estimate = (1.5f - 0.5f * value * estimate * estimate) * estimate;
    if (0.f - value < 0.f)
    {
        value = (1.5f - 0.5f * value * estimate * estimate) * estimate * value;
    }
    return value;
}

// Xbox 0x00152BB0.
float animation_absolute(float value)
{
    if (0.f <= value)
    {
        return value;
    }
    return 0.f - value;
}

// Xbox 0x00153530.
long double animation_wrap_angle(float angle)
{
    constexpr float pi = animation_float(0x40490fdbu);
    constexpr float turn = animation_float(0x40c90fdbu);
    if (-pi <= angle)
    {
        while (pi < angle)
        {
            angle -= turn;
        }
    }
    else
    {
        angle = turn + angle;
        if (angle < -pi)
        {
            do
            {
                angle = turn + angle;
            } while (angle < -pi);
            return angle;
        }
    }
    return angle;
}

// Xbox 0x00153320.
long double animation_sin(float angle)
{
    const float value = float(animation_wrap_angle(angle));
    float absolute = value;
    if (value < 0.f)
    {
        absolute = -value;
    }
    constexpr float a = animation_float(0x3c051003u);
    constexpr float b = animation_float(0x3e2a999au);
    constexpr float c = animation_float(0x3f7fffacu);
    if (absolute <= animation_float(0x3f490fdbu))
    {
        return ((static_cast<long double>(a) * value * value - b) * value * value + c) * value;
    }
    if (absolute <= animation_float(0x4016cbe4u))
    {
        absolute = (absolute - animation_float(0x3fc90fdbu)) * (absolute - animation_float(0x3fc90fdbu));
        absolute = ((animation_float(0x3d2a9e75u) - absolute * animation_float(0x3ab21287u)) * absolute -
                    animation_float(0x3effffd0u)) * absolute + 1.f;
        if (value < 0.f)
        {
            return -absolute;
        }
        return absolute;
    }
    const float offset = absolute - animation_float(0x40490fdbu);
    const float polynomial = (offset * offset * a - b) * offset * offset + c;
    if (value < 0.f)
    {
        return polynomial * offset;
    }
    return (animation_float(0x40490fdbu) - absolute) * polynomial;
}

// Xbox 0x00152C40.
long double animation_acos(float value)
{
    constexpr float a = animation_float(0x3d59c381u);
    constexpr float b = animation_float(0x3d1c91efu);
    constexpr float c = animation_float(0x3d9b205au);
    constexpr float d = animation_float(0x3e2aa19fu);
    constexpr float e = animation_float(0x3f800004u);
    const float squared = value * value;
    if (squared <= 0.25f)
    {
        return animation_float(0x3fc90fdbu) - ((((squared * a + b) * squared + c) * squared + d) * squared + e) * value;
    }
    if (value <= 0.5f)
    {
        const float argument = (value + 1.f) * 0.5f;
        float estimate = float(animation_rsqrt_estimate(argument));
        estimate = (1.5f - 0.5f * argument * estimate * estimate) * estimate;
        float root = argument;
        if (-argument < 0.f)
        {
            root = (1.5f - 0.5f * argument * estimate * estimate) * estimate * argument;
        }
        return animation_float(0x40490fdbu) -
               ((((argument * a + b) * argument + c) * argument + d) * argument + e) * root * 2.f;
    }
    const float argument = 0.5f - 0.5f * value;
    float estimate = float(animation_rsqrt_estimate(argument));
    estimate = (1.5f - 0.5f * argument * estimate * estimate) * estimate;
    float root = argument;
    if (-argument < 0.f)
    {
        root = (1.5f - 0.5f * argument * estimate * estimate) * estimate * argument;
    }
    return ((((argument * a + b) * argument + c) * argument + d) * argument + e) * root * 2.f;
}
