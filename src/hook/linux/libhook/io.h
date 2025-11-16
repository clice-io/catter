#pragma once

#include <cstddef>
#include <initializer_list>
#include <limits.h>

namespace catter {
class Recorder {
public:
    Recorder() noexcept = default;
    virtual ~Recorder() noexcept = default;

    virtual int write(std::initializer_list<const char*> data) noexcept;
    virtual int writeErr(const char* data) noexcept;
    virtual int writeCmd(const char* data) noexcept;
    virtual bool valid() noexcept;

public:
    static void build(Recorder& recorder, const char* dir) noexcept;

protected:
    const char* getFilePath() noexcept;

    // protected:

public:
    constexpr static size_t dir_sz_ = PATH_MAX;
    char dir_[dir_sz_ + 1];
    char* file_name_index_;
};
}  // namespace catter
