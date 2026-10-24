---
name: format
description: Format every source file (C++, Python, TypeScript; whitespace and YAML checks everywhere) — run before every commit.
---

Run: `pixi run -e dev format` (pre-commit over every file: clang-format, ruff, ESLint, Prettier, trailing whitespace and final newlines, YAML syntax). ESLint and Prettier need the pnpm workspace installed: `pixi run -e dev npm-install` first.

The lint workflow also runs autocorrect over `docs/` (CJK spacing and punctuation), which pixi does not install: after editing `docs/zh`, run `npx autocorrect-node --lint ./docs`.
