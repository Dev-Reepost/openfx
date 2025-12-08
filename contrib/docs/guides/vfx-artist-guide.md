# ComfyUI OFX Plugins - VFX Artist Guide

User guide for VFX artists using ComfyUI OFX plugins in professional post-production workflows.

## Table of Contents

1. [Introduction](#introduction)
2. [System Requirements](#system-requirements)
3. [Installation](#installation)
4. [Quick Start](#quick-start)
5. [Plugin Reference](#plugin-reference)
6. [Workflows](#workflows)
7. [Troubleshooting](#troubleshooting)
8. [Tips & Best Practices](#tips--best-practices)

---

## Introduction

### What Are ComfyUI OFX Plugins?

ComfyUI OFX plugins bring AI-powered image processing to your favorite compositing and editing applications (Flame, Nuke, DaVinci Resolve, etc.). They connect to a ComfyUI server to leverage state-of-the-art AI models for:

- **Segmentation** - Automatically isolate people, objects, or specific elements
- **Upscaling** - Enhance resolution using AI upscaling models
- **Inpainting** - Remove objects and fill in backgrounds intelligently
- **Style Transfer** - Apply artistic styles to footage
- **Depth Estimation** - Generate depth maps from 2D footage

### How It Works

```
Your OFX Host (Flame/Nuke/Resolve)
         ↓
   OFX Plugin (on your workstation)
         ↓
   Network Connection
         ↓
   ComfyUI Server (can be remote)
         ↓
   AI Processing (GPU-accelerated)
         ↓
   Results Back to Your Timeline
```

**Key Benefits:**
- ✅ Works in your existing tools (no new software to learn)
- ✅ Leverage powerful AI models
- ✅ Server can be remote (use facility render farm)
- ✅ Professional EXR workflow (32-bit float)
- ✅ Deterministic (same settings = same results)

---

## System Requirements

### Client (Your Workstation)

**Minimum:**
- **OS:** macOS 10.15+, Linux (Ubuntu 20.04+), Windows 10+
- **RAM:** 8 GB
- **Storage:** 100 MB for plugins
- **Network:** Access to ComfyUI server

**Supported OFX Hosts:**
- Autodesk Flame 2023+
- Foundry Nuke 13.0+
- Blackmagic DaVinci Resolve Studio 18+
- Assimilate Scratch
- FilmLight Baselight
- Other OFX-compatible hosts

### Server (ComfyUI)

**Minimum:**
- **OS:** Linux (recommended), macOS, Windows
- **GPU:** NVIDIA RTX 3060 or better (12+ GB VRAM recommended)
- **RAM:** 16 GB (32 GB recommended for large models)
- **Storage:** 50+ GB for AI models
- **Python:** 3.10+
- **CUDA:** 11.8+ (for NVIDIA GPUs)

**Recommended Setup:**
- Dedicated server or workstation
- NVIDIA RTX 4090 or A6000 (24 GB VRAM)
- 64 GB RAM
- NVMe SSD for model storage
- 10 Gigabit Ethernet connection

### Network Storage

**Requirements:**
- Shared storage accessible by both client and server
- Minimum 100 MB/s throughput
- EXR file support
- Proper permissions for read/write

**Examples:**
- NAS (Synology, QNAP)
- SAN (Xsan, StorNext)
- Cloud storage (mounted locally)
- SMB/CIFS shares
- NFS mounts

---

## Installation

### Step 1: Install ComfyUI Server

**On your server machine:**

```bash
# Clone ComfyUI
git clone https://github.com/comfyanonymous/ComfyUI.git
cd ComfyUI

# Create virtual environment
python3 -m venv venv
source venv/bin/activate  # On Windows: venv\Scripts\activate

# Install dependencies
pip install -r requirements.txt

# Install Segment Anything extension (for SAM plugin)
cd custom_nodes
git clone https://github.com/storyicon/comfyui_segment_anything.git
cd comfyui_segment_anything
pip install -r requirements.txt
cd ../..

# Start server
python main.py --listen 0.0.0.0 --port 8188
```

**Verify server is running:**
```bash
# From another terminal
curl http://localhost:8188/system_stats
```

### Step 2: Download AI Models

Models are automatically downloaded on first use, but you can pre-download:

**For SAM Segmentation:**
```bash
cd ComfyUI/models/grounding-dino
# Download from https://github.com/IDEA-Research/GroundingDINO/releases
wget https://github.com/IDEA-Research/GroundingDINO/releases/download/v0.1.0-alpha/groundingdino_swint_ogc.pth

cd ../sams
# Download from https://github.com/facebookresearch/segment-anything
wget https://dl.fbaipublicfiles.com/segment_anything/sam_vit_h_4b8939.pth
```

**Model Sizes:**
- Grounding DINO SwinT: 694 MB
- Grounding DINO SwinB: 938 MB
- SAM ViT-H: 2.56 GB
- SAM ViT-L: 1.25 GB
- SAM ViT-B: 375 MB

### Step 3: Configure Shared Storage

**macOS:**
```bash
# Mount SMB share
open smb://server/shared

# Or use automount
sudo mkdir /Volumes/shared
# Edit /etc/fstab or use Disk Utility
```

**Linux:**
```bash
# Mount NFS
sudo mkdir /mnt/shared
sudo mount server:/export/shared /mnt/shared

# Or add to /etc/fstab for automatic mounting
server:/export/shared /mnt/shared nfs defaults 0 0
```

**Windows:**
```powershell
# Map network drive
net use Z: \\server\shared /persistent:yes
```

**Verify access:**
```bash
# Create test directory
mkdir /Volumes/shared/comfyui_test
touch /Volumes/shared/comfyui_test/test.txt

# Check from server
ls /path/to/shared/comfyui_test/test.txt
```

### Step 4: Install OFX Plugins

**macOS:**
```bash
# Download latest release
# Or build from source (see developer guide)

# Install to user directory (recommended)
cp -r SAMSegmentation.ofx.bundle ~/Library/OFX/Plugins/

# Or system directory (requires sudo)
sudo cp -r SAMSegmentation.ofx.bundle /Library/OFX/Plugins/
```

**Linux:**
```bash
# Install to user directory
mkdir -p ~/.local/share/OFX/Plugins
cp -r SAMSegmentation.ofx.bundle ~/.local/share/OFX/Plugins/

# Or system directory
sudo cp -r SAMSegmentation.ofx.bundle /usr/OFX/Plugins/
```

**Windows:**
```powershell
# Copy to system directory
Copy-Item SAMSegmentation.ofx.bundle "C:\Program Files\Common Files\OFX\Plugins\" -Recurse
```

### Step 5: Verify Installation

1. **Restart your OFX host**
2. **Check plugin appears:**
   - Flame: Tools → Effects → ComfyUI
   - Nuke: Toolbar → Other → ComfyUI
   - Resolve: Effects Library → OpenFX → ComfyUI

3. **Test connection:**
   - Apply plugin to clip
   - Set server address
   - Plugin should connect (check for errors)

---

## Quick Start

### Example 1: Segment a Person (Flame)

**Goal:** Isolate a person from background

1. **Load clip in Flame**
   - Import footage to Media Hub
   - Add to timeline

2. **Apply SAM Segmentation**
   - Right-click clip → Add OFX Effect
   - Select "ComfyUI SAM Segmentation"

3. **Configure Server** (Server Configuration group)
   - Server Address: `192.168.1.211` (your ComfyUI server)
   - Server Port: `8188`

4. **Configure Storage** (Storage Configuration group)
   - Shared Mount Path: `/Volumes/shared`
   - Project Name: `my_project`

5. **Configure Segmentation** (Segmentation group)
   - Prompt: `person`
   - Threshold: `0.3`
   - SAM Model: `SAM ViT-H (highest quality)`
   - Resolution: `1080`

6. **Render frame**
   - Right-click → Render Effect
   - Wait for processing (~5-10 seconds first time, ~2-3 seconds cached)
   - Check result

7. **Adjust if needed**
   - If person not detected: Lower threshold to `0.2`
   - If too much detected: Raise threshold to `0.4`
   - Try different prompt: `"person in foreground"`, `"main character"`

### Example 2: Segment Specific Object (Nuke)

**Goal:** Isolate a car for color correction

1. **Create Nuke script**
   ```
   Read (your_footage.exr)
     ↓
   ComfyUI_SAMSegmentation
     ↓
   Viewer
   ```

2. **Configure plugin:**
   ```
   Server Configuration:
     - Server Address: localhost
     - Server Port: 8188

   Storage Configuration:
     - Shared Mount Path: /mnt/shared
     - Project Name: shot_010

   Segmentation:
     - Prompt: red car
     - Threshold: 0.35
     - SAM Model: ViT-H
     - Resolution: 1080
   ```

3. **Render**
   - Frame range: 1001-1100
   - Format: EXR (32-bit float)
   - Check first frame before batch

4. **Use result:**
   ```
   ComfyUI_SAMSegmentation
     ↓
   Grade (color correct just the car)
     ↓
   Merge (combine with original)
   ```

### Example 3: Batch Processing (DaVinci Resolve)

**Goal:** Segment people in 100-frame clip

1. **Setup timeline**
   - Add clip to timeline
   - Trim to desired range

2. **Apply effect**
   - Effects Library → OpenFX → ComfyUI → SAM Segmentation
   - Drag onto clip

3. **Configure once**
   - Server: `localhost:8188`
   - Storage: `Z:\shared`
   - Project: `batch_job_001`
   - Prompt: `people`

4. **Render**
   - Deliver Page
   - Add to Render Queue
   - Format: EXR
   - Resolution: Match source
   - Start Render

5. **Monitor progress**
   - Check ComfyUI server logs
   - Watch shared storage for output files
   - First frame slower (model loading), then ~2-3 sec/frame

---

## Plugin Reference

### SAM Segmentation

**Purpose:** Text-based semantic segmentation using Grounding DINO + Segment Anything Model

**Location:** ComfyUI → Segmentation → SAM Segmentation

**Use Cases:**
- Isolate people, animals, objects
- Create alpha mattes for compositing
- Quick rotoscoping replacement
- Foreground/background separation

**Parameters:**

**Segmentation Group:**

| Parameter | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| Prompt | Text | - | "foreground" | What to segment (e.g., "person", "car", "building") |
| Threshold | Slider | 0.0-1.0 | 0.3 | Detection confidence (higher = stricter) |
| SAM Model | Dropdown | - | ViT-H | Quality vs speed (H=best, L=balanced, B=fast) |
| Grounding DINO Model | Dropdown | - | SwinT | Detection model (T=fast, B=accurate) |
| Resolution | Number | 512-4096 | 1080 | Preprocessing resolution (longer side) |

**Server Configuration Group:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| Server Address | Text | "localhost" | ComfyUI hostname or IP |
| Server Port | Number | 8188 | ComfyUI port |

**Storage Configuration Group:**

| Parameter | Type | Description |
|-----------|------|-------------|
| Shared Mount Path | Text | Network path accessible by both client and server |
| Project Name | Text | Identifier for organizing files |

**Output Options Group:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| Input: Linear to sRGB | Checkbox | On | Convert input color space |
| Output: sRGB to Linear | Checkbox | On | Convert output color space |
| Output Alpha Matte | Checkbox | Off | Separate matte output (future) |

**Performance:**

| Resolution | Model | First Frame | Cached Frames |
|------------|-------|-------------|---------------|
| 1080p | ViT-H | ~8-10 sec | ~2-3 sec |
| 1080p | ViT-L | ~5-7 sec | ~1-2 sec |
| 1080p | ViT-B | ~3-5 sec | ~1 sec |
| 4K | ViT-H | ~15-20 sec | ~5-7 sec |

**Tips:**

✅ **DO:**
- Start with default settings
- Test single frame before batch
- Use descriptive prompts ("person in red shirt")
- Cache results when possible
- Monitor server GPU usage

❌ **DON'T:**
- Use resolution higher than source
- Set threshold below 0.2 (too many false positives)
- Process 4K if 1080p is sufficient
- Batch large ranges without testing first

**Common Prompts:**

| Goal | Prompt | Threshold |
|------|--------|-----------|
| Isolate person | "person" | 0.3 |
| Foreground only | "foreground" | 0.3-0.4 |
| Specific object | "red car" | 0.35 |
| Multiple people | "people" | 0.25 |
| Background | "background" | 0.3 |
| Animal | "dog", "cat", "horse" | 0.3 |
| Building | "building", "house" | 0.35 |

---

## Workflows

### Workflow 1: Green Screen Replacement

**Scenario:** Replace green screen with AI-generated background

```
Source Clip (green screen)
     ↓
ComfyUI SAM Segmentation
  - Prompt: "person"
  - Creates clean matte
     ↓
Keyer (refine edges)
     ↓
Merge with new background
```

**Advantages over traditional keying:**
- Works with uneven lighting
- No green spill
- Handles hair/fine details better
- Works with non-green backgrounds

### Workflow 2: Selective Color Grading

**Scenario:** Color correct only specific elements

```
Source
  ↓
SAM Segmentation (isolate element)
  ↓
Grade (adjust isolated element)
  ↓
Merge (combine with original)
```

**Example - Enhance car color:**
1. Segment: Prompt "red car"
2. Grade: Increase saturation, adjust hue
3. Merge: Overlay result

### Workflow 3: Object Removal Prep

**Scenario:** Prepare matte for inpainting

```
Source
  ↓
SAM Segmentation
  - Prompt: "object to remove"
  ↓
Dilate (expand matte)
  ↓
Use as inpainting mask
```

### Workflow 4: Quick Roto

**Scenario:** Fast rotoscoping for simple shots

```
Source (person walking)
  ↓
SAM Segmentation
  - Prompt: "person"
  - Process every 10th frame
  ↓
Interpolate missing frames
  ↓
Refine edges (manual if needed)
```

**Time savings:**
- Traditional roto: 30-60 min for 100 frames
- SAM + interpolation: 5-10 min

---

## Troubleshooting

### Connection Issues

**Problem:** "Failed to connect to ComfyUI server"

**Solutions:**
1. **Verify server running:**
   ```bash
   curl http://your-server:8188/system_stats
   ```
   Expected: JSON response with system info

2. **Check firewall:**
   ```bash
   # Allow port 8188
   sudo ufw allow 8188  # Linux
   # Or configure macOS/Windows firewall
   ```

3. **Test from client:**
   ```bash
   ping your-server
   telnet your-server 8188
   ```

4. **Check server logs:**
   ```bash
   # In ComfyUI directory
   tail -f comfyui.log
   ```

### Performance Issues

**Problem:** Processing too slow

**Solutions:**
1. **Reduce resolution:**
   - Change from 1080 to 720
   - Or match source resolution if already lower

2. **Use smaller model:**
   - Change SAM model from ViT-H to ViT-L or ViT-B
   - Trade quality for speed

3. **Check GPU usage:**
   ```bash
   # On server
   nvidia-smi
   ```
   - GPU should be 80-100% during processing
   - If low, check for CPU bottleneck

4. **Enable caching:**
   - Processing Options → Enable Cache: On
   - Identical frames will be cached

5. **Upgrade hardware:**
   - More VRAM = larger batches
   - Faster GPU = faster processing

### Quality Issues

**Problem:** Segmentation not accurate

**Solutions:**
1. **Adjust threshold:**
   - Too much detected → Increase threshold (0.3 → 0.4)
   - Too little detected → Decrease threshold (0.3 → 0.2)

2. **Improve prompt:**
   - "person" → "person in foreground"
   - "car" → "red sports car"
   - Be specific but not too specific

3. **Try different model:**
   - SwinT → SwinB (more accurate detection)
   - ViT-B → ViT-H (better segmentation quality)

4. **Increase resolution:**
   - 720 → 1080 → 2160 (if needed)
   - More detail = better segmentation

5. **Preprocess footage:**
   - Denoise if very noisy
   - Color correct for better contrast
   - Increase brightness if too dark

### Storage Issues

**Problem:** "Failed to find output file"

**Solutions:**
1. **Verify shared storage:**
   ```bash
   # On client
   ls /Volumes/shared
   touch /Volumes/shared/test.txt

   # On server
   ls /path/to/shared
   cat /path/to/shared/test.txt
   ```

2. **Check permissions:**
   ```bash
   # Make sure server can write
   chmod 777 /path/to/shared/project_name
   ```

3. **Check disk space:**
   ```bash
   df -h /path/to/shared
   ```
   - EXR files: ~10-50 MB per frame
   - Ensure enough space

4. **Check project name:**
   - No special characters
   - No spaces (use underscores)
   - Should match on client and server

### Model Issues

**Problem:** "Model not found"

**Solutions:**
1. **Install models:**
   ```bash
   cd ComfyUI/models

   # Grounding DINO
   cd grounding-dino
   wget <model-url>

   # SAM
   cd ../sams
   wget <model-url>
   ```

2. **Check model names:**
   - Must match exactly (case-sensitive)
   - Check ComfyUI logs for expected name

3. **Restart ComfyUI:**
   ```bash
   # After adding models
   Ctrl+C
   python main.py --listen 0.0.0.0 --port 8188
   ```

4. **Verify models loaded:**
   - Check ComfyUI startup logs
   - Should see "Loading models..."

---

## Tips & Best Practices

### Optimization

**1. Resolution Management:**
```
Source Resolution → Processing Resolution
4K (3840x2160) → 2K (1920x1080) for most shots
2K (1920x1080) → 1080p (same) or 720p for speed
HD (1280x720) → 720p (same)
```

**2. Batch Processing:**
- Test 1 frame first
- Then test 10 frames
- Then batch full sequence
- Use render farm for large batches

**3. Caching:**
- Enable cache for repeated frames
- Use same parameters for entire sequence
- Cache saves ~50-80% processing time

**4. Model Selection:**
```
Hero shots, final delivery:
  SAM: ViT-H, Grounding DINO: SwinB

Temp/preview work:
  SAM: ViT-B, Grounding DINO: SwinT

Batch processing (hundreds of frames):
  SAM: ViT-L, Grounding DINO: SwinT
```

### Quality Control

**1. Always Check:**
- ✓ First frame before batch
- ✓ Middle frame
- ✓ Last frame
- ✓ Motion-heavy frames
- ✓ Lighting changes

**2. Quality Checklist:**
- [ ] Clean edges (no artifacts)
- [ ] Consistent across frames
- [ ] No flickering
- [ ] Correct element isolated
- [ ] No holes in matte

**3. Refinement:**
```
SAM Segmentation (80% there)
  ↓
Dilate/Erode (clean up edges)
  ↓
Blur (soften edges)
  ↓
Manual paint fixes (final 20%)
```

### Production Integration

**1. Naming Conventions:**
```
Project Name: {show}_{sequence}_{shot}
Example: starwars_seq010_shot0250

Directory Structure:
/shared/
  starwars_seq010_shot0250/
    input_0001.exr
    input_0002.exr
    OutImage_0001.exr
    OutMatte_0001.exr
```

**2. Version Control:**
```
shot0250_v001 → First pass
shot0250_v002 → After notes
shot0250_v003 → Final
```

**3. Documentation:**
- Note parameters used
- Save settings per shot
- Document any issues

**4. Archiving:**
```
After delivery:
  - Archive input EXRs
  - Keep output EXRs
  - Save project file
  - Document settings
```

### Collaboration

**1. Team Workflow:**
```
Artist A: Segments people
  ↓ (shared storage)
Artist B: Color grades
  ↓ (shared storage)
Artist C: Composites
```

**2. Server Sharing:**
- Schedule heavy processing
- Monitor queue
- Use priority system (if implemented)

**3. Communication:**
- Document issues
- Share successful settings
- Report bugs

---

## Appendix

### Keyboard Shortcuts (by Host)

**Flame:**
- Apply effect: Drag from Effects
- Render frame: Ctrl+R
- Toggle effect: Bypass button

**Nuke:**
- Create node: Tab → type "ComfyUI"
- Render range: F5
- Solo node: 1 key

**Resolve:**
- Apply effect: Drag from Effects Library
- Render: Ctrl+B (add to render queue)

### Performance Benchmarks

**SAM Segmentation (1080p, RTX 4090):**

| Configuration | First Frame | Cached |
|---------------|-------------|--------|
| ViT-H + SwinB | 8.2 sec | 2.4 sec |
| ViT-H + SwinT | 7.1 sec | 2.1 sec |
| ViT-L + SwinT | 4.8 sec | 1.3 sec |
| ViT-B + SwinT | 2.9 sec | 0.8 sec |

**Note:** First frame includes model loading. Cached means identical parameters.

### Support

**Documentation:**
- Developer Guide: `/contrib/docs/developer-guide.md`
- Plugin README: `/contrib/plugins/ComfyUI/segmentation/README.md`

**Community:**
- GitHub Issues: https://github.com/AcademySoftwareFoundation/openfx/issues
- ComfyUI Discord: https://discord.gg/comfyui

**Commercial Support:**
- Contact your facility's pipeline team
- Or ComfyUI plugin developers

---

**Last Updated:** 2025-10-09
**Version:** 1.0
**License:** BSD-3-Clause
