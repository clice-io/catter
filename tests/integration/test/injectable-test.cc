// UNSUPPORTED: system-windows
// RUN: %cc
// RUN: %inject-hook11 %t x execve | %filecheck %s --check-prefix=EXECVE -DEXE_PATH=%t
// RUN: %inject-hook12 %t x execv | %filecheck %s --check-prefix=EXECV -DEXE_PATH=%t
// RUN: %inject-hook13 %t x execvp | %filecheck %s --check-prefix=EXECVP -DEXE_PATH=%t
// RUN: %inject-hook14 %t x execl | %filecheck %s --check-prefix=EXECL -DEXE_PATH=%t
// RUN: %inject-hook15 %t x posix_spawn | %filecheck %s --check-prefix=SPAWN -DEXE_PATH=%t
// RUN: %inject-hook16 %t x posix_spawnp | %filecheck %s --check-prefix=SPAWNP -DEXE_PATH=%t

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <spawn.h>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char** environ;
using Argv_t = char* const*;

template <typename... Args>
auto argvify(Args&&... args) {
    return std::vector<char*>{const_cast<char*>(args)..., nullptr};
}

std::filesystem::path exec_path = "%t";
auto exec_name = exec_path.filename().string();

namespace {

void print_args(int argc, const char** argv) {
    for(int i = 1; i < argc; ++i) {
        std::printf("%s ", argv[i]);
    }
    std::printf("\n");
}

int run_execve() {
    auto argv = argvify(exec_name.c_str(), "-a");
    // test
    for(size_t i = 0; argv[i] != nullptr; ++i) {
        std::printf("argv[%zu]: %s\n", i, argv[i]);
    }
    ::execve(exec_path.c_str(), argv.data(), environ);
    std::perror("execve");
    return 1;
}

int run_execv() {
    auto argv = argvify(exec_name.c_str(), "-a");
    ::execv(exec_path.c_str(), argv.data());
    std::perror("execv");
    return 1;
}

int run_execvp() {
    auto argv = argvify(exec_name.c_str(), "-a");
    ::execvp(exec_name.c_str(), argv.data());
    std::perror("execvp");
    return 1;
}

int run_execl() {
    ::execl(exec_path.c_str(), exec_name.c_str(), "-a", static_cast<char*>(nullptr));
    std::perror("execl");
    return 1;
}

int run_posix_spawn(bool use_path) {
    auto argv = argvify(exec_name.c_str(), "-a");
    pid_t pid = 0;
    int res = 0;
    if(use_path) {
        res = ::posix_spawnp(&pid, exec_name.c_str(), nullptr, nullptr, argv.data(), environ);
    } else {
        res = ::posix_spawn(&pid, exec_path.c_str(), nullptr, nullptr, argv.data(), environ);
    }
    if(res != 0) {
        std::fprintf(stderr, "posix_spawn failed: %s\n", std::strerror(res));
        return 1;
    }
    int status = 0;
    if(::waitpid(pid, &status, 0) < 0) {
        std::perror("waitpid");
        return 1;
    }
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return 1;
    }
    return 0;
}

}  // namespace

using namespace std::string_view_literals;

int main(int argc, const char** argv) {
    if(argc == 1) {
        printf("%t\n");
        return -1;
    }
    // call by hook proxy
    if(argv[1][0] != 'x') {
        print_args(argc, argv);
        return 0;
    }

    if(argc < 3) {
        return 2;
    }
    exec_path = argv[0];
    exec_name = exec_path.filename().string();

    // add path
    auto exec_dir = exec_path.parent_path();
    std::string path_env = exec_dir.string();
    if(const char* raw_path = std::getenv("PATH"); raw_path != nullptr && raw_path[0] != '\0') {
        path_env += ":";
        path_env += raw_path;
    }
    setenv("PATH", path_env.c_str(), 1);
    if(argv[2] == "execve"sv) {
        return run_execve();
    }
    if(argv[2] == "execv"sv) {
        return run_execv();
    }
    if(argv[2] == "execvp"sv) {
        return run_execvp();
    }
    if(argv[2] == "execl"sv) {
        return run_execl();
    }
    if(argv[2] == "posix_spawn"sv) {
        return run_posix_spawn(false);
    }
    if(argv[2] == "posix_spawnp"sv) {
        return run_posix_spawn(true);
    }
    return 2;
}

// EXECVE: -p 11 --exec [[EXE_PATH]] -- {{.*}} -a
// EXECV:  -p 12 --exec [[EXE_PATH]] -- {{.*}} -a
// EXECVP: -p 13 --exec [[EXE_PATH]] -- {{.*}} -a
// EXECL:  -p 14 --exec [[EXE_PATH]] -- {{.*}} -a
// SPAWN:  -p 15 --exec [[EXE_PATH]] -- {{.*}} -a
// SPAWNP: -p 16 --exec [[EXE_PATH]] -- {{.*}} -a
