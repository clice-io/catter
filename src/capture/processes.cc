module;

#include "support/prelude.h"

#include "support/messages.h"

module catter;

import :capture.processes;
import :session.process;

namespace catter {

void BuildProcesses::start(std::uint64_t key) {
    members.insert_or_assign(key, Member{});
}

void BuildProcesses::fork(std::uint64_t parent, std::uint64_t child) {
    if(auto found = members.find(parent); found != members.end()) {
        members.insert_or_assign(child, Member{.forked_by = found->second.current()});
    }
}

std::optional<BuildProcesses::Started> BuildProcesses::exec(std::uint64_t key,
                                                            std::uint64_t next,
                                                            data::command command,
                                                            chrono::microseconds time) {
    auto found = members.find(key);
    if(found == members.end()) {
        return std::nullopt;
    }
    auto member = std::move(found->second);
    members.erase(found);
    Started started{.id = new_id(), .parent = member.current(), .command = std::move(command)};
    member.commands.push_back({.id = started.id, .started = time});
    members.insert_or_assign(next, std::move(member));
    return started;
}

std::vector<BuildProcesses::Ended> BuildProcesses::exit(std::uint64_t key,
                                                        const async::process::exit_status& status,
                                                        chrono::microseconds time) {
    auto found = members.find(key);
    if(found == members.end()) {
        return {};
    }
    std::vector<Ended> ended;
    for(const auto& run: found->second.commands | views::reverse) {
        auto result = result_of(status, time - run.started);
        result.start = (run.started - origin).count();
        ended.push_back({.id = run.id, .result = result});
    }
    members.erase(found);
    return ended;
}

}  // namespace catter
