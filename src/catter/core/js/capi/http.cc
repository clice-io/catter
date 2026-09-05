#include <chrono>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <kota/http/http.h>

#include "bridge.h"
#include "../apitool.h"

namespace qjs = catter::qjs;

namespace {

template <typename T>
using JsTask = kota::task<T, qjs::Error>;

struct HttpResponse {
    int32_t status;
    bool ok;
    std::string url;
    std::string body;
    qjs::Object headers;
};

int64_t http_client_id_cnt = 1;
std::unordered_map<int64_t, kota::http::client> http_clients;

kota::http::client& default_http_client() {
    static kota::http::client client;
    return client;
}

kota::http::client& client_by_id(int64_t client_id) {
    auto it = http_clients.find(client_id);
    if(it == http_clients.end()) {
        throw qjs::Exception("Invalid HTTP client id `{}`", client_id);
    }
    return it->second;
}

std::vector<kota::http::header> read_headers(const qjs::Object& flat_headers) {
    auto len = flat_headers["length"].as<uint32_t>();
    if(len % 2 != 0) {
        throw qjs::Exception("HTTP headers must be a flat [name, value, ...] string array.");
    }

    std::vector<kota::http::header> headers;
    headers.reserve(len / 2);
    for(uint32_t i = 0; i < len; i += 2) {
        headers.push_back({
            .name = flat_headers[std::to_string(i)].as<std::string>(),
            .value = flat_headers[std::to_string(i + 1)].as<std::string>(),
        });
    }
    return headers;
}

qjs::Object normalize_headers(JSContext* ctx, const std::vector<kota::http::header>& headers) {
    std::unordered_map<std::string, std::string> normalized;
    normalized.reserve(headers.size());

    for(const auto& header: headers) {
        auto name = header.name;
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return std::tolower(c);
        });

        if(auto [entry, inserted] = normalized.try_emplace(std::move(name), header.value);
           !inserted) {
            entry->second.append(", ").append(header.value);
        }
    }

    qjs::Object result = qjs::Object::empty_one(ctx);
    for(const auto& [name, value]: normalized) {
        result.set_property(name, value);
    }
    return result;
}

qjs::Object response_to_object(JSContext* ctx, const kota::http::response& response) {
    return catter::js::to_reflected_object(ctx,
                                           HttpResponse{
                                               .status = response.status,
                                               .ok = response.ok(),
                                               .url = response.url,
                                               .body = response.text_copy(),
                                               .headers = normalize_headers(ctx, response.headers),
                                           });
}

JsTask<qjs::Object> send_request(JSContext* ctx,
                                 kota::http::client& client,
                                 std::string method,
                                 std::string url,
                                 qjs::Object flat_headers,
                                 std::string body,
                                 int32_t timeout_ms,
                                 int32_t max_redirects,
                                 bool danger_accept_invalid_certs,
                                 bool danger_accept_invalid_hostnames,
                                 std::string proxy_url) {
    auto request =
        client.on(kota::event_loop::current()).request(std::move(method), std::move(url));
    for(auto& header: read_headers(flat_headers)) {
        request.header(std::move(header.name), std::move(header.value));
    }

    if(!body.empty()) {
        request.body(std::move(body));
    }

    if(timeout_ms >= 0) {
        request.timeout(std::chrono::milliseconds(timeout_ms));
    }

    if(max_redirects == 0) {
        request.redirect(kota::http::redirect_policy::none());
    } else if(max_redirects > 0) {
        request.redirect(
            kota::http::redirect_policy::limited(static_cast<std::size_t>(max_redirects)));
    }

    request.danger_accept_invalid_certs(danger_accept_invalid_certs);
    request.danger_accept_invalid_hostnames(danger_accept_invalid_hostnames);

    if(!proxy_url.empty()) {
        request.proxy(std::move(proxy_url));
    }

    auto response = co_await std::move(request).send();
    if(!response) {
        co_await kota::fail(
            qjs::Error::internal_error(ctx, "{}", kota::http::message(response.error())));
    }

    co_return response_to_object(ctx, *response);
}

CAPI(http_client_create, ()->int64_t) {
    auto id = http_client_id_cnt++;
    http_clients.emplace(id, kota::http::client{});
    return id;
}

CAPI(http_client_close, (int64_t client_id)->void) {
    auto erased = http_clients.erase(client_id);
    if(erased == 0) {
        throw qjs::Exception("Invalid HTTP client id: `{}`", client_id);
    }
}

CTX_ASYNC_CAPI(http_client_request,
               (JSContext * ctx,
                int64_t client_id,
                std::string method,
                std::string url,
                qjs::Object flat_headers,
                std::string body,
                int32_t timeout_ms,
                int32_t max_redirects,
                bool danger_accept_invalid_certs,
                bool danger_accept_invalid_hostnames,
                std::string proxy_url)
                   ->JsTask<qjs::Object>) {
    auto& client = client_by_id(client_id);
    co_return co_await send_request(ctx,
                                    client,
                                    std::move(method),
                                    std::move(url),
                                    std::move(flat_headers),
                                    std::move(body),
                                    timeout_ms,
                                    max_redirects,
                                    danger_accept_invalid_certs,
                                    danger_accept_invalid_hostnames,
                                    std::move(proxy_url));
}

CTX_ASYNC_CAPI(http_request,
               (JSContext * ctx,
                std::string method,
                std::string url,
                qjs::Object flat_headers,
                std::string body,
                int32_t timeout_ms,
                int32_t max_redirects,
                bool danger_accept_invalid_certs,
                bool danger_accept_invalid_hostnames,
                std::string proxy_url)
                   ->JsTask<qjs::Object>) {
    co_return co_await send_request(ctx,
                                    default_http_client(),
                                    std::move(method),
                                    std::move(url),
                                    std::move(flat_headers),
                                    std::move(body),
                                    timeout_ms,
                                    max_redirects,
                                    danger_accept_invalid_certs,
                                    danger_accept_invalid_hostnames,
                                    std::move(proxy_url));
}

}  // namespace
