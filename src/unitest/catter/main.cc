#include <boost/ut.hpp>
#include "apitool.h"
#include "js.h"

#include "js.h"
#include <boost/ut.hpp>
#include <filesystem>
#include <print>
#include <filesystem>
#include "libconfig/js-test.h"
#include "libutil/output.h"
namespace ut = boost::ut;

namespace fs = std::filesystem;
using namespace boost::ut::literals;

#define JS_PATH #JS_DIR

int main(int argc, const char** argv) {
    bool res = ut::cfg<ut::override>.run();
    return 0;
}
