#include "util/pipe_proxy.h"

#include <string>
#include <string_view>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

using namespace catter::util;

ZEST_SUITE(pipe_proxy) {
ZEST_CASE(append_bounded_output_keeps_full_text_within_limit) {
    std::string buffer = "hello";
    bool truncated = false;

    PipeProxy::append_bounded_output(buffer,
                                     " world",
                                     truncated,
                                     PipeProxy::truncation_marker.size() + 32);

    EXPECT(!truncated);
    EXPECT(buffer == "hello world");
};

ZEST_CASE(append_bounded_output_keeps_latest_bytes_after_truncation) {
    constexpr size_t payload_limit = 8;
    const size_t limit = PipeProxy::truncation_marker.size() + payload_limit;

    std::string buffer(40, '0');
    bool truncated = false;

    PipeProxy::append_bounded_output(buffer, "abcdefghij", truncated, limit);

    EXPECT(truncated);
    EXPECT(buffer.starts_with(PipeProxy::truncation_marker));
    EXPECT(buffer.size() == limit);
    EXPECT(buffer.substr(PipeProxy::truncation_marker.size()) == "cdefghij");

    PipeProxy::append_bounded_output(buffer, "KLMN", truncated, limit);

    EXPECT(buffer.starts_with(PipeProxy::truncation_marker));
    EXPECT(buffer.substr(PipeProxy::truncation_marker.size()) == "ghijKLMN");
};
};  // ZEST_SUITE(pipe_proxy)
