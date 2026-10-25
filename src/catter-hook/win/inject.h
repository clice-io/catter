#pragma once

#include <string_view>

#include "winapi/win32.h"

namespace catter::proxy::hook {
/// Adds the hook DLL of the suspended process's machine to its imports, so
/// that the loader maps it before the program's own code runs. A process of a
/// machine catter has no hook for runs without one, its commands uncaptured:
/// a warning names the program.
void inject(HANDLE process, std::string_view executable);
}  // namespace catter::proxy::hook
