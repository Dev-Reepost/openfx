# Multi-Input Workflow Support - Implementation Complete

## Status: IMPLEMENTED

This document describes the multi-input workflow support that has been implemented for the AnyComfy OFX plugin.

---

## Summary of Changes

### File Naming (Collision-Free)
```
InputA (Source):  {basename}.{frame:04d}.exr          (backward compatible)
InputB (Source2): {basename}_B.{frame:04d}.exr
InputC (Source3): {basename}_C.{frame:04d}.exr

Example:
/Volumes/silo2/002_COMFYUI/in/project/workflow/v001/shot01.0001.exr      (InputA)
/Volumes/silo2/002_COMFYUI/in/project/workflow/v001/shot01_B.0001.exr    (InputB)
/Volumes/silo2/002_COMFYUI/in/project/workflow/v001/shot01_C.0001.exr    (InputC)
```

### LoadEXR Node Mapping Strategy

**Priority 1**: Match by node title (from UI format `title` field or API format `_meta.title`)
- Looks for: "InputA", "InputB", "InputC", "Input1", "Input2", "Input3"
- Also matches: "Source", "Main", "Secondary", "Background", "Tertiary", "Foreground"

**Priority 2**: Position-based fallback (X coordinate, leftmost first)
- Sorts LoadEXR nodes by X position
- Assigns InputA to leftmost, InputB to next, InputC to rightmost

---

## Files Modified

### Core Infrastructure
- **comfyui_base_plugin.h**
  - Added `_src2Clip`, `_src3Clip` optional clip pointers
  - Added `writeInputImages()` returning `std::map<std::string, std::string>`
  - Added `getConnectedInputCount()`
  - Updated `buildWorkflow()` signature to use `std::map<std::string, std::string>& inputPaths`
  - Updated `constructInputPath()` with optional suffix parameter
  - Updated `writeInputImage()` with optional suffix parameter
  - Updated `customizeWorkflow()` to use inputPaths map

- **comfyui_base_plugin.cpp**
  - Implemented multi-input clip fetching and EXR writing
  - Updated `executeWorkflow()` to use `writeInputImages()`
  - Updated `renderAsync()` for multi-input async rendering
  - Updated `customizeWorkflow()` with placeholders: `${INPUT_PATH_A}`, `${INPUT_PATH_B}`, `${INPUT_PATH_C}`

- **async_job_manager.h / .cpp**
  - Updated `submitJobAsync()` to accept `std::map<std::string, ImageData>` and `std::map<std::string, std::string>`

### AnyComfy Plugin
- **anycomfy_plugin.h**
  - Updated `buildWorkflow()` signature
  - Updated `injectPathsIntoWorkflow()` signature

- **anycomfy_plugin.cpp**
  - Added Source2 and Source3 clip definitions in `describeInContext()`
  - Implemented position-based LoadEXR node mapping in `injectPathsIntoWorkflow()`
  - Smart title matching with fallback to position order

### SAM Plugin (Backward Compatible)
- **sam_segmentation_plugin.h / .cpp**
  - Updated `buildWorkflow()` signature
  - Extracts InputA from map (single-input workflow)

### New Template Files
- **resources/workflows/template.json** - Updated with InputA title
- **resources/workflows/template_2inputs.json** - Two LoadEXR nodes (InputA, InputB)
- **resources/workflows/template_3inputs.json** - Three LoadEXR nodes (InputA, InputB, InputC)

---

## How It Works

### For Users

1. **Connect clips** in host (Flame, Nuke, etc.):
   - Source (required) → maps to InputA
   - Source2 (optional) → maps to InputB
   - Source3 (optional) → maps to InputC

2. **Select or create workflow**:
   - Existing single-input workflows work unchanged
   - Multi-input workflows automatically detect connected clips

3. **Render**:
   - Plugin writes each connected clip to separate EXR file
   - LoadEXR nodes are matched by title or position
   - Paths injected automatically

### For Workflow Creators

1. **Name LoadEXR nodes** clearly:
   - Use titles like "InputA", "InputB", "InputC"
   - Or "Source", "Background", "Foreground"

2. **Position matters** as fallback:
   - Leftmost LoadEXR → InputA
   - Middle LoadEXR → InputB
   - Rightmost LoadEXR → InputC

---

## Backward Compatibility

| Scenario | Behavior |
|----------|----------|
| Existing single-input workflow | Works unchanged (InputA only) |
| New multi-input workflow | Uses InputA, InputB, InputC |
| Old plugin version + new workflow | Would ignore extra LoadEXR nodes |
| New plugin version + old workflow | Works (single input mapped to first LoadEXR) |

---

## Testing Checklist

- [x] Compilation verified (AnyComfy and SAMSegmentation)
- [ ] Single input workflow still works (backward compatibility)
- [ ] Two input workflow correctly maps clips
- [ ] Three input workflow correctly maps clips
- [ ] Missing clip shows warning, doesn't crash
- [ ] File names don't collide
- [ ] Position-based fallback works for unnamed nodes
- [ ] Title matching works for named nodes
