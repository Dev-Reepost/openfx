# Alpha Matte Integration for SAM Segmentation Plugin

**Date:** 2025-11-07
**Status:** Implementation Plan

---

## Problem Statement

The SAM Segmentation workflow generates two outputs:
1. **RGB Image** (node 24: SAMPreprocessor output) - The processed image
2. **Alpha Matte** (node 20: MaskToImage output) - Black/white mask representing segmentation

Currently, the plugin only returns the RGB image. The matte is saved separately as `OutMatte` but not accessible in the OFX plugin output.

---

## OFX Multi-Output Investigation

**Question:** Does OFX support multiple output clips per plugin?

**Answer:** **No, OFX only supports ONE output clip per effect.**

From OFX specification and examples:
- Filter context: 1 output clip ("Output")
- All example plugins define single output clip
- No mechanism for multiple named output clips

**This is a fundamental OFX limitation, not a host-specific restriction.**

---

## Solution: RGBA with Embedded Alpha Channel

### Approach: Combine RGB + Matte into Single RGBA EXR

**Benefits:**
- ✅ Standard VFX workflow (RGBA is industry standard)
- ✅ Single file output (simpler pipeline)
- ✅ OFX natively supports RGBA
- ✅ EXR format supports RGBA perfectly
- ✅ Autodesk Flare fully supports RGBA EXR
- ✅ No workflow changes needed in Flare

**Implementation:**
Use ComfyUI's built-in `JoinImageWithAlpha` node to combine:
- **Image input:** Node 24 (SAMPreprocessor RGB output)
- **Alpha input:** Node 20 (MaskToImage matte)
- **Output:** RGBA image ready for SaveEXR

---

## Updated Workflow Structure

### Current Workflow (2 outputs):
```
LoadEXR (1) → SAMPreprocessor (24) → SaveEXR (27) [RGB only]
            ↓
GroundingDINO+SAM (16) → MaskToImage (20) → SaveEXR (23) [Matte only]
```

### New Workflow (1 RGBA output):
```
LoadEXR (1) → SAMPreprocessor (24) ─┐
            ↓                         ├→ JoinImageWithAlpha (NEW) → SaveEXR (27) [RGBA]
GroundingDINO+SAM (16) → MaskToImage (20) ─┘
```

**Changes:**
1. Add `JoinImageWithAlpha` node between processing and SaveEXR
2. Connect RGB from node 24 to image input
3. Connect matte from node 20 to alpha input
4. Remove separate OutMatte SaveEXR (node 23) - no longer needed
5. Update SaveEXR (node 27) to save RGBA

---

## ComfyUI Node: JoinImageWithAlpha

**Class Type:** `JoinImageWithAlpha` (built-in ComfyUI core)

**Inputs:**
- `image`: RGB image (3 channels)
- `alpha`: Alpha mask (1 channel, grayscale)

**Output:**
- `IMAGE`: RGBA image (4 channels)

**Usage in Workflow:**
```json
{
  "25": {
    "inputs": {
      "image": ["24", 0],   // RGB from SAMPreprocessor
      "alpha": ["20", 0]    // Matte from MaskToImage
    },
    "class_type": "JoinImageWithAlpha"
  }
}
```

---

## Implementation Changes Required

### File: `sam_segmentation_plugin.cpp`

**1. Remove Node 23 (OutMatte SaveEXR):**
```cpp
// DELETE THIS:
{"23", {
    {"inputs", {
        {"filename_prefix", "OutMatte"},
        ...
    }},
    {"class_type", "SaveEXR"}
}}
```

**2. Add Node 25 (JoinImageWithAlpha):**
```cpp
// ADD THIS:
{"25", {
    {"inputs", {
        {"image", json::array({"24", 0})},  // RGB from SAMPreprocessor
        {"alpha", json::array({"20", 0})}   // Matte from MaskToImage
    }},
    {"class_type", "JoinImageWithAlpha"}
}}
```

**3. Update Node 27 (SaveEXR) to use RGBA:**
```cpp
// CHANGE FROM:
{"images", json::array({"24", 0})}  // RGB only

// CHANGE TO:
{"images", json::array({"25", 0})}  // RGBA with embedded alpha
```

---

## Plugin Code Changes

### No Changes Needed in Image I/O!

**Good news:** `comfyui_image_io.cpp` already supports RGBA:

```cpp
struct ImageData {
    int width, height, channels;  // channels can be 4 for RGBA
    std::vector<float> pixels;    // Already stores RGBA interleaved
};
```

**TinyEXR supports:**
- Reading RGBA EXR files
- Writing RGBA EXR files
- Channel de-interleaving/interleaving

**OFX Buffer:**
- `PixelComponentEnum` has `ePixelComponentRGBA`
- `toOFXBuffer()` and `fromOFXBuffer()` already handle RGBA

**No image I/O changes required!**

---

## Validation Steps

### 1. Verify JoinImageWithAlpha Available

**Check if node exists on your ComfyUI server:**
```bash
curl http://192.168.1.211:8188/object_info | jq '.JoinImageWithAlpha'
```

**Expected:** Node definition with `image` and `alpha` inputs

**If not available:** Update ComfyUI to latest version (node is built-in since early versions)

### 2. Test Workflow Manually

**Before implementing in plugin, test in ComfyUI UI:**
1. Load test image
2. Run SAM segmentation
3. Use JoinImageWithAlpha to combine
4. Save as EXR
5. Open in image viewer and verify:
   - RGB channels show processed image
   - Alpha channel shows mask (white = foreground, black = background)

### 3. Test in Plugin

**After implementation:**
1. Rebuild plugin
2. Render frame in Flare
3. Check output EXR has 4 channels (RGBA)
4. Verify alpha channel contains mask
5. Test compositing in Flare using alpha

---

## Benefits of This Approach

### Technical Benefits

1. **Single File Output**
   - Easier file management
   - Atomic operations (one file = one complete result)
   - Simpler caching logic

2. **Standard VFX Format**
   - RGBA EXR is industry standard
   - All compositing tools expect this format
   - Premultiplication options available

3. **OFX Native Support**
   - No workarounds needed
   - Fully supported pixel format
   - Automatic alpha handling in Flare

4. **No Code Changes to I/O**
   - TinyEXR already handles RGBA
   - OFX buffer conversion already works
   - Plugin infrastructure ready

### Workflow Benefits

1. **Simpler for Artists**
   - One output instead of two
   - Alpha channel automatically available
   - Standard compositing workflow

2. **Better Performance**
   - One file read/write instead of two
   - Less disk I/O
   - Faster caching

3. **Easier Debugging**
   - Single output to validate
   - Standard tools can view RGBA EXR
   - Clear success/failure

---

## Alternative Approaches (NOT RECOMMENDED)

### Alternative 1: Separate Matte Plugin

**Concept:** Create second plugin "SAMSegmentationMatte" that outputs only matte

**Problems:**
- ❌ Two plugins to maintain
- ❌ Users must run both plugins
- ❌ Synchronization issues (which matte matches which RGB?)
- ❌ Double ComfyUI server calls
- ❌ Cache invalidation complexity

**Verdict:** Much more complex, no real benefits

### Alternative 2: Parameter to Choose Output

**Concept:** Add "Output Type" parameter: RGB | Matte | Both (separate files)

**Problems:**
- ❌ "Both" still requires two files
- ❌ User confusion about which mode to use
- ❌ More complex workflow logic
- ❌ Doesn't solve the fundamental single-output limitation

**Verdict:** Adds complexity without solving core issue

### Alternative 3: Custom Multi-Channel EXR

**Concept:** Use EXR's arbitrary channel naming (Beauty.R, Beauty.G, Matte.Y, etc.)

**Problems:**
- ❌ Non-standard format
- ❌ Flare may not recognize custom channels
- ❌ More complex I/O code
- ❌ Harder for artists to use

**Verdict:** Over-engineered for simple RGBA solution

---

## Recommended Solution: RGBA with JoinImageWithAlpha

**This is the BEST approach because:**

1. ✅ **Standard:** Industry-standard RGBA format
2. ✅ **Simple:** One node change in workflow
3. ✅ **No Code Changes:** Image I/O already supports RGBA
4. ✅ **OFX Native:** Fully supported pixel format
5. ✅ **Artist-Friendly:** Standard compositing workflow
6. ✅ **Proven:** Used throughout VFX industry

---

## Implementation Checklist

- [ ] Verify `JoinImageWithAlpha` available on ComfyUI server
- [ ] Update `sam_segmentation_plugin.cpp` workflow:
  - [ ] Remove node 23 (OutMatte SaveEXR)
  - [ ] Add node 25 (JoinImageWithAlpha)
  - [ ] Update node 27 to use node 25 output
- [ ] Test workflow manually in ComfyUI UI
- [ ] Rebuild plugin
- [ ] Test in Flare with real footage
- [ ] Verify RGBA output in image viewer
- [ ] Test alpha channel compositing in Flare
- [ ] Update documentation

---

## Expected Result

**After implementation:**

**Single EXR output with:**
- **R, G, B channels:** Processed SAM segmentation image
- **Alpha channel:** Binary mask (1.0 = foreground, 0.0 = background)

**Usage in Flare:**
- Import EXR
- Alpha channel automatically recognized
- Use for compositing, keying, or masking
- Standard Flare alpha operations apply

**File Example:**
```
/Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/image_v002.0056.exr
  - Width: 1920
  - Height: 1080
  - Channels: 4 (RGBA)
  - Format: Float32
  - Compression: ZIP (lossless)
```

---

**Status:** Ready for implementation
**Complexity:** Low (single workflow change)
**Risk:** Very low (standard approach)
**Testing Required:** Moderate (verify alpha channel correct)

