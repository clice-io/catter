# 运行机制（Runtime）

runtime 是 catter 实际捕获进程创建行为的机制。catter 围绕 runtime 展开：脚本不关心命令是如何被捕获的，只关心当前 runtime 提供了什么能力。

## 为什么需要多种 runtime

"拦截进程创建"在不同平台上的可行性和代价差异很大：

- **hook 最全能**，能透明拦截任意子进程创建，但依赖平台特性。例如 macOS 的 `DYLD_INSERT_LIBRARIES` 受系统保护机制限制、需要特殊开发权限；Windows 没有预加载机制，只能靠 DLL 注入。
- **有些场景根本不需要 hook**。构建系统如果遵守 `CC`/`CXX` 环境变量约定，直接替换编译器即可捕获全部编译命令。
- **有些平台提供了专门的监控 API**，例如 macOS 的 Event Stream / Endpoint Security，可以作为 hook 的替代或补充。

因此 catter 把捕获机制抽象为 runtime：一个 runtime 就是一种捕获实现，向脚本暴露固定的一组能力描述，脚本基于能力适配行为，而不是假设某个 runtime 一定存在。

## 能力模型

脚本在 `onStart` 收到的 `config.runtime` 中可以看到当前 runtime 的能力：

| 字段 | 含义 |
|------|------|
| `type` | runtime 类型：`inject` / `eslogger` / `env` |
| `supportActions` | 支持的脚本动作（`skip` / `drop` / `abort` / `modify` 的子集） |
| `supportParentId` | 捕获的命令能否报告父命令 ID（命令树、目标树等功能依赖它） |

## 机制一览

### inject（已实现，默认）

- **原理**：hook 共享库注入构建系统进程，拦截进程创建调用（Unix 的 `execve` / `posix_spawn` 族，Windows 的 `CreateProcess`），把命令重写为经 `catter-proxy` 中转后执行。
- **平台**：Linux、macOS、Windows。
- **能力**：`supportActions = [skip, drop, modify]`，`supportParentId = true`。
- **特点**：递归捕获——所有子孙进程都会被拦截；脚本看到的命令带有完整环境变量和父命令 ID。
- **限制**：实现依赖平台特性（预加载 / DLL 注入）。macOS 上 `DYLD_INSERT_LIBRARIES` 受系统保护机制限制，需要特殊开发权限；Windows 的 DLL 注入可能与强签名或安全软件策略冲突。
- **实现细节**：[钩子机制](hook-mechanism.md)、[IPC 协议](ipc-protocol.md)。

### env（规划中）

- **原理**：通过设置 `CC` / `CXX` 等环境变量，把编译器调用重定向到 `catter-proxy`（包装模式）。
- **适用场景**：构建系统遵守标准编译器环境变量约定时，无需 hook 即可捕获编译命令。
- **预期限制**：只能捕获经由这些环境变量发起的调用；硬编码编译器路径、内嵌 `cc` 的构建脚本可能绕过；预期无法报告父命令 ID。
- **状态**：尚未实现，具体能力（如 `supportActions`）待实现后确认。

### eslogger（规划中）

- **原理**：使用 macOS 的 Event Stream 监控 API 观察进程创建 / 执行事件，属于旁路式监控。
- **适用场景**：macOS 上 hook 受权限限制时的替代机制。
- **预期限制**：仅限 macOS；依赖系统授予的监控权限；旁路式监听对命令执行的干预能力（如 `modify` / `drop`）待实现后确认。
- **状态**：尚未实现。

## 能力矩阵

| 机制 | 平台 | 捕获方式 | supportActions | supportParentId | 状态 |
|------|------|---------|----------------|-----------------|------|
| inject | Linux / macOS / Windows | hook 拦截进程创建 | `skip` `drop` `modify` | 是 | 已实现（默认） |
| env | 跨平台 | `CC` / `CXX` 环境变量 | 待定 | 预期否 | 规划中 |
| eslogger | macOS | Event Stream API | 待定 | 待定 | 规划中 |

## 选择与检查

- 默认使用 `inject`，无需配置；其他机制上线后通过 `-m/--mode` 选择。
- 脚本作者不应假设机制：在 `onStart` 中检查 `config.runtime.supportActions`，只使用当前 runtime 支持的动作。
- `supportParentId = false` 时，`CommandData.parent` 不会出现，依赖父子关系的功能（命令树、目标树）需要降级处理。
