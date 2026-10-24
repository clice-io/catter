---
name: format
description: Format every source file (C++, Python, JS/TS, Markdown, JSON, TOML, YAML) — run before every commit.
---

Run: `pixi run -e format format` — clang-format (C++), ruff (Python), Prettier (JS/TS, Markdown, JSON, YAML), tombi (TOML), over every tracked file. `xmake.lua` is not formatted. ESLint is a lint, not a formatter: `pixi run -e dev ci-check`.

The lint workflow also runs autocorrect over `docs/` (CJK spacing and punctuation), which pixi does not install: after editing `docs/zh`, run `npx autocorrect-node --lint ./docs`.
