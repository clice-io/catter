#include <fstream>
#include <print>
#include <quickjs.h>
#include <string>
#include <unordered_map>
#include "../apitool.h"
#include "libqjs/qjs.h"
#include "libutil/output.h"

namespace {
CAPI(stdout_print, (const std::string content)->void) {
    std::print("{}", content);
}

CAPI(stdout_print_green, (const std::string content)->void) {
    catter::output::green("{}", content);
}

CAPI(stdout_print_red, (const std::string content)->void) {
    catter::output::red("{}", content);
}

CAPI(stdout_print_yellow, (const std::string content)->void) {
    catter::output::yellow("{}", content);
}

CAPI(stdout_print_blue, (const std::string content)->void) {
    catter::output::blue("{}", content);
}

}  // namespace

// file read / write
// notice that we have ensure that is in single thread
namespace {
static long file_id_cnt = 1;
static std::unordered_map<int, std::fstream> open_files;

CAPI(file_open, (std::string path)->long) {
    std::fstream fs;
    fs.exceptions(std::fstream::badbit);
    fs.open(catter::capi::util::absolute_of(path), std::ios::in | std::ios::out);
    if(!fs.is_open()) {
        throw catter::qjs::Exception("Failed to open file: " + path);
    }
    auto id = file_id_cnt++;
    open_files.emplace(id, std::move(fs));
    return id;
}

CAPI(file_close, (long file_id)->void) {
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    it->second.close();
    open_files.erase(it);
}

/// whence: 0 - beg, 1 - cur, 2 - end
CAPI(file_seek_read, (long file_id, long offset, long whence)->void) {
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    std::ios_base::seekdir dir;
    switch(whence) {
        case 0: dir = std::ios::beg; break;
        case 1: dir = std::ios::cur; break;
        case 2: dir = std::ios::end; break;
        default: throw catter::qjs::Exception("Invalid whence: " + std::to_string(whence));
    }
    it->second.clear();
    it->second.seekg(offset, dir);
};

CAPI(file_seek_write, (long file_id, long offset, long whence)->void) {
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    std::ios_base::seekdir dir;
    switch(whence) {
        case 0: dir = std::ios::beg; break;
        case 1: dir = std::ios::cur; break;
        case 2: dir = std::ios::end; break;
        default: throw catter::qjs::Exception("Invalid whence: " + std::to_string(whence));
    }
    it->second.clear();
    it->second.seekp(offset, dir);
};

CAPI(file_tell_read, (long file_id)->long) {
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    return it->second.tellg();
};

CAPI(file_tell_write, (long file_id)->long) {
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    return it->second.tellp();
};

/// Receive file_id, size and a ArrayBuffer to write data into
/// return size that actually read
CAPI(file_read_n, (long file_id, long buf_size, catter::qjs::Object array_buffer)->long) {
    if(buf_size < 0) {
        throw catter::qjs::Exception("Buffer size must be non-negative!");
        return 0;
    }
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    if(!JS_IsArrayBuffer(array_buffer.value())) {
        throw catter::qjs::Exception("Third argument must be an ArrayBuffer");
    }

    unsigned long reserved_sz = buf_size;
    auto buf = JS_GetArrayBuffer(array_buffer.context(), &reserved_sz, array_buffer.value());
    if(buf == nullptr || reserved_sz < buf_size) {
        throw catter::qjs::Exception("Failed to get ArrayBuffer data or buffer is too small!");
    }
    it->second.read(reinterpret_cast<char*>(buf), buf_size);
    return it->second.gcount();
}

// Receive file_id, size and a ArrayBuffer to write data from
// return void
CAPI(file_write_n, (long file_id, long buf_size, catter::qjs::Object array_buffer)->void) {
    if(buf_size < 0) {
        throw catter::qjs::Exception("Buffer size must be non-negative!");
        return;
    }
    auto it = open_files.find(file_id);
    if(it == open_files.end()) {
        throw catter::qjs::Exception("Invalid file id: " + std::to_string(file_id));
    }
    if(!JS_IsArrayBuffer(array_buffer.value())) {
        throw catter::qjs::Exception("Third argument must be an ArrayBuffer");
    }

    unsigned long reserved_sz = buf_size;
    auto buf = JS_GetArrayBuffer(array_buffer.context(), &reserved_sz, array_buffer.value());
    if(buf == nullptr || reserved_sz < buf_size) {
        throw catter::qjs::Exception("Failed to get ArrayBuffer data or buffer is small!");
    }
    it->second.write(reinterpret_cast<char*>(buf), buf_size);
}

}  // namespace
