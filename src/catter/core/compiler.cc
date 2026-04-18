
#include "compiler.h"

#include <array>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace catter {
struct CompilerPattern {
    Compiler type;
    std::regex pattern;
};

Compiler identify_compiler(std::string_view compiler_name) {

    const static auto DEFAULT_PATTERNS = []() {
        auto create_compiler_regex = [](std::string_view base_pattern,
                                        bool with_version) -> std::regex {
            std::string optional_path_prefix = R"((?:(?:.*[\\/]))?)";
            std::string exe_suffix = R"((?:\.exe)?)";

            std::string pattern_with_version = std::string(base_pattern);
            if(with_version) {
                pattern_with_version += R"((?:[-_]?([0-9]+(?:[._-][0-9a-zA-Z]+)*))?)";
            }

            std::string full_pattern =
                "^" + optional_path_prefix + pattern_with_version + exe_suffix + "$";

            return std::regex(full_pattern);
        };

        return std::to_array<CompilerPattern>({
            // simple cc and c++ (no version support)
            {Compiler::gcc,     create_compiler_regex(R"((?:[^/]*-)?(?:cc|c\+\+))",        false)},
            // GCC pattern
            {Compiler::gcc,
             create_compiler_regex(R"((?:[^/]*-)?(?:gcc|g\+\+|gfortran|egfortran|f95))",   true) },
            // GCC internal executables pattern: matches GCC's internal compiler phases
            {Compiler::gcc,
             create_compiler_regex(R"((?:cc1(?:plus|obj|objplus)?|f951|collect2|lto1))",   false)},
            // Clang pattern: matches clang, clang++, cross-compilation variants, and versioned
            // variants
            {Compiler::clang,   create_compiler_regex(R"((?:[^/]*-)?clang(?:\+\+)?)",      true) },
            // Fortran pattern: matches flang, cross-compilation variants, and versioned variants
            {Compiler::flang,   create_compiler_regex(R"((?:[^/]*-)?(?:flang|flang-new))", true) },
            // Intel Fortran pattern: matches ifort, ifx, and versioned variants
            {Compiler::ifort,   create_compiler_regex(R"((?:ifort|ifx))",                  true) },
            // Cray Fortran pattern: matches crayftn, ftn
            {Compiler::crayftn, create_compiler_regex(R"((?:crayftn|ftn))",                true) },
            // CUDA pattern: matches nvcc (NVIDIA CUDA Compiler) with optional cross-compilation
            // prefixes and version suffixes
            {Compiler::nvcc,    create_compiler_regex(R"((?:[^/]*-)?nvcc)",                true) },
            // Wrapper pattern: matches common compiler wrappers (no version support)
            {Compiler::wrapper, create_compiler_regex(R"((?:ccache|distcc|sccache))",      false)}
        });
    }();

    for(auto& compiler_pattern: DEFAULT_PATTERNS) {
        if(std::regex_match(std::string(compiler_name), compiler_pattern.pattern)) {
            return compiler_pattern.type;
        }
    }
    return Compiler::unknown;
}
}  // namespace catter
