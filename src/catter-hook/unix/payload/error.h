#pragma once

#include <exception>
#include <string>

namespace catter {

class PayloadError : public std::exception {
public:
    PayloadError(int code, std::string message) : code_(code), message_(std::move(message)) {}

    [[nodiscard]]
    int code() const noexcept {
        return code_;
    }

    [[nodiscard]]
    const char* what() const noexcept override {
        return message_.c_str();
    }

private:
    int code_;
    std::string message_;
};

}  // namespace catter
