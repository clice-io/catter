module;

#include "support/prelude.h"

module catter;

import :feature.descendants;

using namespace catter;

ZEST_SUITE(builtin_descendants_tests) {

ZEST_CASE(leaves_out_every_descendant) {
    feature::Descendants descendants;
    descendants.add(1, std::nullopt);
    descendants.leave_out_descendants_of(1);
    descendants.add(2, 1);
    descendants.add(3, 2);
    descendants.add(4, std::nullopt);

    ZEXPECT(!descendants.left_out(1));
    ZEXPECT(descendants.left_out(2));
    ZEXPECT(descendants.left_out(3));
    ZEXPECT(!descendants.left_out(4));
};

ZEST_CASE(a_parent_may_come_after_its_children) {
    feature::Descendants descendants;
    descendants.add(2, 1);
    ZEXPECT(!descendants.left_out(2));

    descendants.add(1, std::nullopt);
    descendants.leave_out_descendants_of(1);
    ZEXPECT(descendants.left_out(2));
};

ZEST_CASE(parents_in_a_cycle_end_the_walk) {
    feature::Descendants descendants;
    descendants.add(40, 41);
    descendants.add(41, 40);
    descendants.add(42, 41);

    ZEXPECT(!descendants.left_out(40));
    ZEXPECT(!descendants.left_out(41));
    ZEXPECT(!descendants.left_out(42));
};

};  // ZEST_SUITE(builtin_descendants_tests)
