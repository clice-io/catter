module;

#include "modules/prelude.h"

#include "support/log.h"
#include "support/messages.h"
#include "support/output.h"

#ifdef CATTER_MAC
#include <signal.h>
#include <unistd.h>

#include <mach/mach_time.h>
#endif

module catter;

import :capture.backend;
import :capture.eslogger;
import :capture.processes;
import :session.handler;
import :session.process;
import :session.session;

namespace catter {

#ifdef CATTER_MAC

namespace eslogger {

namespace {

std::uint64_t key_of(const AuditToken& token) {
    return (std::uint64_t(std::uint32_t(token.pid)) << 32) | std::uint32_t(token.pidversion);
}

/// `ticks` of Mach's absolute time, in microseconds.
std::chrono::microseconds microseconds_of(std::uint64_t ticks) {
    const static auto base = [] {
        mach_timebase_info_data_t info;
        mach_timebase_info(&info);
        return info;
    }();
    return std::chrono::microseconds(ticks * base.numer / base.denom / 1000);
}

}  // namespace

Event parse(std::string_view line) {
    auto event = kotatsu::codec::json::from_string<Event>(line);
    if(!event.has_value()) {
        throw cpptrace::runtime_error(
            std::format("eslogger wrote an event catter cannot read: {}", event.error().message));
    }
    if(event->schema_version != 1) {
        throw cpptrace::runtime_error(std::format(
            "eslogger writes its events in schema version {}, which catter does not read",
            event->schema_version));
    }
    if(!event->event.exec && !event->event.fork && !event->event.exit) {
        throw cpptrace::runtime_error("eslogger wrote an event that is no exec, fork or exit");
    }
    return std::move(*event);
}

std::vector<Report> follow(BuildProcesses& processes, Event event, int starter, int build) {
    const auto key = key_of(event.process.audit_token);
    if(event.event.fork.has_value()) {
        const auto& child = event.event.fork->child.audit_token;
        if(event.process.audit_token.pid == starter && child.pid == build) {
            processes.start(key_of(child));
        } else {
            processes.fork(key, key_of(child));
        }
        return {};
    }
    const auto time = microseconds_of(event.mach_time);
    if(event.event.exec.has_value()) {
        auto& exec = *event.event.exec;
        data::command command{
            .cwd = std::move(exec.cwd.path),
            .executable = std::move(exec.dyld_exec_path),
            .args = std::move(exec.args),
            .env = std::move(exec.env),
        };
        auto started =
            processes.exec(key, key_of(exec.target.audit_token), std::move(command), time);
        if(!started) {
            return {};
        }
        return {std::move(*started)};
    }
    return processes.exit(key, exit_status_of(event.event.exit->stat), time) |
           std::ranges::to<std::vector<Report>>();
}

}  // namespace eslogger

namespace {

constexpr std::string_view SUDO = "/usr/bin/sudo";

/// What sudo runs as root: eslogger, reporting the events catter reads.
constexpr std::array ESLOGGER = {"/usr/bin/eslogger", "exec", "fork", "exit"};

/// What catter runs to find where eslogger stands in its output.
constexpr std::string_view MARKER = "/usr/bin/true";

/// How long a marker waits for its event before another runs.
constexpr std::chrono::milliseconds MARKER_INTERVAL{100};

bool succeeded(const kotatsu::async::process::exit_status& status) {
    return status.code == 0 && status.term_signal == 0;
}

/// Makes sure sudo starts eslogger without asking for a password: it asks on
/// the terminal, before the build starts.
kotatsu::async::task<> authorize() {
    std::vector<std::string> args = {"-n", "-l"};
    args.append_range(ESLOGGER);
    auto probe = co_await kotatsu::async::process::capture(
        {.program = std::string(SUDO), .args = std::move(args)});
    if(probe.has_value() && succeeded(probe->status)) {
        co_return;
    }
    auto status = co_await run_process({
        .program = std::string(SUDO),
        .args = {"-v", "-p", "catter runs eslogger as root: password for %u: "},
    });
    if(!succeeded(status)) {
        throw cpptrace::runtime_error(std::format(
            "The eslogger backend runs {} as root, through sudo, which did not let it: run catter where sudo can ask for your password, or let sudo run {} without one",
            ESLOGGER.front(),
            ESLOGGER.front()));
    }
}

/// A build eslogger reports on: its commands go to the session in the order
/// eslogger reports them, until the build has ended.
class Capture {
public:
    Capture(kotatsu::async::process::spawn_result spawned, Session& session) :
        process(std::move(spawned.proc)), output(std::move(spawned.stdout_pipe)), session(session) {
    }

    /// Reads what eslogger writes until it ends.
    kotatsu::async::task<> read();

    /// Runs `build` once eslogger reports, and stops eslogger once it has
    /// reported every event of the build: how the build ended.
    kotatsu::async::task<kotatsu::async::process::exit_status> run(const Build& build);

private:
    /// What a line eslogger wrote tells.
    kotatsu::async::task<> take(std::string_view line);

    /// Runs markers until eslogger reports one: then every event before it has
    /// been read.
    kotatsu::async::task<> mark();

    /// Throws why eslogger stopped reporting.
    [[noreturn]] void stopped() const;

private:
    kotatsu::async::process process;

    kotatsu::async::pipe output;

    Session& session;

    /// This process, which starts the build and the markers.
    const int self = ::getpid();

    /// The markers that run, by pid.
    std::vector<int> markers;

    /// Whether eslogger reported one of `markers`.
    bool marked = false;

    /// Set once eslogger reports one of `markers`, or once what it writes is
    /// not read any more.
    kotatsu::async::event woken;

    /// Set once the build has ended: the marker eslogger reports next is the
    /// last of its events read.
    bool ending = false;

    /// Set once the last event of the build is read.
    bool done = false;

    /// Set once what eslogger writes is not read any more, before the last
    /// event of the build: eslogger stopped, or wrote what catter cannot read,
    /// `failure`.
    bool gone = false;

    std::exception_ptr failure;

    /// The build command's process, once it runs.
    int build_pid = -1;

    std::optional<BuildProcesses> processes;

    std::optional<std::uint64_t> last_seq;

    /// The events Endpoint Security dropped, which skip eslogger's count.
    std::uint64_t lost = 0;
};

kotatsu::async::task<> Capture::read() {
    while(true) {
        auto line = co_await output.read_line();
        if(!line.has_value() || !line->has_value()) {
            break;
        }
        // Stopped, eslogger may cut its last line.
        if(done || gone) {
            continue;
        }
        try {
            co_await take(**line);
        } catch(...) {
            failure = std::current_exception();
            gone = true;
            woken.set();
        }
    }
    gone = !done;
    woken.set();
}

kotatsu::async::task<> Capture::take(std::string_view line) {
    auto event = eslogger::parse(line);
    if(last_seq.has_value() && event.global_seq_num > *last_seq + 1) {
        lost += event.global_seq_num - *last_seq - 1;
    }
    last_seq = event.global_seq_num;

    if(event.event.exec.has_value() && event.process.ppid == self &&
       std::ranges::contains(markers, event.process.audit_token.pid)) {
        marked = true;
        done = ending;
        woken.set();
        co_return;
    }
    if(!processes.has_value()) {
        co_return;
    }
    for(auto& report: eslogger::follow(*processes, std::move(event), self, build_pid)) {
        if(auto* started = std::get_if<BuildProcesses::Started>(&report)) {
            co_await session.decide(started->id, started->parent, std::move(started->command));
        } else {
            const auto& ended = std::get<BuildProcesses::Ended>(report);
            co_await session.exited(ended.id, ended.result);
        }
    }
}

kotatsu::async::task<> Capture::mark() {
    markers.clear();
    marked = false;
    while(!marked) {
        if(gone) {
            stopped();
        }
        woken.reset();
        auto marker = kotatsu::async::process::spawn({
            .program = std::string(MARKER),
            .streams = {kotatsu::async::process::stdio::ignore(),
                        kotatsu::async::process::stdio::ignore(),
                        kotatsu::async::process::stdio::ignore()},
        });
        if(!marker.has_value()) {
            throw cpptrace::runtime_error(
                std::format("Failed to start {}: {}", MARKER, marker.error().message()));
        }
        markers.push_back(marker->proc.pid());
        co_await marker->proc.wait();
        co_await kotatsu::async::when_any(
            [](kotatsu::async::event& woken) -> kotatsu::async::task<> {
                co_await woken.wait();
            }(woken),
            kotatsu::async::sleep(MARKER_INTERVAL));
    }
}

void Capture::stopped() const {
    if(failure) {
        std::rethrow_exception(failure);
    }
    throw cpptrace::runtime_error(
        "eslogger stopped before it reported the build's commands. It runs as root, through sudo, and needs Full Disk Access for the app catter runs in (System Settings > Privacy & Security > Full Disk Access), or over SSH, Allow full disk access for remote users (System Settings > General > Sharing > Remote Login)");
}

kotatsu::async::task<kotatsu::async::process::exit_status> Capture::run(const Build& build) {
    kotatsu::ScopeExit stop([&] { [[maybe_unused]] auto error = process.kill(SIGTERM); });

    co_await mark();

    kotatsu::async::process::options options{
        .program = build.command.front(),
        .cwd = build.cwd,
    };
    options.args.assign(build.command.begin() + 1, build.command.end());
    const auto line = std::ranges::to<std::string>(build.command | std::views::join_with(' '));
    LOG_INFO("Starting the build command `{}` in {}", line, build.cwd);

    kotatsu::async::process::exit_status status;
    std::string error;
    try {
        status = co_await run_process(
            std::move(options),
            {
                .stand_in = true,
                .started =
                    [&](int pid) {
                        build_pid = pid;
                        processes.emplace([&] { return session.new_id(); },
                                          eslogger::microseconds_of(mach_absolute_time()) -
                                              session.time_of(std::chrono::steady_clock::now()));
                    },
            });
    } catch(const cpptrace::exception& ex) {
        error = ex.message();
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    }
    if(!error.empty()) {
        co_await session.uncaptured(session.new_id(),
                                    std::nullopt,
                                    std::format("Run `{}` failed:\n{}", line, error));
        co_return kotatsu::async::process::exit_status{.code = -1};
    }

    ending = true;
    co_await mark();
    if(lost != 0) {
        output::warn(
            "Endpoint Security dropped {} events eslogger was to report while the build ran: commands of the build may be missing",
            lost);
    }
    co_return status;
}

class ESLoggerBackend final : public Backend {
public:
    std::string_view name() const noexcept override {
        return "eslogger";
    }

    bool follows_decisions() const noexcept override {
        return false;
    }

    kotatsu::async::task<kotatsu::async::process::exit_status>
        run(const Build& build, Session& session) const override {
        assert(!build.command.empty());
        co_await authorize();
        std::vector<std::string> args = {"-n"};
        args.append_range(ESLOGGER);
        // eslogger reports no process of its own process group.
        auto spawned = kotatsu::async::process::spawn({
            .program = std::string(SUDO),
            .args = std::move(args),
            .creation = {.process_group = true},
            .streams = {kotatsu::async::process::stdio::ignore(),
                         kotatsu::async::process::stdio::pipe(),
                         kotatsu::async::process::stdio::inherit()},
        });
        if(!spawned.has_value()) {
            throw cpptrace::runtime_error(
                std::format("Failed to start {}: {}", ESLOGGER.front(), spawned.error().message()));
        }
        Capture capture(std::move(*spawned), session);
        auto [_, status] = co_await kotatsu::async::when_all{capture.read(), capture.run(build)};
        co_return status;
    }
};

}  // namespace

const Backend& eslogger_backend() noexcept {
    const static ESLoggerBackend backend;
    return backend;
}

#endif

}  // namespace catter
