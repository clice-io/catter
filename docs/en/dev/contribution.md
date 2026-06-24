# Contributing

## Testing

Three ways to run tests:

```bash
# Run everything (build + unit tests + integration tests)
pixi run -e dev test

# Unit tests only
pixi run -e dev unit-test

# Integration tests only
pixi run -e dev integration-test
```

### Unit Tests

Unit tests use the Kotatsu testing framework (`kota::zest`). Located in `tests/unit/`, the structure mirrors `src/`:

- `tests/unit/common/` -- Tests for shared utilities and option parsing
- `tests/unit/catter/` -- Tests for core logic (compiler identification, JS runtime)
- `tests/unit/catter-hook/unix/` -- Tests for Unix hook payload

To enable test targets in the build:

```bash
pixi run cfg  # or: xmake config --test=y
```

### Integration Tests

Integration tests use the [LLVM Lit](https://llvm.org/docs/CommandGuide/lit.html) framework. Located in `tests/integration/`, they compile and run C++ test programs, using FileCheck for output verification.

```bash
# Run integration tests with verbose output
lit ./tests/integration -sav
```

## Commit Message Format

Use [conventional commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short description>
```

**Types**: `feat`, `fix`, `refactor`, `chore`, `docs`, `ci`, `test`, `perf`, `style`, `revert`

Scopes should match source directories or feature names. Keep the subject line under 70 characters.

## Code Style

- C++23 standard
- Use `.clang-format` for C++ formatting
- Use ESLint + Prettier for TypeScript
- Pre-commit hooks are configured (`.pre-commit-config.yaml`)
