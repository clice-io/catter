#include "apitool.h"
#include "js.h"
#include "qjs.h"

using namespace catter;

namespace {

CAPI(service_on_start, (qjs::Object cb)->void) {
    catter::js::set_on_start(std::move(cb));
}

CAPI(service_on_finish, (qjs::Object cb)->void) {
    catter::js::set_on_finish(std::move(cb));
}

CAPI(service_on_command, (qjs::Object cb)->void) {
    catter::js::set_on_command(std::move(cb));
}

CAPI(service_on_execution, (qjs::Object cb)->void) {
    catter::js::set_on_execution(std::move(cb));
}

}  // namespace
