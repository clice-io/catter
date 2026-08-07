#include <exception>
#include <filesystem>
#include <print>
#include <string>
#include <vector>

#include "replay.h"
#include "util/log.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::println(stderr, "usage: it-catter-replay <replay.json> <script> [script args...]");
        return 2;
    }

    try {
        catter::log::mute_logger();

        const fs::path replay_path = argv[1];
        auto replay = catter::core::parse_replay_file(replay_path);

        catter::core::ReplayConfig config;
        config.script = argv[2];
        for(int index = 3; index < argc; ++index) {
            config.script_args.emplace_back(argv[index]);
        }

        // Relative fixture paths resolve against the replay file directory so
        // checked-in fixtures stay portable across platforms.
        const fs::path base = replay_path.parent_path();
        if(replay.cwd.has_value()) {
            fs::path cwd = *replay.cwd;
            config.working_directory = cwd.is_absolute() ? cwd : (base / cwd).lexically_normal();
        } else {
            config.working_directory = base;
        }

        catter::core::ReplayRunner runner;
        runner.run(std::move(config), replay);
        return 0;
    } catch(const std::exception& error) {
        std::println(stderr, "replay failed: {}", error.what());
        return 1;
    } catch(...) {
        std::println(stderr, "replay failed: unknown error");
        return 1;
    }
}
