#pragma once

#include <cstdint>
#include <memory>
#include <kota/deco/deco.h>

#include "session/handler.h"

namespace catter::feature {

/// The options of `catter targets` that are its own.
struct TargetsOptions {
    DECO_CFG_START(required = false)

    DecoKVStyled(kota::deco::decl::KVStyle::JoinedOrSeparate,
                 names = {"-d", "--depth", "--depth="},
                 meta_var = "<n>",
                 help = "Limit render depth.")
    <std::int64_t> depth;

    DECO_CFG_END()
};

/// Renders the files the build makes as the forest of the files each is made from.
std::unique_ptr<Handler> make_targets(TargetsOptions options);

}  // namespace catter::feature
