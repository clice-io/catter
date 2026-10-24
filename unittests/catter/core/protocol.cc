#include "protocol.h"

#include <string>
#include <variant>
#include <vector>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>
#include <kota/codec/json/json.h>
#include <kota/ipc/codec.h>

using namespace catter;

namespace {

template <typename T>
std::string json(const T& value) {
    return *kota::codec::json::to_string<kota::ipc::lsp_config>(value);
}

template <typename T>
T parse(std::string_view text) {
    return *kota::codec::json::from_string<T, kota::ipc::lsp_config>(text);
}

}  // namespace

ZEST_SUITE(protocol_tests) {

ZEST_CASE(utf8_is_a_string_and_other_bytes_are_base64) {
    EXPECT(json(protocol::to_os_string("caf\xc3\xa9")) == R"("café")");
    EXPECT(json(protocol::to_os_string("caf\xe9")) == R"({"base64":"Y2Fm6Q=="})");
}

ZEST_CASE(bytes_survive_base64_of_every_length) {
    for(std::string bytes: {"", "\xff", "\xff\xfe", "\xff\xfe\xfd", "\xff\xfe\xfd\xfc"}) {
        auto text = protocol::to_os_string(bytes);
        auto back = protocol::to_bytes(parse<protocol::OsString>(json(text)));
        EXPECT(back.has_value());
        EXPECT(*back == bytes);
    }
}

ZEST_CASE(invalid_base64_is_an_error) {
    for(std::string text: {"Y2F", "Y2F*", "Y=Fm", "Y2Fm6Q==Y2Fm"}) {
        EXPECT(!protocol::to_bytes(protocol::Base64{.base64 = text}).has_value());
    }
}

ZEST_CASE(an_action_is_tagged_by_its_name) {
    auto modify = parse<protocol::Action>(R"({"action":"modify","args":["cc",{"base64":"6Q=="}]})");
    auto* fields = std::get_if<protocol::Modify>(&modify);
    EXPECT(fields != nullptr);
    EXPECT(!fields->cwd.has_value());
    auto args = protocol::to_bytes(fields->args);
    EXPECT(args.has_value());
    EXPECT(*args == std::vector<std::string>{"cc", "\xe9"});

    auto abort = parse<protocol::Action>(R"({"action":"abort","reason":"no"})");
    EXPECT(std::get<protocol::Abort>(abort).reason == "no");
    EXPECT(std::holds_alternative<protocol::Skip>(parse<protocol::Action>(R"({"action":"skip"})")));
}

ZEST_CASE(an_absent_field_is_null_or_left_out) {
    EXPECT(json(protocol::CaptureError{.id = 2, .parent = {}, .message = "m"}) ==
           R"({"id":2,"parent":null,"message":"m"})");
    EXPECT(!parse<protocol::CaptureError>(R"({"id":2,"message":"m"})").parent.has_value());
}

};  // ZEST_SUITE(protocol_tests)
