#include <kota/zest/zest.h>

#include "util/crossplat.h"
#include "util/log.h"

int main(int argc, char** argv) {
    catter::log::init_logger("ut", catter::util::get_catter_data_path() / "test.log", false);
    return kota::zest::run_cli(argc, argv);
}
