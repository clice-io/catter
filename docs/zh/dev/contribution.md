# 贡献指南

## 测试

三种方式运行测试：

```bash
# 运行全部（构建 + 单元测试 + 集成测试）
pixi run -e dev test

# 仅单元测试
pixi run -e dev unit-test

# 仅集成测试
pixi run -e dev integration-test
```

### 单元测试

单元测试使用 Kotatsu 测试框架（`kota::zest`）。位于 `tests/unit/`，目录结构与 `src/` 对应：

- `tests/unit/common/` -- 共享工具和选项解析的测试
- `tests/unit/catter/` -- 核心逻辑的测试（编译器识别、JS 运行时）
- `tests/unit/catter-hook/unix/` -- Unix hook payload 的测试

启用测试目标：

```bash
pixi run cfg  # 或: xmake config --test=y
```

每个测试程序把测试分给一组 worker 进程同时运行，每个测试限时 60 秒。用到固定文件名或目录名的 suite 必须单独运行：用 `ZEST_SUITE_ATTRS(serial = true);` 标记。调试某个测试时，用 `--no-isolation --test-filter=<suite>.<test>` 让它在测试程序自己的进程里运行；其他选项见 `--help`。

### 集成测试

集成测试使用 [LLVM Lit](https://llvm.org/docs/CommandGuide/lit.html) 框架。位于 `tests/integration/`，编译并运行 C++ 测试程序，使用 FileCheck 进行输出验证。

```bash
# 以详细输出运行集成测试
lit ./tests/integration -sav
```

## 提交信息格式

使用[约定式提交](https://www.conventionalcommits.org/zh-hans/)：

```
<type>(<scope>): <简短描述>
```

**类型**：`feat`、`fix`、`refactor`、`chore`、`docs`、`ci`、`test`、`perf`、`style`、`revert`

scope 应与源码目录或功能名称一致。标题行不超过 70 个字符。

## 代码风格

- C++23 标准
- C++ 格式化使用 `.clang-format`
- TypeScript 使用 ESLint + Prettier
- 已配置 pre-commit 钩子（`.pre-commit-config.yaml`）
