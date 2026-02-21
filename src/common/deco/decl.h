#pragma once
#include <concepts>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace deco::decl {
enum class DecoType {
    Input,
    // after "--"
    TrailingInput,
    // -p
    Flag,
    // -o 1
    KV,
    // -x,a,b,c
    CommaJoined,
    // -x 1 2 3, fixed size
    Multi,
};

enum class KVStyle : char {
    // -KEYValue
    Joined = 0,
    // -o 1
    Separate = 1
};

struct Prefix {
    enum { Dash = 0b1, DashDash = 0b10, Slash = 0b100 };

    unsigned char value = 0;

    constexpr Prefix() = default;

    constexpr Prefix(unsigned char v) : value(v) {}

    constexpr bool empty() const {
        return value == 0;
    }
};

struct CommonOptionFields {
    std::string_view help;
    bool required = true;

    constexpr CommonOptionFields() = default;
};

struct NamedOptionFields : CommonOptionFields {
    Prefix prefix = Prefix::DashDash;
    std::string_view name;
    std::vector<std::string_view> alias;

    constexpr NamedOptionFields() = default;
};

template <typename Ty>
using BaseResultTy = std::remove_cvref_t<Ty>;

template <typename Ty>
struct VectorResultTraits {
    constexpr static bool is_vector = false;
};

template <typename ElemTy, typename AllocTy>
struct VectorResultTraits<std::vector<ElemTy, AllocTy>> {
    constexpr static bool is_vector = true;
    using value_type = ElemTy;
};

template <typename Ty>
concept StringResultType =
    std::same_as<BaseResultTy<Ty>, std::string> || std::same_as<BaseResultTy<Ty>, std::string_view>;

template <typename Ty>
concept ScalarResultType = std::integral<BaseResultTy<Ty>> ||
                           std::floating_point<BaseResultTy<Ty>> || StringResultType<Ty>;

template <typename Ty>
concept MultiResultType = VectorResultTraits<BaseResultTy<Ty>>::is_vector && requires {
    typename VectorResultTraits<BaseResultTy<Ty>>::value_type;
} && ScalarResultType<typename VectorResultTraits<BaseResultTy<Ty>>::value_type>;

template <typename Ty>
concept SingleResultType = ScalarResultType<Ty>;

template <typename ResTy>
struct InputOption : CommonOptionFields {
    static_assert(
        SingleResultType<ResTy>,
        "InputOption ResTy must be bool/number/string (std::string or std::string_view).");
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::Input;
    std::optional<ResTy> value = std::nullopt;

    constexpr InputOption() = default;
};

template <typename ResTy>
struct PackOption : CommonOptionFields {
    static_assert(MultiResultType<ResTy>,
                  "PackOption ResTy must be std::vector<T>, where T is bool/number/string.");
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::TrailingInput;
    std::optional<ResTy> value = std::nullopt;

    constexpr PackOption() = default;
};

struct FlagOption : NamedOptionFields {
    using result_type = bool;
    constexpr static DecoType decoTy = DecoType::Flag;
    bool value = false;

    constexpr FlagOption() = default;
};

template <typename ResTy>
struct KVOption : NamedOptionFields {
    static_assert(SingleResultType<ResTy>,
                  "KVOption ResTy must be bool/number/string (std::string or std::string_view).");
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::KV;
    KVStyle style = KVStyle::Separate;
    std::optional<ResTy> value = std::nullopt;

    constexpr KVOption() = default;
};

template <typename ResTy>
struct CommaJoinedOption : NamedOptionFields {
    static_assert(MultiResultType<ResTy>,
                  "CommaJoinedOption ResTy must be std::vector<T>, where T is bool/number/string.");
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::CommaJoined;
    std::optional<ResTy> value = std::nullopt;

    constexpr CommaJoinedOption() = default;
};

template <typename ResTy>
struct MultiOption : NamedOptionFields {
    static_assert(MultiResultType<ResTy>,
                  "MultiOption ResTy must be std::vector<T>, where T is bool/number/string.");
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::Multi;
    unsigned arg_num = 1;
    std::optional<ResTy> value = std::nullopt;

    constexpr MultiOption() = default;
};
}  // namespace deco::decl
