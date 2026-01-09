# Filename Version Suffix Fix

## Date
December 30, 2025

## Issue

After fixing directory creation, workflows execute successfully and write output files, but Flame doesn't display them because of a **filename mismatch**.

**Plugin expects:**
```
/Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/SAM_TEST_AnyComfy.0023.exr
```

**ComfyUI actually writes:**
```
/Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/SAM_TEST_AnyComfy_v001.0023.exr
                                                                              ^^^^^^
                                                                        Extra version suffix!
```

**Result**: Plugin can't find output files, Flame shows placeholder/passthrough instead of rendered result.

## Root Cause Analysis

### Discovery

**From logs (line 1604):**
```
[error] AsyncJobManager: Frame 23 - output file missing:
    /Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/SAM_TEST_AnyComfy.0023.exr
```

**Then later (line 1895):**
```
exception_message":"File exists already, stopping to avoid overwriting"
```

**On disk:**
```bash
$ ls /Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/
SAM_TEST_AnyComfy_v001.0023.exr
SAM_TEST_AnyComfy_v001.0024.exr
```

### The Problem

Looking at the workflow sent to ComfyUI (from error log):
```python
'6': {
    'class_type': 'SaveEXR',
    'inputs': {
        'filename_prefix': 'Z:\\out\\SAM_TEST\\any_segmentation\\v001\\SAM_TEST_AnyComfy',
        'start_frame': 23,
        'version': 1  # ← THIS IS THE PROBLEM!
    }
}
```

**SaveEXR node behavior:**
- `version: 1` → Adds `_v001` suffix to filename
- `version: 2` → Adds `_v002` suffix to filename
- `version: -1` → No version suffix (what we need!)

**Naming pattern:**
```
prefix + _v{version:03d} + .{frame:04d}.exr  (when version >= 0)
prefix + .{frame:04d}.exr                     (when version == -1)
```

**Current workflow generates:**
```
SAM_TEST_AnyComfy + _v001 + .0023.exr = SAM_TEST_AnyComfy_v001.0023.exr
```

**But plugin expects:**
```
SAM_TEST_AnyComfy + .0023.exr = SAM_TEST_AnyComfy.0023.exr
```

### Why This Happened

The workflow loaded by the user (`comfyui_normal_map_deepbump_workflow_api.json`) has SaveEXR configured with `version: 1`:

```json
{
  "6": {
    "inputs": {
      "filename_prefix": "Result",
      "version": 1,     ← From original workflow
      "start_frame": 1001,
      ...
    },
    "class_type": "SaveEXR"
  }
}
```

**Smart injection (v1.0.1-1.0.3) overwrites:**
- ✓ `filename_prefix` → Full path prefix
- ✓ `start_frame` → Current frame number
- ✗ `version` → Left unchanged (still `1`)

**Result**: Version parameter retained from original workflow, adding unwanted suffix.

### Architecture Decision

**The version is ALREADY in the directory path:**
```
.../v001/SAM_TEST_AnyComfy.0023.exr
    ^^^^  Version in directory
```

**We DON'T want it duplicated in the filename:**
```
.../v001/SAM_TEST_AnyComfy_v001.0023.exr  ← WRONG
    ^^^^                   ^^^^
    Directory version      Filename version (duplicate!)
```

**This is consistent with the base plugin architecture:**

From `constructExpectedOutputPath()` in [comfyui_base_plugin.cpp](../common/comfyui_base_plugin.cpp):
```cpp
// NOTE: SaveEXR version is set to -1 (no version suffix) because the directory
// already contains the version number (e.g., v001, v002).
//
// Example: /Volumes/silo2/002_COMFYUI/out/TEST_SAM/segmentation/v001/shot01.0056.exr
```

## The Fix

### Code Changes

**File**: [anycomfy_plugin.cpp](anycomfy_plugin.cpp)
**Method**: `injectPathsIntoWorkflow()`
**Lines**: 381-390

**Before (v1.0.3):**
```cpp
// Find and modify SaveEXR nodes
if (classType == "SaveEXR") {
    // Inject the output prefix and frame
    nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
    nodeData["inputs"]["start_frame"] = frame;
    saveEXRCount++;
}
```

**After (v1.0.4):**
```cpp
// Find and modify SaveEXR nodes
if (classType == "SaveEXR") {
    // Inject the output prefix and frame
    nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
    nodeData["inputs"]["start_frame"] = frame;
    // CRITICAL: Set version to -1 to prevent adding version suffix to filename
    // The version is already in the directory path (e.g., .../v001/)
    // so we don't want it duplicated in the filename
    nodeData["inputs"]["version"] = -1;
    saveEXRCount++;

    if (_logger) {
        _logger->info("  → Injected filename_prefix: {}", comfyOutputPrefix);
        _logger->info("  → Injected start_frame: {}", frame);
        _logger->info("  → Injected version: -1 (no filename suffix)");
    }
}
```

### What Changed

**Smart injection now overwrites THREE parameters:**
1. ✓ `filename_prefix` → Full output path prefix
2. ✓ `start_frame` → Current frame number
3. ✓ **`version` → `-1` (no suffix)** ← NEW

This ensures the filename matches what the plugin expects, regardless of the original workflow configuration.

## Expected Behavior After Fix

### Before Fix (v1.0.3)

**ComfyUI writes:**
```
SAM_TEST_AnyComfy_v001.0023.exr
```

**Plugin looks for:**
```
SAM_TEST_AnyComfy.0023.exr
```

**Result**: ❌ File not found, frame FAILS, Flame shows placeholder

### After Fix (v1.0.4)

**ComfyUI writes:**
```
SAM_TEST_AnyComfy.0023.exr
```

**Plugin looks for:**
```
SAM_TEST_AnyComfy.0023.exr
```

**Result**: ✓ File found, frame SUCCESS, Flame shows rendered output

## Testing

### Build and Install
```bash
./contrib/dev-tools/build-macos-universal-plugin.sh -p AnyComfy -t AnyComfy --install
```

### Test Workflow

1. **Render frame in Flame**
2. **Check logs** for version injection:
   ```
   [info] Found SaveEXR node: 6
   [info]   → Injected filename_prefix: Z:\out\SAM_TEST\any_segmentation\v001\SAM_TEST_AnyComfy
   [info]   → Injected start_frame: 23
   [info]   → Injected version: -1 (no filename suffix)  ← NEW LOG
   ```

3. **Verify output filename** (no `_v001` suffix):
   ```bash
   $ ls /Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/
   SAM_TEST_AnyComfy.0023.exr  ← CORRECT (no _v001)
   ```

4. **Verify result in Flame**: Should display rendered output, not placeholder

### Cleanup Old Files

If testing with frames that were already rendered with the wrong suffix, delete them first:
```bash
rm /Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/SAM_TEST_AnyComfy_v001.*.exr
```

Or rename to avoid conflicts:
```bash
cd /Volumes/silo2/002_COMFYUI/out/SAM_TEST/any_segmentation/v001/
for f in SAM_TEST_AnyComfy_v001.*.exr; do mv "$f" "${f/_v001/}"; done
```

## Impact

### All ComfyUI Plugins

This fix applies to the **smart injection** feature, which is specific to AnyComfy. The base plugin and SAM plugin use template-based workflows that already specify `version: -1` in the template.

**Affected:**
- ✓ AnyComfy plugin (uses smart injection) - FIXED

**Not affected:**
- SAM plugin (uses template workflow with explicit `"version": -1`)
- Any plugin using template-based workflows

### Workflow Compatibility

**Works with:**
- ✓ Non-templated workflows (raw ComfyUI exports) - version overridden
- ✓ Templated workflows with `"version": 1` - overridden to `-1`
- ✓ Templated workflows with `"version": -1` - stays `-1` (no change)

**Result**: Universal compatibility, correct filenames in all cases.

## Related Issues

### Secondary Note: File Overwrite Protection

From the error logs:
```
exception_message":"File exists already, stopping to avoid overwriting"
```

The SaveEXR node has overwrite protection enabled. When re-rendering the same frame:
- First render: Writes `SAM_TEST_AnyComfy_v001.0023.exr`
- Second render: Fails with "File exists already" error

**After this fix:**
- Plugin will correctly find and load the cached file
- Avoid re-submission if file already exists
- Overwrite protection becomes irrelevant

## Version History

- **v1.0.0**: Initial release
- **v1.0.1**: Smart injection added
- **v1.0.2**: Path escaping fix
- **v1.0.3**: Async directory creation fix
- **v1.0.4**: **Filename version suffix fix** (this fix)

## Summary

**Problem**: SaveEXR node adds version suffix (`_v001`) to filenames, causing mismatch with plugin expectations

**Root Cause**: Smart injection didn't override `version` parameter, leaving it at workflow default (usually `1`)

**Solution**: Override `version` to `-1` in smart injection to prevent suffix

**Result**: Filenames match plugin expectations, Flame displays rendered output correctly

---

**Fixed**: December 30, 2025
**Plugin Version**: 1.0.4
**Issue Type**: Logic bug in smart injection
**Severity**: Critical (blocks output display)
**Fix Type**: Add `version: -1` override in smart injection
