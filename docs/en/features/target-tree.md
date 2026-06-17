# Target Tree

The target tree renders build artifacts as a dependency forest, showing what was built from what.

Built-in script: `script::target-tree`

## Usage

```bash
catter script::target-tree [options] -- <build-command>
```

## Options

| Option | Description |
|--------|-------------|
| `-d, --depth <n>` | Limit render depth. |

## Output

The target tree inverts the perspective of the command tree. Instead of showing process parent-child relationships, it shows output artifacts (executables, libraries, object files) as parents, with their input files as children.

For example, where the command tree shows `gcc main.o util.o -o app`, the target tree shows:

```
app
├── main.o
│   └── main.c
├── util.o
│   └── util.c
```

This makes it clear which source files contribute to which final artifacts.

## Use Cases

- **Understanding target dependencies** -- See the full dependency chain from final binaries down to source files.
- **Analyzing build graphs for C++20 modules** -- Trace module interface units and their consumers.
- **Identifying source contributions** -- Determine which sources contribute to which targets, useful for splitting or restructuring builds.
