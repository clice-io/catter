---
name: release
description: Read before any release work — handing a user a CI build, tagging a release, the promote workflow, publishing the draft.
---

# Releases

## Versioning

Git tag `vX.Y.Z` == release page; a tag with a `-` (`v0.1.0-alpha.4`) is a
pre-release. xpack names the packages after the version in `xmake.lua`
(`set_version`): `catter-<version>-<plat>-<arch>.*`; the promote workflow
renames them to the tag's version, so a tag needs no version bump in the
tree. The binary reports no version. Releases so far are
`v0.1.0-alpha.N` pre-releases.

## CI builds (every green run)

Nothing to operate. Every `main` push and PR run that builds packages the
release-mode binaries its suites just validated, as workflow artifacts:
`package-ubuntu-24.04`, `package-macos-15`, `package-windows-2025-mingw`
(x64) and `package-windows-11-arm-mingw` (ARM64), kept 30 days. To hand a fix to a user:
point them at the run's artifact (GitHub login required).

## Releasing a tag

Every step that tags or publishes needs the maintainer's explicit approval first.

1. Pick the commit: a `main` commit whose `main` run is green and built,
   less than 30 days old (the package artifacts expire). Path-filtered runs,
   e.g. of docs-only commits, build nothing; when such a commit is main's
   tip, `gh workflow run main.yml` builds it.
2. Tag it: `git tag v0.1.0-alpha.5 <commit> && git push origin v0.1.0-alpha.5`.
3. The tag push runs the `release` job of `main.yml`, which calls
   `publish.yml`. It builds nothing: it waits for the commit's `main` run
   to finish, takes the newest green one that still has every package
   artifact, downloads every package artifact above, renames them to the
   tag's version, and uploads them to a **draft** release (created with
   generated notes, marked pre-release for a `-` tag; an existing release
   is reused, its assets replaced with `--clobber`). It fails loudly when no
   green run with live packages exists.
4. The draft stays hidden until the maintainer publishes it on the releases
   page, after editing the notes. Nothing publishes it automatically.

A promote that failed (the run was still red, the artifacts expired) is
redone without a new tag: fix or rerun the commit's `main` run, then
`gh workflow run publish.yml -f release_tag=<tag>` — with the maintainer's
approval like any release trigger.

## Plumbing changes

There is no separate dry run: packaging runs on every building CI run (the
release builds of `test.yml` that name a package run `pixi run -e dev
package` in `test-target.yml` and upload the artifacts), so it is exercised
by every code-touching PR. Only the promote glue (`publish.yml`) is release-time-only, and it has no dry run either:
dispatching it for an existing tag replaces that release's assets.
