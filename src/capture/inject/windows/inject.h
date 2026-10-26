#pragma once

#include <string_view>
#include <windows.h>

namespace catter {
/// Adds the hook DLL of the suspended process's machine to its imports, so
/// that the loader maps it before the program's own code runs. A process of a
/// machine catter has no hook for runs without one, its commands uncaptured:
/// a warning names the program.
void inject_hook(HANDLE process, std::string_view executable);
}  // namespace catter
