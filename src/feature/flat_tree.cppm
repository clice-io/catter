module;

#include "modules/prelude.h"

#include "support/output.h"

module catter:feature.flat_tree;

namespace catter::feature {

/// A graph built from flat nodes, each naming its parents: a node may come before its parents,
/// name several, or name one that never comes, which makes it a start of the graph. Nodes keep
/// the order they first came in, and so do the links of each.
template <typename Id, typename Content>
class FlatTree {
public:
    struct Node {
        Id id;

        Content content;

        std::vector<Id> parents;

        std::vector<Id> children;
    };

    /// Adds node `id`, or replaces its content and adds `parent` to its parents.
    void merge(Id id, Content content, std::optional<Id> parent = std::nullopt) {
        auto [it, added] = positions.try_emplace(id, pool.size());
        if(added) {
            pool.push_back(Node{.id = std::move(id), .content = std::move(content)});
        } else {
            pool[it->second].content = std::move(content);
        }
        auto& parents = pool[it->second].parents;
        if(parent.has_value() && !std::ranges::contains(parents, *parent)) {
            parents.push_back(std::move(*parent));
        }
    }

    std::size_t size() const {
        return pool.size();
    }

    const std::vector<Node>& nodes() const {
        return pool;
    }

    const Node* find(const Id& id) const {
        auto it = positions.find(id);
        return it == positions.end() ? nullptr : &pool[it->second];
    }

    /// Links each node to the children that name it, and returns the cycles of the graph: one
    /// per strongly connected component that holds one, each a path whose last node links back
    /// to its first; a node linked to itself is a cycle of one.
    std::vector<std::vector<Id>> assemble() {
        for(auto& node: pool) {
            node.children.clear();
        }
        for(const auto& node: pool) {
            for(const auto& parent: node.parents) {
                if(auto position = this->position(parent)) {
                    pool[*position].children.push_back(node.id);
                }
            }
        }

        // Kahn's algorithm: what is left once the nodes whose parents are all gone are gone is
        // in a cycle or after one.
        std::vector<std::size_t> degrees(pool.size(), 0);
        for(std::size_t i = 0; i < pool.size(); i += 1) {
            for(const auto& parent: pool[i].parents) {
                if(position(parent).has_value()) {
                    degrees[i] += 1;
                }
            }
        }
        std::vector<std::size_t> ready;
        for(std::size_t i = 0; i < pool.size(); i += 1) {
            if(degrees[i] == 0) {
                ready.push_back(i);
            }
        }
        std::size_t removed = 0;
        while(!ready.empty()) {
            auto node = ready.back();
            ready.pop_back();
            removed += 1;
            for(const auto& child: pool[node].children) {
                auto position = *this->position(child);
                degrees[position] -= 1;
                if(degrees[position] == 0) {
                    ready.push_back(position);
                }
            }
        }
        if(removed == pool.size()) {
            return {};
        }

        // Of what is left, the cycles are the strongly connected components of more than one
        // node, or of one linked to itself (Tarjan's algorithm).
        Tarjan tarjan{.tree = *this, .left = std::vector<bool>(pool.size())};
        for(std::size_t i = 0; i < pool.size(); i += 1) {
            tarjan.left[i] = degrees[i] > 0;
        }
        for(std::size_t i = 0; i < pool.size(); i += 1) {
            if(tarjan.left[i] && !tarjan.index.contains(i)) {
                tarjan.connect(i);
            }
        }
        return std::move(tarjan.cycles);
    }

    /// The nodes none of whose parents are in the graph.
    std::vector<Id> starts() const {
        return pool | std::views::filter([&](const Node& node) {
                   return std::ranges::none_of(node.parents, [&](const Id& parent) {
                       return position(parent).has_value();
                   });
               }) |
               std::views::transform(&Node::id) | std::ranges::to<std::vector>();
    }

private:
    struct Tarjan {
        FlatTree& tree;

        std::vector<bool> left;

        std::unordered_map<std::size_t, std::size_t> index;

        std::unordered_map<std::size_t, std::size_t> low;

        std::unordered_set<std::size_t> on_stack;

        std::vector<std::size_t> stack;

        std::vector<std::vector<Id>> cycles;

        std::vector<std::size_t> children_left(std::size_t node) const {
            return tree.pool[node].children |
                   std::views::transform([&](const Id& child) { return *tree.position(child); }) |
                   std::views::filter([&](std::size_t child) { return left[child]; }) |
                   std::ranges::to<std::vector>();
        }

        void connect(std::size_t node) {
            const auto next = index.size();
            index[node] = next;
            low[node] = next;
            stack.push_back(node);
            on_stack.insert(node);

            for(auto child: children_left(node)) {
                if(!index.contains(child)) {
                    connect(child);
                    low[node] = std::min(low[node], low[child]);
                } else if(on_stack.contains(child)) {
                    low[node] = std::min(low[node], index[child]);
                }
            }
            if(low[node] != index[node]) {
                return;
            }

            std::vector<std::size_t> component;
            std::size_t top;
            do {
                top = stack.back();
                stack.pop_back();
                on_stack.erase(top);
                component.push_back(top);
            } while(top != node);

            const auto self_loop = std::ranges::any_of(component, [&](std::size_t member) {
                return std::ranges::contains(tree.pool[member].children, tree.pool[member].id);
            });
            if(component.size() > 1 || self_loop) {
                cycles.push_back(cycle_of(component));
            }
        }

        /// A cycle of a strongly connected component: its first node's links within it, followed
        /// until a node comes again.
        std::vector<Id> cycle_of(std::span<const std::size_t> component) const {
            std::unordered_map<std::size_t, std::size_t> seen;
            std::vector<Id> path;
            auto current = component.front();
            while(!seen.contains(current)) {
                seen[current] = path.size();
                path.push_back(tree.pool[current].id);
                const auto children = children_left(current);
                current = *std::ranges::find_if(children, [&](std::size_t child) {
                    return std::ranges::contains(component, child);
                });
            }
            return std::vector<Id>(path.begin() + seen[current], path.end());
        }
    };

    std::optional<std::size_t> position(const Id& id) const {
        auto it = positions.find(id);
        if(it == positions.end()) {
            return std::nullopt;
        }
        return it->second;
    }

private:
    std::vector<Node> pool;

    std::unordered_map<Id, std::size_t> positions;
};

/// The depth a tree renders to, as `-d` gives it.
inline std::optional<std::uint32_t> max_depth_of(std::optional<std::int64_t> depth) {
    if(!depth.has_value()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(*depth);
}

/// The tree of `tree` from its starts, as `catter tree` and `catter targets` print it: a line per
/// node, the text `text` gives it colored by its depth, branching to its children down to
/// `max_depth`; a node shows again under each of its parents, but not under itself.
template <typename Id, typename Content, typename Text>
std::string render(const FlatTree<Id, Content>& tree,
                   Text&& text,
                   std::optional<std::uint32_t> max_depth) {
    constexpr std::format_string<std::string> colors[] = {
        output::BLUE,
        output::GREEN,
        output::YELLOW,
        output::RED,
    };
    std::string out;
    std::vector<Id> path;
    const auto walk = [&](this const auto& self,
                          const Id& id,
                          std::uint32_t depth,
                          const std::string& prefix,
                          bool last,
                          bool branch) -> void {
        const auto& node = *tree.find(id);
        path.push_back(id);
        out += prefix;
        if(branch) {
            out += last ? "└── " : "├── ";
        }
        out += std::format(colors[depth % std::size(colors)], text(node));
        out += '\n';
        if(!max_depth.has_value() || depth < *max_depth) {
            const auto children = node.children | std::views::filter([&](const Id& child) {
                                      return !std::ranges::contains(path, child);
                                  }) |
                                  std::ranges::to<std::vector>();
            const auto next = prefix + (branch ? (last ? "    " : "│   ") : "");
            for(std::size_t i = 0; i < children.size(); i += 1) {
                self(children[i], depth + 1, next, i + 1 == children.size(), true);
            }
        }
        path.pop_back();
    };

    const auto roots = tree.starts();
    if(roots.size() == 1) {
        walk(roots.front(), 0, "", true, false);
    } else if(!roots.empty()) {
        out += ".\n";
        for(std::size_t i = 0; i < roots.size(); i += 1) {
            walk(roots[i], 0, "", i + 1 == roots.size(), true);
        }
    }
    return out;
}

/// What a tree's nodes are: `commands`, a `command`, and what they make, a `command tree`.
struct Words {
    std::string_view nodes;

    std::string_view node;

    std::string_view tree;
};

/// Prints `tree` once the build ended with `code`, as `catter tree` and `catter targets` do: as
/// `render` lays it out, partial when the build failed, then the cycles among its nodes, each
/// node as `text` gives it.
template <typename Id, typename Content, typename Text>
void print(FlatTree<Id, Content>& tree,
           Text&& text,
           std::optional<std::uint32_t> max_depth,
           std::int64_t code,
           Words words) {
    if(code != 0) {
        std::println("Build failed with exit code {}. Printing partial {}.", code, words.tree);
    }
    if(tree.size() == 0) {
        std::println("No {} found.", words.nodes);
        return;
    }
    const auto cycles = tree.assemble();
    std::print("{}", render(tree, text, max_depth));
    if(cycles.empty()) {
        return;
    }
    std::println("");
    std::println("Detected {} cycles:", words.node);
    for(const auto& cycle: cycles) {
        const auto names = cycle |
                           std::views::transform([&](const Id& id) { return text(*tree.find(id)); }) |
                           std::ranges::to<std::vector>();
        std::println("[cycle] {} -> {}",
                     names | std::views::join_with(std::string_view(" -> ")) |
                         std::ranges::to<std::string>(),
                     names.front());
    }
}

}  // namespace catter::feature
