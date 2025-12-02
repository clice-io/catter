#pragma once

#include <cstddef>

namespace catter::array {

/**
 * Return a pointer to the last element of a nullptr terminated array.
 *
 * @param it the input array to count,
 * @return the pointer which points the nullptr.
 */
template <typename T>
constexpr T* end(T* it) noexcept {
    if(it == nullptr) {
        return nullptr;
    }

    while(*it != 0)
        ++it;
    return it;
}

/**
 * Return the size of a nullptr terminated array.
 *
 * @param begin the input array to count,
 * @return the size of the array.
 */
template <typename T>
constexpr size_t length(T* const begin) noexcept {
    return end(begin) - begin;
}

/**
 * Re-implementation of std::copy to avoid `memmove` symbol.
 *
 * @tparam I input type
 * @tparam O output type
 * @param src_begin
 * @param src_end
 * @param dst_begin
 * @param dst_end
 * @return output iterator to the last copied element.
 */
template <typename I, typename O>
constexpr O*
    copy(I* const src_begin, I* const src_end, O* const dst_begin, O* const dst_end) noexcept {
    auto src_it = src_begin;
    auto dst_it = dst_begin;
    for(; src_it != src_end && dst_it != dst_end;)
        *dst_it++ = *src_it++;

    return (src_it == src_end) ? dst_it : nullptr;
}

/**
 * Check if two nullptr terminated array is equal till the limit.
 *
 * @tparam T
 * @param lhs
 * @param rhs
 * @param length
 * @return true if the two array is equal till the limit.
 */
template <typename T>
constexpr bool equal_n(T* const lhs, T* const rhs, const size_t length) noexcept {
    for(size_t idx = 0; idx < length; ++idx) {
        if(lhs[idx] != rhs[idx])
            return false;
    }
    return true;
}
}  // namespace catter::array
