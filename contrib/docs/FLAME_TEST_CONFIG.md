# Flame Test Configuration for SAM Segmentation Plugin

## Your Environment

✅ **ComfyUI Server**: `192.168.1.211:8188` (Windows, CUDA, RTX 4060 Ti)
✅ **Shared Storage**: `/Volumes/silo2/002_COMFYUI` (SMB mount)
✅ **Test Project**: `TEST_SAM` (created)

## Exact Parameters to Use in Flame

### Server Configuration Group
```
Server Address: 192.168.1.211
Server Port: 8188
```

### Storage Configuration Group
```
Shared Mount Path: /Volumes/silo2/002_COMFYUI
Project Name: TEST_SAM
Workflow Name: segmentation
Basename: shot01
Layer Name: beauty
Output Version: v001
```

### Segmentation Group (start with defaults)
```
Prompt: person
Threshold: 0.3
SAM Model: SAM ViT-H (2.56GB) - Highest quality
Grounding DINO Model: SwinT (694MB) - Faster
Resolution: 1080
```

### Output Options Group
```
Input: Linear to sRGB: ✓ (checked)
Output: sRGB to Linear: ✓ (checked)
Output Alpha Matte: ☐ (unchecked - disabled anyway)
```

## Expected File Paths

When you render frame 1, the plugin will:

**1. Write input:**
```
/Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr
```

**2. ComfyUI generates:**
```
/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/image_0001.exr
/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/OutMatte_0001.exr
```

**3. Plugin reads back:**
```
/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/image_0001.exr
```

## Network Path Mapping

**Important**: The ComfyUI server (Windows) needs to access the same shared storage.

**Your Mac sees:**
```
/Volumes/silo2/002_COMFYUI/
```

**Windows server must see the same path as:**
```
\\silo2\002_COMFYUI\
or
Z:\002_COMFYUI\
```

Make sure the Windows ComfyUI server has this path mounted and accessible!

## Pre-Flight Checklist

Before testing in Flame:

- [x] ComfyUI server running (verified: 192.168.1.211:8188)
- [x] Shared storage accessible from Mac (verified: /Volumes/silo2/002_COMFYUI)
- [ ] Shared storage accessible from Windows ComfyUI server (verify with: dir \\\\silo2\\002_COMFYUI)
- [ ] SAM models installed on ComfyUI server
- [ ] Grounding DINO models installed on ComfyUI server
- [ ] Segment Anything extension installed on ComfyUI server

## Verify Windows Server Access

On your Windows ComfyUI server, verify it can access the shared storage:

```powershell
# Check if path is accessible
dir \\silo2\002_COMFYUI\in
dir \\silo2\002_COMFYUI\out

# Or if mapped as drive letter:
dir Z:\002_COMFYUI\in
dir Z:\002_COMFYUI\out
```

## Common Issue: Path Mapping

The plugin uses **absolute paths** like:
```
/Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr
```

But ComfyUI on Windows needs to access the same file. You have two options:

### Option A: UNC Path (Recommended)
Configure ComfyUI to understand Unix-style paths by mapping them:
- Mac: `/Volumes/silo2` → Windows: `\\silo2`

### Option B: Drive Letter
Map the share to a drive letter on Windows:
```powershell
net use Z: \\silo2\002_COMFYUI /persistent:yes
```

Then modify the plugin to use `Z:\` instead of `/Volumes/silo2/002_COMFYUI/`

## Test Procedure

1. **Start ComfyUI on Windows** (if not already running)

2. **In Flame**:
   - Create a new timeline with a test clip (any footage)
   - Apply "ComfyUI SAM Segmentation" effect to the clip
   - Fill in all parameters exactly as shown above
   - Render a single frame

3. **Expected behavior**:
   - Flame shows "Processing..." or progress
   - Input EXR appears in `/Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/`
   - ComfyUI server console shows workflow execution
   - Output EXR appears in `/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/`
   - Flame displays the result

4. **If it fails**:
   - Check files were written: `ls /Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/`
   - Check ComfyUI server console for errors
   - Check files were created: `ls /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/`

## Debugging Commands (Mac Terminal)

**Monitor input directory:**
```bash
watch -n 1 "ls -lht /Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/ | head -5"
```

**Monitor output directory:**
```bash
watch -n 1 "ls -lht /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/ | head -5"
```

**Test network connectivity:**
```bash
# Verify ComfyUI server is reachable
curl -s http://192.168.1.211:8188/system_stats | python3 -m json.tool

# Test WebSocket (should show "426 Upgrade Required")
curl -I http://192.168.1.211:8188/ws
```

## Workflow JSON Being Sent

The plugin will send this JSON to ComfyUI:

```json
{
  "prompt": {
    "1": {
      "inputs": {
        "filepath": "/Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/shot01_beauty_0001_v001_.exr",
        "linear_to_sRGB": "true",
        "image_load_cap": 0,
        "skip_first_images": 0,
        "select_every_nth": 1
      },
      "class_type": "LoadEXR"
    },
    "16": {
      "inputs": {
        "prompt": "person",
        "threshold": 0.3,
        "sam_model": ["18", 0],
        "grounding_dino_model": ["17", 0],
        "image": ["1", 0]
      },
      "class_type": "GroundingDinoSAMSegment (segment anything)"
    },
    "17": {
      "inputs": {
        "model_name": "GroundingDINO_SwinT_OGC (694MB)"
      },
      "class_type": "GroundingDinoModelLoader (segment anything)"
    },
    "18": {
      "inputs": {
        "model_name": "sam_vit_h (2.56GB)"
      },
      "class_type": "SAMModelLoader (segment anything)"
    },
    "20": {
      "inputs": {
        "mask": ["16", 1]
      },
      "class_type": "MaskToImage"
    },
    "23": {
      "inputs": {
        "filename_prefix": "OutMatte",
        "sRGB_to_linear": "true",
        "version": 1,
        "start_frame": 1,
        "frame_pad": 4,
        "images": ["20", 0]
      },
      "class_type": "SaveEXR"
    },
    "24": {
      "inputs": {
        "resolution": 1080,
        "image": ["1", 0]
      },
      "class_type": "SAMPreprocessor"
    },
    "27": {
      "inputs": {
        "filename_prefix": "/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/image",
        "sRGB_to_linear": "true",
        "version": 2,
        "start_frame": 1,
        "frame_pad": 4,
        "images": ["24", 0]
      },
      "class_type": "SaveEXR"
    }
  },
  "client_id": "<generated-uuid>"
}
```

**Note**: The filepath contains `/Volumes/silo2/...` which is a Mac path. ComfyUI on Windows needs to handle this. Verify your LoadEXR node can handle cross-platform paths.

## Success Criteria

✅ Plugin doesn't crash ("Plugin rendering failed")
✅ Input EXR is written to shared storage
✅ ComfyUI processes the workflow without errors
✅ Output EXR is created
✅ Flame displays the segmented result

## Next Steps If Successful

1. Test with different prompts ("car", "building", "foreground")
2. Test with different resolutions (720, 1080, 2160)
3. Try different SAM models (ViT-L, ViT-B for speed)
4. Process a short sequence (10-20 frames)
5. Optimize performance settings
