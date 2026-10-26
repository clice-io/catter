#pragma once
#include <functional>
#include <kota/async/async.h>

#include "session/data.h"

namespace catter {

/// The options that start `command` as it was captured: its program, argv,
/// environment and working directory. On Windows its command line is written
/// from its arguments, as CreateProcess takes it, which runs a batch file too.
kota::process::options process_options(const data::command& command);

/// How a program ended, as catter and its handlers are told it: a signal that
/// ended it as 128 and the signal's number, as a shell reports it.
data::process_result result_of(const kota::process::exit_status& status);

struct RunOptions {
    /// This process stands in for the program, as catter-proxy does: the
    /// program starts with the signals as exec leaves them, a signal another
    /// process sends this one goes on to it, and the program ends when this
    /// process is killed, but on macOS, where `started` has to see to it.
    bool stand_in = false;

    /// Told the program's pid once it runs.
    std::function<void(int)> started;
};

/// Starts a program as `opts` say; the task waits for its exit.
kota::task<kota::process::exit_status> run_process(kota::process::options opts,
                                                   RunOptions run = {});

}  // namespace catter
