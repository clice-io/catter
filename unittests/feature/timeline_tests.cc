module;

#include "support/prelude.h"

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

ZEST_CASE(the_spans_of_a_lane_nest_and_overlapping_siblings_share_none) {
    for(std::uint32_t seed = 0; seed < 20; seed += 1) {
        const auto spans = random_spans(seed);
        const auto lanes = feature::lanes_of(spans);
        for(std::size_t a = 0; a < spans.size(); a += 1) {
            for(std::size_t b = a + 1; b < spans.size(); b += 1) {
                if(lanes[a] != lanes[b] || !overlap(spans[a], spans[b])) {
                    continue;
                }
                ZEXPECT((within(spans[a], spans[b]) || within(spans[b], spans[a])));
                ZEXPECT((!spans[a].parent.has_value() || spans[a].parent != spans[b].parent));
            }
        }
    }
};

ZEST_CASE(a_span_starting_with_its_parent_works_in_its_place) {
    const std::vector<Span> spans{
        {.start = 0, .end = 100, .parent = 1},
        {.start = 0, .end = 100},
    };
    const auto activity = feature::activity_of(spans);
    ZEXPECT(activity.worked == std::vector<std::int64_t>{100, 0});
    ZEXPECT(activity.working == std::vector<std::pair<std::int64_t, std::uint32_t>>{
                                    {0,   1},
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
        ZEXPECT(working == ranges::fold_left(activity.worked, std::int64_t(0), std::plus()));
        for(std::size_t i = 0; i < spans.size(); i += 1) {
            ZEXPECT(0 <= activity.worked[i]);
            ZEXPECT(activity.worked[i] <= spans[i].end - spans[i].start);
        }
    }
};

};  // ZEST_SUITE(feature_timeline_tests)
