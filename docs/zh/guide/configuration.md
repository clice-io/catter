# 配置参考

## catter 命令行

```
catter <command> [options] -- <build-command>
```

每次运行都要指定一个命令，选项写在命令之后。`catter --help` 列出所有命令，`catter <command> --help` 显示某个命令的选项。不指定命令时，catter 列出所有命令并报错退出。

### 命令

| 命令               | 说明                                                                                                         |
| ------------------ | ------------------------------------------------------------------------------------------------------------ |
| `compile-commands` | 根据捕获的编译器命令生成 `compile_commands.json` 文件。见[编译数据库](../features/compilation-database.md)。 |
| `tree`             | 把捕获的命令渲染为命令树。见[命令树](../features/command-tree.md)。                                          |
| `targets`          | 把捕获的构建目标渲染为扁平的目标森林。见[目标树](../features/target-tree.md)。                               |
| `explain`          | 打印 catter 如何分析 `--` 之后的命令，而不运行它。见[命令分析](../features/explain.md)。                     |
| `run`              | 只用 handler 运行构建，由它们对构建的命令做决定或观察它们。见 [Handler](../scripting/handlers.md)。          |

`compile-commands`、`tree` 和 `targets` 各自在构建上运行一个内置功能，命令行指定的 [handler](../scripting/handlers.md)（如果有的话）与它一起运行。`run` 只运行 handler，至少要指定一个。

### 选项

`compile-commands`、`tree`、`targets` 和 `run` 除了各自的选项，还接受以下选项：

| 选项                          | 说明                                                                                     | 默认值   |
| ----------------------------- | ---------------------------------------------------------------------------------------- | -------- |
| `-C, --directory <path>`      | 命令运行所在的目录。                                                                     | 当前目录 |
| `--capture <backend>`         | 构建的命令如何被捕获：`hook` 或 `wrapper`。见[捕获后端](../design/capture-backends.md)。 | `hook`   |
| `--from <file>`               | 回放该文件中录制的构建，而不运行构建。                                                   |          |
| `--handler <command>`         | 启动一个 handler，它经自己的 stdin 和 stdout 对构建的命令做决定或观察它们；可多次指定。  |          |
| `--connect <host:port>`       | 连接在该地址监听的 handler，未指定 `--allow-remote` 时只限本机；可多次指定。             |          |
| `--allow-remote`              | 允许 `--connect` 连接其他机器上的 handler。                                              |          |
| `--handler-timeout <seconds>` | catter 等待 handler 应答、以及会话结束后等待它退出的时长；`0` 表示一直等下去。           | `60`     |

`--from` 代替 `--` 之后的构建命令：一次运行只能给出两者之一。相对路径从 `-C` 指定的目录算起。录制的构建不经过捕获：`--from` 不接受 `--capture`。

`explain` 也接受 `-C`，要分析的命令写在 `--` 之后。

### 退出状态

catter 像构建那样结束：以构建的退出码退出；构建被信号结束时，catter 也以同一个信号结束。重放的构建以其文件记录的退出码结束。catter 自身失败时，比如命令行有误、handler 中止了构建或编译数据库无法写入，catter 打印原因并以 1 退出。

停止 catter 就会停止构建：发给 catter 的 SIGTERM、SIGINT 或 SIGHUP 会转给构建命令，构建命令像直接收到它一样结束，catter 随后像构建那样结束。catter 被杀死时，构建命令随之结束。

### 数据目录

catter 把日志和每个运行中会话的套接字放在 `$HOME/.catter`（Linux、macOS）或 `%LOCALAPPDATA%\catter`（Windows）。设置了环境变量 `CATTER_HOME` 时，改用它指定的目录（须为绝对路径）。在 Linux 和 macOS 上，构建中的进程把日志写到它们所属会话的目录，不管构建步骤给它们设置了什么 `HOME` 或 `CATTER_HOME`。

## catter-proxy 命令行

::: warning
`catter-proxy` 是 catter 的内部组件，由 catter 自动调用，不需要用户直接使用。
:::

```
catter-proxy [options] -- <command>
```

| 选项           | 说明                     |
| -------------- | ------------------------ |
| `-p <id>`      | 用于 IPC 会话的父进程 ID |
| `<executable>` | 已解析的可执行文件路径   |

## 环境变量

以下环境变量由 catter 在运行时自动设置，此处仅作参考，无需手动配置。

### Unix（Linux / macOS）

| 变量                             | 用途                            |
| -------------------------------- | ------------------------------- |
| `__key_catter_proxy_path_v1`     | `catter-proxy` 可执行文件的路径 |
| `__key_catter_command_id_v1`     | IPC 命令标识符                  |
| `LD_PRELOAD`（Linux）            | 注入 catter 钩子共享库          |
| `DYLD_INSERT_LIBRARIES`（macOS） | 注入 catter 钩子共享库          |

### Windows

catter 不设置任何环境变量：它直接把会话交给构建中每个进程里的钩子，构建的环境保持构建自己设置的样子。
