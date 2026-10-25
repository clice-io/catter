# Contributing

## Testing

Two kinds of tests: C++ unit tests, and the cases, which run the built catter on small projects and compare every output with a snapshot.

```bash
# Run everything: configure, build, unit tests and cases (debug, or another mode)
pixi run -e dev test

# Unit tests only
pixi run -e dev unit-test

# Cases only, against the release build (or debug, releasedbg)
pixi run -e dev cases-test release
```

### Unit Tests

Unit tests use the Kotatsu testing framework (`kota::zest`). Located in `unittests/`, the structure mirrors `src/`:

- `unittests/common/` -- Tests for shared utilities and option parsing
- `unittests/catter/` -- Tests for core logic (compiler identification, argument parsing, the built-ins, the handler protocol)
- `unittests/catter-hook/unix/` -- Tests for Unix hook payload

To enable test targets in the build:

```bash
pixi run cfg  # or: xmake config --test=y
```

Each test binary runs its tests on a pool of worker processes, several at once, with a 60 second limit per test. A suite that uses a fixed file or directory name must run alone: mark it with `ZEST_SUITE_ATTRS(serial = true);`. To debug a test, run it in the binary's own process with `--no-isolation --test-filter=<suite>.<test>`; `--help` lists the other options.

### Cases

Every test of catter's behavior is a case under `tests/<topic>/<subtopic>/NN_name/`: the inputs of a small project, an optional `test.ts` saying what to run, and a `snap.toml` holding every output the case pins. The runner (`tools/testing/`, TypeScript on vitest) runs the catter of a build, so the cases hold whatever builds catter and whatever language it is written in. `tests/README.md` is the contract: writing a case, the placeholders paths are normalized to, platform differences and known bugs.

```bash
# The cases under a path
CATTER_CASES=builds/make pixi run -e dev cases-test

# Accept the new outputs of the cases that changed; review the diff of snap.toml
UPDATE_SNAPSHOTS=1 pixi run -e dev cases-test

# Another catter than a build's
CATTER_EXECUTABLE=/path/to/catter pixi run -e dev pnpm --dir tests run cases
```

## Commit Message Format

Use [conventional commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <short description>
```

**Types**: `feat`, `fix`, `refactor`, `chore`, `build`, `docs`, `ci`, `test`, `perf`, `style`, `revert`

Scopes should match source directories or feature names. Keep the subject line under 70 characters.

## Code Style

- C++23 standard
- Use `.clang-format` for C++ formatting
- Use ESLint + Prettier for TypeScript
- `pixi run -e format format` formats every source file; CI fails on any change it makes
