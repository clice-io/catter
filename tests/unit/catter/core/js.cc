#include <algorithm>

#include "js/async.h"
#include "js/sync.h"
#if defined(CATTER_LINUX) || defined(CATTER_MAC)
#include <cctype>
#include <cerrno>
#include <cstring>
#include <thread>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <cstdio>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cpptrace/exceptions.hpp>
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

#include "temp_file_manager.h"
#include "config/js-test.h"
#include "util/output.h"

namespace fs = std::filesystem;
using namespace catter;
using namespace catter::js;

namespace {

void ensure_qjs_initialized(const fs::path& js_path) {
    static bool initialized = false;
    if(!initialized) {
        js::init_qjs({.pwd = js_path});
        initialized = true;
    }
}

void run_js_file_by_name(const fs::path& js_path, std::string_view file_name) {
    auto full_path = js_path / file_name;

    std::ifstream ifs{full_path};
    if(!ifs.good()) {
        throw cpptrace::runtime_error("js test file cannot be opened: " + full_path.string());
    }

    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    js::run_js_file(content, full_path.string());
}

void run_basic_js_case(std::string_view file_name, bool with_fs_test_env = false) {
    try {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);

        if(with_fs_test_env) {
            auto js_path_res = fs::path(config::data::js_test_res_path.data());
            TempFileManager manager(js_path_res / "fs-test-env");

            std::error_code ec;
            manager.create("a/tmp.txt", ec, "Alpha!\nBeta!\nKid A;\nend;");
            if(ec) {
                throw cpptrace::runtime_error("failed to prepare fs test file: a/tmp.txt");
            }
            manager.create("b/tmp2.txt", ec, "Ok computer!\n");
            if(ec) {
                throw cpptrace::runtime_error("failed to prepare fs test file: b/tmp2.txt");
            }
            manager.create("c/a.txt", ec);
            if(ec) {
                throw cpptrace::runtime_error("failed to prepare fs test file: c/a.txt");
            }
            manager.create("c/b.txt", ec);
            if(ec) {
                throw cpptrace::runtime_error("failed to prepare fs test file: c/b.txt");
            }

            run_js_file_by_name(js_path, file_name);
            return;
        }

        run_js_file_by_name(js_path, file_name);
    } catch(qjs::Exception& ex) {
        output::redLn("{}", ex.what());
        throw ex;
    }
}

kota::task<> run_async_js_source(std::string source, std::string file_name) {
    auto js_path = fs::path(config::data::js_test_path.data());
    js::JsLoop js_loop;
    js::JsLoopScope js_loop_scope(js_loop);

    co_await js::async_init_qjs({.pwd = js_path});
    co_await js::async_run_js_file(source, std::move(file_name));
}

void run_async_js_case(std::string source, std::string file_name) {
    auto task = run_async_js_source(std::move(source), std::move(file_name));
    kota::event_loop loop;
    loop.schedule(task);
    loop.run();
    task.result();
}

#if defined(CATTER_LINUX) || defined(CATTER_MAC)
class LocalHttpServer {
public:
    explicit LocalHttpServer(int expected_requests) : expected_requests(expected_requests) {
        listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);
        if(listen_fd < 0) {
            throw_errno("socket");
        }

        int enabled = 1;
        ::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;

        if(::bind(listen_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw_errno("bind");
        }

        if(::listen(listen_fd, expected_requests) < 0) {
            throw_errno("listen");
        }

        socklen_t addr_len = sizeof(addr);
        if(::getsockname(listen_fd, reinterpret_cast<sockaddr*>(&addr), &addr_len) < 0) {
            throw_errno("getsockname");
        }
        listen_port = ntohs(addr.sin_port);

        worker = std::jthread([this, fd = listen_fd](std::stop_token stop) { serve(fd, stop); });
    }

    ~LocalHttpServer() {
        close_listen_socket();
    }

    uint16_t port() const noexcept {
        return listen_port;
    }

private:
    struct Socket {
        int fd = -1;

        explicit Socket(int fd) noexcept : fd(fd) {}

        Socket(const Socket&) = delete;
        Socket& operator= (const Socket&) = delete;

        ~Socket() {
            if(fd >= 0) {
                ::close(fd);
            }
        }
    };

    static void throw_errno(std::string_view action) {
        throw std::runtime_error(std::format("{} failed: {}", action, std::strerror(errno)));
    }

    void close_listen_socket() noexcept {
        if(listen_fd >= 0) {
            ::shutdown(listen_fd, SHUT_RDWR);
            ::close(listen_fd);
            listen_fd = -1;
        }
    }

    void serve(int fd, std::stop_token stop) noexcept {
        for(int handled = 0; handled < expected_requests && !stop.stop_requested();) {
            int client_fd = ::accept(fd, nullptr, nullptr);
            if(client_fd < 0) {
                if(errno == EINTR) {
                    continue;
                }
                return;
            }

            ++handled;
            handle_client(client_fd);
        }
    }

    static void handle_client(int client_fd) noexcept {
        Socket client{client_fd};
        const auto request = read_request(client.fd);
        const auto [method, path] = request_line(request);
        const auto body = request_body(request);

        int status = 200;
        std::string reason = "OK";
        std::string content_type = "text/plain";
        std::string response_body;

        if(method == "GET" && path == "/payload") {
            content_type = "application/json";
            response_body = R"({"ok":true,"path":"/payload"})";
        } else if(method == "POST" && path == "/echo") {
            response_body = method + " " + path + " " + body;
        } else {
            status = 404;
            reason = "Not Found";
            response_body = "not found";
        }

        const auto response = std::format(
            "HTTP/1.1 {} {}\r\n"
            "Content-Type: {}\r\n"
            "X-Catter-Test: yes\r\n"
            "Connection: close\r\n"
            "Content-Length: {}\r\n"
            "\r\n"
            "{}",
            status,
            reason,
            content_type,
            response_body.size(),
            response_body);
        send_all(client.fd, response);
    }

    static std::string read_request(int fd) {
        std::string request;
        char buffer[4096]{};

        while(request.find("\r\n\r\n") == std::string::npos) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if(n <= 0) {
                return request;
            }
            request.append(buffer, static_cast<std::size_t>(n));
        }

        const auto header_end = request.find("\r\n\r\n");
        const auto body_start = header_end + 4;
        const auto content_length = request_content_length(request);
        while(request.size() < body_start + content_length) {
            auto n = ::recv(fd, buffer, sizeof(buffer), 0);
            if(n <= 0) {
                break;
            }
            request.append(buffer, static_cast<std::size_t>(n));
        }

        return request;
    }

    static std::pair<std::string, std::string> request_line(std::string_view request) {
        const auto line_end = request.find("\r\n");
        std::string line{request.substr(0, line_end)};
        std::istringstream stream{line};
        std::string method;
        std::string path;
        stream >> method >> path;
        return {method, path};
    }

    static std::size_t request_content_length(std::string_view request) {
        const auto header_end = request.find("\r\n\r\n");
        if(header_end == std::string_view::npos) {
            return 0;
        }

        std::size_t pos = 0;
        while(pos < header_end) {
            const auto next = request.find("\r\n", pos);
            const auto line_end = next == std::string_view::npos ? header_end : next;
            const auto line = request.substr(pos, line_end - pos);
            const auto colon = line.find(':');
            if(colon != std::string_view::npos) {
                auto name = std::string{line.substr(0, colon)};
                std::ranges::transform(name, name.begin(), [](unsigned char ch) {
                    return static_cast<char>(std::tolower(ch));
                });
                if(name == "content-length") {
                    auto value = std::string{line.substr(colon + 1)};
                    return static_cast<std::size_t>(std::stoull(value));
                }
            }
            pos = line_end + 2;
        }

        return 0;
    }

    static std::string request_body(std::string_view request) {
        const auto header_end = request.find("\r\n\r\n");
        if(header_end == std::string_view::npos) {
            return {};
        }

        const auto body_start = header_end + 4;
        const auto content_length = request_content_length(request);
        const auto available = request.size() - body_start;
        return std::string{request.substr(body_start, std::min(content_length, available))};
    }

    static void send_all(int fd, std::string_view data) noexcept {
#ifdef MSG_NOSIGNAL
        constexpr int send_flags = MSG_NOSIGNAL;
#else
        constexpr int send_flags = 0;
#endif
        std::size_t offset = 0;
        while(offset < data.size()) {
            auto n = ::send(fd, data.data() + offset, data.size() - offset, send_flags);
            if(n < 0 && errno == EINTR) {
                continue;
            }
            if(n <= 0) {
                return;
            }
            offset += static_cast<std::size_t>(n);
        }
    }

    int listen_fd = -1;
    uint16_t listen_port = 0;
    int expected_requests = 0;
    std::jthread worker;
};
#endif

bool auto_js_case_uses_fs_test_env(const fs::path& relative_path) {
    return relative_path.filename() == "fs.js";
}

std::vector<fs::path> collect_auto_js_case_paths(const fs::path& js_path) {
    std::vector<fs::path> paths;
    const auto auto_path = js_path / "auto";
    if(!fs::exists(auto_path)) {
        return paths;
    }

    for(const auto& entry: fs::recursive_directory_iterator(auto_path)) {
        if(!entry.is_regular_file() || entry.path().extension() != ".js") {
            continue;
        }
        paths.push_back(entry.path().lexically_relative(js_path));
    }

    std::sort(paths.begin(), paths.end());
    return paths;
}

std::string auto_js_case_name(const fs::path& relative_path) {
    auto name = relative_path.lexically_relative("auto");
    name.replace_extension();
    return name.generic_string();
}

void run_auto_js_case(const fs::path& relative_path) {
    run_basic_js_case(relative_path.generic_string(), auto_js_case_uses_fs_test_env(relative_path));
}

kota::zest::TestState run_auto_js_test_case(const fs::path& relative_path) {
    try {
        run_auto_js_case(relative_path);
        return kota::zest::TestState::Passed;
    } catch(const std::exception& ex) {
        output::redLn("auto js test failed: {}: {}", relative_path.string(), ex.what());
        return kota::zest::TestState::Failed;
    } catch(...) {
        output::redLn("auto js test failed: {}: unknown exception", relative_path.string());
        return kota::zest::TestState::Fatal;
    }
}

std::vector<kota::zest::TestCase> auto_js_test_cases() {
    std::vector<kota::zest::TestCase> cases;
    const auto js_path = fs::path(config::data::js_test_path.data());

    for(const auto& relative_path: collect_auto_js_case_paths(js_path)) {
        const auto full_path = (js_path / relative_path).string();
        const auto case_name = auto_js_case_name(relative_path);
        cases.emplace_back(kota::zest::TestCase{
            .name = case_name,
            .path = full_path,
            .line = 1,
            .attrs = {},
            .test = [relative_path] { return run_auto_js_test_case(relative_path); },
        });
    }

    return cases;
}

}  // namespace

TEST_SUITE(js_tests) {
TEST_CASE(run_service_js_file_and_callbacks) {
    auto f = [&]() {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);
        run_js_file_by_name(js_path, "service.js");

        js::CatterRuntime runtime{
            .supportActions = {js::ActionType::skip,
                               js::ActionType::drop,
                               js::ActionType::abort,
                               js::ActionType::modify},
            .type = js::CatterRuntime::Type::inject,
            .supportParentId = true,
        };

        js::CatterConfig config{
            .scriptPath = "script.ts",
            .scriptArgs = {"--input", "compile_commands.json"},
            .buildSystemCommand = {"xmake", "build"},
            .runtime = runtime,
            .options = {.log = true},
            .execute = true,
        };

        auto updated_config = js::on_start(config);
        EXPECT_TRUE(updated_config.scriptPath == config.scriptPath);
        EXPECT_TRUE(updated_config.scriptArgs.size() == 3);
        EXPECT_TRUE(updated_config.scriptArgs.back() == "--from-service");
        EXPECT_TRUE(updated_config.options.log == false);
        EXPECT_TRUE(updated_config.execute == true);

        js::CommandData data{
            .cwd = "/tmp",
            .exe = "clang++",
            .argv = {"clang++", "main.cc", "-c"},
            .env = {"CC=clang++", "CATTER_LOG=1"},
            .runtime = runtime,
            .parent = 41,
        };

        auto action = js::on_command(7, data);
        action.visit([&]<auto E>(const Tag<E>& tag) {
            if constexpr(E == js::ActionType::modify) {
                EXPECT_TRUE(tag.data.argv.size() == 4);
                EXPECT_TRUE(tag.data.argv.back() == "--from-service");
                EXPECT_TRUE(tag.data.parent.has_value());
                EXPECT_TRUE(tag.data.parent.value() == 41);
            } else {
                EXPECT_TRUE(E == js::ActionType::modify);
            }
        });

        js::CatterErr err{.msg = "spawn failed"};
        auto error_action = js::on_command(7, std::unexpected(err));
        EXPECT_TRUE(error_action.type() == js::ActionType::skip);

        js::ProcessResult execution_result{
            .code = 0,
            .stdOut = "hello from stdout",
            .stdErr = "hello from stderr",
        };
        js::on_execution(7, execution_result);

        js::ProcessResult finish_result{
            .code = 0,
        };
        js::on_finish(finish_result);
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(run_cdb_js_file) {
    auto f = [&]() {
        run_basic_js_case("cdb.js");
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(run_js_file_reports_async_error_message_and_stack) {
    auto f = [&]() {
        auto js_path = fs::path(config::data::js_test_path.data());
        ensure_qjs_initialized(js_path);

        bool caught = false;
        try {
            js::run_js_file("await Promise.reject(new Error('async boom'));\n", "reject.js");
        } catch(const qjs::Exception& ex) {
            caught = true;
            std::string message = ex.what();
            EXPECT_TRUE(message.contains("async boom"));
            EXPECT_TRUE(message.contains("Stack Trace:"));
            EXPECT_TRUE(message.contains("reject.js"));
        }

        EXPECT_TRUE(caught);
    };

    EXPECT_NOTHROWS(f());
};

TEST_CASE(run_http_client_js_file_through_async_loop) {
#if defined(CATTER_LINUX) || defined(CATTER_MAC)
    auto f = [&]() {
        LocalHttpServer server{2};
        auto source = std::string{R"JS(
            import { debug, http } from "catter";

            const base = "__BASE_URL__";
            const client = new http.Client();
            const res = await client.get(`${base}/payload`, {
              headers: [["X-From-JS", "yes"]],
              timeoutMs: 5_000,
            });

            debug.assertThrow(res.ok);
            debug.assertThrow(res.status === 200);
            debug.assertThrow(res.header("content-type") === "application/json");
            debug.assertThrow(res.header("x-catter-test") === "yes");
            debug.assertThrow(res.json().ok === true);
            debug.assertThrow(res.json().path === "/payload");

            const echoed = await http.post(`${base}/echo`, "hello async http", {
              timeoutMs: 5_000,
            });
            debug.assertThrow(echoed.text() === "POST /echo hello async http");

            client.close();
        )JS"};
        const auto base_url = std::format("http://127.0.0.1:{}", server.port());
        source.replace(source.find("__BASE_URL__"),
                       std::string_view{"__BASE_URL__"}.size(),
                       base_url);

        run_async_js_case(std::move(source), "http-client-test.js");
    };

    EXPECT_NOTHROWS(f());
#else
    EXPECT_TRUE(true);
#endif
};
};  // TEST_SUITE(js_tests)

namespace {

const bool auto_js_tests_registered = [] {
    kota::zest::Runner::instance().add_suite("js_auto_tests", &auto_js_test_cases);
    return true;
}();

}  // namespace
