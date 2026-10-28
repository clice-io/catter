---
name: cpp-style
description: Read before writing, modifying or reviewing any C++ code — redundancy (the rule we care most about), files, what the hook and proxy must preserve, errors and defense, async, logging and output, naming, words, templates, API preferences.
---

# catter C++ Coding Style

## Redundancy Is a Defect

This is the convention we care most about. Redundant code is not a style
nit — it actively misleads: every guard implies the guarded state can
occur, every branch implies it can be taken, every parameter implies a
caller needs it. When that implication is false, the reader wastes time
defending against ghosts.

- **Every branch must be reachable.** Before adding a guard or fallback,
  prove the state can actually occur — construct the input that hits it.
  If you can't, don't write it.
- **No speculative generality.** No parameters, options, hooks, or
  abstraction layers for hypothetical future callers. Add them when the
  second real caller arrives.
- **One way to do each thing.** Don't leave an old path alive next to its
  replacement "just in case" — migrate all callers and delete it in the
  same change.
- **Re-read after every change.** Edits leave residue: conditions that
  became constant, variables read once, branches that now collapse,
  `else` after `return`, a helper with one remaining caller. Fold them
  before you're done — simplification that removes a concept beats one
  that merely shortens lines.
- **Delete, don't comment out.** Git history is the archive.
- **One behavior difference is one options field.** Two behaviors of the
  same logic are selected by a boolean in an options struct
  (`Options{.capture_output = true}`), not by a subclass, a dummy
  collaborator, or a parallel code path.
- **Cross-cutting invariants get a single writer.** Accounting, budgets,
  ownership and visibility guarantees live in one type whose methods are the
  only transitions and whose doc comment lists the invariants; before coding,
  grep every site the invariant touches and funnel them through one helper
  so a missing site is discoverable. Per-site patches never catch up with a
  reviewer who re-derives the invariant from the whole diff.

## Files & Organization

- C++23. File names are `snake_case`.
- Above `support` the code is C++20 modules (CLAUDE.md): `src/command` is
  `catter.command`, the rest `catter`. What other files use is declared in
  a partition, `<stem>.cppm`, named by its path:
  `module catter:session.process;`, and under `src/command`
  `export module catter.command:option.clang;`, whose namespace is
  exported (`export namespace catter {`) and which `src/command/module.cppm`
  re-exports, as `catter` imports it. It is defined in its source, an
  implementation unit (`module catter;`) that imports the partitions it
  uses (`import :session.process;`). A module unit starts with `module;`,
  then `#include "modules/prelude.h"`, then the headers it still includes:
  `support`'s and the platform's (`<windows.h>`, `<sys/resource.h>`); then
  its module declaration and imports. A `main` is `extern "C++"`.
- The standard library and the libraries of `modules/` come from the
  prelude's `import std.compat;` and `import deps;`, never from an include
  in a module unit. A header or macro of theirs `deps` does not export yet
  goes into `modules/demand.cpp`, and `modules/` is regenerated (the
  upgrade-deps skill).
- `support` and the hook payloads stay headers (`.h`, `#pragma once`,
  never include guards) and sources (`.cc`), which include the standard
  and library headers themselves: the payloads import nothing.
- Includes name the header by its path under `src/`, whatever directory
  includes it: `"support/log.h"`, `"hook/unix/environment.h"`; a directory
  includes and imports only the layers below it (CLAUDE.md,
  `pixi run -e dev check-layers`).
- Code lives in `namespace catter`, flat as clice's: a namespace of its
  own only for the built-ins (`catter::feature`), `catter`'s program
  (`catter::driver`), the hook and its payloads (`catter::hook`), for a
  vocabulary whose names would
  clash in `catter` (the handler protocol's `catter::protocol`, the IPC's
  `catter::ipc` and `catter::data`, the variables of the build's
  environment, `catter::env`, each option table's
  `catter::option::clang`), and for `catter::log` and `catter::output`.
- Platform code is chosen by the macros the build defines (`CATTER_LINUX`,
  `CATTER_MAC`, `CATTER_WINDOWS`), or by a file per platform when most of
  it differs (`capture/inject/unix.cc` and `windows.cc`, `hook/unix` and
  `hook/windows`).
- File-local helpers: a single one is `static`; a cluster of them goes in
  one anonymous namespace.

## Between the Build and Its Commands

The hook and `catter-proxy` stand between the build and every command it
starts. A command under catter sees what it would see without catter: its
arguments byte for byte, its environment and working directory, the
descriptors and signal dispositions it inherits, its terminal; and the
build sees the command's real exit status and output. #119, #138 and #145
are what breaking this cost.

The hook payloads (`src/hook`) run inside every process
of the build, next to that program's own runtime. They:

- link only what they must (`kota::fs` and `kota::sys`, on Windows kotatsu's
  IPC codec too, `:ipc_codec`, which needs no event loop; of `support` the
  messages and the program search, and the logging of the `Debug` build),
  built without ASan and, for each
  Windows DLL, for the DLL's architecture (`payload` in `bazel/defs.bzl`);
  keep their symbols hidden (`exports.map`, `exports.def`); and carry a
  private C++ runtime, xclang's libc++ linked in, so nothing they bring can
  clash with the host program's;
- never let an exception escape into the host: a hooked entry point
  catches everything and turns it into the error the original call
  reports (`errno` and the return value), as the payloads' do
  (`executor.cc`, `guarded` in `hook/windows/main.cc`).

## Data & Types

- `struct` by default, even for types with methods. `class` only when
  there is a real invariant that private access protects.
- `enum class` with an explicit underlying type (e.g. `: std::uint8_t`);
  document each enumerator with `///` when its meaning is not obvious.
- Prefer designated initializers (`{.field = value}`) for aggregate
  construction.
- West const: `const T&`, never `T const&`.
- A class lays out its public members first, then its private member
  functions under one `private:`, then its data members under a second
  `private:` — functions and fields never interleave. Data members stand
  one blank line apart, each one's comment on the lines above it, never
  trailing it.

## Errors, Exceptions & Defense

- An error the process cannot recover from throws
  `cpptrace::runtime_error` (it carries the stack trace) with a message that
  names the failed operation and its input; the entry points (`main.cc` of
  `catter` and `catter-proxy`) catch it and report it. Nothing inspects
  types at run time (`dynamic_cast`, `typeid`).
- A failure the caller handles is a value: `std::expected<T, E>`;
  async code uses kota's outcome types.
- `assert` pins preconditions; impossible branches end in
  `std::unreachable()`.
- Invalid external input (an unknown compiler, arguments no table knows, a
  build command that fails) degrades gracefully — the command runs as it
  would without catter, the database simply lacks what could not be
  parsed. But graceful degradation is not scattered null checks: per the
  redundancy rule, defend only against states that can occur. The `Debug`
  (ASan) build is the safety net.

## Error Handling (control flow)

- Scope an error variable with an `if` init-statement when nothing after
  the `if` needs it: `if (auto result = foo(); !result.has_value())`.
- A type with `operator bool` needs no condition: `if (auto err = foo())`,
  never `if (auto err = foo(); err)`.
- When the success path needs the variable, declare it as a plain local
  and return early on the error — never an init-statement that forces the
  success path into an `else`.

## Concurrency & Async

- Async code is kota coroutines (`kota::task`, `co_await`) on kotatsu's
  event loop — no callback style. A public interface may stay synchronous
  and drive a coroutine internally when the caller has no event loop.
- Review coroutine code by interleavings, not function by function: list
  every `co_await` suspension point, the events that can fire while it is
  suspended (a client connecting or disconnecting, a process exiting, a
  pipe closing, cancellation, another coroutine resuming), and trace the
  shared state through each chain; then check that what the frame captured
  before suspending is still valid when it resumes. Bugs here are always
  cross-function interleavings; a function-level read finds none of them.
- Cancellation flows down the ownership tree through `task_group::cancel()`;
  tokens and sources exist only to cross an ownership boundary. kota has no
  shield — a cancel cascades all the way down — so a graceful shutdown is
  a cancellable phase bounded by `with_token`, then a non-cancellable drain
  that joins each task in order.
- `cancellation_token::wait()` completes by cancelling itself. Awaited
  directly inside `when_any` it propagates that cancellation into the parent
  frame and skips everything after the `co_await`; wrap it in `with_token`
  (which catches internally) or add `.catch_cancel()`.

## Logging and Output

- Log through the `LOG_TRACE` / `LOG_DEBUG` / `LOG_INFO` / `LOG_WARN` /
  `LOG_ERROR` / `LOG_CRITICAL` macros of `support/log.h` (`std::format`
  syntax, source location captured automatically; `TRACE` and `DEBUG`
  compile away outside `Debug`). Never call spdlog directly. The Unix hook
  payload logs through its own macros (`hook/unix/debug.h`), which exist
  only in `Debug`.
- stdout and stderr belong to the build under catter and to catter's
  results: diagnostics go to the log. What catter shows the user goes
  through `support/output.h`.

## Naming Conventions

- **Variables, member fields, function names**: `snake_case`. Class member fields do NOT use any special suffix/prefix (no trailing `_`, no `m_` prefix).
- **Class names, template parameter names, enum names**: `PascalCase`. Exception: some types use `snake_case`, among them the plain data shared over IPC (`data::command`, `data::process_result`) — follow the existing style in the file.
- **Enum values**: `PascalCase`. Existing enums in another style (`RequestType::MAKE_DECISION`) keep it.
- **Acronyms stay uppercase in type names**: `OSString`, `JSON` — never `OsString`, `Json`.
- **A file-local type is named by its role in the file**, without the
  words of the file's own subject: in an anonymous namespace, the type
  building the file's answer is `Builder`, not `<Subject>Builder`. A type
  that is the file's subject itself keeps its name (`RemoteHandler` in
  `remote_handler.cc`).
- **Nothing is a `Collector`**: the type producing the file's answer is a
  `Builder`; a visitor gathering nodes is a `<What>Finder`, or is named by
  what it gathers; a callback that only records what it is told is a
  `<What>Recorder`.
- **Overloads, not prefixes, tell one job's variants apart**: every
  conversion to one form is an overload of one name, never one name per
  input type.
- Doc comments on declarations use `///`; the bar for when to write a
  comment at all is in CLAUDE.md.

## Words to Use and Avoid

Identifiers and comments use plain words: the word a newcomer would guess
for the thing, not a metaphor or a rare word that needs a glossary
(verdict, envelope, ledger). One word means one thing across the codebase —
before naming, grep whether the word already means something else nearby,
and don't split one concept over two words. A name that its own doc comment
has to correct is the wrong name.

These were renamed out of clice, catter's sibling; don't bring them here:

| Avoid                              | Say instead                                                        |
| ---------------------------------- | ------------------------------------------------------------------ |
| verdict                            | `result` / `decision` / `outcome`, whichever the site is           |
| claim                              | one word per site: `take`, `acquire`, `reserve`, `cover`, `report` |
| envelope                           | what it is: the message, the request, a container, a header        |
| ledger                             | `table` / `list`                                                   |
| wire (the serialized form)         | `serialized` / `encoded`; "in the protocol" for IPC                |
| settle                             | `finish` / `become idle`                                           |
| flavor                             | `kind` / `variant`                                                 |
| land / landing (a result arriving) | `commit` / `merge` / `finish`                                      |
| sweep                              | `pass` / `run` / `scan`; for cleanup, `remove`                     |
| surface (verb), vouch for          | `report` / `show`, `guarantee` / `confirm`                         |
| collector                          | `Builder` / `<What>Finder` / `<What>Recorder` (see Naming)         |

Domain words with one meaning each — use them only in that meaning:

- **command**: one process the build starts, as catter captured it (cwd,
  executable, arguments, environment) — not a shell command line.
- **handler**: what decides on a session's commands and is told of its
  events (`Handler`): a built-in of `src/feature`, or a
  program catter speaks the handler protocol to (`RemoteHandler`).
- **action**: what a handler decides for a command (`skip`, `drop`,
  `abort`, `modify`, and over the protocol `edit`: `protocol::Action`; in
  `catter::Action` an abort is the exception the handler throws, and an edit
  the modified command). What the
  daemon then tells catter-proxy, or the Windows hook, to do is the IPC
  protocol's `data::action` (`DROP`, `RUN`, `FAIL`); keep the two apart.
- **backend**: a way of capturing a build (`Backend`): the hook, the
  wrapper (shims), eslogger on macOS and ptrace on Linux, which only
  observe the build, or replay of a recorded build.
- **payload**: the hook library loaded into the build's processes.

## Templates, Type Traits & Concepts

- `std::remove_cvref_t<T>` only where `T` can be deduced as a reference:
  a forwarding reference `T&&`. `T`, `T&` and `const T&` parameters,
  class template parameters and return types never deduce one.
- A type trait is a variable template specialized directly
  (`template<> inline constexpr bool is_my_type_v<MyType> = true;`), never
  a variable template wrapping a class template's `::value`.
- A concept over a trait uses `T` as is (`concept MyTrait = is_my_type_v<T>;`)
  unless it deliberately sees through references and cv-qualifiers; the
  caller passes the right type.

## String Literals

- Prefer C++11 raw string literals `R"(...)"` over escaped strings. Avoid `\"`, `\\`, `\n` in string literals when a raw literal is cleaner.

## Style

- Prefer `[[maybe_unused]]` over `(void)` for intentionally unused variables or parameters.
- Arithmetic counters step with `x += 1` / `x -= 1`, in `for` heads too — never `++`/`--`; split fused forms like `if(++x > n)` into two statements. Iterators and other types without `+=` are exempt. This applies to code you write or touch, not as a repository-wide sweep.

## Modern C++ Usage

- Use C++23 APIs whenever possible. Do NOT use `<iostream>` facilities (`std::cout`, `std::cin`, `std::cerr`, etc.) or C-style I/O (`printf`, `fprintf`, etc.); `std::format` and `std::print` instead.
- Prefer `std::ranges` / `std::views` APIs over raw loops and traditional `<algorithm>` calls.
- Use kotatsu for what it covers — paths and files (`kota::fs`), processes, the environment and program search (`kota::sys`), pipes, the event loop, IPC, option parsing, tests — rather than std::filesystem, libuv or the OS directly. std::filesystem is used nowhere, the unit tests included. The hook payloads use `kota::fs`, `kota::sys` and, on Windows, kotatsu's IPC codec alone, which need no event loop.

## Parameter Passing Preferences

- For string parameters, prefer `std::string_view` > `const std::string&`.
- For array/span parameters, prefer `std::span` > `const std::vector&`.
