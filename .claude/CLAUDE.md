# catter — Project Guide

catter intercepts the processes a build starts and turns them into a compilation database and other analyses (command trees, target trees, build profiles, fake compilation). A hook library injected into the build (`LD_PRELOAD` / `DYLD_INSERT_LIBRARIES` on Unix, DLL injection on Windows) sends every new process through `catter-proxy`, which asks the `catter` daemon over IPC what to do with the command.

The detailed rules live in the skills under `.claude/skills/`; each skill's description says when to load it. Load it at that moment — don't work from memory.

## Hard Rules

- **Never skip, disable, or weaken a failing test** — no `continue-on-error`, no expected-failure markers, no retries around flakiness. Fix the root cause. A failure on your branch is yours to fix now, even if it looks pre-existing (main is green), and never deferred to a follow-up PR.
- **Never leave a push unverified.** Pushing and verifying run side by side: push without waiting, while a box (the build skill) and CI run the relevant build and tests; the work is done only when both are green, and "it compiles" is not verified. A failure is reproduced and fixed on a box, not by pushing guesses at CI.
- **Never rewrite history on an open PR branch** — no `--amend`, no force push, no rebase without asking first. Append ordinary commits; squash merge cleans up.
- **Never create or push tags, trigger release workflows, or publish anything** without explicit maintainer approval. Never change repository settings, rulesets, or branch protection.
- **Never merge without the maintainer's explicit authorization for that PR.** Approval of the plan or of another PR does not carry over. A PR that is green with every review thread handled is reported as ready, then left alone.

## Working Style

Distilled from real correction history — these mistakes keep recurring:

- **Read before you write.** The project already has the option tables (`src/command/option`), the program resolver (`src/support/resolver`), cross-platform paths (`src/support`) and process helpers (`src/session/process.h`), and kotatsu's async runtime, IPC peers, option parser and zest — search before adding anything that feels generic. New features copy the structure of 2-3 existing features of the same kind. When unsure whether to extend or create, ask.
- **Understand and design yourself.** Read the code in order and think in this conversation. A subagent is for a narrow factual lookup — one or two at most — never for a judgment or a design. Mechanical batches (migrations, scans, multi-dimension review) go to Opus subagents (`model: "opus"`) whose prompt fixes the spec and the acceptance criteria.
- **Optimize the real scenario.** First ask when the code actually runs and what the user experiences (e.g. the hook and `catter-proxy` run once for every process of a build that may start tens of thousands of them; the daemon starts once per build).
- **Ask who hits a corner case, and how often.** A rare state a real workflow reaches gets an explicit error that names it, never a silent fallback that guesses; a state no real workflow reaches gets no code and is reported as a known limitation. Unknown compilers, odd arguments and failing build commands are not corner cases: they are ordinary input, and the build under catter behaves as it would without it.
- **Stay in scope.** A problem outside the task is reported, not fixed.
- **Refactoring means improving the abstraction**, not moving code. Understand what design problem the refactor solves before touching anything.
- **Comments talk to the future reader, not the diff reviewer.** The default for any change is zero new comments. Add one only when the final code, read on its own, would trap or mislead a competent reader — a non-obvious why, an invariant nothing enforces, a workaround for an external quirk that cost real debugging. Never comment what the code does, why the change is correct, or what was there before: that belongs in the PR description and becomes noise the moment it merges. Litmus test: cover the comment; if the surrounding code already tells the reader everything it said, delete it. When moving or rewriting code, existing explanatory comments DO survive — this rule is about adding, not preserving.
- **Apply cleanup and style instructions project-wide.** Grep for every occurrence and fix them in one pass, not just the file under discussion.
- **Calibrate confirmation to impact.** Small reversible changes: just do them. Architecture decisions, API changes, large refactors: propose first. Creating PRs, modifying CI: confirm. Once told "go ahead", execute fully without re-asking mid-way — follow-up pushes inside an approved PR flow are covered by the go-ahead.

## Source Layout

C++23 in headers and `.cc` sources, built by xmake (`xmake.lua`) into a target per directory of `src/`. The directories are layers that include only downward — {`support`, `command`} ← `session` ← {`capture`, `protocol`, `feature`} ← `driver`; `command` builds on nothing of `src/`, `proxy` on `support`, `session` and `capture`, the hook payloads on `support` alone — and every include names its header by its path under `src/` (`"support/log.h"`). `pixi run -e dev check-layers` checks it, as CI does.

- `src/support/` — logging, data paths, UTF-8 and the system's strings, what catter prints; `config/` (names and paths every process agrees on, the hook's environment among them), `resolver/` (finding a program as `exec`/`CreateProcess` would), `winapi/`
- `src/command/` — what a command is: the compiler and its identification, its arguments parsed on the option tables of `option/` (clang, lld, llvm-lib and llvm-dlltool, from LLVM's TableGen output, and nvcc, read by kotatsu's option parser), its mode and target, the files it reads and writes; `ar`; the arguments of linkers and librarians; edits of a command's arguments
- `src/session/` — the handler that decides each command and observes the session (`handler.h`) and the chain of handlers a run has; the session, which starts the build and serves its proxies; the IPC between catter and `catter-proxy` (`data.h`, its messages; `ipc.h`, catter's side); starting a program (`process.h`)
- `src/capture/` — how a session gets the build's commands (`runtime_driver.h`): running the build with the hook, or replaying a recorded one (`replay.h`); `inject/`, launching a command with the hook (`hook.h`, linked into `catter-proxy`; `windows/` adds the DLL of the process's architecture with Detours)
- `src/protocol/` — the handler in another process that catter speaks the handler protocol to: `protocol.h`, its messages; `remote_handler.h`; `analysis.h`, what it is told of a command and how its edits are made; `schema.cc`, `catter-protocol-schema`, which prints the schema of the messages
- `src/feature/` — the built-ins (`compile-commands`, `tree`, `targets`, `explain`) on `command/`: the options of each that are its own, and a handler, but for `explain`, which analyzes one command without a session
- `src/driver/` — the `catter` program: `main.cc` hands the command line to the subcommand it names, a file each (`compile_commands.cc`, `tree.cc`, `targets.cc`, `explain.cc`, `run.cc`) that registers itself (`driver.h`); the options the subcommands that run a build share (`option.h`), the layout of their help (`cli.h`), and the session they run (`app_runner.h`)
- `src/proxy/` — `catter-proxy`: launches the build with the hook attached, and stands in for every intercepted command, relaying it to the daemon and running what the daemon decides
- `src/hook/` — the hook payloads, loaded into every process of the build: `unix/` is the shared library preloaded into every process (the exec-family hooks, the environment it keeps), `windows/` the DLL that hooks process creation with Detours, built once per architecture (`catter-hook-x64.dll`, `catter-hook-arm64.dll`)
- `sdk/` — the handler SDKs: `sdk/python` (the `catter` package, standard library only) and `sdk/typescript` (`@catter/sdk`), whose protocol types `sdk/generate.py` generates from `sdk/protocol.schema.json`, the schema `catter-protocol-schema` (`src/protocol/schema.cc`) prints of `protocol.h`: `pixi run -e dev protocol` regenerates both, and CI fails when they are stale
- `tests/` — the snapshot cases, `tests/<topic>/<subtopic>/NN_name/`: a small project, `test.ts` and `snap.toml`, run against the built `catter` (`tests/README.md` is their contract; the test and write-tests skills)
- `tools/` — the `@catter/tools` package: the case runner (`tools/testing`), the repository's ESLint rules (`tools/lint`), the docs translation check (`tools/docs`)
- `unittests/` — the zest unit tests, mirroring `src/`: `<name>_tests.cc` beside the helpers of `utils/`, in three programs, `unittests` and the hook payloads' `unittests-hook-unix` and `unittests-hook-windows`
- `docs/` — the `docs/en` and `docs/zh` pages published at docs.clice.io/catter (the docs skill)

`tools/`, `tests/` and `sdk/typescript` are one pnpm workspace rooted at the repo top level: run `pixi run -e dev npm-install` from the root, never `pnpm install` inside a package.

### Where it is going

Decided, not yet done — write code for where it stands today, and don't build on what is about to go:

- **Handlers**: a Rust SDK joins those of `sdk/` later.
- **A command library shared with clice**: `src/command` becomes a library of its own, which clice uses in place of the like code of its `src/command`, split out with the move to Bazel. Until then `command/` depends on nothing but kota::option, kota::support and the option tables (no session, process, IPC or protocol), throws nothing, and gives the same output for the same input; new code there follows clice's model and names (`CommandEdit`, `apply_edits`), so clice's response files, wrapper prefixes and nvcc can move in.
- **Staying for now**: the xmake build (Bazel, then C++20 modules, later).

## Build & Test

- **pixi** for environments, **xmake** for building. Environments: `dev` (the default toolchain of the OS: GCC 14 on Linux, xclang's clang on macOS, MSVC on Windows) and `mingw` (Windows, xclang for `x86_64-w64-mingw32`); `clang` (conda-forge clang 20) is a local convenience CI does not use. Modes `debug` (ASan, except MinGW), `release`, `releasedbg`.
- `pixi run -e <env> ci-build <mode>` is what every CI build job runs: configure, build, the unit tests and `check-protocol` (the generated SDK types are up to date); `cases` jobs then run the snapshot cases on the programs it built (the test skill). `pixi run -e dev package` packs a release build. The build skill has the steps one by one.
- **Where builds run:** on a box (the build skill), not on this machine unless truly necessary — it hosts every agent's session, and one build saturates it. macOS and Windows build and test on boxes of that OS, as CI does. A session may hold several, one per platform. Pushing never waits for a box: CI has the capacity to verify in parallel.
- CI (`test.yml`) builds debug and release on Linux, macOS and Windows, plus Windows MinGW and a Windows ARM64 release; every one must pass.
- Format and lint gate, as the lint workflow runs it: `pixi run -e format format` (clang-format, ruff, Prettier, tombi; CI fails on any diff it leaves), `pixi run -e format lint-python` (ruff check) and `pixi run -e dev ci-check` (`check-testing`: the types, lint and tests of `tools/` and `tests/`; `check-sdk-ts`: the types and lint of the TypeScript SDK; `check-layers`: the includes of `src/`).

## Branches and Commits

- Branches are `<type>/<short-topic>`, always created from freshly fetched `origin/main` (`git fetch origin && git switch -c <branch> origin/main`) — never from the local `main`, which goes stale.
- Commit with the repository's configured identity (`git config user.email`), never an email the session environment supplies.
- Commit and PR title format and everything from "code is ready" to "ready to merge" is the **pr** skill.
