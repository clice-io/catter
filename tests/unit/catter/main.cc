#include <boost/ut.hpp>

namespace ut = boost::ut;

int main(int argc, const char** argv) {
    bool failed = ut::cfg<ut::override>.run();
    return -failed;
}
