module;

#include "modules/prelude.h"

module catter;

import :protocol.protocol;

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
    ZEXPECT(json(protocol::to_os_string("caf\xc3\xa9")) == R"("café")");
    ZEXPECT(json(protocol::to_os_string("caf\xe9")) == R"({"base64":"Y2Fm6Q=="})");
}

ZEST_CASE(bytes_survive_base64_of_every_length) {
    for(std::string bytes: {"", "\xff", "\xff\xfe", "\xff\xfe\xfd", "\xff\xfe\xfd\xfc"}) {
        auto text = protocol::to_os_string(bytes);
        auto back = protocol::to_bytes(parse<protocol::OSString>(json(text)));
        ZEXPECT(back.has_value());
        ZEXPECT(*back == bytes);
    }
}

ZEST_CASE(invalid_base64_is_an_error) {
    for(std::string text: {"Y2F", "Y2F*", "Y=Fm", "Y2Fm6Q==Y2Fm"}) {
        ZEXPECT(!protocol::to_bytes(protocol::Base64{.base64 = text}).has_value());
    }
}

ZEST_CASE(an_action_is_tagged_by_its_name) {
    auto modify = parse<protocol::Action>(R"({"action":"modify","args":["cc",{"base64":"6Q=="}]})");
    auto* fields = std::get_if<protocol::Modify>(&modify);
    ZEXPECT(fields != nullptr);
    ZEXPECT(!fields->cwd.has_value());
    auto args = protocol::to_bytes(fields->args);
    ZEXPECT(args.has_value());
    ZEXPECT(*args == std::vector<std::string>{"cc", "\xe9"});

    auto abort = parse<protocol::Action>(R"({"action":"abort","reason":"no"})");
    ZEXPECT(std::get<protocol::Abort>(abort).reason == "no");
    ZEXPECT(
        std::holds_alternative<protocol::Skip>(parse<protocol::Action>(R"({"action":"skip"})")));
}

ZEST_CASE(an_absent_field_is_null_or_left_out) {
    ZEXPECT(json(protocol::CaptureError{.id = 2, .parent = {}, .message = "m"}) ==
            R"({"id":2,"parent":null,"message":"m"})");
    ZEXPECT(!parse<protocol::CaptureError>(R"({"id":2,"message":"m"})").parent.has_value());
}

};  // ZEST_SUITE(protocol_tests)
