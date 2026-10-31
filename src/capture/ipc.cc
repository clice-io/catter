module;

#include "support/prelude.h"

#ifdef CATTER_WINDOWS
#include <windows.h>
#endif

#include "support/log.h"
#include "support/messages.h"
#include "support/paths.h"

module catter;

import :capture.ipc;
#ifdef CATTER_WINDOWS
import :capture.inject;
#endif
import :session.process;
import :session.session;

namespace catter::ipc {

namespace {

using MakeDecision = Request<RequestType::MAKE_DECISION>;
using ReportError = Request<RequestType::REPORT_ERROR>;
using Finish = Request<RequestType::FINISH>;
using Started = Request<RequestType::STARTED>;

using Context = kotatsu::ipc::BincodePeer::RequestContext;

}  // namespace

/// A client of the pipe, which reports one command, `id`, of the session.
class Server::Client {
public:
    Client(Server& server, data::ipcid_t id) : server(server), id(id) {}

    Client(const Client&) = delete;
    Client& operator= (const Client&) = delete;

#ifdef CATTER_WINDOWS
    ~Client() {
        release();
    }
#endif

    async::task<> answer(async::pipe pipe);

private:
    async::task<MakeDecision::Result> decide(MakeDecision::Params params);

#ifdef CATTER_WINDOWS
    /// The hook started the command's program suspended, as process `pid`:
    /// whether it is made to run as decided, which the hook resumes then, or
    /// cannot run, which the hook ends.
    async::task<bool> started(std::int64_t pid);

    /// Ends the program started, which is not to run, with exit code `code`:
    /// from within, once its caller resumes it, if it can have the hook.
    void end(std::uint32_t code);

    /// Waits for the program started to exit, and tells its end; at once when
    /// none was. The hook goes once it has started the program.
    async::task<> wait_started();

    /// Tells those waiting for the program's exit that it is told.
    void release();
#endif

private:
    Server& server;

    data::ipcid_t id;

#ifdef CATTER_WINDOWS
    std::optional<data::ipcid_t> parent;

    decltype(data::action::type) decided = data::action::FAIL;

    std::string executable;

    /// The program the hook started, from its start until its exit is told.
    std::optional<async::process> program;

    /// When catter took the program the hook started.
    chrono::steady_clock::time_point began;

    std::optional<std::list<Untold>::iterator> untold;
#elif defined(CATTER_MAC)
    /// The program catter-proxy started, until it tells how it ended: macOS has
    /// nothing to end it with its proxy.
    std::optional<std::int64_t> running;
#endif
};

async::task<> Server::Client::answer(async::pipe pipe) {
    kotatsu::ipc::BincodePeer peer(
        async::event_loop::current(),
        std::make_unique<kotatsu::ipc::StreamTransport>(std::move(pipe)));

    peer.on_request(MakeDecision::method,
                    [&](const Context&,
                        MakeDecision::Params params) -> kotatsu::ipc::RequestResult<MakeDecision> {
                        co_return co_await decide(std::move(params));
                    });

    peer.on_request(
        ReportError::method,
        [&](const Context&,
            ReportError::Params params) -> kotatsu::ipc::RequestResult<ReportError> {
            co_await server.session.uncaptured(id, params.parent, std::move(params.message));
            co_return nullptr;
        });

    peer.on_request(
        Finish::method,
        [&](const Context&, const Finish::Params& params) -> kotatsu::ipc::RequestResult<Finish> {
#ifdef CATTER_MAC
            running.reset();
#endif
            const chrono::steady_clock::time_point started{chrono::microseconds(params.started)};
            co_await server.session.exited(id,
                                           {
                                               .code = params.code,
                                               .start = server.session.time_of(started).count(),
                                               .duration = params.duration,
                                           });
            co_return nullptr;
        });

#ifdef CATTER_WINDOWS
    peer.on_request(
        Started::method,
        [&](const Context&, const Started::Params& params) -> kotatsu::ipc::RequestResult<Started> {
            co_return co_await started(params.pid);
        });
#elif defined(CATTER_MAC)
    peer.on_notification(Started::method,
                         [&](const Started::Params& params) { running = params.pid; });
#endif

    co_await peer.run();
#ifdef CATTER_WINDOWS
    co_await wait_started();
#elif defined(CATTER_MAC)
    if(running) {
        LOG_WARN("The proxy of program {} is gone: the program is killed", *running);
        ::kill(static_cast<pid_t>(*running), SIGKILL);
    }
#endif
}

async::task<MakeDecision::Result> Server::Client::decide(MakeDecision::Params params) {
#ifdef CATTER_WINDOWS
    co_await server.ended();
    parent = params.parent;
#endif
    auto action = co_await server.session.decide(id, params.parent, std::move(params.command));
#ifdef CATTER_WINDOWS
    decided = action.type;
    executable = action.cmd.executable;
#endif
    co_return MakeDecision::Result{.id = id, .action = std::move(action)};
}

#ifdef CATTER_WINDOWS

async::task<bool> Server::Client::started(std::int64_t pid) {
    std::string error;
    try {
        // What injecting into it takes, besides waiting for it and ending it.
        auto opened = async::process::open(static_cast<int>(pid),
                                           PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                               PROCESS_VM_READ | PROCESS_VM_WRITE);
        if(!opened) {
            throw cpptrace::runtime_error(
                std::format("Failed to open process {}: {}", pid, opened.error().message()));
        }
        program.emplace(std::move(*opened));
        began = chrono::steady_clock::now();
        switch(decided) {
            case data::action::RUN:
                inject_hook(program->native_handle(), executable, id, server.name);
                break;
            case data::action::DROP: end(0); break;
            case data::action::FAIL: end(1); break;
        }
        untold =
            server.untold.insert(server.untold.end(),
                                 {.program = &*program, .told = std::make_shared<async::event>()});
        co_return true;
    } catch(const cpptrace::exception& ex) {
        error = ex.message();
        LOG_ERROR("{}\nStack trace:\n{}", ex.message(), ex.trace().to_string());
    }
    program.reset();
    co_await server.session.uncaptured(
        id,
        parent,
        std::format("`{}` cannot start with the hook: {}", executable, error));
    co_return false;
}

void Server::Client::end(std::uint32_t code) {
    if(!inject_end(program->native_handle(), executable, code)) {
        TerminateProcess(program->native_handle(), code);
    }
}

async::task<> Server::Client::wait_started() {
    if(!program) {
        co_return;
    }
    auto status = co_await program->wait();
    if(!status) {
        server.session.fail(std::make_exception_ptr(cpptrace::runtime_error(
            std::format("Failed to wait for `{}`: {}", executable, status.error().message()))));
        co_return;
    }
    auto result = result_of(*status, began);
    result.start = server.session.time_of(began).count();
    co_await server.session.exited(id, result);
    release();
    program.reset();
}

void Server::Client::release() {
    if(untold) {
        (*untold)->told->set();
        server.untold.erase(*untold);
        untold.reset();
    }
}

async::task<> Server::ended() {
    std::vector<std::shared_ptr<async::event>> ended;
    for(const auto& entry: untold) {
        if(WaitForSingleObject(entry.program->native_handle(), 0) == WAIT_OBJECT_0) {
            ended.push_back(entry.told);
        }
    }
    for(const auto& told: ended) {
        co_await told->wait();
    }
}

#endif

Server::Server(Session& session) :
    session(session),
#ifdef CATTER_WINDOWS
    name(std::format(R"(\\.\pipe\catter-ipc-{})", sys::pid()))
#else
    name(data_path(std::format("ipc-{}.sock", sys::pid())))
#endif
{
}

async::task<async::process::exit_status> Server::serve(Start start) {
#ifndef CATTER_WINDOWS
    // A socket a session of the same pid left behind.
    fs::remove(name);
#endif
    auto listened = async::pipe::listen(name, async::pipe::options(), async::event_loop::current());
    if(!listened) {
        throw cpptrace::runtime_error(
            std::format("Failed to listen on `{}` for the build's commands: {}",
                        name,
                        listened.error().message()));
    }
    acceptor.emplace(std::move(*listened));

    auto build = [](Server& self, Start start) -> async::task<async::process::exit_status> {
        // Once the build has ended, or failed to start, no command comes.
        kotatsu::ScopeExit stop([&] {
            if(auto error = self.acceptor->stop()) {
                LOG_ERROR("Failed to stop accepting clients: {}", error.message());
            }
        });
        co_return co_await start();
    }(*this, std::move(start));

    auto [_, status] = co_await async::when_all{accept(), std::move(build)};
#ifndef CATTER_WINDOWS
    fs::remove(name);
#endif
    co_return status;
}

async::task<> Server::accept() {
    async::task_group<> clients;
    while(true) {
        auto pipe = co_await acceptor->accept();
        if(!pipe) {
            assert(pipe.error() == async::error::operation_aborted);
            break;
        }
        const auto id = session.new_id();
        clients.spawn(answer(id, std::move(*pipe)));
        LOG_INFO("Accepted new client with id: {}", id);
    }
    // A client still connected once the build has ended, as a build server's,
    // is let go: cancelled, and gone before the server, which it uses.
    clients.cancel();
    co_await clients.join();
}

async::task<> Server::answer(data::ipcid_t id, async::pipe pipe) {
    try {
        Client client(*this, id);
        co_await client.answer(std::move(pipe));
    } catch(...) {
        session.fail(std::current_exception());
    }
}

}  // namespace catter::ipc
