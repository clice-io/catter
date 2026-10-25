---
name: translate-docs
description: Read before translating, reviewing or editing any docs/zh page — catter's glossary and page conventions, on top of the shared translation rules of clice-io/docs.
---

# Translating the catter docs into Chinese

catter is a build-process interception tool; its zh docs follow the shared
rules of clice-io/docs, **RULES.md at v1.0.0** — the contract, what stays
verbatim by position on the page, and style:

    gh api 'repos/clice-io/docs/contents/tools/translations/RULES.md?ref=v1.0.0' -H 'Accept: application/vnd.github.raw'

Read it first. This page adds only what is catter's: the page conventions
of its site and its glossary. `pixi run -e dev review-doc-translations`
hands this page to the model as the glossary, so it speaks to a translator.
The mechanics (`report`/`record`/`check`, the end of a branch) are in the
docs skill.

## By position on the page

| Where                             | Rule                                                                                                                                                                                                                       |
| --------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Sidebar groups                    | Translated: 指南 / 功能 / 脚本系统 / 设计 / 开发. Product names `catter`, `clice`, `kotatsu` stay lowercase English.                                                                                                       |
| Page and section headings (h1–h3) | Translated on every page including design/. Headings that are identifiers stay verbatim: `initialize`, `command`, `catter-proxy`. A planned feature's heading carries （计划中）, and planned status is 计划中 everywhere. |
| Home page frontmatter             | `text`, `tagline`, the feature cards' `title` and `details`, the actions' `text` are translated; `link`, `src`, `theme` stay.                                                                                              |
| Callouts                          | The `> [!WARNING]` marker stays; its text is translated (此功能尚在计划中，尚未实现。).                                                                                                                                    |
| Table headers                     | Field → 字段, Type → 类型, Description → 说明, Meaning → 含义, Option → 选项, Default → 默认值, Variable → 变量, Purpose → 用途.                                                                                           |
| Built-in and command names        | `compile-commands`, `catter`, `catter-proxy`, callback names (`onStart`), action names (`skip`, `drop`) and runtime names (`inject`, `env`, `eslogger`) are code, never translated.                                        |

## By term

Translate:

- Feature names, as the feature pages name them: 编译数据库, 命令树, 目标树,
  伪编译, 构建分析; CDB stays CDB.
- catter's own concepts: daemon → 守护进程, proxy → 代理 (the program
  stays `catter-proxy`), hook → 钩子 in names and headings (钩子机制,
  钩子库; prose may keep hook, one choice per page; `catter-hook` and code
  stay), injection → 注入, interception → 拦截, session → 会话,
  command → 命令, callback → 回调, action → 动作, built-in → 内置功能,
  build system → 构建系统, child process → 子进程, linker → 链接器.
- runtime, catter's capture mechanism: the page title is
  运行机制（Runtime）; in prose it stays `runtime` (inject runtime).
- A page that keeps one of these terms in English — hook, proxy, or Action
  (the design pages' section on actions is titled and cross-referenced as
  Action) — keeps it that way; a translator follows the page, not the list.

Keep English (never transliterate):

- Product and tool names: CMake, Make, Ninja, MSBuild, xmake, Bazel,
  Meson, GCC, Clang, MSVC, clang-cl, nvcc, LLVM, Bear, VS Code.
- Acronyms: CDB, IPC, DLL, ABI, API, CLI, JSON, LSP.
- Platform mechanisms, as they are named: `LD_PRELOAD`,
  `DYLD_INSERT_LIBRARIES`, Endpoint Security, MinHook, Detours.
