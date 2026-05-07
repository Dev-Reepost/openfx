# matte_mama — Upstream Research Brief

## Summary
`matte_mama` takes an image sequence plus a binary mask seed (typically produced by SAM3 inside the same workflow) and runs **VideoMaMa**, a diffusion-based video matting model, to refine that coarse mask into a high-quality, temporally-consistent alpha matte with hair, edge, and semi-transparent detail. The output is a single-channel alpha clip you can use as a key — useful for greenscreen-free keying of talent or objects, refining roto for hair and fur, and producing soft mattes from rough roto without per-frame manual cleanup.

## Upstream sources
- **Paper:** *VideoMaMa: Mask-Guided Video Matting via Generative Prior* — Sangbeom Lim, Seoung Wug Oh, Jiahui Huang, Heeji Yoon, Seungryong Kim, Joon-Young Lee (KAIST / Korea University / Adobe Research), CVPR 2026. arXiv: https://arxiv.org/abs/2601.14255
- **Project page:** https://cvlab-kaist.github.io/VideoMaMa/
- **Official GitHub:** https://github.com/cvlab-kaist/VideoMaMa
- **ComfyUI node implementation:** https://github.com/okdalto/ComfyUI-VideoMaMa (third-party wrapper by `okdalto`; nodes: `VideoMaMaPipelineLoader`, `VideoMaMaSampler` / `VideoMaMa Run`, plus an optional SAM2 mask helper)
- **Model weights:** https://huggingface.co/SammyLim/VideoMaMa (fine-tuned UNet + DINO projection MLP). Requires the base checkpoint `stabilityai/stable-video-diffusion-img2vid-xt` (~9.5 GB). VideoMaMa weight file sizes <UNVERIFIED>; auto-downloaded by the ComfyUI node on first use.
- **Hugging Face demo:** https://huggingface.co/spaces/SammyLim/VideoMaMa
- **Dataset (MA-V):** Matting Anything in Video, ~50K real-world clips with pseudo-labels.

## Technical approach
VideoMaMa is built on top of Stable Video Diffusion (SVD): the pretrained video diffusion UNet acts as a strong generative prior over plausible video appearance, which is repurposed to predict an alpha-matte latent conditioned on the input frames and a binary guide mask. A two-stage training schedule first fine-tunes spatial layers on single images at 1024×1024 to learn fine details (hair strands, semi-transparent edges), then trains temporal layers on 3-frame clips at 704×704 for cross-frame consistency. Semantic features from DINOv3 are injected via a projection MLP to stabilize zero-shot generalization. Crucially, inference is a **single forward pass** in latent space (not iterative diffusion sampling), so it is far faster than typical SVD generation. Pairing with SAM3 makes sense because SAM gives reliable but binary, blocky masks — VideoMaMa specializes exactly in turning those into pixel-accurate, temporally-stable alpha mattes.

## Requirements & limitations
- **VRAM:** No exact figure published. Built on SVD-XT, so realistically a 24 GB consumer GPU (e.g. RTX 3090/4090) is the practical floor for the default 1024-px max resolution; lower `max_resolution` reduces VRAM linearly. <UNVERIFIED>
- **Sequence length:** SVD-XT backbone processes clips in chunks of ~14–25 frames per pass (the ComfyUI node handles windowing internally). Very long clips are processed in overlapping windows; expect minor seam artifacts at window joins. <UNVERIFIED specific frame count>
- **Input resolution:** ComfyUI node exposes `max_resolution` 256–2048 (default 1024) on the longest axis, aspect ratio preserved, snapped to multiples of 8.
- **Inputs:** RGB image sequence + binary guide mask sequence (one mask per frame, or a single seed mask). The guide mask quality directly bounds the result — bad seed = bad matte.
- **Failure modes:** extremely fine isolated hair against busy backgrounds, heavy motion blur, smoke/fog/transparent fluids, very thin filaments, and subjects whose silhouette diverges drastically from the seed mask. Trained only on synthetic data, so unusual cinematic looks may need higher `noise_aug_strength`.
- **Performance:** single forward pass per window — fast for a video diffusion model, but still seconds-to-minutes per shot, not real-time.
- **Dependencies:** Python 3.10+, PyTorch 2.0+ with CUDA, ComfyUI, and SVD-XT base weights.

## License
- **Code license (VideoMaMa repo):** **CC BY-NC 4.0** — non-commercial only, attribution required. This is a hard blocker for paid VFX work using the official code as-is.
- **Weights license:** **Stability AI Community License** (inherited from SVD-XT — applies to `unet/*` and `dino_projection_mlp.pth`). Permits free use up to a revenue/usage threshold; commercial use above that requires a Stability commercial license.
- **ComfyUI wrapper license:** Defers to the upstream VideoMaMa license (CC BY-NC 4.0). <UNVERIFIED — repo has no separate LICENSE file at time of research>
- **Attribution:** Cite the CVPR 2026 paper; credit "VideoMaMa (KAIST CVLab / Adobe Research)" in user-facing docs. Note both licenses to artists.

## Example imagery
All candidate visuals come from sources whose license does not permit redistribution in our BSD-3-Clause docs without explicit permission.

1. **Project-page teaser / hero comparison** — https://cvlab-kaist.github.io/VideoMaMa/ shows side-by-side input mask vs. refined alpha on hair-heavy subjects. Attribution: KAIST CVLab / Adobe Research, CC BY-NC 4.0. **Not redistributable** under BSD-3-Clause; link out instead. <UNVERIFIED exact license tag on individual assets>
2. **arXiv paper figures (Figs. 1, 4–6)** — https://arxiv.org/html/2601.14255v1 — qualitative before/after grids showing alpha edge quality vs. baselines (MatAnyone, RVM). arXiv default license is "arXiv perpetual non-exclusive" — **not redistributable**; cite and link.
3. **ComfyUI node example workflow output** — https://github.com/okdalto/ComfyUI-VideoMaMa `examples/` — shows the node graph and a sample run. Repo license unspecified; **safer to recreate our own example shot in-house** and host that, rather than redistribute.

Recommendation: generate our own teaser using a CC0/owned plate run through the plugin, and link out to the project page for upstream comparisons.

## Citations
```bibtex
@article{lim2026videomama,
  title   = {VideoMaMa: Mask-Guided Video Matting via Generative Prior},
  author  = {Lim, Sangbeom and Oh, Seoung Wug and Huang, Jiahui and
             Yoon, Heeji and Kim, Seungryong and Lee, Joon-Young},
  journal = {arXiv preprint arXiv:2601.14255},
  year    = {2026}
}
```
