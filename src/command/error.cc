module;

#include "modules/prelude.h"

module catter.command;

namespace catter {

std::string_view name(Error::Kind kind) {
    switch(kind) {
        case Error::Kind::CompilerUnsupported: return "compiler-unsupported";
        case Error::Kind::CompilerParse: return "compiler-parse";
        case Error::Kind::ArchiverNotRecognized: return "archiver-not-recognized";
        case Error::Kind::ArchiverUnsupported: return "archiver-unsupported";
        case Error::Kind::ToolNotRecognized: return "tool-not-recognized";
        case Error::Kind::ToolParse: return "tool-parse";
    }
    std::unreachable();
}

}  // namespace catter
