# 贡献指南

## 测试

测试分两种：C++ 单元测试，以及用例（case）：在小项目上运行构建出的 catter，把每一项输出和快照比较。

```bash
# Run everything: configure, build, unit tests and cases (debug, or another mode)
pixi run -e dev test

# Unit tests only
pixi run -e dev unit-test

# Cases only, against the release build (or debug, releasedbg)
pixi run -e dev cases-test release
```

### 单元测试

单元测试使用 Kotatsu 测试框架（`kota::zest`）。位于 `unittests/`，目录结构与 `src/` 对应：

- `unittests/support/` -- 共享工具的测试（程序解析器）
- `unittests/command/` -- 命令分析的测试（编译器识别、参数解析、选项表）
- `unittests/protocol/`、`unittests/feature/` -- handler 协议和内置功能的测试
- `unittests/hook/` -- hook payload 的测试

启用测试目标：

```bash
pixi run cfg  # or: xmake config --test=y
```

每个测试程序把测试分给一组 worker 进程同时运行，每个测试限时 60 秒。用到固定文件名或目录名的 suite 必须单独运行：用 `ZEST_SUITE_ATTRS(serial = true);` 标记。调试某个测试时，用 `--no-isolation --test-filter=<suite>.<test>` 让它在测试程序自己的进程里运行；其他选项见 `--help`。

### 用例

catter 行为的每个测试都是 `tests/<topic>/<subtopic>/NN_name/` 下的一个用例：一个小项目的输入、可选的 `test.ts`（说明运行什么），以及记录用例全部输出的 `snap.toml`。运行器（`tools/testing/`，基于 vitest 的 TypeScript）运行构建出的 catter，因此用例不依赖 catter 用什么构建、用什么语言实现。约定见 `tests/README.md`：如何编写用例、路径归一化用的占位符、平台差异和已知 bug。

```bash
# The cases under a path
CATTER_CASES=builds/make pixi run -e dev cases-test

# Accept the new outputs of the cases that changed; review the diff of snap.toml
UPDATE_SNAPSHOTS=1 pixi run -e dev cases-test

# Another catter than a build's
CATTER_EXECUTABLE=/path/to/catter pixi run -e dev pnpm --dir tests run cases
```

## 提交信息格式

使用[约定式提交](https://www.conventionalcommits.org/)：

```
<type>(<scope>): <short description>
```

**类型**：`feat`、`fix`、`refactor`、`chore`、`build`、`docs`、`ci`、`test`、`perf`、`style`、`revert`

scope 应与源码目录或功能名称一致。标题行不超过 70 个字符。

## 代码风格

- C++23 标准
- C++ 格式化使用 `.clang-format`
- TypeScript 使用 ESLint + Prettier
- `pixi run -e format format` 格式化所有源文件；CI 发现它会改动文件即失败
