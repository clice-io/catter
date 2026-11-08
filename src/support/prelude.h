#pragma once

// What every module unit of catter includes first, in its global module
// fragment: the standard library and the third-party module, then the short
// names catter writes the namespaces it uses by. kotatsu::ipc keeps its name,
// which catter's own ipc takes. catter.command, to become a library of its
// own, includes modules/prelude.h alone.

#include "modules/prelude.h"

namespace catter {

namespace ranges = std::ranges;
namespace views = std::views;
namespace chrono = std::chrono;

namespace async = kotatsu::async;
namespace cli = kotatsu::cli;
namespace codec = kotatsu::codec;
namespace json = kotatsu::codec::json;
namespace fs = kotatsu::fs;
namespace meta = kotatsu::meta;
namespace sys = kotatsu::sys;
namespace zest = kotatsu::zest;

}  // namespace catter
