# matte_ma2 — Research Brief (MatAnyone2)

## Summary
`matte_ma2` takes an input clip plus a binary mask seed (typically generated upstream by SAM3) and returns a soft alpha matte with hair-and-edge detail suitable for compositing. Under the hood it runs **MatAnyone2**, a recurrent memory-propagation video matting network — the lighter, faster counterpart to `matte_mama` (which is diffusion-based). Typical use: rotoscoping a person/subject for keying, background replacement, or relighting passes where you want clean edges without paying the per-frame cost of a diffusion sampler.

## Upstream sources
- **Paper (v2):** *MatAnyone 2: Scaling Video Matting via a Learned Quality Evaluator*, Peiqing Yang, Shangchen Zhou, Kai Hao, Qingyi Tao — CVPR 2026 (Highlight). arXiv: https://arxiv.org/abs/2512.11782
- **Paper (v1, foundation):** *MatAnyone: Stable Video Matting with Consistent Memory Propagation*, Peiqing Yang, Shangchen Zhou, Jixin Zhao, Qingyi Tao, Chen Change Loy — CVPR 2025. arXiv: https://arxiv.org/abs/2501.14677
- **MatAnyone2 is a separate publication** (not a fork): a successor paper from the same lead author / S-Lab @ NTU + SenseTime, introducing a learned Matting Quality Evaluator (MQE) for scaling training data. v1 and v2 share architectural lineage (memory-propagation recurrent matter).
- **Project page (v2):** https://pq-yang.github.io/projects/MatAnyone2/
- **Project page (v1):** https://pq-yang.github.io/projects/MatAnyone/
- **Official GitHub (v2):** https://github.com/pq-yang/MatAnyone2
- **Official GitHub (v1):** https://github.com/pq-yang/MatAnyone
- **ComfyUI node (matches plugin's `MatAnyone2` node):** https://github.com/spiritform/comfy-matanyone2 — exposes `Load MatAnyone2 Model` + `MatAnyone2 Video Matting`. Alternative: https://github.com/FuouM/ComfyUI-MatAnyone (also exposes a `MatAnyone2Video` node)
- **Model weights:** `matanyone2.pth` from https://github.com/pq-yang/MatAnyone2/releases/download/v1.0.0/matanyone2.pth — also mirrored on Hugging Face at `PeiqingYang/MatAnyone2`. Checkpoint size ~400 MB <UNVERIFIED — node README states "~400MB"; exact size not pinned in upstream release notes>.

## Technical approach
MatAnyone2 is a **recurrent video matting network with consistent memory propagation**: given a target mask in the first frame, the network maintains a memory bank of features from previous frames and fuses them with the current frame via region-adaptive memory fusion (stable semantics in the core, fine detail at boundaries). v2 adds a learned Matting Quality Evaluator that lets training scale on unlabeled video. Because each frame is a single forward pass with a recurrent state — no iterative denoising — it is dramatically faster and lighter than diffusion-based matting (`matte_mama`). Concrete FPS / VRAM numbers are not published in a single canonical table <UNVERIFIED>; in practice MatAnyone-class models run at interactive rates on a single mid-range GPU, whereas diffusion matters need high-end VRAM and multi-second-per-frame sampling.

## Requirements & limitations
- **VRAM:** substantially lower than diffusion matters; expected to fit on 8–12 GB GPUs at 1080p <UNVERIFIED — upstream does not publish a hard minimum>.
- **Sequence length:** recurrent design supports arbitrarily long clips, but memory state can drift on very long shots, occlusions, or shot changes. Re-seed with a fresh mask per shot.
- **Resolution:** no hard cap upstream (`--max-size` lets you downsample if min-dim exceeds a threshold); 4K works but is slower and VRAM-hungry.
- **Seed quality matters:** the binary mask from SAM3 defines the target. A poor seed (missed limbs, halo) propagates.
- **Failure modes:** fast motion blur with thin structures, near-camera transparent objects, identical-color BG/FG, multi-instance ambiguity (it tracks the seeded subject only).
- **Humans-first:** trained primarily on human video matting; non-human subjects are out-of-distribution and may degrade.

## License
- **Code license:** **NTU S-Lab License 1.0** (both `pq-yang/MatAnyone` and `pq-yang/MatAnyone2`). This is a **non-commercial** research license — redistribution and use are restricted to non-commercial purposes. Commercial use requires a separate agreement with NTUitive / SenseTime.
- **Weights license:** same NTU S-Lab License 1.0 applies to the released checkpoints <UNVERIFIED — repos state code license; weights typically inherit but verify on HF model card>.
- **ComfyUI wrapper (`spiritform/comfy-matanyone2`):** "provided as-is"; no explicit OSI license declared <UNVERIFIED>. Node depends on upstream MatAnyone2 and inherits its restrictions.
- **Attribution requirements:** cite both papers (v1 and v2) when publishing results; preserve copyright notices when redistributing code.

## Example imagery
1. **Teaser (v2 project page):** https://pq-yang.github.io/MatAnyone2/raw/main/assets/teaser.jpg — shows alpha-matte quality on hair/translucent edges across difficult shots. Attribution: © Yang et al., S-Lab NTU. Non-commercial research use only — **not redistributable in BSD-3-Clause docs without explicit permission**.
2. **v1-vs-v2 comparison:** https://pq-yang.github.io/MatAnyone2/raw/main/assets/matanyone1vs2.jpg — side-by-side showing improved boundary fidelity. Same license caveat — **not redistributable**.
3. **Animated teaser:** https://pq-yang.github.io/MatAnyone2/raw/main/assets/teaser_demo.gif — motion-domain matting demo. Same license caveat — **not redistributable**.

Recommendation: link to upstream URLs in release docs rather than copying images into the BSD-3-Clause-licensed openfx repo. If a self-contained example is needed, generate one in-house with a CC0 plate and ship that instead.

## Citations
```bibtex
@InProceedings{yang2026matanyone2,
  title     = {{MatAnyone 2}: Scaling Video Matting via a Learned Quality Evaluator},
  author    = {Yang, Peiqing and Zhou, Shangchen and Hao, Kai and Tao, Qingyi},
  booktitle = {CVPR},
  year      = {2026}
}

@InProceedings{yang2025matanyone,
  title     = {{MatAnyone}: Stable Video Matting with Consistent Memory Propagation},
  author    = {Yang, Peiqing and Zhou, Shangchen and Zhao, Jixin and Tao, Qingyi and Loy, Chen Change},
  booktitle = {CVPR},
  year      = {2025}
}
```
