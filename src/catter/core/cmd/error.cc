#include "cmd/error.h"

#include <string_view>
#include <utility>

namespace catter::cmd {

std::string_view name(Error::Kind kind) {
    switch(kind) {
        case Error::Kind::CompilerUnsupported: return "compiler-unsupported";
        case Error::Kind::CompilerParse: return "compiler-parse";
        case Error::Kind::ArchiverNotRecognized: return "archiver-not-recognized";
        case Error::Kind::ArchiverUnsupported: return "archiver-unsupported";
    }
    std::unreachable();
}

}  // namespace catter::cmd
