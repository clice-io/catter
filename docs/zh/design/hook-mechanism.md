# 钩子机制（inject runtime 实现）

> 本文描述 **inject runtime** 的底层实现，handler 作者通常不需要阅读。它解答的是"命令是如何被捕获的"这一平台细节；handler 关心的能力与限制见[运行机制](runtime.md)。

钩子是 inject runtime 的捕获载体：一个共享库，被加载到构建系统生成的每个进程中。它的唯一职责是拦截进程创建调用，并将其重写，使每个子进程都通过 `catter-proxy` 中转，而非直接执行。

钩子的实现完全依赖于平台特性。Unix 和 Windows 使用了截然不同的拦截技术。

## Unix（Linux 与 macOS）-- 基于 LD_PRELOAD 的拦截

Unix 钩子是通过动态链接器的预加载机制加载的共享库：

- **Linux**: `libcatter-hook-unix.so`，通过 `LD_PRELOAD` 加载
- **macOS**: `libcatter-hook-unix.dylib`，通过 `DYLD_INSERT_LIBRARIES` 加载

### 被拦截的函数

钩子替换了所有用于创建新进程的标准 POSIX 函数：

**exec 系列**:

- `execve()`、`execv()`
- `execvpe()`、`execvp()`、`execvP()`
- `execl()`、`execlp()`、`execle()`

**posix_spawn 系列**:

- `posix_spawn()`
- `posix_spawnp()`

在 Linux 上，钩子使用 `dlsym(RTLD_NEXT, "execve")` 从加载顺序中的下一个库获取原始函数指针，然后提供一个签名相同的替换函数。当替换函数被调用时，钩子可以检查和修改参数，然后选择性地调用原始函数。

在 macOS 上，钩子使用 `DYLD_INTERPOSE` 宏在 dyld 层面替换函数，这是该平台上的首选技术。

### 核心类

钩子由一组协作的类实现：

- **`Session`** -- 从环境变量中读取并存储会话信息（代理路径、自身命令 ID 和会话的管道）。
- **`Resolver`** -- 解析目标可执行文件的路径。处理 PATH 查找、相对路径解析及可执行文件缺失等边界情况。
- **`Command` / `build_proxy_command()`** -- 构造重写后的代理命令，将 `catter-proxy` 替换原始可执行文件。
- **`SanitizedEnv` / `sanitize_environment()`** -- 在调用真正的 `execve` 之前清理环境。移除 catter 相关变量，并从预加载变量中剥离钩子库。
- **`Executor`** -- 协调整个拦截过程。验证会话、解析可执行文件、构造代理命令、清理环境、调用原始函数；并持有真实 `execve` / `posix_spawn` 的函数指针（早期独立的 `Linker` 类已并入其中）。

### 钩子初始化

钩子库通过动态链接器的构造函数机制（`on_load`，`__attribute__((constructor))`）加载。初始化期间，库会：

1. 设置日志（调试构建下输出到 catter 数据目录下的 `log/catter-hook.log`）
2. 调用 `Executor::init()`：从环境变量中读取会话状态，并解析真实的 `execve` / `posix_spawn` 函数指针（Linux 上经 `dlsym(RTLD_NEXT, ...)`，macOS 上直接引用系统符号）

### 环境变量

钩子从以下环境变量中读取会话信息，这些变量由代理在启动构建命令时设置：

| 变量                         | 用途                                |
| ---------------------------- | ----------------------------------- |
| `__key_catter_proxy_path_v1` | `catter-proxy` 二进制文件的绝对路径 |
| `__key_catter_command_id_v1` | 父进程的会话 ID                     |
| `__key_catter_ipc_v1`        | catter 会话的管道                   |

### 拦截流程

当任何进程创建函数被调用时（例如 `execve("/usr/bin/g++", argv, envp)`）：

1. 进入钩子的替换函数。

2. **`Executor`** 验证会话。如果会话无效（缺少环境变量），它会构建一个错误命令，将问题报告给守护进程。

3. **`Resolver`** 将目标可执行文件解析为绝对路径。对于 `execvp()` 和 `execvpe()` 等函数，它会搜索 `PATH` 中的目录。对于 `execve()`，则相对于当前目录解析。

4. **`Command`**（`build_proxy_command()`）构造代理命令：

   ```
   <proxy_path> -p <self_id> --exec <resolved_path> --ipc <pipe> -- <original_argv...>
   ```

   原始的 `argv[0]` 及所有后续参数保留在 `--` 分隔符之后。

5. **`SanitizedEnv`**（`sanitize_environment()`）修改环境数组：
   - 移除 `__key_catter_proxy_path_v1`、`__key_catter_command_id_v1` 和 `__key_catter_ipc_v1`
   - 从 `LD_PRELOAD`（或 `DYLD_INSERT_LIBRARIES`）中剥离钩子库名称
   - 如果剥离后 `LD_PRELOAD` 变为空，则将其完全移除

6. **`Executor`** 使用重写后的命令调用**真正的** `execve()`（通过初始化时解析得到的函数指针）。

7. 如果 `execve` 成功，当前进程镜像被替换，函数不会返回。如果失败，钩子恢复 `errno` 并将错误返回给调用者。

### 为什么要清理环境？

这一步至关重要。如果在启动 `catter-proxy` 时钩子库仍留在 `LD_PRELOAD` 中：

1. `catter-proxy` 本身会被钩住
2. 当代理尝试执行实际的编译器时，钩子会拦截该调用
3. 钩子会将其重写为启动另一个 `catter-proxy`
4. 这将造成**无限递归**

通过从 `LD_PRELOAD` 中剥离钩子，代理可以不受拦截地执行。代理仅在以 `INJECT` 动作启动命令时才重新添加 `LD_PRELOAD`，确保钩子仅挂载到正确的进程上。

### 递归钩子

由于 `LD_PRELOAD` 通过环境变量被子进程继承，拦截是自动递归的。考虑如下构建场景：

```
make
  -> sh -c "gcc main.c -o main"
     -> gcc main.c -o main
        -> cc1 main.c -o main.s
        -> as main.s -o main.o
        -> ld main.o -o main
```

这棵进程树中的每个进程都继承了 `LD_PRELOAD`，因此都会被钩住。每次调用都会经过 `catter-proxy`，由守护进程决定如何处理。构建系统完全无法感知到拦截的存在。

---

## Windows -- DLL 注入与 API 钩子

Windows 钩子使用了与 Unix 完全不同的方法，因为 Windows 没有 `LD_PRELOAD` 的等价机制。catter 结合使用了：

1. **DLL 注入** -- 将钩子库加载到目标进程中
2. **基于 [Microsoft Detours](https://github.com/microsoft/Detours) 的 API 钩子** -- 在这些进程内拦截 `CreateProcess` 调用

### 钩子 DLL

进程只会加载与自身架构相同的 DLL，因此每种架构各有一个钩子 DLL，都安装在 `catter-proxy.exe` 所在的目录：

| DLL                     | 加载到                                                 |
| ----------------------- | ------------------------------------------------------ |
| `catter-hook-x64.dll`   | x64 进程、在 ARM64 上模拟运行的 x64 进程、ARM64EC 程序 |
| `catter-hook-arm64.dll` | 原生 ARM64 进程                                        |

catter 的每个 Windows 版本都同时带有这两个 DLL：在 Windows on ARM 上，一次构建中既有原生 ARM64 工具，也有模拟运行的 x64 工具。32 位（x86）进程目前还没有对应的钩子 DLL：它不加载钩子直接运行，与没有 catter 时一样，它启动的命令也不会被捕获。catter-proxy 会记录一条警告，写明是哪个程序。

两个 DLL 构建时都不启用 AddressSanitizer，并静态链接 C 运行时：它们运行在构建的每个进程中，这些进程未必带有运行时 DLL；而且 Detours 在地址空间中搜索时会读到 ASan 的影子内存（shadow memory）。

### 环境变量

| 变量                | 用途                          |
| ------------------- | ----------------------------- |
| `CATTER_IPC_ID`     | 父进程的会话 ID               |
| `CATTER_IPC_PIPE`   | catter 会话的管道             |
| `CATTER_PROXY_PATH` | `catter-proxy.exe` 的绝对路径 |

### DLL 注入过程

当 `catter-proxy`（注入模式）需要启动挂载了钩子的构建命令时：

1. **以挂起状态创建目标进程**。代理调用 `CreateProcessW()`，使用 `CREATE_SUSPENDED` 标志。进程被创建但主线程不会运行。

2. **设置环境变量**。`CATTER_IPC_ID`、`CATTER_IPC_PIPE` 和 `CATTER_PROXY_PATH` 在创建进程前通过环境块传入目标进程。

3. **确定进程的架构**。`GetProcessInformation(ProcessMachineTypeInfo)` 给出进程所运行代码的机器类型：在 ARM64 上模拟运行的 x64 进程和 ARM64EC 进程得到的都是 x64，而 `IsWow64Process2` 无法把它们与原生 ARM64 进程区分开。Windows 10 没有 `ProcessMachineTypeInfo`，也不支持模拟 x64，在那里由 `IsWow64Process2` 给出架构。

4. **把钩子 DLL 加入进程的导入表**。`DetourUpdateProcessWithDll()` 把对应架构的 DLL 加入挂起的可执行文件的导入表，这样加载器会在程序自身的任何代码运行之前映射它。加载器按进程自己的 ANSI 代码页读取导入表里的 DLL 路径，而程序可以把这个代码页设为 UTF-8，所以路径必须是纯 ASCII：含非 ASCII 字符的路径改用它的 8.3 短文件名；在不保留短文件名的卷上，则改用复制到 catter 数据目录中的那份 DLL。

5. **恢复主线程**。代理调用 `ResumeThread()` 让目标进程开始执行。加载器把钩子 DLL 与程序的其他导入项一同映射，钩子在程序入口点运行之前就已激活。

### 被钩住的 Windows API

钩子 DLL 在 `DLL_PROCESS_ATTACH` 期间用 Detours 为以下函数安装钩子，全部在同一个事务中完成：

- **`CreateProcessA()`** -- ANSI 版本的进程创建函数
- **`CreateProcessW()`** -- 宽字符版本的进程创建函数

程序以其他账户的令牌（token）启动的进程（`CreateProcessAsUser`、`CreateProcessWithTokenW`、`CreateProcessWithLogonW`）不带钩子运行，也不会被捕获：要捕获它，`catter-proxy` 也得以该令牌运行，而那个账户可能无法启动它，也可能访问不到会话的管道。

Detours 的工作原理：

1. 保存目标函数的前几条指令
2. 将其覆写为跳转到替代函数的指令
3. 提供一个 trampoline（跳板），包含保存的指令及跳回原函数的跳转，使原始函数仍可被调用

### 钩子替代逻辑

当被钩住的进程调用 `CreateProcessA` 或 `CreateProcessW` 时：

1. **提取命令行**。钩子读取 `lpApplicationName` 和 `lpCommandLine`。

2. **解析绝对路径**。钩子使用 `resolve_abspath()` 解析可执行文件路径，搜索程序目录、当前目录、System32、Windows 目录和 `PATH`（与标准 Windows 搜索顺序一致）。

3. **重写命令行**：

   ```
   {proxy_path} -p {ipc_id} --exec {resolved_path} --ipc {pipe} -- {original_cmdline}
   ```

   `lpApplicationName` 被设为 `nullptr`，使 `CreateProcess` 正常解析命令行。

4. **通过 Detours trampoline 调用原始 `CreateProcess`**，传入修改后的命令行。

### 与 Unix 的关键区别

在 Unix 上，`LD_PRELOAD` 是一个由所有子进程自动继承的环境变量。钩子默认具有"传染性" -- 每个子进程都会被钩住。

在 Windows 上没有这样的机制。钩子 DLL 必须显式注入到每个新进程中。这通过以下方式实现：

1. 钩子的 `CreateProcess` 替代函数拦截每次进程创建
2. 将命令重写为通过 `catter-proxy` 中转
3. 当代理决定以 `INJECT` 动作执行时，它对新进程执行 DLL 注入（创建挂起、注入、恢复）

这意味着代理负责在每一代子进程中重新注入钩子，维持递归拦截链。链上可以交替出现不同架构的进程：每个进程都会得到与自身架构对应的钩子 DLL。

### DLL 生命周期

```
DLL_PROCESS_ATTACH:
  DetourRestoreAfterWith()   -- Restore the import table catter-proxy changed
  DetourTransactionBegin()
  DetourAttach(...)          -- Hook CreateProcessA and CreateProcessW
  DetourTransactionCommit()  -- Activate all hooks at once
```

在 attach 阶段调用 `DisableThreadLibraryCalls()` 以抑制 `DLL_THREAD_ATTACH` 和 `DLL_THREAD_DETACH` 通知，减少运行开销。该 DLL 是可执行文件的导入项，只随进程一起卸载，因此 `DLL_PROCESS_DETACH` 时没有需要撤销的操作。它导出序号为 1 的符号，凡是由 Detours 注入的 DLL 都必须如此。

## 平台对比

| 方面         | Linux                                                      | macOS                            | Windows                              |
| ------------ | ---------------------------------------------------------- | -------------------------------- | ------------------------------------ |
| 钩子库       | `libcatter-hook-unix.so`                                   | `libcatter-hook-unix.dylib`      | `catter-hook-{x64,arm64}.dll`        |
| 注入方式     | `LD_PRELOAD` 环境变量                                      | `DYLD_INSERT_LIBRARIES` 环境变量 | `DetourUpdateProcessWithDll`         |
| 函数替换     | `dlsym(RTLD_NEXT, ...)`                                    | `DYLD_INTERPOSE` 宏              | Detours trampoline 钩子              |
| 拦截的 API   | `execve`、`execvp`、`posix_spawn` 等                       | 与 Linux 相同                    | `CreateProcessA`、`CreateProcessW`   |
| 递归钩子     | 自动（环境变量继承）                                       | 自动（环境变量继承）             | 显式（每个进程重新注入 DLL）         |
| 会话环境变量 | `__key_catter_proxy_path_v1`、`__key_catter_command_id_v1` | 与 Linux 相同                    | `CATTER_PROXY_PATH`、`CATTER_IPC_ID` |
