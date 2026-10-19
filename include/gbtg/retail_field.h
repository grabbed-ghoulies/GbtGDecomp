#pragma once

#include "gbtg/types.h"

#include <type_traits>

namespace gbtg
{

// A field of a retail object whose bytes are kept at their retail offsets: the field's type, offset and name are
// declared once beside the object, and reads and writes go through get / set instead of literal offsets.
template <typename T, u32 Offset>
struct RetailField
{
    static_assert(std::is_trivially_copyable_v<T>);
    using type = T;
    static constexpr u32 offset = Offset;
    static constexpr u32 size = sizeof(T);
};

// Count records of Stride bytes from Offset inside a retail object, for example an array of sub-objects. Fields of
// one record are RetailFields relative to the record's start.
template <u32 Offset, u32 Stride, u32 Count = 1>
struct RetailRecord
{
    static constexpr u32 offset = Offset;
    static constexpr u32 stride = Stride;
    static constexpr u32 count = Count;
    static constexpr u32 size = Stride * Count;
};

// For static_assert: the field or record lies inside an object of `object_size` bytes.
template <typename F>
constexpr bool retail_fits(u32 object_size, F)
{
    return F::offset + F::size <= object_size;
}

namespace detail
{
inline void check_retail_range(size_t available, size_t offset, size_t size)
{
    if (offset + size > available)
    {
        throw Error("retail field out of range");
    }
}

template <typename T>
struct RetailElement
{
    using type = T;
};

template <typename T, size_t N>
struct RetailElement<std::array<T, N>>
{
    using type = T;
};

template <typename T, size_t N>
struct RetailElement<T[N]>
{
    using type = T;
};
} // namespace detail

template <typename T, u32 Offset>
T get(std::span<const u8> bytes, RetailField<T, Offset>)
{
    detail::check_retail_range(bytes.size(), Offset, sizeof(T));
    T value;
    std::memcpy(&value, bytes.data() + Offset, sizeof(T));
    return value;
}

template <typename T, u32 Offset>
void set(std::span<u8> bytes, RetailField<T, Offset>, const std::type_identity_t<T>& value)
{
    detail::check_retail_range(bytes.size(), Offset, sizeof(T));
    std::memcpy(bytes.data() + Offset, &value, sizeof(T));
}

// The field's storage, as its element type for array fields: the float pairs and wide strings the retail code passes
// by address. Const bytes give a const pointer.
template <typename Bytes, typename T, u32 Offset>
auto* field_data(Bytes&& bytes, RetailField<T, Offset>)
{
    const auto view = std::span(bytes);
    using Byte = typename decltype(view)::element_type;
    using Element = typename detail::RetailElement<T>::type;
    using Pointer = std::conditional_t<std::is_const_v<Byte>, const Element*, Element*>;
    detail::check_retail_range(view.size(), Offset, sizeof(T));
    return reinterpret_cast<Pointer>(view.data() + Offset);
}

// One byte of a field, for the retail code's single-byte writes such as a color word's alpha (+3).
template <typename Bytes, typename T, u32 Offset>
auto& field_byte(Bytes&& bytes, RetailField<T, Offset>, u32 index)
{
    const auto view = std::span(bytes);
    detail::check_retail_range(sizeof(T), index, 1);
    detail::check_retail_range(view.size(), Offset + index, 1);
    return view[Offset + index];
}

// Record `index` of a RetailRecord as its own byte span.
template <typename Bytes, u32 Offset, u32 Stride, u32 Count>
auto record(Bytes&& bytes, RetailRecord<Offset, Stride, Count>, size_t index = 0)
{
    const auto view = std::span(bytes);
    detail::check_retail_range(Count, index, 1);
    detail::check_retail_range(view.size(), Offset + index * Stride, Stride);
    using Byte = typename decltype(view)::element_type;
    return std::span<Byte>(view.data() + Offset + index * Stride, Stride);
}

} // namespace gbtg
