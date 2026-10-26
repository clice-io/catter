#include <optional>
#include <string>
#include <string_view>
#include <kota/zest/zest.h>
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

ZEST_SUITE(win_payload_util) {
ZEST_CASE(proxy_command_line_quotes_what_needs_it) {
    ScopedEnvVar proxy("CATTER_PROXY_PATH", R"(C:\Program Files\Catter\catter-proxy.exe)");
    ScopedEnvVar id("CATTER_IPC_ID", "12345");
    ScopedEnvVar pipe("CATTER_IPC_PIPE", R"(\\.\pipe\catter-ipc-1)");

    auto command = ct::proxy_command_line(LR"(C:\Program Files\LLVM\bin\clang-cl.exe)",
                                          LR"("clang-cl.exe" /c main.cc)");

    ZEXPECT(
        command ==
        LR"("C:\Program Files\Catter\catter-proxy.exe" -p 12345 --exec "C:\Program Files\LLVM\bin\clang-cl.exe" --ipc \\.\pipe\catter-ipc-1 -- "clang-cl.exe" /c main.cc)");
};

ZEST_CASE(application_name_is_taken_from_the_current_directory) {
    std::string expected;
    ZEXPECT(!kota::fs::current_dir(expected));
    kota::fs::path::push(expected, "tool.exe");

    ZEXPECT(ct::resolve_program(L"tool.exe", L"tool a b") == expected);
};

ZEST_CASE(command_line_path_is_taken_from_the_current_directory) {
    std::string expected;
    ZEXPECT(!kota::fs::current_dir(expected));
    kota::fs::path::push(expected, "catter-util-tests");
    ZASSERT(kota::fs::create_directories(expected).has_value());
    kota::fs::path::push(expected, "tool.exe");
    ZASSERT(!kota::fs::write_file(expected, ""));

    auto program = ct::resolve_program(nullptr, LR"(.\catter-util-tests\tool.exe a b)");
    kota::fs::remove_all(kota::fs::path::parent(expected));

    ZEXPECT(program == expected);
};

ZEST_CASE(command_line_program_is_looked_for_as_create_process_does) {
    auto program = ct::resolve_program(nullptr, LR"("cmd" /c echo)");

    ZEXPECT(program.ends_with(R"(\cmd.exe)"));
};

ZEST_CASE(program_found_nowhere_stays_as_written) {
    ZEXPECT(ct::resolve_program(nullptr, L"catter-no-such-program a b") ==
            "catter-no-such-program");
};
};  // ZEST_SUITE(win_payload_util)

}  // namespace
