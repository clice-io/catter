/// The directories of tests/: its topics (tests/README.md, Topics). Any
/// other directory there is an error.

/// Each topic's family: what catter does to a build as it runs, how a
/// builtin script answers on the commands it captured, or catter's own
/// command line.
export const TOPICS: Readonly<Record<string, "engine" | "builtin" | "command">> = {
    capture: "engine",
    builds: "engine",
    scripts: "engine",
    handlers: "engine",
    cdb: "builtin",
    cmd_tree: "builtin",
    target_tree: "builtin",
    inspect: "builtin",
    cli: "command",
};

/// What tests/ holds beside the topics.
export const SUITES = new Set(["node_modules"]);
