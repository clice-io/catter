#include <kota/zest/zest.h>

#include "support/log.h"
#include "support/paths.h"

int main(int argc, char** argv) {
    catter::log::init_logger("ut", catter::data_path("test.log"), false);
    return kota::zest::run_cli(argc, argv);
}
