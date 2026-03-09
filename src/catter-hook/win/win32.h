
#pragma once
#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

#include <windows.h>
#include <libloaderapi.h>
#include <minwindef.h>
#include <Psapi.h>

namespace catter::win {
class Handle {
public:
    Handle(HANDLE handle) : h(handle) {}

    Handle(const Handle&) = delete;

    Handle(Handle&& other) noexcept : h(std::exchange(other.h, INVALID_HANDLE_VALUE)) {}

    Handle& operator= (const Handle&) = delete;

    Handle& operator= (Handle&& other) noexcept {
        if(this != &other) {
            this->~Handle();
            new (this) Handle(std::move(other));
        }
        return *this;
    }

    ~Handle() {
        if(h != INVALID_HANDLE_VALUE && h != NULL) {
            CloseHandle(h);
        }
    }

    operator HANDLE() const {
        return h;
    }

    HANDLE get() const {
        return h;
    }

private:
    HANDLE h;
};

template <typename Invokable>
class Gaurd {
public:
    explicit Gaurd(Invokable invokable) : invokable(std::move(invokable)) {}

    Gaurd(const Gaurd&) = delete;
    Gaurd(Gaurd&&) = delete;
    Gaurd& operator= (const Gaurd&) = delete;
    Gaurd& operator= (Gaurd&&) = delete;

    ~Gaurd() {
        invokable();
    }

private:
    Invokable invokable;
};

template <typename Invokable, typename R = std::remove_reference_t<Invokable>>
    requires std::is_invocable_v<Invokable> && std::is_nothrow_invocable_v<Invokable>
Gaurd<R> make_gaurd(Invokable&& invokable) {
    return Gaurd<R>(std::forward<Invokable>(invokable));
}

template <typename F>
    requires std::is_function_v<F>
F* get_function_from_ntdll(const char* name) {
    HMODULE hNtDllModule = GetModuleHandleA("ntdll.dll");
    if(hNtDllModule == NULL) {
        throw std::system_error(
            GetLastError(),
            std::system_category(),
            std::format("Failed to get handle of ntdll.dll when looking for function {}", name));
    }

    auto* fn = reinterpret_cast<F*>(GetProcAddress(hNtDllModule, name));
    if(fn == nullptr) {
        throw std::system_error(ERROR_PROC_NOT_FOUND,
                                std::system_category(),
                                std::format("Failed to find {} in ntdll.dll", name));
    }
    return fn;
}

inline void wait_for_object(HANDLE handle, std::string_view action) {
    const auto wait_result = WaitForSingleObject(handle, INFINITE);
    if(wait_result == WAIT_OBJECT_0) {
        return;
    }
    if(wait_result == WAIT_FAILED) {
        throw std::system_error(GetLastError(), std::system_category(), std::string(action));
    }
    throw std::runtime_error(std::string(action));
}

inline std::string quote_win32_arg(std::string_view arg) noexcept {
    // No quoting needed if it's empty or has no special characters.
    if(arg.empty() || arg.find_first_of(" \t\n\v\"") == std::string_view::npos) {
        return std::string(arg);
    }

    std::string quoted_arg;
    quoted_arg.push_back('"');

    for(auto it = arg.begin();; ++it) {
        int num_backslashes = 0;
        while(it != arg.end() && *it == '\\') {
            ++it;
            ++num_backslashes;
        }

        if(it == arg.end()) {
            // End of string; append backslashes and a closing quote.
            quoted_arg.append(num_backslashes * 2, '\\');
            break;
        }

        if(*it == '"') {
            // Escape all backslashes and the following double quote.
            quoted_arg.append(num_backslashes * 2 + 1, '\\');
            quoted_arg.push_back(*it);
        } else {
            // Backslashes aren't special here.
            quoted_arg.append(num_backslashes, '\\');
            quoted_arg.push_back(*it);
        }
    }
    quoted_arg.push_back('"');
    return quoted_arg;
}
}  // namespace catter::win
