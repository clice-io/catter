---
name: release
description: Read before any release work — handing a user a CI build, the nightly, tagging a stable release, the promote workflow, publishing the draft.
---

# Releases

## Versioning

Git tag `vX.Y.Z` == release page. Odd minor = pre-release (the nightlies),
even minor = stable. Nightlies compute `X.<odd>.YYYYMMDDHH` (UTC hour) on the
odd minor above the newest release — `0.1.*` today, `0.3.*` after stable
`v0.2.0` — so nobody picks version numbers by hand; the first stable is
`v0.2.0`. The build stamps no version and the binary reports none: each
packaged build makes `catter.tar.gz` (`catter.zip` for Windows),
`catter/{bin/, LICENSE}`, and its symbols, `catter-symbol.tar.xz` (`.zip`),
the GSYM of `catter` and `catter-proxy`, for `llvm-gsymutil`; the promote
workflow names them after the tag's version and the target triple,
`catter-<version>.<triple>.tar.gz` and
`catter-<version>.<triple>.symbols.tar.xz`, so a tag needs no version bump
in the tree. Release notes state the commit.

## CI builds (every green run)

Nothing to operate. Every `main` push and PR run that builds packages the
`RelWithDebInfo` programs its suites just validated, stripped, as workflow
artifacts: `package-x86_64-unknown-linux-gnu`, `package-aarch64-apple-darwin`,
`package-x86_64-w64-mingw32` and `package-aarch64-w64-mingw32`, kept 30
days; each build checks that its package depends on the system's libraries
alone (`scripts/check_artifact_deps.py`). To hand a fix to a user: point
them at the run's artifact (GitHub login required).

## Nightly (pre-release channel)

`nightly.yml` runs daily (cron, 02:43 UTC) and builds nothing. It picks the
newest `main` commit whose green `main` run — pushed, or run by hand with
`gh workflow run main.yml` — still has every package artifact above
(path-filtered runs such as docs-only commits build nothing); a pick older
than a tagged release means the artifact listing is stale, and is retried,
then fails the run. When that commit already has the newest nightly, the
scheduled run tags nothing. Otherwise it tags exactly that commit with the
nightly version, creates a draft pre-release with notes (the commit, the
changes since the previous tag, the download table), calls `publish.yml` to
attach the packages, and publishes the draft once every package is there.
Every run prunes odd-minor pre-releases older than 30 days or beyond the
newest 25, release and tag, but for the newest, whatever its age; stable
releases are never pruned.

A failed nightly means no nightly that day. A failed promote leaves the
draft and its tag: `gh run rerun <id> --failed` finishes and publishes it.
Otherwise fix main and dispatch it again, `gh workflow run nightly.yml`
(a dispatch skips only a nightly of the same hour) — with the maintainer's
approval like any release trigger. When main has built nothing for 30 days
(docs-only commits), every package has expired and each run fails to pick:
`gh workflow run main.yml` builds main's tip, and the next run promotes it.

## Stable release (manual, even minor)

Every step that tags or publishes needs the maintainer's explicit approval first.

1. Pick a nightly that is judged good and tag its commit:
   `git tag v0.2.0 <commit> && git push origin v0.2.0`. Its packages live
   as long as its `main` run's artifacts, 30 days; an older commit has none
   and cannot be built for `main` again: tag a newer nightly. When catter's
   ecosystem round exists — real projects over each
   nightly, which `nightly.yml` starts once it has tagged one, as clice's
   does — the nightly a stable tags is one whose round shows no new failure.
2. The tag push runs the `release` job of `main.yml`, which calls
   `publish.yml`. It builds nothing: it waits for the commit's `main` run
   to finish, takes the newest green one that still has every package
   artifact, downloads them, names them after the tag's version and their
   target, and uploads all of them to a **draft** release (created with
   generated notes, marked pre-release for an odd minor; an existing release
   is reused, its assets replaced with `--clobber`). It fails loudly,
   uploading nothing, when no green run has every package.
3. The draft stays hidden until the maintainer publishes it on the releases
   page, after editing the notes (the nightly's download table is a good
   template). Nothing publishes a stable release automatically.

A promote that failed because the commit's `main` run was red (a flaky
job) is redone without a new tag: `gh run rerun <run-id> --failed`, then
`gh workflow run publish.yml -f release_tag=<tag>` — with the maintainer's
approval like any release trigger.

## Plumbing changes

Packaging runs on every building CI run (the packaged builds of `test.yml`
build `//:package //:symbols` in `test-target.yml` and upload the
artifacts), so it is exercised by every code-touching PR. The release glue
has a dry run: it picks the commit, versions it, writes the notes, and
downloads and names the packages, then stops before the tag, and only lists
what it would prune. Every PR that changes `nightly.yml` or `publish.yml`
runs it (the `nightly` workflow's run on the PR); on main,
`gh workflow run nightly.yml -f dry_run=true`, and for `publish.yml` alone
`gh workflow run publish.yml -f release_tag=<tag> -f commit=<sha> -f dry_run=true`.
A dispatch without `dry_run` publishes for real.
