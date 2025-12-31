#pragma once
#include <cstring>
#include <string_view>
#include <tuple>
#include <type_traits>

namespace catter::meta {

template <class T>
constexpr inline static bool dep_true = true;

template <typename T>
    requires std::is_function_v<std::remove_pointer_t<T>>
struct FuncDecomposer {};

template <typename Ret, typename... Args>
struct FuncDecomposer<Ret(Args...) noexcept> {
    using ParamTy = std::tuple<Args...>;
    using RetTy = Ret;
};

template <typename Ret, typename... Args>
struct FuncDecomposer<Ret(Args...) const noexcept> {
    using ParamTy = std::tuple<Args...>;
    using RetTy = Ret;
};

template <typename Ret, typename... Args>
struct FuncDecomposer<Ret(Args...) const> {
    using ParamTy = std::tuple<Args...>;
    using RetTy = Ret;
};

template <typename Ret, typename... Args>
struct FuncDecomposer<Ret(Args...)> {
    using ParamTy = std::tuple<Args...>;
    using RetTy = Ret;
};
}  // namespace catter::meta

namespace catter::meta {
template <typename T>
consteval std::string_view type_name() {
    std::string_view name =
#if defined(__clang__) || defined(__GNUC__)
        __PRETTY_FUNCTION__;  // Clang / GCC
#elif defined(_MSC_VER)
        __FUNCSIG__;  // MSVC
#else
#error "Unsupported compiler for meta::type_name"
#endif

#if defined(__clang__)
    constexpr std::string_view prefix = "std::string_view catter::meta::type_name() [T = ";
    constexpr std::string_view suffix = "]";
#elif defined(__GNUC__)
    constexpr std::string_view prefix =
        "consteval std::string_view catter::meta::type_name() [with T = ";
    constexpr std::string_view suffix = "; std::string_view = std::basic_string_view<char>]";
#elif defined(_MSC_VER)
        constexpr std::string_view prefix =
            "class std::basic_string_view<char,struct std::char_traits<char> > __cdecl catter::meta::type_name<";
    constexpr std::string_view suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

template <typename T, template <typename...> class TT>
struct is_specialization_of : std::false_type {};

template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template<Args...>, Template> : std::true_type {};

template <typename T, template <typename...> class TT>
constexpr bool is_specialization_of_v = is_specialization_of<T, TT>::value;

template <typename T, unsigned n>
struct tuple_slice {
    static_assert(dep_true<T>, "Unsupported type for tuple_slice");
};

template <typename First, typename... Ts, unsigned n>
struct tuple_slice<std::tuple<First, Ts...>, n> {
    using result = typename tuple_slice<std::tuple<Ts...>, n - 1>::result;
};

template <typename First, typename... Ts>
struct tuple_slice<std::tuple<First, Ts...>, 0> {
    using result = std::tuple<First, Ts...>;
};

template <typename... Ts, unsigned n>
struct tuple_slice<std::tuple<Ts...>, n> {
    static_assert(n == 0, "Slice size exceeds tuple size");
    using result = std::tuple<>;
};

template <typename T, unsigned n>
using tuple_slice_t = typename tuple_slice<T, n>::result;

template <typename Tuple, typename Ret>
struct build_func_signature {
    static_assert(dep_true<Tuple>, "Unsupported type for build_func_signature");
};

template <typename... Args, typename Ret>
struct build_func_signature<std::tuple<Args...>, Ret> {
    using type = Ret(Args...);
};

template <typename Tuple, typename Ret>
using build_func_signature_t = typename build_func_signature<Tuple, Ret>::type;

template <typename FuncSign>
using without_first_param_t =
    build_func_signature_t<tuple_slice_t<typename FuncDecomposer<FuncSign>::ParamTy, 1>,
                           typename FuncDecomposer<FuncSign>::RetTy>;

}  // namespace catter::meta
