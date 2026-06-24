# 编译器选项解析

`option` 模块提供主流 C/C++ 工具链的完整选项表解析，数据源自 LLVM 的选项定义。它可以解析、查询、重写和跨工具链转换编译器选项。

## 支持的选项表

| 选项表 | 工具链 |
|--------|--------|
| `"clang"` | Clang/GCC 驱动选项 |
| `"nvcc"` | NVIDIA CUDA 编译器 |
| `"lld-elf"` | LLD 链接器 (ELF/Linux) |
| `"lld-coff"` | LLD 链接器 (COFF/Windows) |
| `"lld-macho"` | LLD 链接器 (Mach-O/macOS) |
| `"lld-mingw"` | LLD 链接器 (MinGW) |
| `"lld-wasm"` | LLD 链接器 (WebAssembly) |
| `"llvm-lib"` | LLVM 库管理器 |
| `"llvm-dlltool"` | LLVM DLL 工具 |

## 收集解析结果

将参数数组中的所有选项解析为结构化数据：

```js
import { option } from "catter";

const args = ["-std=c++20", "-O2", "-c", "main.cc", "-o", "main.o"];
const items = option.collect("clang", args);

if (typeof items === "string") {
  throw new Error(`Parse error: ${items}`);
}

for (const item of items) {
  const meta = option.info("clang", item);
  io.println(`${meta.prefixedKey} => ${item.values.join(", ")}`);
}
```

## 流式解析

使用回调而非收集方式进行解析：

```js
import { option } from "catter";

option.parse("clang", args, (result) => {
  if (typeof result === "string") {
    io.println(`Error: ${result}`);
    return false; // 停止解析
  }
  io.println(`Option: ${result.key}, values: ${result.values}`);
  return true; // 继续
});
```

## 选项查询

获取已解析选项的元数据：

```js
const items = option.collect("clang", ["-std=c++20"]);
if (Array.isArray(items)) {
  const info = option.info("clang", items[0]);
  io.println(info.prefixedKey); // "-std="
  io.println(info.kind);       // OptionKindClass.JoinedClass
  io.println(info.help);       // 来自 LLVM 选项表的帮助文本
}
```

## 渲染选项

将解析后的选项项转换回命令行字符串：

```js
const str = option.stringify("clang", item);
io.println(str); // 例如 "-std=c++20"
```

## 别名解析

将别名选项转换为规范形式：

```js
const items = option.collect("nvcc", ["-ofoo.o"]);
if (Array.isArray(items)) {
  option.convertToUnalias("nvcc", items[0]);
  io.println(items[0].key); // "--output-file"
}
```

注意：`convertToUnalias()` 会原地修改传入的 item 对象并返回，方便链式调用。

## 重写参数

替换或删除参数数组中的选项：

```js
import { option } from "catter";

const rewritten = option.replace("clang", ["-Iold", "-O0", "main.cc"], (result) => {
  if (typeof result === "string") {
    throw new Error(result);
  }

  // 替换头文件搜索路径
  if (result.key === "-I") {
    return { ...result, values: ["new-include"] };
  }

  // 移除优化选项（返回字符串以替换该段）
  if (result.key === "-O") {
    return "-O2";
  }

  // 返回 undefined 保持不变
});

io.println(rewritten); // "-Inew-include -O2 main.cc"
```

回调可以返回：

| 返回值 | 效果 |
|--------|------|
| `undefined` | 保持原始文本 |
| `OptionItem` | 替换为渲染后的选项 |
| `string` | 替换为该字符串 |
| `string[]` | 替换为合并后的数组 |
| `boolean` | `true` 继续，`false` 停止 |

## 跨选项表转换

将参数从一个选项表转换到另一个，仅保留在目标表中有效的选项：

```js
import { option } from "catter";

const nvccArgs = ["--gpu-architecture=sm_70", "-std=c++17", "-O2"];
const clangArgs = option.table2table("nvcc", "clang", nvccArgs);

if (typeof clangArgs === "string") {
  throw new Error(clangArgs);
}

io.println(clangArgs.join(" ")); // 两个表中都有效的选项
```

该函数先用源表解析参数，将其拆分为逐选项的片段，然后用目标表重新解析每个片段。解析为 `UnknownClass` 或匹配排除 ID 的片段会被丢弃。

## OptionItem 结构

每个解析后的选项表示为 `OptionItem`：

| 字段 | 类型 | 描述 |
|------|------|------|
| `key` | `string` | 选项键（如 `"-std="`、`"-c"`、`"-I"`） |
| `values` | `string[]` | 选项值数组 |
| `id` | `number` | 选项表中的数字 ID |
| `unalias` | `number \| undefined` | 如果是别名，则为规范选项的 ID |
| `index` | `number` | 在原始 args 中的位置 |

## OptionInfo 结构

`option.info()` 返回的元数据：

| 字段 | 类型 | 描述 |
|------|------|------|
| `id` | `number` | 数字选项 ID |
| `prefixedKey` | `string` | 完整选项字符串（如 `"-std="`） |
| `kind` | `OptionKindClass` | 选项类型（见下文） |
| `group` | `number` | 分组 ID |
| `alias` | `number` | 别名目标 ID |
| `aliasArgs` | `string[]` | 解析别名时追加的参数 |
| `flags` | `number` | 选项标志位 |
| `visibility` | `number` | 平台可见性掩码 |
| `param` | `number` | 多参数选项的参数个数 |
| `help` | `string` | 来自 LLVM 表的帮助文本 |
| `meta_var` | `string` | 帮助文本中的占位符标签 |

## 选项类型

`OptionKindClass` 枚举描述了选项如何消费参数：

| 类型 | 描述 |
|------|------|
| `FlagClass` | 无值（如 `-c`、`-v`） |
| `JoinedClass` | 值直接连接在键后面（如 `-std=c++20`） |
| `SeparateClass` | 值在下一个参数中（如 `-o main.o`） |
| `JoinedOrSeparateClass` | 连接或分离均可（如 `-Ipath` 或 `-I path`） |
| `CommaJoinedClass` | 逗号分隔的值连接在键后面 |
| `RemainingArgsClass` | 消费所有剩余参数 |
| `RemainingArgsJoinedClass` | 键 + 剩余参数 |
| `MultiArgClass` | 固定数量的分离值参数 |
| `ValuesClass` | 多个分离的值 |
| `InputClass` | 位置输入（源文件） |
| `UnknownClass` | 未识别的选项 |
| `GroupClass` | 选项分组（非实际选项） |

## 可见性过滤

解析时可以按平台可见性过滤选项。这对于处理平台特定选项（如 MSVC 风格的 `/TC`）很有用：

```js
import { option } from "catter";

// 使用默认可见性解析（所有选项可见）
const items = option.collect("clang", args);

// 使用显式可见性掩码过滤平台特定选项
const clItems = option.collect("clang", args, ClangVisibility.CLOption);
```

每个选项表导出自己的可见性常量（如 `ClangVisibility.DefaultVis`、`ClangVisibility.CLOption`）。
