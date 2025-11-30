#include "command.h"
#include "buffer.h"
#include "libhook/linux-mac/debug.h"
#include "libconfig/linux-mac-hook.h"

namespace catter {

const char* CmdBuilder::store_arg(Buffer& buf, const char* str) noexcept {
    if(append_argv_ptr >= argv + MAX_ARGC - 1)  // reserve null
        return nullptr;

    const char* stored = buf.store(str);
    if(!stored)
        return nullptr;

    *append_argv_ptr++ = const_cast<char*>(stored);
    return stored;
}

CmdBuilder::CmdBuilder(const char* proxy_path, const char* self_id) noexcept {

    append_ptr = cmd_buf_area;
    append_argv_ptr = argv;

    Buffer buf(cmd_buf_area, cmd_buf_area + BUF_SIZE);

    // at lease we provide a space enough for proxy_path, -p, self_id, "", and null
    // therefore we do not check the return value here
    executable_path_ptr = store_arg(buf, proxy_path);
    store_arg(buf, "-p");
    store_arg(buf, self_id);

    append_ptr = const_cast<char*>(buf.store(""));
}

CmdBuilder::command CmdBuilder::proxy_str(const char* path, char* const* argv_in) noexcept {

    auto argv_backup = append_argv_ptr;
    auto ptr_backup = append_ptr;

    Buffer buf(append_ptr, cmd_buf_area + BUF_SIZE);

    if(!store_arg(buf, "--") || !store_arg(buf, path)) {
        WARN("Overflow when building proxy command");
        goto rollback;
    }

    for(unsigned i = 1; argv_in[i]; ++i)  // argv_in[0] is the path
        if(!store_arg(buf, argv_in[i])) {
            WARN("Overflow when building proxy command args");
            goto rollback;
        }

    append_argv_ptr = argv_backup;
    append_ptr = ptr_backup;
    INFO("Built proxy command:");
    for(auto it = this->argv; *it != nullptr; ++it) {
        INFO("arg: {}", *it);
    }
    return {executable_path_ptr, this->argv};

rollback:
    append_argv_ptr = argv_backup;
    append_ptr = ptr_backup;
    return error_str("Overflow when building command", path);
}

CmdBuilder::command CmdBuilder::error_str(const char* msg,
                                          const char* path,
                                          char* const* argv_in) noexcept {

    auto* argv_backup = append_argv_ptr;
    char* ptr_backup = append_ptr;

    Buffer buf(append_ptr, cmd_buf_area + BUF_SIZE);

    buf.push(catter::config::hook::ERROR_PREFIX);
    buf.push(" ");
    buf.push(msg);
    buf.push(" in executing:\n    --->");
    buf.push(path);

    if(argv_in != nullptr) {
        for(unsigned i = 1; argv_in[i]; ++i) {
            buf.push(" ");
            buf.push(argv_in[i]);
        }
    }
    auto res = buf.store("");

    if(!res) {
        ERROR("failed to build error string: buffer overflow");
        append_argv_ptr = argv_backup;
        append_ptr = ptr_backup;
        return {path, argv_in};
    }

    *append_argv_ptr++ = append_ptr;

    append_argv_ptr = argv_backup;
    append_ptr = ptr_backup;
    ERROR("{}", append_ptr);
    return {executable_path_ptr, this->argv};
}

}  // namespace catter
