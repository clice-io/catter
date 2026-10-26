---
name: docs
description: Read before editing anything under docs/ — the en and zh trees, the translation checker and its pixi commands, where the site is built and published, what belongs in the docs.
---

# Documentation system

## Layout and publishing

- `docs/en/` — English pages: `guide/`, `features/`, `scripting/`,
  `design/`, `dev/`, and `index.md` (the VitePress home page, whose
  frontmatter carries the hero and the feature cards).
- `docs/zh/` — Chinese pages, equally real and equally hand-edited (by a
  person or a model). Every en page has a zh page at the same path, and
  stays **segment-isomorphic** to it: same sequence of markdown blocks,
  translated text in the translatable blocks, code blocks and HTML comments
  byte-identical.
- `docs/meta/translations/` — the hash pairs attesting each translated
  segment. Maintained exclusively by `record`; never edit by hand.
- Each tree has its own hand-maintained `sidebar.yaml`.
- The site is docs.clice.io (catter under `/catter/`), built from the
  clice-io/docs repository: the `docs-check` job of `main.yml` builds it
  with a PR's pages (markup VitePress rejects fails there), and every push
  to main that touches `docs/` syncs the pages there (`deploy-docs`).
  Images and other assets live in clice-io/docs, not here.
- The lint workflow runs `autocorrect --lint ./docs` (CJK spacing and
  punctuation) whenever docs change.

## Commands (pixi)

| command                                   | what it does                                       |
| ----------------------------------------- | -------------------------------------------------- |
| `pixi run -e dev check-doc-translations`  | hard gate: zh isomorphic to en, all pairs attested |
| `pixi run -e dev report-doc-translations` | translator worklist: drifted segments with texts   |
| `pixi run -e dev record-doc-translations` | re-attest hash pairs after deliberate edits        |
| `pixi run -e dev review-doc-translations` | model review of zh pages, segment by segment       |

## Translation contract

The checker is `@clice-io/translate` from the clice-io/docs repository,
shared by every clice-io project with a Chinese tree. Its **RULES.md at
v1.0.0** states the contract, the commands and the editing workflow — read
it before any edit touching translated pages:

    gh api 'repos/clice-io/docs/contents/tools/translations/RULES.md?ref=v1.0.0' -H 'Accept: application/vnd.github.raw'

CI runs `clice-io/docs/check-translations` in the lint workflow's
`translations` job whenever `docs/` changes, pinned to an exact release;
the pixi tasks above run that same release locally
(`tools/docs/translations.ts` reads the pin, until the package is on npm
and the tasks call `npx @clice-io/translate@1`). A new release is adopted
by bumping the pin in lint.yml and the RULES.md references here and in the
translate-docs skill, with `check-doc-translations` green on the new
release. What catter adds on top:

- The frontmatter that translates is index.md's (the VitePress home page).
- The formatter of step 4 in RULES.md's workflow is
  `pixi run -e format format` (Prettier canonicalizes table padding and
  emphasis style); CJK spacing and punctuation are autocorrect's, which CI
  runs (`npx autocorrect-node --lint ./docs` here). Both change segment
  hashes: fix what autocorrect reports and format before `record`.
- The wording — catter's page conventions and glossary — is the
  translate-docs skill. `review-doc-translations [page...]` passes that
  skill to the model as the glossary; review its diff, then `record`.

## At the end of a branch

A branch that changed en pages translates them once, right before the
checks of the pr skill, not after every edit: `report-doc-translations`
lists every broken pair with both texts; translate each new or drifted en
segment into the zh page, keeping the skeleton and the terminology of the
surrounding page, and delete zh segments whose en segment is gone (a whole
new page: copy the en page over the zh one and `review` it). A page added,
removed or renamed in `docs/en/sidebar.yaml` changes `docs/zh/sidebar.yaml`
too — the checker reads only the pages. Then fix what autocorrect
reports, `pixi run -e format format`, `record`, and `check`. For more
than a page or two, delegate it to an Opus subagent with this skill, so the
page texts stay out of the main context.

## What the docs describe

The docs describe what catter does today, for its users. Planned work
appears only where a page already says it is planned, and leaves the
page when it ships or is dropped. A change to behavior, options or output a page
describes updates the page — both trees — in the same PR.

**docs/ contains no changelog content** — neither per-page "Changelog"
sections nor changelog pages. History lives in git, PRs and the release
notes.

## What belongs in a "Known Limitations" section

Design-level, user-visible trade-offs that are stable on a months timescale,
written in behavior terms — they answer the reader's "why does it work this
way". Bugs never go there: they live in the issue tracker and simply
disappear when fixed; putting them in docs creates staleness debt. The flow
is one-way: an issue graduates into a doc limitation only once it is decided
to be design (or long-term deferral), and a doc limitation is removed only
when the design changes. Never reference internal IDs, file paths, or
timelines in docs.
