#include "js_case.h"

#if defined(CATTER_LINUX) || defined(CATTER_MAC)
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <format>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif
#include <kota/zest/macro.h>
#include <kota/zest/zest.h>

namespace {

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
            "x-catter-test: again\r\n"
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

}  // namespace

TEST_SUITE(js_unit_tests) {
TEST_CASE(run_http_client_js_file_through_async_loop) {
#if defined(CATTER_LINUX) || defined(CATTER_MAC)
    auto f = [&]() {
        LocalHttpServer server{2};
        auto source = std::string{R"JS(
            import { assertThrow } from "catter/debug";
            import { Client, post } from "catter/http";

            const base = "__BASE_URL__";
            const client = new Client();
            const res = await client.get(`${base}/payload`, {
              headers: [["X-From-JS", "yes"]],
              timeoutMs: 5_000,
            });

            assertThrow(res.ok);
            assertThrow(res.status === 200);
            assertThrow(res.header("content-type") === "application/json");
            assertThrow(res.header("x-catter-test") === "yes, again");
            assertThrow(!("rawHeaders" in res));
            assertThrow(res.json().ok === true);
            assertThrow(res.json().path === "/payload");

            const echoed = await post(`${base}/echo`, "hello async http", {
              timeoutMs: 5_000,
            });
            assertThrow(echoed.text() === "POST /echo hello async http");

            client.close();
        )JS"};
        const auto base_url = std::format("http://127.0.0.1:{}", server.port());
        source.replace(source.find("__BASE_URL__"),
                       std::string_view{"__BASE_URL__"}.size(),
                       base_url);

        catter::tests::js::run_async_js_case(std::move(source), "http-client-test.js");
    };

    EXPECT_NOTHROWS(f());
#else
    EXPECT_TRUE(true);
#endif
};
};  // TEST_SUITE(js_unit_tests)
