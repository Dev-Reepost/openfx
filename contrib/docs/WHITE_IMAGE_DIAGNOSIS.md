# White Image Diagnosis & Fix

**Date:** 2025-11-07 14:23
**Issue:** EXR files written by plugin contain all-white pixels [1, 1, 1, 1]

## Problem Identified

The EXR validation test confirmed that the file written by the plugin contains:
- **Average pixel value:** 1.0
- **Maximum pixel value:** 1.0
- **Minimum pixel value:** 1.0
- **First pixel:** [1, 1, 1, 1]
- **Middle pixel:** [1, 1, 1, 1]

This means **ALL pixels are pure white**.

## Root Cause Analysis

The EXR I/O code is working correctly (verified by standalone tests). The problem is that **Flame is providing all-white pixel data** to the plugin.

### Possible Causes

1. **Source clip not connected in Flame**
   - User may not have connected an input to the plugin
   - Plugin is getting a default white buffer

2. **Timing issue**
   - Plugin may be requesting the frame before Flame has rendered it
   - OFX host may provide default buffer when frame not ready

3. **Wrong clip type**
   - Source clip may be set up incorrectly
   - Plugin may be reading from wrong context

## Fix Applied

Added comprehensive validation and error checking in `comfyui_base_plugin.cpp`:

### 1. Source Clip Connection Check (Line 206-209)
```cpp
if (!_srcClip->isConnected()) {
    if (_logger) _logger->error("ERROR: Source clip is not connected!");
    throw std::runtime_error("Source clip is not connected. Please connect an input to the plugin.");
}
```

### 2. Fetch Validation (Line 211-218)
```cpp
std::unique_ptr<OFX::Image> src(_srcClip->fetchImage(args.time));

if (!src.get()) {
    if (_logger) _logger->error("ERROR: Failed to fetch source image!");
    throw std::runtime_error("Failed to fetch source image from clip.");
}

if (_logger) _logger->info("Source image fetched successfully");
```

### 3. White/Black Image Detection (Line 438-452)
```cpp
// Check if image appears to be blank/white
float sum = 0.0f;
int sampleCount = std::min(1000, static_cast<int>(imageData.pixels.size() / pixelComponents));
for (int i = 0; i < sampleCount; ++i) {
    sum += imageData.pixels[i * pixelComponents + 0]; // Sample R channel
}
float avg = sum / sampleCount;
_logger->info("Average pixel value (sampled): {:.4f}", avg);

if (avg > 0.95f) {
    _logger->warn("WARNING: Input image appears to be mostly white! Check source connection in host.");
} else if (avg < 0.05f) {
    _logger->warn("WARNING: Input image appears to be mostly black! Check source connection in host.");
}
```

## Testing Instructions

### 1. Run Standalone EXR Validation Test

Test the EXR I/O independently (no Flame required):

```bash
# Run all tests
./build/Release/contrib/plugins/ComfyUI/tests/test_exr_validation

# Test specific file
./build/Release/contrib/plugins/ComfyUI/tests/test_exr_validation /path/to/file.exr
```

Expected output:
- ✅ Creates realistic 1920x1080 test image
- ✅ Validates pixel statistics
- ✅ Verifies round-trip conversion

### 2. Test Plugin in Flame

**Setup:**
1. Launch Flame/Flare
2. **IMPORTANT:** Make sure you have a **valid video clip** in your timeline
3. Apply the SAMSegmentation plugin to the clip
4. Configure plugin parameters:
   - Server: `192.168.1.110:8188`
   - Client Mount Path: `/Volumes/silo2/002_COMFYUI`
   - Server Mount Point: `Z:\`
   - Project: `TEST_SAM`
   - Workflow Name: `segmentation`

**What to Check:**
1. Check if plugin throws error about source clip not connected
2. Check log file for warnings about white/black image
3. Check log file for "First pixel after conversion" values

**Expected Log Output:**
```
[INFO] Source image fetched successfully
[INFO] First pixel after conversion (RGBA): [0.xxxx, 0.xxxx, 0.xxxx, 1.0000]
[INFO] Average pixel value (sampled): 0.xxxx
```

**If you see white pixels:**
```
[WARN] WARNING: Input image appears to be mostly white! Check source connection in host.
```

## Next Steps Based on Test Results

### If "Source clip is not connected" error appears:
- **Solution:** Make sure the plugin is applied to a valid clip in Flame
- The plugin needs to be in the timeline with actual footage feeding it

### If image is still all white:
- Check Flame/Flare setup - ensure plugin is receiving actual rendered frames
- May need to investigate OFX context settings
- May need to look at how the plugin is being inserted into Flame's node graph

### If image has proper pixel values:
- Proceed with testing ComfyUI workflow execution
- Verify path conversion and server communication

## Files Modified

- `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp` - Added validation
- `contrib/plugins/ComfyUI/tests/test_exr_validation.cpp` - New comprehensive test
- `contrib/plugins/ComfyUI/tests/CMakeLists.txt` - Added test target

## Plugin Status

**Version:** Built 2025-11-07 14:23
**Location:** `~/Library/OFX/Plugins/SAMSegmentation.ofx.bundle`
**Status:** ✅ Ready for testing with enhanced validation and logging
