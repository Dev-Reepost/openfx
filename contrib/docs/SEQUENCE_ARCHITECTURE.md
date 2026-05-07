# Sequence vs Frame-Based Plugin Architecture

## Context

OpenFX operates on a per-frame model: the host calls `render(time)` once per frame,
and the plugin receives one image and produces one image. This is correct for
spatial-only effects.

Modern generative AI video models require **temporal context** — they process N
consecutive frames together to produce temporally-consistent output. Feeding them
one frame at a time produces flickering results with no temporal coherence.

This document describes how the ComfyUI OFX plugin system bridges that gap,
classifies all current plugins, and explains exactly how the decision is made and
enforced.

---

## Plugin Classification

### Frame-Based Plugins

Each OFX `render()` call maps to one ComfyUI job. One frame in, one frame out.
Processing is triggered by the **Enable Processing** boolean toggle in the UI.

| Directory | Display Name | `isSequencePlugin()` | `setTemporalClipAccess` | Model / Reason |
|---|---|---|---|---|
| `depth_da3` | ComfyUI Depth Anything 3 | `false` (default) | `false` | Purely spatial per-frame depth. No temporal attention. |
| `segmentation` | ComfyUI SAM Segmentation | `false` (default) | `false` | SAM1/2 + GroundingDINO operate on single images. |
| `anycomfy` | ComfyUI AnyComfy | `false` (default) | `false` | Workflow-agnostic by design. User controls the workflow. |

### Sequence-Based Plugins

One ComfyUI job covers the **entire** frame sequence. Multiple OFX `render()` calls
for the same sequence share a single job and read from its cached output.
Processing is primarily triggered by the **Collect & Process** push button in the UI
(see [Sequence Plugin UI: Collect & Process Button](#sequence-plugin-ui-collect--process-button) below).

| Directory | Display Name | `isSequencePlugin()` | `setTemporalClipAccess` | Model / Reason |
|---|---|---|---|---|
| `depth_crafter` | ComfyUI DepthCrafter | `true` (override) | `true` | Diffusion temporal window across frames (`window_size`, `overlap`). |
| `segmentation_sam3` | ComfyUI SAM3 Segmentation | `true` (override) | `true` | Mask propagation through all frames via `SAM3Propagate`. Cannot work on isolated frames. |
| `matte_mama` | ComfyUI MatteMaMa | `true` (override) | `true` | SAM3 propagation feeds VideoMaMa diffusion sampler. Both require the full sequence. |
| `matte_ma2` | ComfyUI MatAnyone V2 | `true` (override) | `true` | SAM3 propagation feeds MatAnyone2 recurrent memory network. |
| `upscale_seedvr2` | ComfyUI SeedVR2 Upscale | `true` (override) | `true` | Explicit temporal consistency via `temporal_overlap` and `batch_size`. |

---

## How the Decision Is Made and Enforced

### The mechanism: a single virtual method

`comfyui_base_plugin.h` declares:

```cpp
virtual bool isSequencePlugin() const { return false; }  // default: per-frame
```

Sequence plugins override this in their class header:

```cpp
// e.g. depth_crafter_plugin.h
bool isSequencePlugin() const override { return true; }
```

That is the **only** place the decision lives. There is no build flag, no config
file, no runtime detection. It is a fixed, compile-time property of the plugin class.

### Why not a build script argument?

A build argument (`--sequence`) would be wrong because:
- It would have to be passed every time and could silently be omitted or wrong.
- It doesn't change what gets compiled — it's not a dependency on different
  libraries or headers. GPU support (`-DOFX_SUPPORTS_CUDARENDER`) is a real
  build option because it changes what gets linked. Sequence vs per-frame does not.
- The decision belongs with the source code, co-located with the workflow and
  model knowledge, not at the invocation site.

Build command is identical for all plugins:

```bash
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI <TargetName> --install
```

### How to decide for a new plugin

Open the ComfyUI workflow JSON and find the model's node. Ask:

1. **Does the model have temporal parameters?**
   - `window_size`, `temporal_overlap`, `direction` (forward/backward/both),
     `batch_size` (video batch), `frame_idx`, memory state → **sequence plugin**
   - None of these → **per-frame plugin**

2. **Does the model propagate information across frames?**
   - Mask propagation (SAM3), optical flow, recurrent hidden state → **sequence plugin**
   - Each frame is processed independently → **per-frame plugin**

3. **Does `LoadEXR` in the workflow use `${IMAGE_LOAD_CAP}` as a template variable?**
   - Yes → definitely sequence (the cap is user-controlled and applies to a batch)
   - Hardcoded `0` or a fixed number → ambiguous, check (1) and (2) above

> Note: All workflows use `"filepath": "${INPUT_PATH}"` — this is **not** a
> distinguishing signal. The base class substitutes a folder path (sequence) or
> a file path (per-frame) depending solely on `isSequencePlugin()`.

---

## Workflow JSON Analysis

All workflow files use `${INPUT_PATH}` for `LoadEXR.filepath`. What differs is
whether the runtime value will be a folder or a file (determined by `isSequencePlugin()`),
and what `image_load_cap` is set to.

| Plugin | `LoadEXR.image_load_cap` | `SaveEXR.start_frame` | Notes |
|---|---|---|---|
| `depth_crafter` | `${IMAGE_LOAD_CAP}` | `${FRAME}` | Cap is user-controlled |
| `matte_mama` | `${IMAGE_LOAD_CAP}` | `${FRAME}` | Cap is user-controlled |
| `matte_ma2` | `${SAM3_IMAGE_LOAD_CAP}` + `${IMAGE_LOAD_CAP}` | `${FRAME}` | Two LoadEXR nodes (SAM3 reference + full sequence) |
| `segmentation_sam3` | `0` (hardcoded) | `${FRAME}` | Unlimited, hardcoded in workflow |
| `upscale_seedvr2` | `${IMAGE_LOAD_CAP}` | `${FRAME}` | Cap is user-controlled |
| `depth_da3` | `10` (hardcoded) | `${FRAME}` | Per-frame: cap irrelevant in practice |
| `segmentation` | `0` (hardcoded) | `${FRAME}` | Per-frame: only one frame is ever written |

`${FRAME}` in `SaveEXR.start_frame` is replaced by:
- Sequence plugins: `_sequenceStartFrame` (the first frame of the sequence, e.g. `1001`)
- Per-frame plugins: the current render frame `N`

This ensures output files are named to match the actual OFX frame numbers so the
cache check finds them.

---

## How `${INPUT_PATH}` Gets Its Value

The substitution chain at runtime:

```
isSequencePlugin() == true
  → writeInputSequence(startFrame, endFrame)
      writes: {in_dir}/{basename}/{basename}.{t:04d}.exr  for t in [start..end]
      returns: {in_dir}/{basename}/                        ← a folder
  → inputPaths["InputA"] = folder_path
  → customizeWorkflow(): ${INPUT_PATH} → folder_path      ← LoadEXR receives folder

isSequencePlugin() == false
  → writeInputImages(frame)
      writes: {in_dir}/{basename}.{frame:04d}.exr         ← a single file
  → inputPaths["InputA"] = file_path
  → customizeWorkflow(): ${INPUT_PATH} → file_path        ← LoadEXR receives file
```

The workflow JSON is identical in both cases. Only the substituted value differs.

---

## Render Flow

### Frame-Based

```text
OFX host calls render(frame=N)
  ├─ Cache hit? → load {out}/{basename}.{N:04d}.exr → fill OFX buffer → done
  └─ Cache miss:
       write {in}/{basename}.{N:04d}.exr          (single file, on render thread)
       customizeWorkflow(): ${INPUT_PATH} = file
                            ${FRAME}     = N
       submitJobAsync(frame=N, ...)               (one job per frame)
       return placeholder
```

### Sequence-Based

```text
OFX host calls render(frame=N)
  ├─ Cache hit? → load {out}/{basename}.{N:04d}.exr → fill OFX buffer → done
  ├─ _pendingSequenceOutputPrefix matches current config?
  │    ├─ Job QUEUED/PROCESSING → return placeholder
  │    └─ Job ended without output (failed) → clear pending, fall through
  └─ First miss (no active job):
       determine range [startFrame, endFrame] from clip range, capped by imageLoadCap
       write ALL frames synchronously on render thread:
         for t in [start..end]:
           fetchImage(t) → writeEXR({in}/{basename}/{basename}.{t:04d}.exr)
       customizeWorkflow(): ${INPUT_PATH} = {in}/{basename}/    (folder)
                            ${FRAME}     = startFrame           (not N)
       submitJobAsync(startFrame, {}, {folder}, firstOutputPath)
       set _pendingSequenceOutputPrefix
       return placeholder
       (background: ComfyUI loads folder, runs video model,
        writes {out}/{basename}.{startFrame:04d}.exr ...
               {out}/{basename}.{endFrame:04d}.exr)
```

Key differences:
1. `${INPUT_PATH}` is a **folder**, not a file
2. `${FRAME}` is `startFrame`, not the current frame `N`
3. One job per sequence (not one per frame)
4. All input frames are written before job submission

---

## Sequence Plugin UI: Collect & Process Button

Sequence plugins expose a **Collect & Process** push button (defined in
`describeCommonParameters` when `isSequencePlugin=true`). This is the primary
way users trigger processing — rather than waiting for a full-resolution render
to hit the submission path.

### Button flow (changedParam action thread)

1. **Collecting** (Status: Cyan)
   - The plugin iterates the clip frame range [startFrame, endFrame] (capped by imageLoadCap)
   - For each frame NOT already on disk in the input folder: `fetchImage(t)` → save to in-memory map
   - Status updates live: `"Collecting: N / M"` or `"Collecting: N / M (cached)"` for disk-cached frames
   - `fetchImage()` is valid here because changedParam runs on the OFX action thread

2. **Output cache check**
   - If ALL expected output EXRs already exist → log "cached" and return (no submission)
   - If any partial outputs exist → delete them (ComfyUI HQ-Image-Save SaveEXR refuses to overwrite)

3. **Writing** (Status: Orange) — background thread writes EXRs to network share
4. **Submitting** (Status: Amber) — background thread builds workflow JSON and calls queuePrompt()
5. **Processing** (Status: Yellow) — background monitor polls; status shows elapsed time and poll count, updates every ~0.5s
6. **Done** (Status: Green) or **Failed** (Status: Red)

### Status color reference

| Color | RGB | Meaning |
|---|---|---|
| Gray | (0.5, 0.5, 0.5) | `"Ready"` — idle, no job |
| Cyan | (0, 0.7, 1) | Collecting frames from timeline |
| Orange | (1, 0.55, 0) | Writing input EXRs to disk |
| Amber | (1, 0.75, 0) | Submitting workflow to ComfyUI |
| Yellow | (1, 0.9, 0.1) | ComfyUI processing |
| Green | (0, 0.8, 0.2) | Done — `"N frame(s) done"` |
| Red | (1, 0, 0) | Failed — `"Failed — [error message]"` |

### Render()-triggered path (still exists)

The original render()-based submission path is still active. When the host calls render() for a
frame and there is no active job and no cached output, the plugin will submit the sequence job
from the render thread (writing all frames synchronously). This path has no live status feedback
during collection — the host UI freezes while frames are written.

The button is the recommended workflow for sequence plugins because:
- Collection feedback is visible in real-time
- No host UI freeze during frame collection
- User controls when the expensive operation runs

---

## Path Structure

### Frame-Based

```
Input:  {mount}/in/{project}/{workflow}/{version}/{basename}.{N:04d}.exr
Output: {mount}/out/{project}/{workflow}/{version}/{basename}.{N:04d}.exr
```

### Sequence-Based

```
Input folder: {mount}/in/{project}/{workflow}/{version}/{basename}/
Input files:  .../{basename}/{basename}.{t:04d}.exr   for t in [start..end]
Output:       {mount}/out/{project}/{workflow}/{version}/{basename}.{N:04d}.exr
```

Output path structure is identical — the cache check is unchanged.

---

## Adding a New Sequence Plugin

1. Implement the plugin as usual (inherit `BasePlugin`, implement `buildWorkflow()`)
2. In the `.h` file, add:
   ```cpp
   bool isSequencePlugin() const override { return true; }
   int  getImageLoadCap()  const override;  // if plugin has imageLoadCap param
   ```
3. In the `.cpp` file, implement `getImageLoadCap()` if applicable:
   ```cpp
   int MyPlugin::getImageLoadCap() const {
       int cap = 0;
       if (_imageLoadCap) _imageLoadCap->getValue(cap);
       return cap;
   }
   ```
4. In the factory `describe()`, change:
   ```cpp
   desc.setTemporalClipAccess(true);  // was false
   ```
5. Ensure the workflow JSON uses `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, and `"${FRAME}"`
6. Build and install as normal — no build flags needed:
   ```bash
   ./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI/<dir> <Target> --install
   ```

---

## References

- `spacepxl/ComfyUI-HQ-Image-Save` — LoadEXR / SaveEXR nodes
- `akatz-ai/ComfyUI-DepthCrafter-Nodes` — DepthCrafter
- `PozzettiAndrea/ComfyUI-SAM3` — SAM3VideoSegmentation, SAM3Propagate
- `FuouM/ComfyUI-MatAnyone` — MatAnyone2
- `kijai/ComfyUI-SeedVR2_VideoUpscaler` — SeedVR2VideoUpscaler
