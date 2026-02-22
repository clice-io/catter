#pragma once

#include <concepts>
#include <ranges>
#include <type_traits>
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
concept ScalarResultType =
    std::is_same_v<BaseResultTy<Ty>, bool> || std::integral<BaseResultTy<Ty>> ||
    std::floating_point<BaseResultTy<Ty>> || StringResultType<Ty>;

template <typename Ty>
concept VectorResultType =
    std::ranges::range<BaseResultTy<Ty>> &&
    std::ranges::output_range<BaseResultTy<Ty>, std::ranges::range_value_t<BaseResultTy<Ty>>> &&
    requires(BaseResultTy<Ty> v) {
        requires ScalarResultType<std::ranges::range_value_t<BaseResultTy<Ty>>>;
    };
};  // namespace deco::trait

#define DecScalarResultErrString                                                                   \
    "Result type must be a scalar type (bool/number/string) or convertible from a string_view."

#define DecVectorResultErrString                                                                   \
    "Result type must be a vector of scalar type that is either (bool/number/string) or convertible from a string_view."
