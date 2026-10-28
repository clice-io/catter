module;

#include "modules/prelude.h"

#include "support/messages.h"

#ifndef CATTER_WINDOWS
#include <signal.h>
#endif

module catter:session.process;

namespace catter {

/// Holds each standard stream this process was started without, which nothing
/// it opens then takes the number of; the program it stands in for gets it
/// closed (`RunOptions::stand_in`). Called first in main, before anything is
/// opened. Windows numbers no handle as a standard stream: nothing to hold.
void hold_closed_streams();

/// This process's environment, as a command carries it.
std::vector<std::string> environment();

/// The options that start `command` as it was captured: its program, argv,
/// environment and working directory. On Windows its command line is written
/// from its arguments, as CreateProcess takes it, which runs a batch file too.
kota::process::options process_options(const data::command& command);

/// How a program that ran for `ran` ended, as catter and its handlers are told
/// it: a signal that ended it as 128 and the signal's number, as a shell
/// reports it.
data::process_result result_of(const kota::process::exit_status& status,
                               std::chrono::microseconds ran);

/// How a program that started at `started` ended, now.
data::process_result result_of(const kota::process::exit_status& status,
                               std::chrono::steady_clock::time_point started);

#ifndef CATTER_WINDOWS
/// How a program ended, as its wait status `status` tells.
kota::process::exit_status exit_status_of(int status);
#endif

/// Ends this process as `status` says a program ended: by the same signal, or
/// with the same exit code, which it returns for main to return.
int exit_like(const kota::process::exit_status& status);

struct RunOptions {
    /// This process stands in for the program, as catter stands in for the
    /// build and catter-proxy for each command: the program starts with the
    /// signals as exec leaves them, a signal another process sends this one
    /// goes on to it, and the program ends when this process is killed, but
    /// on macOS, where nothing ends a child with its parent.
    bool stand_in = false;

    /// Told the program's pid once it runs.
    std::function<void(int)> started;
};

/// `opts` for a program this process stands in for (RunOptions::stand_in).
kota::process::options standing_in(kota::process::options opts);

/// While it lives, this process stands in for the program process `pid` runs,
/// which it started with the options standing_in gives: the signals that ask
/// this process to stop go on to the program, whose exit it waits for; on
/// Windows this process waits out an interrupt for it.
class StandIn {
public:
    explicit StandIn(int pid);

    StandIn(const StandIn&) = delete;
    StandIn& operator= (const StandIn&) = delete;

    ~StandIn();

#ifndef CATTER_WINDOWS

private:
    constexpr static std::array SIGNALS = {SIGHUP, SIGINT, SIGTERM};

    std::array<struct sigaction, SIGNALS.size()> previous{};
#endif
};

/// Starts a program as `opts` say; the task waits for its exit.
kota::task<kota::process::exit_status> run_process(kota::process::options opts,
                                                   RunOptions run = {});

}  // namespace catter
