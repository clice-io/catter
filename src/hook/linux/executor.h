
#include "linker.h"
#include "resolver.h"
#include "session.h"
#include <expected>

namespace catter {

/**
 * This class implements the process execution logic.
 *
 * The caller of this is the POSIX interface for process creation.
 * This class encapsulate most of the logic and leave the C wrapper light
 * in order to test the functionality in unit tests.
 *
 * This is just a subset of all process creation calls.
 *
 * - Variable argument methods are not implemented. (The `execl*` methods.)
 *   Caller needs to convert those convenient functions by collecting the
 *   arguments into a C array.
 *
 * - Environment needs to pass for this methods. If a method does not have
 *   the environment explicitly passed as argument, it needs to grab it
 *   and pass to these methods.
 */
class Executor {
public:
    Executor(const Linker& linker, const Session& session_, Resolver& resolver) noexcept;

    ~Executor() noexcept = default;

public:
    std::expected<int, int> execve(const char* path, char* const argv[], char* const envp[]) const;

    std::expected<int, int> execvpe(const char* file, char* const argv[], char* const envp[]) const;

    std::expected<int, int> execvP(const char* file,
                                   const char* search_path,
                                   char* const argv[],
                                   char* const envp[]) const;

    std::expected<int, int> posix_spawn(pid_t* pid,
                                        const char* path,
                                        const posix_spawn_file_actions_t* file_actions,
                                        const posix_spawnattr_t* attrp,
                                        char* const argv[],
                                        char* const envp[]) const;

    std::expected<int, int> posix_spawnp(pid_t* pid,
                                         const char* file,
                                         const posix_spawn_file_actions_t* file_actions,
                                         const posix_spawnattr_t* attrp,
                                         char* const argv[],
                                         char* const envp[]) const;

private:
    const catter::Linker& linker_;
    const catter::Session& session_;
    catter::Resolver& resolver_;
};
}  // namespace catter
