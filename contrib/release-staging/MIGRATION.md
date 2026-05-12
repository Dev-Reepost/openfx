# Migration playbook: from `contrib/release-staging/` to the public repo

This document is the step-by-step recipe for extracting this staged content
into a new, clean, public GitHub repository when testing is complete.

## Pre-flight checklist

Before running the migration:

- [ ] All seven plugins tested in at least one production OFX host. Results
      logged in [`RELEASE_SPEC.md` §10](RELEASE_SPEC.md#10-testing-plan).
- [x] Suite name decided: `AIFX`.
- [x] Repo location decided: `Dev-Reepost/AIFX`.
- [ ] CNC funding wording finalized. Update the placeholder in `README.md`,
      `docs/index.md`, and `_config.yml` footer.
- [ ] Final review of model weight license summaries on each plugin page.
- [ ] A round of plugin-output screenshots captured to populate
      `docs/assets/plugins/<plugin>/`. Replace any "image candidates marked
      not redistributable" notes.
- [ ] OpenFX dependency strategy decided (submodule, FetchContent, or
      vendored). See [§3 below](#3-resolve-the-openfx-dependency).

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
`AcademySoftwareFoundation/openfx`. Pick one:

**A. CMake `FetchContent` (recommended).** Pin to a known-good OpenFX tag:

```cmake
include(FetchContent)
FetchContent_Declare(
    openfx
    GIT_REPOSITORY https://github.com/AcademySoftwareFoundation/openfx.git
    GIT_TAG        OFX_Release_2_0_BRANCH
)
FetchContent_MakeAvailable(openfx)
```

Pros: no submodules to manage, reproducible builds, clean repo.
Cons: build-time download.

**B. git submodule.** Vendor a pinned commit:

```bash
git submodule add https://github.com/AcademySoftwareFoundation/openfx.git third_party/openfx
git submodule update --init --recursive
```

Pros: explicit version pin in tree, offline builds work.
Cons: contributors need to remember `--recurse-submodules`.

**C. Vendored copy.** Copy the OpenFX headers and Support library into the
repo. Not recommended — divergence from upstream becomes a maintenance burden.

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
  At the time the staging tree was prepared, `{{SUITE_NAME}}` → `AIFX`,
  `{{REPO}}` → `AIFX`, `{{ORG}}` → `Dev-Reepost`, and `{{YEAR}}` → `2026`
  were already resolved. Anything new added later will surface here.

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
git remote add origin https://github.com/Dev-Reepost/AIFX.git
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

## Post-migration cleanup in the upstream fork

After the new public repo is live:

- The `contrib/plugins/ComfyUI/`, `contrib/release-staging/`, `contrib/docs/`
  folders in the upstream fork can be left in place as historical record, or
  removed once the new repo is the canonical location.
- Update the upstream fork's `CLAUDE.md` and `README.md` to point users at
  the new repo.

## Open issues to address before V1.0.0

See [RELEASE_SPEC.md §11](RELEASE_SPEC.md#11-open-decisions) for the
authoritative list. The remaining headlines are:

1. OpenFX dependency strategy.
2. Final CNC funding wording.
3. Plugin demo imagery — embed direct upstream image URLs in the
   "Demos & comparisons" sections of each plugin page (the fair-use
   strategy and central credits page are already in place), and add
   self-generated screenshots as plugins are tested.

Resolved: suite name (`AIFX`), repo location (`Dev-Reepost/AIFX`),
copyright year (`2026`).
