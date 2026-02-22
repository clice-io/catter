#pragma once
#include "option/parsed_arg.h"
#include "trait.h"
#include <optional>
#include <string_view>
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

struct DecoFields {
    // if true, this option must be provided, otherwise it's optional
    bool required = true;
    // if true, options in the same category cannot be set at the same time, otherwise they must be
    // set together
    bool exclusive = false;
    // the category of this option, 0 means no category, options in the same category must be all
    // set or all unset
    unsigned category = 0;
};

struct CommonOptionFields : DecoFields {
    std::string_view help;
    std::string_view meta_var;

    constexpr CommonOptionFields() = default;

    virtual ~CommonOptionFields() = default;
    // return error message if parsing fails, otherwise return std::nullopt
    // virtual std::optional<std::string> into(backend::ParsedArgument&& arg) = 0;
};

// just to override the default value in this area
struct ConfigFields : CommonOptionFields {
    enum class Type : char {
        Start = 0,
        End = 1,
        Next = 2,  // just make sense to next
    };
    Type type;
};

struct NamedOptionFields : CommonOptionFields {

    std::vector<std::string_view> names;

    constexpr NamedOptionFields() = default;
};

template <typename ResTy>
struct InputOption : CommonOptionFields {
    static_assert(trait::ScalarResultType<ResTy>, DecScalarResultErrString);
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::Input;
    std::optional<ResTy> value = std::nullopt;

    constexpr InputOption() = default;
};

template <typename ResTy>
struct PackOption : CommonOptionFields {
    static_assert(trait::VectorResultType<ResTy>, DecVectorResultErrString);
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
    static_assert(trait::ScalarResultType<ResTy>, DecScalarResultErrString);
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::KV;
    KVStyle style = KVStyle::Separate;
    std::optional<ResTy> value = std::nullopt;

    constexpr KVOption() = default;
};

template <typename ResTy>
struct CommaJoinedOption : NamedOptionFields {
    static_assert(trait::VectorResultType<ResTy>, DecVectorResultErrString);
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::CommaJoined;
    std::optional<ResTy> value = std::nullopt;

    constexpr CommaJoinedOption() = default;
};

template <typename ResTy>
struct MultiOption : NamedOptionFields {
    static_assert(trait::VectorResultType<ResTy>, DecVectorResultErrString);
    using result_type = ResTy;
    constexpr static DecoType decoTy = DecoType::Multi;
    unsigned arg_num = 1;
    std::optional<ResTy> value = std::nullopt;

    constexpr MultiOption() = default;
};
}  // namespace deco::decl
