# 命令树

命令树将构建命令的有向无环图以 ASCII 树的形式可视化，展示进程之间的父子关系及其执行的命令。

内置脚本：`script::cmd-tree`

## 用法

```bash
catter script::cmd-tree [options] -- <build-command>
```

## 选项

| 选项 | 说明 |
|------|------|
| `-d, --depth <n>` | 限制渲染深度。 |
| `-a, --args <n>` | 每条命令显示的参数数量。默认：全部。 |
| `-w, --argWidth <n>` | 将过长的参数截断到指定宽度。默认：10 个字符。 |

## 输出

输出为 ANSI 彩色的 ASCII 树，使用制表符（`│`、`├──`、`└──`）绘制。颜色按深度在 4 种颜色之间循环，便于快速区分嵌套层级。

典型的构建树可能如下所示：

```
make -j8
├── gcc -c main.c -o main.o
│   └── as -o main.o /tmp/ccXXXX.s
├── gcc -c util.c -o util.o
│   └── as -o util.o /tmp/ccYYYY.s
└── gcc main.o util.o -o app
    └── ld -o app main.o util.o -lc
```

每个节点显示进程的命令行，子节点为该进程派生的子进程。树结构由实际的进程拦截构建而成，因此完全反映构建过程中的真实行为。

## 使用场景

- **理解构建编排** -- 查看构建系统如何调度编译器、汇编器和链接器。
- **调试异常进程派生** -- 识别构建中未预期的进程调用。
- **验证拦截完整性** -- 确认所有预期的编译操作都被 catter 正确拦截。
