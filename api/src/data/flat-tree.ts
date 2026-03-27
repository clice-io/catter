/**
 * A tool that you can put an id,
 * parent_id(number or something comparable), and content into it.
 * It will automatically make it into a tree, which provides like
 * child(id), father(id), etc.
 */

/**
 * And we provide a print tree function,
 * print(cb: (content) => string, default is stringify if object, else tostring itself)
 * the logic is same as the follow cpp demo:
 * #pragma once

#include <algorithm>
#include <iostream>
#include <iterator>
#include <ostream>
#include <print>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
template <class Cont>
  requires requires(Cont c) { std::cout << c; }
struct Node {
  Cont content;
  std::vector<Node *> children;

  bool operator==(const Node &other) const {
    return content == other.content &&
           std::equal(children.begin(), children.end(), other.children.begin(),
                      other.children.end(),
                      [](auto a, auto b) { return *a == *b; });
  }

  static constexpr std::string_view COL = " │ ";
  static constexpr std::string_view ROW = "───";
  static constexpr std::string_view TEE = " ├─";
  static constexpr std::string_view TEE_COL = "─┬─";
  static constexpr std::string_view ELBOW = " └─";
  static constexpr std::string_view SPACE = "   ";
  static constexpr std::string_view REPEAT_WRAP_L = "[";
  static constexpr std::string_view REPEAT_WRAP_R = "]";
  static constexpr unsigned sym_len = 3;

  void print_self(std::ostream &o, const std::string &preffix = "",
                  const std::string &surffix = "") const {
    std::stringstream ss;
    ss << content;
    // with the length of this content
    auto nxt_preffix = preffix + std::string(ss.str().size(), ' ');
    o << ss.str();
    // if we need repeat a child, use nxt_surffix rather surffix for new ] in
    // the back
    auto nxt_surffix = surffix + std::string(REPEAT_WRAP_R);
    if (children.empty()) {
      o << surffix << '\n';
      return;
    }

    if (children.size() == 1) {
      o << ROW;
      nxt_preffix += SPACE;
      children[0]->print_self(o, nxt_preffix);
      return;
    }

    auto mid_nxt_preffix = nxt_preffix + COL.data();
    auto end_nxt_preffix = nxt_preffix + SPACE.data();
    auto it = children.begin();
    while (it != children.end()) {

      auto nxt_it = std::find_if_not(
          it, children.end(), [&](auto &other) { return (**it) == (*other); });
      if (it == children.begin() && nxt_it == children.end()) {
        o << ROW;
      } else if (it == children.begin()) {
        o << TEE_COL;
      } else if (nxt_it == children.end()) {
        o << nxt_preffix << ELBOW;
      } else {
        o << nxt_preffix << TEE;
      }
      if (auto n = std::distance(it, nxt_it); n > 1) {
        auto p = *(children.begin());
        auto count_pre = std::format("{}*{}", n, REPEAT_WRAP_L);
        o << count_pre;
        (*it)->print_self(
            o,
            ((nxt_it == children.end()) ? end_nxt_preffix : mid_nxt_preffix) +
                std::string(count_pre.size(), ' '),
            nxt_surffix);
      } else {
        (*it)->print_self(
            o, (nxt_it == children.end()) ? end_nxt_preffix : mid_nxt_preffix,
            surffix);
      }
      it = nxt_it;
    }
  }
};
 */
