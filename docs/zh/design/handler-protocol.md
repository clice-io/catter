# Handler 协议

本文规定 catter 与 [handler](../scripting/handlers.md) 之间的协议，供不借助 SDK 编写 handler 时参考。各 SDK 都实现了该协议，其类型与 catter 的类型生成自同一份定义。

## 传输

消息采用 [JSON-RPC 2.0](https://www.jsonrpc.org/specification)，按 Language Server Protocol 的方式分帧：一个 `Content-Length: <n>` 头部、一个空行（`\r\n\r\n`），然后是 `n` 字节的 UTF-8 JSON。

- 由 catter 启动的 handler（`--handler`）从 stdin 读取 catter 的消息，把自己的消息写到 stdout。它的 stderr 就是终端的 stderr。
- 监听端口的 handler（`--connect`）在每个 TCP 连接上做同样的事：一个连接就是一个会话。

catter 发送请求和通知；handler 应答这些请求，也可以发出自己的请求 `analyze`，由 catter 应答（其他方法的请求，catter 以错误应答），但不发送通知。除下文的消息外，catter 还可能像 Language Server Protocol 那样，为它不再等待的请求发送 `$/cancelRequest`：handler 可以忽略它。

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
| `mode`          | 字符串        | 捕获后端：`hook` 或 `replay`。               |
| `build.command` | OS 字符串数组 | 构建命令。                                   |
| `build.cwd`     | OS 字符串     | 它的工作目录。                               |

结果：

| 字段             | 类型               | 说明                                                                                                                         |
| ---------------- | ------------------ | ---------------------------------------------------------------------------------------------------------------------------- |
| `decides`        | 布尔值             | 每条命令都以请求的形式发来，构建会等待它的应答。                                                                             |
| `events.command` | 布尔值             | 不做决定的 handler 会收到每条命令的通知。                                                                                    |
| `events.error`   | 布尔值             | handler 会收到 catter 未能捕获的命令的通知。                                                                                 |
| `events.exit`    | 布尔值             | handler 会收到每条命令退出的通知。                                                                                           |
| `env`            | 布尔值             | 命令携带它的环境变量。                                                                                                       |
| `analysis`       | 布尔值             | 命令携带 catter 对它的分析（见下文）。                                                                                       |
| `filter`         | pattern 数组，可选 | handler 只会收到某个 pattern 所指的命令及其退出的消息；对做决定的 handler 而言，其余命令都按 `skip` 处理。省略时为所有命令。 |

pattern 按 catter 对命令的分析来指定命令：

| 字段     | 类型             | 说明                                                                                        |
| -------- | ---------------- | ------------------------------------------------------------------------------------------- |
| `kind`   | 字符串           | 命令运行的程序种类：`compiler`、`archiver` 或 `tool`。                                      |
| `names`  | 字符串数组，可选 | 编译器和工具的名字（`gcc`、`clang`、`clang-cl`、`msvc`、`nvcc`；`lld-elf`……）。省略时不限。 |
| `phases` | 字符串数组，可选 | 编译器命令的阶段（`preprocess`、`syntax-only`、`compile`、`link`……）。省略时不限。          |

catter 读不了参数的命令没有阶段：带 `phases` 的 pattern 不会指向它。

### `command`

| 字段         | 类型                | 说明                                                                        |
| ------------ | ------------------- | --------------------------------------------------------------------------- |
| `id`         | 整数                | 命令在会话中的 id。                                                         |
| `parent`     | 整数，可选          | 启动它的命令；构建命令没有此字段。                                          |
| `cwd`        | OS 字符串           | 它的工作目录。                                                              |
| `executable` | OS 字符串           | 它运行的程序。                                                              |
| `args`       | OS 字符串数组       | 它的参数，第一个是程序名。                                                  |
| `env`        | OS 字符串数组，可选 | 它的环境变量，`NAME=value` 形式的条目；仅当 handler 要求时提供。            |
| `analysis`   | 分析，可选          | catter 对它的分析，仅当 handler 要求时提供；catter 对其程序一无所知时省略。 |

请求的结果是一个动作，由其 `action` 字段指明：

| `action` | 字段                                          | catter 的行为                                                    |
| -------- | --------------------------------------------- | ---------------------------------------------------------------- |
| `skip`   |                                               | 原样运行命令。                                                   |
| `drop`   |                                               | 不运行命令：它以 0 退出。                                        |
| `abort`  | `reason`：字符串                              | 结束会话（见下文），并报告原因。                                 |
| `modify` | `args`，以及可选的 `executable`、`cwd`、`env` | 用这些字段代替命令自己的值运行命令；省略的字段沿用命令原有的值。 |
| `edit`   | `edits`：编辑数组                             | 修改命令的参数后再运行它（见下文）。                             |

### 分析

catter 对一条命令的分析，由其 `kind` 指明。catter 按可执行文件的名字认程序，名字前可以有目标前缀（`x86_64-linux-gnu-gcc`），后面可以有版本号（`gcc-13`、`llvm-ar-18`、MacPorts 的 `clang-mp-18`）：`cc` 和 `c++` 都算 `gcc`，即使它们实际运行的是 clang。

- `compiler`：gcc、clang、clang-cl 或 MSVC 的 cl 的命令。包含 `compiler`；`dialect`，读取其参数所用的语法（`gcc`、`clang` 或 `msvc`，cl 模式下的 clang 也是 `msvc`）；`phase` 和 `artifact`，它产出什么；`target`（`triple`、`os`、`objectFormat`、`env`、`artifactModel`，以及 `source`：目标由什么决定）；`expansion`，命令引用了 catter 读到的响应文件（`@file`）时才有：`files` 是读到的文件，按引用时的写法，也包括这些文件引用的文件，`args` 是读入这些文件后的命令参数，下文的各个下标都按它计算；`arguments`；`reads`，每项有 `path`、`argument` 和 `index`（命令在哪里指定它）、`role`（`source` 或 `link`），源文件还有 `language`；`writes`，每项有 `path`、`reads`（由哪些读入生成）和 `origin`；以及 `diagnostics`，参数没有说清时 catter 如何推断文件。
- `archiver`：`ar`、`gcc-ar` 或 `llvm-ar` 的命令。包含 `operation`、`modifiers`、`thin`、`archive`、`members`、`reads` 和 `writes`。
- `tool`：链接器或库管理工具的命令，catter 用它的选项表读取参数，`tool` 为 `lld-elf`、`lld-coff`、`lld-macho`、`lld-mingw`、`lld-wasm`、`llvm-lib` 或 `llvm-dlltool`；`lld-coff` 和 `llvm-lib` 与 `link.exe`、`lib.exe` 一样，选项不区分大小写。包含 `arguments`，其中的响应文件保持命令里的写法：catter 不读取工具的响应文件。
- `unparsed`：上述程序中 catter 读不了参数的命令，目前包括 nvcc 的命令。包含 `of`（程序种类）、`name`、`error` 和 `message`。

每个参数都有 `option`，即解析别名之后的选项，用带前缀的名字表示（`/I` 也写作 `-I`；输入是 `<input>`，选项表不认识的选项是 `<unknown>`，GCC 和 cl 那些带值的选项连同其后的值一起算作一个：`-dumpbase`、`/ifcOutput` 等）；`spelling`，命令里的原始写法（例如实际是 `-G` 的 `-msmall-data-limit=`）；`index` 和 `count`，它占用命令的哪些参数；以及 `values`。

源文件的语言按 `-x` 的叫法命名（`c`、`c++`、`c++-module`、`objective-c`、`assembler`、`cuda` 等）：`/Tc`、`/Tp` 指定它时用的语言，它前面最后一个 `-x` 设定的语言（`-x none` 会复位），`/TC`、`/TP` 设定的语言，否则由后缀决定，其中 `.C`、`.M`、`.S` 与小写时表示不同的语言（cl 除外），对 C++ 驱动（`g++`、`c++`、`clang++`、`--driver-mode=g++`）而言 `.c` 和 `.i` 也是 C++。`-x` 之后的输入，以及带 `/TC` 或 `/TP` 时的输入，无论后缀如何都是该语言的源文件，目标文件和库除外。

### 编辑

`edit` 的各项编辑依次进行。每项有 `kind` 和 `flags`：按命令的程序接受的写法写出的参数。

| `kind`    | 作用                                                                                                                                                                                                                                                                                                                                          |
| --------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `remove`  | 删掉 flags 所指的参数，以及之前的编辑添加的参数：`-DX` 删掉写作 `-DX` 的参数（`-DX=1` 的值不同，不会删掉），`-D*`（或放在最后、不带值的 `-D`）删掉所有定义，`-W*` 删掉 `-W` 所在选项组中以 `-W` 开头的所有选项。选项表不认识的选项按原样指定，后面加 `=*` 表示任意值；其中带独立值的选项（`-dumpbase`、`/ifcOutput`）单独写出时也表示任意值。 |
| `append`  | 把 flags 加在命令自己的参数之后，使其优先于这些参数生效，也正是库应在的位置：在结束选项的 `--` 和 `/link` 之前。                                                                                                                                                                                                                              |
| `prepend` | 把 flags 加在程序名之后。                                                                                                                                                                                                                                                                                                                     |

没有被删掉的参数保留命令原有的写法，添加的 flags 也原样放入：catter 不会重新生成任何参数。编译器命令的响应文件保持命令里的写法，除非某个编辑删掉了其中的参数或在其中的参数之间添加了 flags，这时由它包含的参数替换它。只有 catter 读得懂参数的命令可以编辑：编译器或工具的命令，`unparsed` 的不行。

### `analyze`

handler 发给 catter 的请求，询问 catter 对 handler 自己给出的命令的分析：`programs`，每项有 `executable`、`args`（第一个是程序名），以及可选的 `cwd`，即程序运行的目录，相对路径的响应文件从这里读取：没有它时只读取绝对路径的响应文件。结果是 `analyses`，每个程序一项，catter 对其一无所知的程序为 `null`。

### `error`、`exit` 和 `finish`

| 消息     | 字段                                                                                                                                                                                                               |
| -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `error`  | `id`、`parent`（可选）、`message`：catter 未能捕获该命令的原因。                                                                                                                                                   |
| `exit`   | `id`、`code`：它的退出码，被信号结束的命令为 128 加信号编号，`duration`：从该 handler 对命令做出决定（对只观察的 handler 而言，是收到通知）到它退出所经过的微秒数，链上排在它之后的 handler 做决定的时间也算在内。 |
| `finish` | `code`：构建的退出码，与 `exit` 的相同。结果为 `null`。                                                                                                                                                            |

## 字符串与数字

- **OS 字符串。** 参数、路径或环境变量条目的字节是合法的 UTF-8 时，它是一个 JSON 字符串，否则是 `{"base64": "<the bytes in base64>"}`：在 Linux 和 macOS 上，它们是字节，不一定是 UTF-8。handler 把这样的值按收到时的样子交回，或者用同样的方式构造一个。
- 其他字符串都是合法的 Unicode。JSON 文本不带字节序标记（BOM），任何对象都不会重复同一个键。
- 缺省的可选字段可以省略，也可以为 `null`。
- 版本 1 的每个整数都小于 2<sup>53</sup>，JavaScript 数值可以精确表示。

## 错误

handler 无法应答某个请求时，以 JSON-RPC 错误应答；catter 报告的就是其中的 `message`。handler 以错误、`abort` 或 catter 无法执行的动作应答、停止运行（输出结束）、写出协议不允许的内容，或没有在 `--handler-timeout` 内应答时，catter 结束会话：正在做决定的命令，以及构建在其后启动的每条命令，都不运行、直接失败；构建结束后，catter 不发送 `finish` 就结束各个 handler 的输入，打印原因并以退出码 1 退出。

只观察的 handler 无法应答通知：它出错时就退出，catter 在下一次向它写入时报告这一情况。
