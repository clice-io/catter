
#pragma once

#include <system_error>
#include <cpptrace/exceptions.hpp>

namespace catter {
class system_error : public cpptrace::runtime_error {
public:
    system_error(std::error_code __ec = std::error_code()) :
        runtime_error(__ec.message()), ec(__ec) {}

    system_error(std::error_code __ec, const std::string& __what) :
        runtime_error(__what + (": " + __ec.message())), ec(__ec) {}

    system_error(std::error_code __ec, const char* __what) :
        runtime_error(__what + (": " + __ec.message())), ec(__ec) {}

    system_error(int __v, const std::error_category& __ecat, const char* __what) :
        system_error(std::error_code(__v, __ecat), __what) {}

    system_error(int __v, const std::error_category& __ecat) :
        runtime_error(std::error_code(__v, __ecat).message()), ec(__v, __ecat) {}

    system_error(int __v, const std::error_category& __ecat, const std::string& __what) :
        runtime_error(__what + (": " + std::error_code(__v, __ecat).message())), ec(__v, __ecat) {}

    system_error(const system_error&) = default;
    system_error& operator= (const system_error&) = default;

    const std::error_code& code() const noexcept {
        return this->ec;
    }

private:
    std::error_code ec;
};
}  // namespace catter
