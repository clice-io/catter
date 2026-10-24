# Issue Triage Rules

The label taxonomy lives in `.github/labels.yml` — the single source of
truth for label names and descriptions. These rules define how to choose
labels for an issue; they bind model classifiers and human triage equally.

## General

- Decide from the issue title, body and comments only. Never guess beyond
  the evidence — a missing label is better than a wrong one.
- The full label set describes the issue; do not stop at the first label
  that fits.

## kind: — exactly one, always

Every issue gets exactly one `kind:` label:

- `kind:crash` — catter crashes, asserts or hangs, or breaks the
  intercepted build (a command that dies or hangs only under catter).
  Takes precedence over `kind:bug` when both apply.
- `kind:bug` — observed behavior differs from what a user may reasonably
  expect.
- `kind:performance` — wrong speed or resource usage, not wrong result
  (a wrong result is a bug even if slowness is mentioned).
- `kind:feature-request` — a concrete capability or improvement is asked
  for.
- `kind:discussion` — open-ended design or direction thread without a
  single concrete deliverable; typical for maintainer-authored design and
  planning threads. Litmus: "should we / how should we do X" is a
  discussion; "please support X" is a feature request. A tracking issue
  that enumerates concrete planned work is NOT a discussion — it gets the
  kind of the work it tracks (usually `kind:feature-request`) plus the
  `tracking` label.
- `kind:question` — the reporter asks how to use or understand something;
  an answer closes the issue.
- `kind:docs` — documentation missing, wrong or unclear.

## area: — the affected part of catter

Apply when identifiable. Prefer the single primary area; two only when
the issue is genuinely cross-cutting. Attribute by where the fault lies,
which the symptom alone often does not tell:

- A command missing from the output because catter never saw the process
  → `area:hook`; a command seen but recorded wrong → `area:compiler-args`
  (the compiler not recognized, its arguments misparsed) or `area:cdb`
  (what the database makes of a parsed command: entries, paths, merging,
  the output file).
- A command that behaves differently under catter (arguments, environment,
  exit status, output, terminal, signals) → `area:proxy`, or `area:hook`
  when it is the hook that rewrites it in the parent process.
- Hangs or lost messages between catter, the proxy and the hook, and two
  sessions interfering → `area:ipc`.
- The `catter/*` modules and the callbacks a user script sees →
  `area:script-api`; the `catter` command line itself → `area:cli`.

If no area label fits, omit it and report the gap via `taxonomy_gap`.

## os: / compiler: / toolchain: — evidence only

- Apply only when stated or unambiguous from logs and paths (e.g.
  `/usr/include/c++` → linux, `C:\` or `.exe` → windows).
- WSL environments get `os:wsl` alone — never combined with `os:linux` or
  `os:windows`.
- Platform-independent issues (feature requests, design threads) get no
  os label.
- `compiler:` is the compiler the intercepted build runs, and
  `toolchain:` how the intercepted project is built (cmake, make, msbuild,
  xmake, ...) — never what catter itself is built with (that is `build`).

## status: — thread state, by checklist

- `status:needs-repro` — a bug/crash whose thread contains neither steps
  nor a project that reproduces it.
- `status:needs-info` — acting requires information the reporter has not
  given (version, logs, config, exact command). An empty-body issue from a
  non-maintainer is automatically needs-info.
- `status:needs-investigation` — reproduction material is present but the
  cause is unknown.
- `status:confirmed` — only when a maintainer states they reproduced it.
- `status:blocked-upstream` — only when the thread says so explicitly.
- `status:wontfix` — only when a maintainer says so.
- Closed issues get no `status:` labels at all — the thread is settled.

## Scope labels

- `build` — building catter itself from source.
- `packaging` — prebuilt binaries, releases, distribution.
- `ci` — CI workflows and repository automation.
- `tests` — test infrastructure and test failures.
- `security` — security-related issues.
- `tracking` — the issue tracks progress of a larger work area (checklist
  of planned work, umbrella issue). Combine with a normal `kind:`.

## Restricted

- `regression` — only when "used to work" is explicitly claimed.
- `duplicate` — only when you can cite the duplicated issue number in the
  rationale.
- `invalid` — only when a maintainer says the report is not a catter
  issue.
- NEVER apply `good first issue` or `help wanted` — maintainer judgment
  only.
- Never include `needs-triage` or `triaged` in the output — they are
  process markers managed by the triage tooling itself, not
  classification facts.

## Title normalization

Issue titles follow the PR-title convention:

```
<type>(<scope>): <summary>        or, with no fitting scope:  <type>: <summary>
```

- `type` derives from the kind label: `bug`, `crash`, `perf`
  (kind:performance), `feat` (kind:feature-request), `docs`, `question`,
  `discussion`.
- `scope` = the primary `area:` label without its prefix
  (e.g. `hook`, `cdb`), or a scope label (`build`, `packaging`, `ci`,
  `tests`). Omit when nothing fits.
- `summary` — concise, lowercase start, under 70 characters, preserves the
  reporter's meaning. Reporters are never required to write titles this
  way — normalization happens at triage time.

## Classifier output schema

Reply with ONLY a JSON array, one object per issue:

```json
[
  {
    "issue": 0,
    "labels": [],
    "confidence": "high|medium|low",
    "rationale": "<one sentence>",
    "ask_reporter": null,
    "better_title": null,
    "taxonomy_gap": null
  }
]
```

The reply is always an array, even for a single issue.

- `labels` — the complete label set for the issue (including labels it
  already has when they remain correct).
- `confidence` — high: every label evidence-backed; medium: some judgment
  calls; low: thin evidence (empty body, vague report).
- `ask_reporter` — what a maintainer would need to ask before acting, or
  null when the issue is actionable as-is.
- `better_title` — the normalized title per the title-normalization
  section; null only when the current title already conforms.
- `taxonomy_gap` — what label is missing from the taxonomy, or null.
