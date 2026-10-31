module;

#include "modules/prelude.h"

module catter;

import :protocol.protocol;

using namespace catter::protocol;

namespace {

/// Every message of the protocol, for the schema to define each.
struct Messages {
    InitializeParams initialize_params;
    InitializeResult initialize_result;
    Command command;
    Action action;
    Analysis analysis;
    CaptureError capture_error;
    Exit exit;
    Finish finish;
    AnalyzeParams analyze_params;
    AnalyzeResult analyze_result;
};

}  // namespace

/// Writes the JSON Schema of the protocol's messages (src/protocol/protocol.cppm),
/// which the SDKs' types are generated from, to the file it is given.
extern "C++" int main(int argc, char* argv[]) {
#ifdef CATTER_WINDOWS
    // main's argv is in the ANSI code page, which holds few characters past
    // ASCII.
    const auto& args = kotatsu::sys::arguments();
#else
    const std::vector<std::string> args(argv, argv + argc);
#endif
    if(args.size() != 2) {
        std::println(stderr, "usage: catter-protocol-schema <output file>");
        return 1;
    }
    auto schema = kotatsu::codec::json::schema_string<Messages, kotatsu::ipc::lsp_config>(true);
    if(!schema) {
        std::println(stderr, "{}", schema.error().to_string());
        return 1;
    }
    if(auto error = kotatsu::fs::write_file(args[1], *schema + '\n')) {
        std::println(stderr, "Failed to write the schema to `{}`: {}", args[1], error.message());
        return 1;
    }
    return 0;
}
