---
name: format
description: Format every source file (C++, Python, JS/TS, Markdown, JSON, TOML, YAML) — run before every commit.
---

Run: `pixi run -e format format` — clang-format (C++), ruff (Python), Prettier (JS/TS, Markdown, JSON, YAML), tombi (TOML), over every tracked file. `xmake.lua` is not formatted, nor are the inputs of a case under `tests/` (test data whose bytes its snapshot pins; `tests/.fdignore`, `.prettierignore`): only its `test.ts` and `README.md`. Nor are the files `pixi run -e dev protocol` generates (`sdk/protocol.schema.json`, `sdk/python/catter/protocol.py`, `sdk/typescript/src/protocol.ts`), which `check-protocol` compares byte for byte. Linters are not part of it: `pixi run -e format lint-python` (ruff, with `ruff.toml`'s rule set) and `pixi run -e dev ci-check` (ESLint), both run by CI.

The lint workflow also runs autocorrect over `docs/` (CJK spacing and punctuation), which pixi does not install: after editing `docs/zh`, run `npx autocorrect-node --lint ./docs`.
