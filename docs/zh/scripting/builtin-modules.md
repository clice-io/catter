# 内置模块参考

Catter 提供多个内置模块，通过 `import { ... } from "catter"` 导入。本页介绍核心 `service`、`cmd` 和 `option` API 之外的工具模块。

## fs -- 文件系统

`fs` 模块提供同步文件操作和路径工具。

### 文件操作

```js
import { fs } from "catter";

fs.exists("/path/to/file");        // boolean
fs.isFile("/path/to/file");        // boolean
fs.isDir("/path/to/dir");          // boolean
fs.pwd();                          // 当前工作目录
fs.readDirs("/path/to/dir");       // string[]，绝对路径数组
fs.mkdir("/path/to/dir");          // 创建目录（默认递归）
fs.createFile("/path/to/file");    // 创建空文件（默认递归）
fs.removeAll("/path/to/dir");      // 递归删除文件或目录
fs.rename("old", "new");           // 重命名/移动，old 不存在时返回 false
```

### 路径工具

```js
import { fs } from "catter";

fs.path.isAbsolute("/tmp/file");           // true
fs.path.absolute("./relative");            // 解析为绝对路径
fs.path.joinAll("/home", "user", "file");  // "/home/user/file"
fs.path.toAncestor("/a/b/c/d", 2);        // "/a/b"
fs.path.extension("file.cpp");             // ".cpp"
fs.path.filename("/path/to/file.txt");     // "file.txt"
fs.path.relativeTo("/home", "/home/user"); // "user"
fs.path.lexicalNormal("src/./a/../b.cc");  // "src/b.cc"
```

### 异步操作

```js
import { fs } from "catter";

await fs.async.exists(path);
await fs.async.isFile(path);
await fs.async.isDir(path);
await fs.async.readDirs(path);
await fs.async.mkdir(path);
await fs.async.createFile(path);
await fs.async.removeAll(path);
await fs.async.rename(oldPath, newPath);
await fs.async.readText(path);         // 读取文件为字符串
await fs.async.writeText(path, text);  // 将字符串写入文件
```

## io -- 输入/输出

### 文本输出

```js
import { io } from "catter";

io.print("no newline");
io.println("with newline");
io.coloredPrint("warning", "yellow");
io.coloredPrintln("error", "red");
io.coloredPrintln("success", "green");
io.coloredPrintln("info", "blue");
```

支持的颜色：`"red"`、`"yellow"`、`"blue"`、`"green"`。

### 二进制文件流

`FileStream` 类提供底层的二进制读写能力：

```js
import { io } from "catter";

const stream = new io.FileStream("data.bin");
const bytes = stream.read(1024);
stream.write(new Uint8Array([1, 2, 3]));
stream.append(new Uint8Array([4, 5]));
const size = stream.file_size();
const all = stream.readEntireFile();
stream.close();
```

使用 `with()` 模式自动清理资源：

```js
io.FileStream.with("data.bin", (stream) => {
  const data = stream.readEntireFile();
  io.println(`Read ${data.length} bytes`);
});
```

文件定位操作使用 `SeekWhence`：

```js
stream.seekRead(0, io.SeekWhence.SET);   // 文件开头
stream.seekWrite(0, io.SeekWhence.END);  // 文件末尾
stream.seekRead(10, io.SeekWhence.CUR);  // 相对当前位置
```

### 文本文件流

`TextFileStream` 类封装了 `FileStream`，支持编码（ASCII 和 UTF-8）：

```js
import { io } from "catter";

io.TextFileStream.with("log.txt", "utf-8", (stream) => {
  const lines = stream.readLines();
  for (const line of lines) {
    io.println(line);
  }
});
```

主要方法：

| 方法 | 描述 |
|------|------|
| `read(chars)` | 读取 N 个字符 |
| `write(text)` | 写入字符串 |
| `readLine()` | 读取一行（不包含换行符） |
| `readLines()` | 将所有行读入数组 |
| `readUntil(delimiter)` | 读取到分隔符为止 |
| `readEntireFile()` | 将整个文件读取为字符串 |
| `append(text)` | 追加到文件末尾 |

## os -- 操作系统

```js
import { os } from "catter";

os.platform();  // "linux" | "macos" | "windows"
os.arch();      // "x86" | "x64" | "arm" | "arm64"
```

## http -- HTTP 客户端

### 快捷方法

```js
import { http } from "catter";

const text = await http.text("https://example.com");
const data = await http.json("https://api.example.com/data");
const res = await http.get(url);
const res = await http.post(url, body);
const res = await http.put(url, body);
const res = await http.patch(url, body);
const res = await http.del(url);
const res = await http.head(url);
```

### 完整请求

```js
import { http } from "catter";

const response = await http.request("https://example.com/api", {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify({ key: "value" }),
  timeoutMs: 5000,
  maxRedirects: 5,
});

io.println(`Status: ${response.status}`);  // 200
io.println(`OK: ${response.ok}`);          // true
io.println(`Body: ${response.text()}`);
const data = response.json();
const contentType = response.header("content-type");
```

### Response 对象

| 字段/方法 | 类型 | 描述 |
|----------|------|------|
| `status` | `number` | HTTP 状态码 |
| `ok` | `boolean` | 状态码为 2xx 时为 true |
| `url` | `string` | 最终 URL（经过重定向后） |
| `body` | `string` | 响应体 |
| `headers` | `Record<string, string>` | 规范化的响应头（小写键名） |
| `text()` | `string` | 以字符串形式返回响应体 |
| `json()` | `T` | 将响应体解析为 JSON |
| `header(name)` | `string \| undefined` | 按名称获取响应头（不区分大小写） |

### 请求选项

| 选项 | 类型 | 默认值 | 描述 |
|------|------|--------|------|
| `method` | `string` | `"GET"` | HTTP 方法 |
| `headers` | `Record \| Array` | `{}` | 请求头 |
| `body` | `string` | `""` | 请求体 |
| `timeoutMs` | `number` | `-1`（无超时） | 请求超时时间 |
| `maxRedirects` | `number` | `-1`（默认） | 最大重定向次数 |
| `proxy` | `string` | `""` | 代理 URL |

### 复用客户端

对于多次请求，可以创建可复用的客户端：

```js
const client = new http.Client();
const res = await client.get(url);
client.close();
```

## time -- 时间工具

### 时间戳

```js
import { time } from "catter";

time.now();          // Unix 时间戳（毫秒），与 time.unixMs() 相同
time.unixMs();       // Unix 时间戳（毫秒）
time.unixUs();       // Unix 时间戳（微秒）
time.unixSeconds();  // Unix 时间戳（秒）
time.monotonicMs();  // 单调时钟（毫秒，用于计算耗时）
time.monotonicUs();  // 单调时钟（微秒）
```

### 时间间隔转换

所有时间间隔辅助函数以毫秒为单位：

```js
time.ms(1500);       // 1500（恒等，用于代码清晰性）
time.seconds(2);     // 2000
time.minutes(1);     // 60000
time.hours(1);       // 3600000
time.days(1);        // 86400000
time.ns(1000000);    // 1
time.us(1000);       // 1
```

在任意单位之间转换：

```js
time.convert(2, "s", "ms");  // 2000
time.toMs(5, "s");           // 5000
time.fromMs(60000, "min");   // 1
```

### 耗时计算

```js
const start = time.monotonicMs();
// ... 执行操作 ...
const ms = time.elapsedMs(start);
const seconds = time.elapsed(start, "s");
```

## debug -- 断言

```js
import { debug } from "catter";

debug.assertPrint(condition);         // 失败时打印 "assertion failed!"
debug.assertThrow(condition);         // 失败时抛出 Error
debug.assertDo(condition, () => {     // 失败时执行回调
  io.println("custom failure handler");
});
```

## cli -- 脚本参数解析

`cli` 模块提供声明式的参数解析器，用于解析脚本选项。通常在 `onStart` 中使用，解析 `config.scriptArgs`。

### 定义命令

```js
import { cli } from "catter";

const myCommand = cli.command({
  name: "my-script",
  description: "My custom build analysis script.",
  options: [
    cli.string("output", {
      short: "o",
      valueName: "path",
      description: "Output file path.",
    }),
    cli.flag("verbose", {
      short: "v",
      description: "Enable verbose output.",
    }),
    cli.number("depth", {
      short: "d",
      description: "Maximum tree depth.",
      default: 3,
      min: 1,
      max: 100,
    }),
    cli.string("include", {
      short: "I",
      description: "Include paths.",
      multiple: true,
    }),
  ] as const,
  positionals: [
    cli.positional("files", {
      multiple: true,
      required: false,
      description: "Additional input files.",
    }),
  ] as const,
  examples: [
    "my-script -o result.json",
    {
      command: "my-script -v -d 5 -o out.json",
      description: "Verbose output with depth 5.",
    },
  ],
});
```

### 解析参数

使用 `cli.run()` 最为简便 -- 它会自动打印帮助/错误信息，解析成功返回值对象，失败返回 `undefined`：

```js
service.onStart((config) => {
  const result = cli.run(myCommand, config.scriptArgs);
  if (result === undefined) {
    config.execute = false;
    return config;
  }

  io.println(`Output: ${result.output}`);    // string | undefined
  io.println(`Verbose: ${result.verbose}`);  // boolean
  io.println(`Depth: ${result.depth}`);      // number
  io.println(`Files: ${result.files}`);      // string[]
  return config;
});
```

需要更细粒度的控制时，使用 `cli.parse()`（返回结果对象）或 `cli.parseOrThrow()`（失败时抛出 `CLIParseError`）：

```js
const result = cli.parse(myCommand, args);
if (!result.ok) {
  io.println(cli.formatError(result));
  return;
}
if (result.helpRequested) {
  io.println(result.usage);
  return;
}
// result.values 是解析后的值对象
```

### 选项类型

| 工厂函数 | 值类型 | 描述 |
|----------|--------|------|
| `cli.flag(name, opts)` | `boolean` | 布尔标志，默认 `false` |
| `cli.string(name, opts)` | `string` | 字符串选项 |
| `cli.number(name, opts)` | `number` | 数字选项（支持 `min`、`max`、`integer`） |
| `cli.positional(name, opts)` | `string` | 位置参数 |

所有选项类型均支持 `short`、`description`、`required`、`multiple`、`default` 和 `hidden`。数字选项额外支持 `min`、`max` 和 `integer`。字符串和数字选项支持 `parse` 函数进行自定义验证。

### 使用说明文本

以编程方式生成格式化的帮助文本：

```js
const helpText = cli.usage(myCommand);
io.println(helpText);
```

内置的 `-h` / `--help` 处理默认启用。在命令定义中设置 `help: false` 可禁用。
