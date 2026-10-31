module;

#include "support/messages.h"
#include "support/prelude.h"

module catter;

import catter.command;
import :feature.descendants;
import :feature.json;
import :feature.profile;
import :feature.timeline;
import :session.handler;

namespace catter::feature {

namespace {

namespace dyn = codec::dyn;

/// What catter makes of a command it analyzes, a compiler's or an archiver's.
struct Analysis {
    /// Its phase, `compile`, `link`, ..., or `archive`.
    std::string category;

    /// The name of the file it is shown by: the source it compiles, or the file it makes.
    std::string file;

    /// The files it reads and writes, absolute; the standard streams, `-`, left out.
    std::vector<std::string> reads;

    std::vector<std::string> writes;
};

void add_path(std::vector<std::string>& paths, std::string_view path, std::string_view cwd) {
    if(path != "-") {
        fs::path::resolve(paths.emplace_back(), path, cwd);
    }
}

std::optional<Analysis> analyze(const data::command& command) {
    if(auto compiler =
           analyze_compiler(command.executable, command.args, command.cwd, fs::read_file)) {
        Analysis analysis{.category = std::string(name(compiler->phase))};
        for(const auto& read: compiler->reads) {
            add_path(analysis.reads, read.path, command.cwd);
        }
        for(const auto& write: compiler->writes) {
            add_path(analysis.writes, write.path, command.cwd);
        }
        const bool makes = compiler->phase == Phase::Link || compiler->phase == Phase::Archive ||
                           compiler->phase == Phase::RelocatableLink;
        if(makes && !compiler->writes.empty()) {
            analysis.file = fs::path::filename(compiler->writes.front().path);
        } else if(auto source = ranges::find(compiler->reads, InputRole::Source, &Read::role);
                  !makes && source != compiler->reads.end()) {
            analysis.file = fs::path::filename(source->path);
        }
        return analysis;
    }
    if(auto archiver = analyze_archiver(command.executable, command.args)) {
        Analysis analysis{
            .category = "archive",
            .file = std::string(fs::path::filename(archiver->archive)),
        };
        for(const auto& read: archiver->reads) {
            add_path(analysis.reads, read, command.cwd);
        }
        for(const auto& write: archiver->writes) {
            add_path(analysis.writes, write, command.cwd);
        }
        return analysis;
    }
    return std::nullopt;
}

/// `time`, in microseconds, as the summary shows it: in milliseconds under a second, else in
/// seconds.
std::string format_time(std::int64_t time) {
    if(time < 1'000'000) {
        return std::format("{} ms", time / 1'000);
    }
    return std::format("{:.1f} s", static_cast<double>(time) / 1e6);
}

/// `count` commands, in words.
std::string commands_of(std::size_t count) {
    return std::format("{} command{}", count, count == 1 ? "" : "s");
}

/// A command of the build, as the trace shows it.
struct Command {
    data::ipcid_t id;

    std::optional<data::ipcid_t> parent;

    /// Its program's name, and for a command catter analyzes, the file it is shown by.
    std::string name;

    /// For a command catter analyzes, its category.
    std::string category;

    /// Its arguments, on one line.
    std::string line;

    std::string cwd;

    /// For a command catter analyzes, the files it reads and writes.
    std::vector<std::string> reads;

    std::vector<std::string> writes;

    /// Whether catter analyzes it: the commands it starts are part of it, as a compiler driver's
    /// `cc1plus` and `as` are.
    bool analyzed = false;

    /// Whether it is part of a command catter analyzes.
    bool inside = false;

    /// Whether catter could not capture it after all, as when its program could not start.
    bool uncaptured = false;

    /// How it ran, once it has.
    std::optional<data::process_result> result;
};

/// The commands of a build that ran, as the trace lays them out.
struct Timeline {
    /// The commands, each as it ran.
    std::vector<const Command*> commands;

    std::vector<Span> spans;

    std::vector<std::uint32_t> lanes;

    /// The longest chain of the commands catter analyzes, each reading a file the one before it
    /// wrote, by the time they ran: positions in `commands`.
    std::vector<std::size_t> critical;

    /// Of each compile, the files the commands that read what it wrote write.
    std::vector<std::vector<std::string_view>> targets;

    /// What the commands did over time, a command catter analyzes working while what it started
    /// runs.
    Activity activity;

    /// When the first command started.
    std::int64_t origin = 0;

    /// From then on until the last ended.
    std::int64_t wall = 0;
};

Timeline timeline_of(std::span<const Command> commands) {
    Timeline timeline;
    std::unordered_map<data::ipcid_t, std::size_t> positions;
    for(const auto& command: commands) {
        if(command.result.has_value()) {
            positions.insert_or_assign(command.id, timeline.commands.size());
            timeline.commands.push_back(&command);
        }
    }
    const auto position_of = [&](std::optional<data::ipcid_t> id) -> std::optional<std::size_t> {
        if(!id.has_value()) {
            return std::nullopt;
        }
        auto found = positions.find(*id);
        return found == positions.end() ? std::nullopt : std::optional(found->second);
    };
    for(const auto* command: timeline.commands) {
        // A replayed build may name a command as its own parent.
        auto parent = position_of(command->parent);
        if(parent == timeline.spans.size()) {
            parent.reset();
        }
        timeline.spans.push_back({
            .start = command->result->start,
            .end = command->result->start + command->result->duration,
            .parent = parent,
        });
    }
    timeline.lanes = lanes_of(timeline.spans);
    // What a command catter analyzes starts works as part of it.
    auto working = timeline.spans;
    for(std::size_t i = 0; i < working.size(); i += 1) {
        if(timeline.commands[i]->inside) {
            working[i].end = working[i].start;
        }
    }
    timeline.activity = activity_of(working);

    if(!timeline.spans.empty()) {
        timeline.origin = ranges::min(timeline.spans | views::transform(&Span::start));
        timeline.wall =
            ranges::max(timeline.spans | views::transform(&Span::end)) - timeline.origin;
    }

    auto order = views::iota(std::size_t(0), timeline.commands.size()) | ranges::to<std::vector>();
    ranges::sort(order, {}, [&](std::size_t i) { return std::pair(timeline.spans[i].start, i); });
    timeline.targets.resize(timeline.commands.size());
    std::unordered_map<std::string_view, std::size_t> writers;
    std::vector<std::int64_t> lengths(timeline.commands.size());
    std::vector<std::optional<std::size_t>> before(timeline.commands.size());
    std::optional<std::size_t> last;
    for(const auto i: order) {
        const auto& command = *timeline.commands[i];
        if(!command.analyzed) {
            continue;
        }
        for(const auto& read: command.reads) {
            auto writer = writers.find(read);
            if(writer == writers.end() ||
               timeline.spans[writer->second].start >= timeline.spans[i].start) {
                continue;
            }
            const auto w = writer->second;
            if(!before[i].has_value() || lengths[w] > lengths[*before[i]]) {
                before[i] = w;
            }
            if(timeline.commands[w]->category == "compile") {
                for(const auto& write: command.writes) {
                    if(!ranges::contains(timeline.targets[w], write)) {
                        timeline.targets[w].push_back(write);
                    }
                }
            }
        }
        lengths[i] = command.result->duration + (before[i].has_value() ? lengths[*before[i]] : 0);
        for(const auto& write: command.writes) {
            writers.insert_or_assign(write, i);
        }
        if(!last.has_value() || lengths[i] > lengths[*last]) {
            last = i;
        }
    }
    for(auto step = last; step.has_value(); step = before[*step]) {
        timeline.critical.push_back(*step);
    }
    ranges::reverse(timeline.critical);
    return timeline;
}

/// A complete event of the trace: `command` on thread `tid`.
dyn::Object event_of(const Command& command, std::int64_t tid, std::int64_t origin) {
    return {
        {"name", command.name                  },
        {"ph",   "X"                           },
        {"pid",  1                             },
        {"tid",  tid                           },
        {"ts",   command.result->start - origin},
        {"dur",  command.result->duration      },
    };
}

/// The thread metadata of the trace: thread `tid` is shown as `name`, `index`-th.
std::array<dyn::Object, 2> thread_of(std::int64_t tid, std::string name, std::int64_t index) {
    return {
        dyn::Object{
                    {"name", "thread_name"},
                    {"ph", "M"},
                    {"pid", 1},
                    {"tid", tid},
                    {"args", dyn::Object{{"name", std::move(name)}}},
                    },
        dyn::Object{
                    {"name", "thread_sort_index"},
                    {"ph", "M"},
                    {"pid", 1},
                    {"tid", tid},
                    {"args", dyn::Object{{"sort_index", index}}},
                    },
    };
}

/// The trace of `timeline`, of the build `process` names, as Chrome's trace event format writes
/// it: the critical path on thread 1, each lane on a thread after it, how many commands work
/// over time as the counter `running`; an event on each line.
std::string trace_of(const Timeline& timeline, std::string_view process) {
    std::vector<JSON> events;
    events.push_back(dyn::Object{
        {"name", "process_name"                },
        {"ph",   "M"                           },
        {"pid",  1                             },
        {"args", dyn::Object{{"name", process}}},
    });
    if(!timeline.critical.empty()) {
        ranges::move(thread_of(1, "critical path", 0), std::back_inserter(events));
    }
    const auto lanes = timeline.lanes.empty() ? 0 : ranges::max(timeline.lanes) + 1;
    for(std::uint32_t lane = 0; lane < lanes; lane += 1) {
        ranges::move(thread_of(lane + 2, std::format("lane {}", lane + 1), lane + 1),
                     std::back_inserter(events));
    }

    auto order = views::iota(std::size_t(0), timeline.commands.size()) | ranges::to<std::vector>();
    ranges::sort(order, {}, [&](std::size_t i) {
        return std::tuple(timeline.spans[i].start, -timeline.spans[i].end, i);
    });
    for(const auto i: order) {
        const auto& command = *timeline.commands[i];
        auto event = event_of(command, timeline.lanes[i] + 2, timeline.origin);
        if(!command.category.empty()) {
            event.insert("cat", command.category);
        }
        dyn::Object args{
            {"command", command.line        },
            {"cwd",     command.cwd         },
            {"exit",    command.result->code},
            {"id",      command.id          },
        };
        if(command.parent.has_value()) {
            args.insert("parent", *command.parent);
        }
        if(!command.writes.empty()) {
            args.insert("writes", dyn::Array(command.writes | ranges::to<std::vector<JSON>>()));
        }
        if(!timeline.targets[i].empty()) {
            args.insert("target",
                        dyn::Array(timeline.targets[i] | ranges::to<std::vector<JSON>>()));
        }
        event.insert("args", std::move(args));
        events.push_back(std::move(event));
    }
    for(const auto i: timeline.critical) {
        auto event = event_of(*timeline.commands[i], 1, timeline.origin);
        event.insert("args",
                     dyn::Object{
                         {"id", timeline.commands[i]->id}
        });
        events.push_back(std::move(event));
    }
    for(const auto& [time, working]: timeline.activity.working) {
        events.push_back(dyn::Object{
            {"name", "running"                         },
            {"ph",   "C"                               },
            {"pid",  1                                 },
            {"ts",   time - timeline.origin            },
            {"args", dyn::Object{{"commands", working}}},
        });
    }

    const auto lines =
        events |
        views::transform([](const JSON& event) { return to_json(event, {.pretty = false}); }) |
        views::join_with(std::string_view(",\n")) | ranges::to<std::string>();
    return std::format(R"({{"displayTimeUnit":"ms","traceEvents":[
{}
]}}
)",
                       lines);
}

/// Writes the timeline of the build's commands as a trace, and prints what it shows.
class Profile final : public Handler {
public:
    explicit Profile(ProfileOptions options) : options(std::move(options)) {}

    async::task<std::optional<Build>> on_start(Build build) override {
        fs::path::resolve(output, *options.output, build.cwd);
        process = command_line({.args = build.command});
        co_return build;
    }

    async::task<Action> on_command(data::ipcid_t id,
                                   std::optional<data::ipcid_t> parent,
                                   data::command command) override {
        descendants.add(id, parent);
        const auto& program = command.args.empty() ? command.executable : command.args.front();
        Command entry{
            .id = id,
            .parent = parent,
            .name = kotatsu::replace_invalid_utf8(fs::path::filename(program)),
            .line = command_line(command),
            .inside = descendants.left_out(id),
        };
        if(auto analysis = entry.inside ? std::nullopt : analyze(command)) {
            if(!analysis->file.empty()) {
                entry.name += " " + kotatsu::replace_invalid_utf8(analysis->file);
            }
            entry.category = std::move(analysis->category);
            entry.reads = std::move(analysis->reads);
            entry.writes = std::move(analysis->writes);
            entry.analyzed = true;
            descendants.leave_out_descendants_of(id);
        }
        entry.cwd = std::move(command.cwd);
        positions.insert_or_assign(id, commands.size());
        commands.push_back(std::move(entry));
        co_return Action{};
    }

    async::task<> on_error(data::ipcid_t id,
                           [[maybe_unused]] std::optional<data::ipcid_t> parent,
                           std::string message) override {
        descendants.add(id, std::nullopt);
        if(auto found = positions.find(id); found != positions.end()) {
            commands[found->second].uncaptured = true;
        }
        uncaptured += 1;
        if(!*options.quiet) {
            std::println("{}", kotatsu::replace_invalid_utf8(message));
        }
        co_return;
    }

    async::task<> on_exit(data::ipcid_t id, data::process_result result) override {
        // A backend that only observes the build tells the exit of a command it could not capture.
        if(auto found = positions.find(id); found != positions.end()) {
            commands[found->second].result = result;
        }
        co_return;
    }

    async::task<> on_finish([[maybe_unused]] data::process_result result) override {
        const auto timeline = timeline_of(commands);
        save_file(output, trace_of(timeline, process), "trace");
        if(!*options.quiet) {
            print(timeline);
        }
        co_return;
    }

private:
    void print(const Timeline& timeline) const {
        const auto worked =
            ranges::fold_left(timeline.activity.worked, std::int64_t(0), std::plus());
        const auto most =
            ranges::fold_left(timeline.activity.working | views::values,
                              std::uint32_t(0),
                              [](std::uint32_t a, std::uint32_t b) { return std::max(a, b); });
        std::println("Profiled {} in {}: {:.1f} ran at once on average, {} at most.",
                     commands_of(timeline.commands.size()),
                     format_time(timeline.wall),
                     timeline.wall == 0
                         ? 0.0
                         : static_cast<double>(worked) / static_cast<double>(timeline.wall),
                     most);

        if(!timeline.critical.empty()) {
            const auto step = [&](std::size_t i) {
                const auto& command = *timeline.commands[i];
                return std::format("{} {}", command.name, format_time(command.result->duration));
            };
            const auto& path = timeline.critical;
            std::string steps;
            if(path.size() <= 6) {
                steps = path | views::transform(step) | views::join_with(std::string_view(" > ")) |
                        ranges::to<std::string>();
            } else {
                steps = std::format("{} > {} > {} > ({} more) > {} > {}",
                                    step(path[0]),
                                    step(path[1]),
                                    step(path[2]),
                                    path.size() - 5,
                                    step(path[path.size() - 2]),
                                    step(path.back()));
            }
            std::int64_t length = 0;
            for(const auto i: path) {
                length += timeline.commands[i]->result->duration;
            }
            std::println("Critical path, {}: {}", format_time(length), steps);
        }

        auto slowest =
            views::iota(std::size_t(0), timeline.commands.size()) |
            views::filter([&](std::size_t i) { return timeline.activity.worked[i] > 0; }) |
            ranges::to<std::vector>();
        ranges::sort(slowest, {}, [&](std::size_t i) {
            return std::pair(-timeline.activity.worked[i], timeline.spans[i].start);
        });
        slowest.resize(std::min(slowest.size(), static_cast<std::size_t>(*options.top)));
        if(!slowest.empty()) {
            std::println("Slowest:");
            const auto times = slowest | views::transform([&](std::size_t i) {
                                   return format_time(timeline.activity.worked[i]);
                               }) |
                               ranges::to<std::vector>();
            const auto width = ranges::max(times | views::transform(&std::string::size));
            for(std::size_t i = 0; i < slowest.size(); i += 1) {
                std::println("  {:>{}}  {}", times[i], width, timeline.commands[slowest[i]]->name);
            }
        }

        if(uncaptured != 0) {
            std::println("Left out of the trace: {} catter could not capture.",
                         commands_of(uncaptured));
        }
        if(const auto unfinished = ranges::count_if(commands,
                                                    [](const Command& command) {
                                                        return !command.result.has_value() &&
                                                               !command.uncaptured;
                                                    });
           unfinished != 0) {
            std::println("Left out of the trace: {} that did not end before the build did.",
                         commands_of(static_cast<std::size_t>(unfinished)));
        }
        std::println("Trace saved to {}: open it in https://ui.perfetto.dev or chrome://tracing.",
                     output);
    }

private:
    ProfileOptions options;

    std::string output;

    /// The build command, on one line.
    std::string process;

    Descendants descendants;

    /// The commands, as they were reported.
    std::vector<Command> commands;

    /// Of each command, its position in `commands`.
    std::unordered_map<data::ipcid_t, std::size_t> positions;

    std::size_t uncaptured = 0;
};

}  // namespace

std::unique_ptr<Handler> make_profile(ProfileOptions options) {
    return std::make_unique<Profile>(std::move(options));
}

}  // namespace catter::feature
