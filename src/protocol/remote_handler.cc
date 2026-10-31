module;

#include "modules/prelude.h"

#include "support/log.h"
#include "support/messages.h"
#include "support/output.h"
#include "support/program.h"

module catter;

import :protocol.analysis;
import :protocol.remote_handler;

namespace catter {

namespace {

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

struct Address {
    /// The addresses of the host, in the order to try them.
    std::vector<std::string> hosts;

    int port;
};

/// `host:port`, the host of an IPv6 address in brackets.
Address parse_address(std::string_view address, bool allow_remote) {
    auto colon = address.rfind(':');
    int port = 0;
    if(colon != std::string_view::npos) {
        auto digits = address.substr(colon + 1);
        auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), port);
        if(error != std::errc() || end != digits.data() + digits.size() || port <= 0 ||
           port > 65535) {
            port = 0;
        }
    }
    if(port == 0) {
        throw cpptrace::runtime_error(
            std::format("`{}` is not an address for --connect: give it as host:port", address));
    }

    auto host = address.substr(0, colon);
    if(host.starts_with('[') && host.ends_with(']')) {
        host = host.substr(1, host.size() - 2);
    }
    if(host == "localhost") {
        // A handler listening on localhost may have taken either.
        return {
            .hosts = {"127.0.0.1", "::1"},
            .port = port
        };
    }
    // catter resolves no names: the host is an IPv4 or IPv6 address.
    if(host.empty() || (host.find_first_not_of("0123456789.") != std::string_view::npos &&
                        host.find(':') == std::string_view::npos)) {
        throw cpptrace::runtime_error(std::format(
            "`{}` names no IP address: give --connect the handler's IP address and port",
            address));
    }
    const bool local = host == "::1" || host.starts_with("127.");
    if(!local) {
        if(!allow_remote) {
            throw cpptrace::runtime_error(std::format(
                "`{}` is not on this machine, and a handler can change and drop the build's commands: give --allow-remote to connect to it",
                address));
        }
        output::warn("the handler at `{}` on another machine decides on the build", address);
    }
    return {.hosts = {std::string(host)}, .port = port};
}

std::string seconds(std::chrono::milliseconds time) {
    return std::format("{} s", std::chrono::duration<double>(time).count());
}

}  // namespace

RemoteHandler::RemoteHandler(std::unique_ptr<kotatsu::ipc::Transport> transport,
                             kotatsu::async::process process,
                             Options options) :
    options(std::move(options)), peer(kotatsu::async::event_loop::current(), std::move(transport)),
    process(std::move(process)) {
    // A handler answers catter's requests, asks its own, and writes nothing
    // else: anything else ends the conversation, rather than leave catter
    // waiting for answers.
    peer.set_violation_policy(kotatsu::ipc::ViolationPolicy::End);
    peer.on_request(
        "analyze",
        [](kotatsu::ipc::JSONPeer::RequestContext&, const protocol::AnalyzeParams& params)
            -> kotatsu::async::task<protocol::AnalyzeResult, kotatsu::ipc::Error> {
            protocol::AnalyzeResult result;
            for(const auto& program: params.programs) {
                auto executable = protocol::to_bytes(program.executable);
                auto args = protocol::to_bytes(program.args);
                auto cwd = protocol::to_bytes(program.cwd.value_or(""));
                if(!executable || !args || !cwd) {
                    co_return kotatsu::outcome_error(
                        kotatsu::ipc::Error(kotatsu::ipc::protocol::ErrorCode::InvalidParams,
                                            !executable ? executable.error()
                                            : !args     ? args.error()
                                                        : cwd.error()));
                }
                result.analyses.push_back(
                    CommandAnalysis(*executable,
                                    *args,
                                    program.cwd ? std::optional<std::string_view>(*cwd)
                                                : std::nullopt)
                        .message());
            }
            co_return result;
        });
    kotatsu::async::event_loop::current().schedule(
        [](RemoteHandler& self) -> kotatsu::async::task<> {
            co_await self.peer.run();
            self.stopped.set();
        }(*this));
}

std::unique_ptr<RemoteHandler> RemoteHandler::spawn(std::string_view command, Options options) {
    // The terminal's Ctrl+C is the build's: the handler is told how the build
    // ended, and ends when catter closes its input.
    kotatsu::async::process::options opts{
        .creation = {.process_group = true},
        .streams = {kotatsu::async::process::stdio::pipe(),
                     kotatsu::async::process::stdio::pipe(),
                     kotatsu::async::process::stdio::inherit()},
    };
#ifdef CATTER_WINDOWS
    // The command line goes to CreateProcessW as it is, to be split by the
    // program as Windows programs split theirs.
    auto words = kotatsu::sys::windows::split_command_line(command);
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
    auto spawned = kotatsu::async::process::spawn(opts);
    if(!spawned) {
        throw cpptrace::runtime_error(std::format("Failed to start handler `{}`: {}",
                                                  options.name,
                                                  spawned.error().message()));
    }
    auto transport =
        std::make_unique<kotatsu::ipc::StreamTransport>(std::move(spawned->stdout_pipe),
                                                        std::move(spawned->stdin_pipe));
    return std::unique_ptr<RemoteHandler>(
        new RemoteHandler(std::move(transport), std::move(spawned->proc), std::move(options)));
}

kotatsu::async::task<std::unique_ptr<RemoteHandler>>
    RemoteHandler::connect(std::string_view address, bool allow_remote, Options options) {
    auto [hosts, port] = parse_address(address, allow_remote);
    std::string error;
    for(const auto& host: hosts) {
        auto transport = co_await kotatsu::ipc::StreamTransport::connect_tcp(
            host,
            port,
            kotatsu::async::event_loop::current());
        if(transport) {
            co_return std::unique_ptr<RemoteHandler>(new RemoteHandler(std::move(*transport),
                                                                       kotatsu::async::process(),
                                                                       std::move(options)));
        }
        if(error.empty()) {
            error = std::move(transport.error().message);
        }
    }
    throw cpptrace::runtime_error(
        std::format("Failed to connect to handler `{}`: {}", options.name, error));
}

std::string RemoteHandler::stopped_message(std::string_view before) const {
    if(auto end = peer.ended(); end->kind == kotatsu::ipc::ConversationEnd::Kind::Malformed) {
        return std::format("Handler `{}` wrote what the protocol does not allow: {}",
                           options.name,
                           end->message);
    }
    return std::format("Handler `{}` stopped{}", options.name, before);
}

std::string RemoteHandler::failure(std::string_view what, const kotatsu::ipc::Error& error) const {
    if(peer.ended()) {
        return stopped_message(std::format(" before it answered {}", what));
    }
    if(error.code == static_cast<int>(kotatsu::ipc::protocol::ErrorCode::RequestCancelled)) {
        return std::format("Handler `{}` did not answer {} within {}",
                           options.name,
                           what,
                           seconds(options.timeout));
    }
    return std::format("Handler `{}` failed on {}: {}", options.name, what, error.message);
}

template <typename Result, typename Params>
kotatsu::async::task<Result> RemoteHandler::request(std::string_view method,
                                                    std::string_view what,
                                                    Params params) {
    kotatsu::ipc::request_options opts;
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
            peer.ended()
                ? stopped_message("")
                : std::format("Handler `{}` stopped: {}", options.name, sent.error().message));
    }
}

kotatsu::async::task<std::optional<Build>> RemoteHandler::on_start(Build build) {
    subscription = co_await request<protocol::InitializeResult>(
        "initialize",
        "initialize",
        protocol::InitializeParams{
            .version = protocol::version,
            .session = options.session,
            .mode = options.mode,
            .decisions = options.decisions,
            .build =
                {
                        .command = protocol::to_os_string(build.command),
                        .cwd = protocol::to_os_string(build.cwd),
                        },
    });
    if(subscription.decides && !options.decisions) {
        throw cpptrace::runtime_error(std::format(
            "Handler `{}` decides on the build's commands, which the {} backend only observes: each runs before catter hears of it. Give a handler that observes, or another --capture",
            options.name,
            options.mode));
    }
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

kotatsu::async::task<Action> RemoteHandler::on_command(data::ipcid_t id,
                                                       std::optional<data::ipcid_t> parent,
                                                       data::command command) {
    auto action = co_await decide(id, parent, std::move(command));
    if(!action) {
        co_return Action{.type = Action::Type::Skip};
    }
    if(subscription.events.exit) {
        told.insert(id);
    }
    co_return std::move(*action);
}

kotatsu::async::task<std::optional<Action>>
    RemoteHandler::decide(data::ipcid_t id,
                          std::optional<data::ipcid_t> parent,
                          data::command command) {
    std::optional<CommandAnalysis> analysis;
    if(subscription.analysis || subscription.filter) {
        analysis.emplace(command.executable, command.args, command.cwd);
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
            analysis.emplace(command.executable, command.args, command.cwd);
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

kotatsu::async::task<> RemoteHandler::on_error(data::ipcid_t id,
                                               std::optional<data::ipcid_t> parent,
                                               std::string message) {
    if(subscription.events.error) {
        notify("error",
               protocol::CaptureError{
                   .id = id,
                   .parent = parent,
                   .message = kotatsu::replace_invalid_utf8(message),
               });
    }
    co_return;
}

kotatsu::async::task<> RemoteHandler::on_exit(data::ipcid_t id, data::process_result result) {
    if(told.erase(id) != 0) {
        notify("exit",
               protocol::Exit{.id = id,
                              .code = result.code,
                              .start = result.start,
                              .duration = result.duration});
    }
    co_return;
}

kotatsu::async::task<> RemoteHandler::on_finish(data::process_result result) {
    co_await request<kotatsu::ipc::protocol::null>("finish",
                                                   "finish",
                                                   protocol::Finish{.code = result.code});
}

kotatsu::async::task<> RemoteHandler::close() {
    peer.close_output();
    auto ended = [](RemoteHandler& self) -> kotatsu::async::task<> {
        co_await self.stopped.wait();
        if(self.process.pid() != -1) {
            co_await self.process.wait();
        }
    }(*this);
    if(options.timeout.count() == 0) {
        co_await std::move(ended);
        co_return;
    }

    auto result =
        co_await kotatsu::async::when_any(std::move(ended), kotatsu::async::sleep(options.timeout));
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
