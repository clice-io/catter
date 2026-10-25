# Inspect

Inspect prints how catter analyzes one command, without running it: which compiler or archiver it recognizes, the compiler's mode and target, and the files the command reads and writes. It answers why a command gets, or does not get, an entry in the compilation database.

Built-in: `inspect`

## Usage

```bash
catter inspect -- <command>
```

## Output

A JSON object. For a compiler:

```bash
catter inspect -- gcc -c a.c
```

```json
{
  "analyzer": "compiler",
  "compiler": "builtin:gcc",
  "dialect": "gcc",
  "phase": "compile",
  "artifact": "object",
  "target": {
    "os": "linux",
    "objectFormat": "elf",
    "artifactModel": "elf",
    "from": "host-fallback"
  },
  "sources": ["a.c"],
  "reads": ["a.c"],
  "writes": ["a.o"],
  "edges": [{ "output": "a.o", "inputs": ["a.c"] }],
  "inferred": [{ "path": "a.o", "reason": "default-output" }],
  "entries": [
    {
      "directory": "/work",
      "file": "a.c",
      "arguments": ["gcc", "-c", "a.c"],
      "output": "/work/a.o"
    }
  ]
}
```

| Field                      | Meaning                                                                                                                            |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------------------------- |
| `analyzer`                 | `compiler`, `archiver`, or `null` when no analyzer recognizes the command; `errors` then says why each declined.                   |
| `compiler`, `dialect`      | The compiler rule that matched, and the command-line syntax it is parsed with.                                                     |
| `phase`, `artifact`        | What the command does (`preprocess`, `syntax-only`, `compile`, `link`, …) and what it produces.                                    |
| `target`                   | The target the outputs are named for, and where it came from (`argument`, `executable-prefix`, `driver-default`, `host-fallback`). |
| `sources`                  | The sources that get compilation database entries.                                                                                 |
| `reads`, `writes`, `edges` | The files the command reads and writes, and which output is made from which inputs (the target tree's edges).                      |
| `inferred`                 | Outputs the command does not name, inferred from the compiler's defaults.                                                          |
| `diagnostics`              | What the analysis could not settle, such as an output it cannot infer.                                                             |
| `entries`                  | The compilation database entries `cdb` writes for this command.                                                                    |
