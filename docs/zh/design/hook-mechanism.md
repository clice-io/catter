# 钩子机制（hook 后端实现）

> 本文描述 **hook 后端**的底层实现，handler 作者通常不需要阅读。它解答的是"命令是如何被捕获的"这一平台细节；handler 关心的能力与限制见[捕获后端](capture-backends.md)。

钩子是 hook 后端捕获构建所用的载体：一个共享库，被加载到构建系统生成的每个进程中。它拦截进程创建调用。在 Linux 和 macOS 上，它将调用重写，使每个子进程都通过 `catter-proxy` 中转，而非直接执行；在 Windows 上，它自己向 catter 询问如何处理每次调用，并按 catter 的决定启动程序。

钩子的实现完全依赖于平台特性。Unix 和 Windows 使用了截然不同的拦截技术。

## Unix（Linux 与 macOS）-- 基于 LD_PRELOAD 的拦截

Unix 钩子是通过动态链接器的预加载机制加载的共享库：

- **Linux**: `libcatter-hook-unix.so`，通过 `LD_PRELOAD` 加载
- **macOS**: `libcatter-hook-unix.dylib`，通过 `DYLD_INSERT_LIBRARIES` 加载

### 被拦截的函数

钩子替换了所有用于创建新进程的标准 POSIX 函数：

**exec 系列**:

- `execve()`、`execv()`
- `execvp()`、`execvpe()`（Linux）、`execvP()`（macOS）
- `execl()`、`execlp()`、`execle()`

**posix_spawn 系列**:

- `posix_spawn()`
- `posix_spawnp()`

在 Linux 上，钩子使用 `dlsym(RTLD_NEXT, "execve")` 从加载顺序中的下一个库获取原始函数指针，然后提供一个签名相同的替换函数。当替换函数被调用时，钩子可以检查和修改参数，然后选择性地调用原始函数。

在 macOS 上，钩子使用 `DYLD_INTERPOSE` 宏在 dyld 层面替换函数，这是该平台上的首选技术。

dyld 会终止无法加载插入库的程序，而钩子是按 catter 自身的架构构建的，Apple 自带的工具（arm64e）和在 Rosetta 下运行的 Intel 程序（x86_64）都不以这个架构运行。插入钩子之前，`catter-proxy` 先向 dyld 询问程序以哪种架构运行（`macho_best_slice`；脚本按其解释器计）：架构不同的程序不带钩子运行，它启动的进程不会被捕获。开启系统完整性保护（SIP）时，dyld 同样会从 Apple 的工具中去掉钩子，因此构建只能捕获到它所运行的 Apple 工具为止：`/usr/bin/make`，或 Ninja 用来运行每条命令的 `/bin/sh`。

### 核心类

钩子由一组协作的类实现：

- **`find_program()`**（kotatsu）-- 像 exec 函数那样，在 `PATH` 或当前目录中查找目标可执行文件；程序不存在或无法运行时，像原调用一样失败。
- **`proxy_environment()`** -- `catter-proxy` 得到的环境：即程序要求的环境，去掉预加载库中的钩子，并加上指明会话的变量。
- **`Executor`** -- 协调整个拦截过程。它从库加载时的环境中取得会话，查找可执行文件，再以 `catter-proxy` 代替原程序调用原始函数；并持有真实 `execve` / `posix_spawn` 的函数指针。

### 钩子初始化

钩子库通过动态链接器的构造函数机制（`on_load`，`__attribute__((constructor))`）加载。初始化期间，库会：

1. 设置日志（调试构建下输出到 catter 数据目录下的 `log/catter-hook.log`）
2. 以环境和真实的 `execve` / `posix_spawn` 函数指针调用 `Executor::init()`（Linux 上经 `dlsym(RTLD_NEXT, ...)`，macOS 上直接引用系统符号）
3. 在 Linux 上调用 `Executor::restart_undecided()`：如果进程的 exec 不是 `catter-proxy` 启动的，就经 `catter-proxy` 重新启动这个进程（见下文）

### 环境变量

钩子从以下环境变量中读取会话信息。代理带着钩子运行它的命令（`RUN`）时设置这些变量，其中的命令 ID 就是这条命令自己的 ID：

| 变量                         | 用途                                                                    |
| ---------------------------- | ----------------------------------------------------------------------- |
| `__key_catter_proxy_path_v1` | `catter-proxy` 二进制文件的绝对路径                                     |
| `__key_catter_command_id_v1` | 进程所运行命令的 ID                                                     |
| `__key_catter_ipc_v1`        | catter 会话的管道                                                       |
| `__key_catter_decided_v1`    | 在 Linux 上，`catter-proxy` 启动进程时所用的 exec，钩子读到后即将其移除 |

### 拦截流程

当任何进程创建函数被调用时（例如 `execve("/usr/bin/g++", argv, envp)`）：

1. 进入钩子的替换函数。

2. 如果库加载时的环境中没有指明 `catter-proxy`，调用原样交给系统：该进程没有会话。

3. **`Executor`** 用 kotatsu 的 `find_program()` 查找目标可执行文件。对于 `execvp()` 和 `execvpe()` 等函数，它会搜索 `PATH` 中的目录。对于 `execve()`，不含 `/` 的路径从当前目录查找。

4. **`Executor`** 构造代理命令：

   ```
   <proxy_path> <resolved_path> <original_argv...>
   ```

   原始的 `argv[0]` 及所有后续参数跟在解析出的路径之后。

5. **`proxy_environment()`** 修改环境数组：
   - 从 `LD_PRELOAD`（或 `DYLD_INSERT_LIBRARIES`）中剥离钩子库名称
   - 如果剥离后 `LD_PRELOAD` 变为空，则将其完全移除
   - 放入进程加载时的 `__key_catter_ipc_v1` 和 `__key_catter_command_id_v1`，替换程序环境中已有的同名变量：程序可能以自己构造的环境启动另一个程序，如 `env -i` 那样，`catter-proxy` 在那里仍能找到它的会话

6. **`Executor`** 使用重写后的命令调用**真正的** `execve()`（通过初始化时解析得到的函数指针）。

7. 如果 `execve` 成功，当前进程镜像被替换，函数不会返回。如果失败，钩子恢复 `errno` 并将错误返回给调用者。

### 为什么要清理环境？

这一步至关重要。如果在启动 `catter-proxy` 时钩子库仍留在 `LD_PRELOAD` 中：

1. `catter-proxy` 本身会被钩住
2. 当代理尝试执行实际的编译器时，钩子会拦截该调用
3. 钩子会将其重写为启动另一个 `catter-proxy`
4. 这将造成**无限递归**

通过从 `LD_PRELOAD` 中剥离钩子，代理可以不受拦截地执行。代理仅在以 `RUN` 动作启动命令时才重新添加 `LD_PRELOAD`，确保钩子仅挂载到正确的进程上。

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

### 不经钩子启动的程序（Linux）

静态程序不加载钩子，Go 的运行时自己发起系统调用：它们启动的程序（例如 bazelisk 启动 Bazel，`go build` 启动 cgo 的 C 编译器）来自钩子看不到的 exec。`LD_PRELOAD` 仍会传到这些程序，钩子照样加载进每一个，然后由它们自行上报：

1. `catter-proxy` 给它按决定启动的每个程序设置 `__key_catter_decided_v1`：它自己的 pid，以及 `argv[0]` 之后参数的个数和哈希。内核经解释器运行脚本时，这些参数仍留在 `argv` 末尾；`catter-proxy` 经 `/bin/sh` 运行没有可执行格式的文件时也是如此。
2. 钩子的构造函数把这个变量从环境中取出，让它只对应一次 exec，再拿它与自己的进程比对：父进程是那个 `catter-proxy`，且 `argv` 以那些参数结尾。
3. 比对一致的进程照常运行。不一致的进程（构建中某个程序启动的子进程，或替换掉 `catter-proxy` 所启动程序的程序）会以 `catter-proxy` 的身份重新启动，带上自己的程序（exec 所指的文件，脚本则为运行它的解释器）、`argv`、环境以及 `__key_catter_late_v1`。`catter-proxy` 像对待其他命令一样上报它，作为 `__key_catter_command_id_v1` 所指命令的子命令，并按决定运行。决定运行时，程序带着钩子重新启动，这时它的变量只写 `catter-proxy` 的 pid，因此不会再次重启；被丢弃时，程序不运行，以退出码 0 结束。

此时程序自己的代码还没有运行：运行过的只有它所链接的库的构造函数，重启后它们会再运行一次。有些进程不会重启：以动态加载器为程序运行的进程（`ld.so <program>`），其 `argv` 已去掉加载器自己的参数；valgrind 在自己的进程中运行的程序，重启会让它脱离 valgrind；在进程之外没有对应文件的程序，例如已删除的文件或 fexecve 的 memfd。静态程序如果用另一个程序替换掉自身，而新程序的参数以它自己的参数结尾（不带参数启动的静态程序换成任何程序都是如此），上报的仍是它自己。

---

## Windows -- DLL 注入与 API 钩子

Windows 钩子使用了与 Unix 完全不同的方法，因为 Windows 没有 `LD_PRELOAD` 的等价机制。catter 结合使用了：

1. **DLL 注入** -- 将钩子库加载到目标进程中
2. **基于 [Microsoft Detours](https://github.com/microsoft/Detours) 的 API 钩子** -- 在这些进程内拦截 `CreateProcess` 调用

Windows 钩子不运行 `catter-proxy`。它在调用方自己的 `CreateProcess` 调用中运行，在这里就能完成 `catter-proxy` 在 Unix 上做的事：通过会话的管道与 catter 通信，并亲自启动程序。catter 只要持有某个进程的句柄，Windows 就会把这个进程的退出码交给它，因此 catter 无需在每个程序旁边另起一个自己的进程来得知程序如何结束。

### 钩子 DLL

进程只会加载与自身架构相同的 DLL，因此每种架构各有一个钩子 DLL，都安装在 `catter.exe` 所在的目录：

| DLL                     | 加载到                                                 |
| ----------------------- | ------------------------------------------------------ |
| `catter-hook-x64.dll`   | x64 进程、在 ARM64 上模拟运行的 x64 进程、ARM64EC 程序 |
| `catter-hook-arm64.dll` | 原生 ARM64 进程                                        |

catter 的每个 Windows 版本都同时带有这两个 DLL：在 Windows on ARM 上，一次构建中既有原生 ARM64 工具，也有模拟运行的 x64 工具。32 位（x86）进程目前还没有对应的钩子 DLL：它不加载钩子直接运行，与没有 catter 时一样，它启动的命令也不会被捕获。catter 会记录一条警告，写明是哪个程序。handler 丢弃这类程序的命令时，catter 会在调用方恢复该进程之前就立即结束它。

两个 DLL 构建时都不启用 AddressSanitizer，并把 C++ 运行时链接进 DLL 本身，只导入 Windows 自带的 DLL（包括 UCRT）：它们运行在构建的每个进程中，这些进程未必带有其他运行时 DLL；而且 Detours 在地址空间中搜索时会读到 ASan 的影子内存（shadow memory）。

### 进程的会话

进程中的钩子从 catter 那里得知自己向哪个会话报告，而不是从环境变量中读取：catter 向它注入钩子的每个进程复制一份 Detours 载荷（payload），其中存有会话的管道和该进程所运行命令的 ID；对于不该运行的命令，存的则是钩子一加载就让进程结束时使用的退出码。构建的环境保持构建自己设置的样子；即使调用方把自己的环境原样传给程序，每个程序也会得到自己的命令 ID。

### DLL 注入过程

catter 把钩子注入它自己启动的构建命令，以及钩子启动的每个程序：

1. **进程处于挂起状态**。catter 用 kotatsu 的 `kota::sys::spawn` 启动构建命令，其 `windows_before_start` 回调拿到已创建的进程：进程已在自己的 job 对象中，主线程尚未运行。钩子启动的程序由钩子以挂起状态创建，钩子把它的进程 ID 发给 catter，catter 随后打开该进程。

2. **确定进程的架构**。`GetProcessInformation(ProcessMachineTypeInfo)` 给出进程所运行代码的机器类型：在 ARM64 上模拟运行的 x64 进程和 ARM64EC 进程得到的都是 x64，而 `IsWow64Process2` 无法把它们与原生 ARM64 进程区分开。Windows 10 没有 `ProcessMachineTypeInfo`，也不支持模拟 x64，在那里由 `IsWow64Process2` 给出架构。

3. **把钩子 DLL 加入进程的导入表**。`DetourUpdateProcessWithDll()` 把对应架构的 DLL 加入挂起的可执行文件的导入表，这样加载器会在程序自身的任何代码运行之前映射它。加载器按进程自己的 ANSI 代码页读取导入表里的 DLL 路径，而程序可以把这个代码页设为 UTF-8，所以路径必须是纯 ASCII：含非 ASCII 字符的路径改用它的 8.3 短文件名；在不保留短文件名的卷上，则改用复制到 catter 数据目录中的那份 DLL。

4. **把会话交给钩子**。`DetourCopyPayloadToProcess()` 把会话的管道和命令的 ID 复制到进程中。

5. **恢复主线程**。构建命令在回调返回后由 `spawn` 恢复；钩子启动的程序在 catter 答复后由钩子恢复。加载器把钩子 DLL 与程序的其他导入项一同映射，钩子在程序入口点运行之前就已激活。

### 被钩住的 Windows API

钩子 DLL 在 `DLL_PROCESS_ATTACH` 期间用 Detours 为以下函数安装钩子，全部在同一个事务中完成：

- **`CreateProcessA()`** -- ANSI 版本的进程创建函数
- **`CreateProcessW()`** -- 宽字符版本的进程创建函数

程序以其他账户的令牌（token）启动的进程（`CreateProcessAsUser`、`CreateProcessWithTokenW`、`CreateProcessWithLogonW`）不带钩子运行，也不会被捕获：那个账户可能访问不到会话的管道。

Detours 的工作原理：

1. 保存目标函数的前几条指令
2. 将其覆写为跳转到替代函数的指令
3. 提供一个 trampoline（跳板），包含保存的指令及跳回原函数的跳转，使原始函数仍可被调用

### 钩子替代逻辑

当被钩住的进程调用 `CreateProcessA` 或 `CreateProcessW` 时：

1. **解析程序**。钩子用 `resolve_program()` 解析：`lpApplicationName` 从当前目录补全为绝对路径；没有它时，取命令行的第一个词，按 `CreateProcess` 的方式查找（kotatsu 的 `find_create_process_program`：程序所在目录、当前目录、系统目录、Windows 目录，最后是 `PATH`）；不带引号的名字会依次读到每个空白处，和 `CreateProcess` 读 `C:\Program Files\tool.exe a b` 的方式一样。找不到这样的文件时，这次调用原样交给原始的 `CreateProcess`，失败方式和没有 catter 时相同。

2. **连接会话**。钩子打开会话的管道。打不开时（例如早先的会话启动的构建服务器，而那个会话已经结束），这次调用原样交给原始的 `CreateProcess`，程序运行但不被捕获。

3. **询问 catter**。钩子按 `CreateProcess` 将要启动的样子读取命令：命令行中的参数、环境块（没有时取调用方自己的环境）和工作目录。它发送 `MAKE_DECISION`，然后等待 catter 的答复；除此之外，`CreateProcess` 本身不等待任何别的东西。

4. **以挂起状态启动程序**。钩子用调用方自己的参数、标志、句柄和 `STARTUPINFO` 调用原始的 `CreateProcess`，只额外加上 `CREATE_SUSPENDED`。handler 改动过的部分会重新写入：程序，以命令的工作目录为当前目录，按 `CreateProcess` 的方式查找；命令行，仅当 handler 改了参数时才由参数重新拼出；环境；工作目录。其余部分保持调用方写下的样子，因此 `cmd.exe` 以及其他自己读取命令行的程序，读到的和没有 catter 时一样。程序无法启动时，钩子报告这一错误（`REPORT_ERROR`），并让这次调用以系统给出的错误失败。

5. **交给 catter 接管**。钩子发送进程 ID（`started`）。catter 打开该进程，让它按决定的方式运行：注入钩子；对于不该运行的命令，注入的钩子会在加载后立即以退出码 0（已丢弃）或 1（会话已失败）结束进程，赶在程序自身的代码运行之前。随后 catter 等待进程退出，并把它的退出码告诉各个 handler。catter 无法让程序按决定的方式运行时，会报告错误，钩子则以退出码 1 结束该程序。

6. **恢复程序**。除非调用方要求以挂起状态创建进程，钩子会恢复主线程，然后返回。调用方拿到的是程序自己的进程和线程句柄及 ID。

调用方持有的就是程序本身，与没有 catter 时一样。调用方对自己启动的进程所做的事，也都和没有 catter 时一样有效：例如 MSBuild 的 tracker，它以挂起状态启动每个工具，在工具运行前注入文件追踪；又如调试器，或记下各个任务进程 ID 的构建工具。

### 与 Unix 的关键区别

在 Unix 上，`LD_PRELOAD` 是一个由所有子进程自动继承的环境变量。钩子默认具有"传染性" -- 每个子进程都会被钩住。

在 Windows 上没有这样的机制。钩子 DLL 必须显式注入到每个新进程中：钩子启动的每个程序还处于挂起状态时，catter 就把钩子注入其中。链上可以交替出现不同架构的进程：每个进程都会得到与自身架构对应的钩子 DLL。

### DLL 生命周期

```
DLL_PROCESS_ATTACH:
  DetourRestoreAfterWith()   -- Restore the import table catter changed
  DetourFindPayloadEx()      -- Read the session catter copied into the process
  TerminateProcess(...)      -- End a process whose command is not to run
  DetourTransactionBegin()
  DetourAttach(...)          -- Hook CreateProcessA and CreateProcessW
  DetourTransactionCommit()  -- Activate all hooks at once
```

在 attach 阶段调用 `DisableThreadLibraryCalls()` 以抑制 `DLL_THREAD_ATTACH` 和 `DLL_THREAD_DETACH` 通知，减少运行开销。该 DLL 是可执行文件的导入项，只随进程一起卸载，因此 `DLL_PROCESS_DETACH` 时没有需要撤销的操作。它导出序号为 1 的符号，凡是由 Detours 注入的 DLL 都必须如此。

## 平台对比

| 方面         | Linux                                                                             | macOS                            | Windows                            |
| ------------ | --------------------------------------------------------------------------------- | -------------------------------- | ---------------------------------- |
| 钩子库       | `libcatter-hook-unix.so`                                                          | `libcatter-hook-unix.dylib`      | `catter-hook-{x64,arm64}.dll`      |
| 注入方式     | `LD_PRELOAD` 环境变量                                                             | `DYLD_INSERT_LIBRARIES` 环境变量 | `DetourUpdateProcessWithDll`       |
| 函数替换     | `dlsym(RTLD_NEXT, ...)`                                                           | `DYLD_INTERPOSE` 宏              | Detours trampoline 钩子            |
| 拦截的 API   | `execve`、`execvp`、`posix_spawn` 等                                              | 与 Linux 相同                    | `CreateProcessA`、`CreateProcessW` |
| 递归钩子     | 自动（环境变量继承）                                                              | 自动（环境变量继承）             | 显式（每个进程重新注入 DLL）       |
| 会话         | `__key_catter_proxy_path_v1`、`__key_catter_command_id_v1`、`__key_catter_ipc_v1` | 与 Linux 相同                    | 每个进程中的一份 Detours 载荷      |
| 由谁启动命令 | `catter-proxy`                                                                    | 与 Linux 相同                    | 钩子，在调用方自己的调用中         |
