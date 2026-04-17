#include "compiler.h"

#include "apitool.h"

#include <kota/meta/enum.h>

using namespace catter;

namespace {

CAPI(identify_compiler, (std::string compiler_name)->std::string) {
    return std::string{kota::meta::enum_name(catter::identify_compiler(compiler_name))};
}

}  // namespace
