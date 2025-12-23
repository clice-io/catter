#include <boost/ut.hpp>

#include "uv/rpc_data.h"

using namespace boost;
using namespace catter;


ut::suite<"rpc::data"> rpc_data = [] {

    ut::test("serialize and deserialize command") = [] {
        rpc::data::command cmd{
            .executable = "/bin/ls",
            .args = {"-l", "-a"},
            .env = {"PATH=/usr/bin", "HOME=/home/user"}};

        auto serialized = Serde<rpc::data::command>::serialize(cmd);

        auto deserialized = Serde<rpc::data::command>::deserialize(
            [&](char* buf, size_t size) {
                static size_t offset = 0;
                size_t to_copy = std::min(size, serialized.size() - offset);
                std::memcpy(buf, serialized.data() + offset, to_copy);
                offset += to_copy;
                return to_copy;
            });

        ut::expect(deserialized.executable == cmd.executable);
        ut::expect(deserialized.args == cmd.args);
        ut::expect(deserialized.env == cmd.env);
    };

    ut::test("serialize and deserialize action") = [] {
        rpc::data::action act{
            .type = rpc::data::action::WRAP,
            .cmd = {.executable = "/bin/echo", .args = {"Hello, World!"}, .env = {}}};

        auto serialized = Serde<rpc::data::action>::serialize(act);

        auto deserialized = Serde<rpc::data::action>::deserialize(
            [&](char* buf, size_t size) {
                static size_t offset = 0;
                size_t to_copy = std::min(size, serialized.size() - offset);
                std::memcpy(buf, serialized.data() + offset, to_copy);
                offset += to_copy;
                return to_copy;
            });

        ut::expect(deserialized.type == act.type);
        ut::expect(deserialized.cmd.executable == act.cmd.executable);
        ut::expect(deserialized.cmd.args == act.cmd.args);
        ut::expect(deserialized.cmd.env == act.cmd.env);
    };

};