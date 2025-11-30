#pragma once
#define NON_DEFAULT_CONSTRUCTABLE(T) T() noexcept = delete;

#define NON_COPYABLE_NOR_MOVABLE(T)                                                                \
    T(T const&) = delete;                                                                          \
    T& operator= (T const&) = delete;                                                              \
    T(T&&) noexcept = delete;                                                                      \
    T& operator= (T&&) noexcept = delete;

#define NON_COPYABLE(T)                                                                            \
    T(T const&) = delete;                                                                          \
    T& operator= (T const&) = delete;
