module;

#include "modules/prelude.h"

module catter;

import catter.command;

namespace kota_opt = kotatsu::option;
using namespace catter;

namespace {

std::vector<std::string> copy_values(std::span<const std::string_view> values) {
    std::vector<std::string> copied;
    copied.reserve(values.size());
    for(auto value: values) {
        copied.emplace_back(value);
    }
    return copied;
}

struct ParseResult {
    struct OwnedArg {
        std::uint32_t id;
        std::string spelling;
        std::vector<std::string> values;
        std::uint32_t index;
    };

    std::vector<OwnedArg> args;
    std::vector<std::string> errors;
};

ParseResult parse_command(const kota_opt::OptTable& table, std::span<const std::string> argv) {
    std::vector<std::string> args(argv.begin() + 1, argv.end());

    ParseResult result;
    kota_opt::ParseOptions options;
    options.dash_dash_parsing = true;
    for(auto parsed: table.parse(args, options)) {
        if(parsed.has_value()) {
            result.args.emplace_back(ParseResult::OwnedArg{
                .id = parsed->id,
                .spelling = parsed->spelling,
                .values = copy_values(parsed->values),
                .index = parsed->index,
            });
        } else {
            result.errors.emplace_back(parsed.error().message);
        }
    }
    return result;
};

std::string_view canonical_spelling(const kota_opt::OptTable& table,
                                    const ParseResult::OwnedArg& arg) {
    auto option = table.option(arg.id);
    if(!option.has_value()) {
        return arg.spelling;
    }
    return option->unaliased_option().prefixed_name();
};

}  // namespace

ZEST_SUITE(external_option_table_tests) {
ZEST_CASE(parse_lld_coff_link_command) {
    const auto argv =
        std::to_array<std::string>({"lld-link", "/out:app.exe", "/libpath:lib", "foo.obj", "/WX"});

    auto parsed = parse_command(option::lld_coff::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 4U);

    ZEXPECT(canonical_spelling(option::lld_coff::table(), parsed.args[0]) == "/out:");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "app.exe");

    ZEXPECT(canonical_spelling(option::lld_coff::table(), parsed.args[1]) == "/libpath:");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "lib");

    ZEXPECT(parsed.args[2].id == option::lld_coff::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.obj");

    ZEXPECT(canonical_spelling(option::lld_coff::table(), parsed.args[3]) == "/WX");
};

ZEST_CASE(parse_lld_elf_link_command) {
    const auto argv =
        std::to_array<std::string>({"ld.lld", "-o", "a.out", "--build-id=sha1", "foo.o"});

    auto parsed = parse_command(option::lld_elf::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(option::lld_elf::table(), parsed.args[0]) == "-o");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "a.out");

    ZEXPECT(canonical_spelling(option::lld_elf::table(), parsed.args[1]) == "--build-id=");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "sha1");

    ZEXPECT(parsed.args[2].id == option::lld_elf::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.o");
};

ZEST_CASE(parse_lld_macho_link_command) {
    const auto argv =
        std::to_array<std::string>({"ld64.lld", "-o", "a.out", "--help-hidden", "foo.o"});

    auto parsed = parse_command(option::lld_macho::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(option::lld_macho::table(), parsed.args[0]) == "-o");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "a.out");

    ZEXPECT(canonical_spelling(option::lld_macho::table(), parsed.args[1]) == "--help-hidden");
    ZEXPECT(option::lld_macho::table()
                .option(option::lld_macho::ID_force_cpusubtype_ALL)
                .value()
                .has_flag(kota_opt::HelpHidden));

    ZEXPECT(parsed.args[2].id == option::lld_macho::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.o");
};

ZEST_CASE(parse_lld_mingw_link_command) {
    const auto argv = std::to_array<std::string>({"ld.lld", "-L", "lib", "-o", "app.exe", "foo.o"});

    auto parsed = parse_command(option::lld_mingw::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(option::lld_mingw::table(), parsed.args[0]) == "-L");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "lib");

    ZEXPECT(canonical_spelling(option::lld_mingw::table(), parsed.args[1]) == "-o");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "app.exe");

    ZEXPECT(parsed.args[2].id == option::lld_mingw::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.o");
};

ZEST_CASE(parse_lld_wasm_link_command) {
    const auto argv =
        std::to_array<std::string>({"wasm-ld", "-o", "a.wasm", "--export-all", "foo.o"});

    auto parsed = parse_command(option::lld_wasm::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(option::lld_wasm::table(), parsed.args[0]) == "-o");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "a.wasm");

    ZEXPECT(canonical_spelling(option::lld_wasm::table(), parsed.args[1]) == "--export-all");

    ZEXPECT(parsed.args[2].id == option::lld_wasm::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.o");
};

ZEST_CASE(parse_llvm_dlltool_aliases) {
    const auto argv = std::to_array<std::string>(
        {"llvm-dlltool", "--machine", "i386:x86-64", "--dllname", "foo.dll"});

    auto parsed = parse_command(option::llvm_dlltool::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 2U);

    ZEXPECT(parsed.args[0].id == option::llvm_dlltool::ID_m);
    ZEXPECT(canonical_spelling(option::llvm_dlltool::table(), parsed.args[0]) == "-m");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "i386:x86-64");

    ZEXPECT(parsed.args[1].id == option::llvm_dlltool::ID_D);
    ZEXPECT(canonical_spelling(option::llvm_dlltool::table(), parsed.args[1]) == "-D");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "foo.dll");
};

ZEST_CASE(parse_llvm_lib_command) {
    const auto argv =
        std::to_array<std::string>({"llvm-lib", "/out:foo.lib", "/libpath:lib", "foo.obj"});

    auto parsed = parse_command(option::llvm_lib::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 3U);

    ZEXPECT(canonical_spelling(option::llvm_lib::table(), parsed.args[0]) == "/out:");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "foo.lib");

    ZEXPECT(canonical_spelling(option::llvm_lib::table(), parsed.args[1]) == "/libpath:");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "lib");

    ZEXPECT(parsed.args[2].id == option::llvm_lib::ID_INPUT);
    ZEXPECT(parsed.args[2].spelling == "foo.obj");
};

ZEST_CASE(parse_nvcc_command_and_aliases) {
    const auto argv = std::to_array<std::string>(
        {"nvcc", "-ofoo.o", "-I=include", "--std=c++17", "-no-align-double", "kernel.cu"});

    auto parsed = parse_command(option::nvcc::table(), argv);

    ZEXPECT(parsed.errors.empty());
    ZASSERT(parsed.args.size() == 5U);

    ZEXPECT(parsed.args[0].id == option::nvcc::ID_output_file);
    ZEXPECT(canonical_spelling(option::nvcc::table(), parsed.args[0]) == "--output-file");
    ZASSERT(parsed.args[0].values.size() == 1U);
    ZEXPECT(parsed.args[0].values[0] == "foo.o");

    ZEXPECT(parsed.args[1].id == option::nvcc::ID_include_path);
    ZEXPECT(canonical_spelling(option::nvcc::table(), parsed.args[1]) == "--include-path");
    ZASSERT(parsed.args[1].values.size() == 1U);
    ZEXPECT(parsed.args[1].values[0] == "include");

    ZEXPECT(parsed.args[2].id == option::nvcc::ID_std);
    ZEXPECT(canonical_spelling(option::nvcc::table(), parsed.args[2]) == "--std");
    ZASSERT(parsed.args[2].values.size() == 1U);
    ZEXPECT(parsed.args[2].values[0] == "c++17");

    ZEXPECT(parsed.args[3].id == option::nvcc::ID_no_align_double);
    ZEXPECT(canonical_spelling(option::nvcc::table(), parsed.args[3]) == "--no-align-double");

    ZEXPECT(parsed.args[4].id == option::nvcc::ID_INPUT);
    ZEXPECT(parsed.args[4].spelling == "kernel.cu");
};
};  // ZEST_SUITE(external_option_table_tests)
