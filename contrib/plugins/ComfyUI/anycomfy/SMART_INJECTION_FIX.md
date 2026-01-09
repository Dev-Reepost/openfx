# Smart Workflow Path Injection - Fix for Non-Templated Workflows

## Problem

AnyComfy plugin was only working with **templated workflows** that had placeholder variables like:
```json
{
  "1": {
    "inputs": {
      "filepath": "${INPUT_PATH}",  // Template variable
      ...
    },
    "class_type": "LoadEXR"
  }
}
```

But real-world ComfyUI workflows exported from the UI have **hardcoded values**:
```json
{
  "3": {
    "inputs": {
      "filepath": "Front",  // Hardcoded value
      ...
    },
    "class_type": "LoadEXR"
  }
}
```

When users selected these raw workflows, the plugin would fail with errors like:
```
Exception: Path not found: Front
```

## Root Cause

The original implementation only did **string-based template variable substitution**:
1. Load workflow JSON
2. Convert to string
3. Replace `${INPUT_PATH}` → actual path
4. Replace `${OUTPUT_PREFIX}` → actual prefix
5. Parse back to JSON

This approach **failed for non-templated workflows** because there were no template variables to replace. The hardcoded values like `"Front"` remained unchanged.

## Solution

Added **smart JSON node injection** that works with ANY workflow, templated or not.

### Two-Phase Approach

#### Phase 1: Template Variable Substitution (Existing)
```cpp
json customized = customizeWorkflow(baseWorkflow, frame, inputPath);
```
- Replaces `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, `${FRAME}` in templated workflows
- Harmless for non-templated workflows (nothing to replace)

#### Phase 2: Smart Node Injection (NEW)
```cpp
json final = injectPathsIntoWorkflow(customized, frame, inputPath, outputPrefix);
```
- Parses the workflow JSON structure
- **Finds ALL LoadEXR nodes** by searching for `class_type == "LoadEXR"`
- **Directly modifies** their `inputs.filepath` with actual input path
- **Finds ALL SaveEXR nodes** by searching for `class_type == "SaveEXR"`
- **Directly modifies** their `inputs.filename_prefix` and `inputs.start_frame`

### Smart Injection Algorithm

```cpp
json AnyComfyPlugin::injectPathsIntoWorkflow(const json& workflow, int frame,
                                              const std::string& inputPath,
                                              const std::string& outputPrefix)
{
    json modifiedWorkflow = workflow;

    // Convert to ComfyUI format (Windows paths if server is Windows)
    std::string comfyInputPath = convertPathForComfyUI(inputPath);
    std::string comfyOutputPrefix = convertPathForComfyUI(outputPrefix);

    // Iterate through all nodes
    for (auto& [nodeId, nodeData] : modifiedWorkflow.items()) {
        if (!nodeData.is_object()) continue;
        if (!nodeData.contains("class_type")) continue;

        std::string classType = nodeData["class_type"];

        // Find and modify LoadEXR nodes
        if (classType == "LoadEXR") {
            nodeData["inputs"]["filepath"] = comfyInputPath;
        }

        // Find and modify SaveEXR nodes
        if (classType == "SaveEXR") {
            nodeData["inputs"]["filename_prefix"] = comfyOutputPrefix;
            nodeData["inputs"]["start_frame"] = frame;
        }
    }

    return modifiedWorkflow;
}
```

### Key Features

1. **No assumptions about node names or IDs**
   - Doesn't care if LoadEXR is node "1", "3", or "Front_Load"
   - Finds nodes by `class_type`, not by hardcoded IDs

2. **Handles multiple LoadEXR/SaveEXR nodes**
   - Some workflows have multiple input/output nodes
   - Modifies ALL of them

3. **Preserves other node parameters**
   - Only modifies `filepath`, `filename_prefix`, and `start_frame`
   - Leaves `linear_to_sRGB`, `sRGB_to_linear`, `frame_pad`, etc. unchanged

4. **Works with both approaches**
   - Template substitution happens first (for templated workflows)
   - Smart injection happens second (for non-templated workflows)
   - Both can coexist in the same workflow

5. **Comprehensive logging**
   - Logs which nodes were found and modified
   - Warns if no LoadEXR or SaveEXR nodes found
   - Helps debug workflow issues

## Example Transformations

### Before (Non-Templated Workflow)

```json
{
  "3": {
    "inputs": {
      "filepath": "Front",  // ← Will fail - file doesn't exist
      "linear_to_sRGB": true,
      ...
    },
    "class_type": "LoadEXR"
  },
  "6": {
    "inputs": {
      "filename_prefix": "Result",  // ← Will output to wrong location
      "start_frame": 1001,           // ← Wrong frame
      ...
    },
    "class_type": "SaveEXR"
  }
}
```

### After (Smart Injection)

```json
{
  "3": {
    "inputs": {
      "filepath": "S:\\002_COMFYUI\\in\\project1\\inputs\\frame_0001.exr",  // ✅ Real path
      "linear_to_sRGB": true,  // ← Preserved
      ...
    },
    "class_type": "LoadEXR"
  },
  "6": {
    "inputs": {
      "filename_prefix": "S:\\002_COMFYUI\\out\\project1\\workflow\\v001\\anycomfy_effect1",  // ✅ Real path
      "start_frame": 1,  // ✅ Current frame
      ...
    },
    "class_type": "SaveEXR"
  }
}
```

## Logging Output

The plugin now logs detailed information about the injection process:

```
[INFO] Injecting paths into workflow nodes (smart injection for non-templated workflows)
[INFO] Injecting input path: S:\002_COMFYUI\in\project1\inputs\frame_0001.exr
[INFO] Injecting output prefix: S:\002_COMFYUI\out\project1\workflow\v001\anycomfy_effect1
[INFO] Injecting frame: 1
[INFO] Found LoadEXR node: 3
[INFO]   → Injected filepath: S:\002_COMFYUI\in\project1\inputs\frame_0001.exr
[INFO] Found SaveEXR node: 6
[INFO]   → Injected filename_prefix: S:\002_COMFYUI\out\project1\workflow\v001\anycomfy_effect1
[INFO]   → Injected start_frame: 1
[INFO] Smart injection complete: 1 LoadEXR nodes, 1 SaveEXR nodes modified
```

## Error Detection

The plugin detects and warns about invalid workflows:

```cpp
if (loadEXRCount == 0) {
    logger->warn("No LoadEXR nodes found in workflow! Workflow may not have input.");
}
if (saveEXRCount == 0) {
    logger->warn("No SaveEXR nodes found in workflow! Workflow may not produce output.");
}
```

## Use Cases Now Supported

### 1. Raw ComfyUI Workflows (NEW ✅)
Export a workflow from ComfyUI UI → Use directly in AnyComfy
- No manual editing needed
- No template variable insertion required
- Just works™

### 2. Templated Workflows (Still Supported ✅)
Create workflows with `${INPUT_PATH}`, `${OUTPUT_PREFIX}`, etc.
- Template substitution happens first
- Smart injection ensures correctness

### 3. Mixed Workflows (NEW ✅)
Some nodes templated, some hardcoded
- Both approaches work together
- Maximum flexibility

## Testing

### Test Workflow Files

Located in `contrib/plugins/ComfyUI/anycomfy/`:

1. **comfyui_normal_map_deepbump_workflow_api.json**
   - Non-templated workflow from ComfyUI UI
   - Has `filepath: "Front"` and `filename_prefix: "Result"`
   - Now works with smart injection

2. **comfyui_segmentation_segment_anything_workflow_api.json**
   - Raw SAM workflow from ComfyUI
   - Non-templated
   - Now works with smart injection

3. **template.json** (in resources/workflows/)
   - Templated workflow with `${INPUT_PATH}`, etc.
   - Still works with template substitution

### Verification

```bash
# Before: Error
Exception: Path not found: Front

# After: Success
[INFO] Found LoadEXR node: 3
[INFO]   → Injected filepath: /path/to/real/input.exr
[INFO] Smart injection complete: 1 LoadEXR nodes, 1 SaveEXR nodes modified
```

## Files Modified

1. **[anycomfy_plugin.h](anycomfy_plugin.h)**
   - Added `injectPathsIntoWorkflow()` method declaration

2. **[anycomfy_plugin.cpp](anycomfy_plugin.cpp)**
   - Implemented smart injection in `injectPathsIntoWorkflow()`
   - Updated `buildWorkflow()` to call smart injection after template substitution

## Backward Compatibility

✅ **100% backward compatible**
- Templated workflows still work (template substitution)
- Smart injection is additive, not replacing existing functionality
- No breaking changes to API or workflow format

## Performance Impact

**Negligible**
- JSON parsing/iteration is fast (milliseconds)
- Only done once per frame
- Much faster than network I/O to ComfyUI server

## Future Enhancements

### Potential Improvements

1. **Auto-detect node connections**
   - Find LoadEXR output → First processing node
   - Find last processing node → SaveEXR input
   - Validate the image processing chain

2. **Support for other loaders**
   - LoadImage nodes (PNG, JPG, etc.)
   - LoadLatent nodes
   - Custom loader nodes

3. **Support for other savers**
   - SaveImage nodes
   - SaveLatent nodes
   - Custom saver nodes

4. **Workflow validation**
   - Check that LoadEXR is connected to processing chain
   - Check that processing chain is connected to SaveEXR
   - Warn about disconnected nodes

## Conclusion

AnyComfy now intelligently handles **both templated and non-templated workflows**, making it truly generic and user-friendly. Users can:

1. Export any workflow from ComfyUI UI
2. Drop it in the workflows directory
3. Select it in AnyComfy
4. It just works ✅

No more manual editing of JSON files!
No more template variable insertion!
No more "Path not found" errors!

---

**Implementation Date**: December 26, 2025
**Plugin Version**: 1.0.1 (smart injection added)
**Fixed Issue**: Non-templated workflow support
