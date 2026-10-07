// Typed entry to the already translated Xbox 0x00153320.
long double retail_sin(float value)
{
    return animation_sin(value);
}

// Xbox 0x00153230
long double retail_cos(float value)
{
    const float angle = float(animation_wrap_angle(value));
    float absolute = angle;
    if (angle < 0.f)
    {
        absolute = 0.f - angle;
    }
    constexpr float a = animation_float(0x3D2A9E75);
    constexpr float b = animation_float(0x3AB21287);
    constexpr float c = animation_float(0x3EFFFFD0);
    if (absolute <= animation_float(0x3F490FDB))
    {
        const long double square = static_cast<long double>(angle) * angle;
        return ((static_cast<long double>(a) - static_cast<long double>(b) * square) * square - c) * square + 1.L;
    }
    if (absolute <= animation_float(0x4016CBE4))
    {
        const float offset = animation_float(0x3FC90FDB) - absolute;
        const float square = offset * offset;
        return ((static_cast<long double>(square) * animation_float(0x3C051003) - animation_float(0x3E2A999A)) *
            square + animation_float(0x3F7FFFAC)) * offset;
    }
    const float square = (absolute - animation_float(0x40490FDB)) * (absolute - animation_float(0x40490FDB));
    return -(((static_cast<long double>(a) - static_cast<long double>(square) * b) * square - c) * square + 1.L);
}

// Xbox 0x00152E90
long double retail_asin(float value)
{
    constexpr float a = animation_float(0x3D59C381);
    constexpr float b = animation_float(0x3D1C91EF);
    constexpr float c = animation_float(0x3D9B205A);
    constexpr float d = animation_float(0x3E2AA19F);
    constexpr float e = animation_float(0x3F800004);
    const float square = value * value;
    if (square <= 0.25f)
    {
        return ((((square * a + b) * square + c) * square + d) * square + e) * value;
    }
    if (value <= 0.5f)
    {
        const float argument = (value + 1.f) * 0.5f;
        float estimate = float(animation_rsqrt_estimate(argument));
        estimate = (1.5f - argument * 0.5f * estimate * estimate) * estimate;
        float root = argument;
        if (0.f - argument < 0.f)
        {
            root = (1.5f - argument * 0.5f * estimate * estimate) * estimate * argument;
        }
        return ((((argument * a + b) * argument + c) * argument + d) * argument + e) * root * 2.f -
            animation_float(0x3FC90FDB);
    }
    const float argument = 0.5f - value * 0.5f;
    float estimate = float(animation_rsqrt_estimate(argument));
    estimate = (1.5f - argument * 0.5f * estimate * estimate) * estimate;
    float root = argument;
    if (0.f - argument < 0.f)
    {
        root = (1.5f - argument * 0.5f * estimate * estimate) * estimate * argument;
    }
    return animation_float(0x3FC90FDB) -
        ((((argument * a + b) * argument + c) * argument + d) * argument + e) * root * 2.f;
}

// Xbox 0x001530E0
long double retail_atan2(float y, float x)
{
    const float original_y = y;
    if (y < 0.f)
    {
        y = 0.f - y;
    }
    float absolute_x = x;
    if (x < 0.f)
    {
        absolute_x = 0.f - x;
    }
    float angle;
    if (absolute_x <= y)
    {
        absolute_x /= y;
        const float square = absolute_x * absolute_x;
        angle = animation_float(0x3FC90FDB) -
            ((((((square * 0.0066582896f - 0.03310525f) * square + 0.07899983f) * square - 0.13195902f) *
            square + 0.19796892f) * square - 0.33316f) * square + 0.99999565f) * absolute_x;
    }
    else
    {
        y /= absolute_x;
        const float square = y * y;
        angle = ((((((square * 0.0066582896f - 0.03310525f) * square + 0.07899983f) * square - 0.13195902f) *
            square + 0.19796892f) * square - 0.33316f) * square + 0.99999565f) * y;
    }
    if (0.f <= x)
    {
        if (0.f <= original_y)
        {
            return angle;
        }
        return -static_cast<long double>(angle);
    }
    if (0.f <= original_y)
    {
        return static_cast<long double>(animation_float(0x40490FDB)) - angle;
    }
    return static_cast<long double>(angle) - animation_float(0x40490FDB);
}

// Xbox 0x00103AF0
void normalize_model_matrix(float matrix[12], float target)
{
    float factors[3];
    int index = 0;
    do
    {
        const float square = matrix[index + 8] * matrix[index + 8] +
            matrix[index + 4] * matrix[index + 4] + matrix[index] * matrix[index];
        float factor;
        if (square == 0.f)
        {
            factor = 1.f;
        }
        else
        {
            factor = float(animation_rsqrt(square));
            if (factor != 0.f)
            {
                factor *= target;
            }
        }
        factors[index] = factor;
        ++index;
    }
    while (index < 3);
    index = 0;
    do
    {
        matrix[index] *= factors[index];
        matrix[index + 4] *= factors[index];
        matrix[index + 8] *= factors[index];
        ++index;
    }
    while (index < 3);
}
// Xbox 0x00104400
bool invert_model_matrix(float output[12], const float input[12])
{
    float temporary[12];
    float* destination = output;
    if (input == output)
    {
        destination = temporary;
    }
    const float determinant =
        (((input[5] * input[0] * input[10] + input[1] * input[6] * input[8] +
            input[4] * input[9] * input[2]) - input[5] * input[8] * input[2]) -
            input[1] * input[4] * input[10]) - input[9] * input[6] * input[0];
    if (determinant == 0.f)
    {
        return false;
    }
    const float reciprocal = 1.f / determinant;
    destination[0] = (input[5] * input[10] - input[9] * input[6]) * reciprocal;
    destination[1] = 0.f - (input[1] * input[10] - input[9] * input[2]) * reciprocal;
    destination[2] = (input[1] * input[6] - input[5] * input[2]) * reciprocal;
    destination[4] = 0.f - (input[4] * input[10] - input[6] * input[8]) * reciprocal;
    destination[5] = (input[0] * input[10] - input[8] * input[2]) * reciprocal;
    destination[6] = 0.f - (input[6] * input[0] - input[4] * input[2]) * reciprocal;
    destination[8] = (input[4] * input[9] - input[5] * input[8]) * reciprocal;
    destination[9] = 0.f - (input[9] * input[0] - input[1] * input[8]) * reciprocal;
    destination[10] = (input[5] * input[0] - input[1] * input[4]) * reciprocal;
    destination[3] = ((0.f - destination[0] * input[3]) - input[7] * destination[1]) - destination[2] * input[11];
    destination[7] = ((0.f - destination[4] * input[3]) - input[7] * destination[5]) - input[11] * destination[6];
    destination[11] = ((0.f - destination[8] * input[3]) - destination[9] * input[7]) - destination[10] * input[11];
    if (destination == temporary)
    {
        std::memcpy(output, temporary, sizeof(temporary));
    }
    return true;
}
