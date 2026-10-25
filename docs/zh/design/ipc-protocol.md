# IPC 协议（inject runtime 实现）

> 本文描述 catter 守护进程与 `catter-proxy` 之间的通信协议。它是所有 runtime 共用的传输层，目前由 inject runtime 使用；handler 作者通常不需要阅读本文。

Catter 使用进程间通信在守护进程（`catter`）和代理实例（`catter-proxy`）之间传递信息。守护进程作为服务端，每个代理实例作为客户端连接。

## 传输层

传输层的实现因平台而异：

| 平台          | 机制          | 路径 / 名称                 |
| ------------- | ------------- | --------------------------- |
| Linux / macOS | Unix 域套接字 | `<data dir>/ipc-<pid>.sock` |
| Windows       | 命名管道      | `\\.\pipe\catter-ipc-<pid>` |

每个守护进程监听自己的套接字/管道，以其进程 ID 命名，因此同时在 catter 下运行的多个构建互不干扰。数据目录为 `$HOME/.catter`（Linux、macOS）或 `%LOCALAPPDATA%\catter`（Windows）；设置了 `CATTER_HOME` 时使用它。守护进程在启动时创建监听套接字/管道，并通过命令行（`--ipc <pipe>`）把它的名称传给每个 `catter-proxy` 实例；每个实例启动时作为客户端连接。连接在代理进程的整个生命周期内持续存在。

程序可能比启动它的会话活得更久：构建服务进程（sccache 服务、Gradle 守护进程）会带着早先会话的管道名为之后的构建运行命令。那个管道已经不存在时，`catter-proxy` 像没有 catter 时一样运行该命令，不捕获，并记录一条警告；在会话结束后才结束的命令保留它的退出码。

## 序列化

所有消息使用 [Bincode](https://github.com/bincode-org/bincode) 编码，这是一种紧凑的二进制序列化格式。实现使用 `kota::ipc::BincodePeer`，它在流传输（`kota::ipc::StreamTransport`）之上提供了请求 - 响应抽象。

选择 Bincode 是因为其高效性 -- 消息体积小、编解码速度快。这一点很重要，因为每个被拦截的构建命令都会导致多次 IPC 往返。

## 请求 - 响应模型

通信遵循严格的请求 - 响应模式。客户端（代理）发送请求并等待服务端（守护进程）响应后才继续执行。每种请求类型都有明确定义的参数类型和结果类型。

协议在 `src/common/util/data.h` 中使用 C++ 模板特化定义：

```cpp
enum class RequestType : uint8_t {
    CHECK_MODE,
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
};
```

每种请求类型映射到一个 `Request<Type>` 特化，声明 `Params`（请求负载）和 `Result`（响应负载）。

## 请求类型

### CHECK_MODE

查询守护进程当前活动的服务模式。

| 字段       | 类型                  | 说明                                    |
| ---------- | --------------------- | --------------------------------------- |
| **Params** | `ServiceMode`（枚举） | 要检查的模式（当前仅有 `INJECT`）       |
| **Result** | `bool`                | 如果守护进程处于请求的模式则返回 `true` |

这是代理连接后发送的第一个请求。它确认守护进程已准备好处理被拦截的命令。

### CREATE

在守护进程中注册一个新的进程会话。

| 字段       | 类型                   | 说明                          |
| ---------- | ---------------------- | ----------------------------- |
| **Params** | `ipcid_t`（`int32_t`） | 父进程会话 ID                 |
| **Result** | `ipcid_t`（`int32_t`） | 分配给此代理实例的唯一会话 ID |

父 ID 在会话树中建立父子关系。对于第一个代理（注入模式），父 ID 由守护进程在生成代理时提供。对于包装模式的代理，父 ID 来自 `__key_catter_command_id_v1` 环境变量（Windows 上为 `CATTER_IPC_ID`），由钩子设置。

### MAKE_DECISION

核心请求。代理发送捕获到的命令，守护进程决定如何处理。

**Params** -- `command`：

| 字段         | 类型       | 说明                           |
| ------------ | ---------- | ------------------------------ |
| `cwd`        | `string`   | 被拦截进程的工作目录           |
| `executable` | `string`   | 已解析的可执行文件绝对路径     |
| `args`       | `string[]` | 完整参数数组（包含 `argv[0]`） |
| `env`        | `string[]` | 环境变量（`KEY=VALUE` 格式）   |

**Result** -- `action`：

| 字段   | 类型           | 说明                                      |
| ------ | -------------- | ----------------------------------------- |
| `type` | `uint8_t` 枚举 | `DROP`、`INJECT`、`WRAP` 或 `FAIL` 之一   |
| `cmd`  | `command`      | 要执行的命令（可能已被某个 handler 修改） |

**动作类型**：

- **`DROP`（0）** -- 不执行命令。代理立即返回退出码 0。用于 handler 判定命令无关紧要的情况（例如用户想跳过的编译器调用）。
- **`INJECT`（1）** -- 挂载钩子库后执行命令。代理重新添加 `LD_PRELOAD`（或在 Windows 上执行 DLL 注入），使此命令的子进程也被拦截。这是需要监控子进程的构建命令的默认动作。
- **`WRAP`（2）** -- 直接执行命令，不挂载钩子。代理运行命令，但不注入钩子。用于叶子命令（如实际的编译器调用），这些命令不会生成更多的构建子进程。
- **`FAIL`（3）** -- 不执行命令：代理立即返回退出码 1。会话已因错误结束（某个 handler 中止了构建，或者自身出错），此后启动的每条命令都会这样失败，直到构建结束。

守护进程可能会修改返回动作中的命令。例如，handler 可以更改编译器标志、重定向输出路径或替换可执行文件。

> 这里的动作是**协议层**概念，与 handler 决定的 `skip` / `drop` / `abort` / `modify` 不是同一层：handler 的动作由 runtime 驱动映射为协议动作。映射关系与语义见[系统架构](architecture.md)的 Action 一节。

### REPORT_ERROR

将来自钩子或代理的错误状况报告给守护进程。

**Params**：

| 字段        | 类型      | 说明               |
| ----------- | --------- | ------------------ |
| `parent_id` | `ipcid_t` | 父进程的会话 ID    |
| `error_msg` | `string`  | 人类可读的错误描述 |

**Result**: `null`（无响应负载）

当钩子遇到无效状态（例如缺少环境变量）或代理在命令处理过程中捕获到异常时使用。守护进程记录错误并可通知用户。

### FINISH

报告命令执行完成。

**Params** -- `process_result`：

| 字段   | 类型      | 说明       |
| ------ | --------- | ---------- |
| `code` | `int64_t` | 进程退出码 |

**Result**: `null`（无响应负载）

守护进程收到此请求后，把结果告知会话的各个 handler（即 handler 协议中的 `exit`）。随后代理断开连接。

## 典型消息序列

一个完整的代理生命周期包含如下 IPC 消息序列：

### 注入模式（第一个代理）

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
[Proxy launches build command with hook attached]
[Proxy waits for build to complete]
[Proxy exits when build finishes]
```

注入模式的代理同样会为顶层构建命令发送 `CREATE`、`MAKE_DECISION` 和 `FINISH`，因此构建系统命令本身也会像其他被拦截的命令一样送达各个 handler。

### 包装模式（被拦截的命令）

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
Proxy -> Daemon:  CREATE(parent_id)
Daemon -> Proxy:  new_session_id
Proxy -> Daemon:  MAKE_DECISION(command)
Daemon -> Proxy:  action {type, cmd}
[Proxy executes or drops the command]
Proxy -> Daemon:  FINISH(process_result)
Daemon -> Proxy:  null
[Proxy disconnects]
```

### 错误情况

```
Proxy -> Daemon:  REPORT_ERROR(parent_id, error_message)
Daemon -> Proxy:  null
[Proxy exits with code -1]
```

## 会话模型

守护进程维护一棵与构建进程树对应的会话树：

```
Session 0 (root -- injector proxy)
  +-- Session 1 (make -> gcc file1.c)
  +-- Session 2 (make -> gcc file2.c)
  +-- Session 3 (make -> ar rcs libfoo.a file1.o file2.o)
  +-- Session 4 (make -> sh -c "gcc file3.c")
       +-- Session 5 (sh -> gcc file3.c)
```

每个会话由唯一的 `ipcid_t`（32 位整数）标识。会话树随着 `CREATE` 请求的到达而增量构建，每个请求都指定了 `parent_id`。这种结构支持以下功能：

- **目标树重建** -- 通过了解哪些目标文件链接到哪些目标
- **构建分析** -- 通过跟踪每个会话的计时数据并可视化并行度
- **错误归因** -- 通过追溯错误到引发错误的构建命令

## 连接生命周期

1. **守护进程启动** -- 创建并绑定 Unix 域套接字或命名管道。
2. **代理连接** -- 使用 `kota::pipe::connect()` 打开到套接字/管道的连接。
3. **守护进程接受连接** -- 对于每个传入连接，守护进程创建一个 `kota::ipc::BincodePeer` 并为每种请求类型注册请求处理器（`on_request<Request<Type>>(...)`）。
4. **请求处理** -- 守护进程的事件循环将传入请求分发到相应的处理器。请求处理器可以是异步的（使用 `co_await`），以等待会话的各个 handler 或其他守护进程状态。
5. **对端断开** -- 当代理进程退出时，传输层关闭，`BincodePeer::run()` 协程完成。守护进程记录断开事件并清理会话。

## 数据类型

跨 IPC 边界使用的核心数据结构：

```cpp
// Unique identifier for an IPC session
using ipcid_t = int32_t;

// Captured command from the build system
struct command {
    std::string cwd;                   // Working directory
    std::string executable;            // Resolved executable path
    std::vector<std::string> args;     // Argument array
    std::vector<std::string> env;      // Environment (KEY=VALUE entries)
};

// Result of executing a command
struct process_result {
    int64_t code = -1;                 // Exit code
};

// Decision returned by the daemon
struct action {
    enum : uint8_t {
        DROP,    // Do not execute
        INJECT,  // Execute with hook attached
        WRAP,    // Execute without hook
    } type;
    command cmd;                       // Possibly modified command
};
```
