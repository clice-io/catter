#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <kota/zest/zest.h>
#include <kota/support/utf16.h>
#include <kota/fs/directory.h>
#include <kota/fs/file.h>
#include <kota/fs/path.h>
#include <kota/fs/resolve.h>
#include <kota/sys/env.h>

#include "hook/windows/util.h"

namespace ct = catter::hook;

namespace {

struct ScopedEnvVar {
    std::string name;
    std::optional<std::string> original;

    ScopedEnvVar(std::string name_value, std::string_view value) : name(std::move(name_value)) {
        std::string old;
        if(kota::sys::get_env(old, name)) {
            original = std::move(old);
        }
        kota::sys::set_env(name, value);
    }

    ~ScopedEnvVar() {
        if(original) {
            kota::sys::set_env(name, *original);
        } else {
            kota::sys::unset_env(name);
        }
    }
};

/// A file `name` in directory `directory` under the current one, which goes
/// with it. Tests run at once: each takes a directory of its own.
struct ScopedFile {
    std::string path;

    ScopedFile(std::string_view directory, std::string_view name) {
        kota::fs::current_dir(path);
        kota::fs::path::push(path, directory);
        kota::fs::create_directories(path);
        kota::fs::path::push(path, name);
        kota::fs::write_file(path, "");
    }

    ~ScopedFile() {
        kota::fs::remove_all(kota::fs::path::parent(path));
    }
};

std::wstring wide(std::string_view text) {
    std::wstring out;
    kota::to_utf16(out, text);
    return out;
}

ZEST_SUITE(win_payload_util) {
ZEST_CASE(proxy_command_line_quotes_what_needs_it) {
    ScopedEnvVar proxy("CATTER_PROXY_PATH", R"(C:\Program Files\Catter\catter-proxy.exe)");
    ScopedEnvVar id("CATTER_IPC_ID", "12345");
    ScopedEnvVar pipe("CATTER_IPC_PIPE", R"(\\.\pipe\catter-ipc-1)");
    ScopedFile tool("catter util tests", "clang-cl.exe");

    auto command = ct::proxy_command_line(wide(tool.path).c_str(), LR"("clang-cl.exe" /c main.cc)");

    ZEXPECT(
        command ==
        wide(std::format(
            R"("C:\Program Files\Catter\catter-proxy.exe" -p 12345 --exec "{}" --ipc \\.\pipe\catter-ipc-1 -- "clang-cl.exe" /c main.cc)",
            tool.path)));
};

ZEST_CASE(application_name_is_taken_from_the_current_directory) {
    ScopedFile tool("catter-application-name", "tool.exe");

    ZEXPECT(ct::resolve_program(LR"(catter-application-name\tool.exe)", L"tool a b") == tool.path);
};

ZEST_CASE(command_line_path_is_taken_from_the_current_directory) {
    ScopedFile tool("catter-command-line-path", "tool.exe");

    ZEXPECT(ct::resolve_program(nullptr, LR"(.\catter-command-line-path\tool.exe a b)") ==
            tool.path);
};

ZEST_CASE(unquoted_path_with_blanks_is_read_up_to_each_blank_in_turn) {
    ScopedFile tool("unquoted path tests", "tool.exe");

    ZEXPECT(ct::resolve_program(nullptr, wide(tool.path + " a b").c_str()) == tool.path);
    ZEXPECT(ct::resolve_program(nullptr, wide(tool.path).c_str()) == tool.path);
    ZEXPECT(ct::resolve_program(nullptr, wide(std::format(R"("{}" a b)", tool.path)).c_str()) ==
            tool.path);
};

ZEST_CASE(command_line_program_is_looked_for_as_create_process_does) {
    auto program = ct::resolve_program(nullptr, LR"("cmd" /c echo)");

    ZASSERT(program.has_value());
    ZEXPECT(program->ends_with(R"(\cmd.exe)"));
};

ZEST_CASE(program_found_nowhere_is_left_to_create_process) {
    ZEXPECT(ct::resolve_program(nullptr, L"catter-no-such-program a b") == std::nullopt);
    ZEXPECT(ct::resolve_program(L"catter-no-such-program.exe", L"a b") == std::nullopt);
    ZEXPECT(ct::proxy_command_line(nullptr, L"catter-no-such-program a b") == std::nullopt);
};
};  // ZEST_SUITE(win_payload_util)

}  // namespace
