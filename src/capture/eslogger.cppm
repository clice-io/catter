module;

#include "support/messages.h"
#include "support/prelude.h"

module catter:capture.eslogger;

import :capture.processes;

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

using Report = std::variant<BuildProcesses::Started, BuildProcesses::Ended>;

/// What `event` tells of the build that process `starter` started as process
/// `build`, whose processes are `processes`: the command that starts, or those
/// that end with their process. A process is its audit token, which changes as
/// it execs.
std::vector<Report> follow(BuildProcesses& processes, Event event, int starter, int build);

#endif

}  // namespace catter::eslogger
