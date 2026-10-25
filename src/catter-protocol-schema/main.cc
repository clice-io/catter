#include <fstream>
#include <print>
#include <kota/codec/json/schema.h>
#include <kota/ipc/codec.h>

#include "protocol.h"

using namespace catter::protocol;

namespace {

/// Every message of the protocol, for the schema to define each.
struct Messages {
    InitializeParams initialize_params;
    InitializeResult initialize_result;
    Command command;
    Action action;
    CaptureError capture_error;
    Exit exit;
    Finish finish;
};

}  // namespace

/// Writes the JSON Schema of the protocol's messages (src/catter/core/protocol.h),
/// which the SDKs' types are generated from, to the file it is given.
int main(int argc, char* argv[]) {
    if(argc != 2) {
        std::println(stderr, "usage: catter-protocol-schema <output file>");
        return 1;
    }
    auto schema = kota::codec::json::schema_string<Messages, kota::ipc::lsp_config>(true);
    if(!schema) {
        std::println(stderr, "{}", schema.error().to_string());
        return 1;
    }
    std::ofstream output(argv[1], std::ios::binary);
    output << *schema << '\n';
    if(!output.good()) {
        std::println(stderr, "Failed to write the schema to `{}`", argv[1]);
        return 1;
    }
    return 0;
}
