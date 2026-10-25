#pragma once

#include <exception>
#include <string>

namespace catter {

class PayloadError : public std::exception {
public:
    PayloadError(int code, std::string message) : m_code(code), m_message(std::move(message)) {}

    [[nodiscard]]
    int code() const noexcept {
        return m_code;
    }

    [[nodiscard]]
    const char* what() const noexcept override {
        return m_message.c_str();
    }

private:
    int m_code;
    std::string m_message;
};

}  // namespace catter
