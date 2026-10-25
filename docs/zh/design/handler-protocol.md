# Handler 协议

本文规定 catter 与 [handler](../scripting/handlers.md) 之间的协议，供不借助 SDK 编写 handler 时参考。各 SDK 都实现了该协议，其类型与 catter 的类型生成自同一份定义。

## 传输

消息采用 [JSON-RPC 2.0](https://www.jsonrpc.org/specification)，按 Language Server Protocol 的方式分帧：一个 `Content-Length: <n>` 头部、一个空行（`\r\n\r\n`），然后是 `n` 字节的 UTF-8 JSON。

- 由 catter 启动的 handler（`--handler`）从 stdin 读取 catter 的消息，把自己的消息写到 stdout。它的 stderr 就是终端的 stderr。
- 监听端口的 handler（`--connect`）在每个 TCP 连接上做同样的事：一个连接就是一个会话。

catter 发送请求和通知；handler 只应答请求，不发送其他任何消息。除下文的消息外，catter 还可能像 Language Server Protocol 那样，为它不再等待的请求发送 `$/cancelRequest`：handler 可以忽略它。

## 一次会话

1. `initialize`（请求）：会话开始。handler 在应答中声明它想收到哪些消息。
2. 构建运行期间，针对每条命令：`command`，对做决定的 handler 是请求，对不做决定的 handler 是通知；catter 未能捕获某条命令时发送 `error`；命令运行完毕后发送 `exit`。只有 handler 要求过的通知才会发来。
3. `finish`（请求）：构建已结束。handler 应答后，catter 结束它的输入：由 catter 启动的 handler 随即退出；通过 TCP 连接时，catter 关闭自己一侧的连接，由 handler 关闭连接。

并行构建中的命令会相互重叠：多个 `command` 请求可以同时等待应答，handler 按请求的 id 应答，顺序不限。

## 消息

### `initialize`

| 字段            | 类型          | 说明                                         |
| --------------- | ------------- | -------------------------------------------- |
| `version`       | 整数          | 协议版本：`1`。                              |
| `session`       | 字符串        | 区分同一个 handler 通过 TCP 服务的多个会话。 |
| `mode`          | 字符串        | 命令的捕获方式：`inject` 或 `replay`。       |
| `build.command` | OS 字符串数组 | 构建命令。                                   |
| `build.cwd`     | OS 字符串     | 它的工作目录。                               |

结果：

| 字段             | 类型   | 说明                                             |
| ---------------- | ------ | ------------------------------------------------ |
| `decides`        | 布尔值 | 每条命令都以请求的形式发来，构建会等待它的应答。 |
| `events.command` | 布尔值 | 不做决定的 handler 会收到每条命令的通知。        |
| `events.error`   | 布尔值 | handler 会收到 catter 未能捕获的命令的通知。     |
| `events.exit`    | 布尔值 | handler 会收到每条命令退出的通知。               |
| `env`            | 布尔值 | 命令携带它的环境变量。                           |

### `command`

| 字段         | 类型                | 说明                                                             |
| ------------ | ------------------- | ---------------------------------------------------------------- |
| `id`         | 整数                | 命令在会话中的 id。                                              |
| `parent`     | 整数，可选          | 启动它的命令；构建命令没有此字段。                               |
| `cwd`        | OS 字符串           | 它的工作目录。                                                   |
| `executable` | OS 字符串           | 它运行的程序。                                                   |
| `args`       | OS 字符串数组       | 它的参数，第一个是程序名。                                       |
| `env`        | OS 字符串数组，可选 | 它的环境变量，`NAME=value` 形式的条目；仅当 handler 要求时提供。 |

请求的结果是一个动作，由其 `action` 字段指明：

| `action` | 字段                                          | catter 的行为                                                    |
| -------- | --------------------------------------------- | ---------------------------------------------------------------- |
| `skip`   |                                               | 原样运行命令。                                                   |
| `drop`   |                                               | 不运行命令：它以 0 退出。                                        |
| `abort`  | `reason`：字符串                              | 结束会话（见下文），并报告原因。                                 |
| `modify` | `args`，以及可选的 `executable`、`cwd`、`env` | 用这些字段代替命令自己的值运行命令；省略的字段沿用命令原有的值。 |

### `error`、`exit` 和 `finish`

| 消息     | 字段                                                                                                                                                                            |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `error`  | `id`、`parent`（可选）、`message`：catter 未能捕获该命令的原因。                                                                                                                |
| `exit`   | `id`、`code`：它的退出码，`duration`：从该 handler 对命令做出决定（对只观察的 handler 而言，是收到通知）到它退出所经过的微秒数，链上排在它之后的 handler 做决定的时间也算在内。 |
| `finish` | `code`：构建的退出码。结果为 `null`。                                                                                                                                           |

## 字符串与数字

- **OS 字符串。** 参数、路径或环境变量条目的字节是合法的 UTF-8 时，它是一个 JSON 字符串，否则是 `{"base64": "<the bytes in base64>"}`：在 Linux 和 macOS 上，它们是字节，不一定是 UTF-8。handler 把这样的值按收到时的样子交回，或者用同样的方式构造一个。
- 其他字符串都是合法的 Unicode。JSON 文本不带字节序标记（BOM），任何对象都不会重复同一个键。
- 缺省的可选字段可以省略，也可以为 `null`。
- 版本 1 的每个整数都小于 2<sup>53</sup>，JavaScript 数值可以精确表示。

## 错误

handler 无法应答某个请求时，以 JSON-RPC 错误应答；catter 报告的就是其中的 `message`。handler 以错误或 `abort` 应答、停止运行（输出结束）、写出的内容不是对 catter 某个请求的应答，或没有在 `--handler-timeout` 内应答时，catter 结束会话：正在做决定的命令，以及构建在其后启动的每条命令，都不运行、直接失败；构建结束后，catter 不发送 `finish` 就结束各个 handler 的输入，打印原因并以退出码 1 退出。

只观察的 handler 无法应答通知：它出错时就退出，catter 在下一次向它写入时报告这一情况。
