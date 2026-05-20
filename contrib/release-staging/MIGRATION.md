# Migration playbook: from `contrib/release-staging/` to the public repo

This document is the step-by-step recipe for extracting this staged content
into a new, clean, public GitHub repository when testing is complete.

## Pre-flight checklist

Before running the migration:

- [ ] All seven plugins tested in at least one production OFX host. Results
      logged in [`RELEASE_SPEC.md` §10](RELEASE_SPEC.md#10-testing-plan).
- [x] Suite name decided: `AIFX`.
- [x] Repo location decided: `Dev-Reepost/aifx`.
- [ ] CNC funding wording finalized. Update the placeholder in `README.md`,
      `docs/index.md`, and `_config.yml` footer.
- [ ] Final review of model weight license summaries on each plugin page.
- [ ] A round of plugin-output screenshots captured to populate
      `docs/assets/plugins/<plugin>/`. Replace any "image candidates marked
      not redistributable" notes.
- [x] OpenFX dependency strategy decided: **CMake `FetchContent`**, pinned
      to a specific OpenFX tag. See [§3 below](#3-resolve-the-openfx-dependency)
      for the CMake snippet.

## Migration steps

### 1. Initialize the new repo

```bash
mkdir <new-repo>
cd <new-repo>
git init
git checkout -b main
```

### 2. Copy staged content

```bash
SRC=/path/to/openfx/contrib/release-staging
cp -R "$SRC"/{README.md,LICENSE,CITATION.cff,CHANGELOG.md,CONTRIBUTING.md,_config.yml} .
cp -R "$SRC/docs" .
cp -R "$SRC/RELEASE_SPEC.md" .   # optional — keep as internal reference
```

The `_research/` folder is staging-only and **does not** get copied.

### 3. Resolve the OpenFX dependency

The plugins depend on the OpenFX C API and the C++ Support library from
`AcademySoftwareFoundation/openfx`. AIFX uses CMake `FetchContent` to pull
these in at configure time, pinned to a specific OpenFX tag. Add this near
the top of the new repo's root `CMakeLists.txt`:

```cmake
include(FetchContent)
FetchContent_Declare(
    openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG        OFX_Release_2_0_BRANCH   # pin to a tag or commit SHA
)
FetchContent_MakeAvailable(openfx)
```

Then add include paths and link against the OpenFX Support targets the
plugins use (e.g. `OfxSupport`). Pick the exact `GIT_TAG` by checking the
last known-good build in the openfx upstream fork.

**Why FetchContent over the alternatives:** submodules force contributors
to remember `--recurse-submodules` (constant footgun); vendoring commits
OpenFX source into the AIFX repo and creates upstream-drift maintenance
debt. FetchContent has neither problem and upgrades are a one-line
`GIT_TAG` bump.

### 4. Copy plugin sources from the upstream fork

```bash
SRC_OFX=/path/to/openfx
mkdir -p plugins
cp -R "$SRC_OFX/contrib/plugins/ComfyUI/common"           plugins/common
for p in depth_da3 depth_crafter normal_crafter segmentation_sam3 \
         matte_mama matte_ma2 upscale_seedvr2 ; do
  cp -R "$SRC_OFX/contrib/plugins/ComfyUI/$p" "plugins/$p"
done
cp "$SRC_OFX/contrib/plugins/ComfyUI/CMakeLists.txt"      plugins/CMakeLists.txt
mkdir -p tools
cp "$SRC_OFX/contrib/dev-tools/build-plugin.sh"           tools/
```

Adjust `plugins/CMakeLists.txt` to remove the `add_subdirectory()` lines for
the plugins that are NOT in scope for V1 (`segmentation`, `anycomfy`, etc.).

### 5. Adjust paths and placeholders

- Sweep for any remaining `{{...}}` placeholders:
  ```bash
  grep -r "{{[A-Z_]*}}" .
  ```
  At the time the staging tree was prepared, the following placeholders were
  already resolved:
  - `{{SUITE_NAME}}` → `AIFX` (display name / branding)
  - `{{REPO}}` → `aifx` in URL contexts, `AIFX` in display contexts
    (GitHub repo paths follow the lowercase convention)
  - `{{ORG}}` → `Dev-Reepost`
  - `{{YEAR}}` → `2026`

  Anything new added later will surface here.

- Update CMake include paths and `OFX_ROOT` references to match the new
  layout. The `build-plugin.sh` script needs its `OPENFX_ROOT` detection
  updated for the new repo structure.

### 6. Set up CI

Add `.github/workflows/build.yml` for the build matrix and
`.github/workflows/pages.yml` for the Jekyll site. Templates:

- Build: cmake-action across `macos-14`, `ubuntu-22.04`, `windows-2022`.
- Pages: `actions/configure-pages@v4` + `actions/jekyll-build-pages@v1` +
  `actions/deploy-pages@v4`, scoped to push-on-`main`.

### 7. First commit

```bash
git add .
git commit -m "Initial public release"
```

Do **not** carry over commit history from the upstream fork. The intent is a
clean root commit.

### 8. Push and configure the GitHub repo

```bash
git remote add origin https://github.com/Dev-Reepost/aifx.git
git push -u origin main
```

Then in the GitHub web UI:

- Settings → Pages → Source: GitHub Actions.
- Settings → General → enable Issues, Discussions (optional).
- Branch protection on `main`: require PR reviews, require status checks.

### 9. Tag the release

```bash
git tag -a v1.0.0 -m "Initial public release"
git push origin v1.0.0
```

In Releases, attach prebuilt `.ofx.bundle` archives once CI produces them.

### 10. Announce

Update `CHANGELOG.md` with the released version. Move the `[Unreleased]`
heading content to `[1.0.0] — YYYY-MM-DD`.

## Post-migration cleanup: archive the openfx fork

The `Dev-Reepost/openfx` fork was the development tree where AIFX was built.
Once `Dev-Reepost/aifx` is live and verified, the fork stops being the
source of truth for plugin work. Two things happen:

1. **Update the fork's `README.md`** to a short banner at the top:

   > **This fork is no longer the home of AIFX.** Active development has
   > moved to [Dev-Reepost/aifx](https://github.com/Dev-Reepost/aifx).
   > This fork remains for historical context and any future contributions
   > back to [AcademySoftwareFoundation/openfx](https://github.com/AcademySoftwareFoundation/openfx).

2. **Archive the fork** (Settings → General → Archive this repository).
   Once archived, the fork becomes read-only and visually flagged as such.
   It stays accessible for git history and reference, but cannot accept new
   commits or issues. If you later need to contribute a patch back to
   upstream OpenFX, you can un-archive temporarily.

The fork's commit history (including all of AIFX's development) remains
intact and browsable. The new `aifx` repo starts at a fresh root commit
by design — it carries the *result* of the work, not the development log.

## Open issues to address before V1.0.0

See [RELEASE_SPEC.md §11](RELEASE_SPEC.md#11-open-decisions) for the
authoritative list. The remaining headlines are:

1. Final CNC funding wording.
2. Code of Conduct (Contributor Covenant v2.1 unless preferred otherwise).
3. Plugin demo imagery — embed direct upstream image URLs in the
   "Demos & comparisons" sections of each plugin page (the fair-use
   strategy and central credits page are already in place), and add
   self-generated screenshots as plugins are tested.

Resolved: suite name (`AIFX`), repo location (`Dev-Reepost/aifx`),
copyright year (`2026`), OpenFX dependency strategy (`FetchContent`).
