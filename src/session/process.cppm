module;

#include "modules/prelude.h"

module catter:session.process;

import :session.data;

namespace catter {

/// Holds each standard stream this process was started without, which nothing
/// it opens then takes the number of; the program it stands in for gets it
/// closed (`RunOptions::stand_in`). Called first in main, before anything is
/// opened. Windows numbers no handle as a standard stream: nothing to hold.
void hold_closed_streams();

/// The options that start `command` as it was captured: its program, argv,
/// environment and working directory. On Windows its command line is written
/// from its arguments, as CreateProcess takes it, which runs a batch file too.
kota::process::options process_options(const data::command& command);

/// How a program ended, as catter and its handlers are told it: a signal that
/// ended it as 128 and the signal's number, as a shell reports it.
data::process_result result_of(const kota::process::exit_status& status);

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

/// Starts a program as `opts` say; the task waits for its exit.
kota::task<kota::process::exit_status> run_process(kota::process::options opts,
                                                   RunOptions run = {});

}  // namespace catter
