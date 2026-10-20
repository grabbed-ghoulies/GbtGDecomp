#pragma once
#include "gbtg/retail_field.h"

namespace gbtg
{
// The base strategy's (factory type 8) fields its derived strategies read, at their retail offsets (see
// docs/RETAIL_LAYOUTS.md).
namespace strategy_field
{
// The strategy's state: the skeleton's 0x000BFAC0 switches on it, and 0x405 is its weapon fetch.
constexpr RetailField<u16, 0x0E> state{};
} // namespace strategy_field
} // namespace gbtg
