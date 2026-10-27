# IPC 协议（inject runtime 实现）

> 本文描述 catter 守护进程与向它报告构建命令的一方之间的通信协议：Linux 和 macOS 上是 `catter-proxy`，Windows 上是钩子本身。它是所有 runtime 共用的传输层，目前由 inject runtime 使用；handler 作者通常不需要阅读本文。

Catter 使用进程间通信在守护进程（`catter`）和它的客户端之间传递信息：Linux 和 macOS 上是每个 `catter-proxy` 实例，每个实例代替一条命令；Windows 上是钩子，它在启动一条命令的那次 `CreateProcess` 调用内部通信。守护进程作为服务端，每个客户端为每条命令连接一次。

## 传输层

传输层的实现因平台而异：

| 平台          | 机制          | 路径 / 名称                 |
| ------------- | ------------- | --------------------------- |
| Linux / macOS | Unix 域套接字 | `<data dir>/ipc-<pid>.sock` |
| Windows       | 命名管道      | `\\.\pipe\catter-ipc-<pid>` |

每个守护进程监听自己的套接字/管道，以其进程 ID 命名，因此同时在 catter 下运行的多个构建互不干扰。数据目录为 `$HOME/.catter`（Linux、macOS）或 `%LOCALAPPDATA%\catter`（Windows）；设置了 `CATTER_HOME` 时使用它。守护进程在启动时创建监听套接字/管道。在 Linux 和 macOS 上，它通过命令行（`--ipc <pipe>`）把管道名称传给每个 `catter-proxy` 实例；每个实例启动时作为客户端连接，连接在代理进程的整个生命周期内持续存在。在 Windows 上，守护进程把管道名称交给它注入钩子的每个进程中的钩子；钩子为它启动的每个程序连接一次，程序启动后即断开。

程序可能比启动它的会话活得更久：构建服务进程（sccache 服务、Gradle 守护进程）会带着环境中早先会话的管道名（Windows 上则是带着它的钩子）为之后的构建运行命令。那个管道已经不存在时，命令像没有 catter 时一样运行，不捕获；`catter-proxy` 会记录一条警告。在会话结束后才结束的命令保留它的退出码。代理读不到工作目录的命令（比如构建删除了它的工作目录）照样在那里运行，不捕获，代理把它作为错误报告（`REPORT_ERROR`）。

`catter-proxy` 代替它启动的程序站在构建与程序之间，构建看到的代理的结束就是程序的结束：被信号结束的程序，代理也以同一个信号结束。程序启动时的信号状态与 exec 留下的一致：构建忽略的信号仍被忽略，屏蔽的仍被屏蔽。构建关闭的标准流对程序仍是关闭的：代理打开的任何东西都不会占用它的位置。其他进程发给代理的 SIGTERM、SIGINT 或 SIGHUP 会转给程序；终端发出的信号本来就会到达程序。代理被杀死时，程序随之结束：Linux 上由系统结束它；macOS 没有办法把两者绑在一起，由守护进程结束它——代理通过 `started` 告诉守护进程程序的进程 ID，代理在发送 `FINISH` 之前断开时，守护进程就杀死该程序。

守护进程以同样的方式代替构建：它像代理启动程序那样启动构建命令的代理，并像构建那样结束。发给守护进程的 SIGTERM、SIGINT 或 SIGHUP 会转给构建命令；守护进程被杀死时，构建命令的代理随之结束：在 macOS 上，这个代理与守护进程的连接在发送 `FINISH` 之前断开时，由它自己结束它的程序。

Windows 上没有代理：守护进程自己决定如何处理构建命令，并带着钩子启动它；构建启动的每个程序则由钩子启动。调用方拿到的是程序自己的进程，与没有 catter 时一样，守护进程则等待该进程，以取得它的退出码。守护进程代替构建命令站在前面：Ctrl+C 和 Ctrl+Break 会在两者共用的控制台中到达构建，守护进程等待构建结束；守护进程被杀死时，构建命令随之结束。

## 序列化

所有消息使用 [Bincode](https://github.com/bincode-org/bincode) 编码，这是一种紧凑的二进制序列化格式，每条消息都以一个 `Content-Length` 头分帧。守护进程和 `catter-proxy` 使用 `kota::ipc::BincodePeer`，它在流传输（`kota::ipc::StreamTransport`）之上提供了请求 - 响应抽象；Windows 钩子运行在构建的进程中，没有事件循环，它用 kotatsu 的分帧和 Bincode 编解码器自己读写同样的帧，逐个等待答复。

选择 Bincode 是因为其高效性 -- 消息体积小、编解码速度快。这一点很重要，因为每个被拦截的构建命令都会导致多次 IPC 往返。

## 请求 - 响应模型

除 macOS 上作为通知发送的 `started` 外，通信遵循严格的请求 - 响应模式。客户端发送请求并等待服务端（守护进程）响应后才继续执行。每种请求类型都有明确定义的参数类型和结果类型。

协议在 `src/support/messages.h` 中使用 C++ 模板特化定义，守护进程、`catter-proxy` 和 Windows 钩子共用这个文件：

```cpp
enum class RequestType : uint8_t {
    CHECK_MODE,
    CREATE,
    MAKE_DECISION,
    REPORT_ERROR,
    FINISH,
    STARTED,
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

这是代理连接后发送的第一个请求。它确认守护进程已准备好处理被拦截的命令。Windows 钩子不发送它。

### CREATE

在守护进程中注册一个新的进程会话。

| 字段       | 类型                   | 说明                        |
| ---------- | ---------------------- | --------------------------- |
| **Params** | `ipcid_t`（`int32_t`） | 父进程会话 ID               |
| **Result** | `ipcid_t`（`int32_t`） | 分配给这条命令的唯一会话 ID |

父 ID 在会话树中建立父子关系。对于第一个代理（注入模式），父 ID 由守护进程在生成代理时提供。对于包装模式的代理，父 ID 来自 `__key_catter_command_id_v1` 环境变量，由钩子设置。在 Windows 上，钩子发送其所在进程运行的命令的 ID，这个 ID 由守护进程交给它。

### MAKE_DECISION

核心请求。客户端发送捕获到的命令，守护进程决定如何处理。

**Params** -- `command`：

| 字段         | 类型       | 说明                           |
| ------------ | ---------- | ------------------------------ |
| `cwd`        | `string`   | 被拦截进程的工作目录           |
| `executable` | `string`   | 已解析的可执行文件绝对路径     |
| `args`       | `string[]` | 完整参数数组（包含 `argv[0]`） |
| `env`        | `string[]` | 环境变量（`KEY=VALUE` 格式）   |

**Result** -- `action`：

| 字段   | 类型           | 说明                                                  |
| ------ | -------------- | ----------------------------------------------------- |
| `type` | `uint8_t` 枚举 | `DROP`、`INJECT` 或 `FAIL` 之一                       |
| `cmd`  | `command`      | 要执行的命令：即发来的命令，除非某个 handler 修改了它 |

**动作类型**：

- **`DROP`（0）** -- 不执行命令：它以退出码 0 结束。用于 handler 判定命令无关紧要的情况（例如用户想跳过的编译器调用）。代理立即返回退出码 0；在 Windows 上，守护进程把钩子注入钩子以挂起状态启动的程序，并让钩子一加载就以退出码 0 结束进程，赶在程序自身的代码运行之前。
- **`INJECT`（1）** -- 挂载钩子库后执行命令。代理重新添加 `LD_PRELOAD`，在 Windows 上则由守护进程把钩子 DLL 注入钩子启动的程序，使此命令的子进程也被拦截；钩子无法加载的程序（macOS 上 Apple 自带的工具和在 Rosetta 下运行的程序，Windows 上没有对应钩子 DLL 的架构）除外，它们不带钩子运行。这是需要监控子进程的构建命令的默认动作。
- **`FAIL`（2）** -- 不执行命令：它以退出码 1 结束，方式与 `DROP` 相同。会话已因错误结束（某个 handler 中止了构建，或者自身出错），此后启动的每条命令都会这样失败，直到构建结束。

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

当钩子遇到无效状态（例如缺少环境变量）、代理在命令处理过程中捕获到异常，或在 Windows 上钩子要启动的程序无法启动时使用。守护进程记录错误并可通知用户。

### FINISH

报告命令执行完成。

**Params** -- `process_result`：

| 字段   | 类型      | 说明                                          |
| ------ | --------- | --------------------------------------------- |
| `code` | `int64_t` | 进程退出码；被信号结束的进程为 128 加信号编号 |

**Result**: `null`（无响应负载）

守护进程收到此请求后，把结果告知会话的各个 handler（即 handler 协议中的 `exit`）。随后代理断开连接。Windows 钩子不发送它：守护进程从进程本身得知退出码。

### STARTED

把命令所运行程序的进程 ID 告诉守护进程。

**Params**：

| 字段  | 类型      | 说明          |
| ----- | --------- | ------------- |
| `pid` | `int64_t` | 程序的进程 ID |

**Result**: `bool` -- 程序是否按决定的方式运行

在 Windows 上，钩子以挂起状态启动程序后发送它。守护进程打开该进程，让它按决定的方式运行：注入钩子（`INJECT`）；或者注入一个设为加载后立即以退出码 0（`DROP`）或 1（`FAIL`）结束进程的钩子，这样在进程挂起期间对它做准备的调用方仍然可以照常进行；catter 没有对应钩子的架构（x86）的进程，则会被立即结束。随后守护进程答复 `true`，钩子恢复程序，除非它的调用方要求程序保持挂起；守护进程等待进程退出，然后把它的退出码告诉会话的各个 handler，与 `FINISH` 一样。守护进程无法让程序按决定的方式运行时，会把错误报告给各个 handler 并答复 `false`，钩子则以退出码 1 结束该程序。

在决定一条命令之前，以及构建命令结束之后，守护进程会等到已结束程序的退出都已告知 handler：构建比守护进程先看到程序结束，它接下来启动的命令以及它自己的结束都发生在那次退出之后，与 Linux 和 macOS 上一样，因为在那里 `catter-proxy` 会在退出前发送 `FINISH`。

在 macOS 上，`catter-proxy` 在程序运行后把它作为通知发送，不等待响应。macOS 不像 Linux 和 Windows 那样能让子进程随父进程结束：代理在 `started` 之后、`FINISH` 之前断开，说明它被杀死了，守护进程就用 SIGKILL 杀死它的程序。

## 典型消息序列

一个完整的客户端生命周期包含如下 IPC 消息序列：

### 注入模式（第一个代理）

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
[Proxy launches build command with hook attached]
[Proxy waits for build to complete]
[Proxy exits when build finishes]
```

注入模式的代理同样会为顶层构建命令发送 `CREATE`、`MAKE_DECISION`、`started`（macOS 上）和 `FINISH`，因此构建系统命令本身也会像其他被拦截的命令一样送达各个 handler。

### 包装模式（被拦截的命令）

```
Proxy -> Daemon:  CHECK_MODE(INJECT)
Daemon -> Proxy:  true
Proxy -> Daemon:  CREATE(parent_id)
Daemon -> Proxy:  new_session_id
Proxy -> Daemon:  MAKE_DECISION(command)
Daemon -> Proxy:  action {type, cmd}
[Proxy starts the command, or drops it]
Proxy -> Daemon:  started(pid)              (macOS, a notification)
[Proxy waits for the command]
Proxy -> Daemon:  FINISH(process_result)
Daemon -> Proxy:  null
[Proxy disconnects]
```

### Windows 钩子（被拦截的命令）

```
Hook -> Daemon:   CREATE(parent_id)
Daemon -> Hook:   new_session_id
Hook -> Daemon:   MAKE_DECISION(command)
Daemon -> Hook:   action {type, cmd}
[Hook starts the command suspended]
Hook -> Daemon:   STARTED(pid)
[Daemon injects the hook]
Daemon -> Hook:   true
[Hook resumes the command and disconnects]
[Daemon waits for the command to exit]
```

守护进程自己决定如何处理构建命令，并带着钩子启动它。

### 错误情况

```
Proxy -> Daemon:  REPORT_ERROR(parent_id, error_message)
Daemon -> Proxy:  null
[Proxy exits with code -1]
```

## 会话模型

守护进程维护一棵与构建进程树对应的会话树：

```
Session 1 (root -- the build command)
  +-- Session 2 (make -> gcc file1.c)
  +-- Session 3 (make -> gcc file2.c)
  +-- Session 4 (make -> ar rcs libfoo.a file1.o file2.o)
  +-- Session 5 (make -> sh -c "gcc file3.c")
       +-- Session 6 (sh -> gcc file3.c)
```

每个会话由唯一的 `ipcid_t`（32 位整数）标识。会话树随着 `CREATE` 请求的到达而增量构建，每个请求都指定了 `parent_id`。这种结构支持以下功能：

- **目标树重建** -- 通过了解哪些目标文件链接到哪些目标
- **构建分析** -- 通过跟踪每个会话的计时数据并可视化并行度
- **错误归因** -- 通过追溯错误到引发错误的构建命令

## 连接生命周期

1. **守护进程启动** -- 创建并绑定 Unix 域套接字或命名管道。
2. **客户端连接** -- 打开到套接字/管道的连接：代理中用 `kota::pipe::connect()`，Windows 钩子中用 `CreateFileW`。
3. **守护进程接受连接** -- 对于每个传入连接，守护进程创建一个 `kota::ipc::BincodePeer` 并为每种请求类型注册请求处理器（`on_request<Request<Type>>(...)`）。
4. **请求处理** -- 守护进程的事件循环将传入请求分发到相应的处理器。请求处理器可以是异步的（使用 `co_await`），以等待会话的各个 handler 或其他守护进程状态。
5. **对端断开** -- 当代理进程退出，或 Windows 钩子已启动它的程序时，传输层关闭，`BincodePeer::run()` 协程完成。守护进程记录断开事件并清理会话；在 macOS 上，若代理在 `FINISH` 之前就已断开，守护进程会杀死它的程序；在 Windows 上，守护进程等待钩子启动的程序退出。

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
        FAIL,    // Do not execute: the session has ended with an error
    } type;
    command cmd;                       // Possibly modified command
};
```
