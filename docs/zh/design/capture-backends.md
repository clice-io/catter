# 捕获后端

捕获后端（capture backend）是 catter 获取构建命令的方式：它启动构建，将构建启动的每条命令上报给 catter；catter 让会话的各个 handler 决定如何处理命令，并向它们报告命令的结束情况。无论由哪个后端上报，handler 看到的命令、决定和事件都一样。

## 选择后端

`--capture <backend>` 指定捕获构建的后端：`hook`（默认）、`wrapper`，或者 macOS 上的 `eslogger`。`--from <file>` 回放一次录制的构建，而不运行构建：回放录制的构建不涉及捕获，因此使用 `--from` 时不能同时使用 `--capture`；指定不存在的后端名称也会报错。这两种错误都会在任何程序运行之前报告。

handler 从会话的 `initialize` 消息得知后端：其中的 `mode` 是后端的名字，回放录制的构建时则为 `replay`；其中的 `decisions` 说明构建的命令是否等待 handler 的决定。

## 后端做什么

- **启动构建**，构建怎样结束，catter 就怎样结束。catter 代替构建命令运行：发给 catter 的 SIGTERM、SIGINT 或 SIGHUP 会转给构建命令，catter 被杀死时构建命令也随之结束，`eslogger` 下除外。
- **上报每条命令**：工作目录、程序、参数和环境变量，以及启动它的命令。并行构建的命令会同时上报。catter 未能捕获的命令则以 `error` 的形式上报。
- **遵从 handler 的决定，或者只观察**。遵从决定的后端在每条命令运行前上报它，并按 handler 的决定运行：原样运行（`skip`）、修改后运行（`modify`、`edit`），或者不运行（`drop`，命令以退出码 0 结束）。只观察的后端在命令运行之后才得知它，此时任何决定都已无法改变它：对于做决定的 handler（它对 `initialize` 的应答中的 `decides`），catter 会在构建开始前拒绝它，并指出是哪一个。
- **中止构建或出错的 handler 会结束会话**：此后任何 handler 都不再收到通知。已经启动的命令会继续运行。在遵从决定的后端下，正在决定的那条命令以及构建在它之后启动的每条命令都不运行，直接以退出码 1 结束；在只观察的后端下，catter 得知每条命令时它都已启动，构建会一直运行到结束。随后 catter 以 1 退出。
- **上报每条命令的结束情况**，先于构建在这条命令结束之后启动的任何命令，也先于构建本身的结束。

## 后端一览

### hook（默认）

- **原理**：将钩子库加载到构建的每个进程中，拦截进程创建（Linux 和 macOS 上的 `execve` / `posix_spawn` 族，Windows 上的 `CreateProcess`）。在 Linux 和 macOS 上，每条命令随后通过 `catter-proxy` 运行：它向 catter 询问如何处理命令，按决定运行命令，并为它加载钩子；在 Windows 上，钩子自己询问 catter，并按 catter 的决定启动程序。参见[钩子机制](hook-mechanism.md)和 [IPC 协议](ipc-protocol.md)。
- **平台**：Linux、macOS、Windows。
- **捕获范围**：构建的每个进程及启动该进程的命令，但无法加载钩子的程序所启动的进程除外：这样的程序本身仍会上报，并按决定运行，只是不带钩子。在 macOS 上，这类程序是 Apple 自带的工具（`/bin/sh`、`/usr/bin/make`），以及在 Rosetta 下运行的 Intel 程序；Apple 自带的工具以 arm64e 运行，系统完整性保护（System Integrity Protection）会移除其中的钩子。在 Windows 上，这类程序是 32 位（x86）程序。Windows 程序以其他账户启动的程序则完全看不到。
- 构建及其命令直接使用 catter 自己的标准输入、标准输出和标准错误：它们直接写到终端，颜色不丢，输出顺序也和写入时一致。

### eslogger（macOS）

- **原理**：catter 原样运行构建，由 Endpoint Security（macOS 用于观察进程的接口）通过 macOS 为此提供的程序 `eslogger`（`eslogger exec fork exit`）上报构建启动的每个进程。catter 从构建命令的进程开始跟踪构建的进程：构建的进程运行的每个程序都是一条命令，它的父命令是同一进程在它之前运行的程序，或者 fork 出该进程的那个进程所属的命令。catter 在命令运行之后才读到它，构建从不等待 catter。
- **平台**：macOS。
- **前提条件**：eslogger 需要通过 `sudo` 获得的 root 权限：sudo 会在构建开始前在终端上询问密码；无法询问时 catter 拒绝运行，除非 sudo 无需密码即可运行 `/usr/bin/eslogger`。运行 catter 的应用没有完全磁盘访问权限（Full Disk Access）时，Endpoint Security 不接受任何程序：需要在 System Settings > Privacy & Security > Full Disk Access 中为终端（或其他终端应用）授权；通过 SSH 运行时，需要在远程登录（Remote Login）的设置中打开 "Allow full disk access for remote users"。
- **捕获范围**：构建的进程运行的每个程序，包括 Apple 自带的工具（`/bin/sh`、`/usr/bin/make`、`xcodebuild`）以及它们启动的程序。`/usr/bin/cc` 或 `/bin/sh` 用来代替自己运行的程序（Xcode 的 `clang`、`bash`）是一条独立的命令，是被它代替的那条命令的子命令。不是由构建自己启动的进程看不到，例如 `launchd` 代表构建启动的服务，或者构建之前就已启动的服务器；启动失败的程序也看不到。
- **动作**：不支持任何动作；做决定的 handler 会被拒绝。
- **代价**：catter 要等 eslogger 开始上报才启动构建，构建结束后还要等它最后的事件：总共大约一秒。构建同时启动大量进程时，eslogger 可能跟不上：catter 会取完每个事件，这在构建结束后可能还要花几秒。eslogger 落后太多时，Endpoint Security 可能丢弃事件：catter 会警告构建运行期间 Endpoint Security 在整个系统中丢弃了多少事件；构建的命令可能缺失，丢失的进程启动的每条命令也随之缺失。
- **限制**：
  - 在 macOS 上，程序不会随启动它的程序一起结束：catter 代替构建命令运行时，构建会因 catter 转发的 SIGTERM、SIGINT 和 SIGHUP 而结束，但 catter 被杀死时不会。
  - catter 读取命令的文件（例如它的响应文件 `@file`）时，命令已经运行完毕或正在运行：构建在命令运行完毕后删除的文件（Ninja 对响应文件就是这样）可能已经不在了。

### replay（`--from`）

- **原理**：回放文件中录制的构建，而不运行构建：按文件中的顺序重现每条命令、错误和退出事件，并使用文件记录的 ID 和父命令。构建以文件记录的退出码结束。
- **动作**：没有任何命令运行，因此 `skip`、`drop`、`modify` 和 `edit` 不改变任何东西。`abort` 像结束真实构建一样结束回放：之后的事件不再送达 handler，catter 以 1 退出。

### wrapper

- **原理**：catter 在构建的 `PATH` 上的每个目录之前放一个会话自己的目录，为那个目录中 catter 按名字认得的每个编译器、链接器和归档工具各放一个 shim（包装程序）：无论构建工具查找哪些名字、按什么顺序查找，都会经由 shim 找到没有 catter 时它会找到的那个程序。`CC`、`CXX`、`AR` 这类以路径指定程序的变量改为指向该程序的 shim；CMake 则通过 wrapper 后端的启动器（launcher）运行每次编译和链接（`CMAKE_<LANG>_COMPILER_LAUNCHER` 和 `CMAKE_<LANG>_LINKER_LAUNCHER`）；在 Windows 上，MSBuild 通过 shim 运行 `cl.exe`、`link.exe` 和 `lib.exe`（`CLToolPath`、`LinkToolPath` 和 `LibToolPath`）。以上每一项都只在构建没有自行设置时生效。shim 就是 `catter-proxy`：它上报自己的命令，按决定运行它所代替的程序，并上报这条命令如何结束。它运行的程序在自己的 `PATH` 上找不到 shim：构建缓存运行的编译器（例如 ccache 运行它自己目录之后的那个编译器）就是编译器本身，而不会又是 shim。
- **平台**：Linux、macOS、Windows。
- **捕获范围**：有 shim 的程序，无论构建中由谁运行它们，Apple 自带的 `/usr/bin/make` 和 `/bin/sh` 运行的也包括在内。命令的父命令是启动它的、离它最近的有 shim 的程序，或者构建命令；编译器启动的程序看不到。
- **限制**：构建如果按 catter 运行之前找到的路径运行编译器（CMake 生成的 `build.ninja` 就是这样），就会绕过 shim：应在同一个会话中配置并构建，`catter compile-commands --capture wrapper -- sh -c "cmake -B build && cmake --build build"`。shim 随会话一起消失，因此在 wrapper 后端下配置的构建只能在那个会话中运行：请为 catter 单独配置一个构建。构建自己放在 `PATH` 最前面的目录（例如工具链的目录）中的程序，会先于 shim 被找到；通过 `PATH` 中的相对路径项找到的程序没有 shim，因为构建每换一个目录，相对路径项指向的就是另一个目录。在 Windows 上，只有 `.exe` 程序有 shim；项目如果自己设置了 `CLToolPath`、`LinkToolPath` 或 `LibToolPath`（MSVC 以外的工具集可能会这样做），就会绕过 shim 运行这些工具。

## 能力矩阵

| 后端     | 平台                    | 捕获方式                             | 动作                                  | 父命令               | 状态           |
| -------- | ----------------------- | ------------------------------------ | ------------------------------------- | -------------------- | -------------- |
| hook     | Linux / macOS / Windows | 构建的每个进程中的钩子               | `skip` `drop` `abort` `modify` `edit` | 启动它的进程         | 已实现（默认） |
| eslogger | macOS                   | Endpoint Security，通过 `eslogger`   | 无：只观察                            | 启动它的进程         | 已实现         |
| replay   | 任意                    | 录制的构建（`--from`）               | 仅 `abort` 生效                       | 按录制内容           | 已实现         |
| wrapper  | Linux / macOS / Windows | `PATH` 上的编译器 shim，`CC` / `CXX` | `skip` `drop` `abort` `modify` `edit` | 离它最近的被包装程序 | 已实现         |

## 为任意后端编写 handler

- 不要假定使用某个特定后端：`initialize` 会指明所用后端（`mode`），并说明命令是否等待决定（`decisions`）。不做决定也能工作的 handler，按 `decisions` 的值应答 `decides`。
- 命令的父命令是后端所能看到的、启动它的命令：依赖进程树的功能（命令树、目标树）不应假设两者之间的每个进程都会出现。
