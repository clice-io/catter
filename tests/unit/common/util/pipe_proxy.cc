#include <string>
#include <string_view>

#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

#include "util/pipe_proxy.h"

using namespace catter::util;

TEST_SUITE(pipe_proxy) {
TEST_CASE(append_bounded_output_keeps_full_text_within_limit) {
    std::string buffer = "hello";
    bool truncated = false;

    append_bounded_output(buffer, " world", truncated, PIPE_PROXY_TRUNCATION_MARKER.size() + 32);

    EXPECT_FALSE(truncated);
    EXPECT_TRUE(buffer == "hello world");
};

TEST_CASE(append_bounded_output_keeps_latest_bytes_after_truncation) {
    constexpr size_t payload_limit = 8;
    const size_t limit = PIPE_PROXY_TRUNCATION_MARKER.size() + payload_limit;

    std::string buffer(40, '0');
    bool truncated = false;

    append_bounded_output(buffer, "abcdefghij", truncated, limit);

    EXPECT_TRUE(truncated);
    EXPECT_TRUE(buffer.starts_with(PIPE_PROXY_TRUNCATION_MARKER));
    EXPECT_TRUE(buffer.size() == limit);
    EXPECT_TRUE(buffer.substr(PIPE_PROXY_TRUNCATION_MARKER.size()) == "cdefghij");

    append_bounded_output(buffer, "KLMN", truncated, limit);

    EXPECT_TRUE(buffer.starts_with(PIPE_PROXY_TRUNCATION_MARKER));
    EXPECT_TRUE(buffer.substr(PIPE_PROXY_TRUNCATION_MARKER.size()) == "ghijKLMN");
};
};  // TEST_SUITE(pipe_proxy)
