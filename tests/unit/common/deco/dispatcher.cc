#include "deco/decl.h"
#include "deco/macro.h"
#include "deco/runtime.h"
#include "zest/zest.h"
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

    DecoFlag(help = "Enable verbose output";)
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

    DECO_CFG(required = false, category = Cate::request_category);
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
    res.reserve(sizeof...(args));
    (res.emplace_back(std::forward<Args>(args)), ...);
    return res;
}
};  // namespace

TEST_SUITE(deco_runtime) {
    TEST_CASE(dispatcher) {
        deco::cli::Dispatcher<WebCliOpt> dispatcher("webcli [options]");
        dispatcher
            .dispatch(WebCliOpt::Cate::version_category,
                      [](const auto*) { std::println("WebCli version 1.0.0"); })
            .dispatch(WebCliOpt::Cate::help_category, [&](const auto*) { ; });
    };

    TEST_CASE(parsing) {
        auto args = into_deco_args("-X", "POST", "--url", "https://example.com");
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
        EXPECT_TRUE(
            res->matched_categories.contains(&InputAndTrailingOpt::Cate::trailing_category));
    }

    TEST_CASE(when_error) {
        auto res = deco::cli::parse<WebCliOpt>(into_deco_args("-X", "INVALID"));
        EXPECT_FALSE(res.has_value());
        std::println("Error message: {}", res.error().message);
    }
};
