# ComfyUI OFX Plugins — VFX Artist Guide

Guide for VFX artists using ComfyUI OFX plugins in professional post-production workflows.

**Last Updated:** 2026-04-12
**Version:** 2.0
**License:** BSD-3-Clause

---

## Table of Contents

1. [Introduction](#introduction)
2. [Plugin Overview](#plugin-overview)
3. [Per-Frame Plugins — Workflow](#per-frame-plugins--workflow)
4. [Sequence Plugins — Workflow](#sequence-plugins--workflow)
5. [Status Panel](#status-panel)
6. [Output Cache and Versioning](#output-cache-and-versioning)
7. [VRAM Management for Sequence Plugins](#vram-management-for-sequence-plugins)
8. [Troubleshooting](#troubleshooting)

---

## Introduction

ComfyUI OFX plugins bring AI video processing into your compositing host (Flame, Nuke,
DaVinci Resolve, etc.) without requiring you to leave your timeline. Each plugin connects
to a ComfyUI server over the network, submits frames as EXR files via shared storage, and
returns results back to your timeline — all transparently.

### Two Processing Modes

**Per-frame** — each frame is processed independently. Turn on a toggle, render a frame,
get a result. Fast feedback; no temporal consistency between frames.

**Sequence** — the entire clip (up to a configurable frame limit) is sent to ComfyUI as a
batch. You click a button once, watch the status update in real time, and when it finishes
every frame is available as a cache hit. This is required for models that need temporal
context — depth consistency, mask propagation, video upscaling.

---

## Plugin Overview

| Plugin | Type | What it does | Min VRAM |
| --- | --- | --- | --- |
| Depth Anything V3 | Per-frame | Monocular depth map per frame | 4 GB |
| SAM Segmentation | Per-frame | Text-prompted mask per frame | 8 GB |
| AnyComfy | Per-frame | Run any custom ComfyUI workflow | Depends on workflow |
| DepthCrafter | Sequence | Temporally consistent depth across clip | 16 GB (24 GB at 1080p) |
| SAM3 Segmentation | Sequence | Propagate mask through entire clip | 12 GB |
| MatteMaMa | Sequence | High-quality video alpha matte (diffusion) | 24 GB |
| MatAnyone V2 | Sequence | Fast video alpha matte (recurrent network) | 16 GB |
| SeedVR2 Upscale | Sequence | AI video super-resolution | 16 GB |

---

## Per-Frame Plugins — Workflow

Applies to: Depth Anything V3, SAM Segmentation, AnyComfy.

1. Apply the plugin to your clip in the host timeline.
2. Configure the **Server** group: address, port.
3. Configure the **Storage** group: Client Mount Path, Server Mount Path, Project Name, Workflow Name, Output Version.
4. Set any plugin-specific parameters (prompt, model variant, etc.).
5. Turn ON the **Enable Processing** toggle.
6. Render any frame. The plugin submits it to ComfyUI and returns a checkerboard placeholder.
7. Watch the **Status** parameter. It progresses from Yellow ("Processing: fr.N Xs pP") to Green ("1 frame(s) done").
8. The host automatically re-renders the frame when processing completes — the result loads from cache instantly.
9. Subsequent renders of the same frame hit the disk cache with no ComfyUI round-trip.

To re-process a frame with different settings, bump **Output Version** (e.g., `v001` → `v002`).

---

## Sequence Plugins — Workflow

Applies to: DepthCrafter, SAM3 Segmentation, MatteMaMa, MatAnyone V2, SeedVR2 Upscale.

1. Apply the plugin to your clip.
2. Configure the **Server** and **Storage** groups (same as per-frame).
3. Configure plugin-specific parameters (text prompt, model paths, etc.).
4. Set **Frame Limit** to the number of frames you want to process (e.g., 25 for DepthCrafter). Setting it to 0 processes the entire clip.
5. Click the **Collect & Process** button.
6. Watch the **Status** panel progress through the phases:
   - **Cyan** — `"Collecting: 12 / 25"` — fetching frames from the timeline one by one
   - **Orange** — `"Writing 25 frames to disk..."` — writing EXRs to the network share
   - **Amber** — `"Submitting workflow to ComfyUI..."` — queueing the job
   - **Yellow** — `"ComfyUI processing 25 frame(s) — 45s (poll 30)"` — updates every ~0.5 seconds
   - **Green** — `"25 frame(s) done"` — all output EXRs are on disk
7. Render any frame in the processed range — it loads from cache instantly (no ComfyUI round-trip).

If Status turns **Red**, the error message is shown directly in the Status text (e.g., `"Failed — CUDA out of memory"`). See [Troubleshooting](#troubleshooting) for common causes.

---

## Status Panel

Every plugin has two read-only parameters that show processing state:

**Status** — text field that updates live throughout the job lifecycle.

**Status Color** — an RGB color swatch that changes automatically to reflect the current phase. Use this for a quick visual check without reading the text.

| Color | Phase | Example Status Text |
| --- | --- | --- |
| Gray | Idle | `"Ready"` |
| Cyan | Collecting frames | `"Collecting: 12 / 25"` |
| Orange | Writing EXRs to disk | `"Writing 25 frames to disk..."` |
| Amber | Submitting to ComfyUI | `"Submitting workflow to ComfyUI..."` |
| Yellow | ComfyUI processing | `"ComfyUI processing 25 frame(s) — 45s (poll 30)"` |
| Yellow | Per-frame processing | `"Processing: fr.42 15s p10 / fr.43 3s p2"` |
| Green | Done | `"25 frame(s) done"` |
| Red | Failed | `"Failed — [error message]"` |

The Status Color updates on the OFX action thread (not just the render thread), so it
reflects the live state even while the host UI is idle between renders.

---

## Output Cache and Versioning

### Per-frame plugins

Each output frame is cached independently on the network share as an EXR file. Once a
frame has been processed and its EXR exists, every subsequent render of that frame is an
instant cache hit — no ComfyUI submission.

### Sequence plugins

All frames in the processed range are cached together. Once the Collect & Process job
completes, every frame in the range is a cache hit.

Clicking **Collect & Process** when all output EXRs already exist is a no-op — the plugin
detects the complete cache and skips submission immediately.

### Forcing a re-process

Change the **Output Version** parameter (e.g., `v001` → `v002`). This changes the output
path, so the existing cache is bypassed and a new set of output EXRs is written. The old
version's files are left on disk and can be recovered by reverting the version string.

Do not delete output files manually while a job is running — the plugin tracks the pending
job by the expected output prefix. If you need to force a clean re-run, bump the version
instead.

---

## VRAM Management for Sequence Plugins

Sequence plugins load multiple frames into GPU memory simultaneously. VRAM requirements
scale with frame count and resolution.

### DepthCrafter

At 1920x1080 with default settings, DepthCrafter needs approximately 24 GB of VRAM even
for 10 frames. The default Frame Limit is 25.

- Enable **CPU Offload** for GPUs with less than 16 GB VRAM.
- Enable **Sequential CPU Offload** for more aggressive offloading on very low VRAM GPUs (slower).
- Reduce **Frame Limit** to reduce peak VRAM — fewer frames = less memory, at the cost of
  shorter temporal consistency windows.

### SeedVR2 Upscale

SeedVR2 processes frames in temporal batches. If you get an out-of-memory error:

- Reduce **Batch Size** (e.g., from 8 to 4 or 2).
- Increase **Blocks to Swap** to offload transformer blocks to CPU.
- Reduce **Frame Limit** to process a shorter clip segment.

### MatteMaMa

MatteMaMa uses a diffusion sampler (VideoMaMa) on top of SAM3 propagation. It requires
approximately 24 GB VRAM at 1080p. There is no CPU offload option in the current workflow;
use a GPU with sufficient VRAM or reduce resolution.

### MatAnyone V2

MatAnyone V2 uses a recurrent memory network (lower VRAM than MatteMaMa). If OOM:

- Reduce **Frame Limit**.
- Reduce **Max Memory Frames** (long-term memory buffer size).

---

## Troubleshooting

### Status stays Gray — nothing happens

**Per-frame:** The **Enable Processing** toggle is OFF. Turn it on, then render a frame.

**Sequence:** The **Collect & Process** button has not been clicked. Click it to start collection.

### Status stays Yellow — job never finishes

ComfyUI is still running or has stalled. Check:

1. ComfyUI server logs for errors (OOM, missing model, node error).
2. The Status text — when the job fails, the color changes to Red and the error is shown inline.
3. Network connectivity between the OFX host and the ComfyUI server.
4. Whether the output EXR directory on the server is writable.

### "Failed — File exists already" in Status text

Partial outputs from a previous interrupted run are blocking the new submission. The
ComfyUI HQ-Image-Save SaveEXR node refuses to overwrite existing files.

Click **Collect & Process** again — the plugin automatically detects and deletes partial
outputs before re-submitting. You do not need to clean files manually.

### Checkerboard never clears after Status turns Green

The output EXR files exist but the host is not re-rendering. Check:

1. That **Client Mount Path** is correctly set and the output directory is readable from the OFX host.
2. That the path separators are correct for your OS (forward slashes on Mac/Linux).
3. Try scrubbing to a different frame and back — some hosts require a manual refresh.

### Wrong frame numbers in the output

If output files are numbered starting from 0 instead of your clip's actual in-point:

- Check that the clip's frame range is correctly set in the host (the plugin uses `getFrameRange().min` as the start frame).
- For sequence plugins, the `${FRAME}` substitution in the workflow equals the first frame of the clip range, not 0.

### Output Version mismatch after a version bump

If you bumped Output Version mid-session and old cached frames appear for some renders:

- The in-memory cache is keyed by file path including the version token.
- Changing Output Version clears the in-memory cache automatically on the next parameter change.
- If stale results persist, apply a small parameter change (e.g., toggle a setting and revert) to force a cache flush.

### "Path not found" or "Failed to write EXR"

1. Verify that **Client Mount Path** points to a directory that is mounted and writable from your workstation: `ls /Volumes/yourshare/`
2. Verify that **Server Mount Path** points to the same physical location as seen from the ComfyUI server (Windows UNC path or Linux NFS mount).
3. Check that the project and workflow subdirectories can be created (permissions).
4. On Windows ComfyUI servers, Server Mount Path must use UNC format: `\\192.168.1.110\sharename` (the plugin doubles the backslashes in the workflow JSON automatically).

### SAM3 / MatteMaMa mask is wrong or empty

- Adjust **Text Prompt** to be more specific (e.g., `"person in foreground"` instead of `"person"`).
- Lower **Score Threshold** if nothing is detected (try 0.2).
- Raise **Score Threshold** if too many objects are detected (try 0.5).
- Check **Frame Index** — this is the 0-based index within the loaded sequence, not the timeline frame number.
- Try **Direction: both** instead of `forward` if the subject enters or exits the frame.
