module;

#include "modules/prelude.h"

module catter;

import :feature.timeline;

namespace catter::feature {

std::vector<std::uint32_t> lanes_of(std::span<const Span> spans) {
    // Whether the spans each span started overlap one another.
    std::vector<bool> parallel(spans.size());
    auto children = std::views::iota(std::size_t(0), spans.size()) |
                    std::views::filter([&](std::size_t i) { return spans[i].parent.has_value(); }) |
                    std::ranges::to<std::vector>();
    std::ranges::sort(children, {}, [&](std::size_t i) {
        return std::pair(*spans[i].parent, spans[i].start);
    });
    for(std::size_t i = 0; i < children.size();) {
        const auto parent = *spans[children[i]].parent;
        auto ended = spans[children[i]].end;
        for(i += 1; i < children.size() && spans[children[i]].parent == parent; i += 1) {
            const auto& child = spans[children[i]];
            if(child.start < ended) {
                parallel[parent] = true;
            }
            ended = std::max(ended, child.end);
        }
    }

    auto order = std::views::iota(std::size_t(0), spans.size()) | std::ranges::to<std::vector>();
    std::ranges::sort(order, {}, [&](std::size_t i) {
        return std::tuple(spans[i].start, -spans[i].end, i);
    });
    constexpr auto unplaced = std::numeric_limits<std::uint32_t>::max();
    std::vector<std::uint32_t> lanes(spans.size(), unplaced);
    // The spans running on each lane, each within the one before it.
    std::vector<std::vector<std::size_t>> running;
    const auto running_at = [&](std::uint32_t lane,
                                std::int64_t time) -> std::vector<std::size_t>& {
        auto& open = running[lane];
        while(!open.empty() && spans[open.back()].end <= time) {
            open.pop_back();
        }
        return open;
    };
    const auto lane_of = [&](const Span& span) -> std::uint32_t {
        if(span.parent.has_value() && !parallel[*span.parent] && lanes[*span.parent] != unplaced &&
           span.end <= spans[*span.parent].end) {
            const auto lane = lanes[*span.parent];
            if(const auto& open = running_at(lane, span.start);
               !open.empty() && open.back() == *span.parent) {
                return lane;
            }
        }
        for(std::uint32_t lane = 0; lane < running.size(); lane += 1) {
            if(running_at(lane, span.start).empty()) {
                return lane;
            }
        }
        running.emplace_back();
        return static_cast<std::uint32_t>(running.size() - 1);
    };
    for(const auto i: order) {
        lanes[i] = lane_of(spans[i]);
        running[lanes[i]].push_back(i);
    }
    return lanes;
}

Activity activity_of(std::span<const Span> spans) {
    struct Event {
        std::int64_t time;

        bool starts;

        std::size_t span;
    };

    std::vector<Event> events;
    for(std::size_t i = 0; i < spans.size(); i += 1) {
        if(spans[i].start < spans[i].end) {
            events.push_back({.time = spans[i].start, .starts = true, .span = i});
            events.push_back({.time = spans[i].end, .starts = false, .span = i});
        }
    }
    // At one time, what ends first, and of what starts, what runs longer.
    std::ranges::sort(events, {}, [&](const Event& event) {
        return std::tuple(event.time, event.starts, event.starts ? -spans[event.span].end : 0);
    });

    Activity activity{.worked = std::vector<std::int64_t>(spans.size())};
    std::vector<std::vector<std::size_t>> started(spans.size());
    for(std::size_t i = 0; i < spans.size(); i += 1) {
        if(spans[i].parent.has_value()) {
            started[*spans[i].parent].push_back(i);
        }
    }
    // Of each span, whether it runs, the spans it started that run, whether it counts among those
    // of its parent, and since when it works.
    std::vector<bool> running(spans.size());
    std::vector<std::uint32_t> children(spans.size());
    std::vector<bool> counted(spans.size());
    std::vector<std::int64_t> since(spans.size());
    std::uint32_t working = 0;
    const auto work = [&](std::size_t i, std::int64_t time) {
        working += 1;
        since[i] = time;
    };
    const auto rest = [&](std::size_t i, std::int64_t time) {
        working -= 1;
        activity.worked[i] += time - since[i];
    };
    for(std::size_t e = 0; e < events.size();) {
        const auto time = events[e].time;
        for(; e < events.size() && events[e].time == time; e += 1) {
            const auto i = events[e].span;
            const auto parent = spans[i].parent;
            if(events[e].starts) {
                running[i] = true;
                if(parent.has_value() && running[*parent]) {
                    counted[i] = true;
                    if(children[*parent] == 0) {
                        rest(*parent, time);
                    }
                    children[*parent] += 1;
                }
                // A span may start after one it started, when they start at once.
                for(const auto child: started[i]) {
                    if(running[child] && !counted[child] && spans[child].start == time) {
                        counted[child] = true;
                        children[i] += 1;
                    }
                }
                if(children[i] == 0) {
                    work(i, time);
                }
                continue;
            }
            running[i] = false;
            if(children[i] == 0) {
                rest(i, time);
            }
            if(counted[i]) {
                children[*parent] -= 1;
                if(children[*parent] == 0 && running[*parent]) {
                    work(*parent, time);
                }
            }
        }
        if(activity.working.empty() || activity.working.back().second != working) {
            activity.working.emplace_back(time, working);
        }
    }
    return activity;
}

}  // namespace catter::feature
