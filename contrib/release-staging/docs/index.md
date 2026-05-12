---
title: Home
nav_order: 1
---

# AIFX

State-of-the-art ComfyUI AI models, available as OpenFX plugins for any
OFX-compatible compositor, editor, or color grading tool.

> **Status:** pre-release. Plugins are functional but undergoing testing in
> production hosts.

## What's in the box

Seven plugins, all bridging an OFX host to a ComfyUI server:

- **[Depth Anything V3](plugins/depth_da3.md)** — monocular depth estimation, per-frame.
- **[NormalCrafter](plugins/normal_crafter.md)** — surface normal maps, per-frame.
- **[DepthCrafter](plugins/depth_crafter.md)** — temporally consistent video depth, diffusion-based.
- **[SAM3 Segmentation](plugins/segmentation_sam3.md)** — text/click-prompted mask propagation through a clip.
- **[MaMa Matting](plugins/matte_mama.md)** — diffusion-based high-quality video alpha matting.
- **[MatAnyone2 Matting](plugins/matte_ma2.md)** — fast recurrent video alpha matting.
- **[SeedVR2 Upscaler](plugins/upscale_seedvr2.md)** — generative video super-resolution.

## Get started

1. **[Install the plugins](installation.md)** in your OFX host.
2. **[Set up the ComfyUI server](comfyui-server-setup.md)** with the right
   custom nodes and model weights.
3. Apply a plugin to a clip in your host. Set the server URL and the
   shared-folder paths. Render.

## Important: model weight licenses

The **plugin code** is BSD-3-Clause and freely usable, including for
commercial production. The **AI model weights** are governed by their
upstream licenses. Several models in this suite are restricted to
non-commercial use.

The plugins that are **safe for paid commercial production work** are:

- [Depth Anything V3](plugins/depth_da3.md) (with Apache 2.0 variants only).
- [SAM3 Segmentation](plugins/segmentation_sam3.md) (with attribution; some
  prohibited use cases).
- [SeedVR2 Upscaler](plugins/upscale_seedvr2.md).

See each plugin page for the exact license breakdown.

## Documentation

- **[Installation](installation.md)** — getting plugins into your host.
- **[ComfyUI server setup](comfyui-server-setup.md)** — the model server side.
- **[Workflow customization](workflow-customization.md)** — replacing or
  extending the workflows.
- **[Architecture](architecture.md)** — for advanced users and contributors.
- **[Troubleshooting](troubleshooting.md)** — common errors and fixes.
- **[Plugin reference](plugins/)** — one page per plugin.

## Acknowledgements

This work was supported by **CNC (Centre national du cinéma et de l'image
animée)**.

Each plugin's documentation page credits the upstream researchers whose work
makes this project possible. The plugins build on the
[OpenFX](https://github.com/AcademySoftwareFoundation/openfx) specification
maintained by the Academy Software Foundation.
