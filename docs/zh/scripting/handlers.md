# Handler

handler 是一个程序，catter 会把构建启动的命令告诉它。**做决定**的 handler 在每条命令运行前给出答复：原样运行、换成另一条命令运行、丢弃，或中止构建。不做决定的 handler **只观察**：它只接收通知，构建从不等它。

catter 通过一个简单的协议与 handler 通信（见 [Handler 协议](../design/handler-protocol.md)），所以 handler 可以用任何语言编写。Python 和 TypeScript 的 SDK 替你处理协议。

## 运行 handler

```bash
catter --handler "python handler.py" -- make
```

catter 启动 handler，经它的 stdin 和 stdout 与它通信，并在构建结束时终止它。handler 输出到 stderr 的内容显示在终端中。handler 在构建之外运行：它启动的程序不会被捕获。

在 Linux 和 macOS 上，命令按 POSIX shell 的规则拆分成单词，但不做任何展开；在 Windows 上，它像 Windows 命令行一样原样交给程序。

已在运行、监听某个 TCP 端口的 handler，用 `--connect` 连接：

```bash
catter --connect 127.0.0.1:9000 -- make
```

这适合在调试器下运行的 handler，或依次为多次构建服务的 handler。主机写 IP 地址或 `localhost`；对 `localhost`，catter 先尝试 `127.0.0.1`，再尝试 `::1`。除非指定了 `--allow-remote`，catter 只连接本机上的 handler，因为 handler 能修改、丢弃构建的命令。

### 选项

| 选项                          | 说明                                                                           | 默认值 |
| ----------------------------- | ------------------------------------------------------------------------------ | ------ |
| `--handler <command>`         | 由 catter 启动的 handler。可多次指定。                                         |        |
| `--connect <host:port>`       | 在该地址监听的 handler。可多次指定。                                           |        |
| `--allow-remote`              | 允许 `--connect` 连接其他机器上的 handler。                                    |        |
| `--handler-timeout <seconds>` | catter 等待 handler 应答、以及会话结束后等待它退出的时长。`0` 表示一直等下去。 | `60`   |

## SDK

SDK 尚未发布：它们在 catter 仓库的 `sdk/` 下，需要从仓库的检出目录中使用。把 `sdk/python` 加入 `PYTHONPATH` 后，Python handler 就能找到 `catter` 包，需要 Python 3.10 或更高版本。TypeScript handler 通过 `node_modules/@catter/sdk` 处指向 `sdk/typescript` 的符号链接导入 `@catter/sdk`，前提是 `pixi run -e dev npm-install` 已安装好 SDK 自身的依赖；它需要能直接运行 TypeScript 的 Node.js 22.18 或更高版本。

## 用 Python 编写 handler

Python SDK 只依赖标准库。handler 继承 `catter.Handler`，重写它需要的回调：

```python
import catter


class Handler(catter.Handler):
    decides = True

    def on_command(self, command):
        if "-O0" in command.args:
            return catter.Modify(args=[*command.args, "-g"])
        if command.args[0].endswith("ranlib"):
            return catter.Drop()
        return None  # run it as it is


catter.serve(Handler())
```

| 回调                  | 时机                                                   |
| --------------------- | ------------------------------------------------------ |
| `on_start(session)`   | 会话开始：它的构建命令和工作目录，以及命令的捕获方式。 |
| `on_command(command)` | 一条命令即将运行。做决定的 handler 返回对它的动作。    |
| `on_error(error)`     | catter 未能捕获一条命令。                              |
| `on_exit(exit)`       | 一条命令已运行完毕：它的退出码和耗时。                 |
| `on_finish(finish)`   | 构建已结束，附带它的退出码。                           |

handler 只会收到它重写了回调的那些事件。设置 `env = True` 后，命令会携带它的环境变量。

动作有 `catter.Skip()`（等同于返回 `None`）、`catter.Drop()`（命令不运行，以 0 退出）、`catter.Abort(reason=...)` 和 `catter.Modify(args=..., executable=..., cwd=..., env=...)`，后者必须给出 `args`，省略的其他字段沿用命令自己的值。

导入 `catter` 之后，handler 打印的内容都会进入 stderr，因为它的 stdout 用来传输协议。`serve()` 运行之后，handler 和它启动的程序从 stdin 读到的是空设备，因为 stdin 也用来传输协议。参数、路径和环境变量条目都是字符串；不是 UTF-8 的字节以孤立代理项（lone surrogate）出现，与 `os.fsdecode` 的表示方式相同，交回后原样传给命令。

要供 catter 以 `--connect` 连接，监听一个端口，为每个连上来的 catter 处理它的会话：

```python
with catter.listen(port=9000) as listener:
    while True:
        listener.accept(Handler())
```

## 用 TypeScript 编写 handler

TypeScript SDK `@catter/sdk` 运行在 Node.js 上：

```ts
import { serve } from "@catter/sdk";

await serve({
  decides: true,
  onCommand(command) {
    if (command.args.includes("-O0")) {
      return { action: "modify", args: [...command.args, "-g"] };
    }
    return undefined; // run it as it is
  },
});
```

回调有 `onStart`、`onCommand`、`onError`、`onExit` 和 `onFinish`，与 Python 中的相同。不是 UTF-8 的参数以 `{ base64 }` 的形式出现，交回的命令会保留它。配合 `--connect` 时，`listen(port)` 返回一个监听器，它的 `accept(handler)` 为下一个连接上来的 catter 服务一次会话。

导入 `@catter/sdk` 之后，handler 用 `console` 打印的内容会输出到 stderr。它的 stdin 和 stdout 用于传输协议：由 catter 启动的 handler 不能读 `process.stdin`、不能写 `process.stdout`，也不能让它启动的程序继承这两者。

## 多个 handler

`--handler` 和 `--connect` 都可以多次指定，也可以与脚本一起使用：

```bash
catter --handler "python observe.py" --handler "python decide.py" script::cdb -o compile_commands.json -- make
```

handler 按固定顺序组成一条链：脚本在最前，其后是按命令行顺序排列的 handler。每条命令依次交给它们，每个 handler 看到的都是经前面的 handler 处理后的命令，直到有一个丢弃它为止；排在后面的 handler 永远看不到被丢弃的命令。只观察的 handler 若要看到每条命令，应放在最前面。

## handler 失败时

handler 中止构建、在回调中抛出异常、退出、写出的内容不是对 catter 请求的应答，或没有在 `--handler-timeout` 内应答，都会结束会话：它正在做决定的那条命令，以及构建在其后启动的每条命令，都不运行、直接失败（退出码 1）。构建结束后，catter 不发送 `finish`（`on_finish` 不会运行）就结束各个 handler 的输入，打印发生了什么，并以退出码 1 退出：

```
Handler `python handler.py` did not answer command 12 within 60 s
```

会话结束后，由 catter 启动的 handler 若没有在超时时间内退出，会被终止。构建从不等待只观察的 handler，但落后于构建的观察者必须在构建结束后的超时时间内赶上进度：它要在那时应答 `finish`。
