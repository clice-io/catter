#include "uv/rpc_data.h"

#include <zest/macro.h>
#include <zest/zest.h>


using namespace catter;

TEST_SUITE(rpc_data) {
    TEST_CASE(serialize_and_deserialize_command) {
        rpc::data::command cmd{
            .working_dir = "/home/user",
            .executable = "/bin/ls",
            .args = {"-l",            "-a"             },
            .env = {"PATH=/usr/bin", "HOME=/home/user"}
        };

        auto serialized = Serde<rpc::data::command>::serialize(cmd);
        size_t offset = 0;
        auto deserialized = Serde<rpc::data::command>::deserialize([&](char* buf, size_t size) {
            size_t to_copy = std::min(size, serialized.size() - offset);
            std::memcpy(buf, serialized.data() + offset, to_copy);
            offset += to_copy;
            return to_copy;
        });

        EXPECT_TRUE(deserialized.working_dir == cmd.working_dir);
        EXPECT_TRUE(deserialized.executable == cmd.executable);
        EXPECT_TRUE(deserialized.args == cmd.args);
        EXPECT_TRUE(deserialized.env == cmd.env);
    };

    TEST_CASE(serialize_and_deserialize_action) {
        rpc::data::action act{
            .type = rpc::data::action::WRAP,
            .cmd = {.executable = "/bin/echo", .args = {"Hello, World!"}, .env = {}}
        };

        auto serialized = Serde<rpc::data::action>::serialize(act);

        auto deserialized = Serde<rpc::data::action>::deserialize([&](char* buf, size_t size) {
            static size_t offset = 0;
            size_t to_copy = std::min(size, serialized.size() - offset);
            std::memcpy(buf, serialized.data() + offset, to_copy);
            offset += to_copy;
            return to_copy;
        });

        EXPECT_TRUE(deserialized.type == act.type);
        EXPECT_TRUE(deserialized.cmd.executable == act.cmd.executable);
        EXPECT_TRUE(deserialized.cmd.args == act.cmd.args);
        EXPECT_TRUE(deserialized.cmd.env == act.cmd.env);
    };
};
