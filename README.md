# Catter: A Universal Compilation Command Capture Tool

![C++ Standard](https://img.shields.io/badge/C++-23-blue.svg)
[![GitHub license](https://img.shields.io/github/license/clice-io/catter)](https://github.com/clice-io/catter/blob/main/LICENSE)

> [!IMPORTANT]
> This project is currently under active development.

## The Problem

A significant challenge in the C++ ecosystem is that many build tools do not natively support the generation of a Compilation Database (CDB), which is essential for modern C++ language tooling. Examples include traditional Makefiles, CMake when using the MSBuild generator, and other complex build systems like Bazel or MSBuild itself.

While solutions like `bear` exist, they often require installing a different, specialized tool for each build system, which is inconvenient and limits cross-platform usability.

We are developing a new C++ language server that relies on a CDB to locate files and provide accurate language features. To ensure a seamless experience for our users, we aim to create **catter**, a single, unified, cross-platform tool to capture compilation commands from **any** build process.

## Features

### 1. Cross-Platform Support

`catter` is engineered to be fully operational on all major desktop operating systems:
* **Windows**
* **macOS**
* **Linux**

And it actually applies to any scenario where a series of tree-like commands need to be invoked in sequence. You can use catter's JavaScript script to capture, analyze and replace them in real time.


### 2. Build-System Agnostic via Process Interception

`catter` captures compilation commands without requiring modifications to the user's existing build scripts. This is achieved by:

* **Process Monitoring/Interception:** On each platform, `catter` monitors the build process tree to capture invocations of all commands invoked more than command of known C/C++ compilers (`g++`, `clang`, `cl.exe`, etc.) and their full command-line arguments.

### 3. Extensibility and Scripting with JavaScript

To provide ultimate flexibility, `catter` includes a full JavaScript runtime environment for advanced control over the compilation command capture process.

* **Full Scripting Support:** Users can write custom JavaScript scripts to **intercept, modify, and replace** *all* commands executed during the compilation period.
* **Dynamic Profiling:** The tool supports real-time command sensing, enabling users to write dynamic JavaScript scripts for advanced build **profiling** and analysis.
* **Custom CDB Generation:** Users can leverage the scripting engine to implement their own custom logic for transforming captured commands, or even *generating* the Compilation Database entirely via script, giving unprecedented control over the final `compile_commands.json`.
* **Avoid Full Compilation (If Possible):** We can just writes scripts to easily intercept the compiler call and redirecting it to a "fake" compiler that simply records the command and creates an empty object file (`.o`, `.obj`), enabling **dry run** and **fake run** scenarios without a time-consuming full build.


## Architecture

User run `catter` in command line, `catter` will call `catter-proxy` responsible to invoke commands. Every system call for real command will be replaced with `catter-proxy` in hooked program, therefore we can log the command instantly.
