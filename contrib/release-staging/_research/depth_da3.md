# depth_da3 — Research Brief

## Summary
Depth Anything 3 (DA3) is a foundation model for monocular depth estimation: feed it a single RGB frame and it returns a per-pixel depth map (relative depth by default, metric depth with the `DA3METRIC-LARGE` variant). For VFX and grading work, the depth pass drives matte extraction, depth-based defocus / atmospheric haze, relighting, parallax-based 2.5D camera moves, and rotoscoping assistance — without needing tracked geometry or a stereo rig.

## Upstream sources
- **Paper:** *Depth Anything 3: Recovering the Visual Space from Any Views*, Haotong Lin, Sili Chen, Jun Hao Liew, Donny Y. Chen, Zhenyu Li, Guang Shi, Jiashi Feng, Bingyi Kang, 2025. https://arxiv.org/abs/2511.10647
- **Project page:** https://depth-anything-3.github.io/
- **Official GitHub:** https://github.com/ByteDance-Seed/Depth-Anything-3
- **ComfyUI node implementation:** https://github.com/PozzettiAndrea/ComfyUI-DepthAnythingV3 (the `DownloadAndLoadDepthAnythingV3Model` / `DepthAnything_V3` nodes our plugin drives). Note: `kijai` published the V2 equivalent; the V3 node we depend on is by `PozzettiAndrea`. <UNVERIFIED: which fork the user installs may vary; community forks at `n0debear`, `itsumoe`, `Ltamann/...-TBG` exist.>
- **Model weights / checkpoints** (Hugging Face, auto-downloaded to `ComfyUI/models/depthanything3/`):
  - `depth-anything/DA3-SMALL` — 0.08 B params
  - `depth-anything/DA3-BASE` — 0.12 B params
  - `depth-anything/DA3-LARGE` — 0.35 B params, `model.safetensors` ~1.64 GB
  - `depth-anything/DA3-GIANT` — 1.15 B params
  - `depth-anything/DA3METRIC-LARGE` — 0.35 B (metric depth)
  - `depth-anything/DA3MONO-LARGE` — 0.35 B (mono-only variant)

## Technical approach
DA3 uses a single plain Vision Transformer (vanilla DINO backbone) with no task-specific architectural specialization, trained against a unified "depth-ray" prediction target that subsumes monocular depth, multi-view depth, and camera-pose estimation. For VFX this matters because the same checkpoint that produces a stable monocular depth pass can also be fed multiple views (e.g. a clean plate + reference) for more consistent geometry. Reported figures: DA3 outperforms VGGT by 44.3% in camera-pose accuracy and 25.1% in geometric accuracy, and beats DA2 on monocular depth. Per-frame FPS and exact VRAM are not published in the paper. <UNVERIFIED: VRAM/FPS by variant.>

## Requirements & limitations
- **VRAM (typical):** Small ~2 GB, Base ~3 GB, Large ~6 GB, Giant ~12 GB+ at moderate resolution. <UNVERIFIED — not in upstream docs; estimated from parameter counts in F32/F16.>
- **Input resolution:** No fixed input size; the ComfyUI node exposes a `process-res-method` option to pad/resize. The ViT operates on patchified inputs, so dimensions are typically rounded to a multiple of 14. <UNVERIFIED for exact patch size.>
- **Known failure modes for live action:**
  - Transparent / translucent surfaces (glass, water, smoke) — depth collapses to background or surface ambiguously.
  - Specular highlights and mirrors — model often returns the reflected-scene depth.
  - Heavy motion blur — depth noise increases; DA3 has no explicit temporal consistency for video (consider Video Depth Anything for sequences).
  - Depth output is **relative / affine-invariant** unless you use `DA3METRIC-LARGE`; do not assume world-scale units.
  - Trained on public academic datasets — domain-specific shots (medical, microscopy, IR) may degrade.

## License
- **Code license (DA3 repo):** Apache 2.0.
- **Weights license (split):**
  - **Apache 2.0:** `DA3-BASE`, `DA3-SMALL`, `DA3METRIC-LARGE`, `DA3MONO-LARGE`.
  - **CC BY-NC 4.0 (non-commercial only):** `DA3-LARGE`, `DA3-GIANT`, `DA3NESTED-*`.
- **Attribution:** Cite the arXiv paper (BibTeX below). For CC BY-NC weights, attribution + non-commercial use is mandatory; commercial VFX delivery requires sticking to the Apache-licensed variants.
- **ComfyUI node (`PozzettiAndrea/ComfyUI-DepthAnythingV3`):** MIT.

## Example imagery
- **Source:** https://depth-anything-3.github.io/ (project-page teaser / gallery)
  - Shows: monocular RGB → depth visualizations and multi-view geometry recoveries.
  - Attribution: ByteDance Seed / DA3 authors. License of the page assets is **not stated explicitly**. <UNVERIFIED — redistribution permission unclear; safest path is to link/embed rather than mirror.>
- **Source:** https://github.com/ByteDance-Seed/Depth-Anything-3 README assets
  - Shows: side-by-side RGB / depth / point-cloud comparisons.
  - Attribution: same authors; repo code is Apache 2.0 but **image assets are not explicitly covered** by that license. <UNVERIFIED.>
- **Source:** https://huggingface.co/spaces/depth-anything/depth-anything-3 (HF Space demo)
  - Shows: live before/after on user-uploaded images — useful for generating our own example pairs we can self-license.
  - Recommendation: generate fresh examples in-house from BSD-3-Clause-compatible source plates and self-host, rather than redistributing upstream figures.

**Redistribution verdict for a BSD-3-Clause docs site:** Do **not** mirror upstream teaser images without written permission. Either (a) hot-link with attribution, or (b) generate our own depth visualizations from clips we own and ship those.

## Citations
```bibtex
@article{depthanything3,
  title   = {Depth Anything 3: Recovering the Visual Space from Any Views},
  author  = {Haotong Lin and Sili Chen and Jun Hao Liew and Donny Y. Chen and
             Zhenyu Li and Guang Shi and Jiashi Feng and Bingyi Kang},
  journal = {arXiv preprint arXiv:2511.10647},
  year    = {2025}
}
```
