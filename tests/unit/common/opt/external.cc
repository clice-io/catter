#include <array>
#include <expected>
#include <string>
#include <vector>

#include <kota/option/option.h>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "opt/external/lld_coff.h"
#include "opt/external/lld_elf.h"
#include "opt/external/lld_macho.h"
#include "opt/external/lld_mingw.h"
#include "opt/external/lld_wasm.h"
#include "opt/external/nvcc.h"
#include "opt/external/llvm_dlltool.h"
#include "opt/external/llvm_lib.h"

namespace eo = kota::option;
using namespace catter;

namespace {

struct ParseResult {
    std::vector<eo::ParsedArgumentOwning> args;
    std::vector<std::string> errors;
};

ParseResult parse_command(const eo::OptTable& table, std::span<const std::string> argv) {
    std::vector<std::string> args(argv.begin() + 1, argv.end());

    ParseResult result;
    table.parse_args(args, [&](std::expected<eo::ParsedArgument, std::string> parsed) {
        if(parsed.has_value()) {
            result.args.emplace_back(eo::ParsedArgumentOwning::from_parsed_argument(*parsed));
        } else {
            result.errors.emplace_back(parsed.error());
        }
    });
    return result;
};

std::string_view canonical_spelling(const eo::OptTable& table,
                                    const eo::ParsedArgumentOwning& arg) {
    auto option = table.option(arg.unaliased_opt());
    if(!option.valid()) {
        return arg.get_spelling_view();
    }
    return option.prefixed_name();
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

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::lld_coff::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.obj");

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

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::lld_elf::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.o");
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
                    .has_flag(eo::HelpHidden));

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::lld_macho::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.o");
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

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::lld_mingw::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.o");
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

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::lld_wasm::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.o");
};

TEST_CASE(parse_llvm_dlltool_aliases) {
    const auto argv = std::to_array<std::string>(
        {"llvm-dlltool", "--machine", "i386:x86-64", "--dllname", "foo.dll"});

    auto parsed = parse_command(opt::llvm_dlltool::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 2U);

    EXPECT_EQ(parsed.args[0].unaliased_opt().id(), opt::llvm_dlltool::ID_m);
    EXPECT_EQ(canonical_spelling(opt::llvm_dlltool::table(), parsed.args[0]), "-m");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "i386:x86-64");

    EXPECT_EQ(parsed.args[1].unaliased_opt().id(), opt::llvm_dlltool::ID_D);
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

    EXPECT_EQ(parsed.args[2].option_id.id(), opt::llvm_lib::ID_INPUT);
    EXPECT_EQ(parsed.args[2].get_spelling_view(), "foo.obj");
};

TEST_CASE(parse_nvcc_command_and_aliases) {
    const auto argv = std::to_array<std::string>(
        {"nvcc", "-ofoo.o", "-I=include", "--std=c++17", "-no-align-double", "kernel.cu"});

    auto parsed = parse_command(opt::nvcc::table(), argv);

    EXPECT_TRUE(parsed.errors.empty());
    ASSERT_EQ(parsed.args.size(), 5U);

    EXPECT_EQ(parsed.args[0].unaliased_opt().id(), opt::nvcc::ID_output_file);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[0]), "--output-file");
    ASSERT_EQ(parsed.args[0].values.size(), 1U);
    EXPECT_EQ(parsed.args[0].values[0], "foo.o");

    EXPECT_EQ(parsed.args[1].unaliased_opt().id(), opt::nvcc::ID_include_path);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[1]), "--include-path");
    ASSERT_EQ(parsed.args[1].values.size(), 1U);
    EXPECT_EQ(parsed.args[1].values[0], "include");

    EXPECT_EQ(parsed.args[2].unaliased_opt().id(), opt::nvcc::ID_std);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[2]), "--std");
    ASSERT_EQ(parsed.args[2].values.size(), 1U);
    EXPECT_EQ(parsed.args[2].values[0], "c++17");

    EXPECT_EQ(parsed.args[3].unaliased_opt().id(), opt::nvcc::ID_no_align_double);
    EXPECT_EQ(canonical_spelling(opt::nvcc::table(), parsed.args[3]), "--no-align-double");

    EXPECT_EQ(parsed.args[4].option_id.id(), opt::nvcc::ID_INPUT);
    EXPECT_EQ(parsed.args[4].get_spelling_view(), "kernel.cu");
};
};  // TEST_SUITE(external_option_table_tests)
