#include "util/serde.h"
#include "util/data.h"

#include <eventide/zest/macro.h>
#include <eventide/zest/zest.h>

using namespace catter;

TEST_SUITE(serde) {
TEST_CASE(serialize_and_deserialize_aggregate_types) {
    data::command cmd{
        .cwd = "/home/user",
        .executable = "/bin/ls",
        .args = {"-l",            "-a"             },
        .env = {"PATH=/usr/bin", "HOME=/home/user"}
    };
    {
        auto serialized = Serde<data::command>::serialize(cmd);
        BufferReader reader(serialized);
        auto deserialized = Serde<data::command>::deserialize(reader);

        EXPECT_TRUE(deserialized.cwd == cmd.cwd);
        EXPECT_TRUE(deserialized.executable == cmd.executable);
        EXPECT_TRUE(deserialized.args == cmd.args);
        EXPECT_TRUE(deserialized.env == cmd.env);
    }
    data::action act{
        .type = data::action::WRAP,
        .cmd = {.executable = "/bin/echo", .args = {"Hello, World!"}, .env = {}}
    };
    {

        auto serialized = Serde<data::action>::serialize(act);
        BufferReader reader(serialized);
        auto deserialized = Serde<data::action>::deserialize(reader);

        EXPECT_TRUE(deserialized.type == act.type);
        EXPECT_TRUE(deserialized.cmd.executable == act.cmd.executable);
        EXPECT_TRUE(deserialized.cmd.args == act.cmd.args);
        EXPECT_TRUE(deserialized.cmd.env == act.cmd.env);
    }
};

TEST_CASE(buffer_reader_consumes_exact_bytes) {
    data::command cmd{.cwd = "test", .executable = "echo"};
    auto serialized = Serde<data::command>::serialize(cmd);
    BufferReader reader(serialized);
    Serde<data::command>::deserialize(reader);
    EXPECT_TRUE(reader.remaining() == 0);
};
};  // TEST_SUITE(serde)
