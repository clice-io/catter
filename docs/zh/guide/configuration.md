# 配置参考

## catter 命令行

```
catter [选项] <脚本> [脚本参数] -- <构建命令>
```

### 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `-m, --mode <mode>` | 运行模式，控制 catter 拦截进程的方式。 | `inject` |
| `-d, --dir <path>` | 目标进程的工作目录。 | 当前目录 |
| `--stdio-mode <mode>` | 子进程标准输入输出的处理方式，见下文。 | `inherit` |
| `-h, --help` | 显示帮助信息。 | |

### `--stdio-mode`

控制被拦截进程的标准输出和标准错误流的处理方式：

- **`inherit`** -- 实时透传。构建输出会像正常一样显示在终端中。
- **`capture`** -- 缓冲 stdout 和 stderr。捕获的输出会传递给脚本的 `onFinish` 回调，而不是立即打印。

### 脚本指定

**内置脚本**使用 `script::` 前缀：

```bash
catter script::<名称> [脚本参数] -- <构建命令>
```

可用的内置脚本：

| 名称 | 说明 |
|------|------|
| `script::cdb` | 生成 `compile_commands.json` |
| `script::cmd-tree` | 展示构建命令树 |
| `script::target-tree` | 展示构建目标树 |

**自定义脚本**使用文件路径：

```bash
catter ./path/to/script.js [脚本参数] -- <构建命令>
```

## catter-proxy 命令行

::: warning
`catter-proxy` 是内部组件，由 catter 自动调用，不需要用户直接使用。
:::

```
catter-proxy [选项] -- <命令>
```

| 选项 | 说明 |
|------|------|
| `-p <id>` | 用于 IPC 会话的父进程 ID |
| `<executable>` | 已解析的可执行文件路径 |

## 环境变量

以下环境变量由 catter 在运行时自动设置，此处仅作参考，无需手动配置。

### Unix（Linux / macOS）

| 变量 | 用途 |
|------|------|
| `__key_catter_proxy_path_v1` | `catter-proxy` 可执行文件的路径 |
| `__key_catter_command_id_v1` | IPC 命令标识符 |
| `LD_PRELOAD`（Linux） | 注入 catter 钩子共享库 |
| `DYLD_INSERT_LIBRARIES`（macOS） | 注入 catter 钩子共享库 |

### Windows

| 变量 | 用途 |
|------|------|
| `CATTER_IPC_ID` | IPC 会话标识符 |
| `CATTER_PROXY_PATH` | `catter-proxy` 可执行文件的路径 |
