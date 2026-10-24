# 命令分析

命令分析打印 catter 如何分析一条命令，而不运行它：识别出的编译器或归档工具、编译器的模式和目标平台，以及命令读写的文件。用它可以查明一条命令为什么会（或不会）在编译数据库里生成条目。

内置脚本：`script::inspect`

## 用法

```bash
catter script::inspect -- <command>
```

## 输出

一个 JSON 对象。对编译器：

```bash
catter script::inspect -- gcc -c a.c
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

| 字段                       | 含义                                                                                                             |
| -------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| `analyzer`                 | `compiler`、`archiver`，没有分析器识别该命令时为 `null`，此时 `errors` 给出每个分析器拒绝的原因。                |
| `compiler`、`dialect`      | 匹配到的编译器规则，以及解析命令行所用的语法。                                                                   |
| `phase`、`artifact`        | 命令做什么（`preprocess`、`syntax-only`、`compile`、`link` 等）以及产出什么。                                    |
| `target`                   | 输出文件按哪个目标平台命名，以及它的来源（`argument`、`executable-prefix`、`driver-default`、`host-fallback`）。 |
| `sources`                  | 会在编译数据库中生成条目的源文件。                                                                               |
| `reads`、`writes`、`edges` | 命令读写的文件，以及哪个输出由哪些输入生成（即目标树的边）。                                                     |
| `inferred`                 | 命令没有写出、按编译器默认规则推断出的输出。                                                                     |
| `diagnostics`              | 分析无法确定的地方，例如推断不出的输出。                                                                         |
| `entries`                  | `script::cdb` 为这条命令写入的编译数据库条目。                                                                   |
