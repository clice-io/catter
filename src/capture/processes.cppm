module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:capture.processes;

namespace catter {

/// The processes of a build a backend observes, which a process forks into its
/// own and each program they exec is a command of: the parent of a command is
/// the command its process ran before it, or the command that forked its
/// process. The backend gives each process a key, and the time of each event
/// on a clock of its own.
class BuildProcesses {
public:
    /// A command of the build, which `parent` started.
    struct Started {
        data::ipcid_t id;
        std::optional<data::ipcid_t> parent;
        data::command command;
    };

    /// How a command of the build ended.
    struct Ended {
        data::ipcid_t id;
        data::process_result result;
    };

    /// Of a build whose commands take their ids from `new_id`.
    explicit BuildProcesses(std::function<data::ipcid_t()> new_id) : new_id(std::move(new_id)) {}

    /// Process `key` is the build command's, which it execs next.
    void start(std::uint64_t key);

    /// Process `parent` forked process `child`, which is of the build when
    /// `parent` is.
    void fork(std::uint64_t parent, std::uint64_t child);

    /// Process `key` execs `command` at `time`, and is process `next` from then
    /// on: the command that starts; none for a process not of the build.
    std::optional<Started> exec(std::uint64_t key,
                                std::uint64_t next,
                                data::command command,
                                std::chrono::microseconds time);

    /// Process `key` ended as `status` says at `time`: the commands that end
    /// with it, the last it ran first, each having run from its exec on.
    std::vector<Ended> exit(std::uint64_t key,
                            const kota::process::exit_status& status,
                            std::chrono::microseconds time);

private:
    struct Run {
        data::ipcid_t id;

        /// When the process execed the command's program.
        std::chrono::microseconds started;
    };

    struct Member {
        /// The command its parent ran when it forked, which starts the first
        /// program it runs; none for the build command's process.
        std::optional<data::ipcid_t> forked_by;

        /// The commands it ran, the last the one it runs.
        std::vector<Run> commands;

        /// The command of the process.
        std::optional<data::ipcid_t> current() const {
            return commands.empty() ? forked_by : commands.back().id;
        }
    };

private:
    std::function<data::ipcid_t()> new_id;

    std::unordered_map<std::uint64_t, Member> members;
};

}  // namespace catter
