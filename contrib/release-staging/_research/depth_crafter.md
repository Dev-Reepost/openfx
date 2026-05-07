# DepthCrafter — Research Brief

## Summary
DepthCrafter takes an input video clip (an image sequence) and produces a temporally consistent depth sequence — one depth map per input frame, scaled in the same relative depth space across the whole window. Unlike per-frame models such as Depth Anything (which estimate each frame independently and tend to flicker, swim, and rescale across cuts), DepthCrafter is a video diffusion model that processes a long window of frames jointly, so depth is stable over time. Typical compositing uses: 2.5D camera moves, depth-based defocus / atmospherics, relighting, rotoscoping assists, and stereo/parallax generation from monocular plates.

## Upstream sources
- **Paper:** Hu, Gao, Li, Zhao, Cun, Zhang, Quan, Shan — *DepthCrafter: Generating Consistent Long Depth Sequences for Open-world Videos* — CVPR 2025 (Highlight). arXiv: https://arxiv.org/abs/2409.02095
- **Project page:** https://depthcrafter.github.io/ (side-by-side comparison videos against Depth Anything, Marigold, NVDS, etc.)
- **Official GitHub:** https://github.com/Tencent/DepthCrafter
- **ComfyUI node implementation:** https://github.com/akatz-ai/ComfyUI-DepthCrafter-Nodes (registers `DepthCrafter` and `DownloadAndLoadDepthCrafterModel`). Note: this is the akatz-ai port, not a kijai wrapper — kijai does not maintain a DepthCrafter wrapper as of the search.
- **Model weights:** https://huggingface.co/tencent/DepthCrafter — `diffusion_pytorch_model.safetensors`, ~3.05 GB (repo total ~3.14 GB). Also requires the SVD base: `stabilityai/stable-video-diffusion-img2vid-xt` (~9 GB) cached under `models/depthcrafter/stabilityai_stable-video-diffusion-img2vid-xt/`.

## Technical approach
DepthCrafter fine-tunes a pre-trained image-to-video diffusion backbone (Stable Video Diffusion / SVD-XT) into a video-to-depth model via a three-stage training schedule that mixes realistic and synthetic depth datasets. Because the prior is a video diffusion UNet with temporal attention, the model treats depth estimation as a denoising problem over an entire frame window jointly, which is what gives it temporal consistency without requiring optical flow or camera poses. The trained model can produce variable-length sequences in a single pass up to ~110 frames; longer clips are handled by sliding-window inference with a 25-frame overlap that is blended/aligned. Reported figures of merit: state-of-the-art zero-shot performance on open-world video depth benchmarks (AbsRel / δ1) versus Depth Anything, Marigold, NVDS, and ChronoDepth.

## Requirements & limitations
- **VRAM:** ~26 GB at 1024×576, ~9 GB at 512×256 (official repo). The ComfyUI port advertises an 8 GB minimum with CPU offload (~25% saving) or sequential CPU offload (~37% saving, slower).
- **Sequence length / window:** up to **110 frames per pass** (window_size, recommended 75–110). Longer clips use sliding-window with overlap=25.
- **Resolution:** dimensions must be multiples of 64. Official examples use 1024×576 (high) and 512×256 (low). Higher resolution scales VRAM roughly quadratically.
- **Inference time:** ~2.1 fps at 1024×576 and ~8.6 fps at 512×256 on an A100 (i.e. ~465 ms/frame at high res). Multiple denoising steps (typ. 5–25) multiply this.
- **Known failure modes:** outputs are **affine-invariant relative depth** (not metric); scale/shift drift can occur across overlapping windows on very long shots; very fast motion or heavy motion blur can degrade temporal stability; high-frequency texture sometimes appears in depth (residual diffusion noise) at low step counts; transparent/reflective surfaces handled poorly (inherited from SVD prior).

## License
- **Code license (Tencent/DepthCrafter):** custom Tencent license — permits academic/research/education use and redistribution with notice, but explicitly **prohibits commercial or production use** ("refrain from using it for any commercial or production purposes under any circumstances"). Some bundled code is MIT (Stability AI). <UNVERIFIED>: exact license name string not stated; treat as "Tencent DepthCrafter Non-Commercial License".
- **Weights license (HF tencent/DepthCrafter):** same non-commercial restriction inherited from the repo LICENSE; HF model card tags it as `license: other`. SVD-XT base weights additionally carry the Stability AI Non-Commercial Community License.
- **ComfyUI port (akatz-ai):** repo-level license <UNVERIFIED>; downstream use is still bound by Tencent's non-commercial terms on weights.
- **Attribution:** "The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software" — must reproduce notice; also cite the CVPR 2025 paper.

## Example imagery
1. **Project-page hero comparison reel** — https://depthcrafter.github.io/ (top of page)
   - Shows: side-by-side input video / DepthCrafter depth / per-frame baselines, demonstrating no-flicker temporal consistency.
   - Attribution: Tencent AI Lab et al., CVPR 2025. License clause governing media on the project page is <UNVERIFIED> (no explicit reuse license shown).
   - Redistribution in BSD-3-Clause docs: **not clearly permitted** — recommend hot-linking or replacing with our own depth output rather than copying.
2. **GitHub README teaser GIF** — https://github.com/Tencent/DepthCrafter/raw/main/assets/teaser.gif (path <UNVERIFIED>; check `assets/` in repo).
   - Shows: open-world clips with their depth sequences animated.
   - Attribution: same as above; falls under repo LICENSE (non-commercial).
   - Redistribution: not compatible with our BSD-3-Clause docs given the non-commercial weight/code terms — link instead.
3. **Self-generated depth pass from our plugin** (recommended)
   - Shows: a CC0 / our own plate run through `depth_crafter` plugin, side-by-side with a per-frame Depth Anything pass to highlight flicker reduction.
   - Attribution: none required (our own output); safe for BSD-3-Clause docs.
   - This is the only option with clean licensing — strongly recommend this path.

## Citations
```bibtex
@inproceedings{hu2025depthcrafter,
  author    = {Hu, Wenbo and Gao, Xiangjun and Li, Xiaoyu and Zhao, Sijie and
               Cun, Xiaodong and Zhang, Yong and Quan, Long and Shan, Ying},
  title     = {DepthCrafter: Generating Consistent Long Depth Sequences for Open-world Videos},
  booktitle = {Proceedings of the IEEE/CVF Conference on Computer Vision and Pattern Recognition (CVPR)},
  year      = {2025},
  note      = {Highlight},
  eprint    = {2409.02095},
  archivePrefix = {arXiv}
}
```
