#include <cstdint>


#include "qjs.h"

#include "../apitool.h"

using namespace catter;
namespace {

CAPI(service_on_start, (qjs::Object cb)->void){
    auto func = cb.as<qjs::Function<qjs::Object(qjs::Object config)>>();
}

CAPI(service_on_finish, (qjs::Object cb)->void){
    auto func = cb.as<qjs::Function<void()>>();
}

CAPI(service_on_command, (qjs::Object cb)->void){
    auto func = cb.as<qjs::Function<qjs::Object(uint32_t id, qjs::Object data)>>();
}

CAPI(service_on_execution, (qjs::Object cb)->void){
    auto func = cb.as<qjs::Function<void(uint32_t id, qjs::Object data)>>();
}


}