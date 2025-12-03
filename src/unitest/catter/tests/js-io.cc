#include <boost/ut.hpp>
namespace ut = boost::ut;

ut::suite js_io_tests = [] {
    ut::test("js io stdout_print") = [] {
        ut::expect(true);
    };
};
