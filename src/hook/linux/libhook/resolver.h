#pragma once

#include <climits>
#include <expected>
#include <linux/limits.h>
#include <string_view>

namespace catter {

/**
 * This class implements the logic how the program execution resolves the
 * executable path from the system environment.
 *
 * The resolution logic implemented as a class to be able to unit test
 * the code and avoid memory allocation.
 */
class Resolver {
public:
    Resolver() noexcept;
    virtual ~Resolver() noexcept = default;

    /**
     * Resolve the executable from system environments.
     *
     * @return resolved executable path as absolute path.
     */
    [[nodiscard]]
    virtual std::expected<const char*, int> from_current_directory(const std::string_view& file);

    [[nodiscard]]
    virtual std::expected<const char*, int> from_path(const std::string_view& file,
                                                      const char** envp);

    [[nodiscard]]
    virtual std::expected<const char*, int> from_search_path(const std::string_view& file,
                                                             const char* search_path);

    Resolver(const Resolver&) = delete;
    Resolver& operator= (const Resolver&) = delete;
    Resolver(Resolver&&) = delete;
    Resolver& operator= (Resolver&&) = delete;

private:
    char result_[PATH_MAX];
};
}  // namespace catter
