# ComfyUI Plugin Test Checklist

## Plugin Status
- **Version**: Built 2025-11-07 13:18:10
- **Location**: ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle
- **Key Fix**: Removed duplicate "prompt" wrapper in workflow JSON

## Critical Fixes Applied

### 1. Workflow JSON Structure (FIXED)
**Before (Broken)**:
```json
{
  "prompt": {
    "prompt": {
      "1": {...},
      "16": {...}
    }
  }
}
```

**After (Fixed)**:
```json
{
  "prompt": {
    "1": {...},
    "16": {...}
  },
  "client_id": "..."
}
```

### 2. Dual-Path Mapping (IMPLEMENTED)
- **Client Mount Path**: `/Volumes/silo2/002_COMFYUI` (Mac Flame)
- **Server Mount Point**: `Z:` (Windows ComfyUI)
- **Conversion**: `/Volumes/silo2/002_COMFYUI/in/TEST_SAM/...` → `Z:\in\TEST_SAM\...`

### 3. Comprehensive Logging (ADDED)
- Log files: `~/comfyui_plugin_YYYYMMDD_HHMMSS.log`
- All parameters, paths, and workflow JSON logged

## Required Configuration in Flame

### Server Settings
```
Server Address:       192.168.1.211
Port:                 8188
```

### Storage Settings
```
Client Mount Path:    /Volumes/silo2/002_COMFYUI
Server Mount Point:   Z:
```

### Project Organization
```
Flame Project:        TEST_SAM
Workflow Name:        segmentation
Basename:             shot01
Layer Name:           beauty
Output Version:       v001
```

## Pre-Test Verification

### 1. Check ComfyUI Server
```bash
curl http://192.168.1.211:8188/system_stats
```
Expected: Server responds with system stats JSON

### 2. Verify Network Storage
```bash
# On Mac (Flame side)
ls -la /Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation/
ls -la /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/

# On Windows (ComfyUI server)
dir Z:\in\TEST_SAM\segmentation\
dir Z:\out\TEST_SAM\segmentation\v001\
```

### 3. Create Required Directories
```bash
# On Mac
mkdir -p /Volumes/silo2/002_COMFYUI/in/TEST_SAM/segmentation
mkdir -p /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001

# On Windows
mkdir Z:\in\TEST_SAM\segmentation
mkdir Z:\out\TEST_SAM\segmentation\v001
```

### 4. Verify ComfyUI Models
On ComfyUI server, check these models exist:
- `sam_vit_h (2.56GB)` in `models/sam/`
- `GroundingDINO_SwinT_OGC (694MB)` in `models/grounding-dino/`

## Test Procedure

### Step 1: Restart Flame
```bash
# Kill any running Flame processes
killall flame 2>/dev/null

# Start Flame fresh
# (Use your normal Flame startup method)
```

### Step 2: Load Plugin
1. Open Flame
2. Go to Batch
3. Add SAMSegmentation effect to a clip
4. Configure parameters (see above)

### Step 3: Test Render
1. Click render on a single frame
2. Watch Flame progress bar
3. Monitor log file in real-time:
   ```bash
   tail -f ~/comfyui_plugin_*.log
   ```

### Step 4: Verify Success
Check log file for these key indicators:

**✓ Workflow JSON correct structure**:
```
[info] Workflow JSON content:
{
  "1": {
    "class_type": "LoadEXR",
    ...
  },
  "16": {
    "class_type": "GroundingDinoSAMSegment (segment anything)",
    ...
  }
}
```
(Should NOT have `"prompt": {"prompt": {...}}`)

**✓ Path conversion working**:
```
[info] Converting path for ComfyUI: /Volumes/silo2/002_COMFYUI/in/...
[info] Converted path: Z:\in\...
```

**✓ Workflow accepted by ComfyUI**:
```
[info] Workflow queued with prompt ID: <uuid>
[info] Executing node: 1
[info] Executing node: 16
...
[info] Workflow execution completed
```

**✓ Output found and loaded**:
```
[info] Output path found: /Volumes/silo2/002_COMFYUI/out/.../image_0000.exr
[info] EXR file read: 1920x1080 pixels, 4 channels
[info] RENDER COMPLETED SUCCESSFULLY
```

## Troubleshooting

### Error: "Node ID '#prompt'"
**Cause**: Old plugin version still loaded
**Fix**: 
```bash
# Verify plugin timestamp
ls -lh ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle/Contents/MacOS/SAMSegmentation.ofx
# Should be: Nov  7 13:18

# If older, rebuild:
cd /Users/julien/src/openfx
cmake --build build/Release --config Release --target SAMSegmentation --parallel
rm -rf ~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle
cp -r build/Release/Release/SAMSegmentation.ofx.bundle ~/Library/OFX/Plugins/

# Restart Flame
```

### Error: "Path doesn't start with client mount"
**Cause**: Client Mount Path parameter doesn't match actual file paths
**Fix**: Verify and update Client Mount Path parameter in Flame

### Error: "ComfyUI server returned error: 400"
**Cause**: ComfyUI server rejecting workflow
**Fix**: Check ComfyUI console for detailed error message

### Error: "Failed to find output file in ComfyUI history"
**Cause**: ComfyUI workflow didn't produce output files
**Fix**: Check ComfyUI server logs and output directory permissions

## Success Criteria

A successful render will:
1. ✓ Accept workflow without JSON validation errors
2. ✓ Convert paths correctly (Mac → Windows)
3. ✓ Execute all workflow nodes on ComfyUI server
4. ✓ Generate output EXR file
5. ✓ Load result back into Flame
6. ✓ Complete without plugin errors

## Log File Location
```bash
# Find latest log
ls -lht ~/comfyui_plugin_*.log | head -1

# View latest log
less ~/comfyui_plugin_*.log

# Follow log in real-time
tail -f ~/comfyui_plugin_*.log

# Search for errors
grep -i error ~/comfyui_plugin_*.log
```

## Next Steps After Success

Once the test succeeds:
1. Test with multiple frames
2. Test different parameter values (threshold, resolution, prompt)
3. Verify matte output (node 23)
4. Performance testing with sequences
5. Clean up old log files

---

**Plugin Version**: 2025-11-07 13:18:10
**Status**: Ready for testing with corrected workflow JSON structure
