#include <span>
#include <format>
#include <print>

#include "hook/interface.h"

#include "qjs.h"

int main(int argc, char* argv[]) {

    fix_cmd("example command");

    // if(argc < 2) {
    //     std::println("Usage: {} <command>", argv[0]);
    //     return 1;
    // }

    // std::span<const char* const> command{argv + 1, argv + argc};

    // std::error_code ec;

    // auto ret = catter::hook::run(command, ec);

    // if(ec) {
    //     std::println("Failed to attach hook: {}", ec.message());
    //     return 1;
    // }

    // if(ret != 0) {
    //     std::println("Command failed with exit code: {}", ret);
    // }

    // if(auto captured_output = catter::hook::collect_all(); captured_output.has_value()) {
    //     for(const auto& line: captured_output.value()) {
    //         std::println("{}", line);
    //     }
    // } else {
    //     std::println("Failed to collect captured output: {}", captured_output.error());
    // }
    return 0;
}
