#include "deco/decl.h"
#include "deco/macro.h"
#include "deco/runtime.h"
#include "zest/zest.h"
#include <iostream>
#include <optional>
#include <print>
#include <sstream>
#include <string_view>
#include <vector>
#include <zest/macro.h>

namespace {
// clang-format off
struct Version {
    DecoFlag(names = {"-v", "--version"}, help = "Show version and exit")
    version;
};

struct Help {
    DecoFlag(names = {"-h", "--help"}, help = "Show this help message and exit")
    help;
};

struct Request {
    struct RequestType {
        enum class Type {
            Get,
            Post,
        } type;
        std::optional<std::string> into(std::string_view input) {
            if (input == "GET" || input == "POST") {
                type = (input == "GET") ? Type::Get : Type::Post;
                return std::nullopt;
            } else {
                return "Invalid request type. Expected 'GET' or 'POST'.";
            }
        }
    };

    struct Url {
        std::string url;
        std::optional<std::string> into(std::string_view input) {
            if (input.starts_with("http://") || input.starts_with("https://")) {
                url = std::string(input);
                return std::nullopt;
            } else {
                return "Invalid URL. Expected to start with 'http://' or 'https://'.";
            }
        }
    };

    DecoFlag(help = "Enable verbose output", required = false)
    verbose = false;

    DecoKV(names = {"-X", "--type"}, meta_var = "<Method>")<RequestType>
    method;

    DecoKV(meta_var = "<URL>", help = "Request URL")<Url>
    url;
};

struct WebCliOpt {
    struct Cate {
        constexpr static deco::decl::Category version_category{
            .exclusive = true,
            .required = false,
            .name = "version",
            .description = "version-only mode",
        };
        constexpr static deco::decl::Category help_category{
            .exclusive = true,
            .required = false,
            .name = "help",
            .description = "help-only mode",
        };
        constexpr static deco::decl::Category request_category{
            .exclusive = true,
            .required = false,
            .name = "request",
            .description = "request options",
        };
    };
    DECO_CFG(required = false, category = Cate::version_category);
    Version version;

    DECO_CFG(required = true, category = Cate::request_category);
    Request request;

    DECO_CFG(required = false, category = Cate::help_category);
    Help help;
};

struct InputAndTrailingOpt {
    struct Cate {
        constexpr static deco::decl::Category input_category{
            .exclusive = false,
            .required = false,
            .name = "input",
            .description = "single positional input",
        };
        constexpr static deco::decl::Category trailing_category{
            .exclusive = false,
            .required = false,
            .name = "trailing",
            .description = "all arguments after --",
        };
    };

    DecoInput(required = false; category = Cate::input_category;)<std::string> input;
    DecoPack(required = false; category = Cate::trailing_category;)<std::vector<std::string>>
        trailing;
};

// clang-format on
}  // namespace

namespace {
template <typename... Args>
std::span<std::string> into_deco_args(Args&&... args) {
    static std::vector<std::string> res;
    res.clear();
    res.reserve(sizeof...(args));
    (res.emplace_back(std::forward<Args>(args)), ...);
    return res;
}
};  // namespace

TEST_SUITE(cli_parse){

    TEST_CASE(parsing){auto args = into_deco_args("-X", "POST", "--url", "https://example.com");
auto res = deco::cli::parse<WebCliOpt>(args);
EXPECT_TRUE(res.has_value());

const auto& opt = res->options;
EXPECT_TRUE(opt.request.method->type == Request::RequestType::Type::Post);
EXPECT_TRUE(opt.request.url->url == "https://example.com");
}

TEST_CASE(parsing_input_and_trailing) {
    auto args = into_deco_args("front", "--", "a", "b", "c");
    auto res = deco::cli::parse<InputAndTrailingOpt>(args);
    EXPECT_TRUE(res.has_value());
    if(!res.has_value()) {
        return;
    }
    const auto& opt = res->options;
    EXPECT_TRUE(*opt.input == "front");
    EXPECT_TRUE(opt.trailing->size() == 3);
    EXPECT_TRUE((*opt.trailing)[0] == "a");
    EXPECT_TRUE((*opt.trailing)[1] == "b");
    EXPECT_TRUE((*opt.trailing)[2] == "c");
    EXPECT_TRUE(res->matched_categories.contains(&InputAndTrailingOpt::Cate::input_category));
    EXPECT_TRUE(res->matched_categories.contains(&InputAndTrailingOpt::Cate::trailing_category));
}

TEST_CASE(when_error) {
    auto res = deco::cli::parse<WebCliOpt>(into_deco_args("-X", "INVALID"));
    EXPECT_FALSE(res.has_value());
    EXPECT_TRUE(res.error().type == deco::cli::ParseError::Type::IntoError &&
                res.error().message.contains("Invalid request type"));

    auto res2 = deco::cli::parse<WebCliOpt>(into_deco_args("--url", "ftp://example.com"));
    EXPECT_FALSE(res2.has_value());
    EXPECT_TRUE(res2.error().type == deco::cli::ParseError::Type::IntoError &&
                res2.error().message.contains("Invalid URL"));

    auto res3 = deco::cli::parse<WebCliOpt>(into_deco_args("--unknown"));
    EXPECT_FALSE(res3.has_value());
    EXPECT_TRUE(res3.error().type == deco::cli::ParseError::Type::BackendParsing &&
                res3.error().message.contains("unknown option"));

    auto res4 = deco::cli::parse<WebCliOpt>(into_deco_args("-v", "--help"));
    EXPECT_FALSE(res4.has_value());
    EXPECT_TRUE(res4.error().type == deco::cli::ParseError::Type::DecoParsing &&
                res4.error().message.contains("exclusive"));

    auto res5 = deco::cli::parse<WebCliOpt>(into_deco_args("-X", "GET"));
    EXPECT_FALSE(res5.has_value());
    EXPECT_TRUE(res5.error().type == deco::cli::ParseError::Type::DecoParsing &&
                res5.error().message.contains("required option"));

    auto res6 = deco::cli::parse<WebCliOpt>(into_deco_args("--", "a", "b"));
    EXPECT_FALSE(res6.has_value());
    std::println("Error message: {}", res6.error().message);
    EXPECT_TRUE(res6.error().type == deco::cli::ParseError::Type::BackendParsing &&
                res6.error().message.contains("unknown option"));
}
}
;

TEST_SUITE(dispatcher){
    TEST_CASE(dispatching){auto dispactcher = deco::cli::Dispatcher<WebCliOpt>("webcli [OPTIONS]");
std::stringstream ss;
dispactcher.dispatch(WebCliOpt::Cate::version_category, [&](auto) { ss << "Version 1.0.0"; })
    .dispatch(WebCliOpt::Cate::help_category, [&](auto) { dispactcher.usage(std::cout, true); })
    .dispatch(WebCliOpt::Cate::request_category,
              [&](WebCliOpt opt) {
                  EXPECT_TRUE(opt.request.method.value.has_value());
                  EXPECT_TRUE(opt.request.url.value.has_value());
              })
    .when_err([&](auto err) { ss << "Error: " << err.message << "\n"; });

dispactcher(into_deco_args("-v"));
EXPECT_TRUE(ss.str().contains("Version 1.0.0"));

ss.str("");
dispactcher(into_deco_args("--help"));
EXPECT_TRUE(ss.str().contains("usage: webcli [OPTIONS]"));

ss.str("");
dispactcher(into_deco_args("-X", "GET", "--url", "https://example.com"));
}
}
;

TEST_SUITE(subcommander){TEST_CASE(dispatching_with_subcommand_and_default){
    deco::cli::SubCommander subcommander("catter [OPTIONS]");
std::stringstream ss;
subcommander
    .add(
        deco::decl::SubCommand{
            .name = "run",
            .description = "Run task",
        },
        [&](std::span<std::string> args) {
            ss << "run:";
            for(const auto& arg: args) {
                ss << arg << ",";
            }
        })
    .add([&](std::span<std::string> args) {
        ss << "default:";
        for(const auto& arg: args) {
            ss << arg << ",";
        }
    })
    .when_err([&](auto err) { ss << "err:" << err.message; });

std::vector<std::string> run_args = {"run", "-v", "--dry"};
subcommander(run_args);
EXPECT_TRUE(ss.str() == "run:-v,--dry,");

ss.str("");
ss.clear();

std::vector<std::string> default_args = {"--help"};
subcommander(default_args);
EXPECT_TRUE(ss.str() == "default:--help,");
}

TEST_CASE(dispatching_with_subcommand_dispatcher) {
    std::stringstream ss;
    deco::cli::Dispatcher<WebCliOpt> web_dispatcher("web [OPTIONS]");
    web_dispatcher
        .dispatch(WebCliOpt::Cate::request_category,
                  [&](WebCliOpt opt) {
                      EXPECT_TRUE(opt.request.method.value.has_value());
                      EXPECT_TRUE(opt.request.url.value.has_value());
                      ss << "request-ok";
                  })
        .when_err([&](auto err) { ss << "dispatch-err:" << err.message; });

    deco::cli::SubCommander subcommander("catter [OPTIONS]");
    subcommander
        .add(
            deco::decl::SubCommand{
                .name = "web",
                .description = "Web request",
            },
            web_dispatcher)
        .when_err([&](auto err) { ss << "sub-err:" << err.message; });

    std::vector<std::string> args = {"web", "-X", "GET", "--url", "https://example.com"};
    subcommander(args);
    EXPECT_TRUE(ss.str() == "request-ok");
}

TEST_CASE(usage_with_default_and_overview) {
    deco::cli::SubCommander subcommander("catter [OPTIONS]", "Catter command line");
    subcommander
        .add(
            deco::decl::SubCommand{
                .name = "run",
                .description = "Run a task",
            },
            [](std::span<std::string>) {})
        .add(
            deco::decl::SubCommand{
                .name = "inspect",
                .description = "Inspect metadata",
                .command = "show",
            },
            [](std::span<std::string>) {})
        .add([](std::span<std::string>) {});

    std::stringstream ss;
    subcommander.usage(ss);
    const auto usage = ss.str();
    EXPECT_TRUE(usage.starts_with("Catter command line"));
    EXPECT_TRUE(usage.contains("usage: catter [OPTIONS]"));
    EXPECT_TRUE(usage.contains("Subcommands:"));
    EXPECT_TRUE(usage.contains("run"));
    EXPECT_TRUE(usage.contains("Run a task"));
    EXPECT_TRUE(usage.contains("inspect"));
    EXPECT_TRUE(usage.contains("(show)"));
}

TEST_CASE(usage_without_default_and_unknown_subcommand) {
    deco::cli::SubCommander subcommander("catter [OPTIONS]", "Overview text");
    std::stringstream ss;
    subcommander
        .add(
            deco::decl::SubCommand{
                .name = "run",
                .description = "Run a task",
            },
            [](std::span<std::string>) {})
        .when_err([&](auto err) { ss << err.message; });

    std::vector<std::string> args = {"unknown"};
    subcommander(args);
    EXPECT_TRUE(ss.str().contains("unknown subcommand 'unknown'"));

    ss.str("");
    ss.clear();
    subcommander.usage(ss);
    const auto usage = ss.str();
    EXPECT_TRUE(usage.starts_with("Overview text"));
    EXPECT_TRUE(!usage.contains("usage: catter [OPTIONS]"));
    EXPECT_TRUE(usage.contains("Subcommands:"));
    EXPECT_TRUE(usage.contains("run"));
}
}
;
