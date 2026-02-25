#pragma once
#include <eventide/deco/decl.h>
#include <eventide/deco/macro.h>

namespace catter::proxy {
// clang-format off
struct ProxyOption {
    DecoKV(
        names = {"-p"},
        meta_var = "<Parent ID>",
       help = "specify the parent process ID."
    ) <int> parent_id;

    DecoKV(meta_var = "<Executable>",
           help = "a path, specify the executable to run")
    <std::string> exec;

    DecoInput(
        meta_var = "<Error Msg>",
        help = "if the input is not after a '--', then it is an error message from the hook",
        required = false
    ) <std::string> error_msg;

    DecoPack(
        meta_var = "<Args>",
        help = "arguments to the executable, must be after a '--'",
        required = false
    ) <std::vector<std::string>> args;

};

struct HelpOpt {
    DecoFlag(names = {"-h", "--help"}, help = "show this help message")
    help;
};

struct Option {
    struct Cate {
        constexpr static deco::decl::Category proxy = {
            .exclusive = true,
            .required = false,
            .name = "proxy",
            .description = "Options for catter-proxy",
        };
        constexpr static deco::decl::Category help = {
            .exclusive = true,
            .required = false,
            .name = "help",
            .description = "Options for showing help message",
        };
    };

    DECO_CFG(category = Cate::proxy)
    ProxyOption proxy_opt;

    DECO_CFG(category = Cate::help)
    HelpOpt help_opt;
};

// clang-format on
}  // namespace catter::proxy
