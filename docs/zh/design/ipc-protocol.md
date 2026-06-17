# IPC 协议

Catter 使用进程间通信在守护进程（`catter`）和代理实例（`catter-proxy`）之间传递信息。守护进程作为服务端，每个代理实例作为客户端连接。

## 传输层

传输层的实现因平台而异：

| 平台 | 机制 | 路径 / 名称 |
|------|------|-------------|
| Linux / macOS | Unix 域套接字 | `$XDG_DATA_HOME/pipe-catter-ipc.sock`（通常为 `~/.local/share/pipe-catter-ipc.sock`） |
| Windows | 命名管道 | `\\.\pipe\catter-ipc` |

守护进程在启动时创建监听套接字/管道。每个 `catter-proxy` 实例启动时作为客户端连接。连接在代理进程的整个生命周期内持续存在。

## 序列化

所有消息使用 [Bincode](https://github.com/bincode-org/bincode) 编码，这是一种紧凑的二进制序列化格式。实现使用 `kota::ipc::BincodePeer`，它在流传输（`kota::ipc::StreamTransport`）之上提供了请求-响应抽象。

选择 Bincode 是因为其高效性 -- 消息体积小、编解码速度快。这一点很重要，因为每个被拦截的构建命令都会导致多次 IPC 往返。

## 请求-响应模型

通信遵循严格的请求-响应模式。客户端（代理）发送请求并等待服务端（守护进程）响应后才继续执行。每种请求类型都有明确定义的参数类型和结果类型。

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

| 字段 | 类型 | 说明 |
|------|------|------|
| **Params** | `ServiceMode`（枚举） | 要检查的模式（当前仅有 `INJECT`） |
| **Result** | `bool` | 如果守护进程处于请求的模式则返回 `true` |

这是代理连接后发送的第一个请求。它确认守护进程已准备好处理被拦截的命令。

### CREATE

在守护进程中注册一个新的进程会话。

| 字段 | 类型 | 说明 |
|------|------|------|
| **Params** | `ipcid_t`（`int32_t`） | 父进程会话 ID |
| **Result** | `ipcid_t`（`int32_t`） | 分配给此代理实例的唯一会话 ID |

父 ID 在会话树中建立父子关系。对于第一个代理（注入模式），父 ID 由守护进程在生成代理时提供。对于包装模式的代理，父 ID 来自 `__key_catter_command_id_v1` 环境变量（Windows 上为 `CATTER_IPC_ID`），由钩子设置。

### MAKE_DECISION

核心请求。代理发送捕获到的命令，守护进程决定如何处理。

**Params** -- `command`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `cwd` | `string` | 被拦截进程的工作目录 |
| `executable` | `string` | 已解析的可执行文件绝对路径 |
| `args` | `string[]` | 完整参数数组（包含 `argv[0]`） |
| `env` | `string[]` | 环境变量（`KEY=VALUE` 格式） |

**Result** -- `action`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `type` | `uint8_t` 枚举 | `DROP`、`INJECT` 或 `WRAP` 之一 |
| `cmd` | `command` | 要执行的命令（可能已被脚本修改） |

**动作类型**：

- **`DROP`（0）** -- 不执行命令。代理立即返回退出码 0。用于脚本判定命令无关紧要的情况（例如用户想跳过的编译器调用）。
- **`INJECT`（1）** -- 挂载钩子库后执行命令。代理重新添加 `LD_PRELOAD`（或在 Windows 上执行 DLL 注入），使此命令的子进程也被拦截。这是需要监控子进程的构建命令的默认动作。
- **`WRAP`（2）** -- 直接执行命令，不挂载钩子。代理运行命令并捕获标准输出/标准错误，但不注入钩子。用于叶子命令（如实际的编译器调用），这些命令不会生成更多的构建子进程。

守护进程可能会修改返回动作中的命令。例如，脚本可以更改编译器标志、重定向输出路径或替换可执行文件。

### REPORT_ERROR

将来自钩子或代理的错误状况报告给守护进程。

**Params**：

| 字段 | 类型 | 说明 |
|------|------|------|
| `parent_id` | `ipcid_t` | 父进程的会话 ID |
| `error_msg` | `string` | 人类可读的错误描述 |

**Result**: `null`（无响应负载）

当钩子遇到无效状态（例如缺少环境变量）或代理在命令处理过程中捕获到异常时使用。守护进程记录错误并可通知用户。

### FINISH

报告命令执行完成。

**Params** -- `process_result`：

| 字段 | 类型 | 说明 |
|------|------|------|
| `code` | `int64_t` | 进程退出码 |
| `std_out` | `string` | 捕获的标准输出 |
| `std_err` | `string` | 捕获的标准错误 |

**Result**: `null`（无响应负载）

守护进程收到此请求后，调用 `onExecution()` JavaScript 回调并传入结果数据。随后代理断开连接。

## 典型消息序列

一个完整的代理生命周期包含如下 IPC 消息序列：

### 注入模式（第一个代理）

```
代理 -> 守护进程:  CHECK_MODE(INJECT)
守护进程 -> 代理:  true
[代理启动挂载了钩子的构建命令]
[代理等待构建完成]
[构建完成后代理退出]
```

注入模式的代理不发送 `CREATE`、`MAKE_DECISION` 或 `FINISH` -- 它只是启动构建并等待。

### 包装模式（被拦截的命令）

```
代理 -> 守护进程:  CHECK_MODE(INJECT)
守护进程 -> 代理:  true
代理 -> 守护进程:  CREATE(parent_id)
守护进程 -> 代理:  new_session_id
代理 -> 守护进程:  MAKE_DECISION(command)
守护进程 -> 代理:  action {type, cmd}
[代理执行或丢弃命令]
代理 -> 守护进程:  FINISH(process_result)
守护进程 -> 代理:  null
[代理断开连接]
```

### 错误情况

```
代理 -> 守护进程:  REPORT_ERROR(parent_id, error_message)
守护进程 -> 代理:  null
[代理以退出码 -1 退出]
```

## 会话模型

守护进程维护一棵与构建进程树对应的会话树：

```
会话 0（根 -- 注入模式代理）
  +-- 会话 1（make -> gcc file1.c）
  +-- 会话 2（make -> gcc file2.c）
  +-- 会话 3（make -> ar rcs libfoo.a file1.o file2.o）
  +-- 会话 4（make -> sh -c "gcc file3.c"）
       +-- 会话 5（sh -> gcc file3.c）
```

每个会话由唯一的 `ipcid_t`（32 位整数）标识。会话树随着 `CREATE` 请求的到达而增量构建，每个请求都指定了 `parent_id`。这种结构支持以下功能：

- **目标树重建** -- 通过了解哪些目标文件链接到哪些目标
- **构建性能分析** -- 通过跟踪每个会话的计时数据并可视化并行度
- **错误归因** -- 通过追溯错误到引发错误的构建命令

## 连接生命周期

1. **守护进程启动** -- 创建并绑定 Unix 域套接字或命名管道。
2. **代理连接** -- 使用 `kota::pipe::connect()` 打开到套接字/管道的连接。
3. **守护进程接受连接** -- 对于每个传入连接，守护进程创建一个 `kota::ipc::BincodePeer` 并为每种请求类型注册请求处理器（`on_request<Request<Type>>(...)`）。
4. **请求处理** -- 守护进程的事件循环将传入请求分发到相应的处理器。处理器可以是异步的（使用 `co_await`）以与 JS 运行时或其他守护进程状态交互。
5. **对端断开** -- 当代理进程退出时，传输层关闭，`BincodePeer::run()` 协程完成。守护进程记录断开事件并清理会话。

## 数据类型

跨 IPC 边界使用的核心数据结构：

```cpp
// IPC 会话的唯一标识符
using ipcid_t = int32_t;

// 从构建系统捕获的命令
struct command {
    std::string cwd;                   // 工作目录
    std::string executable;            // 已解析的可执行文件路径
    std::vector<std::string> args;     // 参数数组
    std::vector<std::string> env;      // 环境变量（KEY=VALUE 条目）
};

// 命令执行结果
struct process_result {
    int64_t code = -1;                 // 退出码
    std::string std_out;               // 捕获的标准输出
    std::string std_err;               // 捕获的标准错误
};

// 守护进程返回的决策
struct action {
    enum : uint8_t {
        DROP,    // 不执行
        INJECT,  // 挂载钩子后执行
        WRAP,    // 不挂载钩子直接执行
    } type;
    command cmd;                       // 可能已修改的命令
};
```
