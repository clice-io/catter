#pragma once

#include <string_view>

#include "cmd/compiler.h"
#include "cmd/parse.h"

namespace catter::cmd {

/// The target of a triple, alone: what it says, and the conventions of its files.
Target target_of(std::string_view triple, TargetSource source);

/// Resolves what the arguments say into the files the command reads and writes.
///
/// The syntax proves some reads: the values of `/Tc`, the inputs after `/link`. A positional
/// argument is read when its suffix names a source, or, with no `-x`, an object or a
/// library. A write is the output an option names, else the compiler's default, named after
/// the inputs by the target's conventions.
CompilerAnalysis resolve(Identity identity, ParsedCommand parsed);

}  // namespace catter::cmd
