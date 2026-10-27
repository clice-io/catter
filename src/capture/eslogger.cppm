module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:capture.eslogger;

/// The events eslogger writes, `eslogger exec fork exit`, one JSON object a
/// line, and the commands of a build they tell of. The fields are those catter
/// reads, named as eslogger names them after Endpoint Security's es_message_t
/// (libEndpointSecurity(3)).
namespace catter::eslogger {

#ifdef CATTER_MAC

/// A process, by its pid and the version of the pid: a pid another process
/// takes once this one has gone has another version, and so does every
/// program a process execs.
struct AuditToken {
    int pid;
    int pidversion;
};

struct Process {
    AuditToken audit_token;
    int ppid;
};

struct File {
    std::string path;
};

struct Exec {
    /// The process as it runs the program.
    Process target;
    std::vector<std::string> args;
    File cwd;
    std::vector<std::string> env;
    /// The path the program was started by, before its links resolve.
    std::string dyld_exec_path;
};

struct Fork {
    Process child;
};

struct Exit {
    /// How the process ended, as wait() tells it.
    int stat;
};

/// One of them, as the event's type is.
struct Details {
    std::optional<Exec> exec;
    std::optional<Fork> fork;
    std::optional<Exit> exit;
};

struct Event {
    int schema_version;
    /// When it happened, in Mach's absolute time.
    std::uint64_t mach_time;
    /// Counts the events eslogger was sent, which are lost where it skips.
    std::uint64_t global_seq_num;
    /// The process that forked, execed or exited, as it was.
    Process process;
    Details event;
};

/// The event `line` holds; throws on a line that is none catter reads.
Event parse(std::string_view line);

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

using Report = std::variant<Started, Ended>;

/// The processes of a build, which a process forks into its own and each
/// program they exec is a command of: the parent of a command is the command
/// its process ran before it, or the command that forked its process.
class BuildProcesses {
public:
    /// Of the build that process `starter` started as process `build`, whose
    /// commands take their ids from `new_id`.
    BuildProcesses(int starter, int build, std::function<data::ipcid_t()> new_id) :
        starter(starter), build(build), new_id(std::move(new_id)) {}

    /// What `event` tells of the build's commands: the one that starts, or
    /// those that end with their process, the last it ran first, each having
    /// run from its exec to the exit.
    std::vector<Report> add(Event event);

private:
    struct Run {
        data::ipcid_t id;

        /// When the process execed the command's program.
        std::uint64_t started;
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
    int starter;

    int build;

    std::function<data::ipcid_t()> new_id;

    std::unordered_map<std::uint64_t, Member> members;
};

#endif

}  // namespace catter::eslogger
