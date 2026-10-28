module;

#include "modules/prelude.h"

module catter;

import :feature.timeline;

using namespace catter;
using feature::Span;

namespace {

/// Spans each started by an earlier one, or by none, mostly within it, some of them outliving it,
/// some taking no time.
std::vector<Span> random_spans(std::uint32_t seed) {
    std::mt19937 random(seed);
    const auto between = [&](std::int64_t low, std::int64_t high) {
        return std::uniform_int_distribution<std::int64_t>(low, high)(random);
    };
    std::vector<Span> spans;
    for(std::size_t i = 0; i < 300; i += 1) {
        Span span{.start = between(0, 10'000)};
        if(!spans.empty() && between(0, 9) != 0) {
            const auto parent = static_cast<std::size_t>(between(0, std::ssize(spans) - 1));
            const auto& of = spans[parent];
            span.parent = parent;
            span.start = between(of.start, of.end);
            span.end =
                between(0, 9) == 0 ? span.start + between(0, 5'000) : between(span.start, of.end);
        } else {
            span.end = span.start + between(0, 5'000);
        }
        if(between(0, 19) == 0) {
            span.end = span.start;
        }
        spans.push_back(span);
    }
    return spans;
}

bool overlap(const Span& a, const Span& b) {
    return a.start < b.end && b.start < a.end;
}

bool within(const Span& inner, const Span& outer) {
    return outer.start <= inner.start && inner.end <= outer.end;
}

}  // namespace

ZEST_SUITE(feature_timeline_tests) {

ZEST_CASE(a_shell_and_a_compiler_run_within_their_parents) {
    // make runs a shell, which runs the compiler, which runs its stages; then make links.
    const std::vector<Span> spans{
        {.start = 0, .end = 100},
        {.start = 10, .end = 60, .parent = 0},
        {.start = 11, .end = 59, .parent = 1},
        {.start = 12, .end = 50, .parent = 2},
        {.start = 51, .end = 58, .parent = 2},
        {.start = 65, .end = 95, .parent = 0},
    };
    ZEXPECT(feature::lanes_of(spans) == std::vector<std::uint32_t>{0, 0, 0, 0, 0, 0});
};

ZEST_CASE(jobs_run_at_once_each_on_the_first_free_lane) {
    // ninja runs three compiles at once, then the link once the first has ended.
    const std::vector<Span> spans{
        {.start = 0, .end = 100},
        {.start = 1, .end = 50, .parent = 0},
        {.start = 2, .end = 30, .parent = 0},
        {.start = 3, .end = 40, .parent = 0},
        {.start = 35, .end = 90, .parent = 0},
        {.start = 60, .end = 80, .parent = 0},
    };
    ZEXPECT(feature::lanes_of(spans) == std::vector<std::uint32_t>{0, 1, 2, 3, 2, 1});
};

ZEST_CASE(a_command_outliving_its_parent_runs_on_a_lane_of_its_own) {
    const std::vector<Span> spans{
        {.start = 0, .end = 100},
        {.start = 10, .end = 20, .parent = 0},
        {.start = 15, .end = 50, .parent = 1},
    };
    ZEXPECT(feature::lanes_of(spans) == std::vector<std::uint32_t>{0, 0, 1});
};

ZEST_CASE(parents_in_a_cycle_or_out_of_order_are_laid_out) {
    const std::vector<Span> spans{
        {.start = 10, .end = 20, .parent = 1},
        {.start = 11, .end = 15, .parent = 0},
        {.start = 0,  .end = 30, .parent = 3},
        {.start = 5,  .end = 6,  .parent = 2},
    };
    ZEXPECT(feature::lanes_of(spans) == std::vector<std::uint32_t>{1, 1, 0, 0});
};

ZEST_CASE(the_spans_of_a_lane_nest_and_overlapping_siblings_share_none) {
    for(std::uint32_t seed = 0; seed < 20; seed += 1) {
        const auto spans = random_spans(seed);
        const auto lanes = feature::lanes_of(spans);
        for(std::size_t a = 0; a < spans.size(); a += 1) {
            for(std::size_t b = a + 1; b < spans.size(); b += 1) {
                if(lanes[a] != lanes[b] || !overlap(spans[a], spans[b])) {
                    continue;
                }
                ZEXPECT(within(spans[a], spans[b]) || within(spans[b], spans[a]));
                ZEXPECT(!spans[a].parent.has_value() || spans[a].parent != spans[b].parent);
            }
        }
    }
};

ZEST_CASE(a_compile_works_while_its_shell_waits) {
    // make runs a shell, which runs the compiler; then a second compile runs beside a third.
    const std::vector<Span> spans{
        {.start = 0, .end = 100},
        {.start = 10, .end = 60, .parent = 0},
        {.start = 20, .end = 50, .parent = 1},
        {.start = 70, .end = 90, .parent = 0},
        {.start = 75, .end = 95, .parent = 0},
    };
    const auto activity = feature::activity_of(spans);
    ZEXPECT(activity.worked == std::vector<std::int64_t>{25, 20, 30, 20, 20});
    ZEXPECT(activity.working == std::vector<std::pair<std::int64_t, std::uint32_t>>{
                                    {0,   1},
                                    {75,  2},
                                    {90,  1},
                                    {100, 0},
    });
};

ZEST_CASE(what_spans_work_adds_up_to_how_many_work_over_time) {
    for(std::uint32_t seed = 0; seed < 20; seed += 1) {
        const auto spans = random_spans(seed);
        const auto activity = feature::activity_of(spans);
        std::int64_t working = 0;
        for(std::size_t i = 0; i + 1 < activity.working.size(); i += 1) {
            working += (activity.working[i + 1].first - activity.working[i].first) *
                       activity.working[i].second;
        }
        ZEXPECT(activity.working.back().second == 0);
        ZEXPECT(working == std::ranges::fold_left(activity.worked, std::int64_t(0), std::plus()));
        for(std::size_t i = 0; i < spans.size(); i += 1) {
            ZEXPECT(0 <= activity.worked[i] && activity.worked[i] <= spans[i].end - spans[i].start);
        }
    }
};

};  // ZEST_SUITE(feature_timeline_tests)
