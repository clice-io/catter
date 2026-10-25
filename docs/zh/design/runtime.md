# 运行机制（Runtime）

runtime 是 catter 实际捕获进程创建行为的机制。catter 围绕 runtime 展开：handler 不关心命令是如何被捕获的，只关心当前 runtime 提供了什么。

## 为什么需要多种 runtime

"拦截进程创建"在不同平台上的可行性和代价差异很大：

- **hook 最全能**，能透明拦截任意子进程创建，但依赖平台特性。例如 macOS 的 `DYLD_INSERT_LIBRARIES` 受系统保护机制限制、需要特殊开发权限；Windows 没有预加载机制，只能靠 DLL 注入。
- **有些场景根本不需要 hook**。构建系统如果遵守 `CC`/`CXX` 环境变量约定，直接替换编译器即可捕获全部编译命令。
- **有些平台提供了专门的监控 API**，例如 macOS 的 Event Stream / Endpoint Security，可以作为 hook 的替代或补充。

因此 catter 把捕获机制抽象为 runtime：一个 runtime 就是一种捕获实现，具备下文列出的能力。handler 基于这些能力适配行为，而不是假设某个 runtime 一定存在。

## 机制一览

### inject（已实现，默认）

- **原理**：hook 共享库注入构建系统进程，拦截进程创建调用（Unix 的 `execve` / `posix_spawn` 族，Windows 的 `CreateProcess`），把命令重写为经 `catter-proxy` 中转后执行。
- **平台**：Linux、macOS、Windows。
- **能力**：支持全部动作（`skip`、`drop`、`abort`、`modify`）；命令带有父命令 ID。
- **特点**：递归捕获——所有子孙进程都会被拦截；捕获的命令带有完整环境变量和父命令 ID。构建及其命令直接使用 catter 自己的标准输入、标准输出和标准错误：它们直接写到终端，颜色不丢，输出顺序也和写入时一致。
- **限制**：实现依赖平台特性（预加载 / DLL 注入）。macOS 上 `DYLD_INSERT_LIBRARIES` 受系统保护机制限制，需要特殊开发权限；Windows 的 DLL 注入可能与强签名或安全软件策略冲突。
- **实现细节**：[钩子机制](hook-mechanism.md)、[IPC 协议](ipc-protocol.md)。

### env（计划中）

- **原理**：通过设置 `CC` / `CXX` 等环境变量，把编译器调用重定向到 `catter-proxy`（包装模式）。
- **适用场景**：构建系统遵守标准编译器环境变量约定时，无需 hook 即可捕获编译命令。
- **预期限制**：只能捕获经由这些环境变量发起的调用；硬编码编译器路径、内嵌 `cc` 的构建脚本可能绕过；预期无法报告父命令 ID。
- **状态**：尚未实现，具体能力（如它支持哪些动作）待实现后确认。

### eslogger（计划中）

- **原理**：使用 macOS 的 Event Stream 监控 API 观察进程创建 / 执行事件，属于旁路式监控。
- **适用场景**：macOS 上 hook 受权限限制时的替代机制。
- **预期限制**：仅限 macOS；依赖系统授予的监控权限；旁路式监听对命令执行的干预能力（如 `modify` / `drop`）待实现后确认。
- **状态**：尚未实现。

## 能力矩阵

| 机制     | 平台                    | 捕获方式              | 动作                           | 父命令 ID | 状态           |
| -------- | ----------------------- | --------------------- | ------------------------------ | --------- | -------------- |
| inject   | Linux / macOS / Windows | hook 拦截进程创建     | `skip` `drop` `abort` `modify` | 是        | 已实现（默认） |
| env      | 跨平台                  | `CC` / `CXX` 环境变量 | 待定                           | 预期否    | 计划中         |
| eslogger | macOS                   | Event Stream API      | 待定                           | 待定      | 计划中         |

## 选择与检查

- 默认使用 `inject`，无需配置；其他捕获机制上线后通过 `-m/--mode` 选择，就像现在的 `replay`：它回放一次录制的构建，而不捕获构建。
- handler 不应假设机制：`initialize` 会告知当前机制（`mode`）。
- 在命令不带父命令 ID 的 runtime 下，命令的 `parent` 不会出现，依赖父子关系的功能（命令树、目标树）需要降级处理。
