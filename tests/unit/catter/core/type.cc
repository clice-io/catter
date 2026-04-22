#include "capi/type.h"

#include <optional>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "qjs.h"

using namespace catter;
using namespace catter::js;

namespace {

template <typename T>
bool is_roundtrip_equal(const qjs::Context& ctx, const T& value) {
    auto converted_back = T::make(value.to_object(ctx.js_context()));
    return converted_back == value;
}

}  // namespace

TEST_SUITE(api_tests) {
TEST_CASE(catter_runtime_conversion) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        js::CatterRuntime catter_runtime{
            .supportActions = {js::ActionType::skip, js::ActionType::modify},
            .type = js::CatterRuntime::Type::inject,
            .supportParentId = true
        };

        EXPECT_TRUE(is_roundtrip_equal(ctx, catter_runtime));
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(command_data_and_action_conversion) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        js::CommandData command_data{
            .cwd = "D:/Code/hook/catter",
            .exe = "clang++",
            .argv = {"clang++", "main.cc", "-c"},
            .env = {"CC=clang++", "CATTER_LOG=1"},
            .runtime = {.supportActions = {js::ActionType::skip, js::ActionType::modify},
                     .type = js::CatterRuntime::Type::env,
                     .supportParentId = true},
            .parent = 42
        };

        Action modify_action = Tag<ActionType::modify>{.data = command_data};

        Action skip_action = Tag<ActionType::skip>{

        };

        EXPECT_TRUE(is_roundtrip_equal(ctx, command_data));
        EXPECT_TRUE(is_roundtrip_equal(ctx, modify_action));
        EXPECT_TRUE(is_roundtrip_equal(ctx, skip_action));
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(process_result_and_config_conversion) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        js::ProcessResult process_result{
            .code = 0,
            .stdOut = "hello",
            .stdErr = "warn",
        };

        js::CatterConfig config{
            .scriptPath = "scripts/demo.js",
            .scriptArgs = {"--input", "compile_commands.json"},
            .buildSystemCommand = {"xmake", "build"},
            .buildSystemCommandCwd = "/tmp/catter-build",
            .runtime = {.supportActions = {js::ActionType::drop, js::ActionType::abort},
                           .type = js::CatterRuntime::Type::inject,
                           .supportParentId = false},
            .options = {.log = true},
            .execute = true
        };

        EXPECT_TRUE(is_roundtrip_equal(ctx, process_result));
        EXPECT_TRUE(is_roundtrip_equal(ctx, config));
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(option_item_and_info_conversion) {
    auto f = [&]() {
        auto runtime = qjs::Runtime::create();
        auto& ctx = runtime.context();

        js::OptionItem option_item{
            .values = {"include"},
            .key = "-I",
            .id = 12,
            .unalias = 34,
            .index = 1,
        };

        js::OptionInfo option_info{
            .id = 12,
            .prefixedKey = "-I",
            .kind = 11,
            .group = 0,
            .alias = 0,
            .aliasArgs = {"alias"},
            .flags = 4,
            .visibility = 0xffffffff,
            .param = 1,
            .help = "Add include directory",
            .meta_var = "<dir>",
        };

        EXPECT_TRUE(is_roundtrip_equal(ctx, option_item));
        EXPECT_TRUE(is_roundtrip_equal(ctx, option_info));
    };

    EXPECT_NOTHROWS(f());
};
};  // TEST_SUITE(api_tests)
