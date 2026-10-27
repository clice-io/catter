# Handlers

A handler is a program that catter tells about the commands a build starts. A handler that **decides** answers each command before it runs: run it as it is, run another command in its place, drop it, or abort the build. A handler that does not decide only **observes**: it is told, and the build never waits for it.

Under a [capture backend](../design/capture-backends.md) that only observes the build, `eslogger`, each command runs before catter hears of it, and catter refuses a handler that decides before the build starts. The session's start tells (`decisions`): a handler that can do without deciding sets `decides` there.

catter speaks to a handler over a small protocol (see [Handler protocol](../design/handler-protocol.md)), so a handler can be written in any language. The SDKs for Python and TypeScript do the protocol for you.

## Running a handler

```bash
catter run --handler "python handler.py" -- make
```

catter starts the handler, speaks to it over the handler's stdin and stdout, and ends it when the build is over. What the handler prints on stderr shows in the terminal. A Ctrl+C there stops the build, not the handler, which is told how the build ended. The handler runs outside the build: the programs it starts are not captured.

On Linux and macOS the command is split into words as a POSIX shell splits them, without expanding anything; on Windows it goes to the program as it is, as a Windows command line does.

A handler that already runs, listening on a TCP port, is reached with `--connect`:

```bash
catter run --connect 127.0.0.1:9000 -- make
```

This suits a handler run under a debugger, or one that serves several builds in turn. The host is an IP address, or `localhost`, for which catter tries `127.0.0.1`, then `::1`. catter connects only to the machine it runs on, unless `--allow-remote` is given: a handler can change and drop the build's commands.

### Options

| Option                        | Description                                                                                                                   | Default |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------------------- | ------- |
| `--handler <command>`         | Start a handler, which decides or observes the build's commands over its stdin and stdout; may be given more than once.       |         |
| `--connect <host:port>`       | Connect to a handler listening at the address, on this machine unless `--allow-remote` is given; may be given more than once. |         |
| `--allow-remote`              | Let `--connect` reach a handler on another machine.                                                                           |         |
| `--handler-timeout <seconds>` | How long catter waits for a handler to answer, and to exit once the session is over; `0` waits as long as it takes.           | `60`    |

`run` runs the build with handlers alone. `compile-commands`, `tree` and `targets` take these options too, and run the handlers after their built-in.

## The SDKs

The SDKs are not published yet: they are in catter's repository, under `sdk/`, and run from a checkout of it. A Python handler finds the `catter` package once `sdk/python` is on `PYTHONPATH`, and needs Python 3.10 or later. A TypeScript handler imports `@catter/sdk` through a symbolic link to `sdk/typescript` at `node_modules/@catter/sdk`, once `pixi run -e dev npm-install` has installed the SDK's own dependencies, and needs Node.js 22.18 or later, which runs TypeScript as it is.

## Writing a handler in Python

The Python SDK uses the standard library alone. A handler subclasses `catter.Handler` and overrides the callbacks it needs:

```python
import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "-O0" in command.args:
            return catter.Modify(args=[*command.args, "-g"])
        if command.args[0].endswith("ranlib"):
            return catter.Drop()
        return None  # run it as it is


catter.serve(Handler())
```

| Callback              | When                                                                                                   |
| --------------------- | ------------------------------------------------------------------------------------------------------ |
| `on_start(session)`   | The session starts: its build command and working directory, and how commands are captured.            |
| `on_command(command)` | A command is about to run, or under `eslogger` has started. A handler that decides returns its action. |
| `on_error(error)`     | catter could not capture a command.                                                                    |
| `on_exit(exit)`       | A command has run: its exit code and how long it took.                                                 |
| `on_finish(finish)`   | The build has ended, with its exit code.                                                               |

A handler is told only of the events whose callback it overrides. `env = True` makes commands carry their environment.

The actions are `catter.Skip()` (the same as returning `None`), `catter.Drop()` (the command does not run, and exits 0), `catter.Abort(reason=...)`, and `catter.Modify(args=..., executable=..., cwd=..., env=...)`, which needs `args`: the other fields left out keep the command's own.

Once `catter` is imported, what the handler prints goes to stderr: its stdout carries the protocol. Once `serve()` runs, the handler and the programs it starts read the null device as stdin, which carries the protocol too. Arguments, paths and environment entries are strings; bytes that are not UTF-8 appear as lone surrogates, as `os.fsdecode` spells them, and reach the command unchanged when handed back.

To serve catter's `--connect`, listen on a port and serve the session of each catter that connects:

```python
with catter.listen(port=9000) as listener:
    while True:
        listener.accept(Handler())
```

## Writing a handler in TypeScript

The TypeScript SDK, `@catter/sdk`, runs on Node.js:

```ts
import { serve } from "@catter/sdk";

await serve({
  decides: true,
  onCommand(command) {
    if (command.args.includes("-O0")) {
      return { action: "modify", args: [...command.args, "-g"] };
    }
    return undefined; // run it as it is
  },
});
```

The callbacks are `onStart`, `onCommand`, `onError`, `onExit` and `onFinish`, as in Python. An argument that is not UTF-8 comes as `{ base64 }`, which a command handed back keeps. For `--connect`, `listen(port)` returns a listener, whose `accept(handler)` serves the session of the next catter that connects.

Once `@catter/sdk` is imported, what the handler prints with `console` goes to stderr. Its stdin and stdout carry the protocol: a handler started by catter must not read `process.stdin` nor write to `process.stdout`, nor let a program it starts inherit either.

## What catter makes of a command

catter reads the commands of compilers (gcc, clang, clang-cl, MSVC's cl), of `ar`, and the arguments of linkers and librarians (lld and the linkers it stands in for, `llvm-lib`, `llvm-dlltool`), as the [protocol](../design/handler-protocol.md) lists. A handler can be told what catter makes of each command, be told of only some commands, and edit a command's arguments, without parsing a command line itself:

```python
import catter


class Handler(catter.Handler):
    decides = True
    analysis = True  # commands carry command.analysis
    filter = [catter.Pattern(kind="compiler", phases=["compile"])]

    def on_command(self, command):
        sources = [
            read.path for read in command.analysis.reads if read.role == "source"
        ]
        print(f"compiles {sources}")
        return catter.Edit(
            edits=[
                catter.CommandEdit("remove", ["-Werror"]),
                catter.CommandEdit("append", ["-DTRACED=1"]),
            ]
        )


catter.serve(Handler())
```

- **The analysis** of a compiler's command has its phase, its target, every argument (the option it is, and as the command writes it), the files it reads, each source in its language, and the files it writes. The arguments of the response files (`@file`) a compiler's command names are read in, before the command runs.
- **A filter** lists patterns: of a kind of program (`compiler`, `archiver`, `tool`), of some names, of some phases. The handler is told only of the commands a pattern names; the build does not wait for it on the others.
- **An edit** removes flags, by option and value (`-DX`, `-D*`, `-W*`), appends flags after the command's own, or prepends them after the program. The arguments it does not remove stay as the command wrote them, a compiler's response file too unless the edit removes an argument it holds or adds flags among them, and the flags it adds go as the handler wrote them: write them as the command's compiler takes them. Only a command whose arguments catter reads can be edited; `modify` replaces any command whole.
- **`self.analyze(catter.Program(executable, args, cwd), ...)`** asks what catter makes of commands the handler names itself, `cwd` the directory a command's relative response files are read from. In TypeScript, `catter.analyze(...)` through every callback's second argument.

## Several handlers

`--handler` and `--connect` may be given more than once, and to a command that runs a built-in:

```bash
catter compile-commands -o compile_commands.json --handler "python observe.py" --handler "python decide.py" -- make
```

The handlers form a chain, in a fixed order: the built-in first, then the handlers in the order of the command line. Each command goes to them in turn, each seeing it as the ones before left it, until one drops it; a handler after it never sees a dropped command. Put an observer that should see every command first.

## When a handler fails

A handler that aborts the build, raises in a callback, exits, writes what the protocol does not allow, or does not answer within `--handler-timeout` ends the session: the command it was deciding, and every command the build starts after it, fail without running (exit code 1). A command that has started runs on: under a backend that only observes the build (`eslogger`), every command has started by the time catter hears of it, and the build runs on to its end. Once the build has ended, catter ends the handlers' input without a `finish` (`on_finish` does not run), prints what happened and exits 1:

```
Handler `python handler.py` did not answer command 12 within 60 s
```

A handler started by catter that does not exit within the timeout once the session is over is ended. The build never waits for an observer, but one that falls behind must catch up within the timeout once the build has ended: that is when it answers `finish`.
