module;

#include "modules/prelude.h"

#include "support/messages.h"

module catter:session.data;

/// The requests of the IPC, as kota's peers send and serve them.
namespace kota::ipc::protocol {
using namespace catter::ipc;

template <RequestType Type>
struct RequestTraits<Request<Type>> : Request<Type> {};

}  // namespace kota::ipc::protocol
