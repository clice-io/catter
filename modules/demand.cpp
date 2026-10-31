// What catter takes from the libraries modules/ wraps, as a program
// scripts/modules.py indexes to write the rest of modules/: the standard
// headers it reaches C library names through, the libraries' headers it uses
// and the ones the headers of src/support it still includes use, the C
// library names only those headers bring, and the macros it uses, an #ifdef
// each. What catter starts to use that none of these brings goes here first,
// then modules/ is regenerated.

#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <charconv>
#include <chrono>
#include <concepts>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <initializer_list>
#include <iostream>
#include <iterator>
#include <list>
#include <memory>
#include <new>
#include <optional>
#include <print>
#include <random>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include <cpptrace/exceptions.hpp>
#include <kotatsu/async/async.h>
#include <kotatsu/async/io/loop.h>
#include <kotatsu/async/runtime/task.h>
#include <kotatsu/async/vocab/error.h>
#include <kotatsu/codec/dyn/dyn.h>
#include <kotatsu/codec/json/json.h>
#include <kotatsu/codec/json/schema.h>
#include <kotatsu/codec/macro.h>
#include <kotatsu/deco/deco.h>
#include <kotatsu/deco/option.h>
#include <kotatsu/fs/directory.h>
#include <kotatsu/fs/file.h>
#include <kotatsu/fs/path.h>
#include <kotatsu/fs/resolve.h>
#include <kotatsu/fs/status.h>
#include <kotatsu/fs/temp.h>
#include <kotatsu/ipc/codec.h>
#include <kotatsu/ipc/codec/bincode.h>
#include <kotatsu/ipc/codec/json.h>
#include <kotatsu/ipc/peer.h>
#include <kotatsu/ipc/peer/bincode.h>
#include <kotatsu/ipc/peer/json.h>
#include <kotatsu/ipc/protocol.h>
#include <kotatsu/ipc/transport.h>
#include <kotatsu/meta/annotation.h>
#include <kotatsu/meta/enum.h>
#include <kotatsu/support/functional.h>
#include <kotatsu/support/scope_exit.h>
#include <kotatsu/support/type_traits.h>
#include <kotatsu/support/utf16.h>
#include <kotatsu/support/utf8.h>
#include <kotatsu/sys/dirs.h>
#include <kotatsu/sys/process.h>
#include <kotatsu/zest/macro.h>
#include <kotatsu/zest/zest.h>
#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <kotatsu/sys/windows.h>
#endif

#ifndef _WIN32
using ::kill;
using ::pid_t;
using ::pthread_sigmask;
using ::sigaction;
using ::sigaddset;
using ::sigemptyset;
using ::siginfo_t;
using ::sigset_t;
#endif

#ifdef DECO_CFG
#endif
#ifdef DECO_CFG_END
#endif
#ifdef DECO_CFG_START
#endif
#ifdef DecoFlag
#endif
#ifdef DecoInput
#endif
#ifdef DecoKV
#endif
#ifdef DecoKVStyled
#endif
#ifdef DecoPack
#endif
#ifdef KOTATSU_ANNOTATION
#endif
#ifdef ZASSERT
#endif
#ifdef ZEST_CASE
#endif
#ifdef ZEST_CONTEXT
#endif
#ifdef ZEST_SUITE
#endif
#ifdef ZEST_SUITE_ATTRS
#endif
#ifdef ZEXPECT
#endif
#ifdef _IONBF
#endif
#ifdef assert
#endif
#ifdef stderr
#endif
#ifdef stdout
#endif
#ifndef _WIN32
#ifdef SA_RESTART
#endif
#ifdef SA_SIGINFO
#endif
#ifdef SIGHUP
#endif
#ifdef SIGINT
#endif
#ifdef SIGKILL
#endif
#ifdef SIGTERM
#endif
#ifdef SIG_DFL
#endif
#ifdef SIG_IGN
#endif
#ifdef SIG_UNBLOCK
#endif
#ifdef sa_handler
#endif
#ifdef sa_sigaction
#endif
#ifdef si_pid
#endif
#endif
