---
name: release-drumlogue-unit
description: Release a Korg drumlogue DSP unit. Use when the user asks to "release", "create a release", "tag", or "bump the version" of a unit (e.g. "release elementish 1.4.0", "tag drupiter v1.1.0"). Bumps the version in header.c, updates RELEASE_NOTES.md and README.md, builds, creates and pushes the <unit>/v<version> tag (which triggers the Release Unit workflow), verifies the GitHub release, and updates the GH Pages unit page in docs/units/.
license: MIT
compatibility: opencode
metadata:
  audience: developers
  workflow: release
---

# Release Drumlogue Unit Skill

You are releasing a Korg drumlogue DSP unit. Follow the steps in order. The
release has two independent halves: (1) the unit itself (version, notes,
build, tag, GitHub release) and (2) the GH Pages docs site, which is **not**
updated automatically and must be updated manually.

## Step 1 — Gather Information

Ask for (or confirm) these details:

| Field | Description | Rules |
|---|---|---|
| **Unit directory name** | e.g. `elementish-synth` | must exist under `drumlogue/` with `header.c` |
| **Version** | Semantic `x.y.z` or `x.y.z-suffix` (e.g. `1.4.0`, `1.1.0-pre`) | must match `^[0-9]+\.[0-9]+\.[0-9]+(-[a-zA-Z0-9]+)?$` |
| **Working branch** | release happens on the branch the changes are on | if not already there, check out / merge first |

Confirm the unit is on the branch you intend to release (e.g. `main` after a
PR merge): `git branch --show-current`.

## Step 2 — Bump the Version

Update `drumlogue/<unit>/header.c` (hex encoding `x.y.z` → `0xMMNNPPU`):

```bash
make version UNIT=<unit> VERSION=<version>
```

Verify:
```bash
make -s get-version UNIT=<unit>   # must print the new version
```

## Step 3 — Update Release Notes and Docs

Update the unit's own docs (these live with the unit, separate from the GH
Pages site):

- `drumlogue/<unit>/RELEASE_NOTES.md` — add a new `## vX.Y.Z` section at the
  top. Summarize **Bug Fixes**, **New Features**, and **Regression Tests**.
  Match the existing tone: bullet list, bold lead-ins.
- `drumlogue/<unit>/README.md` — add the version to the **Version History**
  section. Also fix any now-outdated parameter tables / preset lists /
  behavior descriptions.

## Step 4 — Build and Verify

```bash
make build UNIT=<unit>        # or: ./build.sh <unit>
```

Check the output:
- Build succeeds with **no unexpected undefined symbols** (the build script
  prints `✓ No unexpected undefined symbols found`)
- Artifact exists: `drumlogue/<unit>/<project>.drmlgunit`

If the build fails, diagnose via `objdump -T` on the artifact and check
`config.mk` source lists (see the debugging instructions in this repo).

## Step 5 — Commit, Tag, and Push

Commit all pending changes first (version bump + notes + docs):

```bash
git add drumlogue/<unit> && git commit -m "Release <unit> v<version>"
git push
```

Create and push the tag:

```bash
make tag UNIT=<unit> VERSION=<version>        # creates tag <unit>/v<version>
git push origin <unit>/v<version>
```

## Step 6 — Let the Release Workflow Do Its Job

**Do NOT run `gh release create` manually.** Pushing the tag triggers the
`Release Unit` workflow (`.github/workflows/release.yml`), which builds the
unit in CI and creates the GitHub release with a **versioned artifact**
named `<unit>-v<version>.drmlgunit`. Manually creating the release first
causes duplicate assets on the release.

Watch the workflow:

```bash
gh run list --workflow=release.yml --limit 1 --json status,conclusion,headBranch
# wait until conclusion == "success"
```

Verify the release and its asset:

```bash
gh release view <unit>/v<version> --json assets,url --jq '{url, assets: [.assets[].name]}'
# asset name must be <unit>-v<version>.drmlgunit
```

If you accidentally created a manual release with a differently-named asset,
delete the duplicate: `gh release delete-asset <unit>/v<version> <bad-asset> --yes`.

## Step 7 — Update the GH Pages Unit Page

The release workflow does **not** touch the Jekyll site in `docs/`. The unit
page `docs/units/<unit>.md` goes stale (old version badge, wrong download URL,
outdated parameter/preset tables, missing version history). Update it:

1. **Frontmatter** — bump `version` and point `download_url` at the
   **versioned** asset produced by the workflow:
   ```yaml
   version: vX.Y.Z
   filename: <project>.drmlgunit
   download_url: https://github.com/cldmnky/drumlogue-units/releases/download/<unit>/v<version>/<unit>-v<version>.drmlgunit
   ```
2. **Content** — fix anything stale versus the unit's `README.md`:
   parameter tables (pages/ranges/names), preset tables, mode tables, and the
   **Version History** section (add the new version).
3. Commit and push — the `pages.yml` workflow deploys on `docs/**` changes:
   ```bash
   git add docs/units/<unit>.md && git commit -m "docs: update <unit> unit page to v<version>"
   git push
   ```

Verify the deployment:

```bash
gh run list --workflow=pages.yml --limit 1 --json status,conclusion   # wait for success
curl -s "https://cldmnky.github.io/drumlogue-units/units/<unit>/" | grep -oE "v[0-9]+\.[0-9]+\.[0-9]+" | sort -u | tail -1
# must show the new version
```

## Step 8 — Summary

Report:
- Version bumped to and build result (with log excerpt on failure)
- Tag name pushed: `<unit>/v<version>`
- GitHub release URL and asset name
- GH Pages URL and confirmed version on the live page
- Deployment instructions: copy `<unit>-v<version>.drmlgunit` to the
  drumlogue's `Units/<Synth|DelFX|RevFX|MasterFX>/` folder via USB mass
  storage (folder depends on the unit type in `header.c`)

## Gotchas (learned the hard way)

- **Never `gh release create` manually** when `release.yml` exists — the
  workflow creates the release with the convention-named asset. A manual
  release adds a duplicate, differently-named asset.
- **The docs site is not auto-updated** by any workflow — the release workflow
  only builds/attaches the artifact; the pages workflow only builds `docs/`.
  Someone must update `docs/units/<unit>.md` or the page shows a stale
  version and a dead download link.
- `download_url` must use the **versioned** artifact name
  (`<unit>-v<version>.drmlgunit`), not the raw SDK project name
  (`<project>.drmlgunit`) — the workflow renames the artifact on upload.
- `make tag` refuses to overwrite an existing tag; if the tag already exists
  after a failed push, delete it locally and remotely first
  (`git tag -d <tag> && git push origin :<tag>`).
- Version suffix releases (`1.1.0-pre`) are marked pre-release by the
  workflow automatically; `make version` encodes only the base `x.y.z`.
