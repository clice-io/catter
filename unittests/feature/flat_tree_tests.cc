module;

#include "support/prelude.h"

module catter;

import :feature.flat_tree;

using namespace catter;

namespace {

using Tree = feature::FlatTree<int, std::string>;

std::vector<int> children_of(const Tree& tree, int id) {
    return tree.find(id)->children;
}

/// The cycles of `tree`, each sorted and then all of them, for an order that does not depend on
/// where the search starts.
std::vector<std::vector<int>> sorted_cycles(Tree& tree) {
    auto cycles = tree.assemble();
    for(auto& cycle: cycles) {
        ranges::sort(cycle);
    }
    ranges::sort(cycles);
    return cycles;
}

}  // namespace

ZEST_SUITE(builtin_flat_tree_tests) {

ZEST_CASE(links_children_in_the_order_they_come) {
    Tree tree;
    tree.merge(1, "root");
    tree.merge(2, "left", 1);
    tree.merge(3, "right", 1);
    tree.merge(4, "leaf", 2);
    tree.merge(5, "orphan", 42);

    ZEXPECT(tree.size() == 5);
    ZEXPECT(tree.assemble().empty());
    ZEXPECT(tree.starts() == std::vector{1, 5});
    ZEXPECT(children_of(tree, 1) == std::vector{2, 3});
    ZEXPECT(children_of(tree, 2) == std::vector{4});
    ZEXPECT(children_of(tree, 5).empty());
};

ZEST_CASE(a_parent_may_come_after_its_children) {
    Tree tree;
    tree.merge(2, "child", 1);
    tree.merge(3, "leaf", 2);
    ZEXPECT(tree.assemble().empty());
    ZEXPECT(tree.starts() == std::vector{2});

    tree.merge(1, "root");
    ZEXPECT(tree.assemble().empty());
    ZEXPECT(tree.starts() == std::vector{1});
    ZEXPECT(children_of(tree, 1) == std::vector{2});
    ZEXPECT(children_of(tree, 2) == std::vector{3});

    tree.merge(3, "leaf", 1);
    tree.assemble();
    ZEXPECT(children_of(tree, 1) == std::vector{2, 3});
    ZEXPECT(tree.find(3)->parents == std::vector{2, 1});
};

ZEST_CASE(a_node_may_have_several_parents) {
    feature::FlatTree<std::string, std::string> tree;
    tree.merge("app", "app");
    tree.merge("tool", "tool");
    tree.merge("main.o", "main.o", "app");
    tree.merge("main.o", "main.o", "tool");

    ZEXPECT(tree.assemble().empty());
    ZEXPECT(tree.starts() == std::vector<std::string>{"app", "tool"});
    ZEXPECT(tree.find("app")->children == std::vector<std::string>{"main.o"});
    ZEXPECT(tree.find("tool")->children == std::vector<std::string>{"main.o"});
};

ZEST_CASE(finds_cycles) {
    Tree two;
    two.merge(1, "one", 2);
    two.merge(2, "two", 1);
    ZEXPECT(sorted_cycles(two) == std::vector<std::vector<int>>{
                                      {1, 2}
    });
    ZEXPECT(two.starts().empty());

    Tree self;
    self.merge(1, "self", 1);
    self.merge(2, "child", 1);
    ZEXPECT(sorted_cycles(self) == std::vector<std::vector<int>>{{1}});

    Tree downstream;
    downstream.merge(1, "one", 2);
    downstream.merge(2, "two", 1);
    downstream.merge(3, "downstream", 2);
    ZEXPECT(sorted_cycles(downstream) == std::vector<std::vector<int>>{
                                             {1, 2}
    });

    Tree apart;
    apart.merge(1, "one", 2);
    apart.merge(2, "two", 1);
    apart.merge(3, "three", 4);
    apart.merge(4, "four", 3);
    ZEXPECT(sorted_cycles(apart) == std::vector<std::vector<int>>{
                                        {1, 2},
                                        {3, 4}
    });
};

ZEST_CASE(a_cycle_is_a_path_that_links_back) {
    Tree tree;
    tree.merge(1, "one", 3);
    tree.merge(2, "two", 1);
    tree.merge(3, "three", 2);
    tree.merge(4, "shortcut", 1);
    tree.merge(1, "one", 4);

    const auto cycles = tree.assemble();
    ZASSERT(cycles.size() == 1);
    const auto& cycle = cycles.front();
    for(std::size_t i = 0; i < cycle.size(); i += 1) {
        const auto next = cycle[(i + 1) % cycle.size()];
        ZEXPECT(ranges::contains(children_of(tree, cycle[i]), next));
    }
};

};  // ZEST_SUITE(builtin_flat_tree_tests)
