#include <array>
#include <expected>
#include <string>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/deco/option.h>

#include "option/lld_coff.h"
#include "option/lld_elf.h"
#include "option/lld_macho.h"
#include "option/lld_mingw.h"
#include "option/lld_wasm.h"
#include "option/llvm_dlltool.h"
#include "option/llvm_lib.h"
#include "option/nvcc.h"

namespace kota_opt = kota::option;
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

TEST_SUITE(external_option_table_tests) {
TEST_CASE(parse_lld_coff_link_command) {
    const auto argv =
        std::to_array<std::string>({"lld-link", "/out:app.exe", "/libpath:lib", "foo.obj", "/WX"});

    auto parsed = parse_command(opt::lld_coff::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 4U);

    EXPECT_EQ(canonical_spelling(opt::lld_coff::table(), parsed.args[0]), "/out:");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "app.exe");

    EXPECT_EQ(canonical_spelling(opt::lld_coff::table(), parsed.args[1]), "/libpath:");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "lib");

    EXPECT_EQ(parsed.args[2].id, opt::lld_coff::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.obj");

    EXPECT_EQ(canonical_spelling(opt::lld_coff::table(), parsed.args[3]), "/WX");
};

TEST_CASE(parse_lld_elf_link_command) {
    const auto argv =
        std::to_array<std::string>({"ld.lld", "-o", "a.out", "--build-id=sha1", "foo.o"});

    auto parsed = parse_command(opt::lld_elf::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(opt::lld_elf::table(), parsed.args[0]), "-o");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "a.out");

    EXPECT_EQ(canonical_spelling(opt::lld_elf::table(), parsed.args[1]), "--build-id=");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "sha1");

    EXPECT_EQ(parsed.args[2].id, opt::lld_elf::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.o");
};

TEST_CASE(parse_lld_macho_link_command) {
    const auto argv =
        std::to_array<std::string>({"ld64.lld", "-o", "a.out", "--help-hidden", "foo.o"});

    auto parsed = parse_command(opt::lld_macho::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(opt::lld_macho::table(), parsed.args[0]), "-o");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "a.out");

    EXPECT_EQ(canonical_spelling(opt::lld_macho::table(), parsed.args[1]), "--help-hidden");
    EXPECT_TRUE(opt::lld_macho::table()
                    .option(opt::lld_macho::ID_force_cpusubtype_ALL)
                    .value()
                    .has_flag(kota_opt::HelpHidden));

    EXPECT_EQ(parsed.args[2].id, opt::lld_macho::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.o");
};

TEST_CASE(parse_lld_mingw_link_command) {
    const auto argv = std::to_array<std::string>({"ld.lld", "-L", "lib", "-o", "app.exe", "foo.o"});

    auto parsed = parse_command(opt::lld_mingw::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(opt::lld_mingw::table(), parsed.args[0]), "-L");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "lib");

    EXPECT_EQ(canonical_spelling(opt::lld_mingw::table(), parsed.args[1]), "-o");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "app.exe");

    EXPECT_EQ(parsed.args[2].id, opt::lld_mingw::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.o");
};

TEST_CASE(parse_lld_wasm_link_command) {
    const auto argv =
        std::to_array<std::string>({"wasm-ld", "-o", "a.wasm", "--export-all", "foo.o"});

    auto parsed = parse_command(opt::lld_wasm::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(opt::lld_wasm::table(), parsed.args[0]), "-o");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "a.wasm");

    EXPECT_EQ(canonical_spelling(opt::lld_wasm::table(), parsed.args[1]), "--export-all");

    EXPECT_EQ(parsed.args[2].id, opt::lld_wasm::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.o");
};

TEST_CASE(parse_llvm_dlltool_aliases) {
    const auto argv = std::to_array<std::string>(
        {"llvm-dlltool", "--machine", "i386:x86-64", "--dllname", "foo.dll"});

    auto parsed = parse_command(opt::llvm_dlltool::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].id, opt::llvm_dlltool::ID_m);
    EXPECT_EQ(canonical_spelling(opt::llvm_dlltool::table(), parsed.args[0]), "-m");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "i386:x86-64");

    EXPECT_EQ(parsed.args[1].id, opt::llvm_dlltool::ID_D);
    EXPECT_EQ(canonical_spelling(opt::llvm_dlltool::table(), parsed.args[1]), "-D");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "foo.dll");
};

TEST_CASE(parse_llvm_lib_command) {
    const auto argv =
        std::to_array<std::string>({"llvm-lib", "/out:foo.lib", "/libpath:lib", "foo.obj"});

    auto parsed = parse_command(opt::llvm_lib::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 3U);

    EXPECT_EQ(canonical_spelling(opt::llvm_lib::table(), parsed.args[0]), "/out:");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "foo.lib");

    EXPECT_EQ(canonical_spelling(opt::llvm_lib::table(), parsed.args[1]), "/libpath:");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "lib");

    EXPECT_EQ(parsed.args[2].id, opt::llvm_lib::ID_INPUT);
    EXPECT_EQ(parsed.args[2].spelling, "foo.obj");
};

TEST_CASE(parse_nvcc_command_and_aliases) {
    const auto argv = std::to_array<std::string>(
        {"nvcc", "-ofoo.o", "-I=include", "--std=c++17", "-no-align-double", "kernel.cu"});

    auto parsed = parse_command(opt::nvcc::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].id, opt::nvcc::ID_output_file);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[0]), "--output-file");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "foo.o");

    EXPECT_EQ(parsed.args[1].id, opt::nvcc::ID_include_path);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[1]), "--include-path");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "include");

    EXPECT_EQ(parsed.args[2].id, opt::nvcc::ID_std);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[2]), "--std");
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "c++17");

    EXPECT_EQ(parsed.args[3].id, opt::nvcc::ID_no_align_double);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[3]), "--no-align-double");

    EXPECT_EQ(parsed.args[4].id, opt::nvcc::ID_INPUT);
    EXPECT_EQ(parsed.args[4].spelling, "kernel.cu");
};
};  // TEST_SUITE(external_option_table_tests)
