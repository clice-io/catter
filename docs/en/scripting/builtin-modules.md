# Built-in Modules Reference

Catter provides several built-in modules accessible via `import { ... } from "catter"`. This page covers the utility modules that complement the core `service`, `cmd`, and `option` APIs.

## fs -- File System

The `fs` module provides synchronous file operations and path manipulation utilities.

### File Operations

```js
import { fs } from "catter";

fs.exists("/path/to/file");        // boolean
fs.isFile("/path/to/file");        // boolean
fs.isDir("/path/to/dir");          // boolean
fs.pwd();                          // current working directory
fs.readDirs("/path/to/dir");       // string[] of absolute entry paths
fs.mkdir("/path/to/dir");          // create directory (recursive by default)
fs.createFile("/path/to/file");    // create empty file (recursive by default)
fs.removeAll("/path/to/dir");      // recursively remove file or directory
fs.rename("old", "new");           // rename/move, returns false if old doesn't exist
```

### Path Utilities

```js
import { fs } from "catter";

fs.path.isAbsolute("/tmp/file");           // true
fs.path.absolute("./relative");            // resolve to absolute path
fs.path.joinAll("/home", "user", "file");  // "/home/user/file"
fs.path.toAncestor("/a/b/c/d", 2);        // "/a/b"
fs.path.extension("file.cpp");             // ".cpp"
fs.path.filename("/path/to/file.txt");     // "file.txt"
fs.path.relativeTo("/home", "/home/user"); // "user"
fs.path.lexicalNormal("src/./a/../b.cc");  // "src/b.cc"
```

### Async Operations

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
await fs.async.readText(path);         // read file as string
await fs.async.writeText(path, text);  // write string to file
```

## io -- Input/Output

### Text Output

```js
import { io } from "catter";

io.print("no newline");
io.println("with newline");
io.coloredPrint("warning", "yellow");
io.coloredPrintln("error", "red");
io.coloredPrintln("success", "green");
io.coloredPrintln("info", "blue");
```

Supported colors: `"red"`, `"yellow"`, `"blue"`, `"green"`.

### Binary File Streams

The `FileStream` class provides low-level binary read/write access:

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

Use the `with()` pattern for automatic cleanup:

```js
io.FileStream.with("data.bin", (stream) => {
  const data = stream.readEntireFile();
  io.println(`Read ${data.length} bytes`);
});
```

Seek operations use `SeekWhence`:

```js
stream.seekRead(0, io.SeekWhence.SET);   // beginning
stream.seekWrite(0, io.SeekWhence.END);  // end of file
stream.seekRead(10, io.SeekWhence.CUR);  // relative to current position
```

### Text File Streams

The `TextFileStream` class wraps `FileStream` with encoding support (ASCII and UTF-8):

```js
import { io } from "catter";

io.TextFileStream.with("log.txt", "utf-8", (stream) => {
  const lines = stream.readLines();
  for (const line of lines) {
    io.println(line);
  }
});
```

Key methods:

| Method | Description |
|--------|-------------|
| `read(chars)` | Read N characters |
| `write(text)` | Write string |
| `readLine()` | Read one line (without newline) |
| `readLines()` | Read all lines into array |
| `readUntil(delimiter)` | Read until delimiter |
| `readEntireFile()` | Read entire file as string |
| `append(text)` | Append to end of file |

## os -- Operating System

```js
import { os } from "catter";

os.platform();  // "linux" | "macos" | "windows"
os.arch();      // "x86" | "x64" | "arm" | "arm64"
```

## http -- HTTP Client

### Quick Methods

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

### Full Request

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

### Response Object

| Field/Method | Type | Description |
|-------------|------|-------------|
| `status` | `number` | HTTP status code |
| `ok` | `boolean` | True if status is 2xx |
| `url` | `string` | Final URL (after redirects) |
| `body` | `string` | Response body |
| `headers` | `Record<string, string>` | Normalized headers (lowercase keys) |
| `text()` | `string` | Returns body as string |
| `json()` | `T` | Parses body as JSON |
| `header(name)` | `string \| undefined` | Get header by name (case-insensitive) |

### Request Options

| Option | Type | Default | Description |
|--------|------|---------|-------------|
| `method` | `string` | `"GET"` | HTTP method |
| `headers` | `Record \| Array` | `{}` | Request headers |
| `body` | `string` | `""` | Request body |
| `timeoutMs` | `number` | `-1` (no timeout) | Request timeout |
| `maxRedirects` | `number` | `-1` (default) | Max redirects to follow |
| `proxy` | `string` | `""` | Proxy URL |

### Managed Client

For multiple requests, create a reusable client:

```js
const client = new http.Client();
const res = await client.get(url);
client.close();
```

## time -- Time Utilities

### Timestamps

```js
import { time } from "catter";

time.now();          // Unix timestamp in ms (same as time.unixMs())
time.unixMs();       // Unix timestamp in milliseconds
time.unixUs();       // Unix timestamp in microseconds
time.unixSeconds();  // Unix timestamp in seconds
time.monotonicMs();  // Monotonic clock in ms (for elapsed time)
time.monotonicUs();  // Monotonic clock in us
```

### Duration Conversion

All duration helpers produce or consume milliseconds:

```js
time.ms(1500);       // 1500 (identity, for clarity)
time.seconds(2);     // 2000
time.minutes(1);     // 60000
time.hours(1);       // 3600000
time.days(1);        // 86400000
time.ns(1000000);    // 1
time.us(1000);       // 1
```

Convert between arbitrary units:

```js
time.convert(2, "s", "ms");  // 2000
time.toMs(5, "s");           // 5000
time.fromMs(60000, "min");   // 1
```

### Elapsed Time

```js
const start = time.monotonicMs();
// ... do work ...
const ms = time.elapsedMs(start);
const seconds = time.elapsed(start, "s");
```

## debug -- Assertions

```js
import { debug } from "catter";

debug.assertPrint(condition);         // prints "assertion failed!" on failure
debug.assertThrow(condition);         // throws Error on failure
debug.assertDo(condition, () => {     // runs callback on failure
  io.println("custom failure handler");
});
```

## cli -- Script Argument Parsing

The `cli` module provides a declarative argument parser for script options. It is typically used inside `onStart` to parse `config.scriptArgs`.

### Defining a Command

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

### Parsing Arguments

Use `cli.run()` for the simplest workflow -- it prints help/errors automatically and returns parsed values or `undefined`:

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

For more control, use `cli.parse()` (returns a result object) or `cli.parseOrThrow()` (throws `CLIParseError`):

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
// result.values is the parsed values object
```

### Option Types

| Factory | Value Type | Description |
|---------|-----------|-------------|
| `cli.flag(name, opts)` | `boolean` | Boolean flag, defaults to `false` |
| `cli.string(name, opts)` | `string` | String-valued option |
| `cli.number(name, opts)` | `number` | Number-valued option (with optional `min`, `max`, `integer`) |
| `cli.positional(name, opts)` | `string` | Positional argument |

All option types support `short`, `description`, `required`, `multiple`, `default`, and `hidden`. Number options additionally support `min`, `max`, and `integer`. String and number options support a `parse` function for custom validation.

### Usage Text

Generate formatted help text programmatically:

```js
const helpText = cli.usage(myCommand);
io.println(helpText);
```

Built-in `-h` / `--help` handling is enabled by default. Set `help: false` in the command definition to disable it.
