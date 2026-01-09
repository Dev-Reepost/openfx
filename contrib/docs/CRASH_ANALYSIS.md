# DaVinci Resolve Crash Analysis - SAMSegmentation Plugin

## Summary

DaVinci Resolve Studio v20.2.3.0006 crashed when attempting to display rendered frames from the SAMSegmentation ComfyUI plugin. The crash occurred **after** successful workflow submission and rendering, specifically when copying the result image back to the OFX buffer.

## Crash Details

**Date:** December 9, 2025
**Time:** 14:33:49
**Application:** DaVinci Resolve Studio v20.2.3.0006 (macOS/Clang arm64)
**System:** Mac Studio with M2 Max, 96GB RAM, macOS 26.1

### Stack Trace

```
==========[CRASH DUMP]==========
#TIME Tue Dec  9 14:33:49 2025 - Uptime 00:03:00 (hh:mm:ss)

0   Resolve                             0x000000010273a3ec
1   Resolve                             0x00000001027397b8
2   libsystem_platform.dylib            0x00000001883f3744 _sigtramp + 56
3   ???                                 0x0000000000000004 0x0 + 4
4   SAMSegmentation.ofx                 0x0000000169425694 ComfyUI::BasePlugin::executeWorkflow + 2248
5   SAMSegmentation.ofx                 0x0000000169424290 ComfyUI::BasePlugin::render + 2116
6   SAMSegmentation.ofx                 0x000000016946f568 OFX::Private::renderAction + 756
7   SAMSegmentation.ofx                 0x000000016946d8b4 OFX::Private::mainEntryStr + 1604
```

### Key Indicators

1. **Crash Location:** `ComfyUI::BasePlugin::executeWorkflow() + 2248`
   - Maps to line 873-874 in `comfyui_base_plugin.cpp`
   - Call to `ImageIO::toOFXBuffer()`

2. **Signal:** SIGSEGV (Segmentation Fault)
   - Indicates invalid memory access
   - NULL pointer dereference or buffer overflow

3. **Warning Before Crash:**
   ```
   0x1f4db6080 | UI | WARN | 2025-12-09 14:33:43,911 | CC thumbnail buffer: TIME out
   ```

## Root Cause Analysis

### The Problem

The crash occurs in `executeWorkflow()` when trying to copy rendered image data back to the OFX destination buffer:

```cpp
// comfyui_base_plugin.cpp:873-874
ImageIO::toOFXBuffer(resultImage, dst->getPixelData(),
                    dstRowBytes, dstPixelComponents, dstBitDepthInt);
```

The function `toOFXBuffer()` attempts to write pixel data to `dst->getPixelData()` without verifying that this pointer is valid.

### What Went Wrong

1. **Workflow Execution Succeeded:**
   - Input image was written ✓
   - Workflow was submitted to ComfyUI ✓
   - Rendering completed successfully ✓
   - Output EXR file was generated ✓

2. **Image Reading Succeeded:**
   - EXR file was read successfully ✓
   - Size validation passed (dimensions matched) ✓

3. **Buffer Allocation Failed:**
   - `dst->getPixelData()` returned NULL or invalid pointer ✗
   - No validation check before dereferencing ✗

### Why the Buffer Was Invalid

Based on the "CC thumbnail buffer: TIME out" warning, Resolve was likely:

1. Requesting a **thumbnail/preview render** at a non-standard resolution
2. The plugin's buffer allocation failed or timed out
3. `fetchImage()` succeeded (returned non-NULL object) but `getPixelData()` returned NULL
4. The code didn't validate the pixel data pointer before use

### Code Analysis

**Missing Validation:**

```cpp
// Line 835-840: Checks if dst object exists
std::unique_ptr<OFX::Image> dst(_dstClip->fetchImage(args.time));
if (!dst.get()) {
    throw std::runtime_error("Failed to fetch destination image");
}

// Line 863-870: Checks size mismatch
if (resultImage.width != dstWidth || resultImage.height != dstHeight) {
    throw std::runtime_error("Output image size mismatch");
}

// Line 873: NO CHECK FOR NULL PIXEL DATA ❌
ImageIO::toOFXBuffer(resultImage, dst->getPixelData(), ...);
```

**Vulnerable Code in toOFXBuffer:**

```cpp
// comfyui_image_io.cpp:228-231
void toOFXBuffer(..., void* dstPixels, ...) {
    uint8_t* dst8 = static_cast<uint8_t*>(dstPixels);  // No NULL check!

    for (int y = 0; y < image.height; ++y) {
        uint8_t* dstRow = dst8 + y * rowBytes;  // CRASH if dst8 is NULL
```

## Plugin Version Context

**Important:** This crash occurred with the **pre-fix version** of the plugin that had:
- Hardcoded 1920x1080 resolution (not source-adaptive)
- No proper RoD (Region of Definition) implementation
- Limited buffer validation

The current version has been updated to:
- Use source-adaptive resolution via `getRegionOfDefinition()`
- Get RoD from source clip: `rod = _srcClip->getRegionOfDefinition(args.time)`

However, the **NULL pointer validation is still missing** in both versions.

## Resolution Strategy

### Immediate Fix (Critical)

Add NULL pointer validation before calling `toOFXBuffer()`:

```cpp
// After line 840 in comfyui_base_plugin.cpp
void* pixelData = dst->getPixelData();
if (!pixelData) {
    if (_logger) _logger->error("Destination pixel data is NULL!");
    throw std::runtime_error("Failed to allocate destination image buffer. "
                            "This may be caused by unsupported image format or resolution.");
}

if (_logger) _logger->info("Destination buffer pointer: {}", pixelData);
```

### Additional Validations (Recommended)

1. **Validate Buffer Size:**
   ```cpp
   size_t expectedBufferSize = dstHeight * dstRowBytes;
   size_t requiredSize = resultImage.width * resultImage.height *
                        dstPixelComponents * (dstBitDepthInt / 8);

   if (expectedBufferSize < requiredSize) {
       throw std::runtime_error("Destination buffer too small");
   }
   ```

2. **Check Render Window:**
   ```cpp
   OfxRectI renderWindow = args.renderWindow;
   if (renderWindow.x1 < 0 || renderWindow.y1 < 0 ||
       renderWindow.x2 > dstBounds.x2 || renderWindow.y2 > dstBounds.y2) {
       if (_logger) _logger->warn("Render window out of bounds");
   }
   ```

3. **Validate Pixel Format Compatibility:**
   ```cpp
   OFX::PixelComponentEnum dstComponents = dst->getPixelComponents();
   if (dstComponents != OFX::ePixelComponentRGBA &&
       dstComponents != OFX::ePixelComponentRGB) {
       throw std::runtime_error("Unsupported pixel format: " +
                               std::to_string(dstComponents));
   }
   ```

### Defensive Programming

Update `toOFXBuffer()` to validate inputs:

```cpp
void toOFXBuffer(
    const ImageData& image,
    void* dstPixels,
    int rowBytes,
    int pixelComponents,
    int bitDepth
) {
    // Add parameter validation
    if (!dstPixels) {
        throw std::invalid_argument("toOFXBuffer: dstPixels is NULL");
    }

    if (image.pixels.empty()) {
        throw std::invalid_argument("toOFXBuffer: source image is empty");
    }

    if (rowBytes < image.width * pixelComponents * (bitDepth / 8)) {
        throw std::invalid_argument("toOFXBuffer: rowBytes too small for image width");
    }

    // ... rest of function
}
```

## Testing Recommendations

1. **Test with thumbnails:** Enable Color Page thumbnails and scrub timeline
2. **Test with multiple resolutions:** 1080p, 4K, 8K, proxy modes
3. **Test with different bit depths:** 8-bit, 16-bit, 32-bit float
4. **Test with different pixel formats:** RGB, RGBA, YUV
5. **Monitor Resolve logs:** Check for warnings about buffer allocation
6. **Test timeline scrubbing:** Rapid frame changes during render
7. **Test with cache enabled/disabled:** Both cached and fresh renders

## Prevention Measures

1. **Always validate pointers before dereferencing**
2. **Always check buffer sizes before memory operations**
3. **Add comprehensive logging around buffer operations**
4. **Use RAII wrappers for OFX resources**
5. **Implement unit tests for buffer conversion functions**
6. **Add crash reporting/telemetry for production builds**

## Files to Modify

1. `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp`
   - Add NULL check at line 840 (after `fetchImage()`)
   - Add buffer validation before `toOFXBuffer()` call

2. `contrib/plugins/ComfyUI/common/comfyui_image_io.cpp`
   - Add parameter validation in `toOFXBuffer()` function
   - Add bounds checking in pixel copy loops

## References

- Crash log 1: `contrib/plugins/ComfyUI/segmentation/ResolveDebug_20251209-1430.txt`
- Crash log 2: `contrib/plugins/ComfyUI/segmentation/ResolveDebug-20251209-1440.txt`
- Plugin source: `contrib/plugins/ComfyUI/common/comfyui_base_plugin.cpp`
- Image I/O: `contrib/plugins/ComfyUI/common/comfyui_image_io.cpp`

---

**Generated:** 2025-12-10
**Analyzer:** Claude Code
**Status:** Fix implementation pending
