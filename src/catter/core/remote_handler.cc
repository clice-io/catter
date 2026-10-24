#include "remote_handler.h"

#include <algorithm>
#include <chrono>
#include <csignal>
#include <format>
#include <string>
#include <utility>
#include <variant>
#include <cpptrace/exceptions.hpp>

#include "util/log.h"

namespace catter::core {

namespace {

using kota::ipc::ReadError;

/// The handler's transport, which keeps how the handler's output ended.
class Transport final : public kota::ipc::Transport {
public:
    Transport(std::unique_ptr<kota::ipc::Transport> inner, std::optional<ReadError>& end) :
        inner(std::move(inner)), end(end) {}

    kota::task<std::string, ReadError> read_message() override {
        auto message = co_await inner->read_message();
        if(!message) {
            if(message.error().kind != ReadError::Kind::Oversized) {
                end = message.error();
            }
            co_return kota::outcome_error(std::move(message.error()));
        }
        co_return std::move(*message);
    }

    kota::task<void, kota::ipc::Error> write_message(std::string_view message) override {
        return inner->write_message(message);
    }

    kota::task<void, kota::ipc::Error> close_output() override {
        return inner->close_output();
    }

    kota::ipc::Result<void> close() override {
        return inner->close();
    }

private:
    std::unique_ptr<kota::ipc::Transport> inner;

    std::optional<ReadError>& end;
};

#ifdef CATTER_WINDOWS

/// The program of a command line, as CreateProcessW and CommandLineToArgvW
/// take it: up to the first space, or between the quotes it starts with.
std::string program_of(std::string_view command) {
    command.remove_prefix(std::min(command.find_first_not_of(" \t"), command.size()));
    if(command.starts_with('"')) {
        command.remove_prefix(1);
        return std::string(command.substr(0, command.find('"')));
    }
    return std::string(command.substr(0, command.find_first_of(" \t")));
}

#else

/// The words of a command line as a POSIX shell splits it, without expanding
/// anything: blanks separate words, and quotes and backslashes quote.
std::vector<std::string> split_words(std::string_view command) {
    std::vector<std::string> words;
    std::string word;
    bool in_word = false;
    for(std::size_t i = 0; i < command.size(); i += 1) {
        char c = command[i];
        if(c == ' ' || c == '\t' || c == '\n') {
            if(in_word) {
                words.push_back(std::move(word));
                word.clear();
                in_word = false;
            }
            continue;
        }
        in_word = true;
        if(c == '\\' && i + 1 < command.size()) {
            i += 1;
            word += command[i];
        } else if(c == '\'') {
            auto end = command.find('\'', i + 1);
            if(end == std::string_view::npos) {
                throw cpptrace::runtime_error(
                    std::format("The handler command `{}` has an unclosed '", command));
            }
            word += command.substr(i + 1, end - i - 1);
            i = end;
        } else if(c == '"') {
            i += 1;
            while(i < command.size() && command[i] != '"') {
                if(command[i] == '\\' && i + 1 < command.size() &&
                   std::string_view("$`\"\\\n").contains(command[i + 1])) {
                    i += 1;
                }
                word += command[i];
                i += 1;
            }
            if(i == command.size()) {
                throw cpptrace::runtime_error(
                    std::format("The handler command `{}` has an unclosed \"", command));
            }
        } else {
            word += c;
        }
    }
    if(in_word) {
        words.push_back(std::move(word));
    }
    return words;
}

#endif

#ifdef CATTER_WINDOWS
// libuv terminates the process for any signal it takes on Windows.
constexpr int kill_signal = SIGTERM;
#else
constexpr int kill_signal = SIGKILL;
#endif

std::string seconds(std::chrono::milliseconds time) {
    return std::format("{} s", std::chrono::duration<double>(time).count());
}

}  // namespace

RemoteHandler::RemoteHandler(std::unique_ptr<kota::ipc::Transport> transport,
                             kota::process process,
                             Options options) :
    options(std::move(options)),
    peer(kota::event_loop::current(),
         std::make_unique<Transport>(std::move(transport), output_end)),
    process(std::move(process)) {
    kota::event_loop::current().schedule([](RemoteHandler& self) -> kota::task<> {
        co_await self.peer.run();
        self.stopped.set();
    }(*this));
}

std::unique_ptr<RemoteHandler> RemoteHandler::spawn(std::string_view command, Options options) {
    kota::process::options opts{
        .streams = {kota::process::stdio::pipe(true, false),
                    kota::process::stdio::pipe(false, true),
                    kota::process::stdio::inherit()},
    };
#ifdef CATTER_WINDOWS
    // The command line goes to CreateProcessW as it is, to be split by the
    // program as Windows programs split theirs.
    opts.file = program_of(command);
    opts.args = {std::string(command)};
    opts.creation.windows_verbatim_arguments = true;
#else
    opts.args = split_words(command);
    if(opts.args.empty()) {
        throw cpptrace::runtime_error("The handler command is empty");
    }
    opts.file = opts.args.front();
#endif
    auto spawned = kota::process::spawn(opts);
    if(!spawned) {
        throw cpptrace::runtime_error(std::format("Failed to start handler `{}`: {}",
                                                  options.name,
                                                  spawned.error().message()));
    }
    auto transport = std::make_unique<kota::ipc::StreamTransport>(std::move(spawned->stdout_pipe),
                                                                  std::move(spawned->stdin_pipe));
    return std::unique_ptr<RemoteHandler>(
        new RemoteHandler(std::move(transport), std::move(spawned->proc), std::move(options)));
}

kota::task<std::unique_ptr<RemoteHandler>> RemoteHandler::connect(std::string host,
                                                                  int port,
                                                                  Options options) {
    auto transport =
        co_await kota::ipc::StreamTransport::connect_tcp(host, port, kota::event_loop::current());
    if(!transport) {
        throw cpptrace::runtime_error(std::format("Failed to connect to handler `{}`: {}",
                                                  options.name,
                                                  transport.error().message));
    }
    co_return std::unique_ptr<RemoteHandler>(
        new RemoteHandler(std::move(*transport), kota::process(), std::move(options)));
}

std::string RemoteHandler::stopped_message(std::string_view before) const {
    if(output_end->kind == ReadError::Kind::Malformed) {
        return std::format("Handler `{}` wrote what is not a protocol message: {}",
                           options.name,
                           output_end->message);
    }
    return std::format("Handler `{}` stopped{}", options.name, before);
}

std::string RemoteHandler::failure(std::string_view what, const kota::ipc::Error& error) const {
    if(output_end) {
        return stopped_message(std::format(" before it answered {}", what));
    }
    if(error.code == static_cast<int>(kota::ipc::protocol::ErrorCode::RequestCancelled)) {
        return std::format("Handler `{}` did not answer {} within {}",
                           options.name,
                           what,
                           seconds(options.timeout));
    }
    return std::format("Handler `{}` failed on {}: {}", options.name, what, error.message);
}

template <typename Result, typename Params>
kota::task<Result> RemoteHandler::request(std::string_view method,
                                          std::string_view what,
                                          Params params) {
    kota::ipc::request_options opts;
    if(options.timeout.count() != 0) {
        opts.timeout = options.timeout;
    }
    auto result = co_await peer.send_request<Result>(method, params, opts);
    if(!result) {
        throw cpptrace::runtime_error(failure(what, result.error()));
    }
    co_return std::move(*result);
}

template <typename Params>
void RemoteHandler::notify(std::string_view method, const Params& params) {
    if(auto sent = peer.send_notification(method, params); !sent) {
        throw cpptrace::runtime_error(
            output_end
                ? stopped_message("")
                : std::format("Handler `{}` stopped: {}", options.name, sent.error().message));
    }
}

kota::task<std::optional<Build>> RemoteHandler::on_start(Build build) {
    subscription = co_await request<protocol::InitializeResult>(
        "initialize",
        "initialize",
        protocol::InitializeParams{
            .version = protocol::version,
            .session = options.session,
            .mode = options.mode,
            .build =
                {
                        .command = protocol::to_os_string(build.command),
                        .cwd = protocol::to_os_string(build.cwd),
                        },
    });
    co_return build;
}

kota::task<Action> RemoteHandler::on_command(data::ipcid_t id,
                                             std::optional<data::ipcid_t> parent,
                                             data::command command) {
    auto action = co_await decide(id, parent, std::move(command));
    if(subscription.events.exit) {
        decided[id] = std::chrono::steady_clock::now();
    }
    co_return action;
}

kota::task<Action> RemoteHandler::decide(data::ipcid_t id,
                                         std::optional<data::ipcid_t> parent,
                                         data::command command) {
    if(!subscription.decides && !subscription.events.command) {
        co_return Action{.type = Action::Type::Skip};
    }
    protocol::Command params{
        .id = id,
        .parent = parent,
        .cwd = protocol::to_os_string(command.cwd),
        .executable = protocol::to_os_string(command.executable),
        .args = protocol::to_os_string(command.args),
    };
    if(subscription.env) {
        params.env = protocol::to_os_string(command.env);
    }
    if(!subscription.decides) {
        notify("command", params);
        co_return Action{.type = Action::Type::Skip};
    }

    auto what = std::format("command {}", id);
    auto action = co_await request<protocol::Action>("command", what, std::move(params));
    if(std::holds_alternative<protocol::Skip>(action)) {
        co_return Action{.type = Action::Type::Skip};
    }
    if(std::holds_alternative<protocol::Drop>(action)) {
        co_return Action{.type = Action::Type::Drop};
    }
    if(auto* abort = std::get_if<protocol::Abort>(&action)) {
        throw cpptrace::runtime_error(std::format("Handler `{}` aborted the build at {}: {}",
                                                  options.name,
                                                  what,
                                                  abort->reason));
    }

    auto& modify = std::get<protocol::Modify>(action);
    auto invalid = [&](const std::string& error) {
        return cpptrace::runtime_error(
            std::format("Handler `{}` answered {} with {}", options.name, what, error));
    };
    auto args = protocol::to_bytes(std::move(modify.args));
    if(!args) {
        throw invalid(args.error());
    }
    command.args = std::move(*args);
    if(modify.executable) {
        auto executable = protocol::to_bytes(std::move(*modify.executable));
        if(!executable) {
            throw invalid(executable.error());
        }
        command.executable = std::move(*executable);
    }
    if(modify.cwd) {
        auto cwd = protocol::to_bytes(std::move(*modify.cwd));
        if(!cwd) {
            throw invalid(cwd.error());
        }
        command.cwd = std::move(*cwd);
    }
    if(modify.env) {
        auto env = protocol::to_bytes(std::move(*modify.env));
        if(!env) {
            throw invalid(env.error());
        }
        command.env = std::move(*env);
    }
    co_return Action{.type = Action::Type::Modify, .command = std::move(command)};
}

kota::task<> RemoteHandler::on_error(data::ipcid_t id,
                                     std::optional<data::ipcid_t> parent,
                                     std::string message) {
    if(subscription.events.error) {
        notify("error",
               protocol::CaptureError{.id = id, .parent = parent, .message = std::move(message)});
    }
    co_return;
}

kota::task<> RemoteHandler::on_exit(data::ipcid_t id, data::process_result result) {
    auto start = decided.extract(id);
    if(start) {
        auto duration = std::chrono::steady_clock::now() - start.mapped();
        notify(
            "exit",
            protocol::Exit{
                .id = id,
                .code = result.code,
                .duration = std::chrono::duration_cast<std::chrono::microseconds>(duration).count(),
            });
    }
    co_return;
}

kota::task<> RemoteHandler::on_finish(data::process_result result) {
    co_await request<kota::ipc::protocol::null>("finish",
                                                "finish",
                                                protocol::Finish{.code = result.code});
}

kota::task<> RemoteHandler::close() {
    peer.close_output();
    auto ended = [](RemoteHandler& self) -> kota::task<> {
        co_await self.stopped.wait();
        if(self.process.pid() != -1) {
            co_await self.process.wait();
        }
    }(*this);
    if(options.timeout.count() == 0) {
        co_await std::move(ended);
        co_return;
    }

    auto result = co_await kota::when_any(std::move(ended), kota::sleep(options.timeout));
    if(result.index() == 0) {
        co_return;
    }
    LOG_WARN("Handler `{}` did not end within {} of the session's end",
             options.name,
             seconds(options.timeout));
    if(process.pid() != -1) {
        if(auto error = process.kill(kill_signal)) {
            LOG_WARN("Failed to kill handler `{}`: {}", options.name, error.message());
        }
        co_await process.wait();
    }
    if(auto closed = peer.close(); !closed) {
        LOG_WARN("Failed to close handler `{}`: {}", options.name, closed.error().message);
    }
    co_await stopped.wait();
}

}  // namespace catter::core
