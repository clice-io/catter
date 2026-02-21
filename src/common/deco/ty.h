#pragma once
#include "decl.h"
#include "util/meta.h"
#include <type_traits>

namespace deco::ty {

template <typename T>
concept is_decoed = std::is_base_of_v<deco::decl::CommonOptionFields, T>;
};  // namespace deco::ty
