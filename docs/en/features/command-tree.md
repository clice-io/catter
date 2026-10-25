# Command Tree

The command tree visualizes the build command DAG as an ASCII tree, showing parent-child process relationships and their commands.

Built-in: `cmd-tree`

## Usage

```bash
catter cmd-tree [options] -- <build-command>
```

## Options

| Option                | Description                                                                                        |
| --------------------- | -------------------------------------------------------------------------------------------------- |
| `-d, --depth <n>`     | Limit render depth.                                                                                |
| `-a, --args <n>`      | Number of arguments to show per command. Default: all.                                             |
| `-w, --arg-width <n>` | Cut each longer argument in its middle to this many characters (Unicode code points). Default: 10. |

## Output

The output is an ANSI-colored ASCII tree using box-drawing characters (`│`, `├──`, `└──`). Colors cycle through 4 values by depth level, making it easy to distinguish nesting at a glance.

A typical build tree might look like:

```
make -j8
├── gcc -c main.c -o main.o
│   └── as -o main.o /tmp/cc1.s
├── gcc -c util.c -o util.o
│   └── as -o util.o /tmp/cc2.s
└── gcc main.o util.o -o app
    └── ld -o app main.o util.o -lc
```

Each node shows the process command line, and its children are the processes it spawned. An argument that holds an ASCII blank (a space, tab, line end, vertical tab or form feed), a quote or a backslash shows as a JSON string; `....` stands for the middle of an argument cut to `--arg-width`. The tree is constructed from actual process interception, so it reflects exactly what happened during the build.

## Use Cases

- **Understanding build orchestration** -- See how a build system spawns compilers, assemblers, and linkers.
- **Debugging unexpected process spawning** -- Identify processes you did not expect the build to invoke.
- **Verifying capture completeness** -- Confirm that all expected compilations are being intercepted by catter.
