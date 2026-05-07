# ComfyUI OFX Plugins — Technical & Functional Reference

OpenFX plugins that integrate with a ComfyUI server to run AI models on image sequences
inside any OFX-compatible host (Autodesk Flame, Foundry Nuke, DaVinci Resolve, etc.).

---

## Table of Contents

1. [Plugins at a Glance](#plugins-at-a-glance)
2. [System Architecture](#system-architecture)
3. [Render Pipeline](#render-pipeline)
   - [Proxy / Thumbnail Renders](#proxy--thumbnail-renders)
   - [Frame-Based Render Flow](#frame-based-render-flow)
   - [Sequence-Based Render Flow](#sequence-based-render-flow)
4. [Shared Storage Layout](#shared-storage-layout)
5. [Common Parameters](#common-parameters)
6. [Plugin Reference](#plugin-reference)
   - [Depth Anything V3](#depth-anything-v3-depth_da3)
   - [DepthCrafter](#depthcrafter-depth_crafter)
   - [SAM Segmentation](#sam-segmentation-segmentation)
   - [SAM3 Segmentation](#sam3-segmentation-segmentation_sam3)
   - [MatteMaMa](#mattemama-matte_mama)
   - [MatAnyone V2](#matanyone-v2-matte_ma2)
   - [SeedVR2 Upscale](#seedvr2-upscale-upscale_seedvr2)
   - [AnyComfy](#anycomfy-anycomfy)
7. [Sequence vs Frame-Based — How It Works](#sequence-vs-frame-based--how-it-works)
8. [Workflow JSON Customisation](#workflow-json-customisation)
9. [Async Job Manager](#async-job-manager)
10. [In-Memory Cache](#in-memory-cache)
11. [Adding a New Plugin](#adding-a-new-plugin)
12. [Building and Installing](#building-and-installing)
13. [Troubleshooting](#troubleshooting)

---

## Plugins at a Glance

| Plugin | Display Name | Processing Model | ComfyUI Extension |
|---|---|---|---|
| `depth_da3` | ComfyUI Depth Anything 3 | **Per-frame** | custom DA3 nodes |
| `depth_crafter` | ComfyUI DepthCrafter | **Sequence** | ComfyUI-DepthCrafterWrapper |
| `segmentation` | ComfyUI SAM Segmentation | **Per-frame** | GroundingDINO + SAM |
| `segmentation_sam3` | ComfyUI SAM3 Segmentation | **Sequence** | ComfyUI-SAM3 |
| `matte_mama` | ComfyUI MatteMaMa | **Sequence** | ComfyUI-MaMa + ComfyUI-SAM3 |
| `matte_ma2` | ComfyUI MatAnyone V2 | **Sequence** | ComfyUI-MatAnyone + ComfyUI-SAM3 |
| `upscale_seedvr2` | ComfyUI SeedVR2 Upscale | **Sequence** | ComfyUI-SeedVR2_VideoUpscaler |
| `anycomfy` | ComfyUI AnyComfy | **Per-frame** | any (user-supplied workflow) |

**Per-frame**: one ComfyUI job per rendered frame — fully independent.  
**Sequence**: one ComfyUI job for the entire clip — the full frame range is processed together.

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                     OFX Host (Flame / Nuke / Resolve)               │
│  calls render(time) once per frame, on the render thread            │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ OFX API
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     Concrete Plugin                                 │
│  (DepthCrafterPlugin, SAM3SegmentationPlugin, ...)                  │
│                                                                     │
│  • Declares isSequencePlugin() — true or false                      │
│  • Implements buildWorkflow(frame, inputPaths) → JSON               │
│  • Declares plugin-specific parameters                              │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ inherits
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     BasePlugin                                      │
│  (contrib/plugins/ComfyUI/common/comfyui_base_plugin.h/.cpp)        │
│                                                                     │
│  render()                                                           │
│    ├─ proxy check → renderPassthrough() if scale < 1.0             │
│    ├─ processingEnabled? → renderPassthrough() if false            │
│    └─ renderAsync()                                                 │
│         ├─ STEP 1: in-memory cache check                           │
│         ├─ STEP 2a [sequence]: active job guard                    │
│         ├─ STEP 2b [sequence]: writeInputSequence() + submitJob()  │
│         ├─ STEP 2  [per-frame]: isJobPending() check               │
│         ├─ STEP 3  [per-frame]: writeInputImages() + submitJob()   │
│         └─ returnPlaceholder() (checkerboard) if not cached        │
│                                                                     │
│  changedParam() [sequence plugins]                                  │
│    └─ "Collect & Process" button → collectAndProcess()             │
│         ├─ Cyan:   collect frames live (fetchImage per frame)      │
│         ├─ Orange: write EXRs to disk (background thread)          │
│         ├─ Amber:  submit workflow to ComfyUI                      │
│         ├─ Yellow: poll status (~0.5s interval)                    │
│         └─ Green / Red: done or failed                             │
│                                                                     │
│  onJobComplete(frame, success)                                      │
│    ├─ seed in-memory cache (all frames for sequence plugins)       │
│    └─ fire refreshTrigger at all frames → host re-renders          │
└──────────────────────────┬──────────────────────────────────────────┘
                           │ uses
            ┌──────────────┴──────────────┐
            ▼                             ▼
┌───────────────────────┐   ┌─────────────────────────────────────┐
│  AsyncJobManager      │   │  ComfyUI::Client                    │
│  (async_job_manager)  │   │  (comfyui_client.h/.cpp)            │
│                       │   │                                     │
│  • Job queue          │   │  • POST /prompt   — queue workflow  │
│  • Status tracking    │   │  • GET  /history  — poll results   │
│  • Background thread  │   │  • POST /interrupt — cancel job    │
│  • Completion CB      │   │  • WebSocket — real-time events    │
└───────────────────────┘   └─────────────────────────────────────┘
                           │ shared network storage
                           ▼
┌─────────────────────────────────────────────────────────────────────┐
│                     ComfyUI Server (Windows/Linux)                  │
│  • Receives workflow JSON                                           │
│  • Loads frames from shared storage (LoadEXR node)                 │
│  • Runs AI model (DepthCrafter, SAM3, SeedVR2, ...)               │
│  • Writes results to shared storage (SaveEXR node)                 │
└─────────────────────────────────────────────────────────────────────┘
```

### Source Files

| File | Purpose |
|---|---|
| `common/comfyui_base_plugin.h/.cpp` | Abstract base class — all render logic, caching, async dispatch |
| `common/async_job_manager.h/.cpp` | Background job queue — submits to ComfyUI, tracks status, fires callbacks |
| `common/comfyui_client.h/.cpp` | ComfyUI REST + WebSocket client |
| `common/comfyui_image_io.h/.cpp` | EXR read/write + OFX buffer conversion (TinyEXR) |
| `<plugin>/` | Concrete plugin — `buildWorkflow()`, parameters, factory |

---

## Render Pipeline

### Proxy / Thumbnail Renders

Any render call with `renderScale < 1.0` (timeline scrubbing, thumbnails, draft preview) is
intercepted immediately and returns the source image unchanged. No EXR is written, no job
is submitted, no network traffic occurs.

```
render(time, scale=0.5)  →  renderPassthrough()  →  done (instant)
```

If processing is disabled via the **Enable Processing** toggle, the same passthrough happens
at full scale.

---

### Frame-Based Render Flow

Applies to: `depth_da3`, `segmentation`, `anycomfy`.

```
render(frame=N, scale=1.0)
  │
  ├─ 1. Cache check
  │      constructExpectedOutputPath(N)
  │      → {mount}/out/{project}/{workflow}/{version}/{basename}.{N:04d}.exr
  │      In-memory set hit? → loadCachedResult() → done (instant)
  │      Filesystem hit?    → loadCachedResult(), add to set → done
  │
  ├─ 2. Job pending?
  │      isJobPending(N) → returnPlaceholder() (checkerboard) → done
  │
  └─ 3. Submit new job
         writeInputImages(N)
           → fetchImage(N) on render thread
           → writeEXR({mount}/in/{project}/{workflow}/{version}/{basename}.{N:04d}.exr)
         buildWorkflow(N, {"InputA": file_path})
           → customizeWorkflow(): ${INPUT_PATH}→file, ${OUTPUT_PREFIX}→prefix, ${FRAME}→N
         submitJobAsync(N, imageData, inputPaths, expectedOutputPath)
           → background thread: queuePrompt() → poll/WebSocket → onJobComplete(N)
         returnPlaceholder() → done

  onJobComplete(N, success=true)
    → seed {basename}.{N:04d}.exr in in-memory cache
    → refreshTrigger.setValueAtTime(N, +0.001)  ← host re-renders frame N
    → host calls render(N) again → cache hit → loadCachedResult()
```

---

### Sequence-Based Render Flow

Applies to: `depth_crafter`, `segmentation_sam3`, `matte_mama`, `matte_ma2`, `upscale_seedvr2`.

> **Note — Recommended workflow for sequence plugins:**
> The primary way to trigger sequence processing is the **Collect & Process** push button in
> the plugin UI (described in [Sequence vs Frame-Based](#sequence-vs-frame-based--how-it-works)
> and [SEQUENCE_ARCHITECTURE.md](../../../contrib/docs/SEQUENCE_ARCHITECTURE.md)).
> The button runs on the OFX `changedParam` action thread, provides live status feedback
> during collection (Cyan → Orange → Amber → Yellow → Green/Red), and lets the user control
> when the expensive frame-collection pass runs.
>
> The `render()`-triggered path documented below **still exists as a fallback**: if the host
> renders a frame and no cached output exists and no job is active, the plugin will submit
> the sequence job from the render thread (writing all frames synchronously, with no live
> feedback during collection). This path is less convenient — the host UI may stall while
> frames are written to the network share.

One ComfyUI job processes the full frame range. Multiple render calls from the host
(one per frame) all share that single job.

```
render(frame=N, scale=1.0)
  │
  ├─ 1. Cache check (identical to per-frame)
  │      {mount}/out/{project}/{workflow}/{version}/{basename}.{N:04d}.exr
  │      Hit? → loadCachedResult() → done (instant, once job has completed)
  │
  ├─ 2. Active job guard
  │      _pendingSequenceOutputPrefix == outputPrefix?
  │        YES → getJobStatus(_sequenceStartFrame)
  │               QUEUED or PROCESSING → returnPlaceholder() → done
  │               ended without output → clear pending, fall through
  │
  └─ 3. Submit sequence job (first miss, or after failure)
         clipRange = _srcClip->getFrameRange()
         startFrame = clipRange.min
         endFrame   = min(clipRange.max, startFrame + imageLoadCap - 1)  [if cap > 0]

         writeInputSequence(startFrame, endFrame)  ← SYNCHRONOUS, render thread
           for t in [start..end]:
             fetchImage(t) → ImageData → writeEXR({mount}/in/…/{basename}/{basename}.{t:04d}.exr)
             img.reset()   ← release before next frame (peak memory = 1 frame)

         _sequenceStartFrame = startFrame
         _sequenceEndFrame   = endFrame

         submitJobAsync(startFrame, {}, {"InputA": folder_path}, firstOutputPath)
           → background thread: buildWorkflow(startFrame, folder_path)
               ${INPUT_PATH} → folder path (LoadEXR loads all files in folder)
               ${FRAME}      → startFrame (= SaveEXR.start_frame, matches actual frame numbers)
             → queuePrompt() → poll → onJobComplete(startFrame)

         _pendingSequenceOutputPrefix = outputPrefix
         returnPlaceholder() → done

  onJobComplete(startFrame, success=true)
    → for t in [_sequenceStartFrame.._sequenceEndFrame]:
        seed {basename}.{t:04d}.exr in in-memory cache
        refreshTrigger.setValueAtTime(t, +0.001)
    → host re-renders all frames → each hits cache → loadCachedResult()
```

**Key constraint:** `fetchImage(t)` in `writeInputSequence()` must run on the OFX render thread.
The background job thread only calls `buildWorkflow()` and `queuePrompt()` — it never touches
OFX objects.

**Output cache check for sequence plugins:** Before submitting a new job, the plugin checks
whether ALL expected output EXRs already exist on disk. If they do, submission is skipped
(cache hit). If only some exist (partial previous run), those partial outputs are deleted
before submission — the ComfyUI `HQ-Image-Save` SaveEXR node refuses to overwrite existing
files, so partial outputs must be cleared to avoid a submission error.

---

## Shared Storage Layout

Both the OFX host machine (Mac/Linux) and the ComfyUI server (Windows) must be able to
access the same storage location. They use different path prefixes to the same physical
location (`macMountPath` / `winMountPath` plugin parameters).

```
{mount}/
├── in/
│   └── {project}/
│       └── {workflow}/
│           └── {version}/
│               ├── {basename}.{N:04d}.exr       ← per-frame input
│               └── {basename}/                  ← sequence input folder
│                   ├── {basename}.{start:04d}.exr
│                   ├── {basename}.{start+1:04d}.exr
│                   └── ...
└── out/
    └── {project}/
        └── {workflow}/
            └── {version}/
                ├── {basename}.{N:04d}.exr        ← per-frame output
                └── {basename}.{N:04d}.exr ...    ← sequence output (one per frame)
```

`basename` is auto-generated from the OFX clip name and is unique per instance.
The output path pattern is identical for frame-based and sequence-based plugins — the
cache check is the same code in both cases.

---

## Common Parameters

Every plugin inherits these from `BasePlugin`:

| Parameter | Default | Description |
|---|---|---|
| **Server Address** | `localhost` | Hostname or IP of the ComfyUI server |
| **Server Port** | `8188` | ComfyUI HTTP port |
| **Client Mount Path** | — | Network path as seen from the OFX host (Mac/Linux) |
| **Server Mount Path** | — | Same storage as seen from the ComfyUI server (Windows UNC or Linux) |
| **Project Name** | — | Subdirectory used to organise all files for this project |
| **Workflow Name** | — | Sub-subdirectory, usually matches the plugin's function |
| **Output Version** | `v001` | Version token in the output path; bump to re-process |
| **Workflow File Path** | — | Optional path to a custom workflow JSON file (overrides hardcoded workflow) |
| **Enable Processing** | ON | Frame-based plugins only: when OFF, plugin passes source through unchanged. Sequence plugins replace this toggle with the **Collect & Process** push button (see below). |
| **Collect & Process** | *(button)* | Sequence plugins only: clicking this triggers the full collect → write → submit → poll flow on the `changedParam` action thread with live status feedback. |
| **Status** | *(display)* | Read-only text showing the current processing phase. Color-coded via the Status Color parameter: Gray = Ready, Cyan = Collecting, Orange = Writing, Amber = Submitting, Yellow = Processing, Green = Done, Red = Failed. Text content updates live (e.g., `"Collecting: 12 / 25"`, `"ComfyUI processing 25 frame(s) — 45s (poll 30)"`, `"25 frame(s) done"`). |
| **Status Color** | *(display)* | Read-only RGB color swatch that reflects the current processing phase (see Status description above). Changes automatically as the job progresses. |

Hidden/internal parameters (not shown in host UI):

| Parameter | Purpose |
|---|---|
| `refreshTrigger` | Hidden animated double; BasePlugin increments it per-frame on job completion to trigger host re-render |
| `asyncMode` | Always 1 (non-blocking); retained for compatibility |
| `placeholderMode` | Always 1 (checkerboard) |

---

## Plugin Reference

### Depth Anything V3 (`depth_da3`)

**Processing model:** Per-frame  
**What it does:** Generates a depth map for each frame independently using the Depth Anything V3 monocular depth model.

| Parameter | Description |
|---|---|
| **Model Variant** | `vitl` / `vitb` / `vits` — larger = more accurate, slower |
| **Precision** | `fp32` / `fp16` / `bf16` |
| **Attention** | `sdpa` / `flash` / `xformers` |
| **Normalization** | How depth values are mapped to 0–1 |
| **Invert Depth** | Flip near/far |
| **Resize Method** | Interpolation for input resizing |
| **Keep Model Size** | Output at model resolution vs input resolution |

---

### DepthCrafter (`depth_crafter`)

**Processing model:** Sequence  
**What it does:** Generates temporally-consistent depth maps across the full clip using a diffusion-based temporal window approach. Unlike DA3, depth values are coherent between frames.

| Parameter | Description |
|---|---|
| **Frame Limit** | Max frames loaded from the sequence (`imageLoadCap`). Default: **25**. 0 = all frames. Reduce to lower VRAM usage; at 1920×1080, even 10 frames can require ~24 GB. |
| **Window Size** | Temporal window for the diffusion model. Larger = better consistency |
| **Overlap** | Frame overlap between windows |
| **Force Size** | Internal processing resolution |
| **Num Inference Steps** | Diffusion steps — more = better quality, slower |
| **Guidance Scale** | Diffusion guidance strength |
| **CPU Offload** | Enable for GPUs with < 16 GB VRAM |
| **Sequential CPU Offload** | More aggressive offloading for low-VRAM GPUs |

---

### SAM Segmentation (`segmentation`)

**Processing model:** Per-frame  
**What it does:** Text-prompted segmentation using Grounding DINO (object detection) + SAM (mask generation). Produces a mask for each frame independently.

| Parameter | Description |
|---|---|
| **Segment Prompt** | Text description of the object to segment |
| **Threshold** | Detection confidence threshold |
| **SAM Model** | `sam_vit_h` / `sam_vit_l` / `sam_vit_b` |
| **DINO Model** | `swinb` / `swint` |
| **Resolution Mode** | Processing resolution |
| **Color Space** | Input/output colour space conversion |

---

### SAM3 Segmentation (`segmentation_sam3`)

**Processing model:** Sequence  
**What it does:** Segments an object in one reference frame and propagates the mask through the entire sequence using SAM3 (`SAM3VideoSegmentation` + `SAM3Propagate`). Far more temporally stable than per-frame SAM.

| Parameter | Description |
|---|---|
| **Text Prompt** | Description of object to segment |
| **Score Threshold** | SAM3 confidence threshold |
| **Frame Index** | Reference frame index within the loaded sequence (0-based) |
| **Direction** | `forward` / `backward` / `both` — propagation direction |
| **Object ID** | Which detected object to track (when not plotting all masks) |
| **Plot All Masks** | Return all detected masks instead of a single tracked mask |
| **Model Path** | Path to `sam3.pt` on the ComfyUI server |
| **Offload Model** | Offload SAM3 to CPU between frames |

---

### MatteMaMa (`matte_mama`)

**Processing model:** Sequence  
**What it does:** High-quality video alpha matte generation. SAM3 propagates a coarse mask through the sequence, then VideoMaMa (diffusion-based) refines it into a clean alpha matte.

| Parameter | Description |
|---|---|
| **Frame Limit** | Max frames loaded (`imageLoadCap`) |
| **Text Prompt** | Object to matte out |
| **Score Threshold** | SAM3 detection threshold |
| **Frame Index** | SAM3 reference frame |
| **Direction** | SAM3 propagation direction |
| **UNet Checkpoint** | VideoMaMa UNet checkpoint path |
| **Base Model Path** | SVD base model path |
| **VAE / Misc** | Standard diffusion pipeline parameters |
| **SAM3 Model / Offload** | SAM3 model path and offload settings |

---

### MatAnyone V2 (`matte_ma2`)

**Processing model:** Sequence  
**What it does:** Video alpha matte generation using MatAnyone2 (recurrent memory network). Faster and lower VRAM than MatteMaMa. Two LoadEXR nodes in the workflow: one for the SAM3 reference, one for the full sequence.

| Parameter | Description |
|---|---|
| **Frame Limit** | Max frames for MatAnyone2 (`imageLoadCap`) |
| **SAM3 Frame Limit** | Frames loaded for SAM3 reference (`sam3ImageLoadCap`) |
| **Text Prompt / Threshold** | SAM3 detection parameters |
| **Frame Index / Direction** | SAM3 propagation parameters |
| **Use Long-Term Memory** | Enable long-term memory in MatAnyone2 |
| **Max Memory Frames** | Number of frames kept in long-term memory |
| **Mask Frame** | Which frame provides the reference mask to MatAnyone2 |
| **Dilate / Erode** | Morphological post-processing on the output mask |
| **SAM3 / Offload settings** | Model paths and CPU offload options |

---

### SeedVR2 Upscale (`upscale_seedvr2`)

**Processing model:** Sequence  
**What it does:** Diffusion-based video super-resolution using SeedVR2 (DiT architecture). Processes the full clip in temporal batches with overlap for consistency. Supports arbitrary upscale factors.

| Parameter | Description |
|---|---|
| **Frame Limit** | Max frames loaded (`imageLoadCap`) |
| **Resolution** | Target short-edge resolution |
| **Batch Size** | Frames per diffusion batch (reduce if OOM) |
| **Temporal Overlap** | Overlap between batches for consistency |
| **Prepend Frames** | Extra frames prepended for warm-up |
| **Warmup Frames** | Frames consumed for temporal context |
| **Color Correction** | `lab` / `none` — match output colours to input |
| **Seed** | Diffusion seed |
| **Input / Latent Noise** | Noise injection levels |
| **DiT Model / VAE Model** | Model checkpoint paths |
| **Blocks to Swap / Offload** | VRAM management settings |
| **Tiling settings** | VAE encode/decode tile size and overlap |

---

### AnyComfy (`anycomfy`)

**Processing model:** Per-frame (workflow-agnostic)  
**What it does:** Executes any user-supplied ComfyUI workflow JSON. Up to 3 input clips can be connected. The user loads workflow JSON files from disk; the plugin substitutes `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, and `${FRAME}` placeholders.

| Parameter | Description |
|---|---|
| **Workflow File** | Path to ComfyUI workflow JSON on the OFX host |
| **Input Count** | 0 / 1 / 2 / 3 connected input clips |
| **ComfyUI Input Dir** | Optional directory pre-populated for the workflow |

Workflow placeholders:

| Placeholder | Substituted value |
|---|---|
| `${INPUT_PATH}` | Primary input EXR file path (same as `${INPUT_PATH_A}`) |
| `${INPUT_PATH_A}` | InputA clip EXR path |
| `${INPUT_PATH_B}` | InputB clip EXR path |
| `${INPUT_PATH_C}` | InputC clip EXR path |
| `${OUTPUT_PREFIX}` | Output path prefix (without frame number or extension) |
| `"${FRAME}"` | Current frame number as integer (quotes replaced) |
| `${IMAGE_LOAD_CAP}` | Plugin `imageLoadCap` parameter value |

---

## Sequence vs Frame-Based — How It Works

The entire distinction flows from a single virtual method in the plugin's header:

```cpp
// In the plugin's .h file:
bool isSequencePlugin() const override { return true; }   // sequence
// or omitted — base class default returns false            // per-frame
```

This controls three runtime behaviours in `BasePlugin`:

1. **Input writing**: `isSequencePlugin() == true` → `writeInputSequence()` (folder of EXRs);  
   `false` → `writeInputImages()` (single EXR per frame).

2. **`${INPUT_PATH}` value**: folder path vs single file path.

3. **`${FRAME}` / `SaveEXR.start_frame`**: `_sequenceStartFrame` (first frame of clip) vs current render frame.

4. **UI controls**: `describeCommonParameters(isSequencePlugin=true)` adds a **Collect & Process**
   push button and omits the **Enable Processing** boolean toggle. The button is the primary
   way users trigger sequence processing — it runs on the `changedParam` action thread and
   provides live status feedback (Cyan → Orange → Amber → Yellow → Green/Red) throughout
   the collect, write, submit, and poll phases. The render()-triggered fallback path still
   exists for hosts that do not support button interactions.

`setTemporalClipAccess(true)` must be set in the factory `describe()` for sequence plugins —
this tells OFX hosts that the plugin will call `fetchImage(t)` at arbitrary times (not only the
current render time), which is required by `writeInputSequence()`.

For the decision procedure when implementing a new plugin, see
[SEQUENCE_ARCHITECTURE.md](../../../contrib/docs/SEQUENCE_ARCHITECTURE.md).

---

## Workflow JSON Customisation

Every plugin ships a workflow JSON file in `resources/workflow/`. The base class loads it
and performs string substitution before submitting to ComfyUI.

`BasePlugin::customizeWorkflow()` replaces:

| Template string | Replaced with | Notes |
|---|---|---|
| `"${FRAME}"` (with quotes) | `1001` (integer, no quotes) | Strips quotes so JSON number type is preserved |
| `${INPUT_PATH}` | ComfyUI-format path to input file or folder | Windows backslashes, JSON-escaped |
| `${INPUT_PATH_A/B/C}` | Per-clip input paths | Multi-input workflows |
| `${OUTPUT_PREFIX}` | ComfyUI-format output prefix | No frame number, no extension |
| `${IMAGE_LOAD_CAP}` | Value of plugin's `imageLoadCap` param | Used by sequence plugins in `LoadEXR.image_load_cap` |

Path conversion: Mac/Linux paths (`/Volumes/silo2/...`) are converted to Windows UNC paths
(`\\\\192.168.1.110\\silo2\\...`) by `convertPathForComfyUI()` using the `macMountPath` →
`winMountPath` prefix substitution. The result is then escaped for raw JSON string insertion
(backslashes doubled).

Plugins can override `customizeWorkflowWithParams()` for node-level parameter injection after
the base substitution pass.

---

## Async Job Manager

`AsyncJobManager` (`common/async_job_manager.h/.cpp`) decouples job submission from the render thread.

**Job lifecycle:**

```
submitJobAsync(frame, imageData, inputPaths, expectedOutputPath, plugin)
    └─ background thread:
         if imageData non-empty: writeEXR() all input images
         plugin->buildWorkflow(frame, inputPaths)
         client->queuePrompt(workflow)          → ComfyUI prompt_id
         poll getHistory(prompt_id) every 0.5s (active) / 5s (idle)
         on COMPLETED: verifyOutputExists() → onJobComplete(frame, true)
         on FAILED:    onJobComplete(frame, false)
```

**Job states:** `QUEUED` → `PROCESSING` → `COMPLETED` / `FAILED` / `CANCELLED`

**For sequence plugins**, `submitJobAsync` is called with an empty `imageData` map because
all frames have already been written synchronously. The background thread goes directly to
`buildWorkflow()` + `queuePrompt()`.

**`getJobStatus(frame)` returns `COMPLETED`** when the frame is not found in the queue
(not running, never submitted, or cleaned up). This is why `_pendingSequenceOutputPrefix`
is the primary guard for "is a job active?" — the job manager alone cannot distinguish
"never submitted" from "completed".

---

## In-Memory Cache

`_cacheFileExists` is an `unordered_set<string>` of output file paths known to exist,
protected by `_cacheMutex`. It avoids repeated filesystem stats over the network share.

**Population:**
- On `renderAsync()`: if a filesystem stat confirms a file exists, it is added to the set.
- On `onJobComplete()`: all output paths for the completed job are added immediately.
  For sequence plugins this means all frames `[_sequenceStartFrame .. _sequenceEndFrame]`
  are seeded at once.

**Invalidation:**
- `changedParam()` calls `_cacheFileExists.clear()` when any path-forming parameter
  changes (`projectName`, `workflowName`, `outputVersion`, `macMountPath`, `winMountPath`).

**Dimension cache:** `_cacheDimensions` maps frame numbers to `(width, height)` pairs for
`getRegionOfDefinition()`, invalidated on the same parameter changes.

---

## Adding a New Plugin

### Per-frame plugin

1. Create `contrib/plugins/ComfyUI/<name>/` with `<name>_plugin.h`, `<name>_plugin.cpp`, `CMakeLists.txt`
2. Inherit `BasePlugin`, implement `buildWorkflow()` and `getRequiredModels()`
3. Add `resources/workflow/<name>.json` with `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, `"${FRAME}"` placeholders
4. Register factory in `<name>_plugin.cpp` using `OFX::Plugin::getPluginID()` pattern
5. Add to `contrib/plugins/ComfyUI/CMakeLists.txt`
6. Build: `./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI/<name> <Target> --install`

### Sequence plugin (additional steps)

7. In the class header, add:
   ```cpp
   bool isSequencePlugin() const override { return true; }
   int  getImageLoadCap()  const override;  // if plugin exposes a frame limit param
   ```
8. In the `.cpp`, implement `getImageLoadCap()`:
   ```cpp
   int MyPlugin::getImageLoadCap() const {
       int cap = 0;
       if (_imageLoadCap) _imageLoadCap->getValue(cap);
       return cap;
   }
   ```
9. In the factory `describe()`, change:
   ```cpp
   desc.setTemporalClipAccess(true);  // required for fetchImage(t) at arbitrary times
   ```
10. Ensure `LoadEXR.filepath` uses `${INPUT_PATH}` and `LoadEXR.image_load_cap` uses
    `${IMAGE_LOAD_CAP}` (or a hardcoded `0` for unlimited)
11. Ensure `SaveEXR.start_frame` uses `"${FRAME}"` (quoted in JSON — base class strips quotes)

**Decision rule:** if the ComfyUI model has temporal parameters (`window_size`, `direction`,
`temporal_overlap`, etc.) or propagates information across frames, it is a sequence plugin.
See [SEQUENCE_ARCHITECTURE.md](../../../contrib/docs/SEQUENCE_ARCHITECTURE.md) for the full
decision procedure.

---

## Building and Installing

Each plugin has its own `.ofx.bundle` and can be built and installed independently.

```bash
# Build a single plugin and install to ~/Library/OFX/Plugins/
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI <TargetName> --install

# All 7 plugins
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI DepthCrafter --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI DepthAnything3 --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI UpscaleSeedVR2 --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI SegmentationSAM3 --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI MatteMA2 --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI MatteMaMa --install
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI AnyComfy --install

# Debug build
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI DepthCrafter --install -d

# Clean rebuild
./contrib/dev-tools/build-plugin.sh contrib/plugins/ComfyUI DepthCrafter --install -c

# Build all plugins at once (full CMake build)
./scripts/build-cmake.sh Release
```

Build output goes to `build/Release/Release/<Target>.ofx.bundle`.  
Install location: `~/Library/OFX/Plugins/` (macOS user dev);
`/Library/OFX/Plugins/` for system-wide (Flame, etc.).

---

## Troubleshooting

### Checkerboard never clears

1. Check the **Status** parameter — it shows the current processing phase (e.g., `"ComfyUI processing 25 frame(s) — 45s (poll 30)"`, `"Failed — [error message]"`, `"Ready"`). The **Status Color** swatch gives a quick visual indication: Yellow = processing, Red = failed, Gray = idle.
2. Check logs in `~/Library/Logs/ComfyUI_OFX/` (or wherever spdlog writes)
3. Verify `macMountPath` is reachable: `ls {macMountPath}/out/`
4. Verify ComfyUI server is running and reachable at the configured address/port
5. For sequence plugins: use the **Collect & Process** button rather than waiting for a render — the button shows live collection progress and avoids host UI stalls.

### Stale results after parameter change

Bump **Output Version** (`v001` → `v002`). The output path includes the version token, so
changing it forces new output files to be written. Changing inference parameters (like
`windowSize` or `guidanceScale`) cancels the running job but does not delete existing output
files — those will be served as cache hits until the version changes.

### Wrong frame numbers in output (off-by-one or starting at 0)

For sequence plugins, `SaveEXR.start_frame` is set from `_sequenceStartFrame`, which is
derived from `_srcClip->getFrameRange().min`. If the clip's in-point differs from what
ComfyUI uses, output files won't be found. Check the Status log for the `${FRAME}` value
that was submitted.

### "Sequence job ended without output — clearing pending, will resubmit"

ComfyUI completed the job but the expected output file is missing. Common causes:
- `OUTPUT_PREFIX` path not writable by ComfyUI server
- `SaveEXR.start_frame` mismatch (see above)
- ComfyUI ran out of VRAM and wrote a partial sequence

### "Failed — File exists already" in Status

Partial outputs from a previous run are blocking a new submission. Click **Collect & Process**
again — the plugin automatically detects and deletes partial outputs before re-submitting.
(The ComfyUI HQ-Image-Save SaveEXR node refuses to overwrite existing files, so partial
outputs must be cleared.)

### Path errors on Windows ComfyUI server

Verify `winMountPath` uses UNC format: `\\\\192.168.1.110\\sharename`. In the plugin UI,
enter it as `\\192.168.1.110\sharename` — the plugin doubles the backslashes when generating
the ComfyUI workflow JSON.

### IDE shows "file not found" errors for OFX headers

False positives. OFX headers are resolved through CMake's include paths, not the IDE's
default path. Real compilation via `build-plugin.sh` will succeed regardless.
