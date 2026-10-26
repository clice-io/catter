module;

#include "modules/prelude.h"

module catter.command:resolve;

import :compiler;
import :parse;
import :response_file;

namespace catter {

/// Resolves what the arguments say into the files the command reads and writes.
///
/// The syntax proves some reads: the values of `/Tc`, the inputs after `/link`. A positional
/// argument is read when its suffix names a source, or, with no `-x`, an object or a
/// library. A write is the output an option names, else the compiler's default, named after
/// the inputs by the target's conventions.
CompilerAnalysis resolve(Identity identity, Expansion expansion, ParsedCommand parsed);

}  // namespace catter
