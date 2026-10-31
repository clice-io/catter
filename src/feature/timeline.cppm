module;

#include "support/prelude.h"

module catter:feature.timeline;

namespace catter::feature {

/// A command of a build as it ran, from `start` to `end`, in microseconds.
struct Span {
    std::int64_t start;

    std::int64_t end;

    /// The span of the command that started it, by its position among the spans.
    std::optional<std::size_t> parent;
};

/// The lane each of `spans` is drawn on, from 0, so that the spans of a lane nest: of two spans
/// of a lane, one ends before the other starts, or runs within it. A span runs within its parent
/// on its parent's lane when it fits there and its parent ran no other span beside it, as a shell
/// runs a compiler and the compiler its stages; any other span is on the first lane where nothing
/// runs as it starts, as each of the jobs a build tool runs at once.
std::vector<std::uint32_t> lanes_of(std::span<const Span> spans);

/// What the spans of a build did over time. A span works while it runs and no span it started
/// does; one that takes no time, never, as if it were not there.
struct Activity {
    /// How many spans work from each time on, each time that changes.
    std::vector<std::pair<std::int64_t, std::uint32_t>> working;

    /// How long each span worked.
    std::vector<std::int64_t> worked;
};

Activity activity_of(std::span<const Span> spans);

}  // namespace catter::feature
