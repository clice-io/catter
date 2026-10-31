module;

#include "support/messages.h"
#include "support/prelude.h"

module catter:feature.descendants;

namespace catter::feature {

/// The commands of a build, by the command that started each, and those whose descendants a
/// built-in leaves out: the programs a compiler driver runs, `cc1plus`, `as`, `ld`, are not the
/// build's commands.
class Descendants {
public:
    void add(data::ipcid_t id, std::optional<data::ipcid_t> parent) {
        parents[id] = parent;
    }

    void leave_out_descendants_of(data::ipcid_t id) {
        ancestors.insert(id);
    }

    /// Whether command `id` descends from one whose descendants are left out. A replayed build
    /// may name its parents in a cycle.
    bool left_out(data::ipcid_t id) const {
        std::unordered_set<data::ipcid_t> seen;
        for(auto parent = parent_of(id); parent.has_value() && seen.insert(*parent).second;
            parent = parent_of(*parent)) {
            if(ancestors.contains(*parent)) {
                return true;
            }
        }
        return false;
    }

private:
    std::optional<data::ipcid_t> parent_of(data::ipcid_t id) const {
        auto it = parents.find(id);
        return it == parents.end() ? std::nullopt : it->second;
    }

private:
    std::unordered_map<data::ipcid_t, std::optional<data::ipcid_t>> parents;

    /// The commands whose descendants are left out.
    std::unordered_set<data::ipcid_t> ancestors;
};

}  // namespace catter::feature
