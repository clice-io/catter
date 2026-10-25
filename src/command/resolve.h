#pragma once

#include "command/compiler.h"
#include "command/parse.h"

namespace catter::cmd {

/// Resolves what the arguments say into the files the command reads and writes.
///
/// The syntax proves some reads: the values of `/Tc`, the inputs after `/link`. A positional
/// argument is read when its suffix names a source, or, with no `-x`, an object or a
/// library. A write is the output an option names, else the compiler's default, named after
/// the inputs by the target's conventions.
CompilerAnalysis resolve(Identity identity, ParsedCommand parsed);

}  // namespace catter::cmd
