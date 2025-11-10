#include "io.h"
#include "array.h"
#include "config.h"
#include "debug.h"
#include "crossplat.h"
#include <unistd.h>

#include <fcntl.h>
#include <sys/syscall.h>
#include <sys/stat.h>

namespace {
int push_num(int num, char*& it, const char* end) {
    auto prev_it = it;
    if(num <= 0)
        return -1;

    char buf[16];
    int len = 0;

    do {
        buf[len++] = static_cast<char>('0' + num % 10);
        num /= 10;
    } while(num > 0);

    for(int i = len - 1; i >= 0; --i) {
        if(it == end) {
            it = prev_it;
            return -1;
        }
        *it++ = buf[i];
    }

    return 0;
}

}  // namespace

namespace catter {
void Recorder::build(Recorder& recorder, const char* dir) noexcept {
    if(dir == nullptr || dir[0] == '\0') {
        recorder.dir_[0] = '\0';
        recorder.file_name_index_ = nullptr;
        return;
    }
    auto dir_sz = array::length(dir);
    catter::array::copy(dir, dir_sz + dir, recorder.dir_, recorder.dir_ + recorder.dir_sz_);
    recorder.file_name_index_ = recorder.dir_ + dir_sz;
    if(*(recorder.file_name_index_ - 1) != config::OS_DIR_SEPARATOR) {
        *recorder.file_name_index_ = config::OS_DIR_SEPARATOR;
        recorder.file_name_index_++;
    }
}

// use pid-tid.log to log cmd and error
const char* Recorder::getFilePath() noexcept {
    if(file_name_index_ == nullptr) {
        return nullptr;
    }
    auto pid = ::getpid();
    auto tid = get_thread_id();
    char* it = file_name_index_;
    if(push_num(pid, it, dir_sz_ + dir_) != 0) {
        return nullptr;
    }
    if(it == dir_ + dir_sz_) {
        return nullptr;
    }
    *it++ = '-';
    if(push_num(tid, it, dir_sz_ + dir_) != 0) {
        return nullptr;
    }
    if(it == dir_ + dir_sz_) {
        return nullptr;
    }
    *it = '\0';
    return dir_;
};

int Recorder::write(std::initializer_list<const char*> data) noexcept {
    if(!valid()) {
        INFO("recorder is not valid");
        return -1;
    }
    if(const auto file_path = getFilePath(); file_path != nullptr) {
        auto fd =
            ::open(file_path, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if(fd == -1) {
            return -1;
        }
        for(auto cont: data) {
            const auto res = ::write(fd, cont, array::length(cont));
            if(res != array::length(cont)) {
                ::close(fd);
                return -1;
            }
        }
        ::close(fd);
        return 0;
    }
    return -1;
};

int Recorder::writeErr(const char* data) noexcept {
    ERROR("recording error: {}", data);
    return write({config::ERROR_PREFFIX, data, "\n"});
};

int Recorder::writeCmd(const char* data) noexcept {
    INFO("recording cmd: {}", data);
    return write({data, "\n"});
};

bool Recorder::valid() noexcept {
    return getFilePath() != nullptr;
}
}  // namespace catter
