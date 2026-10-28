# fmt

A C++ library built four ways: by CMake with Ninja and with Make, its tests
on, and as a dependency of a small program, taken from WrapDB by Meson
(`meson/`) and from the Bazel Central Registry by Bazel (`bazel/`), each the
package of the same release. fmt 12 has no Meson or Bazel build of its own:
those builds are WrapDB's and the registry's overlays, as a project using fmt
gets them.
