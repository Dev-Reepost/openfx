# ComfyUI SAM Segmentation Plugin

OpenFX plugin for semantic segmentation using Grounding DINO and Segment Anything Model (SAM) via ComfyUI.

## Overview

This plugin uses text prompts to automatically segment objects in images. It combines:

- **Grounding DINO** - Text-based object detection
- **SAM (Segment Anything Model)** - High-quality segmentation masks

Based on: <https://github.com/Dev-Reepost/flame_comfyui_segmentation>

## Features

- **Text-based segmentation** - Describe what to segment (e.g., "person", "car", "foreground")
- **Multiple SAM models** - Choose quality vs. speed (ViT-H, ViT-L, ViT-B)
- **Configurable threshold** - Adjust detection sensitivity
- **Preprocessing** - Configurable resolution for optimal quality
- **Color space handling** - Linear ↔ sRGB conversion support

## Requirements

### ComfyUI Server

- ComfyUI with [Segment Anything extension](https://github.com/storyicon/comfyui_segment_anything)
- Grounding DINO models installed
- SAM models installed

### Models

Download and install to ComfyUI's models directory:

**Grounding DINO:**

- `GroundingDINO_SwinT_OGC (694MB)` - Recommended
- `GroundingDINO_SwinB (938MB)` - More accurate

**SAM:**

- `sam_vit_h (2.56GB)` - Highest quality (recommended)
- `sam_vit_l (1.25GB)` - Balanced
- `sam_vit_b (375MB)` - Fastest

### Shared Storage

- Network-accessible directory for image exchange
- Must be accessible by both OFX host and ComfyUI server
- EXR format support required

## Parameters

### Segmentation Group

**Prompt** (String)

- Text description of what to segment
- Examples: "foreground", "person", "car", "building"
- Default: "foreground"

**Threshold** (Double, 0.0-1.0)

- Detection confidence threshold
- Higher = stricter detection
- Default: 0.3

**SAM Model** (Choice)

- ViT-H (2.56GB) - Highest quality
- ViT-L (1.25GB) - Balanced
- ViT-B (375MB) - Fastest
- Default: ViT-H

**Grounding DINO Model** (Choice)

- SwinT (694MB) - Faster
- SwinB (938MB) - More accurate
- Default: SwinT

**Resolution** (Int, 512-4096)

- Preprocessing resolution (longer side)
- Higher = better quality but slower
- Default: 1080

### Server Configuration Group

(Inherited from BasePlugin)

**Server Address** (String)

- ComfyUI server hostname or IP
- Default: "localhost"

**Server Port** (Int, 1-65535)

- ComfyUI server port
- Default: 8188

### Storage Configuration Group

(Inherited from BasePlugin)

**Shared Mount Path** (String)

- Network storage path accessible to both OFX host and ComfyUI
- Example: "/Volumes/shared" or "//server/share"

**Project Name** (String)

- Project identifier for organizing files
- Example: "my_project"

### Output Options Group

**Input: Linear to sRGB** (Boolean)

- Convert input from linear to sRGB color space
- Default: true

**Output: sRGB to Linear** (Boolean)

- Convert output from sRGB to linear color space
- Default: true

**Output Alpha Matte** (Boolean)

- Enable separate alpha matte output (future feature)
- Default: false (disabled)

## Workflow

The plugin executes this ComfyUI workflow:

1. **Load Input** - Reads EXR from shared storage
2. **Load Models** - Loads Grounding DINO and SAM models
3. **Segment** - Detects and segments based on text prompt
4. **Generate Outputs**:
   - Preprocessed image (SAMPreprocessor)
   - Alpha matte (MaskToImage)
5. **Save Results** - Writes both outputs as EXR files
6. **Return to OFX** - Plugin reads result and displays in host

## Building

The plugin is built as part of the ComfyUI plugins suite:

```bash
# From OpenFX root
cmake --preset conan-release -DBUILD_COMFYUI_PLUGINS=ON
cmake --build build/Release --target SAMSegmentation --config Release
```

**Output:**

- `build/Release/Release/SAMSegmentation.ofx.bundle/` (macOS)
- Plugin binary: 1.7 MB (arm64)

## Installation

### macOS

```bash
cp -r build/Release/Release/SAMSegmentation.ofx.bundle ~/Library/OFX/Plugins/
```

### Linux

```bash
cp -r build/Release/SAMSegmentation.ofx.bundle /usr/OFX/Plugins/
```

### Windows

```
Copy SAMSegmentation.ofx.bundle to C:\Program Files\Common Files\OFX\Plugins\
```

## Usage Example

1. **Start ComfyUI server**:

   ```bash
   cd ~/ComfyUI
   python main.py --listen 0.0.0.0 --port 8188
   ```

2. **Configure plugin in OFX host** (Flame, Nuke, Resolve, etc.):
   - Server Address: `192.168.1.211` (or `localhost`)
   - Server Port: `8188`
   - Shared Mount Path: `/Volumes/shared`
   - Project Name: `my_project`

3. **Set segmentation parameters**:
   - Prompt: `"person"`
   - Threshold: `0.3`
   - SAM Model: ViT-H
   - Resolution: `1080`

4. **Apply to clip and render**

## Output Files

The plugin writes/reads these files to shared storage:

**Input** (written by plugin):

```
/mount/project/input_0001.exr
```

**Outputs** (written by ComfyUI):

```
/mount/project/OutImage_00001.exr  - Preprocessed image
/mount/project/OutMatte_00001.exr  - Alpha matte
```

The plugin currently returns the preprocessed image. Alpha matte output requires multi-clip support (future enhancement).

## Limitations

- **Single output** - Currently returns preprocessed image only (not matte)
- **No real-time preview** - Must render to see results
- **No progress bar** - Executes without visual progress indication
- **Frame-by-frame** - No frame caching (processes every frame)

## Future Enhancements

- Multi-output support (RGB + alpha matte)
- Progress bar integration
- Interactive point/box selection for segmentation
- Frame caching
- Model availability validation
- SAM2 support (when available in ComfyUI)

## Technical Details

**Plugin Architecture:**

- Inherits from `ComfyUI::BasePlugin`
- Implements `buildWorkflow()` to generate ComfyUI JSON
- Uses REST API for workflow submission
- Uses WebSocket for execution monitoring
- Uses TinyEXR for image I/O

**Workflow JSON:**

- 7 nodes (LoadEXR, models, segment, preprocess, convert, save×2)
- Based on reference implementation
- Client ID tracking for WebSocket monitoring

**Performance:**

- Model loading: ~5-10 seconds (first run)
- Segmentation: ~1-5 seconds per frame (depends on resolution/model)
- Image I/O: ~100-500ms per frame

## Troubleshooting

**"Failed to connect to server"**

- Check ComfyUI is running: `curl http://localhost:8188/system_stats`
- Verify server address and port parameters

**"Model not found"**

- Install required models in ComfyUI's `models/` directory
- Check model names match exactly

**"Failed to find output file"**

- Verify shared mount path is accessible
- Check project name doesn't contain special characters
- Ensure ComfyUI has write permissions

**Slow performance**

- Reduce resolution (720 instead of 1080)
- Use smaller SAM model (ViT-B instead of ViT-H)
- Use SwinT Grounding DINO model

## License

BSD-3-Clause (same as OpenFX)

## References

- [OpenFX Specification](http://openeffects.org/)
- [ComfyUI](https://github.com/comfyanonymous/ComfyUI)
- [Segment Anything Extension](https://github.com/storyicon/comfyui_segment_anything)
- [Reference Implementation](https://github.com/Dev-Reepost/flame_comfyui_segmentation)
- [SAM Paper](https://arxiv.org/abs/2304.02643)
- [Grounding DINO Paper](https://arxiv.org/abs/2303.05499)
