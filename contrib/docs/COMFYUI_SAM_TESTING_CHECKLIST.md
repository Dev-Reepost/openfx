# SAMSegmentation Plugin Testing Checklist for Flame macOS

## Installation Status

✅ **Plugin Installed**: `~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle`
✅ **Binary Size**: 1.7 MB (arm64)
✅ **Dependencies**: System libraries only (no external deps)

---

## Prerequisites

### 1. ComfyUI Server Setup

**Install ComfyUI:**

```bash
# If not already installed:
git clone https://github.com/comfyanonymous/ComfyUI.git ~/ComfyUI
cd ~/ComfyUI
python -m venv venv
source venv/bin/activate
pip install -r requirements.txt
```

**Install Segment Anything Extension:**

```bash
cd ~/ComfyUI/custom_nodes
git clone https://github.com/storyicon/comfyui_segment_anything
cd comfyui_segment_anything
pip install -r requirements.txt
```

**Download Required Models:**

Place these in `~/ComfyUI/models/`:

**Grounding DINO** (required - at least one):

- `GroundingDINO_SwinT_OGC (694MB)` - Download from HuggingFace
- URL: <https://huggingface.co/ShilongLiu/GroundingDINO/tree/main>

**SAM** (required - at least one):

- `sam_vit_h (2.56GB)` - Recommended
- `sam_vit_l (1.25GB)` - Alternative
- `sam_vit_b (375MB)` - Fastest
- URL: <https://github.com/facebookresearch/segment-anything#model-checkpoints>

**Start ComfyUI Server:**

```bash
cd ~/ComfyUI
source venv/bin/activate
python main.py --listen 0.0.0.0 --port 8188
```

**Verify Server is Running:**

```bash
curl http://localhost:8188/system_stats
# Should return JSON with system information
```

### 2. Shared Storage Setup

**Create shared storage directory structure:**

```bash
# Example using local directory (for testing)
mkdir -p /tmp/comfyui_shared/in/test_project/segmentation
mkdir -p /tmp/comfyui_shared/out/test_project/segmentation

# For production, use network storage:
# /Volumes/shared/in/project_name/workflow_name
# /Volumes/shared/out/project_name/workflow_name
```

**Set permissions:**

```bash
chmod -R 777 /tmp/comfyui_shared
```

### 3. Test Media

**Prepare test image:**

- Create a test clip in Flame with clear foreground objects
- Recommended: Person against background, car, etc.
- Resolution: 1920x1080 or similar
- Format: Any format Flame supports

---

## Testing Procedure

### Test 1: Plugin Visibility

**Steps:**

1. ✅ Launch Flame (restart if already running)
2. ✅ Open MediaHub or create new project
3. ✅ Right-click on timeline → Effects → OFX
4. ✅ Look for "ComfyUI SAM Segmentation" in the effects list

**Expected Result:**

- Plugin appears in OFX effects menu

**Troubleshooting:**

- If not visible, check: `/Library/Application Support/Autodesk/flame_*/OFXPlugin.log`
- Verify bundle path: `~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle`

### Test 2: Plugin Parameters

**Steps:**

1. ✅ Apply plugin to a clip
2. ✅ Open plugin UI/parameters panel

**Verify these parameter groups exist:**

**Segmentation Group:**

- [ ] Prompt (string, default: "foreground")
- [ ] Threshold (0.0-1.0, default: 0.3)
- [ ] SAM Model (choice: ViT-H/ViT-L/ViT-B)
- [ ] Grounding DINO Model (choice: SwinT/SwinB)
- [ ] Resolution (512-4096, default: 1080)

**Server Configuration Group:**

- [ ] Server Address (string, default: "localhost")
- [ ] Server Port (1-65535, default: 8188)

**Storage Configuration Group:**

- [ ] Shared Mount Path (string)
- [ ] Project Name (string)
- [ ] Workflow Name (string)
- [ ] Basename (string)
- [ ] Layer Name (string)
- [ ] Output Version (string)

**Output Options Group:**

- [ ] Input: Linear to sRGB (boolean, default: true)
- [ ] Output: sRGB to Linear (boolean, default: true)
- [ ] Output Alpha Matte (boolean, default: false, DISABLED)

### Test 3: Basic Segmentation

**Configure Plugin:**

```
Server Configuration:
  - Server Address: localhost
  - Server Port: 8188

Storage Configuration:
  - Shared Mount Path: /tmp/comfyui_shared
  - Project Name: test_project
  - Workflow Name: segmentation
  - Basename: shot01
  - Layer Name: beauty
  - Output Version: v001

Segmentation:
  - Prompt: "person"
  - Threshold: 0.3
  - SAM Model: ViT-H (if you have it)
  - Resolution: 1080
```

**Steps:**

1. ✅ Apply plugin to test clip
2. ✅ Configure parameters as above
3. ✅ Render a single frame
4. ✅ Monitor ComfyUI server console for activity

**Expected Files Created:**

```
Input (written by plugin):
/tmp/comfyui_shared/in/test_project/segmentation/shot01_beauty_0001_v001_.exr

Output (written by ComfyUI):
/tmp/comfyui_shared/out/test_project/segmentation/v001/image_0001.exr
/tmp/comfyui_shared/out/test_project/segmentation/OutMatte_0001.exr
```

**Verify:**

- [ ] Input EXR was created
- [ ] ComfyUI processed the request
- [ ] Output files were created
- [ ] Flame displays the result

### Test 4: Different Prompts

**Test various segmentation prompts:**

- [ ] "person" - Segment human figures
- [ ] "car" - Segment vehicles
- [ ] "foreground" - Segment main subject
- [ ] "background" - Segment background
- [ ] "sky" - Segment sky regions

**For each prompt:**

1. Change the prompt parameter
2. Render frame
3. Verify segmentation quality

### Test 5: Model Variations

**Test different SAM models:**

- [ ] ViT-H (2.56GB) - Best quality, slowest
- [ ] ViT-L (1.25GB) - Balanced
- [ ] ViT-B (375MB) - Fastest, lower quality

**Test different Grounding DINO models:**

- [ ] SwinT (694MB) - Faster
- [ ] SwinB (938MB) - More accurate

**Compare:**

- Processing time
- Segmentation quality
- Edge accuracy

### Test 6: Resolution Testing

**Test different resolutions:**

- [ ] 720 - Fast processing
- [ ] 1080 - Default
- [ ] 2160 - High quality, slower

**Note processing times for each**

### Test 7: Multi-Frame Sequence

**Steps:**

1. ✅ Apply plugin to multi-frame clip (10-20 frames)
2. ✅ Render entire sequence
3. ✅ Monitor file creation in shared storage

**Verify:**

- [ ] All frames processed
- [ ] Consistent segmentation across frames
- [ ] No frame drops or errors

### Test 8: Error Handling

**Test error conditions:**

**Server Down:**

1. Stop ComfyUI server
2. Try to render
3. Expected: Clear error message about connection failure

**Invalid Path:**

1. Set Shared Mount Path to non-existent directory
2. Try to render
3. Expected: Error about path not accessible

**Missing Models:**

1. Select a model not installed in ComfyUI
2. Try to render
3. Expected: Error about missing model

**Network Issues:**

1. Set Server Address to invalid IP
2. Try to render
3. Expected: Connection timeout error

---

## Performance Benchmarks

**Record these metrics during testing:**

| Test | Resolution | SAM Model | DINO Model | Frame Time | Notes |
|------|-----------|-----------|------------|------------|-------|
| 1 | 1080 | ViT-H | SwinT | ___s | |
| 2 | 1080 | ViT-L | SwinT | ___s | |
| 3 | 1080 | ViT-B | SwinT | ___s | |
| 4 | 720 | ViT-H | SwinT | ___s | |
| 5 | 2160 | ViT-H | SwinT | ___s | |

**Expected Performance:**

- First frame: +5-10s (model loading)
- Subsequent frames: 1-5s per frame
- Resolution scaling: ~2x time for 2x resolution

---

## Known Limitations

**Current Limitations:**

1. ⚠️ **Single output only** - Returns preprocessed image, alpha matte written but not returned
2. ⚠️ **No progress bar** - Flame won't show ComfyUI processing progress
3. ⚠️ **Frame-by-frame** - No caching between frames
4. ⚠️ **No real-time preview** - Must render to see results
5. ⚠️ **Requires shared storage** - Cannot work without file-based exchange

**Multi-Output Note:**
The workflow generates BOTH:

- `image_0001.exr` - Preprocessed result (returned to Flame)
- `OutMatte_0001.exr` - Alpha matte (written but not returned)

To access the matte, you would need to manually load it as a separate clip in Flame.

---

## Troubleshooting Guide

### Plugin Not Visible in Flame

**Check:**

```bash
# Verify installation
ls -la ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/

# Check Flame OFX log
tail -f "/Library/Application Support/Autodesk/flame_*/OFXPlugin.log"

# Verify binary architecture
file ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
```

**Solutions:**

- Restart Flame
- Check bundle permissions (should be readable)
- Verify arm64 architecture matches your Mac

### "Failed to connect to server"

**Check:**

```bash
# Verify ComfyUI is running
curl http://localhost:8188/system_stats

# Check if port is in use
lsof -i :8188

# Test from Flame machine (if remote)
curl http://192.168.1.211:8188/system_stats
```

**Solutions:**

- Start ComfyUI server
- Check firewall settings
- Verify Server Address and Port parameters

### "Failed to find output file"

**Check:**

```bash
# Verify input was written
ls -la /tmp/comfyui_shared/in/test_project/segmentation/

# Check ComfyUI output
ls -la /tmp/comfyui_shared/out/test_project/segmentation/

# Check ComfyUI console for errors
```

**Solutions:**

- Verify shared storage path is accessible from Flame
- Check ComfyUI has write permissions
- Verify workflow completed successfully in ComfyUI console

### "Model not found"

**Check:**

```bash
# List installed models
ls -la ~/ComfyUI/models/grounding-dino/
ls -la ~/ComfyUI/models/sam/

# Verify model names match exactly
```

**Solutions:**

- Download missing models
- Check model filename matches exactly
- Restart ComfyUI after adding models

### Slow Performance

**Optimization steps:**

1. Use smaller SAM model (ViT-B instead of ViT-H)
2. Reduce resolution (720 instead of 1080)
3. Use SwinT DINO model (faster than SwinB)
4. Ensure shared storage is local or fast network
5. Check ComfyUI is using GPU (check console output)

---

## Success Criteria

The plugin is ready for production if:

- ✅ Plugin appears in Flame OFX menu
- ✅ All parameters are accessible and functional
- ✅ Can successfully segment objects with text prompts
- ✅ Processes multi-frame sequences without errors
- ✅ Error messages are clear and actionable
- ✅ Performance is acceptable for production use
- ✅ Results are consistent across frames

---

## Next Steps After Testing

### If Successful

1. Document production workflow
2. Train artists on usage
3. Set up production shared storage
4. Configure production ComfyUI server
5. Create project templates with common settings

### If Issues Found

1. Document all errors encountered
2. Check ComfyUI console logs
3. Review [comfyui_base_plugin.cpp](../common/comfyui_base_plugin.cpp) render logic
4. Review [sam_segmentation_plugin.cpp](sam_segmentation_plugin.cpp) workflow generation
5. Report issues with full error logs

---

## Additional Resources

**Documentation:**

- [README.md](README.md) - Full plugin documentation
- [ComfyUI BasePlugin](../common/comfyui_base_plugin.h) - Base class documentation
- [OpenFX Specification](http://openeffects.org/) - OFX API reference

**ComfyUI:**

- [ComfyUI GitHub](https://github.com/comfyanonymous/ComfyUI)
- [Segment Anything Extension](https://github.com/storyicon/comfyui_segment_anything)
- [Reference Implementation](https://github.com/Dev-Reepost/flame_comfyui_segmentation)

**Model Downloads:**

- [Grounding DINO Models](https://huggingface.co/ShilongLiu/GroundingDINO)
- [SAM Models](https://github.com/facebookresearch/segment-anything#model-checkpoints)

---

**Testing Date:** _________________
**Tester Name:** _________________
**Flame Version:** _________________
**macOS Version:** _________________
**Result:** ⬜ Pass  ⬜ Fail  ⬜ Partial
