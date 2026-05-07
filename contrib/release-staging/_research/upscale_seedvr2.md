# SeedVR2 — Research Brief

## Summary
SeedVR2 takes a low-resolution input clip (image sequence) and produces a temporally consistent high-resolution clip — generative super-resolution that hallucinates plausible detail rather than just sharpening pixels. Unlike per-frame upscalers (Real-ESRGAN, ESRGAN, Topaz Gigapixel single-frame modes), SeedVR2 is a video diffusion-transformer that conditions on a window of frames at once via temporal attention, so reconstructed micro-detail (skin pores, fabric weave, foliage) stays coherent across time instead of swimming or boiling. Typical VFX uses: legacy SD/HD footage uprez to UHD/4K, plate restoration on damaged or compressed elements, archival recovery of degraded masters, and AI-generated-content cleanup (de-artifacting 720p AIGC video to a deliverable resolution).

## Upstream sources
- **Paper:** Wang, Lin (Shanchuan), Lin (Zhijie), Ren, Wei, Yue, Zhou, Chen, Zhao, Yang, Xiao, Loy, Jiang — *SeedVR2: One-Step Video Restoration via Diffusion Adversarial Post-Training* — ICLR 2026. arXiv: https://arxiv.org/abs/2506.05301 (June 2025 preprint).
- **Predecessor:** Wang et al., *SeedVR: Seeding Infinity in Diffusion Transformer Towards Generic Video Restoration* — CVPR 2025 (Highlight).
- **Project page:** https://iceclear.github.io/projects/seedvr2/ (side-by-side comparison reels vs. multi-step diffusion VR baselines, real-world degraded clips, AIGC cleanup).
- **Official GitHub (ByteDance Seed):** https://github.com/ByteDance-Seed/SeedVR (covers SeedVR and SeedVR2; mirror at https://github.com/IceClear/SeedVR2).
- **ComfyUI node implementation:** https://github.com/numz/ComfyUI-SeedVR2_VideoUpscaler (NumZ + AInVFX/Adrien Toupet). Registers `SeedVR2LoadDiTModel`, `SeedVR2LoadVAEModel`, `SeedVR2VideoUpscaler`, plus a torch.compile settings node.
- **Model weights:**
  - https://huggingface.co/ByteDance-Seed/SeedVR2-3B — 3B-parameter DiT (FP16 ~6 GB on disk; full precision <UNVERIFIED>).
  - https://huggingface.co/ByteDance-Seed/SeedVR2-7B — 7B-parameter DiT (FP16 ~14 GB on disk; <UNVERIFIED>).
  - https://huggingface.co/numz/SeedVR2_comfyUI and https://huggingface.co/AInVFX/SeedVR2_comfyUI — ComfyUI-packaged FP16 / FP8 variants used by the node.
  - https://huggingface.co/cmeka/SeedVR2-GGUF — GGUF Q8_0 / Q4_K_M quantizations for low-VRAM users.
  - VAE checkpoint: ~2–4 GB across configurations.

## Technical approach
SeedVR2 is a Diffusion Transformer (DiT) — the same architectural family as Sora-style video generators — operating in a learned VAE latent space: the VAE encodes input frames to a compact 4D latent tensor, the DiT denoises that latent conditioned on the low-quality input, and the VAE decodes back to pixels. SeedVR2's headline contribution is **one-step inference**: where prior diffusion-based VR models needed 15–50 denoising steps, SeedVR2 distills the process into a single step via diffusion adversarial post-training (a GAN objective applied on top of a pre-trained diffusion prior), making it >4× faster than multi-step baselines at comparable or better quality. Output resolution is **arbitrary** (not a fixed 2×/4× multiple) — the user picks a target shortest-edge resolution and the model handles it via an adaptive window attention mechanism that adjusts attention windows to the output size, avoiding seams at high res. It beats per-frame ESRGAN-class models on temporal consistency (no flicker/swim) and on perceptual realism of generated detail; reported figures of merit include PSNR, LPIPS, DOVER, and user-study preference vs. RealBasicVSR / Upscale-A-Video / VEnhancer.

## Requirements & limitations
- **VRAM (per ComfyUI port table):**
  - 7B FP16: ~20–24 GB+ (full quality)
  - 7B FP8 mixed: ~16–20 GB
  - 3B FP16: ~12–16 GB
  - 3B FP8: ~8–12 GB
  - GGUF Q4_K_M: ~4–8 GB minimum with BlockSwap + VAE tiling.
  - VAE adds ~2–4 GB. Official repo: 1×H100-80G handles 100×720×1280; 4×H100-80G handles 1080p / 2K via sequence parallel (`sp_size=4`).
- **Sequence length:** frame batch must follow the **4n+1 rule** (1, 5, 9, 13, 17, 21, 25, …); minimum 5 frames for temporal consistency, single-image mode at batch=1. Recommended: set batch to full shot length when VRAM allows; otherwise tile temporally with 0–16-frame overlap.
- **Resolution:** any target divisible by 2 (lossless padding); aspect ratio preserved. No hard maximum beyond VRAM. VAE becomes the bottleneck above ~1080p — enable VAE encode/decode tiling.
- **Upscale ratio:** **not fixed** — user-specified target shortest-edge resolution (e.g. 540p → 1080p, 720p → 4K). Effective ratio is whatever the input/target implies.
- **Inference time:** not published as fps numbers; the paper claims >4× speedup over multi-step diffusion VR baselines. Practical speed scales with model size (3B faster than 7B), batch (larger faster per-frame), torch.compile (significant speedup), resolution. <UNVERIFIED> exact ms/frame figure.
- **Known failure modes:**
  - Not robust to **heavy degradations** or **very large motion** — may fail to remove the degradation or produce unpleasing detail.
  - On **very lightly degraded** input (e.g. clean 720p AIGC), it tends to over-generate detail → **oversharpened / "fake-crisp"** look.
  - Real performance may not perfectly match paper claims.
  - macOS/MPS: BlockSwap unavailable (unified memory).
  - Older GPUs without bfloat16 (e.g. GTX 970-class): automatic fallback path, with caveats.
  - Long clips: streaming/`--chunk_size` mode required to avoid OOM.

## License
- **Code license (ByteDance-Seed/SeedVR and ComfyUI port):** **Apache 2.0** — commercial-friendly, requires preserving copyright/license notice.
- **Weights license (SeedVR2-3B, SeedVR2-7B on HuggingFace):** **Apache 2.0** per the model cards. This is unusually permissive for ByteDance — confirms commercial use is allowed, though the model card lists usage caveats (limitations) rather than legal restrictions.
- **Attribution requirements:** preserve Apache 2.0 NOTICE; cite the ICLR 2026 paper (BibTeX below). The numz ComfyUI repo also requests attribution to NumZ + AInVFX (Adrien Toupet) for the integration work.

## Example imagery
1. **Project-page comparison reel** — https://iceclear.github.io/projects/seedvr2/
   - Shows: side-by-side LR input / SeedVR2 / multi-step baselines (RealBasicVSR, Upscale-A-Video, VEnhancer); good demonstration of temporal stability and detail synthesis.
   - Attribution: Wang et al., ByteDance Seed, ICLR 2026.
   - Redistribution in BSD-3-Clause docs: project page has no explicit media reuse license; <UNVERIFIED>. Recommend hot-link rather than copy.
2. **GitHub README teaser** — https://github.com/ByteDance-Seed/SeedVR (assets in `assets/`)
   - Shows: degraded real-world plates restored to high resolution.
   - Attribution: same as above; under repo Apache 2.0 — **redistributable with NOTICE preserved**, the only one of the three with clean licensing.
3. **Self-generated uprez from our plugin** (recommended)
   - Shows: CC0 archival plate or our own captured 540p/720p footage uprezzed to 4K via the `upscale_seedvr2` plugin, alongside a Real-ESRGAN per-frame uprez to highlight flicker reduction and detail coherence.
   - Attribution: none required (our own render); fully BSD-3-Clause-safe.
   - Strongly recommended as the primary doc image.

## Citations
```bibtex
@inproceedings{wang2026seedvr2,
  author    = {Wang, Jianyi and Lin, Shanchuan and Lin, Zhijie and Ren, Yuxi and
               Wei, Meng and Yue, Zongsheng and Zhou, Shangchen and Chen, Hao and
               Zhao, Yang and Yang, Ceyuan and Xiao, Xuefeng and Loy, Chen Change and
               Jiang, Lu},
  title     = {SeedVR2: One-Step Video Restoration via Diffusion Adversarial Post-Training},
  booktitle = {International Conference on Learning Representations (ICLR)},
  year      = {2026},
  eprint    = {2506.05301},
  archivePrefix = {arXiv}
}

@inproceedings{wang2025seedvr,
  author    = {Wang, Jianyi and Lin, Zhijie and others},
  title     = {SeedVR: Seeding Infinity in Diffusion Transformer Towards Generic Video Restoration},
  booktitle = {Proceedings of the IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)},
  year      = {2025},
  note      = {Highlight}
}
```
