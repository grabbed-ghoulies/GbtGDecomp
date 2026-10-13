#pragma once

#include <cmath>
#include <limits>

namespace gbtg
{
// Native arithmetic view of an x87 value with a 64-bit significand. Windows
// long double has only 53 bits; a compensated pair preserves the discarded bits.
class ExtendedFloat
{
public:
    ExtendedFloat(double value = 0.) : value_{value, 0.}
    {
    }

    explicit operator float() const
    {
        float result = float(value_.high);
        if (value_.low == 0. || !std::isfinite(value_.high))
        {
            return result;
        }
        if (!std::isfinite(result))
        {
            const double maximum = double(std::numeric_limits<float>::max());
            const double limit = maximum + std::ldexp(1., 103);
            if (value_.high == limit && value_.low < 0.)
            {
                return std::numeric_limits<float>::max();
            }
            if (value_.high == -limit && value_.low > 0.)
            {
                return -std::numeric_limits<float>::max();
            }
            return result;
        }
        const double candidate = result;
        if (value_.high > candidate)
        {
            const float next = std::nextafter(result, std::numeric_limits<float>::infinity());
            const double midpoint = candidate + (double(next) - candidate) * 0.5;
            if (value_.high == midpoint && value_.low > 0.)
            {
                result = next;
            }
        }
        else if (value_.high < candidate)
        {
            const float next = std::nextafter(result, -std::numeric_limits<float>::infinity());
            const double midpoint = candidate + (double(next) - candidate) * 0.5;
            if (value_.high == midpoint && value_.low < 0.)
            {
                result = next;
            }
        }
        return result;
    }

    friend ExtendedFloat operator+(ExtendedFloat first, ExtendedFloat second)
    {
        return rounded(add(first.value_, second.value_));
    }

    friend ExtendedFloat operator-(ExtendedFloat first, ExtendedFloat second)
    {
        return rounded(add(first.value_, {-second.value_.high, -second.value_.low}));
    }

    friend ExtendedFloat operator*(ExtendedFloat first, ExtendedFloat second)
    {
        return rounded(multiply(first.value_, second.value_));
    }

    friend ExtendedFloat operator/(ExtendedFloat first, ExtendedFloat second)
    {
        const double quotient = first.value_.high / second.value_.high;
        if (!std::isfinite(quotient) || !std::isfinite(first.value_.high)
            || !std::isfinite(second.value_.high))
        {
            return ExtendedFloat{quotient};
        }
        if (first.value_.high == 0. && first.value_.low == 0.)
        {
            return ExtendedFloat{quotient};
        }
        Pair product = multiply(second.value_, {quotient, 0.});
        Pair remainder = add(first.value_, {-product.high, -product.low});
        Pair result = add({quotient, 0.},
            {remainder.high / second.value_.high, remainder.low / second.value_.high});
        return rounded(result);
    }

    friend ExtendedFloat extended_sqrt(ExtendedFloat value)
    {
        const double root = std::sqrt(value.value_.high);
        if (root == 0. || !std::isfinite(root))
        {
            return ExtendedFloat{root};
        }
        const Pair square = product(root, root);
        const Pair remainder = add(value.value_, {-square.high, -square.low});
        return rounded(add({root, 0.},
            {remainder.high / (root * 2.), remainder.low / (root * 2.)}));
    }

private:
    struct Pair
    {
        double high;
        double low;
    };

    static Pair sum(double first, double second)
    {
        const double high = first + second;
        const double recovered = high - first;
        return {high, (first - (high - recovered)) + (second - recovered)};
    }

    static Pair add(Pair first, Pair second)
    {
        if (!std::isfinite(first.high) || !std::isfinite(second.high))
        {
            return {first.high + second.high, 0.};
        }
        if (first.low == 0. && second.low == 0.)
        {
            return sum(first.high, second.high);
        }
        const Pair high = sum(first.high, second.high);
        const Pair low = sum(first.low, second.low);
        const Pair middle = sum(high.low, low.high);
        const Pair combined = sum(high.high, middle.high);
        return sum(combined.high, combined.low + (middle.low + low.low));
    }

    static Pair product(double first, double second)
    {
        const double high = first * second;
        if (!std::isfinite(high))
        {
            return {high, 0.};
        }
        return {high, std::fma(first, second, -high)};
    }

    static Pair multiply(Pair first, Pair second)
    {
        Pair result = product(first.high, second.high);
        if ((first.low == 0. && second.low == 0.) ||
            (first.high == 0. && first.low == 0.) || (second.high == 0. && second.low == 0.))
        {
            return result;
        }
        result = add(result, product(first.high, second.low));
        result = add(result, product(first.low, second.high));
        return add(result, product(first.low, second.low));
    }

    static ExtendedFloat rounded(Pair value)
    {
        if (!std::isfinite(value.high) || value.low == 0.)
        {
            return ExtendedFloat{value.high};
        }
        value = sum(value.high, value.low);
        if (value.high == 0.)
        {
            return ExtendedFloat{value.high};
        }
        const double unit = std::ldexp(1., std::ilogb(value.high) - 63);
        const double correction = std::nearbyint(value.low / unit) * unit;
        ExtendedFloat result;
        result.value_ = sum(value.high, correction);
        return result;
    }

    Pair value_;
};
}
