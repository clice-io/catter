#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "option/opt_specifier.h"
#include "option/opt_table.h"
#include "option/option.h"
#include "option/util.h"

namespace deco {
namespace backend = catter::opt;
}

namespace deco::trait {

template <typename Ty>
using BaseResultTy = std::remove_cvref_t<Ty>;

template <typename Ty>
concept StringResultType = std::constructible_from<std::string_view, BaseResultTy<Ty>>;

template <typename Ty>
struct OptionalTrait {
    using type = void;
};

template <typename Ty>
struct OptionalTrait<std::optional<Ty>> {
    using type = BaseResultTy<Ty>;
};

template <typename Ty>
using OptionalResultType = OptionalTrait<Ty>::type;

template <typename Ty>
concept CustomStringResultTy = requires(BaseResultTy<Ty>& value, std::string_view sv) {
    requires std::convertible_to<OptionalResultType<decltype(value.into(sv))>, std::string_view>;
};

template <typename Ty>
concept CustomStringVectorRsultTy = requires(BaseResultTy<Ty>& value,
                                             std::vector<std::string_view>& vals) {
    requires std::convertible_to<OptionalResultType<decltype(value.into(vals))>, std::string_view>;
};

template <typename Ty>
concept FlagResultType =
    std::same_as<BaseResultTy<Ty>, bool> || std::same_as<BaseResultTy<Ty>, uint32_t>;

template <typename Ty>
concept PrimitiveScalarResultType =
    std::same_as<BaseResultTy<Ty>, bool> || std::integral<BaseResultTy<Ty>> ||
    std::floating_point<BaseResultTy<Ty>> || StringResultType<Ty>;

template <typename Ty>
concept ScalarResultType = PrimitiveScalarResultType<Ty> || CustomStringResultTy<Ty>;

template <typename Ty>
concept PrimitiveVectorResultType =
    std::ranges::range<BaseResultTy<Ty>> &&
    requires(BaseResultTy<Ty>& value, std::ranges::range_value_t<BaseResultTy<Ty>> elem) {
        value.clear();
        value.emplace_back(std::move(elem));
    } && PrimitiveScalarResultType<std::ranges::range_value_t<BaseResultTy<Ty>>>;

template <typename Ty>
concept VectorResultType = PrimitiveVectorResultType<Ty> || CustomStringVectorRsultTy<Ty>;
}  // namespace deco::trait

#define DecoScalarResultErrString                                                                  \
    "Result type must be a primitive scalar (bool/number/string-like) or provide into(string_view)."

#define DecoVectorResultErrString                                                                  \
    "Result type must be a vector of primitive scalar values or provide into(vector<string_view>)."
