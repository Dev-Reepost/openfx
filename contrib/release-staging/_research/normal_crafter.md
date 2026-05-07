# NormalCrafter — Research Brief

## Summary
A surface normal map encodes, per pixel, the 3D direction the surface is facing — typically packed as RGB where R/G/B map to the X/Y/Z components of the unit normal vector. NormalCrafter takes an RGB frame (the OpenFX plugin runs it per-frame, even though the upstream model is video-aware) and outputs an RGB-encoded normal map of the same resolution. Typical VFX uses: relighting and synthetic shading passes, normal-based AOVs for matte/roto refinement, retopology and projection-mapping guides, bump/displacement extraction, and re-shading of plate elements without rebuilding geometry.

## Upstream sources
- **Paper:** Yanrui Bin, Wenbo Hu, Haoyuan Wang, Xinya Chen, Bing Wang — *NormalCrafter: Learning Temporally Consistent Normals from Video Diffusion Priors* — ICCV 2025. arXiv: https://arxiv.org/abs/2504.11427
- **Project page:** https://normalcrafter.github.io/ (side-by-side video comparisons against per-frame normal estimators)
- **Official GitHub:** https://github.com/Binyr/NormalCrafter
- **ComfyUI node implementation:** https://github.com/AIWarper/ComfyUI-NormalCrafterWrapper (registers `NormalCrafterNode`)
- **Model weights:** https://huggingface.co/Yanrui95/NormalCrafter — safetensors, served via the `diffusers` `StableVideoDiffusionPipeline` API. Total weight footprint <UNVERIFIED> (model card listing not exposed); also pulls the SVD base `stabilityai/stable-video-diffusion-img2vid-xt` (~9 GB) on first run.

## Technical approach
NormalCrafter fine-tunes a Stable Video Diffusion (SVD) backbone into a video-to-normal model, exploiting the SVD temporal prior so a sliding window of frames is denoised jointly into a temporally coherent normal sequence. Two key ingredients: **Semantic Feature Regularization (SFR)** aligns intermediate diffusion features with semantic cues so the model latches onto intrinsic surface semantics rather than RGB texture, and a **two-stage training protocol** combines latent-space supervision (long context) with pixel-space supervision (sharp spatial accuracy). The OpenFX `normal_crafter` plugin invokes the ComfyUI wrapper one frame at a time, so the plugin trades some of the upstream temporal consistency for simpler host integration; flicker between frames is therefore expected and not a model defect. Reported zero-shot accuracy beats per-frame baselines (e.g. Marigold-Normals, GeoWizard, StableNormal) on standard normal benchmarks. <UNVERIFIED>: exact AbsAngular / 11.25° accuracy figures.

## Requirements & limitations
- **VRAM:** ~20 GB at 1024×576, ~6 GB at 512×256 (official repo guidance). Per-frame invocation as used by the plugin sits closer to the lower bound.
- **Input resolution:** wrapper default `max_res_dimension=1024`, aspect ratio preserved during resize. SVD-derived constraints typically require dimensions to be multiples of 64.
- **Per-frame vs sequence behavior:** upstream is sequence-aware via a sliding window (`window_size=14`, `time_step_size=10` defaults); the plugin treats it as per-frame, so temporal flicker is the dominant artifact. For shots where stability matters, baking and median-filtering or running upstream with full window context outside the plugin is recommended.
- **Wrapper-disabled knobs:** `fps_for_time_ids`, `motion_bucket_id`, and `noise_aug_strength` were observed by the wrapper author to have minimal effect and are hardcoded.
- **Known failure modes:** transparent / refractive surfaces (glass, water), strong specular highlights and mirrors, very low-light footage with crushed blacks, fine subpixel structures (hair, foliage edges), and heavy motion blur all degrade output quality — these are inherited from the SVD prior plus the ambiguity of normals on non-opaque surfaces.
- **Output convention:** RGB-packed unit normals. <UNVERIFIED>: confirm whether the model emits camera-space or world-space normals and the exact axis convention (typically +X right, +Y up, +Z toward camera, mapped to [0,1] via `n*0.5+0.5`).

## License
- **Code license (Binyr/NormalCrafter):** MIT.
- **Weights license (Yanrui95/NormalCrafter on HF):** Apache-2.0. Note that the SVD base weights pulled at runtime carry the **Stability AI Non-Commercial Community License**, which may contaminate downstream commercial use even though the NormalCrafter delta is permissive.
- **ComfyUI wrapper (AIWarper/ComfyUI-NormalCrafterWrapper):** repo-level license <UNVERIFIED> (no LICENSE file surfaced in the listing). Users still inherit the upstream MIT + Apache-2.0 + SVD-NC stack.
- **Attribution:** preserve the MIT copyright notice in redistributions; cite the ICCV 2025 paper; credit original authors (Yanrui Bin, Wenbo Hu, Haoyuan Wang, Xinya Chen, Bing Wang).

## Example imagery
1. **Project-page hero reel** — https://normalcrafter.github.io/
   - Shows: input video / NormalCrafter normals / per-frame baseline triplet, demonstrating reduced flicker on RGB-encoded normal output.
   - Attribution: Bin et al., ICCV 2025. Reuse license on hosted media is <UNVERIFIED>.
   - Redistributable in BSD-3-Clause docs: **not clearly permitted** — hot-link or replace with self-generated frames.
2. **GitHub README teaser** — https://github.com/Binyr/NormalCrafter (assets path <UNVERIFIED>; typically `assets/` or top-level GIF).
   - Shows: open-world clips with their normal sequences.
   - Attribution: same as above; bound by repo MIT license (compatible with BSD-3-Clause if notice preserved).
   - Redistributable: yes, with MIT notice — preferable to project-page media.
3. **Self-generated normal pass from our plugin** (recommended)
   - Shows: a CC0 / in-house plate run through the `normal_crafter` plugin, optionally next to a relit composite using the resulting normals.
   - Attribution: none required (our own output).
   - Redistributable in BSD-3-Clause docs: yes — cleanest licensing path.

## Citations
```bibtex
@inproceedings{bin2025normalcrafter,
  author    = {Bin, Yanrui and Hu, Wenbo and Wang, Haoyuan and Chen, Xinya and Wang, Bing},
  title     = {NormalCrafter: Learning Temporally Consistent Normals from Video Diffusion Priors},
  booktitle = {Proceedings of the IEEE/CVF International Conference on Computer Vision (ICCV)},
  year      = {2025},
  eprint    = {2504.11427},
  archivePrefix = {arXiv}
}
```
