#include "protocol/remote_handler.h"

#include <chrono>
#include <format>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <cpptrace/exceptions.hpp>
#include <kota/support/utf8.h>

#ifdef CATTER_WINDOWS
#include <kota/sys/windows.h>
#endif

#include "protocol/analysis.h"
#include "support/log.h"
#include "support/paths.h"

namespace catter {

namespace {

using kota::ipc::ReadError;

/// What is wrong with `message` from a handler, which answers catter's
/// requests, asks its own, and writes nothing else; none for those.
std::optional<std::string> not_an_answer(std::string_view message) {
    return std::visit(
        [](const auto& incoming) -> std::optional<std::string> {
            using T = std::remove_cvref_t<decltype(incoming)>;
            if constexpr(std::is_same_v<T, kota::ipc::IncomingResponse>) {
                return std::nullopt;
            } else if constexpr(std::is_same_v<T, kota::ipc::IncomingErrorResponse>) {
                if(incoming.id) {
                    return std::nullopt;
                }
                return std::format("an error that answers no request: {}", incoming.error.message);
            } else if constexpr(std::is_same_v<T, kota::ipc::IncomingRequest>) {
                return std::nullopt;
            } else if constexpr(std::is_same_v<T, kota::ipc::IncomingNotification>) {
                return std::format("a notification `{}`", incoming.method);
            } else {
                return incoming.error.message;
            }
        },
        kota::ipc::JSONCodec().parse_message(std::string(message)));
}

/// The handler's transport, which keeps how the handler's output ended, and
/// ends it at what is no protocol message: the peer would answer that with an
/// error, and go on waiting for the answers it asked for.
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
        if(auto wrong = not_an_answer(*message)) {
            end = ReadError{.kind = ReadError::Kind::Malformed, .message = std::move(*wrong)};
            co_return kota::outcome_error(*end);
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

#ifndef CATTER_WINDOWS

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
                    std::format(R"(The handler command `{}` has an unclosed ")", command));
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
    peer.on_request(
        "analyze",
        [](kota::ipc::JSONPeer::RequestContext&, const protocol::AnalyzeParams& params)
            -> kota::task<protocol::AnalyzeResult, kota::ipc::Error> {
            protocol::AnalyzeResult result;
            for(const auto& program: params.programs) {
                auto executable = protocol::to_bytes(program.executable);
                auto args = protocol::to_bytes(program.args);
                if(!executable || !args) {
                    co_return kota::outcome_error(
                        kota::ipc::Error(kota::ipc::protocol::ErrorCode::InvalidParams,
                                         executable ? args.error() : executable.error()));
                }
                result.analyses.push_back(CommandAnalysis(*executable, *args).message());
            }
            co_return result;
        });
    kota::event_loop::current().schedule([](RemoteHandler& self) -> kota::task<> {
        co_await self.peer.run();
        self.stopped.set();
    }(*this));
}

std::unique_ptr<RemoteHandler> RemoteHandler::spawn(std::string_view command, Options options) {
    kota::process::options opts{
        .streams = {kota::process::stdio::pipe(),
                    kota::process::stdio::pipe(),
                    kota::process::stdio::inherit()},
    };
#ifdef CATTER_WINDOWS
    // The command line goes to CreateProcessW as it is, to be split by the
    // program as Windows programs split theirs.
    auto words = kota::sys::windows::split_command_line(command);
    if(words.begin() == words.end()) {
        throw cpptrace::runtime_error("The handler command is empty");
    }
    opts.program = resolve_program(*words.begin());
    opts.windows_command_line = command;
#else
    auto words = split_words(command);
    if(words.empty()) {
        throw cpptrace::runtime_error("The handler command is empty");
    }
    opts.program = words.front();
    opts.args.assign(words.begin() + 1, words.end());
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

kota::task<std::unique_ptr<RemoteHandler>> RemoteHandler::connect(std::vector<std::string> hosts,
                                                                  int port,
                                                                  Options options) {
    std::string error;
    for(const auto& host: hosts) {
        auto transport =
            co_await kota::ipc::StreamTransport::connect_tcp(host,
                                                             port,
                                                             kota::event_loop::current());
        if(transport) {
            co_return std::unique_ptr<RemoteHandler>(
                new RemoteHandler(std::move(*transport), kota::process(), std::move(options)));
        }
        if(error.empty()) {
            error = std::move(transport.error().message);
        }
    }
    throw cpptrace::runtime_error(
        std::format("Failed to connect to handler `{}`: {}", options.name, error));
}

std::string RemoteHandler::stopped_message(std::string_view before) const {
    if(output_end->kind == ReadError::Kind::Malformed) {
        return std::format("Handler `{}` wrote what the protocol does not allow: {}",
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
    if(subscription.filter) {
        if(auto wrong = check_filter(*subscription.filter)) {
            throw cpptrace::runtime_error(
                std::format("Handler `{}` answered initialize with a filter catter cannot take: {}",
                            options.name,
                            *wrong));
        }
    }
    co_return build;
}

kota::task<Action> RemoteHandler::on_command(data::ipcid_t id,
                                             std::optional<data::ipcid_t> parent,
                                             data::command command) {
    auto action = co_await decide(id, parent, std::move(command));
    if(!action) {
        co_return Action{.type = Action::Type::Skip};
    }
    if(subscription.events.exit) {
        decided[id] = std::chrono::steady_clock::now();
    }
    co_return std::move(*action);
}

kota::task<std::optional<Action>> RemoteHandler::decide(data::ipcid_t id,
                                                        std::optional<data::ipcid_t> parent,
                                                        data::command command) {
    std::optional<CommandAnalysis> analysis;
    if(subscription.analysis || subscription.filter) {
        analysis.emplace(command.executable, command.args);
        if(subscription.filter && !analysis->matches(*subscription.filter)) {
            co_return std::nullopt;
        }
    }
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
    if(subscription.analysis) {
        params.analysis = analysis->message();
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

    auto invalid = [&](std::string_view error) {
        return cpptrace::runtime_error(
            std::format("Handler `{}` answered {} with an action catter cannot take: {}",
                        options.name,
                        what,
                        error));
    };
    if(auto* edit = std::get_if<protocol::Edit>(&action)) {
        if(!analysis) {
            analysis.emplace(command.executable, command.args);
        }
        auto args = analysis->edit(command.args, edit->edits);
        if(!args) {
            throw invalid(args.error());
        }
        command.args = std::move(*args);
        co_return Action{.type = Action::Type::Modify, .command = std::move(command)};
    }

    auto& modify = std::get<protocol::Modify>(action);
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
               protocol::CaptureError{
                   .id = id,
                   .parent = parent,
                   .message = kota::replace_invalid_utf8(message),
               });
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
        if(auto error = process.kill()) {
            LOG_WARN("Failed to kill handler `{}`: {}", options.name, error.message());
        }
        co_await process.wait();
    }
    if(auto closed = peer.close(); !closed) {
        LOG_WARN("Failed to close handler `{}`: {}", options.name, closed.error().message);
    }
    co_await stopped.wait();
}

}  // namespace catter
