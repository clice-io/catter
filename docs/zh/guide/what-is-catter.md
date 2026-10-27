# 什么是 Catter？

Catter 是一个构建过程拦截工具。它通过钩子拦截构建过程中的子进程创建，捕获编译和链接命令，并让你用它的内置功能或 handler 处理这些命令。handler 是你自己编写的程序，可以用任何语言实现。

## 动机

Catter 是为 [clice](https://github.com/clice-io/clice) 这个新的 C++ 语言服务器而创建的。语言服务器依赖[编译数据库 (CDB)](https://clang.llvm.org/docs/JSONCompilationDatabase.html) 来了解每个源文件的编译方式，但获取 CDB 往往非常困难：

- **构建系统支持有限** -- CMake 仅在使用 Makefile 或 Ninja 后端时才能生成 CDB。许多其他流行的 C++ 构建系统完全没有原生 CDB 支持。
- **构建目录不可访问** -- 当 C++ 代码作为 Python 包（或类似形式）的一部分进行构建时，构建目录由外部管理，生成的 CDB 文件无法获取。
- **代码生成器需要完整构建** -- 像 LLVM 这样的项目使用 `tablegen` 等工具在构建时生成头文件。这些文件通过 `-I` 标志在编译命令中被引用，因此在完整构建之前，语言服务器无法正确解析 include 路径——即使你只是想阅读代码。

目前没有跨平台工具能够完整解决上述所有问题，这就是我们创建 catter 的原因。

## 工作原理

Catter 的方式类似于 [Bear](https://github.com/rizsotto/Bear) 和 [scan-build](https://github.com/rizsotto/scan-build)。它使用平台特定的钩子（Linux 上的 `LD_PRELOAD`、macOS 上的 `DYLD_INSERT_LIBRARIES`、Windows 上的 DLL 注入）拦截子进程创建。每个被拦截的命令会发送到一个决策守护进程，由它询问会话的各个 handler 如何处理这条命令。

## 核心功能

### 编译数据库生成

最主要的用例。Catter 可以跨任意构建系统捕获编译器调用，生成标准的 `compile_commands.json`，无需构建系统插件或特殊配置。

### 链接器分析

通过捕获链接器命令，catter 可以重建构建目标之间的依赖图。这对 C++20 模块支持至关重要：C++ 标准规定一个程序中同名模块最多只能有一个，而"程序"由目标（库、可执行文件）定义。这些目标信息在 CDB 标准中是缺失的，但 catter 可以从链接器命令中推断出来。

### 伪编译（计划中）

不将编译任务转发给真正的编译器，catter 将能够生成占位目标文件。这使得构建系统可以正常运行而无需实际编译，从而在极短的时间内生成完整的 CDB。代码生成器（如 `tablegen`）仍然会正常构建——catter 会分析依赖关系以确定必须真正编译的最小工具集。

### 构建分析（计划中）

捕获构建过程中的进程时间、持续时间和父子关系，可以在浏览器中可视化渲染，帮助分析构建并行度和识别性能瓶颈。

### 自定义 handler

handler 是你自己编写的程序，catter 通过一个简单的协议与它通信。它可以观察每条命令，也可以对每条命令做决定：原样运行、换成另一条命令运行、丢弃，或中止构建——无需修改原始构建文件。Python 和 TypeScript 的 SDK 替你处理协议。

## 架构概览

Catter 围绕 **捕获后端**（capture backend）展开：捕获后端是 catter 获取构建命令的方式。默认的 `hook` 后端通过平台钩子（Linux 的 `LD_PRELOAD`、macOS 的 `DYLD_INSERT_LIBRARIES`、Windows 的 DLL 注入）拦截子进程创建；捕获到的每条命令都会交给守护进程，再由守护进程转给会话的各个 handler。

handler 会在这些时刻收到通知：会话开始时（同时得知命令以何种方式捕获）、每条命令运行前（在只观察构建的后端下，则是命令启动之后）和运行后，以及构建结束时。做决定的 handler 决定每条命令原样运行、修改后运行、丢弃，还是中止构建；不做决定的 handler 只观察。

关于架构概念、捕获后端和 IPC 流程的详细介绍，请参阅[设计文档](../design/architecture)与[捕获后端](../design/capture-backends)。
